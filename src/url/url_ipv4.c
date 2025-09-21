#include <ctype.h>
#include "url.h"

// Convert full-width characters to half-width for IPv4 parsing per WPT
char* normalize_fullwidth_characters(const char* input) {
  if (!input)
    return NULL;

  size_t len = strlen(input);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)input[i];

    // Full-width digits (０-９) -> half-width (0-9)
    if (c == 0xEF && i + 2 < len && (unsigned char)input[i + 1] == 0xBC) {
      unsigned char third = (unsigned char)input[i + 2];
      if (third >= 0x90 && third <= 0x99) {
        // ０-９ (0xEFBC90-0xEFBC99) -> 0-9
        result[j++] = '0' + (third - 0x90);
        i += 2;  // Skip next 2 bytes
        continue;
      }
    }

    // Full-width uppercase letters A-F and X
    if (c == 0xEF && i + 2 < len && (unsigned char)input[i + 1] == 0xBC) {
      unsigned char third = (unsigned char)input[i + 2];
      // A-F (Ａ-Ｆ) -> A-F
      if (third >= 0x81 && third <= 0x86) {
        result[j++] = 'A' + (third - 0x81);
        i += 2;
        continue;
      }
      // X (Ｘ) -> X
      else if (third == 0xB8) {
        result[j++] = 'X';
        i += 2;
        continue;
      }
    }

    // Full-width lowercase letters a-f and x
    if (c == 0xEF && i + 2 < len && (unsigned char)input[i + 1] == 0xBD) {
      unsigned char third = (unsigned char)input[i + 2];
      // a-f (ａ-ｆ) -> a-f
      if (third >= 0x81 && third <= 0x86) {
        result[j++] = 'a' + (third - 0x81);
        i += 2;
        continue;
      }
      // x (ｘ) -> x
      else if (third == 0x98) {
        result[j++] = 'x';
        i += 2;
        continue;
      }
    }

    // Full-width dot (．) -> half-width (.)
    if (c == 0xEF && i + 2 < len && (unsigned char)input[i + 1] == 0xBC && (unsigned char)input[i + 2] == 0x8E) {
      result[j++] = '.';
      i += 2;
      continue;
    }

    // Keep ASCII as-is
    result[j++] = c;
  }

  result[j] = '\0';
  return result;
}

