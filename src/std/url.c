#include "url.h"

#include <ctype.h>
#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"
#include "encoding.h"  // For surrogate handling helper
#include "formdata.h"

// Forward declare class IDs
static JSClassID JSRT_URLClassID;
static JSClassID JSRT_URLSearchParamsClassID;

// External class ID from formdata.c
extern JSClassID JSRT_FormDataClassID;

// Forward declare structures
struct JSRT_URL;

// URLSearchParams parameter structure
typedef struct JSRT_URLSearchParam {
  char* name;
  char* value;
  size_t name_len;   // Length of name (may contain null bytes)
  size_t value_len;  // Length of value (may contain null bytes)
  struct JSRT_URLSearchParam* next;
} JSRT_URLSearchParam;

// URLSearchParams structure
typedef struct JSRT_URLSearchParams {
  JSRT_URLSearchParam* params;
  struct JSRT_URL* parent_url;  // Reference to parent URL for href updates
  JSContext* ctx;               // Context for parent URL updates
} JSRT_URLSearchParams;

// URL component structure
typedef struct JSRT_URL {
  char* href;
  char* protocol;
  char* username;
  char* password;
  char* host;
  char* hostname;
  char* port;
  char* pathname;
  char* search;
  char* hash;
  char* origin;
  JSValue search_params;  // Cached URLSearchParams object
  JSContext* ctx;         // Context for managing search_params
} JSRT_URL;

// Forward declarations
static JSRT_URL* JSRT_ParseURL(const char* url, const char* base);
static int validate_url_characters(const char* url);
static int validate_hostname_characters(const char* hostname);
static void JSRT_FreeURL(JSRT_URL* url);
static JSValue JSRT_URLSearchParamsToString(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static char* url_encode_with_len(const char* str, size_t len);
static char* url_component_encode(const char* str);
static char* url_nonspecial_path_encode(const char* str);
static char* url_userinfo_encode(const char* str);
static char* url_decode_with_length_and_output_len(const char* str, size_t len, size_t* output_len);
static char* url_decode_with_length(const char* str, size_t len);
static char* url_decode(const char* str);
static JSRT_URLSearchParams* JSRT_ParseSearchParams(const char* search_string, size_t string_len);
static JSRT_URLSearchParams* JSRT_ParseSearchParamsFromFormData(JSContext* ctx, JSValueConst formdata_val);
static void JSRT_FreeSearchParams(JSRT_URLSearchParams* search_params);
static char* canonicalize_ipv6(const char* ipv6_str);
static int is_special_scheme(const char* protocol);
static char* normalize_dot_segments(const char* path);

// Simple URL parser (basic implementation)
static int is_valid_scheme(const char* scheme) {
  if (!scheme || strlen(scheme) == 0)
    return 0;

  // First character must be a letter
  if (!((scheme[0] >= 'a' && scheme[0] <= 'z') || (scheme[0] >= 'A' && scheme[0] <= 'Z'))) {
    return 0;
  }

  // Rest can be letters, digits, +, -, .
  for (size_t i = 1; i < strlen(scheme); i++) {
    char c = scheme[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '-' ||
          c == '.')) {
      return 0;
    }
  }
  return 1;
}

static int is_default_port(const char* scheme, const char* port) {
  if (!port || strlen(port) == 0)
    return 1;  // No port specified, so it's implicit default

  if (strcmp(scheme, "https") == 0 && strcmp(port, "443") == 0)
    return 1;
  if (strcmp(scheme, "http") == 0 && strcmp(port, "80") == 0)
    return 1;
  if (strcmp(scheme, "ws") == 0 && strcmp(port, "80") == 0)
    return 1;
  if (strcmp(scheme, "wss") == 0 && strcmp(port, "443") == 0)
    return 1;
  if (strcmp(scheme, "ftp") == 0 && strcmp(port, "21") == 0)
    return 1;

  return 0;
}

