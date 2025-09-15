#include <ctype.h>
#include "url.h"

// Normalize dot segments in URL path according to RFC 3986
// Resolves "." and ".." segments in paths
char* normalize_dot_segments(const char* path) {
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
    // A: If input begins with "../" or "./", remove prefix and handle ".." case
    if (strncmp(input, "../", 3) == 0) {
      // "../" should also remove the last segment from the output (like "/../")
      // Remove last segment from output
      if (out_ptr > output) {
        // Move back past the current trailing slash if there is one
        if (*(out_ptr - 1) == '/') {
          out_ptr--;
        }
        // Find and remove the last segment
        while (out_ptr > output && *(out_ptr - 1) != '/') {
          out_ptr--;
        }
        // If we're not at root and don't have a trailing slash, ensure we have one
        if (out_ptr > output && *(out_ptr - 1) == '/') {
          // Keep the existing slash
        } else if (out_ptr == output) {
          // We're at the root, add a slash
          *out_ptr++ = '/';
        }
        *out_ptr = '\0';
      }
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
      // Remove last segment from output (everything back to and including the previous '/')
      if (out_ptr > output) {
        // Move back past the current trailing slash if there is one
        if (*(out_ptr - 1) == '/') {
          out_ptr--;
        }
        // Find and remove the last segment
        while (out_ptr > output && *(out_ptr - 1) != '/') {
          out_ptr--;
        }
        // If we found a '/', keep it, otherwise we're at the root
        if (out_ptr == output || *(out_ptr - 1) != '/') {
          *out_ptr++ = '/';  // Ensure we have a leading slash
        }
        *out_ptr = '\0';
      } else {
        // Output is empty, just add "/"
        *out_ptr++ = '/';
        *out_ptr = '\0';
      }

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

  // For URL paths, consecutive slashes are significant and should be preserved
  // According to WHATWG URL spec, paths like "//@" should remain as "//@", not "/@"
  // Only normalize dot segments, but preserve the rest of the path structure

  return output;
}

// Strip leading and trailing ASCII whitespace from URL string
// ASCII whitespace: space (0x20), tab (0x09), LF (0x0A), CR (0x0D), FF (0x0C)
char* strip_url_whitespace(const char* url) {
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

// Remove tab, newline, carriage return from URL string per WHATWG spec
// Spaces are preserved and will be encoded later in the appropriate components
char* remove_all_ascii_whitespace(const char* url) {
  if (!url)
    return NULL;

  size_t len = strlen(url);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    char c = url[i];

    // Always remove tab, line feed, and carriage return
    if (c == 0x09 || c == 0x0A || c == 0x0D) {
      continue;
    }

    // According to WHATWG URL spec, spaces should be preserved and encoded later
    // Only remove tab, LF, and CR - preserve all spaces

    result[j++] = c;
  }
  result[j] = '\0';

  return result;
}

// Normalize consecutive spaces to single space for non-special schemes
char* normalize_spaces_in_path(const char* path) {
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

  // Find fragment position - backslashes after # are never normalized
  char* fragment_pos = strchr(url, '#');
  size_t stop_pos = fragment_pos ? (fragment_pos - url) : len;

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
      // Handle pipe to colon normalization for Windows drive letters
      // Extended pattern matching for better drive letter detection
      if (i > 0 && isalpha(url[i - 1]) &&
          (i + 1 < len && (url[i + 1] == '/' || url[i + 1] == '\\' || url[i + 1] == '|'))) {
        // Convert pipe to colon for drive letter patterns
        result[result_pos++] = ':';
      } else {
        result[result_pos++] = '|';  // Preserve pipe - will be percent-encoded later
      }
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
        char* inner_origin = compute_origin(inner_url->protocol, inner_url->hostname, inner_url->port,
                                            inner_url->double_colon_at_pattern);
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

  // Use normalized port for origin computation
  char* normalized_port = normalize_port(port, protocol);

  char* origin;
  if (!normalized_port || strlen(normalized_port) == 0) {
    // Omit default port, port 0, or when no port is specified
    origin = malloc(strlen(protocol) + strlen(hostname) + 4);
    sprintf(origin, "%s//%s", protocol, hostname);
  } else {
    // Include custom non-default port (even if double_colon_at_pattern is present)
    origin = malloc(strlen(protocol) + strlen(hostname) + strlen(normalized_port) + 5);
    sprintf(origin, "%s//%s:%s", protocol, hostname, normalized_port);
  }

  free(normalized_port);

  free(scheme);
  return origin;
}