// Check if a hostname looks like an IPv4 address
// Returns 1 if it looks like IPv4 (and should be validated as such), 0 otherwise
int looks_like_ipv4_address(const char* hostname) {
  if (!hostname || strlen(hostname) == 0) {
    return 0;
  }

  JSRT_Debug("looks_like_ipv4_address: checking '%s'", hostname);

  // First normalize full-width characters like canonicalize_ipv4_address does
  char* normalized = normalize_fullwidth_characters(hostname);
  if (!normalized) {
    return 0;
  }

  // According to WHATWG URL spec, a hostname should be processed as IPv4 if:
  // 1. It ends with a numeric segment (like "foo.123", "example.0x10")
  // 2. It consists entirely of digits (pure numeric hostname like "123456")
  // 3. It has hex prefix format (0x...)
  // 4. It's dotted notation where the last part looks numeric

  int has_dots = strchr(normalized, '.') != NULL;
  int has_hex_prefix = strncmp(normalized, "0x", 2) == 0 || strncmp(normalized, "0X", 2) == 0;

  // Special cases that should NOT be treated as IPv4 addresses per WPT:
  // - Single dot: "."
  // - Double dot: ".."
  // These are valid hostnames, not IPv4 addresses
  if (strcmp(normalized, ".") == 0 || strcmp(normalized, "..") == 0) {
    free(normalized);
    return 0;
  }

  // Case 1: Pure numeric (no dots, no hex prefix)
  if (!has_dots && !has_hex_prefix) {
    // Check if all characters are digits
    for (size_t i = 0; i < strlen(normalized); i++) {
      if (normalized[i] < '0' || normalized[i] > '9') {
        free(normalized);
        return 0;  // Not all digits, not IPv4
      }
    }
    free(normalized);
    return 1;  // All digits, looks like IPv4
  }

  // Case 2: Hex prefix format (0x...)
  if (has_hex_prefix && !has_dots) {
    // Check if all characters after 0x are valid hex
    for (size_t i = 2; i < strlen(normalized); i++) {
      char c = normalized[i];
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
        free(normalized);
        return 0;  // Invalid hex character
      }
    }
    free(normalized);
    return 1;  // Valid hex format, looks like IPv4
  }

  // Case 3: Check if hostname should be treated as IPv4 (has numeric-looking segments)
  // Per WHATWG URL spec, hostnames should be processed as IPv4 if they contain
  // segments that look numeric (including hex like 0x4), even if other segments are not
  // This allows proper validation and rejection of malformed IPv4 patterns like "foo.0x4"
  if (has_dots) {
    // First check for trailing dot case (like "1.2.3.4.")
    size_t len = strlen(normalized);
    char* to_check = normalized;
    int has_trailing_dot = 0;

    if (len > 0 && normalized[len - 1] == '.') {
      // Has trailing dot - remove it for checking
      has_trailing_dot = 1;
      to_check = malloc(len);
      if (!to_check) {
        free(normalized);
        return 0;
      }
      strncpy(to_check, normalized, len - 1);
      to_check[len - 1] = '\0';

      // If empty after removing dot, not IPv4
      if (strlen(to_check) == 0) {
        free(to_check);
        free(normalized);
        return 0;
      }
    }

    // Check if hostname should be treated as IPv4 (has numeric-looking segments)
    // Per WHATWG URL spec, if the last segment looks numeric, treat as IPv4 attempt
    // This allows proper validation and rejection of patterns like "foo.0x4"
    int result = 0;
    char* parts_copy = strdup(to_check);
    if (parts_copy) {
      char* token = strtok(parts_copy, ".");
      int has_numeric_segment = 0;
      int part_count = 0;
      char* last_token = NULL;

      while (token) {
        part_count++;

        // If we have more than 4 parts, this is not IPv4
        if (part_count > 4) {
          break;
        }

        // Empty parts (consecutive dots) are invalid
        if (strlen(token) == 0) {
          break;
        }

        // Keep track of the last token
        if (last_token) {
          free(last_token);
        }
        last_token = strdup(token);

        // Check if this token is numeric (decimal, octal, or hex)
        int token_numeric = 0;

        // Check for hex format (0x...)
        if (strlen(token) >= 2 && (strncmp(token, "0x", 2) == 0 || strncmp(token, "0X", 2) == 0)) {
          // Hex format - check if valid hex digits after 0x (or just "0x" which is valid)
          if (strlen(token) == 2) {
            // Just "0x" is valid
            token_numeric = 1;
          } else {
            token_numeric = 1;  // Assume valid, then check
            for (size_t i = 2; i < strlen(token); i++) {
              char c = token[i];
              if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                token_numeric = 0;
                break;
              }
            }
          }
        } else {
          // Decimal/octal format - check if all characters are digits
          token_numeric = 1;  // Assume valid, then check
          for (size_t i = 0; i < strlen(token); i++) {
            if (token[i] < '0' || token[i] > '9') {
              token_numeric = 0;
              break;
            }
          }
        }

        if (token_numeric) {
          has_numeric_segment = 1;
        }

        token = strtok(NULL, ".");
      }

      // Per WHATWG URL spec: treat as IPv4 if last segment looks numeric
      int last_segment_numeric = 0;
      if (last_token) {
        // Check if last token is numeric (hex or decimal)
        if (strlen(last_token) >= 2 && (strncmp(last_token, "0x", 2) == 0 || strncmp(last_token, "0X", 2) == 0)) {
          // Hex format
          last_segment_numeric = 1;
          if (strlen(last_token) > 2) {
            for (size_t i = 2; i < strlen(last_token); i++) {
              char c = last_token[i];
              if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                last_segment_numeric = 0;
                break;
              }
            }
          }
        } else {
          // Decimal format
          last_segment_numeric = 1;
          for (size_t i = 0; i < strlen(last_token); i++) {
            if (last_token[i] < '0' || last_token[i] > '9') {
              last_segment_numeric = 0;
              break;
            }
          }
        }
        free(last_token);
      }

      // Treat as IPv4 if last segment looks numeric
      result = last_segment_numeric && part_count >= 1 && part_count <= 4;
      JSRT_Debug("looks_like_ipv4_address: dotted format - last_segment_numeric=%d, part_count=%d, result=%d",
                 last_segment_numeric, part_count, result);
      free(parts_copy);
    }

    if (has_trailing_dot) {
      free(to_check);
    }

    if (result) {
      free(normalized);
      return 1;  // Valid IPv4 format
    }
  }

  // If none of the above cases match, it's not IPv4
  free(normalized);
  return 0;
}