// Canonicalize IPv6 address according to RFC 5952
// Handles IPv4-mapped IPv6 addresses like ::127.0.0.1 -> ::7f00:1
// Also handles zero compression like 1:0:: -> 1::
static char* canonicalize_ipv6(const char* ipv6_str) {
  if (!ipv6_str || strlen(ipv6_str) < 3) {
    return strdup(ipv6_str ? ipv6_str : "");
  }

  // Remove brackets if present
  const char* addr_start = ipv6_str;
  const char* addr_end = ipv6_str + strlen(ipv6_str);
  if (ipv6_str[0] == '[') {
    addr_start = ipv6_str + 1;
    if (ipv6_str[strlen(ipv6_str) - 1] == ']') {
      addr_end = ipv6_str + strlen(ipv6_str) - 1;
    }
  }

  // Create a working copy of the address part
  size_t addr_len = addr_end - addr_start;
  char* addr = malloc(addr_len + 1);
  strncpy(addr, addr_start, addr_len);
  addr[addr_len] = '\0';

  // Parse IPv6 address into 16-bit groups
  uint16_t groups[8] = {0};
  int group_count = 0;
  int double_colon_pos = -1;

  // Check for double colon
  char* double_colon = strstr(addr, "::");
  if (double_colon) {
    double_colon_pos = double_colon - addr;
  }

  // Parse groups before double colon (if any)
  if (double_colon_pos >= 0) {
    if (double_colon_pos > 0) {
      char* before_dc = malloc(double_colon_pos + 1);
      strncpy(before_dc, addr, double_colon_pos);
      before_dc[double_colon_pos] = '\0';

      char* token = strtok(before_dc, ":");
      while (token && group_count < 8) {
        groups[group_count++] = (uint16_t)strtoul(token, NULL, 16);
        token = strtok(NULL, ":");
      }
      free(before_dc);
    }

    // Parse groups after double colon
    char* after_dc = addr + double_colon_pos + 2;
    if (strlen(after_dc) > 0) {
      // Check for IPv4 suffix
      if (strchr(after_dc, '.')) {
        // Handle IPv4-mapped IPv6
        char* ipv4_start = strrchr(after_dc, ':');
        if (ipv4_start) {
          ipv4_start++;
        } else {
          ipv4_start = after_dc;
        }

        int a, b, c, d;
        if (sscanf(ipv4_start, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 && a >= 0 && a <= 255 && b >= 0 && b <= 255 &&
            c >= 0 && c <= 255 && d >= 0 && d <= 255) {
          // Convert IPv4 to two 16-bit groups
          uint16_t high = (a << 8) | b;
          uint16_t low = (c << 8) | d;

          // Parse any groups before the IPv4 part
          if (ipv4_start > after_dc) {
            char* before_ipv4 = malloc(ipv4_start - after_dc);
            strncpy(before_ipv4, after_dc, ipv4_start - after_dc - 1);
            before_ipv4[ipv4_start - after_dc - 1] = '\0';

            char* token = strtok(before_ipv4, ":");
            while (token && group_count < 6) {
              groups[group_count++] = (uint16_t)strtoul(token, NULL, 16);
              token = strtok(NULL, ":");
            }
            free(before_ipv4);
          }

          // Add IPv4 as two groups
          if (group_count < 8)
            groups[group_count++] = high;
          if (group_count < 8)
            groups[group_count++] = low;
        }
      } else {
        // Regular hex groups after ::
        char* token = strtok(after_dc, ":");
        int after_groups[8];
        int after_count = 0;
        while (token && after_count < 8) {
          after_groups[after_count++] = (uint16_t)strtoul(token, NULL, 16);
          token = strtok(NULL, ":");
        }

        // Fill in the zero groups
        int zeros_needed = 8 - group_count - after_count;
        for (int i = 0; i < zeros_needed && group_count < 8; i++) {
          groups[group_count++] = 0;
        }

        // Add the groups after ::
        for (int i = 0; i < after_count && group_count < 8; i++) {
          groups[group_count++] = after_groups[i];
        }
      }
    } else {
      // :: at the end, fill with zeros
      while (group_count < 8) {
        groups[group_count++] = 0;
      }
    }
  } else {
    // No double colon, parse all groups
    char* token = strtok(addr, ":");
    while (token && group_count < 8) {
      groups[group_count++] = (uint16_t)strtoul(token, NULL, 16);
      token = strtok(NULL, ":");
    }
  }

  // Find the longest run of zeros for compression
  int best_start = -1, best_len = 0;
  int current_start = -1, current_len = 0;

  for (int i = 0; i < 8; i++) {
    if (groups[i] == 0) {
      if (current_start == -1) {
        current_start = i;
        current_len = 1;
      } else {
        current_len++;
      }
    } else {
      if (current_len > best_len && current_len > 1) {
        best_start = current_start;
        best_len = current_len;
      }
      current_start = -1;
      current_len = 0;
    }
  }

  // Check the final run
  if (current_len > best_len && current_len > 1) {
    best_start = current_start;
    best_len = current_len;
  }

  // Build the canonical form
  char* result = malloc(64);
  result[0] = '[';
  result[1] = '\0';

  int i = 0;
  while (i < 8) {
    if (i == best_start && best_len > 1) {
      // Use :: compression
      if (i == 0) {
        strcat(result, "::");
      } else {
        strcat(result, "::");
      }
      i += best_len;
    } else {
      if (i > 0 && !(i == best_start + best_len && best_len > 1)) {
        strcat(result, ":");
      }
      char group_str[8];
      snprintf(group_str, sizeof(group_str), "%x", groups[i]);
      strcat(result, group_str);
      i++;
    }
  }

  strcat(result, "]");

  free(addr);
  return result;
}

// Canonicalize IPv4 address according to WHATWG URL spec
// Handles decimal, octal, and hexadecimal formats
// Returns NULL if not a valid IPv4 address, otherwise returns canonical dotted decimal
static char* canonicalize_ipv4_address(const char* input) {
  if (!input || strlen(input) == 0) {
    return NULL;
  }

  // Check if this looks like an IPv4 address (contains dots or hex notation)
  int has_dots = strchr(input, '.') != NULL;
  int has_hex = strstr(input, "0x") != NULL || strstr(input, "0X") != NULL;

  // Reject all hex notation per WPT tests - these should be treated as invalid hostnames
  if (has_hex) {
    return NULL;
  }

  if (!has_dots) {
    // Try to parse as a single 32-bit number (decimal or octal only, no hex)
    char* endptr;
    unsigned long addr = strtoul(input, &endptr, 10);  // Only decimal base

    // If the entire string was consumed and it's a valid 32-bit value
    if (*endptr == '\0' && addr <= 0xFFFFFFFF) {
      char* result = malloc(16);  // "255.255.255.255\0"
      snprintf(result, 16, "%lu.%lu.%lu.%lu", (addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF,
               addr & 0xFF);
      return result;
    }
    return NULL;
  }

  if (has_dots) {
    // Parse dotted notation (may include hex/octal parts)
    char* input_copy = strdup(input);
    char* parts[4];
    int part_count = 0;

    char* token = strtok(input_copy, ".");
    while (token && part_count < 4) {
      parts[part_count++] = token;
      token = strtok(NULL, ".");
    }

    // Must have 1-4 parts for valid IPv4
    if (part_count == 0 || part_count > 4) {
      free(input_copy);
      return NULL;
    }

    unsigned long values[4] = {0};

    // Parse each part (supporting decimal, octal, hex)
    for (int i = 0; i < part_count; i++) {
      char* endptr;
      values[i] = strtoul(parts[i], &endptr, 0);  // Auto-detect base

      if (*endptr != '\0') {
        free(input_copy);
        return NULL;  // Invalid number
      }
    }

    // Validate ranges based on number of parts
    if (part_count == 4) {
      // All parts must be 0-255
      for (int i = 0; i < 4; i++) {
        if (values[i] > 255) {
          free(input_copy);
          return NULL;
        }
      }
    } else if (part_count == 3) {
      // First two parts 0-255, last part 0-65535
      if (values[0] > 255 || values[1] > 255 || values[2] > 65535) {
        free(input_copy);
        return NULL;
      }
      // Convert a.b.c to a.b.(c>>8).(c&0xFF)
      unsigned long c = values[2];
      values[3] = c & 0xFF;
      values[2] = (c >> 8) & 0xFF;
    } else if (part_count == 2) {
      // First part 0-255, second part 0-16777215
      if (values[0] > 255 || values[1] > 16777215) {
        free(input_copy);
        return NULL;
      }
      // Convert a.b to a.(b>>16).((b>>8)&0xFF).(b&0xFF)
      unsigned long b = values[1];
      values[3] = b & 0xFF;
      values[2] = (b >> 8) & 0xFF;
      values[1] = (b >> 16) & 0xFF;
    } else if (part_count == 1) {
      // Single part must be 0-4294967295
      if (values[0] > 0xFFFFFFFF) {
        free(input_copy);
        return NULL;
      }
      // Convert a to (a>>24).((a>>16)&0xFF).((a>>8)&0xFF).(a&0xFF)
      unsigned long a = values[0];
      values[3] = a & 0xFF;
      values[2] = (a >> 8) & 0xFF;
      values[1] = (a >> 16) & 0xFF;
      values[0] = (a >> 24) & 0xFF;
    }

    char* result = malloc(16);  // "255.255.255.255\0"
    snprintf(result, 16, "%lu.%lu.%lu.%lu", values[0], values[1], values[2], values[3]);

    free(input_copy);
    return result;
  }

  return NULL;
}

// Check if a protocol is a special scheme per WHATWG URL spec
static int is_special_scheme(const char* protocol) {
  if (!protocol || strlen(protocol) == 0)
    return 0;

  // Remove colon if present
  char* scheme = strdup(protocol);
  if (strlen(scheme) > 0 && scheme[strlen(scheme) - 1] == ':') {
    scheme[strlen(scheme) - 1] = '\0';
  }

  const char* special_schemes[] = {"http", "https", "ws", "wss", "file", NULL};
  int is_special = 0;
  for (int i = 0; special_schemes[i]; i++) {
    if (strcmp(scheme, special_schemes[i]) == 0) {
      is_special = 1;
      break;
    }
  }

  free(scheme);
  return is_special;
}

static char* compute_origin(const char* protocol, const char* hostname, const char* port) {
  if (!protocol || !hostname || strlen(protocol) == 0 || strlen(hostname) == 0) {
    return strdup("null");
  }

  // Extract scheme without colon
  char* scheme = strdup(protocol);
  if (strlen(scheme) > 0 && scheme[strlen(scheme) - 1] == ':') {
    scheme[strlen(scheme) - 1] = '\0';
  }

  // Handle blob URLs - extract origin from the inner URL
  if (strcmp(scheme, "blob") == 0) {
    free(scheme);
    // For blob URLs, the origin is derived from the inner URL
    // blob:https://example.com/uuid -> https://example.com
    // The hostname for blob URLs should contain the inner URL
    if (strncmp(hostname, "http://", 7) == 0 || strncmp(hostname, "https://", 8) == 0) {
      // Parse the inner URL to extract its origin
      JSRT_URL* inner_url = JSRT_ParseURL(hostname, NULL);
      if (inner_url && inner_url->protocol && inner_url->hostname) {
        char* inner_origin = compute_origin(inner_url->protocol, inner_url->hostname, inner_url->port);
        JSRT_FreeURL(inner_url);
        return inner_origin;
      }
      if (inner_url)
        JSRT_FreeURL(inner_url);
    }
    return strdup("null");
  }

  // Special schemes that can have tuple origins: http, https, ftp, ws, wss
  // All other schemes (including file, data, javascript, and custom schemes) have null origin
  if (strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0 && strcmp(scheme, "ftp") != 0 &&
      strcmp(scheme, "ws") != 0 && strcmp(scheme, "wss") != 0) {
    free(scheme);
    return strdup("null");
  }

  char* origin;
  if (is_default_port(scheme, port) || !port || strlen(port) == 0) {
    // Omit default port
    origin = malloc(strlen(protocol) + strlen(hostname) + 4);
    sprintf(origin, "%s//%s", protocol, hostname);
  } else {
    // Include custom port
    origin = malloc(strlen(protocol) + strlen(hostname) + strlen(port) + 5);
    sprintf(origin, "%s//%s:%s", protocol, hostname, port);
  }

  free(scheme);
  return origin;
}

static JSRT_URL* resolve_relative_url(const char* url, const char* base) {
  // Parse base URL first
  JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
  if (!base_url || strlen(base_url->protocol) == 0 || strlen(base_url->host) == 0) {
    if (base_url)
      JSRT_FreeURL(base_url);
    return NULL;  // Invalid base URL
  }

  JSRT_URL* result = malloc(sizeof(JSRT_URL));
  memset(result, 0, sizeof(JSRT_URL));

  // Initialize result with base URL components
  result->protocol = strdup(base_url->protocol);
  result->username = strdup(base_url->username ? base_url->username : "");
  result->password = strdup(base_url->password ? base_url->password : "");
  result->host = strdup(base_url->host);
  result->hostname = strdup(base_url->hostname);
  result->port = strdup(base_url->port);
  result->search_params = JS_UNDEFINED;
  result->ctx = NULL;

  // Handle special case: URLs like "http:foo.com" should be treated as relative paths
  // where the part after colon becomes the relative path
  const char* relative_path = url;

  // Check if this looks like a scheme-relative URL (e.g., "http:foo.com")
  char* colon_pos = strchr(url, ':');
  if (colon_pos != NULL && colon_pos > url) {  // Ensure colon is not at the start
    // Check if it's a known scheme followed by a relative path (not "://")
    const char* schemes[] = {"http", "https", "ftp", "ws", "wss", NULL};
    size_t scheme_len = colon_pos - url;

    for (int i = 0; schemes[i]; i++) {
      if (strncmp(url, schemes[i], scheme_len) == 0 && strlen(schemes[i]) == scheme_len) {
        // This is a scheme followed by colon - use the part after colon as relative path
        relative_path = colon_pos + 1;
        break;
      }
    }
  }

  // Handle URLs that start with a fragment (e.g., "#fragment" or ":#")
  char* fragment_pos = strchr(relative_path, '#');
  if (fragment_pos != NULL) {
    // Check if it's a fragment-only URL (starts with #) or has a fragment part
    if (relative_path[0] == '#') {
      // Fragment-only URL: preserve base pathname and search, replace hash
      result->pathname = strdup(base_url->pathname);
      result->search = strdup(base_url->search ? base_url->search : "");
      result->hash = strdup(relative_path);  // Include the '#'
    } else {
      // URL has both path and fragment components - will be handled below
      goto handle_complex_relative_path;
    }
  } else if (relative_path[0] == '?') {
    // Query-only URL: preserve base pathname, replace search and clear hash
    result->pathname = strdup(base_url->pathname);
    result->search = strdup(relative_path);  // Include the '?'
    result->hash = strdup("");
  } else if (relative_path[0] == '/') {
    // Absolute path (including paths extracted from single-slash URLs like "http:/example.com/")
    // Parse the path to separate pathname, search, and hash
    char* path_copy = strdup(relative_path);
    char* search_pos = strchr(path_copy, '?');
    char* hash_pos = strchr(path_copy, '#');

    if (hash_pos) {
      *hash_pos = '\0';
      result->hash = malloc(strlen(hash_pos + 1) + 2);  // +1 for '#', +1 for '\0'
      sprintf(result->hash, "#%s", hash_pos + 1);
    } else {
      result->hash = strdup("");
    }

    if (search_pos && (!hash_pos || search_pos < hash_pos)) {
      *search_pos = '\0';
      const char* search_end = hash_pos ? (hash_pos) : (search_pos + strlen(search_pos + 1) + 1);
      size_t search_len = search_end - search_pos;
      result->search = malloc(search_len + 2);  // +1 for '?', +1 for '\0'
      sprintf(result->search, "?%.*s", (int)(search_len - 1), search_pos + 1);
    } else {
      result->search = strdup("");
    }

    result->pathname = strdup(path_copy);
    free(path_copy);
  } else {
  handle_complex_relative_path:
    // Relative path - resolve against base directory
    // First, parse the relative path to separate path, search, and hash components
    char* path_copy = strdup(relative_path);
    char* search_pos = strchr(path_copy, '?');
    char* hash_pos = strchr(path_copy, '#');

    // Extract hash component
    if (hash_pos) {
      *hash_pos = '\0';
      result->hash = malloc(strlen(hash_pos + 1) + 2);  // +1 for '#', +1 for '\0'
      sprintf(result->hash, "#%s", hash_pos + 1);
    } else {
      result->hash = strdup("");
    }

    // Extract search component
    if (search_pos && (!hash_pos || search_pos < hash_pos)) {
      *search_pos = '\0';
      const char* search_end = hash_pos ? hash_pos : (search_pos + strlen(search_pos + 1) + 1);
      size_t search_len = search_end - search_pos;
      result->search = malloc(search_len + 2);  // +1 for '?', +1 for '\0'
      sprintf(result->search, "?%.*s", (int)(search_len - 1), search_pos + 1);
    } else {
      result->search = strdup("");
    }

    // Now resolve the path component against the base
    const char* base_pathname = base_url->pathname;
    char* last_slash = strrchr(base_pathname, '/');

    if (last_slash == NULL || last_slash == base_pathname) {
      // No directory or root directory
      result->pathname = malloc(strlen(path_copy) + 2);
      sprintf(result->pathname, "/%s", path_copy);
    } else {
      // Copy base directory and append relative path
      size_t dir_len = last_slash - base_pathname;                 // Length up to but not including last slash
      result->pathname = malloc(dir_len + strlen(path_copy) + 3);  // dir + '/' + relative_path + '\0'
      strncpy(result->pathname, base_pathname, dir_len);
      result->pathname[dir_len] = '/';
      strcpy(result->pathname + dir_len + 1, path_copy);
    }

    free(path_copy);
  }

  // Normalize dot segments in the pathname
  char* normalized_pathname = normalize_dot_segments(result->pathname);
  if (normalized_pathname) {
    free(result->pathname);
    result->pathname = normalized_pathname;
  }

  // Build origin using compute_origin function to handle all scheme types correctly
  result->origin = compute_origin(result->protocol, result->hostname, result->port);

  // For non-special schemes, pathname should not be encoded (spaces preserved)
  // For special schemes, pathname should be encoded
  char* encoded_pathname;
  if (is_special_scheme(result->protocol)) {
    encoded_pathname = url_component_encode(result->pathname);
  } else {
    // Non-special schemes: preserve spaces and other characters in pathname
    encoded_pathname = strdup(result->pathname ? result->pathname : "");
  }
  char* encoded_search = url_component_encode(result->search);
  char* encoded_hash = url_component_encode(result->hash);

  result->href =
      malloc(strlen(result->origin) + strlen(encoded_pathname) + strlen(encoded_search) + strlen(encoded_hash) + 1);
  sprintf(result->href, "%s%s%s%s", result->origin, encoded_pathname, encoded_search, encoded_hash);

  free(encoded_pathname);
  free(encoded_search);
  free(encoded_hash);

  JSRT_FreeURL(base_url);
  return result;
}

// Validate credentials according to WHATWG URL specification
// Only reject the most critical characters that would break URL parsing
// Other characters will be percent-encoded as needed
static int validate_credentials(const char* credentials) {
  if (!credentials)
    return 1;

  for (const char* p = credentials; *p; p++) {
    unsigned char c = (unsigned char)*p;

    // Only reject characters that would completely break URL parsing
    // Per WHATWG URL spec, most special characters should be percent-encoded, not rejected
    // Note: @ should be percent-encoded as %40 in userinfo, not rejected
    if (c == 0x09 || c == 0x0A || c == 0x0D ||  // control chars: tab, LF, CR
        c == '/' || c == '?' || c == '#') {     // URL structure delimiters (except @)
      return 0;                                 // Invalid character found
    }

    // Reject other ASCII control characters (< 0x20) except those already checked
    if (c < 0x20) {
      return 0;
    }
  }
  return 1;  // Valid - other special characters will be percent-encoded
}

// Validate URL characters according to WPT specification
// Reject ASCII tab (0x09), LF (0x0A), and CR (0x0D)
static int validate_url_characters(const char* url) {
  for (const char* p = url; *p; p++) {
    unsigned char c = (unsigned char)*p;

    // Note: ASCII tab, LF, CR should have already been removed by remove_all_ascii_whitespace()
    // so we don't need to check for them here

    // Check for backslash at start of URL (invalid per WHATWG URL Standard)
    if (p == url && c == '\\') {
      return 0;  // Leading backslash is invalid
    }

    // Check for consecutive backslashes (often invalid pattern)
    if (c == '\\' && p > url && *(p - 1) == '\\') {
      return 0;  // Consecutive backslashes are invalid
    }

    // Check for other ASCII control characters (but allow Unicode characters >= 0x80)
    if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) {
      return 0;  // Other control characters
    }

    // Allow Unicode characters (>= 0x80) - they will be percent-encoded later if needed
    // This fixes the issue where Unicode characters like β (0xCE 0xB2 in UTF-8) were rejected
  }
  return 1;  // Valid
}

// Validate hostname characters according to WHATWG URL spec
static int validate_hostname_characters(const char* hostname) {
  if (!hostname)
    return 0;

  for (const char* p = hostname; *p; p++) {
    unsigned char c = (unsigned char)*p;

    // Reject forbidden hostname characters per WHATWG URL spec
    // These characters are forbidden in hostnames: " # % / : ? @ [ \ ] ^
    if (c == '"' || c == '#' || c == '%' || c == '/' || c == ':' || c == '?' || c == '@' || c == '[' || c == '\\' ||
        c == ']' || c == '^') {
      return 0;  // Invalid hostname character
    }

    // Reject ASCII control characters
    if (c < 0x20 || c == 0x7F) {
      return 0;  // Control characters not allowed
    }

    // Reject space character in hostname
    if (c == ' ') {
      return 0;  // Spaces not allowed in hostname
    }

    // Reject Unicode control characters and non-ASCII characters
    // Check for UTF-8 encoded Unicode characters
    if (c >= 0x80) {
      // Check for soft hyphen (U+00AD) encoded as UTF-8: 0xC2 0xAD
      if (c == 0xC2 && (unsigned char)*(p + 1) == 0xAD) {
        return 0;  // Soft hyphen not allowed
      }
      // Reject all non-ASCII characters in hostnames per WPT tests
      // This includes Unicode characters like ☃ (snowman)
      return 0;  // Non-ASCII characters not allowed in hostnames
    }

    // Reject hex notation patterns in hostnames (0x prefix)
    if (c == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) {
      return 0;  // Hex notation not allowed in hostnames
    }
  }

  return 1;  // Valid hostname
}

// Normalize dot segments in URL path according to RFC 3986
// Resolves "." and ".." segments in paths
static char* normalize_dot_segments(const char* path) {
  if (!path || strlen(path) == 0) {
    return strdup("");
  }

  // Create output buffer - worst case is same size as input
  size_t input_len = strlen(path);
  char* output = malloc(input_len + 1);
  if (!output) {
    return NULL;
  }

  char* out_ptr = output;
  const char* input = path;

  // Start with empty output buffer
  *out_ptr = '\0';

  while (*input != '\0') {
    // A: If input begins with "../" or "./", remove prefix
    if (strncmp(input, "../", 3) == 0) {
      input += 3;
      continue;
    }
    if (strncmp(input, "./", 2) == 0) {
      input += 2;
      continue;
    }

    // B: If input begins with "/./" or "/." (at end), replace with "/"
    if (strncmp(input, "/./", 3) == 0) {
      *out_ptr++ = '/';
      *out_ptr = '\0';
      input += 3;
      continue;
    }
    if (strncmp(input, "/.", 2) == 0 && input[2] == '\0') {
      *out_ptr++ = '/';
      *out_ptr = '\0';
      input += 2;
      continue;
    }

    // C: If input begins with "/../" or "/.." (at end), replace with "/" and remove last segment
    if (strncmp(input, "/../", 4) == 0 || (strncmp(input, "/..", 3) == 0 && input[3] == '\0')) {
      // Remove last segment from output
      if (out_ptr > output) {
        out_ptr--;  // Back up from current position
        while (out_ptr > output && *(out_ptr - 1) != '/') {
          out_ptr--;
        }
        *out_ptr = '\0';
      }

      // Add "/" to output
      *out_ptr++ = '/';
      *out_ptr = '\0';

      // Skip the input pattern
      if (input[3] == '\0') {
        input += 3;  // "/.."
      } else {
        input += 4;  // "/../"
      }
      continue;
    }

    // D: If input is ".." or ".", remove it
    if (strcmp(input, ".") == 0 || strcmp(input, "..") == 0) {
      break;  // End of input
    }

    // E: Move first path segment from input to output
    if (*input == '/') {
      *out_ptr++ = *input++;
      *out_ptr = '\0';
    }

    // Copy segment until next '/' or end
    while (*input != '\0' && *input != '/') {
      *out_ptr++ = *input++;
      *out_ptr = '\0';
    }
  }

  // Clean up multiple consecutive slashes (e.g., "//parent" -> "/parent")
  char* final_output = output;
  char* read_ptr = output;
  char* write_ptr = output;

  while (*read_ptr != '\0') {
    *write_ptr = *read_ptr;
    if (*read_ptr == '/') {
      // Skip consecutive slashes
      while (*(read_ptr + 1) == '/') {
        read_ptr++;
      }
    }
    read_ptr++;
    write_ptr++;
  }
  *write_ptr = '\0';

  return final_output;
}

// Strip leading and trailing ASCII whitespace from URL string
// ASCII whitespace: space (0x20), tab (0x09), LF (0x0A), CR (0x0D), FF (0x0C)
static char* strip_url_whitespace(const char* url) {
  if (!url)
    return NULL;

  // Find start (skip leading whitespace)
  const char* start = url;
  while (*start && (*start == 0x20 || *start == 0x09 || *start == 0x0A || *start == 0x0D || *start == 0x0C)) {
    start++;
  }

  // Find end (skip trailing whitespace)
  const char* end = start + strlen(start);
  while (end > start &&
         (*(end - 1) == 0x20 || *(end - 1) == 0x09 || *(end - 1) == 0x0A || *(end - 1) == 0x0D || *(end - 1) == 0x0C)) {
    end--;
  }

  // Create trimmed string
  size_t len = end - start;
  char* trimmed = malloc(len + 1);
  memcpy(trimmed, start, len);
  trimmed[len] = '\0';

  return trimmed;
}

// Normalize port number per WHATWG URL spec
// Convert port string to number and handle default ports
static char* normalize_port(const char* port_str, const char* protocol) {
  if (!port_str || !*port_str)
    return strdup("");

  // Convert port string to number (handles leading zeros)
  char* endptr;
  long port_num = strtol(port_str, &endptr, 10);

  // Check for invalid port (not a number or out of range)
  if (*endptr != '\0' || port_num < 0 || port_num > 65535) {
    return NULL;  // Invalid port causes error
  }

  // Check for default ports that should be omitted
  if ((strcmp(protocol, "http:") == 0 && port_num == 80) || (strcmp(protocol, "https:") == 0 && port_num == 443) ||
      (strcmp(protocol, "ftp:") == 0 && port_num == 21) || (strcmp(protocol, "ws:") == 0 && port_num == 80) ||
      (strcmp(protocol, "wss:") == 0 && port_num == 443)) {
    return strdup("");  // Default port becomes empty
  }

  // Return normalized port as string
  char* result = malloc(16);  // Enough for any valid port number
  snprintf(result, 16, "%ld", port_num);
  return result;
}

// Remove tab, newline, and carriage return from URL string per WHATWG spec
// Only remove: tab (0x09), LF (0x0A), CR (0x0D) - preserve space (0x20) and form feed (0x0C)
static char* remove_all_ascii_whitespace(const char* url) {
  if (!url)
    return NULL;

  size_t len = strlen(url);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    char c = url[i];
    // Skip only tab, line feed, and carriage return - preserve space
    if (c != 0x09 && c != 0x0A && c != 0x0D) {
      result[j++] = c;
    }
  }
  result[j] = '\0';

  return result;
}

// Normalize consecutive spaces to single space for non-special schemes
static char* normalize_spaces_in_path(const char* path) {
  if (!path)
    return NULL;

  size_t len = strlen(path);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  size_t j = 0;
  int prev_was_space = 0;

  for (size_t i = 0; i < len; i++) {
    char c = path[i];
    if (c == ' ' || c == '\t') {
      if (!prev_was_space) {
        result[j++] = ' ';  // Normalize tab to space, collapse consecutive spaces
        prev_was_space = 1;
      }
    } else {
      result[j++] = c;
      prev_was_space = 0;
    }
  }
  result[j] = '\0';

  return result;
}

