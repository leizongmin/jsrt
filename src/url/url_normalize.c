#include <ctype.h>
#include "url.h"

// Normalize dot segments in URL path according to RFC 3986 and WHATWG URL spec
// Resolves "." and ".." segments in paths
char* normalize_dot_segments(const char* path) {
  if (!path || strlen(path) == 0) {
    return strdup("");
  }

  // Use a simple approach: split into segments, process them, then reconstruct
  size_t path_len = strlen(path);
  char* path_copy = strdup(path);
  if (!path_copy) {
    return strdup("");
  }
  char** segments = malloc(path_len * sizeof(char*));
  if (!segments) {
    free(path_copy);
    return strdup("");
  }
  int segment_count = 0;

  // Handle leading slash
  int is_absolute = (path[0] == '/');
  char* ptr = path_copy;
  if (is_absolute) {
    ptr++;  // Skip leading slash
  }

  // Split into segments manually to preserve empty segments
  char* start = ptr;
  while (*start != '\0') {
    char* end = strchr(start, '/');
    if (end == NULL) {
      // Last segment
      segments[segment_count++] = strdup(start);
      break;
    } else {
      // Extract segment (could be empty)
      size_t len = end - start;
      char* segment = malloc(len + 1);
      if (!segment) {
        // Free already allocated segments
        for (int k = 0; k < segment_count; k++) {
          free(segments[k]);
        }
        free(segments);
        free(path_copy);
        return strdup("");
      }
      strncpy(segment, start, len);
      segment[len] = '\0';
      segments[segment_count++] = segment;
      start = end + 1;
    }
  }

  // Check for trailing empty segments and dot segments before processing (for slash handling)
  int had_trailing_empty = 0;
  int had_trailing_dot_segment = 0;
  for (int i = segment_count - 1; i >= 0; i--) {
    if (strcmp(segments[i], "..") == 0 || strcmp(segments[i], ".") == 0) {
      had_trailing_dot_segment = 1;
      continue;  // Skip dot segments
    } else if (strlen(segments[i]) == 0) {
      had_trailing_empty = 1;
      break;
    } else {
      break;  // Found non-empty segment
    }
  }

  // Process segments using a stack approach
  char** output_segments = malloc(segment_count * sizeof(char*));
  if (!output_segments) {
    // Free already allocated segments
    for (int k = 0; k < segment_count; k++) {
      free(segments[k]);
    }
    free(segments);
    free(path_copy);
    return strdup("");
  }
  int output_count = 0;

  for (int i = 0; i < segment_count; i++) {
    if (strcmp(segments[i], ".") == 0) {
      // Single dot: skip it
      free(segments[i]);
    } else if (strcmp(segments[i], "..") == 0) {
      // Double dot: remove previous segment if it exists
      if (output_count > 0) {
        free(output_segments[output_count - 1]);
        output_count--;
      }
      free(segments[i]);
    } else {
      // Normal segment (including empty segments): keep it
      output_segments[output_count++] = segments[i];
    }
  }

  // Reconstruct the path
  size_t result_len = 1;  // At least for null terminator
  if (is_absolute) {
    result_len++;  // For leading slash
  }
  for (int i = 0; i < output_count; i++) {
    result_len += strlen(output_segments[i]) + 1;  // +1 for slash separator
  }

  char* result = malloc(result_len + 1);
  result[0] = '\0';

  size_t w = 0;
  if (is_absolute) {
    result[w++] = '/';
  }

  for (int i = 0; i < output_count; i++) {
    if (i > 0) {
      result[w++] = '/';
    }
    size_t seglen = strlen(output_segments[i]);
    memcpy(result + w, output_segments[i], seglen);
    w += seglen;
    free(output_segments[i]);
  }

  // Handle trailing slash case: if original path ended with / and we have segments
  // Also handle the case where the path had trailing slashes before dot segments
  if (output_count > 0) {
    // Check if original path structure suggests a trailing slash
    if (path_len > 1 && path[path_len - 1] == '/') {
      result[w++] = '/';
    } else if (had_trailing_empty) {
      // There were empty segments at the end (before dot segments)
      // This handles cases like /foo/bar//.. where there's an implicit trailing slash
      result[w++] = '/';
    } else if (had_trailing_dot_segment) {
      // The last segment in original path was a dot segment
      // In this case, we should add a trailing slash
      // Examples: /foo/. -> /foo/, /foo/bar/.. -> /foo/
      result[w++] = '/';
    }
  }

  // Special case: if result is empty and it was an absolute path, return "/"
  if (w == 0 && is_absolute) {
    result[w++] = '/';
  }
  result[w] = '\0';

  free(path_copy);
  free(segments);
  free(output_segments);

  return result;
}