// Canonicalize IPv4 address according to WHATWG URL spec
// Handles decimal, octal, and hexadecimal formats
// Returns NULL if not a valid IPv4 address, otherwise returns canonical dotted decimal
char* canonicalize_ipv4_address(const char* input) {
  if (!input || strlen(input) == 0) {
    return NULL;
  }

  // First check if this even looks like an IPv4 address
  if (!looks_like_ipv4_address(input)) {
    return NULL;
  }

  // First normalize full-width characters to half-width for WPT compliance
  char* normalized_input = normalize_fullwidth_characters(input);
  if (!normalized_input) {
    return NULL;
  }

  // Check if this looks like an IPv4 address (contains dots or hex notation)
  int has_dots = strchr(normalized_input, '.') != NULL;
  int has_hex = strstr(normalized_input, "0x") != NULL || strstr(normalized_input, "0X") != NULL;

  // WPT tests expect hex notation to be supported (e.g., http://192.0x00A80001 -> http://192.168.0.1/)
  // So we should not reject hex notation outright

  if (!has_dots) {
    // Try to parse as a single 32-bit number (decimal, octal, or hex)
    char* endptr;
    errno = 0;  // Reset errno to detect overflow
    unsigned long long addr =
        strtoull(normalized_input, &endptr, 0);  // Auto-detect base (supports hex 0x, octal 0, decimal)

    // Check for parsing errors or overflow
    if (errno == ERANGE) {
      // strtoull detected overflow - number is too large
      free(normalized_input);
      return NULL;
    }

    // If the entire string was consumed and it's a valid 32-bit value
    // Check for overflow: IPv4 addresses must fit in 32 bits
    if (*endptr == '\0' && addr <= 0xFFFFFFFFULL) {
      char* result = malloc(16);  // "255.255.255.255\0"
      snprintf(result, 16, "%u.%u.%u.%u", (unsigned int)((addr >> 24) & 0xFF), (unsigned int)((addr >> 16) & 0xFF),
               (unsigned int)((addr >> 8) & 0xFF), (unsigned int)(addr & 0xFF));
      free(normalized_input);
      return result;
    }

    // Check for specific invalid patterns that should be rejected
    // Numbers that are too large (overflow 32-bit) should fail
    if (*endptr == '\0' && addr > 0xFFFFFFFFULL) {
      free(normalized_input);
      return NULL;  // Overflow - invalid IPv4
    }

    // If there are remaining characters after parsing, it's invalid
    if (*endptr != '\0') {
      free(normalized_input);
      return NULL;  // Extra characters make it invalid
    }

    free(normalized_input);
    return NULL;
  }

  if (has_dots) {
    // Check for consecutive dots which make IPv4 addresses invalid
    if (strstr(normalized_input, "..")) {
      free(normalized_input);
      return NULL;  // Invalid IPv4 - consecutive dots
    }

    // Handle trailing dot per WHATWG URL spec: remove it and continue parsing
    size_t len = strlen(normalized_input);
    if (len > 0 && normalized_input[len - 1] == '.') {
      // Remove the trailing dot and continue parsing
      normalized_input[len - 1] = '\0';
      len--;
      // If removing the dot makes it empty, that's invalid
      if (len == 0) {
        free(normalized_input);
        return NULL;
      }
    }

    // Parse dotted notation (may include hex/octal parts) using normalized input
    char* input_copy = strdup(normalized_input);
    char* parts[5];  // Allow up to 5 to detect invalid IPv4 with too many parts
    int part_count = 0;

    char* token = strtok(input_copy, ".");
    while (token && part_count <= 4) {  // Allow reading one extra to detect invalid
      parts[part_count++] = token;
      token = strtok(NULL, ".");
    }

    // Must have exactly 1-4 parts for valid IPv4
    if (part_count == 0 || part_count > 4) {
      free(input_copy);
      free(normalized_input);
      return NULL;
    }

    unsigned long values[4] = {0};

    // Parse each part (supporting decimal, octal, hex)
    for (int i = 0; i < part_count; i++) {
      // Check for empty parts (consecutive dots like "0..0x300")
      if (strlen(parts[i]) == 0) {
        free(input_copy);
        free(normalized_input);
        return NULL;  // Invalid IPv4 - empty part
      }

      char* endptr;
      errno = 0;                                  // Reset errno to detect overflow
      values[i] = strtoul(parts[i], &endptr, 0);  // Auto-detect base

      if (errno == ERANGE) {
        // Overflow occurred
        free(input_copy);
        free(normalized_input);
        return NULL;  // Invalid number or overflow
      }

      // Strict IPv4 parsing per WHATWG URL spec:
      // If any part contains non-numeric characters (except valid hex/octal prefixes),
      // the entire hostname should be rejected as invalid IPv4
      if (endptr == parts[i]) {
        // No digits were parsed at all - this is invalid IPv4
        // Examples: "foo", "bar", etc. should cause IPv4 parsing to fail
        free(input_copy);
        free(normalized_input);
        return NULL;
      } else if (*endptr != '\0') {
        // Some characters remain after parsing - check for special cases
        if (strcmp(parts[i], "0x") == 0 || strcmp(parts[i], "0X") == 0) {
          // Special case: "0x" or "0X" without valid hex digits should be treated as 0
          values[i] = 0;
        } else {
          // Other cases with invalid characters - this is invalid IPv4
          // Examples: "foo.0x4" should fail because "foo" contains invalid characters
          free(input_copy);
          free(normalized_input);
          return NULL;
        }
      }
    }

    // Validate ranges based on number of parts
    if (part_count == 4) {
      // All parts must be 0-255
      for (int i = 0; i < 4; i++) {
        if (values[i] > 255) {
          free(input_copy);
          free(normalized_input);
          return NULL;
        }
      }
    } else if (part_count == 3) {
      // First two parts 0-255, last part 0-65535
      if (values[0] > 255 || values[1] > 255 || values[2] > 65535) {
        free(input_copy);
        free(normalized_input);
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
        free(normalized_input);
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
        free(normalized_input);
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
    free(normalized_input);
    return result;
  }

  free(normalized_input);
  return NULL;
}