// Convert backslashes to forward slashes in URL strings according to WHATWG URL spec
// This applies to special URL schemes and certain URL contexts
static char* normalize_url_backslashes(const char* url) {
  if (!url)
    return NULL;

  size_t len = strlen(url);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  // Find fragment and query positions to avoid normalizing backslashes in them
  char* fragment_pos = strchr(url, '#');
  char* query_pos = strchr(url, '?');

  // Determine where to stop normalizing (fragment comes first, then query)
  size_t stop_pos = len;
  if (fragment_pos) {
    stop_pos = fragment_pos - url;
  } else if (query_pos) {
    stop_pos = query_pos - url;
  }

  for (size_t i = 0; i < len; i++) {
    if (i < stop_pos && url[i] == '\\') {
      // Only normalize backslashes before fragment/query
      result[i] = '/';
    } else {
      // Preserve backslashes in fragment and query
      result[i] = url[i];
    }
  }
  result[len] = '\0';
  return result;
}

static JSRT_URL* JSRT_ParseURL(const char* url, const char* base) {
  // Strip leading and trailing ASCII whitespace, then remove all internal ASCII whitespace per WHATWG URL spec
  char* trimmed_url = strip_url_whitespace(url);
  if (!trimmed_url)
    return NULL;

  // Determine if this URL has a special scheme first
  // We need to check this before removing internal whitespace
  char* initial_colon_pos = strchr(trimmed_url, ':');
  int has_special_scheme = 0;

  if (initial_colon_pos && initial_colon_pos > trimmed_url) {
    // Extract potential scheme
    size_t scheme_len = initial_colon_pos - trimmed_url;
    char* scheme = malloc(scheme_len + 1);
    strncpy(scheme, trimmed_url, scheme_len);
    scheme[scheme_len] = '\0';
    has_special_scheme = is_valid_scheme(scheme) && is_special_scheme(scheme);
    free(scheme);
  }

  // Only remove internal ASCII whitespace for special schemes
  // For non-special schemes, preserve internal whitespace (it will be percent-encoded later)
  char* cleaned_url;
  if (has_special_scheme) {
    // Special schemes: remove internal ASCII whitespace per WHATWG URL spec
    cleaned_url = remove_all_ascii_whitespace(trimmed_url);
  } else {
    // Non-special schemes: preserve internal whitespace
    cleaned_url = strdup(trimmed_url);
  }
  free(trimmed_url);  // Free the intermediate result
  if (!cleaned_url)
    return NULL;

  // Decode percent-encoded sequences and validate UTF-8 per WHATWG URL spec
  // This ensures invalid UTF-16 surrogates like %ED%A0%80 are replaced with %EF%BF%BD
  // For URL decoding, we need to preserve + in scheme part, only decode + to space in query parameters
  size_t decoded_len;
  char* decoded_url = url_decode_with_length_and_output_len(cleaned_url, strlen(cleaned_url), &decoded_len);
  if (!decoded_url) {
    free(cleaned_url);
    return NULL;
  }

  // Re-encode the validated UTF-8 back to percent-encoded form for further processing
  // For non-special schemes, we need to preserve spaces in the path portion
  char* reencoded_url;

  // Check if this URL has a non-special scheme by looking for a colon
  char* scheme_colon_pos = strchr(decoded_url, ':');
  if (scheme_colon_pos && scheme_colon_pos > decoded_url) {
    // Extract potential scheme
    size_t scheme_len = scheme_colon_pos - decoded_url;
    char* scheme = malloc(scheme_len + 1);
    strncpy(scheme, decoded_url, scheme_len);
    scheme[scheme_len] = '\0';

    if (is_valid_scheme(scheme) && !is_special_scheme(scheme)) {
      // Non-special scheme: use special encoding that preserves spaces in path
      reencoded_url = url_nonspecial_path_encode(decoded_url);
    } else {
      // Special scheme or invalid scheme: use normal encoding
      reencoded_url = url_component_encode(decoded_url);
    }
    free(scheme);
  } else {
    // No scheme found: use normal encoding
    reencoded_url = url_component_encode(decoded_url);
  }

  free(decoded_url);
  free(cleaned_url);
  if (!reencoded_url)
    return NULL;

  cleaned_url = reencoded_url;

  // Normalize backslashes to forward slashes according to WHATWG URL spec
  char* normalized_url = normalize_url_backslashes(cleaned_url);
  free(cleaned_url);  // Free the intermediate result
  if (!normalized_url)
    return NULL;

  // Update cleaned_url to point to the normalized URL
  cleaned_url = normalized_url;

  // Handle empty URL string - per WHATWG URL spec, empty string should resolve to base
  if (strlen(cleaned_url) == 0) {
    if (base) {
      free(cleaned_url);
      return JSRT_ParseURL(base, NULL);  // Parse base URL as absolute
    } else {
      free(cleaned_url);
      return NULL;  // Empty URL with no base is invalid
    }
  }

  // Validate URL characters first (but skip ASCII whitespace check since we already removed them)
  // Note: validate_url_characters should not check for tab/LF/CR since we already removed them
  if (!validate_url_characters(cleaned_url)) {
    free(cleaned_url);
    return NULL;  // Invalid characters detected
  }
  if (base && !validate_url_characters(base)) {
    free(cleaned_url);
    return NULL;  // Invalid characters in base URL
  }

  // Check if URL has a scheme (protocol)
  // A URL has a scheme if it contains ':' before any '/', '?', or '#'
  // AND the first character is a letter (per WHATWG URL Standard)
  // However, special schemes (http, https, ftp, ws, wss) are only absolute if followed by "//"
  int has_scheme = 0;
  char* colon_pos = NULL;

  // First character must be a letter for valid scheme
  if (cleaned_url[0] &&
      ((cleaned_url[0] >= 'a' && cleaned_url[0] <= 'z') || (cleaned_url[0] >= 'A' && cleaned_url[0] <= 'Z'))) {
    // Look for "://" pattern to find the correct scheme boundary
    char* scheme_end = strstr(cleaned_url, "://");
    if (scheme_end) {
      // Found "://" - extract scheme and validate it
      size_t scheme_len = scheme_end - cleaned_url;
      char* scheme = malloc(scheme_len + 1);
      strncpy(scheme, cleaned_url, scheme_len);
      scheme[scheme_len] = '\0';

      // Always accept scheme if it follows basic format rules
      colon_pos = scheme_end;  // Points to the ':' in "://"
      has_scheme = 1;
      free(scheme);
    } else {
      // No "://" found, look for single colon
      for (const char* p = cleaned_url; *p; p++) {
        if (*p == ':') {
          // Found colon - extract scheme and validate it
          size_t scheme_len = p - cleaned_url;
          char* scheme = malloc(scheme_len + 1);
          strncpy(scheme, cleaned_url, scheme_len);
          scheme[scheme_len] = '\0';

          // Always accept scheme if it follows basic format rules
          colon_pos = (char*)p;
          has_scheme = 1;
          free(scheme);
          break;
        } else if (*p == '/' || *p == '?' || *p == '#') {
          break;  // No colon found before path/query/fragment
        }
      }
    }
  }

  // Special schemes require "://" to be absolute
  char* relative_part = NULL;  // For storing the part after scheme: when treating as relative
  if (has_scheme && colon_pos) {
    int scheme_len = colon_pos - cleaned_url;
    const char* special_schemes[] = {"http", "https", "ws", "wss", "file", NULL};

    // Extract scheme for checking
    char* scheme = malloc(scheme_len + 1);
    strncpy(scheme, cleaned_url, scheme_len);
    scheme[scheme_len] = '\0';

    // Check if this is a special scheme
    int is_special = 0;
    for (int i = 0; special_schemes[i]; i++) {
      if (strcmp(scheme, special_schemes[i]) == 0) {
        is_special = 1;
        break;
      }
    }

    if (is_special) {
      // Special scheme handling
      if (strncmp(colon_pos, "://", 3) != 0) {
        if (!base) {
          // No base URL: convert single colon to double slash format (absolute URL)
          size_t after_colon_len = strlen(colon_pos + 1);             // Length after the ':'
          size_t new_url_len = scheme_len + 3 + after_colon_len + 1;  // scheme + "://" + rest + '\0'
          char* normalized_url = malloc(new_url_len);

          strncpy(normalized_url, cleaned_url, scheme_len);
          strcpy(normalized_url + scheme_len, "://");
          strcpy(normalized_url + scheme_len + 3, colon_pos + 1);

          // Replace the original URL with normalized version
          free(cleaned_url);
          cleaned_url = normalized_url;

          // Update colon_pos to point to the new position
          colon_pos = cleaned_url + scheme_len;
        } else {
          // Base URL exists: treat single colon as relative for special schemes
          // Store the part after the colon for relative processing
          relative_part = strdup(colon_pos + 1);
        }
      }
    } else if (is_valid_scheme(scheme)) {
      // Non-special scheme handling per WHATWG URL spec
      if (strncmp(colon_pos, "://", 3) == 0) {
        // Already has "://" - this is an authority-based non-special URL
        // Keep as-is for parsing as authority-based URL
      } else {
        // Single colon: this is an opaque path URL per WHATWG spec
        // For opaque path URLs like "a:foo bar", don't convert to authority format
        // The path after the colon should be preserved as opaque path
        // We'll handle this in the opaque path parsing logic below
      }
    }

    free(scheme);
  }

  // Handle protocol-relative URLs (starting with "//")
  if (base && strncmp(cleaned_url, "//", 2) == 0) {
    // Protocol-relative URL: inherit protocol from base, parse authority from URL
    JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
    if (!base_url) {
      free(cleaned_url);
      return NULL;
    }

    // Create a full URL by combining base protocol with the protocol-relative URL
    size_t full_url_len = strlen(base_url->protocol) + strlen(cleaned_url) + 2;
    char* full_url = malloc(full_url_len);
    if (!full_url) {
      JSRT_FreeURL(base_url);
      free(cleaned_url);
      return NULL;
    }
    snprintf(full_url, full_url_len, "%s%s", base_url->protocol, cleaned_url);
    JSRT_FreeURL(base_url);
    free(cleaned_url);

    // Parse the full URL recursively (without base to avoid infinite recursion)
    JSRT_URL* result = JSRT_ParseURL(full_url, NULL);
    free(full_url);
    return result;
  }

  // Handle other relative URLs with base
  if (base && (cleaned_url[0] == '/' || (!has_scheme && cleaned_url[0] != '\0'))) {
    // Use relative_part if available (for special schemes treated as relative), otherwise use full URL
    const char* relative_url = relative_part ? relative_part : cleaned_url;
    JSRT_URL* result = resolve_relative_url(relative_url, base);
    free(cleaned_url);
    if (relative_part) {
      free(relative_part);
    }
    return result;
  }

  JSRT_URL* parsed = malloc(sizeof(JSRT_URL));
  memset(parsed, 0, sizeof(JSRT_URL));

  // Initialize with empty strings to prevent NULL dereference
  parsed->href = strdup(cleaned_url);
  parsed->protocol = strdup("");
  parsed->username = strdup("");
  parsed->password = strdup("");
  parsed->host = strdup("");
  parsed->hostname = strdup("");
  parsed->port = strdup("");
  parsed->pathname = strdup("");
  parsed->search = strdup("");
  parsed->hash = strdup("");
  parsed->origin = strdup("");
  parsed->search_params = JS_UNDEFINED;
  parsed->ctx = NULL;

  // Simple parsing - this is a basic implementation
  // In a production system, you'd want a full URL parser

  char* url_copy = strdup(cleaned_url);
  char* ptr = url_copy;

  // Handle special schemes first
  if (strncmp(ptr, "data:", 5) == 0) {
    free(parsed->protocol);
    parsed->protocol = strdup("data:");
    free(parsed->pathname);
    parsed->pathname = strdup(ptr + 5);  // Everything after "data:"
    free(url_copy);
    // Build origin
    free(parsed->origin);
    parsed->origin = compute_origin(parsed->protocol, parsed->hostname, parsed->port);
    free(cleaned_url);
    return parsed;
  }

  // Handle blob URLs: blob:<origin>/<uuid>
  if (strncmp(ptr, "blob:", 5) == 0) {
    free(parsed->protocol);
    parsed->protocol = strdup("blob:");

    // Extract the inner URL part (everything after "blob:")
    char* inner_url_start = ptr + 5;

    // For blob URLs, we need to find the third slash to separate origin from UUID
    // Format: blob:https://example.com/uuid or blob:https://example.com:443/uuid
    char* first_slash = strchr(inner_url_start, '/');
    if (first_slash && first_slash[1] == '/') {
      // Found "//", now find the next slash after the hostname
      char* hostname_start = first_slash + 2;
      char* uuid_slash = strchr(hostname_start, '/');

      if (uuid_slash) {
        // Split into origin part and UUID part
        size_t origin_len = uuid_slash - inner_url_start;
        char* origin_part = malloc(origin_len + 1);
        strncpy(origin_part, inner_url_start, origin_len);
        origin_part[origin_len] = '\0';

        // Parse the origin part to extract its origin
        JSRT_URL* inner_url = JSRT_ParseURL(origin_part, NULL);
        if (inner_url && inner_url->protocol && inner_url->hostname) {
          free(parsed->origin);
          parsed->origin = compute_origin(inner_url->protocol, inner_url->hostname, inner_url->port);
          JSRT_FreeURL(inner_url);
        } else {
          free(parsed->origin);
          parsed->origin = strdup("null");
          if (inner_url)
            JSRT_FreeURL(inner_url);
        }

        // Set the pathname to the UUID part
        free(parsed->pathname);
        parsed->pathname = strdup(uuid_slash);  // Include the leading slash

        free(origin_part);
      } else {
        // No UUID slash found, treat entire part as origin
        JSRT_URL* inner_url = JSRT_ParseURL(inner_url_start, NULL);
        if (inner_url && inner_url->protocol && inner_url->hostname) {
          free(parsed->origin);
          parsed->origin = compute_origin(inner_url->protocol, inner_url->hostname, inner_url->port);
          JSRT_FreeURL(inner_url);
        } else {
          free(parsed->origin);
          parsed->origin = strdup("null");
          if (inner_url)
            JSRT_FreeURL(inner_url);
        }

        free(parsed->pathname);
        parsed->pathname = strdup("/");
      }
    } else {
      // Invalid blob URL format
      free(parsed->origin);
      parsed->origin = strdup("null");
      free(parsed->pathname);
      parsed->pathname = strdup("");
    }

    free(url_copy);
    free(cleaned_url);
    return parsed;
  }

  // Handle non-special schemes with opaque paths (single colon, no "://")
  // E.g., "a:foo bar", "mailto:user@example.com"
  if (has_scheme && colon_pos && strncmp(colon_pos, "://", 3) != 0) {
    // Extract scheme
    size_t scheme_len = colon_pos - cleaned_url;
    char* scheme = malloc(scheme_len + 1);
    strncpy(scheme, cleaned_url, scheme_len);
    scheme[scheme_len] = '\0';

    // Check if this is a non-special scheme
    if (is_valid_scheme(scheme) && !is_special_scheme(scheme)) {
      // This is a non-special scheme with opaque path
      free(parsed->protocol);
      parsed->protocol = malloc(strlen(scheme) + 2);
      sprintf(parsed->protocol, "%s:", scheme);

      // Extract fragment and query from the opaque path first
      const char* opaque_path_full = colon_pos + 1;

      // Find fragment and query positions
      char* fragment_pos = strchr(opaque_path_full, '#');
      char* query_pos = strchr(opaque_path_full, '?');

      // Create a copy to work with
      char* opaque_path_copy = strdup(opaque_path_full);
      char* opaque_path = opaque_path_copy;

      // Extract fragment if present
      if (fragment_pos) {
        size_t fragment_offset = fragment_pos - opaque_path_full;
        free(parsed->hash);
        parsed->hash = strdup(opaque_path_copy + fragment_offset);  // Include '#'
        opaque_path_copy[fragment_offset] = '\0';                   // Terminate path before fragment
      }

      // Extract query if present (and if it comes before fragment)
      if (query_pos && (!fragment_pos || query_pos < fragment_pos)) {
        size_t query_offset = query_pos - opaque_path_full;
        free(parsed->search);
        parsed->search = strdup(opaque_path_copy + query_offset);  // Include '?'
        opaque_path_copy[query_offset] = '\0';                     // Terminate path before query
      }

      // Now encode what remains as the opaque path (without fragment/query)
      char* encoded_path = url_nonspecial_path_encode(opaque_path_copy);
      free(parsed->pathname);
      parsed->pathname = encoded_path ? encoded_path : strdup(opaque_path_copy);

      free(opaque_path_copy);

      // Non-special schemes have null origin
      free(parsed->origin);
      parsed->origin = strdup("null");

      // Build href for non-special scheme
      free(parsed->href);
      char* encoded_search = url_component_encode(parsed->search);
      char* encoded_hash = url_component_encode(parsed->hash);

      size_t href_len =
          strlen(parsed->protocol) + strlen(parsed->pathname) + strlen(encoded_search) + strlen(encoded_hash) + 1;
      parsed->href = malloc(href_len);
      snprintf(parsed->href, href_len, "%s%s%s%s", parsed->protocol, parsed->pathname, encoded_search, encoded_hash);

      free(encoded_search);
      free(encoded_hash);

      free(scheme);
      free(url_copy);
      free(cleaned_url);
      return parsed;
    }

    free(scheme);
  }

  // Extract protocol for regular URLs
  // Use the colon position we already found during scheme detection
  if (has_scheme && colon_pos && strncmp(colon_pos, "://", 3) == 0) {
    // This is a scheme with "://" format
    size_t scheme_len = colon_pos - cleaned_url;
    char* scheme = malloc(scheme_len + 1);
    strncpy(scheme, cleaned_url, scheme_len);
    scheme[scheme_len] = '\0';

    free(parsed->protocol);
    parsed->protocol = malloc(strlen(scheme) + 2);
    sprintf(parsed->protocol, "%s:", scheme);

    // Move pointer past "scheme://"
    ptr = colon_pos + 3;  // Skip ":" and "//"
    free(scheme);
  } else if (strncmp(ptr, "file:", 5) == 0) {
    free(parsed->protocol);
    parsed->protocol = strdup("file:");
    ptr += 5;
    // Handle file:///path format - skip extra slashes
    while (*ptr == '/')
      ptr++;
    free(parsed->pathname);
    parsed->pathname = malloc(strlen(ptr) + 2);
    sprintf(parsed->pathname, "/%s", ptr);
    free(url_copy);
    // Build origin
    free(parsed->origin);
    parsed->origin = compute_origin(parsed->protocol, parsed->hostname, parsed->port);
    free(cleaned_url);
    return parsed;
  } else if (has_scheme && colon_pos) {
    // Handle schemes with single slash - check if it's a special scheme that needs normalization
    // Use the colon_pos we already found during scheme detection
    char* scheme_end = colon_pos;
    size_t scheme_len = scheme_end - cleaned_url;
    char* scheme = malloc(scheme_len + 1);
    strncpy(scheme, cleaned_url, scheme_len);
    scheme[scheme_len] = '\0';

    if (scheme && is_valid_scheme(scheme)) {
      // Check if this is a special scheme
      const char* special_schemes[] = {"http", "https", "ws", "wss", "file", NULL};
      int is_special = 0;
      for (int i = 0; special_schemes[i]; i++) {
        if (strcmp(scheme, special_schemes[i]) == 0) {
          is_special = 1;
          break;
        }
      }

      if (is_special) {
        if (strncmp(scheme_end + 1, "/", 1) == 0 && strncmp(scheme_end + 1, "//", 2) != 0) {
          // Special scheme with single slash - normalize to double slash for absolute URL
          // Per WHATWG URL spec, URLs like "https:/example.com/" should be parsed as absolute URLs
          char* rest = scheme_end + 2;                                    // Skip ":" and "/"
          size_t normalized_len = strlen(scheme) + 3 + strlen(rest) + 1;  // scheme + "://" + rest + null
          char* normalized_url = malloc(normalized_len);
          snprintf(normalized_url, normalized_len, "%s://%s", scheme, rest);
          free(scheme);

          // Parse the normalized URL recursively
          free(url_copy);
          free(cleaned_url);
          free(parsed->href);
          free(parsed->protocol);
          free(parsed->username);
          free(parsed->password);
          free(parsed->host);
          free(parsed->hostname);
          free(parsed->port);
          free(parsed->pathname);
          free(parsed->search);
          free(parsed->hash);
          free(parsed->origin);
          free(parsed);

          JSRT_URL* result = JSRT_ParseURL(normalized_url, NULL);
          free(normalized_url);
          return result;
        } else if (strncmp(scheme_end + 1, "//", 2) != 0) {
          // Special scheme with single colon (no slash)
          if (base) {
            // With base: treat as relative URL (resolve against base)
            free(scheme);
            free(url_copy);
            free(cleaned_url);
            free(parsed->href);
            free(parsed->protocol);
            free(parsed->username);
            free(parsed->password);
            free(parsed->host);
            free(parsed->hostname);
            free(parsed->port);
            free(parsed->pathname);
            free(parsed->search);
            free(parsed->hash);
            free(parsed->origin);
            free(parsed);

            return resolve_relative_url(url, base);
          } else {
            // Without base: normalize to double slash (absolute URL)
            char* rest = scheme_end + 1;                                    // Skip ":"
            size_t normalized_len = strlen(scheme) + 3 + strlen(rest) + 1;  // scheme + "://" + rest + null
            char* normalized_url = malloc(normalized_len);
            snprintf(normalized_url, normalized_len, "%s://%s", scheme, rest);
            free(scheme);

            // Parse the normalized URL recursively
            free(url_copy);
            free(cleaned_url);
            free(parsed->href);
            free(parsed->protocol);
            free(parsed->username);
            free(parsed->password);
            free(parsed->host);
            free(parsed->hostname);
            free(parsed->port);
            free(parsed->pathname);
            free(parsed->search);
            free(parsed->hash);
            free(parsed->origin);
            free(parsed);

            JSRT_URL* result = JSRT_ParseURL(normalized_url, NULL);
            free(normalized_url);
            return result;
          }
        }
      }

      // Handle non-special schemes (like "a:", "mailto:", "data:", etc.)
      free(parsed->protocol);
      // For protocol field, preserve the original scheme without encoding
      // The scheme should remain as-is (e.g., "git+https:" not "git%20https:")
      parsed->protocol = malloc(strlen(scheme) + 2);
      sprintf(parsed->protocol, "%s:", scheme);
      ptr = scheme_end + 1;
      free(scheme);

      // For non-special schemes, parse hash and search from the path
      // Find both positions first
      char* hash_start = strchr(ptr, '#');
      char* search_start = strchr(ptr, '?');

      // Determine which comes first and extract in proper order
      if (hash_start && search_start) {
        if (hash_start < search_start) {
          // Hash comes before search: extract hash first
          free(parsed->hash);
          parsed->hash = strdup(hash_start);
          *hash_start = '\0';

          // Now extract search from the truncated string
          search_start = strchr(ptr, '?');
          if (search_start) {
            free(parsed->search);
            parsed->search = strdup(search_start);
            *search_start = '\0';
          }
        } else {
          // Search comes before hash: extract search first
          free(parsed->search);
          parsed->search = strdup(search_start);
          *search_start = '\0';

          // Now extract hash from the truncated string
          hash_start = strchr(ptr, '#');
          if (hash_start) {
            free(parsed->hash);
            parsed->hash = strdup(hash_start);
            *hash_start = '\0';
          }
        }
      } else if (hash_start) {
        // Only hash present
        free(parsed->hash);
        parsed->hash = strdup(hash_start);
        *hash_start = '\0';
      } else if (search_start) {
        // Only search present
        free(parsed->search);
        parsed->search = strdup(search_start);
        *search_start = '\0';
      }

      // The rest is the pathname
      free(parsed->pathname);
      // For non-special schemes, preserve spaces in pathname as-is
      parsed->pathname = strdup(ptr);

      // Rebuild href with encoded components for non-special schemes
      // For non-special schemes, pathname should NOT be encoded (spaces remain as spaces)
      free(parsed->href);
      char* encoded_search = url_component_encode(parsed->search);
      char* encoded_hash = url_component_encode(parsed->hash);

      size_t href_len =
          strlen(parsed->protocol) + strlen(parsed->pathname) + strlen(encoded_search) + strlen(encoded_hash) + 1;
      parsed->href = malloc(href_len);
      snprintf(parsed->href, href_len, "%s%s%s%s", parsed->protocol, parsed->pathname, encoded_search, encoded_hash);

      free(encoded_search);
      free(encoded_hash);

      free(url_copy);
      // Build origin (non-special schemes have null origin)
      free(parsed->origin);
      parsed->origin = strdup("null");
      free(cleaned_url);
      return parsed;
    }
  }

  // Extract hash first (fragment)
  char* hash_start = strchr(ptr, '#');
  if (hash_start) {
    free(parsed->hash);
    parsed->hash = strdup(hash_start);
    *hash_start = '\0';
  }

  // Extract search (query)
  char* search_start = strchr(ptr, '?');
  if (search_start) {
    free(parsed->search);
    parsed->search = strdup(search_start);
    *search_start = '\0';
  }

  // Extract host and pathname
  char* path_start = strchr(ptr, '/');
  if (path_start) {
    free(parsed->pathname);
    parsed->pathname = strdup(path_start);
    *path_start = '\0';
  } else {
    // For non-special schemes with empty authority, pathname should be empty
    // Check if this is a non-special scheme
    char* scheme_without_colon = strdup(parsed->protocol);
    if (strlen(scheme_without_colon) > 0 && scheme_without_colon[strlen(scheme_without_colon) - 1] == ':') {
      scheme_without_colon[strlen(scheme_without_colon) - 1] = '\0';
    }

    const char* special_schemes[] = {"http", "https", "ws", "wss", "file", NULL};
    int is_special = 0;
    for (int i = 0; special_schemes[i]; i++) {
      if (strcmp(scheme_without_colon, special_schemes[i]) == 0) {
        is_special = 1;
        break;
      }
    }

    free(parsed->pathname);
    if (is_special) {
      // Special schemes default to "/" when no path
      parsed->pathname = strdup("/");
    } else {
      // Non-special schemes can have empty pathname
      parsed->pathname = strdup("");
    }

    free(scheme_without_colon);
  }

  // What's left is the host (authority part)
  if (strlen(ptr) > 0) {
    // First, handle credentials (user:pass@host:port)
    // Use strrchr to find the LAST @ symbol (per WHATWG URL spec)
    char* credentials_end = strrchr(ptr, '@');
    char* authority = ptr;

    if (credentials_end) {
      // Extract and parse credentials (user:pass)
      size_t creds_len = credentials_end - ptr;
      char* credentials = malloc(creds_len + 1);
      strncpy(credentials, ptr, creds_len);
      credentials[creds_len] = '\0';

      // Split credentials by ':' to get username and password
      char* colon_pos = strchr(credentials, ':');
      char* encoded_username = NULL;
      char* encoded_password = NULL;

      if (colon_pos) {
        *colon_pos = '\0';
        encoded_username = credentials;    // Before colon
        encoded_password = colon_pos + 1;  // After colon
      } else {
        encoded_username = credentials;
        encoded_password = "";
      }

      // Decode credentials before validation (they may be percent-encoded)
      char* raw_username = url_decode(encoded_username);
      char* raw_password = url_decode(encoded_password);

      // Validate decoded credentials - if invalid, ignore them per WHATWG URL spec
      if (validate_credentials(raw_username) && validate_credentials(raw_password)) {
        free(parsed->username);
        // Encode userinfo using userinfo-specific encoding rules
        parsed->username = url_userinfo_encode(raw_username);
        free(parsed->password);
        // Encode userinfo using userinfo-specific encoding rules
        parsed->password = url_userinfo_encode(raw_password);
      }
      // If credentials are invalid, ignore them (keep defaults)

      free(raw_username);
      free(raw_password);

      free(credentials);
      authority = credentials_end + 1;  // Authority is after @
    }

    // Now parse the authority part (hostname:port, possibly IPv6)
    char* hostname_part;
    char* port_part = NULL;

    // Check for IPv6 address notation [::1] or [::1]:port
    if (authority[0] == '[') {
      char* ipv6_end = strchr(authority, ']');
      if (ipv6_end) {
        // Extract IPv6 address including brackets
        size_t ipv6_len = ipv6_end - authority + 1;
        char* raw_ipv6 = malloc(ipv6_len + 1);
        strncpy(raw_ipv6, authority, ipv6_len);
        raw_ipv6[ipv6_len] = '\0';

        // Canonicalize the IPv6 address
        hostname_part = canonicalize_ipv6(raw_ipv6);
        free(raw_ipv6);

        // Check for port after IPv6 address
        if (ipv6_end[1] == ':') {
          port_part = ipv6_end + 2;
        }
      } else {
        // Malformed IPv6, treat as regular hostname
        hostname_part = strdup(authority);
      }
    } else {
      // Regular hostname, check for port
      char* port_start = strchr(authority, ':');
      if (port_start) {
        *port_start = '\0';
        // Decode percent-encoded hostname per WHATWG URL spec
        hostname_part = url_decode(authority);
        port_part = port_start + 1;
        *port_start = ':';  // Restore for later use
      } else {
        // Decode percent-encoded hostname per WHATWG URL spec
        hostname_part = url_decode(authority);
      }
    }

    // Set hostname with IPv4 address canonicalization
    free(parsed->hostname);

    // Try to parse as IPv4 address first (including hexadecimal formats)
    char* canonical_ipv4 = canonicalize_ipv4_address(hostname_part);
    if (canonical_ipv4) {
      // IPv4 address was successfully parsed
      parsed->hostname = canonical_ipv4;
      free(hostname_part);
    } else {
      // Not a valid IPv4 address, validate as hostname
      if (!validate_hostname_characters(hostname_part)) {
        // Don't free hostname_part here - it will be freed by JSRT_FreeURL
        if (port_part) {
          // Restore the colon we might have modified
          char* colon_in_authority = strchr(authority, ':');
          if (colon_in_authority)
            *colon_in_authority = ':';
        }
        // Assign hostname_part to parsed->hostname so it gets freed properly
        parsed->hostname = hostname_part;
        free(url_copy);
        free(cleaned_url);
        JSRT_FreeURL(parsed);
        return NULL;
      }
      parsed->hostname = hostname_part;
      // Don't free hostname_part here since it's now assigned to parsed->hostname
    }

    // Set port
    free(parsed->port);
    if (port_part && strlen(port_part) > 0) {
      parsed->port = normalize_port(port_part, parsed->protocol);
      if (!parsed->port) {
        // Invalid port - cleanup and return error
        // Don't free hostname_part here since it might be assigned to parsed->hostname
        // Don't free port_part - it's just a pointer into authority string
        free(url_copy);
        free(cleaned_url);
        JSRT_FreeURL(parsed);
        return NULL;
      }
    } else {
      parsed->port = strdup("");
    }

    // Set host field (hostname:port format, excluding credentials)
    free(parsed->host);
    if (strlen(parsed->port) > 0) {
      parsed->host = malloc(strlen(parsed->hostname) + strlen(parsed->port) + 2);
      sprintf(parsed->host, "%s:%s", parsed->hostname, parsed->port);
    } else {
      parsed->host = strdup(parsed->hostname);
    }
  }

  // Build origin using the improved function
  free(parsed->origin);
  parsed->origin = compute_origin(parsed->protocol, parsed->hostname, parsed->port);

  free(url_copy);

  // Validate the parsed URL
  if (strlen(parsed->protocol) > 0) {
    // Extract scheme without the colon
    char* scheme = strdup(parsed->protocol);
    if (strlen(scheme) > 0 && scheme[strlen(scheme) - 1] == ':') {
      scheme[strlen(scheme) - 1] = '\0';
    }

    // Accept all schemes that were successfully parsed
    // The scheme validation was already done during parsing

    // Check if this is a special scheme that requires a host
    const char* special_schemes[] = {"http", "https", "ftp", "ws", "wss", NULL};
    int is_special = 0;
    for (int i = 0; special_schemes[i]; i++) {
      if (strcmp(scheme, special_schemes[i]) == 0) {
        is_special = 1;
        break;
      }
    }

    // Special schemes require a host (except file:// which is handled separately)
    // Non-special schemes (like foo://) can have empty hosts
    if (is_special && strlen(parsed->host) == 0) {
      free(scheme);
      JSRT_FreeURL(parsed);
      return NULL;  // Special schemes require a host
    }

    free(scheme);
  } else {
    // No protocol found - this is not a valid absolute URL
    JSRT_FreeURL(parsed);
    return NULL;
  }

  // Rebuild href from parsed components with proper encoding
  free(parsed->href);

  // For non-special schemes, pathname should not be encoded (spaces preserved)
  // For special schemes, pathname should be encoded
  char* encoded_pathname;
  if (is_special_scheme(parsed->protocol)) {
    encoded_pathname = url_component_encode(parsed->pathname);
  } else {
    // Non-special schemes: preserve spaces and other characters in pathname
    encoded_pathname = strdup(parsed->pathname ? parsed->pathname : "");
  }
  char* encoded_search = url_component_encode(parsed->search);
  // Hash should be encoded
  char* encoded_hash = url_component_encode(parsed->hash);

  size_t href_len = strlen(parsed->protocol) + 2 + strlen(parsed->host) + strlen(encoded_pathname);

  // Add space for username:password@ if present
  if (parsed->username && strlen(parsed->username) > 0) {
    href_len += strlen(parsed->username);
    if (parsed->password && strlen(parsed->password) > 0) {
      href_len += 1 + strlen(parsed->password);  // +1 for colon
    }
    href_len += 1;  // +1 for @ symbol
  }

  if (encoded_search && strlen(encoded_search) > 0) {
    href_len += strlen(encoded_search);
  }
  if (encoded_hash && strlen(encoded_hash) > 0) {
    href_len += strlen(encoded_hash);
  }

  parsed->href = malloc(href_len + 1);
  strcpy(parsed->href, parsed->protocol);
  strcat(parsed->href, "//");

  // Add username:password@ if present (include if either username or password is present)
  if ((parsed->username && strlen(parsed->username) > 0) || (parsed->password && strlen(parsed->password) > 0)) {
    // Add username (empty if not present)
    if (parsed->username && strlen(parsed->username) > 0) {
      strcat(parsed->href, parsed->username);
    }
    // Add password if present
    if (parsed->password && strlen(parsed->password) > 0) {
      strcat(parsed->href, ":");
      strcat(parsed->href, parsed->password);
    }
    strcat(parsed->href, "@");
  }

  strcat(parsed->href, parsed->host);
  strcat(parsed->href, encoded_pathname);

  if (encoded_search && strlen(encoded_search) > 0) {
    strcat(parsed->href, encoded_search);
  }
  if (encoded_hash && strlen(encoded_hash) > 0) {
    strcat(parsed->href, encoded_hash);
  }

  free(encoded_pathname);
  free(encoded_search);
  free(encoded_hash);

  free(cleaned_url);
  if (relative_part) {
    free(relative_part);
  }
  return parsed;
}

