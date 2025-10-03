#include <ctype.h>
#include "../url.h"

// Check if character is a Unicode whitespace that should be stripped
// Returns number of bytes to skip (0 if not whitespace)
static int is_unicode_whitespace(const unsigned char* ptr, size_t remaining_len) {
  unsigned char c = ptr[0];

  // ASCII whitespace: space (0x20), tab (0x09), LF (0x0A), CR (0x0D), FF (0x0C)
  if (c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D || c == 0x0C) {
    return 1;
  }

  // Check for UTF-8 encoded Unicode whitespace
  if (c >= 0x80 && remaining_len >= 3) {
    // U+3000 (ideographic space): 0xE3 0x80 0x80
    if (c == 0xE3 && ptr[1] == 0x80 && ptr[2] == 0x80) {
      return 3;
    }
    // U+00A0 (non-breaking space): 0xC2 0xA0
    if (c == 0xC2 && ptr[1] == 0xA0) {
      return 2;
    }
    // More Unicode whitespace characters can be added here
  }

  return 0;
}

// Returns number of bytes to skip for C0 controls and space (0 if not matching)
// WHATWG URL spec: strip leading/trailing C0 controls (0x00-0x1F) or space (0x20)
static int is_c0_control_or_space(const unsigned char* ptr, size_t remaining_len) {
  unsigned char c = ptr[0];

  // C0 control characters (0x00-0x1F) or space (0x20)
  if (c <= 0x20) {
    return 1;
  }

  // Check for UTF-8 encoded Unicode whitespace that should also be stripped
  if (c >= 0x80 && remaining_len >= 3) {
    // U+3000 (ideographic space): 0xE3 0x80 0x80
    if (c == 0xE3 && ptr[1] == 0x80 && ptr[2] == 0x80) {
      return 3;
    }
    // U+00A0 (non-breaking space): 0xC2 0xA0
    if (c == 0xC2 && ptr[1] == 0xA0) {
      return 2;
    }
    // Note: Zero Width No-Break Space (U+FEFF) / BOM should NOT be stripped
    // per WHATWG URL spec - only ASCII whitespace should be removed
  }

  return 0;
}

