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

  // Check if URL has a scheme - if not, still normalize backslashes for relative URLs
  // because they might be used with special scheme bases
  char* colon_pos = strchr(url, ':');
  if (!colon_pos || colon_pos == url) {
    // No scheme or invalid scheme position - still normalize backslashes for relative URLs
    // This ensures ///\//\//test becomes //////test, then normalizes to //test
    colon_pos = NULL;  // Mark as no scheme for later logic
  }

  size_t len = strlen(url);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  // Check if this is a non-special scheme
  int is_non_special = 0;

  if (colon_pos && colon_pos > url) {
    size_t scheme_len = colon_pos - url;
    char* scheme = malloc(scheme_len + 1);
    strncpy(scheme, url, scheme_len);
    scheme[scheme_len] = '\0';

    if (is_valid_scheme(scheme) && !is_special_scheme(scheme)) {
      is_non_special = 1;
    }
    free(scheme);
  }
  // Note: For relative URLs (colon_pos == NULL), is_non_special remains 0,
  // so they will be treated as special schemes for backslash normalization

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

  size_t result_pos = 0;
  for (size_t i = 0; i < len; i++) {
    if (i < stop_pos && url[i] == '\\') {
      // For non-special schemes, backslashes should NOT be normalized to forward slashes
      // According to WHATWG URL spec, non-special schemes preserve backslashes as-is
      if (is_non_special && colon_pos && i > (colon_pos - url)) {
        // For non-special schemes, preserve backslashes in the path
        result[result_pos++] = '\\';
      } else {
        // For all special schemes including file:, convert backslash to forward slash
        result[result_pos++] = '/';

        // Only skip additional consecutive backslashes, not forward slashes
        // This preserves significant forward slashes in URLs like file:///
        while (i + 1 < len && url[i + 1] == '\\') {
          i++;
        }
      }
    } else if (i < stop_pos && url[i] == '/') {
      // Handle slash normalization
      if (result_pos > 0 && result[result_pos - 1] == '/') {
        // Consecutive slashes found
        if (is_non_special && colon_pos && i > (colon_pos - url)) {
          // For non-special schemes, preserve consecutive slashes in paths
          result[result_pos++] = '/';
        } else {
          // For special schemes, normalize consecutive slashes in paths but preserve authority "//"
          // Only skip slashes if we're NOT in the authority section (scheme://)
          // Authority section should always have exactly "//" after the scheme

          // Check if we're at the beginning of the URL and this might be scheme-relative
          if (!colon_pos && result_pos <= 2) {
            // No scheme and we're at the beginning - preserve leading "//" for scheme-relative URLs
            // But for cases like ///test, only preserve the first two slashes (scheme-relative)
            if (result_pos < 2) {
              result[result_pos++] = '/';
            } else {
              // This is the third or later slash at the beginning - skip it for scheme-relative normalization
              continue;
            }
          } else if (colon_pos) {
            // We have a scheme - check if we're building the authority "//" part
            size_t colon_offset = colon_pos - url;
            size_t chars_after_colon = result_pos - colon_offset - 1;  // -1 for the colon itself

            // Check if this is a file URL which might need 3 slashes
            size_t scheme_len = colon_offset;
            int is_file_scheme = (scheme_len == 4 && strncmp(url, "file", 4) == 0);

            if (chars_after_colon < 2) {
              // We're building the "//" authority part - always preserve these first 2 slashes
              result[result_pos++] = '/';
            } else if (is_file_scheme && chars_after_colon == 2) {
              // Special case: file URLs can have a third slash for local files
              result[result_pos++] = '/';
            } else {
              // We're in a path or other section - normalize consecutive slashes
              // Skip this extra slash (for cases like "path//file" -> "path/file")
              continue;
            }
          } else {
            // We're in a path or other section
            if (is_non_special) {
              // Non-special schemes preserve consecutive slashes in paths
              result[result_pos++] = '/';
            } else {
              // For relative URLs (no scheme), preserve consecutive slashes
              // because we don't know the final scheme yet
              if (!colon_pos) {
                result[result_pos++] = '/';
              } else {
                // Special schemes normalize consecutive slashes in paths
                // Skip this extra slash (for cases like "path//file" -> "path/file")
                continue;
              }
            }
          }
        }
      } else {
        // Not a consecutive slash, add it normally
        result[result_pos++] = '/';
      }
    } else if (i < stop_pos && url[i] == '|') {
      // Handle pipe character normalization for file URLs
      // Per WHATWG URL spec: pipe characters should be normalized to colons in file URLs
      // Apply to both absolute file URLs and relative URLs (which might be used with file bases)
      if ((colon_pos && strncmp(url, "file:", 5) == 0) || (!colon_pos)) {
        // Check if this looks like a Windows drive letter pattern (letter followed by |)
        // Valid patterns: C|, /C|, :C| (for file:C| URLs)
        if (i > 0 && isalpha(url[i - 1]) && (i == 1 || url[i - 2] == '/' || url[i - 2] == ':')) {
          // For file URLs or relative URLs, convert pipe to colon (Windows drive letter normalization)
          result[result_pos++] = ':';
          // For Windows drive letters, ensure there's a forward slash after the colon
          // if the next character isn't already a slash and we're not at the end
          if (i + 1 < len && url[i + 1] != '/' && url[i + 1] != '\\') {
            result[result_pos++] = '/';
          }
        } else {
          // Not a drive letter pattern, preserve pipe character
          result[result_pos++] = url[i];
        }
      } else {
        // For non-file URLs, preserve pipe character
        result[result_pos++] = url[i];
      }
    } else {
      // Preserve other characters
      result[result_pos++] = url[i];
    }
  }
  result[result_pos] = '\0';
  return result;
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

  char* origin;
  if (is_default_port(scheme, port) || !port || strlen(port) == 0 || double_colon_at_pattern) {
    // Omit default port or when double colon @ pattern detected (http::@host:port)
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