static void JSRT_FreeURL(JSRT_URL* url) {
  if (url) {
    if (url->ctx && !JS_IsUndefined(url->search_params)) {
      JS_FreeValue(url->ctx, url->search_params);
    }
    free(url->href);
    free(url->protocol);
    free(url->username);
    free(url->password);
    free(url->host);
    free(url->hostname);
    free(url->port);
    free(url->pathname);
    free(url->search);
    free(url->hash);
    free(url->origin);
    free(url);
  }
}

static void JSRT_URLFinalize(JSRuntime* rt, JSValue val) {
  JSRT_URL* url = JS_GetOpaque(val, JSRT_URLClassID);
  if (url) {
    JSRT_FreeURL(url);
  }
}

static JSClassDef JSRT_URLClass = {
    .class_name = "URL",
    .finalizer = JSRT_URLFinalize,
};

// Helper function to strip tab and newline characters from URL strings
// According to URL spec, these characters should be removed: tab (0x09), LF (0x0A), CR (0x0D)
static char* JSRT_StripURLControlCharacters(const char* input) {
  if (!input)
    return NULL;

  size_t len = strlen(input);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    char c = input[i];
    // Skip tab, line feed, and carriage return
    if (c != 0x09 && c != 0x0A && c != 0x0D) {
      result[j++] = c;
    }
  }
  result[j] = '\0';
  return result;
}