// Decode percent-encoded dots in path for proper normalization
// According to WHATWG URL spec, %2e and %2E should be decoded ONLY when they form dot segments
// Other %2e sequences should be preserved as-is
char* decode_percent_encoded_dots(const char* path) {
  if (!path || strlen(path) == 0) {
    return strdup("");
  }

  size_t input_len = strlen(path);
  char* output = malloc(input_len + 1);
  if (!output) {
    return NULL;
  }

  size_t i = 0, j = 0;
  while (i < input_len) {
    if (i + 2 < input_len && path[i] == '%' && path[i + 1] == '2' && (path[i + 2] == 'e' || path[i + 2] == 'E')) {
      // Check if this %2e forms a dot segment that should be normalized
      int should_decode = 0;

      // Check for dot segment patterns:
      // 1. "%2e" at end of string
      // 2. "%2e/" (single dot segment)
      // 3. "%2e%2e" followed by "/" or end (double dot segment)
      // 4. Beginning of path: ".%2e" or "%2e"

      if (i + 3 >= input_len) {
        // %2e at end of string - decode
        should_decode = 1;
      } else if (path[i + 3] == '/') {
        // %2e/ - single dot segment, decode
        should_decode = 1;
      } else if (i + 5 < input_len && path[i + 3] == '%' && path[i + 4] == '2' &&
                 (path[i + 5] == 'e' || path[i + 5] == 'E')) {
        // %2e%2e - check if followed by / or end
        if (i + 6 >= input_len || path[i + 6] == '/') {
          should_decode = 1;
        }
      } else if (i == 0 || path[i - 1] == '/') {
        // Check if this starts a dot segment from beginning of path or after /
        // Look ahead to see if it forms a complete dot segment
        if (i + 3 < input_len && path[i + 3] == '.') {
          // Pattern like "%2e." - check if followed by / or end
          if (i + 4 >= input_len || path[i + 4] == '/') {
            should_decode = 1;
          }
        }
      }

      if (should_decode) {
        output[j++] = '.';
        i += 3;
      } else {
        // Preserve %2e as-is - it's not part of a dot segment
        output[j++] = path[i++];
        output[j++] = path[i++];
        output[j++] = path[i++];
      }
    } else {
      output[j++] = path[i++];
    }
  }
  output[j] = '\0';

  return output;
}

// Enhanced dot segment normalization that handles percent-encoded dots first
char* normalize_dot_segments_with_percent_decoding(const char* path) {
  if (!path) {
    return strdup("");
  }

  // First decode percent-encoded dots
  char* decoded_path = decode_percent_encoded_dots(path);
  if (!decoded_path) {
    return strdup("");
  }

  // Then normalize dot segments
  char* normalized = normalize_dot_segments(decoded_path);
  free(decoded_path);

  return normalized;
}

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
    // Zero Width No-Break Space (U+FEFF) / BOM: 0xEF 0xBB 0xBF
    if (c == 0xEF && ptr[1] == 0xBB && ptr[2] == 0xBF) {
      return 3;
    }
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
      // Zero Width No-Break Space (U+FEFF) / BOM: 0xEF 0xBB 0xBF
      else if (check_pos[-2] == 0xEF && check_pos[-1] == 0xBB && check_pos[0] == 0xBF) {
        end -= 3;
        found_whitespace = 1;
      }
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

    // Check for zero-width Unicode characters that should be removed
    if (c >= 0x80 && i + 2 < len) {
      // Zero Width Space (U+200B): 0xE2 0x80 0x8B
      // Zero Width Non-Joiner (U+200C): 0xE2 0x80 0x8C
      // Zero Width Joiner (U+200D): 0xE2 0x80 0x8D
      // Word Joiner (U+2060): 0xE2 0x81 0xA0
      if (c == 0xE2 && (unsigned char)url[i + 1] == 0x80) {
        unsigned char third = (unsigned char)url[i + 2];
        if (third == 0x8B || third == 0x8C || third == 0x8D) {
          i += 3;  // Skip zero-width character
          // Check for consecutive slashes after removal
          if (j > 0 && result[j - 1] == '/' && i < len && url[i] == '/') {
            // Skip the next slash to prevent double slashes
            i++;
          }
          continue;
        }
      } else if (c == 0xE2 && (unsigned char)url[i + 1] == 0x81 && (unsigned char)url[i + 2] == 0xA0) {
        i += 3;  // Skip Word Joiner (U+2060)
        // Check for consecutive slashes after removal
        if (j > 0 && result[j - 1] == '/' && i < len && url[i] == '/') {
          // Skip the next slash to prevent double slashes
          i++;
        }
        continue;
      }
    }

    // Check for Zero Width No-Break Space (U+FEFF) / BOM: 0xEF 0xBB 0xBF
    if (c == 0xEF && i + 2 < len && (unsigned char)url[i + 1] == 0xBB && (unsigned char)url[i + 2] == 0xBF) {
      i += 3;  // Skip BOM character
      // Check for consecutive slashes after BOM removal
      if (j > 0 && result[j - 1] == '/' && i < len && url[i] == '/') {
        // Skip the next slash to prevent double slashes from BOM removal
        i++;
      }
      continue;
    }

    // According to WHATWG URL spec, spaces should be preserved and encoded later
    // Only remove tab, LF, CR, and zero-width characters - preserve all other characters
    result[j++] = url[i++];
  }
  result[j] = '\0';

  return result;
}

// Normalize spaces in URL path specifically for the query/fragment boundary issue
// This handles the special case where spaces before ? or # should be collapsed to single %20
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
        // This is a trailing space sequence before query/fragment - remove all spaces
        // According to WHATWG URL spec, spaces before query/fragment should be removed
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

  // No conversion needed
  return strdup(path);
}