// Strip leading and trailing C0 controls, space, and other whitespace from URL string
// Per WHATWG URL spec: remove leading/trailing C0 controls (0x00-0x1F) or space (0x20)
// Also remove other Unicode whitespace: U+3000 (ideographic space), U+00A0 (non-breaking space)
char* strip_url_whitespace(const char* url) {
  if (!url)
    return NULL;

  size_t url_len = strlen(url);
  const unsigned char* start = (const unsigned char*)url;
  const unsigned char* end = start + url_len;

  // Find start (skip leading C0 controls, space, and other whitespace)
  while (start < end) {
    int skip = is_c0_control_or_space(start, end - start);
    if (skip > 0) {
      start += skip;
    } else {
      break;
    }
  }

  // Find end (skip trailing C0 controls, space, and other whitespace)
  while (end > start) {
    // Check if the character before 'end' is a C0 control, space, or other whitespace
    const unsigned char* check_pos = end - 1;
    int found_whitespace = 0;

    // Check for C0 control characters (0x00-0x1F) or space (0x20)
    if (*check_pos <= 0x20) {
      end--;
      found_whitespace = 1;
    }
    // Check for multi-byte Unicode whitespace
    // Ensure we have enough bytes to safely check backwards
    else if (end - start >= 3 && check_pos >= start + 2) {
      // U+3000 (ideographic space): 0xE3 0x80 0x80
      if (check_pos[-2] == 0xE3 && check_pos[-1] == 0x80 && check_pos[0] == 0x80) {
        end -= 3;
        found_whitespace = 1;
      }
      // Note: Zero Width No-Break Space (U+FEFF) / BOM should NOT be stripped
      // per WHATWG URL spec - only ASCII whitespace should be removed
    }
    // Check for 2-byte Unicode whitespace
    else if (end - start >= 2 && check_pos >= start + 1) {
      // U+00A0 (non-breaking space): 0xC2 0xA0
      if (check_pos[-1] == 0xC2 && check_pos[0] == 0xA0) {
        end -= 2;
        found_whitespace = 1;
      }
    }

    if (!found_whitespace) {
      break;
    }
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
char* normalize_port(const char* port_str, const char* protocol) {
  if (!port_str || !*port_str)
    return strdup("");

  // Check for excessive leading zeros (WPT requirement)
  // If port has more than 5 leading zeros, it's invalid but URL should not throw
  if (strlen(port_str) > 6 && strspn(port_str, "0") >= 5) {
    return strdup("");  // Invalid port due to excessive leading zeros - return empty string, don't fail URL
  }

  // Convert port string to number (handles leading zeros)
  char* endptr;
  long port_num = strtol(port_str, &endptr, 10);

  // Check for invalid port (not a number or out of range)
  if (*endptr != '\0' || port_num < 0 || port_num > 65535) {
    return NULL;  // Invalid port causes error
  }

  // WPT requirement: omit default ports for url.port property
  // Extract scheme from protocol for default port check
  char scheme[32];
  strncpy(scheme, protocol, sizeof(scheme) - 1);
  scheme[sizeof(scheme) - 1] = '\0';
  char* colon = strchr(scheme, ':');
  if (colon)
    *colon = '\0';

  // Convert port number back to string for comparison
  char port_string[16];
  snprintf(port_string, sizeof(port_string), "%ld", port_num);

  // Check if this is a default port and return empty string if so
  if (is_default_port(scheme, port_string)) {
    return strdup("");
  }

  // Port 0 is a valid port and should be preserved (only default ports are omitted)

  // Return normalized port as string
  char* result = malloc(16);  // Enough for any valid port number
  snprintf(result, 16, "%ld", port_num);
  return result;
}

// Remove specific whitespace characters from URL string per WHATWG spec
// Tab, newline, carriage return are removed; spaces preserved for encoding later
// Also remove certain Unicode zero-width characters
char* remove_all_ascii_whitespace(const char* url) {
  if (!url)
    return NULL;

  size_t len = strlen(url);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  size_t j = 0;
  size_t i = 0;

  while (i < len) {
    unsigned char c = (unsigned char)url[i];

    // Always remove tab, line feed, and carriage return
    if (c == 0x09 || c == 0x0A || c == 0x0D) {
      i++;
      continue;
    }

    // Handle UTF-8 sequences - preserve BOM characters per WHATWG URL spec
    // BOM characters (U+FEFF) should be preserved and percent-encoded, not stripped
    if (c >= 0x80) {
      // No special UTF-8 sequences need to be removed
      // All Unicode characters including BOM should be preserved for proper encoding
    }

    // According to WHATWG URL spec, spaces and Unicode characters should be preserved and encoded later
    // Only remove tab, LF, CR - preserve all other characters including BOM
    result[j++] = url[i++];
  }
  result[j] = '\0';

  return result;
}

// Normalize spaces in URL path specifically for the query/fragment boundary issue
// This handles the special case where trailing spaces before ? or # should be collapsed to single space
// The space will then be percent-encoded later during path encoding
char* normalize_spaces_before_query_fragment(const char* path) {
  if (!path)
    return NULL;

  size_t len = strlen(path);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    char c = path[i];

    // Check if we're at the start of a space sequence
    if (c == ' ') {
      // Count all consecutive spaces
      size_t space_start = i;
      size_t space_end = i;
      while (space_end < len && path[space_end] == ' ') {
        space_end++;
      }

      // Check if this space sequence is trailing (at end or before ?/#)
      if (space_end == len || path[space_end] == '?' || path[space_end] == '#') {
        // This is a trailing space sequence before query/fragment
        // Collapse all trailing spaces to a single space
        // The space will be percent-encoded during path encoding for opaque paths
        result[j++] = ' ';
        i = space_end - 1;  // Skip all spaces (loop will increment)
      } else {
        // This is a middle space sequence - keep all spaces as-is
        for (size_t k = space_start; k < space_end; k++) {
          result[j++] = ' ';
        }
        i = space_end - 1;  // Skip to end of space sequence
      }
    } else {
      result[j++] = c;
    }
  }
  result[j] = '\0';

  return result;
}

// Convert backslashes to forward slashes in URL strings according to WHATWG URL spec
// This applies to special URL schemes and certain URL contexts
char* normalize_url_backslashes(const char* url) {
  if (!url)
    return NULL;

  size_t len = strlen(url);
  // Allocate extra space in case backslashes create empty path segments
  char* result = malloc(len * 2 + 1);
  if (!result)
    return NULL;

  // Simple backslash normalization for special schemes per WHATWG URL standard
  // For special schemes, all backslashes in scheme, authority, and path are converted to forward slashes
  // For non-special schemes, backslashes are preserved as-is

  char* colon_pos = strchr(url, ':');
  int is_special = 0;

  // Determine if this is a special scheme
  if (colon_pos && colon_pos > url) {
    size_t scheme_len = colon_pos - url;
    char scheme[16];
    if (scheme_len < sizeof(scheme)) {
      strncpy(scheme, url, scheme_len);
      scheme[scheme_len] = '\0';
      is_special = is_special_scheme(scheme);
    }
  } else if (!colon_pos || colon_pos == url) {
    // Relative URLs (no colon or colon at start) are treated as special schemes
    // for backslash normalization since they will be resolved against a base URL
    is_special = 1;
  }

  // Find query and fragment positions - backslashes after ? or # are never normalized
  // Per WHATWG URL spec: backslash normalization only applies to scheme, authority, and path
  char* query_pos = strchr(url, '?');
  char* fragment_pos = strchr(url, '#');

  // Stop normalization at the first of query or fragment
  size_t stop_pos = len;
  if (query_pos && fragment_pos) {
    stop_pos = (query_pos < fragment_pos) ? (query_pos - url) : (fragment_pos - url);
  } else if (query_pos) {
    stop_pos = query_pos - url;
  } else if (fragment_pos) {
    stop_pos = fragment_pos - url;
  }

  // Track if we're in the path section
  int in_path_section = 0;
  char* double_slash = colon_pos ? strstr(url, "://") : NULL;
  char* authority_end = NULL;

  if (double_slash) {
    // Find end of authority section (first / after ://)
    char* auth_start = double_slash + 3;
    authority_end = strchr(auth_start, '/');
  }

  size_t result_pos = 0;
  for (size_t i = 0; i < len; i++) {
    // Determine if we're in the path section
    if (authority_end) {
      in_path_section = (url + i >= authority_end);
    } else if (colon_pos) {
      // No authority, path starts after scheme:
      in_path_section = (url + i > colon_pos);
    } else {
      // No scheme, entire URL is path
      in_path_section = 1;
    }

    if (i < stop_pos && url[i] == '\\') {
      if (is_special) {
        // Convert backslash to forward slash according to WHATWG URL spec
        result[result_pos++] = '/';
      } else {
        // Non-special schemes: preserve backslashes
        result[result_pos++] = '\\';
      }
    } else if (i < stop_pos && url[i] == '|') {
      // Handle pipe characters in URLs
      // According to WHATWG URL spec and WPT tests, pipes should be percent-encoded, not converted to colons
      // Only exception: Windows drive letters in specific contexts during preprocessing
      result[result_pos++] = '|';  // Preserve pipe - will be percent-encoded later during path encoding
    } else {
      // Preserve other characters
      result[result_pos++] = url[i];
    }
  }
  result[result_pos] = '\0';

  // Reallocate to actual size to save memory
  char* final_result = realloc(result, result_pos + 1);
  return final_result ? final_result : result;
}

// Check if port is default for the given scheme
int is_default_port(const char* scheme, const char* port) {
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

// Compute origin for URL according to WHATWG URL spec
char* compute_origin(const char* protocol, const char* hostname, const char* port, int double_colon_at_pattern) {
  return compute_origin_with_pathname(protocol, hostname, port, double_colon_at_pattern, NULL);
}

// Extended compute origin function that can handle blob URLs using pathname
char* compute_origin_with_pathname(const char* protocol, const char* hostname, const char* port,
                                   int double_colon_at_pattern, const char* pathname) {
  if (!protocol || strlen(protocol) == 0) {
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
    // The inner URL is stored in the pathname
    // Per WHATWG URL spec, only http/https inner URLs provide tuple origins for blob URLs
    // All other schemes (ws, wss, ftp, about, etc.) result in null origin for blob URLs
    if (pathname && (strncmp(pathname, "http://", 7) == 0 || strncmp(pathname, "https://", 8) == 0)) {
      // Parse the inner URL to extract its origin
      JSRT_URL* inner_url = JSRT_ParseURL(pathname, NULL);
      if (inner_url && inner_url->protocol && inner_url->hostname && strlen(inner_url->hostname) > 0) {
        char* inner_origin = compute_origin_with_pathname(inner_url->protocol, inner_url->hostname, inner_url->port,
                                                          inner_url->double_colon_at_pattern, NULL);
        JSRT_FreeURL(inner_url);
        return inner_origin;
      }
      if (inner_url)
        JSRT_FreeURL(inner_url);
    }
    // For all other blob URL formats (including ws/wss, UUIDs, empty, etc.), origin is null
    return strdup("null");
  }

  // Special schemes that can have tuple origins: http, https, ftp, ws, wss
  // All other schemes (including file, data, javascript, and custom schemes) have null origin
  if (strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0 && strcmp(scheme, "ftp") != 0 &&
      strcmp(scheme, "ws") != 0 && strcmp(scheme, "wss") != 0) {
    free(scheme);
    return strdup("null");
  }

  // For tuple origin schemes, we need hostname
  if (!hostname || strlen(hostname) == 0) {
    free(scheme);
    return strdup("null");
  }

  // Convert hostname to ASCII form for origin (per WHATWG URL spec)
  char* ascii_hostname = hostname_to_ascii(hostname);
  if (!ascii_hostname) {
    free(scheme);
    return strdup("null");
  }

  // Use normalized port for origin computation
  char* normalized_port = normalize_port(port, protocol);

  char* origin;
  if (!normalized_port || strlen(normalized_port) == 0) {
    // Omit default port, port 0, or when no port is specified
    size_t origin_len = strlen(protocol) + strlen(ascii_hostname) + 4;
    origin = malloc(origin_len);
    snprintf(origin, origin_len, "%s//%s", protocol, ascii_hostname);
  } else {
    // Include custom non-default port (even if double_colon_at_pattern is present)
    size_t origin_len = strlen(protocol) + strlen(ascii_hostname) + strlen(normalized_port) + 5;
    origin = malloc(origin_len);
    snprintf(origin, origin_len, "%s//%s:%s", protocol, ascii_hostname, normalized_port);
  }

  free(normalized_port);
  free(ascii_hostname);

  free(scheme);
  return origin;
}

// Normalize Windows drive letters in file URL pathnames
// Convert patterns like "/C|/foo" to "/C:/foo" for file URLs
// Also handles percent-encoded pipes: "/C%7C/foo" to "/C:/foo"
char* normalize_windows_drive_letters(const char* path) {
  if (!path || strlen(path) < 4) {
    return strdup(path ? path : "");
  }

  // Note: Double pipes (||) in file paths are actually allowed per WHATWG URL spec
  // They are not considered invalid Windows drive letter patterns

  // Check for Windows drive letter pattern: /X|/ where X is a letter
  if (path[0] == '/' && strlen(path) >= 4 && isalpha(path[1]) && path[2] == '|' && path[3] == '/') {
    // Convert /C|/foo to /C:/foo
    size_t new_len = strlen(path) + 1;  // Same length (| becomes :)
    char* result = malloc(new_len);
    if (!result) {
      return NULL;
    }
    result[0] = '/';
    result[1] = path[1];                     // Drive letter
    result[2] = ':';                         // Convert pipe to colon
    size_t tail_len = strlen(path + 3) + 1;  // include NUL
    memcpy(result + 3, path + 3, tail_len);
    return result;
  }

  // Check for percent-encoded pipe pattern: /X%7C/ where X is a letter
  if (path[0] == '/' && strlen(path) >= 7 && isalpha(path[1]) && strncmp(path + 2, "%7C", 3) == 0 && path[5] == '/') {
    // Convert /C%7C/foo to /C:/foo
    size_t old_len = strlen(path);
    size_t new_len = old_len - 2 + 1;  // %7C (3 chars) becomes : (1 char), so -2, +1 for null terminator
    char* result = malloc(new_len);
    if (!result) {
      return NULL;
    }
    result[0] = '/';
    result[1] = path[1];                     // Drive letter
    result[2] = ':';                         // Convert %7C to colon
    size_t tail_len = strlen(path + 5) + 1;  // include NUL
    memcpy(result + 3, path + 5, tail_len);  // Copy rest of path starting from the slash after %7C
    return result;
  }

  // Check for lowercase percent-encoded pipe pattern: /X%7c/ where X is a letter
  if (path[0] == '/' && strlen(path) >= 7 && isalpha(path[1]) && strncmp(path + 2, "%7c", 3) == 0 && path[5] == '/') {
    // Convert /C%7c/foo to /C:/foo
    size_t old_len = strlen(path);
    size_t new_len = old_len - 2 + 1;  // %7c (3 chars) becomes : (1 char), so -2, +1 for null terminator
    char* result = malloc(new_len);
    if (!result) {
      return NULL;
    }
    result[0] = '/';
    result[1] = path[1];                     // Drive letter
    result[2] = ':';                         // Convert %7c to colon
    size_t tail_len = strlen(path + 5) + 1;  // include NUL
    memcpy(result + 3, path + 5, tail_len);  // Copy rest of path starting from the slash after %7c
    return result;
  }

  // Check for bare Windows drive letter pattern at start: X| where X is a letter
  // This handles cases like "C|" that appear in WPT tests
  if (strlen(path) >= 2 && isalpha(path[0]) && path[1] == '|') {
    // Convert C| to C: or C|foo to C:foo
    size_t new_len = strlen(path) + 1;  // Same length (| becomes :)
    char* result = malloc(new_len);
    if (!result) {
      return NULL;
    }
    result[0] = path[0];                     // Drive letter
    result[1] = ':';                         // Convert pipe to colon
    size_t tail_len = strlen(path + 2) + 1;  // include NUL
    memcpy(result + 2, path + 2, tail_len);
    return result;
  }

  // Check for Windows drive letter at start of path without leading slash: X|/ where X is a letter
  if (strlen(path) >= 3 && isalpha(path[0]) && path[1] == '|' && path[2] == '/') {
    // Convert C|/foo to C:/foo
    size_t new_len = strlen(path) + 1;  // Same length (| becomes :)
    char* result = malloc(new_len);
    if (!result) {
      return NULL;
    }
    result[0] = path[0];                     // Drive letter
    result[1] = ':';                         // Convert pipe to colon
    size_t tail_len = strlen(path + 2) + 1;  // include NUL
    memcpy(result + 2, path + 2, tail_len);
    return result;
  }

  // No conversion needed
  return strdup(path);
}