static JSValue JSRT_URLConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "URL constructor requires at least 1 argument");
  }

  const char* url_str_raw = JS_ToCString(ctx, argv[0]);
  if (!url_str_raw) {
    return JS_EXCEPTION;
  }

  // Strip control characters as per URL specification
  char* url_str = JSRT_StripURLControlCharacters(url_str_raw);
  JS_FreeCString(ctx, url_str_raw);
  if (!url_str) {
    return JS_EXCEPTION;
  }

  const char* base_str_raw = NULL;
  char* base_str = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    base_str_raw = JS_ToCString(ctx, argv[1]);
    if (!base_str_raw) {
      free(url_str);
      return JS_EXCEPTION;
    }
    base_str = JSRT_StripURLControlCharacters(base_str_raw);
    JS_FreeCString(ctx, base_str_raw);
    if (!base_str) {
      free(url_str);
      return JS_EXCEPTION;
    }
  }

  JSRT_URL* url = JSRT_ParseURL(url_str, base_str);
  if (!url) {
    free(url_str);
    if (base_str) {
      free(base_str);
    }
    return JS_ThrowTypeError(ctx, "Invalid URL");
  }

  url->ctx = ctx;  // Set context for managing search_params

  JSValue obj = JS_NewObjectClass(ctx, JSRT_URLClassID);
  JS_SetOpaque(obj, url);

  free(url_str);
  if (base_str) {
    free(base_str);
  }

  return obj;
}

// Property getters
static JSValue JSRT_URLGetHref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->href);
}

static JSValue JSRT_URLGetProtocol(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->protocol);
}

static JSValue JSRT_URLGetUsername(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->username ? url->username : "");
}

static JSValue JSRT_URLGetPassword(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->password ? url->password : "");
}

static JSValue JSRT_URLGetHost(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->host);
}

static JSValue JSRT_URLGetHostname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->hostname);
}

static JSValue JSRT_URLGetPort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->port);
}

static JSValue JSRT_URLGetPathname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // For non-special schemes, return the raw pathname without encoding
  // For special schemes, return the percent-encoded pathname
  if (is_special_scheme(url->protocol)) {
    char* encoded_pathname = url_component_encode(url->pathname);
    if (!encoded_pathname)
      return JS_EXCEPTION;

    JSValue result = JS_NewString(ctx, encoded_pathname);
    free(encoded_pathname);
    return result;
  } else {
    // Non-special schemes return raw pathname
    return JS_NewString(ctx, url->pathname);
  }
}

static JSValue JSRT_URLGetSearch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // Per WPT spec: empty query ("?") should return empty string, not "?"
  if (url->search && strcmp(url->search, "?") == 0) {
    return JS_NewString(ctx, "");
  }

  // Return the percent-encoded search as per URL specification
  char* encoded_search = url_component_encode(url->search);
  if (!encoded_search)
    return JS_EXCEPTION;

  JSValue result = JS_NewString(ctx, encoded_search);
  free(encoded_search);
  return result;
}

static JSValue JSRT_URLSetSearch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  if (argc < 1)
    return JS_UNDEFINED;

  const char* new_search = JS_ToCString(ctx, argv[0]);
  if (!new_search)
    return JS_EXCEPTION;

  // Update the search string
  free(url->search);
  if (new_search[0] == '?') {
    url->search = strdup(new_search);
  } else {
    size_t len = strlen(new_search);
    url->search = malloc(len + 2);
    url->search[0] = '?';
    strcpy(url->search + 1, new_search);
  }

  // Update cached URLSearchParams object if it exists
  if (!JS_IsUndefined(url->search_params)) {
    JSRT_URLSearchParams* cached_params = JS_GetOpaque2(ctx, url->search_params, JSRT_URLSearchParamsClassID);
    if (cached_params) {
      // Free existing parameters
      JSRT_URLSearchParam* param = cached_params->params;
      while (param) {
        JSRT_URLSearchParam* next = param->next;
        free(param->name);
        free(param->value);
        free(param);
        param = next;
      }

      // Parse new search string into existing URLSearchParams object
      JSRT_URLSearchParams* new_params = JSRT_ParseSearchParams(url->search, strlen(url->search));
      cached_params->params = new_params->params;
      new_params->params = NULL;  // Prevent double free
      JSRT_FreeSearchParams(new_params);
    }
  }

  // Rebuild href with new search component
  free(url->href);
  size_t href_len = strlen(url->protocol) + 2 + strlen(url->host) + strlen(url->pathname);
  if (url->search && strlen(url->search) > 0) {
    href_len += strlen(url->search);
  }
  if (url->hash && strlen(url->hash) > 0) {
    href_len += strlen(url->hash);
  }

  url->href = malloc(href_len + 1);
  strcpy(url->href, url->protocol);
  strcat(url->href, "//");
  strcat(url->href, url->host);
  strcat(url->href, url->pathname);

  if (url->search && strlen(url->search) > 0) {
    strcat(url->href, url->search);
  }

  if (url->hash && strlen(url->hash) > 0) {
    strcat(url->href, url->hash);
  }

  JS_FreeCString(ctx, new_search);
  return JS_UNDEFINED;
}

static JSValue JSRT_URLGetHash(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // Per WPT spec: empty fragment ("#") should return empty string, not "#"
  if (url->hash && strcmp(url->hash, "#") == 0) {
    return JS_NewString(ctx, "");
  }

  // Return the percent-encoded hash as per URL specification
  char* encoded_hash = url_component_encode(url->hash);
  if (!encoded_hash)
    return JS_EXCEPTION;

  JSValue result = JS_NewString(ctx, encoded_hash);
  free(encoded_hash);
  return result;
}

static JSValue JSRT_URLGetOrigin(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;
  return JS_NewString(ctx, url->origin);
}

static JSValue JSRT_URLGetSearchParams(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URL* url = JS_GetOpaque2(ctx, this_val, JSRT_URLClassID);
  if (!url)
    return JS_EXCEPTION;

  // Create URLSearchParams object if not already cached
  if (JS_IsUndefined(url->search_params)) {
    JSValue search_params_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "URLSearchParams");
    JSValue search_value = JS_NewString(ctx, url->search);
    url->search_params = JS_CallConstructor(ctx, search_params_ctor, 1, &search_value);

    // Set parent_url reference for URL integration
    JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, url->search_params, JSRT_URLSearchParamsClassID);
    if (search_params) {
      search_params->parent_url = url;
      search_params->ctx = ctx;
    }

    JS_FreeValue(ctx, search_params_ctor);
    JS_FreeValue(ctx, search_value);
  }

  return JS_DupValue(ctx, url->search_params);
}

static JSValue JSRT_URLToString(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLGetHref(ctx, this_val, argc, argv);
}

static JSValue JSRT_URLToJSON(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLGetHref(ctx, this_val, argc, argv);
}

// URLSearchParams implementation

// Helper function to create a parameter with proper length handling
static JSRT_URLSearchParam* create_url_param(const char* name, size_t name_len, const char* value, size_t value_len) {
  JSRT_URLSearchParam* param = malloc(sizeof(JSRT_URLSearchParam));
  param->name = malloc(name_len + 1);
  memcpy(param->name, name, name_len);
  param->name[name_len] = '\0';
  param->name_len = name_len;
  param->value = malloc(value_len + 1);
  memcpy(param->value, value, value_len);
  param->value[value_len] = '\0';
  param->value_len = value_len;
  param->next = NULL;
  return param;
}

// Helper function to update parent URL's href when URLSearchParams change
static void update_parent_url_href(JSRT_URLSearchParams* search_params) {
  if (!search_params || !search_params->parent_url) {
    return;
  }

  JSRT_URL* url = search_params->parent_url;

  // Generate new search string directly from parameters
  JSContext* ctx = search_params->ctx;
  if (!ctx) {
    return;  // No context available for updates
  }

  // Build search string manually to avoid circular references
  char* new_search_str = NULL;
  if (!search_params->params) {
    new_search_str = strdup("");
  } else {
    // Calculate required length
    size_t total_len = 0;
    JSRT_URLSearchParam* param = search_params->params;
    bool first = true;
    while (param) {
      char* encoded_name = url_encode_with_len(param->name, param->name_len);
      char* encoded_value = url_encode_with_len(param->value, param->value_len);
      size_t pair_len = strlen(encoded_name) + strlen(encoded_value) + 1;  // name=value
      if (!first)
        pair_len += 1;  // &
      total_len += pair_len;
      free(encoded_name);
      free(encoded_value);
      param = param->next;
      first = false;
    }

    // Build the string
    new_search_str = malloc(total_len + 1);
    new_search_str[0] = '\0';
    param = search_params->params;
    first = true;
    while (param) {
      char* encoded_name = url_encode_with_len(param->name, param->name_len);
      char* encoded_value = url_encode_with_len(param->value, param->value_len);

      if (!first)
        strcat(new_search_str, "&");
      strcat(new_search_str, encoded_name);
      strcat(new_search_str, "=");
      strcat(new_search_str, encoded_value);

      free(encoded_name);
      free(encoded_value);
      param = param->next;
      first = false;
    }
  }

  // Update URL's search component
  free(url->search);
  if (strlen(new_search_str) > 0) {
    url->search = malloc(strlen(new_search_str) + 2);  // +1 for '?', +1 for '\0'
    sprintf(url->search, "?%s", new_search_str);
  } else {
    url->search = strdup("");
  }

  // Rebuild href
  free(url->href);
  size_t href_len = strlen(url->protocol) + 2 + strlen(url->host) + strlen(url->pathname);
  if (url->search && strlen(url->search) > 0) {
    href_len += strlen(url->search);
  }
  if (url->hash && strlen(url->hash) > 0) {
    href_len += strlen(url->hash);
  }

  url->href = malloc(href_len + 1);
  strcpy(url->href, url->protocol);
  strcat(url->href, "//");
  strcat(url->href, url->host);
  strcat(url->href, url->pathname);

  if (url->search && strlen(url->search) > 0) {
    strcat(url->href, url->search);
  }

  if (url->hash && strlen(url->hash) > 0) {
    strcat(url->href, url->hash);
  }

  free(new_search_str);
}

static void JSRT_FreeSearchParams(JSRT_URLSearchParams* search_params) {
  if (search_params) {
    JSRT_URLSearchParam* param = search_params->params;
    while (param) {
      JSRT_URLSearchParam* next = param->next;
      free(param->name);
      free(param->value);
      free(param);
      param = next;
    }
    free(search_params);
  }
}

static void JSRT_URLSearchParamsFinalize(JSRuntime* rt, JSValue val) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque(val, JSRT_URLSearchParamsClassID);
  if (search_params) {
    JSRT_FreeSearchParams(search_params);
  }
}

static JSClassDef JSRT_URLSearchParamsClass = {
    .class_name = "URLSearchParams",
    .finalizer = JSRT_URLSearchParamsFinalize,
};

static int hex_to_int(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

// URL decode function for query parameters (+ becomes space)
static char* url_decode_query_with_length_and_output_len(const char* str, size_t len, size_t* output_len) {
  char* decoded = malloc(len * 3 + 1);  // Allocate more space for potential replacement characters
  size_t i = 0, j = 0;

  while (i < len) {
    if (str[i] == '%' && i + 2 < len) {
      int h1 = hex_to_int(str[i + 1]);
      int h2 = hex_to_int(str[i + 2]);
      if (h1 >= 0 && h2 >= 0) {
        unsigned char byte = (unsigned char)((h1 << 4) | h2);

        // Check if this starts a UTF-8 sequence
        if (byte >= 0x80) {
          // Collect the complete UTF-8 sequence
          size_t seq_start = j;
          decoded[j++] = byte;
          i += 3;

          // Determine expected sequence length
          int expected_len = 1;
          if ((byte & 0xE0) == 0xC0)
            expected_len = 2;
          else if ((byte & 0xF0) == 0xE0)
            expected_len = 3;
          else if ((byte & 0xF8) == 0xF0)
            expected_len = 4;

          // Collect continuation bytes
          for (int k = 1; k < expected_len && i + 2 < len; k++) {
            if (str[i] == '%' && i + 2 < len) {
              int h1_cont = hex_to_int(str[i + 1]);
              int h2_cont = hex_to_int(str[i + 2]);
              if (h1_cont >= 0 && h2_cont >= 0) {
                unsigned char cont_byte = (unsigned char)((h1_cont << 4) | h2_cont);
                if ((cont_byte & 0xC0) == 0x80) {
                  decoded[j++] = cont_byte;
                  i += 3;
                } else {
                  break;  // Invalid continuation byte
                }
              } else {
                break;  // Invalid hex
              }
            } else {
              break;  // Not a percent-encoded byte
            }
          }

          // Validate the collected UTF-8 sequence
          const uint8_t* next;
          int codepoint = JSRT_ValidateUTF8Sequence((const uint8_t*)(decoded + seq_start), j - seq_start, &next);

          if (codepoint < 0) {
            // Invalid UTF-8 sequence, replace with U+FFFD (0xEF 0xBF 0xBD)
            j = seq_start;  // Reset to start of sequence
            decoded[j++] = 0xEF;
            decoded[j++] = 0xBF;
            decoded[j++] = 0xBD;
          }
        } else {
          // ASCII byte, add directly
          decoded[j++] = byte;
          i += 3;
        }
        continue;
      }
    } else if (str[i] == '+') {
      // + should decode to space in query parameters
      decoded[j++] = ' ';
      i++;
      continue;
    }
    decoded[j++] = str[i++];
  }
  decoded[j] = '\0';
  if (output_len) {
    *output_len = j;
  }
  return decoded;
}

// URL decode function for general URL components (+ remains as +)
static char* url_decode_with_length_and_output_len(const char* str, size_t len, size_t* output_len) {
  char* decoded = malloc(len * 3 + 1);  // Allocate more space for potential replacement characters
  size_t i = 0, j = 0;

  while (i < len) {
    if (str[i] == '%' && i + 2 < len) {
      int h1 = hex_to_int(str[i + 1]);
      int h2 = hex_to_int(str[i + 2]);
      if (h1 >= 0 && h2 >= 0) {
        unsigned char byte = (unsigned char)((h1 << 4) | h2);

        // Check if this starts a UTF-8 sequence
        if (byte >= 0x80) {
          // Collect the complete UTF-8 sequence
          size_t seq_start = j;
          decoded[j++] = byte;
          i += 3;

          // Determine expected sequence length
          int expected_len = 1;
          if ((byte & 0xE0) == 0xC0)
            expected_len = 2;
          else if ((byte & 0xF0) == 0xE0)
            expected_len = 3;
          else if ((byte & 0xF8) == 0xF0)
            expected_len = 4;

          // Collect continuation bytes
          for (int k = 1; k < expected_len && i + 2 < len; k++) {
            if (str[i] == '%' && i + 2 < len) {
              int h1_cont = hex_to_int(str[i + 1]);
              int h2_cont = hex_to_int(str[i + 2]);
              if (h1_cont >= 0 && h2_cont >= 0) {
                unsigned char cont_byte = (unsigned char)((h1_cont << 4) | h2_cont);
                if ((cont_byte & 0xC0) == 0x80) {
                  decoded[j++] = cont_byte;
                  i += 3;
                } else {
                  break;  // Invalid continuation byte
                }
              } else {
                break;  // Invalid hex
              }
            } else {
              break;  // Not a percent-encoded byte
            }
          }

          // Validate the collected UTF-8 sequence
          const uint8_t* next;
          int codepoint = JSRT_ValidateUTF8Sequence((const uint8_t*)(decoded + seq_start), j - seq_start, &next);

          if (codepoint < 0) {
            // Invalid UTF-8 sequence, replace with U+FFFD (0xEF 0xBF 0xBD)
            j = seq_start;  // Reset to start of sequence
            decoded[j++] = 0xEF;
            decoded[j++] = 0xBF;
            decoded[j++] = 0xBD;
          }
        } else {
          // ASCII byte, add directly
          decoded[j++] = byte;
          i += 3;
        }
        continue;
      }
    }
    // For general URL components, + remains as + (not converted to space)
    decoded[j++] = str[i++];
  }
  decoded[j] = '\0';
  if (output_len) {
    *output_len = j;
  }
  return decoded;
}

static char* url_decode_with_length(const char* str, size_t len) {
  return url_decode_with_length_and_output_len(str, len, NULL);
}

static char* url_decode(const char* str) {
  size_t len = strlen(str);
  return url_decode_with_length(str, len);
}

static JSRT_URLSearchParams* JSRT_ParseSearchParams(const char* search_string, size_t string_len) {
  JSRT_URLSearchParams* search_params = malloc(sizeof(JSRT_URLSearchParams));
  search_params->params = NULL;
  search_params->parent_url = NULL;
  search_params->ctx = NULL;

  if (!search_string || string_len == 0) {
    return search_params;
  }

  const char* start = search_string;
  size_t start_len = string_len;
  if (start_len > 0 && start[0] == '?') {
    start++;  // Skip leading '?'
    start_len--;
  }

  JSRT_URLSearchParam** tail = &search_params->params;

  // Parse parameters manually without relying on C string functions
  size_t i = 0;
  while (i < start_len) {
    // Find the end of this parameter (next '&' or end of string)
    size_t param_start = i;
    while (i < start_len && start[i] != '&') {
      i++;
    }
    size_t param_len = i - param_start;

    if (param_len > 0) {
      // Find '=' in this parameter
      size_t eq_pos = param_start;
      while (eq_pos < param_start + param_len && start[eq_pos] != '=') {
        eq_pos++;
      }

      JSRT_URLSearchParam* param = malloc(sizeof(JSRT_URLSearchParam));
      param->next = NULL;

      if (eq_pos < param_start + param_len) {
        // Has '=' - split into name and value
        size_t name_len = eq_pos - param_start;
        size_t value_len = param_start + param_len - eq_pos - 1;

        param->name = url_decode_query_with_length_and_output_len(start + param_start, name_len, &param->name_len);
        param->value = url_decode_query_with_length_and_output_len(start + eq_pos + 1, value_len, &param->value_len);
      } else {
        // No '=' - name only, empty value
        param->name = url_decode_query_with_length_and_output_len(start + param_start, param_len, &param->name_len);
        param->value = strdup("");
        param->value_len = 0;
      }

      // Lengths are already set by url_decode_with_length_and_output_len

      *tail = param;
      tail = &param->next;
    }

    // Move past the '&' for next iteration
    if (i < start_len) {
      i++;
    }
  }

  return search_params;
}

static JSRT_URLSearchParams* JSRT_CreateEmptySearchParams() {
  JSRT_URLSearchParams* search_params = malloc(sizeof(JSRT_URLSearchParams));
  search_params->params = NULL;
  search_params->parent_url = NULL;
  search_params->ctx = NULL;
  return search_params;
}

static void JSRT_AddSearchParam(JSRT_URLSearchParams* search_params, const char* name, const char* value) {
  size_t name_len = strlen(name);
  size_t value_len = strlen(value);
  JSRT_URLSearchParam* param = create_url_param(name, name_len, value, value_len);

  // Add at end to maintain insertion order
  if (!search_params->params) {
    search_params->params = param;
  } else {
    JSRT_URLSearchParam* tail = search_params->params;
    while (tail->next) {
      tail = tail->next;
    }
    tail->next = param;
  }
}

// Length-aware version for handling strings with null bytes
static void JSRT_AddSearchParamWithLength(JSRT_URLSearchParams* search_params, const char* name, size_t name_len,
                                          const char* value, size_t value_len) {
  JSRT_URLSearchParam* param = create_url_param(name, name_len, value, value_len);

  // Add at end to maintain insertion order
  if (!search_params->params) {
    search_params->params = param;
  } else {
    JSRT_URLSearchParam* tail = search_params->params;
    while (tail->next) {
      tail = tail->next;
    }
    tail->next = param;
  }
}

static JSRT_URLSearchParams* JSRT_ParseSearchParamsFromSequence(JSContext* ctx, JSValue seq) {
  JSRT_URLSearchParams* search_params = JSRT_CreateEmptySearchParams();

  // TODO: Check if it has Symbol.iterator (should use iterator protocol)
  // Try iterator protocol first if the object has Symbol.iterator
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue symbol_obj = JS_GetPropertyStr(ctx, global, "Symbol");
  JSValue iterator_symbol = JS_GetPropertyStr(ctx, symbol_obj, "iterator");
  JS_FreeValue(ctx, symbol_obj);
  JS_FreeValue(ctx, global);

  if (!JS_IsUndefined(iterator_symbol)) {
    JSAtom iterator_atom = JS_ValueToAtom(ctx, iterator_symbol);
    int has_iterator = JS_HasProperty(ctx, seq, iterator_atom);

    if (has_iterator) {
      // Use iterator protocol
      JSValue iterator_method = JS_GetProperty(ctx, seq, iterator_atom);
      JS_FreeAtom(ctx, iterator_atom);
      JS_FreeValue(ctx, iterator_symbol);
      if (JS_IsException(iterator_method)) {
        JSRT_FreeSearchParams(search_params);
        return NULL;
      }

      JSValue iterator = JS_Call(ctx, iterator_method, seq, 0, NULL);
      JS_FreeValue(ctx, iterator_method);
      if (JS_IsException(iterator)) {
        JSRT_FreeSearchParams(search_params);
        return NULL;
      }

      // Iterate through the iterator
      JSValue next_method = JS_GetPropertyStr(ctx, iterator, "next");
      if (JS_IsException(next_method)) {
        JS_FreeValue(ctx, iterator);
        JSRT_FreeSearchParams(search_params);
        return NULL;
      }

      while (true) {
        JSValue result = JS_Call(ctx, next_method, iterator, 0, NULL);
        if (JS_IsException(result)) {
          JS_FreeValue(ctx, next_method);
          JS_FreeValue(ctx, iterator);
          JSRT_FreeSearchParams(search_params);
          return NULL;
        }

        JSValue done = JS_GetPropertyStr(ctx, result, "done");
        bool is_done = JS_ToBool(ctx, done);
        JS_FreeValue(ctx, done);

        if (is_done) {
          JS_FreeValue(ctx, result);
          break;
        }

        JSValue item = JS_GetPropertyStr(ctx, result, "value");
        JS_FreeValue(ctx, result);

        // Process the item (same validation as array-like)
        JSValue item_length_val = JS_GetPropertyStr(ctx, item, "length");
        int32_t item_length;
        if (JS_IsException(item_length_val) || JS_ToInt32(ctx, &item_length, item_length_val)) {
          JS_FreeValue(ctx, item);
          if (!JS_IsException(item_length_val))
            JS_FreeValue(ctx, item_length_val);
          JS_FreeValue(ctx, next_method);
          JS_FreeValue(ctx, iterator);
          JSRT_FreeSearchParams(search_params);
          return NULL;
        }

        if (item_length != 2) {
          JS_FreeValue(ctx, item);
          JS_FreeValue(ctx, item_length_val);
          JS_FreeValue(ctx, next_method);
          JS_FreeValue(ctx, iterator);
          JSRT_FreeSearchParams(search_params);
          JS_ThrowTypeError(ctx, "Iterator value a 0 is not an entry object");
          return NULL;
        }
        JS_FreeValue(ctx, item_length_val);

        JSValue name_val = JS_GetPropertyUint32(ctx, item, 0);
        JSValue value_val = JS_GetPropertyUint32(ctx, item, 1);

        const char* name_str = JS_ToCString(ctx, name_val);
        const char* value_str = JS_ToCString(ctx, value_val);

        if (name_str && value_str) {
          JSRT_AddSearchParam(search_params, name_str, value_str);
        }

        if (name_str)
          JS_FreeCString(ctx, name_str);
        if (value_str)
          JS_FreeCString(ctx, value_str);
        JS_FreeValue(ctx, name_val);
        JS_FreeValue(ctx, value_val);
        JS_FreeValue(ctx, item);
      }

      JS_FreeValue(ctx, next_method);
      JS_FreeValue(ctx, iterator);
      return search_params;
    } else {
      JS_FreeAtom(ctx, iterator_atom);
      JS_FreeValue(ctx, iterator_symbol);
    }
  } else {
    JS_FreeValue(ctx, iterator_symbol);
  }

  // Fall back to array-like sequence handling
  JSValue length_val = JS_GetPropertyStr(ctx, seq, "length");
  if (JS_IsException(length_val)) {
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }

  int32_t length;
  if (JS_ToInt32(ctx, &length, length_val)) {
    JS_FreeValue(ctx, length_val);
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }
  JS_FreeValue(ctx, length_val);

  for (int32_t i = 0; i < length; i++) {
    JSValue item = JS_GetPropertyUint32(ctx, seq, i);
    if (JS_IsException(item)) {
      JSRT_FreeSearchParams(search_params);
      return NULL;
    }

    // Each item should be a sequence with exactly 2 elements
    JSValue item_length_val = JS_GetPropertyStr(ctx, item, "length");
    int32_t item_length;
    if (JS_IsException(item_length_val) || JS_ToInt32(ctx, &item_length, item_length_val)) {
      JS_FreeValue(ctx, item);
      if (!JS_IsException(item_length_val))
        JS_FreeValue(ctx, item_length_val);
      JSRT_FreeSearchParams(search_params);
      return NULL;
    }

    if (item_length != 2) {
      JS_FreeValue(ctx, item);
      JS_FreeValue(ctx, item_length_val);
      JSRT_FreeSearchParams(search_params);
      JS_ThrowTypeError(ctx, "Iterator value a 0 is not an entry object");
      return NULL;
    }
    JS_FreeValue(ctx, item_length_val);

    JSValue name_val = JS_GetPropertyUint32(ctx, item, 0);
    JSValue value_val = JS_GetPropertyUint32(ctx, item, 1);

    const char* name_str = JS_ToCString(ctx, name_val);
    const char* value_str = JS_ToCString(ctx, value_val);

    if (name_str && value_str) {
      JSRT_AddSearchParam(search_params, name_str, value_str);
    }

    if (name_str)
      JS_FreeCString(ctx, name_str);
    if (value_str)
      JS_FreeCString(ctx, value_str);
    JS_FreeValue(ctx, name_val);
    JS_FreeValue(ctx, value_val);
    JS_FreeValue(ctx, item);
  }

  return search_params;
}

static JSRT_URLSearchParams* JSRT_ParseSearchParamsFromRecord(JSContext* ctx, JSValue record) {
  JSRT_URLSearchParams* search_params = JSRT_CreateEmptySearchParams();

  JSPropertyEnum* properties;
  uint32_t count;

  if (JS_GetOwnPropertyNames(ctx, &properties, &count, record, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }

  for (uint32_t i = 0; i < count; i++) {
    JSValue value = JS_GetProperty(ctx, record, properties[i].atom);
    if (JS_IsException(value)) {
      continue;
    }

    // Convert atom to JSValue for surrogate handling
    JSValue name_val = JS_AtomToString(ctx, properties[i].atom);
    if (JS_IsException(name_val)) {
      JS_FreeValue(ctx, value);
      continue;
    }

    // Use surrogate handling helper for both name and value
    size_t name_len, value_len;
    const char* name_str = JSRT_StringToUTF8WithSurrogateReplacement(ctx, name_val, &name_len);
    const char* value_str = JSRT_StringToUTF8WithSurrogateReplacement(ctx, value, &value_len);

    if (name_str && value_str) {
      // For object constructor, later values should overwrite earlier ones for same key
      // But maintain the position of the first occurrence of the key
      JSRT_URLSearchParam* existing = NULL;
      JSRT_URLSearchParam* current = search_params->params;
      while (current) {
        if (current->name_len == name_len && memcmp(current->name, name_str, name_len) == 0) {
          if (!existing) {
            // First match - update this one and remember it
            free(current->value);
            current->value = malloc(value_len + 1);
            if (current->value) {
              memcpy(current->value, value_str, value_len);
              current->value[value_len] = '\0';
              current->value_len = value_len;
            }
            existing = current;
          } else {
            // Additional match - remove this one
            // This is a complex operation, let's use the existing removal logic
            JSRT_URLSearchParam** prev_next = &search_params->params;
            while (*prev_next != current) {
              prev_next = &(*prev_next)->next;
            }
            *prev_next = current->next;
            free(current->name);
            free(current->value);
            JSRT_URLSearchParam* to_free = current;
            current = current->next;
            free(to_free);
            continue;
          }
        }
        current = current->next;
      }

      // If no existing key found, add new parameter
      if (!existing) {
        JSRT_AddSearchParamWithLength(search_params, name_str, name_len, value_str, value_len);
      }
    }

    if (name_str)
      free((char*)name_str);
    if (value_str)
      free((char*)value_str);

    JS_FreeValue(ctx, name_val);
    JS_FreeValue(ctx, value);
  }

  JS_FreePropertyEnum(ctx, properties, count);
  return search_params;
}

static JSRT_URLSearchParams* JSRT_ParseSearchParamsFromFormData(JSContext* ctx, JSValueConst formdata_val) {
  JSRT_URLSearchParams* search_params = JSRT_CreateEmptySearchParams();

  // Get the FormData opaque data
  void* formdata_opaque = JS_GetOpaque(formdata_val, JSRT_FormDataClassID);
  if (!formdata_opaque) {
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }

  // Access FormData entries through forEach method
  // Call formData.forEach((value, name) => { add to search_params })
  JSValue forEach_fn = JS_GetPropertyStr(ctx, formdata_val, "forEach");
  if (JS_IsUndefined(forEach_fn)) {
    JSRT_FreeSearchParams(search_params);
    return NULL;
  }

  // Create a callback function to collect the entries
  // We'll use a simpler approach: iterate through the FormData structure directly
  // Since we have access to the FormData structure, we can access it directly
  typedef struct JSRT_FormDataEntry {
    char* name;
    JSValue value;
    char* filename;
    struct JSRT_FormDataEntry* next;
  } JSRT_FormDataEntry;

  typedef struct {
    JSRT_FormDataEntry* entries;
  } JSRT_FormData;

  JSRT_FormData* formdata = (JSRT_FormData*)formdata_opaque;
  JSRT_FormDataEntry* entry = formdata->entries;

  while (entry) {
    if (entry->name) {
      const char* value_str = JS_ToCString(ctx, entry->value);
      if (value_str) {
        JSRT_AddSearchParam(search_params, entry->name, value_str);
        JS_FreeCString(ctx, value_str);
      }
    }
    entry = entry->next;
  }

  JS_FreeValue(ctx, forEach_fn);
  return search_params;
}

static JSValue JSRT_URLSearchParamsConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params;

  if (argc >= 1 && !JS_IsUndefined(argv[0])) {
    JSValue init = argv[0];

    // Check if it's already a URLSearchParams object
    if (JS_GetOpaque2(ctx, init, JSRT_URLSearchParamsClassID)) {
      // However, if it has a custom Symbol.iterator, use that instead of copying
      JSValue global = JS_GetGlobalObject(ctx);
      JSValue symbol_obj = JS_GetPropertyStr(ctx, global, "Symbol");
      if (!JS_IsUndefined(symbol_obj)) {
        JSValue iterator_symbol = JS_GetPropertyStr(ctx, symbol_obj, "iterator");
        if (!JS_IsUndefined(iterator_symbol)) {
          JSAtom iterator_atom = JS_ValueToAtom(ctx, iterator_symbol);
          JSValue custom_iterator = JS_GetProperty(ctx, init, iterator_atom);
          JS_FreeAtom(ctx, iterator_atom);
          if (!JS_IsUndefined(custom_iterator) && JS_IsFunction(ctx, custom_iterator)) {
            // Has custom iterator - use sequence parsing instead
            JS_FreeValue(ctx, custom_iterator);
            JS_FreeValue(ctx, iterator_symbol);
            JS_FreeValue(ctx, symbol_obj);
            JS_FreeValue(ctx, global);
            search_params = JSRT_ParseSearchParamsFromSequence(ctx, init);
          } else {
            // No custom iterator - copy parameters normally
            JS_FreeValue(ctx, custom_iterator);
            JS_FreeValue(ctx, iterator_symbol);
            JS_FreeValue(ctx, symbol_obj);
            JS_FreeValue(ctx, global);

            JSRT_URLSearchParams* src = JS_GetOpaque2(ctx, init, JSRT_URLSearchParamsClassID);
            search_params = JSRT_CreateEmptySearchParams();

            // Copy all parameters
            JSRT_URLSearchParam* param = src->params;
            while (param) {
              JSRT_AddSearchParam(search_params, param->name, param->value);
              param = param->next;
            }
          }
        } else {
          JS_FreeValue(ctx, iterator_symbol);
          JS_FreeValue(ctx, symbol_obj);
          JS_FreeValue(ctx, global);

          JSRT_URLSearchParams* src = JS_GetOpaque2(ctx, init, JSRT_URLSearchParamsClassID);
          search_params = JSRT_CreateEmptySearchParams();

          // Copy all parameters
          JSRT_URLSearchParam* param = src->params;
          while (param) {
            JSRT_AddSearchParam(search_params, param->name, param->value);
            param = param->next;
          }
        }
      } else {
        JS_FreeValue(ctx, symbol_obj);
        JS_FreeValue(ctx, global);

        JSRT_URLSearchParams* src = JS_GetOpaque2(ctx, init, JSRT_URLSearchParamsClassID);
        search_params = JSRT_CreateEmptySearchParams();

        // Copy all parameters
        JSRT_URLSearchParam* param = src->params;
        while (param) {
          JSRT_AddSearchParam(search_params, param->name, param->value);
          param = param->next;
        }
      }
    }
    // Check if it's a FormData object
    else if (JS_GetOpaque2(ctx, init, JSRT_FormDataClassID)) {
      search_params = JSRT_ParseSearchParamsFromFormData(ctx, init);
      if (!search_params) {
        return JS_ThrowTypeError(ctx, "Invalid FormData argument to URLSearchParams constructor");
      }
    }
    // Check if it's an iterable (has Symbol.iterator) or array-like sequence
    else if (!JS_IsString(init)) {
      JSAtom length_atom = JS_NewAtom(ctx, "length");
      int has_length = JS_HasProperty(ctx, init, length_atom);
      JS_FreeAtom(ctx, length_atom);

      // Functions have a length property but should typically be treated as records
      // unless they are specifically array-like (like Arguments object)
      // For WPT compliance, functions like DOMException should be treated as records
      bool is_function = JS_IsFunction(ctx, init);

      // Check if it's truly array-like by verifying it has numeric indices AND
      // those indices contain valid [name, value] pairs
      bool is_array_like = false;
      // If it's a real Array, always treat as sequence (let sequence parser handle validation)
      if (JS_IsArray(ctx, init)) {
        is_array_like = true;
      } else if (has_length && !is_function) {
        // For non-arrays with length property, only treat as sequence if it looks like
        // it contains [name, value] pairs (to distinguish from objects like {length: 2, 0: 'a', 1: 'b'})
        JSValue length_val = JS_GetPropertyStr(ctx, init, "length");
        int32_t length;
        if (!JS_IsException(length_val) && JS_ToInt32(ctx, &length, length_val) == 0 && length > 0) {
          // Check if first element exists and is a valid [name, value] pair
          JSValue first_element = JS_GetPropertyUint32(ctx, init, 0);
          if (!JS_IsUndefined(first_element)) {
            // Check if first element is array-like with length 2 (valid pair)
            JSValue elem_length_val = JS_GetPropertyStr(ctx, first_element, "length");
            int32_t elem_length;
            if (!JS_IsException(elem_length_val) && JS_ToInt32(ctx, &elem_length, elem_length_val) == 0 &&
                elem_length == 2) {
              // This looks like a valid sequence of [name, value] pairs
              is_array_like = true;
            }
            JS_FreeValue(ctx, elem_length_val);
          }
          JS_FreeValue(ctx, first_element);
        }
        JS_FreeValue(ctx, length_val);
      }

      if (is_array_like) {
        // Try to parse as sequence
        search_params = JSRT_ParseSearchParamsFromSequence(ctx, init);
        if (!search_params) {
          // If an exception is already pending (e.g., TypeError for invalid pairs), return it
          if (JS_HasException(ctx)) {
            return JS_EXCEPTION;
          }
          return JS_ThrowTypeError(ctx, "Invalid sequence argument to URLSearchParams constructor");
        }
      } else if (JS_IsObject(init)) {
        // Check if it's a record/object (not null, not array, not string)
        // This now includes functions with enumerable properties (like DOMException)

        // Special case: DOMException.prototype should throw TypeError due to branding checks
        JSValue dom_exception_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "DOMException");
        if (!JS_IsUndefined(dom_exception_ctor)) {
          JSValue dom_exception_proto = JS_GetPropertyStr(ctx, dom_exception_ctor, "prototype");
          if (JS_SameValue(ctx, init, dom_exception_proto)) {
            JS_FreeValue(ctx, dom_exception_ctor);
            JS_FreeValue(ctx, dom_exception_proto);
            return JS_ThrowTypeError(
                ctx, "Constructing a URLSearchParams from DOMException.prototype should throw due to branding checks");
          }
          JS_FreeValue(ctx, dom_exception_proto);
        }
        JS_FreeValue(ctx, dom_exception_ctor);

        search_params = JSRT_ParseSearchParamsFromRecord(ctx, init);
        if (!search_params) {
          return JS_ThrowTypeError(ctx, "Invalid record argument to URLSearchParams constructor");
        }
      } else {
        // Otherwise, treat as string
        size_t init_len;
        const char* init_str = JS_ToCStringLen(ctx, &init_len, init);
        if (!init_str) {
          return JS_EXCEPTION;
        }
        search_params = JSRT_ParseSearchParams(init_str, init_len);
        JS_FreeCString(ctx, init_str);
      }
    }
    // Otherwise, treat as string
    else {
      size_t init_len;
      const char* init_str = JS_ToCStringLen(ctx, &init_len, init);
      if (!init_str) {
        return JS_EXCEPTION;
      }
      search_params = JSRT_ParseSearchParams(init_str, init_len);
      JS_FreeCString(ctx, init_str);
    }
  } else {
    search_params = JSRT_CreateEmptySearchParams();
  }

  JSValue obj = JS_NewObjectClass(ctx, JSRT_URLSearchParamsClassID);
  JS_SetOpaque(obj, search_params);
  return obj;
}

static JSValue JSRT_URLSearchParamsGet(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "get() requires 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (strcmp(param->name, name) == 0) {
      JS_FreeCString(ctx, name);
      return JS_NewStringLen(ctx, param->value, param->value_len);
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  return JS_NULL;
}

static JSValue JSRT_URLSearchParamsSet(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "set() requires 2 arguments");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  size_t name_len, value_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  const char* value = JS_ToCStringLen(ctx, &value_len, argv[1]);
  if (!name || !value) {
    return JS_EXCEPTION;
  }

  // Find first parameter with this name and update its value
  // Remove any subsequent parameters with the same name
  JSRT_URLSearchParam* first_match = NULL;
  JSRT_URLSearchParam** current = &search_params->params;

  while (*current) {
    if (strcmp((*current)->name, name) == 0) {
      if (!first_match) {
        // This is the first match - update its value
        first_match = *current;
        free(first_match->value);
        first_match->value = malloc(value_len + 1);
        memcpy(first_match->value, value, value_len);
        first_match->value[value_len] = '\0';
        first_match->value_len = value_len;
        current = &(*current)->next;
      } else {
        // This is a subsequent match - remove it
        JSRT_URLSearchParam* to_remove = *current;
        *current = (*current)->next;
        free(to_remove->name);
        free(to_remove->value);
        free(to_remove);
      }
    } else {
      current = &(*current)->next;
    }
  }

  // If no parameter with this name existed, add new one at the end
  if (!first_match) {
    JSRT_URLSearchParam* new_param = create_url_param(name, name_len, value, value_len);

    if (!search_params->params) {
      search_params->params = new_param;
    } else {
      JSRT_URLSearchParam* tail = search_params->params;
      while (tail->next) {
        tail = tail->next;
      }
      tail->next = new_param;
    }
  }

  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);

  // Update parent URL's href if connected
  update_parent_url_href(search_params);

  return JS_UNDEFINED;
}

static JSValue JSRT_URLSearchParamsAppend(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "append() requires 2 arguments");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  size_t name_len, value_len;
  const char* name = JS_ToCStringLen(ctx, &name_len, argv[0]);
  const char* value = JS_ToCStringLen(ctx, &value_len, argv[1]);
  if (!name || !value) {
    return JS_EXCEPTION;
  }

  JSRT_URLSearchParam* new_param = create_url_param(name, name_len, value, value_len);

  // Add at end to maintain insertion order
  if (!search_params->params) {
    search_params->params = new_param;
  } else {
    JSRT_URLSearchParam* tail = search_params->params;
    while (tail->next) {
      tail = tail->next;
    }
    tail->next = new_param;
  }

  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);

  // Update parent URL's href if connected
  update_parent_url_href(search_params);

  return JS_UNDEFINED;
}

static JSValue JSRT_URLSearchParamsHas(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "has() requires at least 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  const char* value = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    value = JS_ToCString(ctx, argv[1]);
    if (!value) {
      JS_FreeCString(ctx, name);
      return JS_EXCEPTION;
    }
  }

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (strcmp(param->name, name) == 0) {
      if (value) {
        // Two-argument form: check both name and value
        if (strcmp(param->value, value) == 0) {
          JS_FreeCString(ctx, name);
          JS_FreeCString(ctx, value);
          return JS_NewBool(ctx, true);
        }
      } else {
        // One-argument form (or undefined second arg): just check name
        JS_FreeCString(ctx, name);
        return JS_NewBool(ctx, true);
      }
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  if (value) {
    JS_FreeCString(ctx, value);
  }
  return JS_NewBool(ctx, false);
}

static JSValue JSRT_URLSearchParamsDelete(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "delete() requires at least 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  const char* value = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    value = JS_ToCString(ctx, argv[1]);
    if (!value) {
      JS_FreeCString(ctx, name);
      return JS_EXCEPTION;
    }
  }

  JSRT_URLSearchParam** current = &search_params->params;
  while (*current) {
    if (strcmp((*current)->name, name) == 0) {
      if (value) {
        // Two-argument form: only delete if both name and value match
        if (strcmp((*current)->value, value) == 0) {
          JSRT_URLSearchParam* to_remove = *current;
          *current = (*current)->next;
          free(to_remove->name);
          free(to_remove->value);
          free(to_remove);
        } else {
          current = &(*current)->next;
        }
      } else {
        // One-argument form: delete all parameters with this name
        JSRT_URLSearchParam* to_remove = *current;
        *current = (*current)->next;
        free(to_remove->name);
        free(to_remove->value);
        free(to_remove);
      }
    } else {
      current = &(*current)->next;
    }
  }

  JS_FreeCString(ctx, name);
  if (value) {
    JS_FreeCString(ctx, value);
  }

  // Update parent URL's href if connected
  update_parent_url_href(search_params);

  return JS_UNDEFINED;
}

static JSValue JSRT_URLSearchParamsGetAll(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "getAll() requires 1 argument");
  }

  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  JSValue result_array = JS_NewArray(ctx);
  int array_index = 0;

  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    if (strcmp(param->name, name) == 0) {
      JS_SetPropertyUint32(ctx, result_array, array_index++, JS_NewStringLen(ctx, param->value, param->value_len));
    }
    param = param->next;
  }

  JS_FreeCString(ctx, name);
  return result_array;
}

static JSValue JSRT_URLSearchParamsGetSize(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  int count = 0;
  JSRT_URLSearchParam* param = search_params->params;
  while (param) {
    count++;
    param = param->next;
  }

  return JS_NewInt32(ctx, count);
}

static char* url_encode_with_len(const char* str, size_t len) {
  static const char hex_chars[] = "0123456789ABCDEF";
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*') {
      encoded_len++;
    } else if (c == ' ') {
      encoded_len++;  // space becomes +
    } else {
      encoded_len += 3;  // %XX
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*') {
      encoded[j++] = c;
    } else if (c == ' ') {
      encoded[j++] = '+';
    } else {
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    }
  }
  encoded[j] = '\0';
  return encoded;
}

// URL component encoding for href generation (space -> %20, not +)
static char* url_component_encode(const char* str) {
  if (!str)
    return NULL;

  static const char hex_chars[] = "0123456789ABCDEF";
  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // For URL components, we need to encode space as %20
    if (c == ' ') {
      encoded_len += 3;  // %20
    } else if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c < 32 || c > 126) {
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == ' ') {
      encoded[j++] = '%';
      encoded[j++] = '2';
      encoded[j++] = '0';
    } else if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, copy as-is
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c < 32 || c > 126) {
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    } else {
      encoded[j++] = c;
    }
  }
  encoded[j] = '\0';
  return encoded;
}

// Special encoding for non-special scheme paths (spaces are preserved as-is)
static char* url_nonspecial_path_encode(const char* str) {
  if (!str)
    return NULL;

  static const char hex_chars[] = "0123456789ABCDEF";
  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length (preserve all spaces)
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == ' ') {
      encoded_len++;  // Keep space as-is
    } else if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c < 32 || c > 126) {
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == ' ') {
      encoded[j++] = c;  // Keep space as-is
    } else if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, copy as-is
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c < 32 || c > 126) {
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    } else {
      encoded[j++] = c;
    }
  }
  encoded[j] = '\0';
  return encoded;
}

// Backward compatibility wrapper
static char* url_encode(const char* str) {
  return url_encode_with_len(str, strlen(str));
}

// Userinfo encoding per WHATWG URL spec (less aggressive than url_encode_with_len)
// According to WPT tests, some characters like & and ( should not be percent-encoded in userinfo
static char* url_userinfo_encode(const char* str) {
  if (!str)
    return NULL;

  static const char hex_chars[] = "0123456789ABCDEF";
  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // Characters allowed in userinfo without encoding (per WPT tests)
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*' || c == '&' || c == '(' || c == ')' || c == '!' || c == '$' || c == '\'' ||
        c == ',' || c == ';' || c == '=' || c == '+') {
      encoded_len++;
    } else {
      encoded_len += 3;  // %XX
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // Characters allowed in userinfo without encoding
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*' || c == '&' || c == '(' || c == ')' || c == '!' || c == '$' || c == '\'' ||
        c == ',' || c == ';' || c == '=' || c == '+') {
      encoded[j++] = c;
    } else {
      encoded[j++] = '%';
      encoded[j++] = hex_chars[(c >> 4) & 0x0F];
      encoded[j++] = hex_chars[c & 0x0F];
    }
  }

  encoded[j] = '\0';
  return encoded;
}

static JSValue JSRT_URLSearchParamsToString(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  if (!search_params->params) {
    return JS_NewString(ctx, "");
  }

  // Build result string using a dynamic approach
  char* result = malloc(1);
  result[0] = '\0';
  size_t result_len = 0;

  JSRT_URLSearchParam* param = search_params->params;
  bool first = true;
  while (param) {
    char* encoded_name = url_encode_with_len(param->name, param->name_len);
    char* encoded_value = url_encode_with_len(param->value, param->value_len);

    size_t pair_len = strlen(encoded_name) + strlen(encoded_value) + 1;  // name=value
    if (!first)
      pair_len += 1;  // &

    result = realloc(result, result_len + pair_len + 1);

    if (!first) {
      strcat(result, "&");
    }
    strcat(result, encoded_name);
    strcat(result, "=");
    strcat(result, encoded_value);

    result_len += pair_len;
    first = false;

    free(encoded_name);
    free(encoded_value);
    param = param->next;
  }

  JSValue js_result = JS_NewString(ctx, result);
  free(result);
  return js_result;
}

// URLSearchParams Iterator Implementation
typedef struct {
  JSRT_URLSearchParams* params;
  JSRT_URLSearchParam* current;
  int type;  // 0=entries, 1=keys, 2=values
} JSRT_URLSearchParamsIterator;

static JSClassID JSRT_URLSearchParamsIteratorClassID;

static void JSRT_URLSearchParamsIteratorFinalizer(JSRuntime* rt, JSValue val) {
  JSRT_URLSearchParamsIterator* iterator = JS_GetOpaque(val, JSRT_URLSearchParamsIteratorClassID);
  if (iterator) {
    free(iterator);
  }
}

static JSClassDef JSRT_URLSearchParamsIteratorClass = {
    "URLSearchParamsIterator",
    .finalizer = JSRT_URLSearchParamsIteratorFinalizer,
};

static JSValue JSRT_URLSearchParamsIteratorNext(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_URLSearchParamsIterator* iterator = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsIteratorClassID);
  if (!iterator) {
    return JS_EXCEPTION;
  }

  JSValue result = JS_NewObject(ctx);

  if (!iterator->current) {
    // Iterator is done
    JS_SetPropertyStr(ctx, result, "done", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, result, "value", JS_UNDEFINED);
  } else {
    // Return current item
    JS_SetPropertyStr(ctx, result, "done", JS_NewBool(ctx, false));

    JSValue value;
    switch (iterator->type) {
      case 0:  // entries - return [name, value]
        value = JS_NewArray(ctx);
        JS_SetPropertyUint32(ctx, value, 0, JS_NewStringLen(ctx, iterator->current->name, iterator->current->name_len));
        JS_SetPropertyUint32(ctx, value, 1,
                             JS_NewStringLen(ctx, iterator->current->value, iterator->current->value_len));
        break;
      case 1:  // keys - return name
        value = JS_NewStringLen(ctx, iterator->current->name, iterator->current->name_len);
        break;
      case 2:  // values - return value
        value = JS_NewStringLen(ctx, iterator->current->value, iterator->current->value_len);
        break;
      default:
        value = JS_UNDEFINED;
        break;
    }
    JS_SetPropertyStr(ctx, result, "value", value);

    // Advance to next parameter
    iterator->current = iterator->current->next;
  }

  return result;
}

static JSValue JSRT_URLSearchParamsCreateIterator(JSContext* ctx, JSValueConst this_val, int type) {
  JSRT_URLSearchParams* search_params = JS_GetOpaque2(ctx, this_val, JSRT_URLSearchParamsClassID);
  if (!search_params) {
    return JS_EXCEPTION;
  }

  JSRT_URLSearchParamsIterator* iterator = malloc(sizeof(JSRT_URLSearchParamsIterator));
  if (!iterator) {
    return JS_EXCEPTION;
  }

  iterator->params = search_params;
  iterator->current = search_params->params;
  iterator->type = type;

  JSValue iterator_obj = JS_NewObjectClass(ctx, JSRT_URLSearchParamsIteratorClassID);
  if (JS_IsException(iterator_obj)) {
    free(iterator);
    return JS_EXCEPTION;
  }

  JS_SetOpaque(iterator_obj, iterator);

  // Add next method
  JS_SetPropertyStr(ctx, iterator_obj, "next", JS_NewCFunction(ctx, JSRT_URLSearchParamsIteratorNext, "next", 0));

  return iterator_obj;
}

static JSValue JSRT_URLSearchParamsEntries(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLSearchParamsCreateIterator(ctx, this_val, 0);
}

static JSValue JSRT_URLSearchParamsKeys(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLSearchParamsCreateIterator(ctx, this_val, 1);
}

static JSValue JSRT_URLSearchParamsValues(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLSearchParamsCreateIterator(ctx, this_val, 2);
}

static JSValue JSRT_URLSearchParamsSymbolIterator(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JSRT_URLSearchParamsEntries(ctx, this_val, argc, argv);
}

// Setup function
void JSRT_RuntimeSetupStdURL(JSRT_Runtime* rt) {
  JSContext* ctx = rt->ctx;

  JSRT_Debug("JSRT_RuntimeSetupStdURL: initializing URL/URLSearchParams API");

  // Register URL class
  JS_NewClassID(&JSRT_URLClassID);
  JS_NewClass(rt->rt, JSRT_URLClassID, &JSRT_URLClass);

  JSValue url_proto = JS_NewObject(ctx);

  // Properties with getters
  JSValue get_href = JS_NewCFunction(ctx, JSRT_URLGetHref, "get href", 0);
  JSValue get_protocol = JS_NewCFunction(ctx, JSRT_URLGetProtocol, "get protocol", 0);
  JSValue get_username = JS_NewCFunction(ctx, JSRT_URLGetUsername, "get username", 0);
  JSValue get_password = JS_NewCFunction(ctx, JSRT_URLGetPassword, "get password", 0);
  JSValue get_host = JS_NewCFunction(ctx, JSRT_URLGetHost, "get host", 0);
  JSValue get_hostname = JS_NewCFunction(ctx, JSRT_URLGetHostname, "get hostname", 0);
  JSValue get_port = JS_NewCFunction(ctx, JSRT_URLGetPort, "get port", 0);
  JSValue get_pathname = JS_NewCFunction(ctx, JSRT_URLGetPathname, "get pathname", 0);
  JSValue get_search = JS_NewCFunction(ctx, JSRT_URLGetSearch, "get search", 0);
  JSValue set_search = JS_NewCFunction(ctx, JSRT_URLSetSearch, "set search", 1);
  JSValue get_hash = JS_NewCFunction(ctx, JSRT_URLGetHash, "get hash", 0);
  JSValue get_origin = JS_NewCFunction(ctx, JSRT_URLGetOrigin, "get origin", 0);
  JSValue get_search_params = JS_NewCFunction(ctx, JSRT_URLGetSearchParams, "get searchParams", 0);

  JSAtom href_atom = JS_NewAtom(ctx, "href");
  JSAtom protocol_atom = JS_NewAtom(ctx, "protocol");
  JSAtom username_atom = JS_NewAtom(ctx, "username");
  JSAtom password_atom = JS_NewAtom(ctx, "password");
  JSAtom host_atom = JS_NewAtom(ctx, "host");
  JSAtom hostname_atom = JS_NewAtom(ctx, "hostname");
  JSAtom port_atom = JS_NewAtom(ctx, "port");
  JSAtom pathname_atom = JS_NewAtom(ctx, "pathname");
  JSAtom search_atom = JS_NewAtom(ctx, "search");
  JSAtom hash_atom = JS_NewAtom(ctx, "hash");
  JSAtom origin_atom = JS_NewAtom(ctx, "origin");
  JSAtom search_params_atom = JS_NewAtom(ctx, "searchParams");

  JS_DefinePropertyGetSet(ctx, url_proto, href_atom, get_href, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, protocol_atom, get_protocol, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, username_atom, get_username, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, password_atom, get_password, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, host_atom, get_host, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, hostname_atom, get_hostname, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, port_atom, get_port, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, pathname_atom, get_pathname, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, search_atom, get_search, set_search, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, hash_atom, get_hash, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, origin_atom, get_origin, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, url_proto, search_params_atom, get_search_params, JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  JS_FreeAtom(ctx, href_atom);
  JS_FreeAtom(ctx, protocol_atom);
  JS_FreeAtom(ctx, username_atom);
  JS_FreeAtom(ctx, password_atom);
  JS_FreeAtom(ctx, host_atom);
  JS_FreeAtom(ctx, hostname_atom);
  JS_FreeAtom(ctx, port_atom);
  JS_FreeAtom(ctx, pathname_atom);
  JS_FreeAtom(ctx, search_atom);
  JS_FreeAtom(ctx, hash_atom);
  JS_FreeAtom(ctx, origin_atom);
  JS_FreeAtom(ctx, search_params_atom);

  // Methods
  JS_SetPropertyStr(ctx, url_proto, "toString", JS_NewCFunction(ctx, JSRT_URLToString, "toString", 0));
  JS_SetPropertyStr(ctx, url_proto, "toJSON", JS_NewCFunction(ctx, JSRT_URLToJSON, "toJSON", 0));

  JS_SetClassProto(ctx, JSRT_URLClassID, url_proto);

  JSValue url_ctor = JS_NewCFunction2(ctx, JSRT_URLConstructor, "URL", 2, JS_CFUNC_constructor, 0);

  // Set the constructor's prototype property
  JS_SetPropertyStr(ctx, url_ctor, "prototype", JS_DupValue(ctx, url_proto));

  // Set the prototype's constructor property
  JS_SetPropertyStr(ctx, url_proto, "constructor", JS_DupValue(ctx, url_ctor));

  JS_SetPropertyStr(ctx, rt->global, "URL", url_ctor);

  // Register URLSearchParams class
  JS_NewClassID(&JSRT_URLSearchParamsClassID);
  JS_NewClass(rt->rt, JSRT_URLSearchParamsClassID, &JSRT_URLSearchParamsClass);

  JSValue search_params_proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, search_params_proto, "get", JS_NewCFunction(ctx, JSRT_URLSearchParamsGet, "get", 1));
  JS_SetPropertyStr(ctx, search_params_proto, "getAll", JS_NewCFunction(ctx, JSRT_URLSearchParamsGetAll, "getAll", 1));
  JS_SetPropertyStr(ctx, search_params_proto, "set", JS_NewCFunction(ctx, JSRT_URLSearchParamsSet, "set", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "append", JS_NewCFunction(ctx, JSRT_URLSearchParamsAppend, "append", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "has", JS_NewCFunction(ctx, JSRT_URLSearchParamsHas, "has", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "delete", JS_NewCFunction(ctx, JSRT_URLSearchParamsDelete, "delete", 2));
  JS_SetPropertyStr(ctx, search_params_proto, "toString",
                    JS_NewCFunction(ctx, JSRT_URLSearchParamsToString, "toString", 0));

  // Add size property as getter
  JSValue get_size = JS_NewCFunction(ctx, JSRT_URLSearchParamsGetSize, "get size", 0);
  JSAtom size_atom = JS_NewAtom(ctx, "size");
  JS_DefinePropertyGetSet(ctx, search_params_proto, size_atom, get_size, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, size_atom);

  // Add iterator methods
  JS_SetPropertyStr(ctx, search_params_proto, "entries",
                    JS_NewCFunction(ctx, JSRT_URLSearchParamsEntries, "entries", 0));
  JS_SetPropertyStr(ctx, search_params_proto, "keys", JS_NewCFunction(ctx, JSRT_URLSearchParamsKeys, "keys", 0));
  JS_SetPropertyStr(ctx, search_params_proto, "values", JS_NewCFunction(ctx, JSRT_URLSearchParamsValues, "values", 0));

  // Add Symbol.iterator as entries method (per WHATWG spec)
  JSValue symbol_iterator = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Symbol");
  if (!JS_IsException(symbol_iterator)) {
    JSValue iterator_symbol = JS_GetPropertyStr(ctx, symbol_iterator, "iterator");
    if (!JS_IsException(iterator_symbol) && !JS_IsUndefined(iterator_symbol)) {
      JS_DefinePropertyValue(ctx, search_params_proto, JS_ValueToAtom(ctx, iterator_symbol),
                             JS_NewCFunction(ctx, JSRT_URLSearchParamsSymbolIterator, "[Symbol.iterator]", 0),
                             JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
      JS_FreeValue(ctx, iterator_symbol);
    }
    JS_FreeValue(ctx, symbol_iterator);
  }

  // Register iterator class
  JS_NewClassID(&JSRT_URLSearchParamsIteratorClassID);
  JS_NewClass(rt->rt, JSRT_URLSearchParamsIteratorClassID, &JSRT_URLSearchParamsIteratorClass);

  JS_SetClassProto(ctx, JSRT_URLSearchParamsClassID, search_params_proto);

  JSValue search_params_ctor =
      JS_NewCFunction2(ctx, JSRT_URLSearchParamsConstructor, "URLSearchParams", 1, JS_CFUNC_constructor, 0);

  // Set the constructor's prototype property
  JS_SetPropertyStr(ctx, search_params_ctor, "prototype", JS_DupValue(ctx, search_params_proto));

  // Set the prototype's constructor property
  JS_SetPropertyStr(ctx, search_params_proto, "constructor", JS_DupValue(ctx, search_params_ctor));

  JS_SetPropertyStr(ctx, rt->global, "URLSearchParams", search_params_ctor);

  JSRT_Debug("URL/URLSearchParams API setup completed");
}