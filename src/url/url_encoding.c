#include <ctype.h>
#include <stdio.h>
#include "url.h"

static const char hex_chars[] = "0123456789ABCDEF";

int hex_to_int(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}

// URL encode function for query parameters (space becomes +)
char* url_encode_with_len(const char* str, size_t len) {
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

// Backward compatibility wrapper
char* url_encode(const char* str) {
  return url_encode_with_len(str, strlen(str));
}

// URL component encoding for href generation (space -> %20, not +)
char* url_component_encode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // Check if character needs encoding per URL spec
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c <= 32 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`' ||
               c == '{' || c == '|' || c == '}') {
      // Encode control characters, space, and specific unsafe characters
      // Include single quotes and backticks per WPT requirements
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, copy as-is
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c <= 32 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`' ||
               c == '{' || c == '|' || c == '}') {
      // Encode control characters, space, and specific unsafe characters
      // Include single quotes and backticks per WPT requirements
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

// Special encoding for non-special scheme paths with trailing space rules
// Per WPT: only the last space in a trailing sequence before ?/# gets encoded
char* url_nonspecial_trailing_space_encode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length with special trailing space handling
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == ' ') {
      // Check if this is the last space in a trailing sequence
      size_t space_count = 0;
      while (i + space_count < len && str[i + space_count] == ' ') {
        space_count++;
      }

      // If we're at the end of string (before query/fragment), encode last space only
      if (i + space_count == len) {
        encoded_len += (space_count - 1) + 3;  // (n-1) literal spaces + one %20
      } else {
        encoded_len += space_count;  // Keep all spaces literal
      }
      i += space_count - 1;  // Skip counted spaces
    } else if (c <= 32 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`' ||
               c == '{' || c == '|' || c == '}') {
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
      // Handle trailing space sequences
      size_t space_count = 0;
      while (i + space_count < len && str[i + space_count] == ' ') {
        space_count++;
      }

      if (i + space_count == len && space_count > 1) {
        // Trailing sequence: keep (n-1) literal, encode last as %20
        for (size_t k = 0; k < space_count - 1; k++) {
          encoded[j++] = ' ';
        }
        encoded[j++] = '%';
        encoded[j++] = '2';
        encoded[j++] = '0';
      } else {
        // Non-trailing or single space: keep all literal
        for (size_t k = 0; k < space_count; k++) {
          encoded[j++] = ' ';
        }
      }
      i += space_count - 1;  // Skip counted spaces
    } else if (c <= 32 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`' ||
               c == '{' || c == '|' || c == '}') {
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

// Fragment-specific encoding (backticks should be encoded in fragments)
char* url_fragment_encode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // Check if character needs encoding per URL spec
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '|' ||
               c == '}' || c == '`') {
      // Need to percent-encode unsafe characters including backtick for fragments
      // Note: backslashes (\) are allowed in fragments per WPT tests
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, copy as-is
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '|' ||
               c == '}' || c == '`') {
      // Need to percent-encode unsafe characters including backtick for fragments
      // Note: backslashes (\) are allowed in fragments per WPT tests
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

// Special encoding for non-special scheme paths
// Per WPT tests, only certain spaces are encoded - trailing spaces become %20
char* url_nonspecial_path_encode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length - need special logic for trailing space handling
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c == ' ') {
      // Space handling: check if this is a trailing space that should be encoded
      // For non-special schemes, only encode the very last space in a trailing sequence
      size_t space_count = 0;
      size_t space_start = i;
      while (i + space_count < len && str[i + space_count] == ' ') {
        space_count++;
      }

      // If this is the end of string, encode only the last space
      if (i + space_count == len && space_count > 1) {
        encoded_len += (space_count - 1) + 3;  // (n-1) spaces + one %20
      } else {
        encoded_len += space_count;  // Keep all spaces as-is
      }
      i += space_count - 1;  // Skip counted spaces (loop will increment)
    } else if (c < 32 || c > 126) {
      // Encode other control characters and non-ASCII
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, copy as-is
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c == ' ') {
      // Space handling: check if this is a trailing space sequence
      size_t space_count = 0;
      size_t space_start = i;
      while (i + space_count < len && str[i + space_count] == ' ') {
        space_count++;
      }

      // If this is the end of string and we have multiple spaces,
      // encode only the last space as %20
      if (i + space_count == len && space_count > 1) {
        // Keep (n-1) spaces as-is, encode the last one
        for (size_t k = 0; k < space_count - 1; k++) {
          encoded[j++] = ' ';
        }
        encoded[j++] = '%';
        encoded[j++] = hex_chars[' ' >> 4];
        encoded[j++] = hex_chars[' ' & 15];
      } else {
        // Keep all spaces as-is for non-trailing sequences
        for (size_t k = 0; k < space_count; k++) {
          encoded[j++] = ' ';
        }
      }
      i += space_count - 1;  // Skip processed spaces
    } else if (c < 32 || c > 126) {
      // Encode other control characters and non-ASCII
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

// Userinfo encoding per WHATWG URL spec (less aggressive than url_encode_with_len)
// According to WPT tests, some characters like & and ( should not be percent-encoded in userinfo
// URL userinfo encoding with scheme awareness
char* url_userinfo_encode_with_scheme_name(const char* str, const char* scheme) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Determine encoding rules based on scheme
  int encode_closing_bracket = 1;  // Default: encode ']'
  int encode_at_symbol = 1;        // Default: encode '@'
  int encode_colon = 1;            // Default: encode ':'

  if (scheme) {
    // Remove trailing colon if present
    char* scheme_clean = strdup(scheme);
    if (strlen(scheme_clean) > 0 && scheme_clean[strlen(scheme_clean) - 1] == ':') {
      scheme_clean[strlen(scheme_clean) - 1] = '\0';
    }

    // WebSocket schemes and non-special schemes: don't encode ']', '@', and ':'
    if (strcmp(scheme_clean, "ws") == 0 || strcmp(scheme_clean, "wss") == 0 || !is_special_scheme(scheme)) {
      encode_closing_bracket = 0;
      encode_at_symbol = 0;
      encode_colon = 0;
    }

    free(scheme_clean);
  }

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*' || c == '&' || c == '(' || c == ')' || c == '!' || c == '$' || c == '\'' ||
        c == ',' || c == ';' || c == '=' || c == '+' || c == '%' || c == '<' || c == '>' || c == '[' || c == '^' ||
        c == '`' || c == '{' || c == '|' || c == '}' || (!encode_closing_bracket && c == ']') ||
        (!encode_at_symbol && c == '@') || (!encode_colon && c == ':')) {
      // Note: @, :, ] encoding depends on scheme
      encoded_len++;
    } else {
      encoded_len += 3;  // %XX
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*' || c == '&' || c == '(' || c == ')' || c == '!' || c == '$' || c == '\'' ||
        c == ',' || c == ';' || c == '=' || c == '+' || c == '%' || c == '<' || c == '>' || c == '[' || c == '^' ||
        c == '`' || c == '{' || c == '|' || c == '}' || (!encode_closing_bracket && c == ']') ||
        (!encode_at_symbol && c == '@') || (!encode_colon && c == ':')) {
      // Note: @, :, ] encoding depends on scheme
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

// Backward compatibility wrapper
char* url_userinfo_encode_with_scheme(const char* str, int is_special_scheme) {
  return url_userinfo_encode_with_scheme_name(str, is_special_scheme ? "http" : "foo");
}

// Original function for compatibility
char* url_userinfo_encode(const char* str) {
  // Default to non-special scheme behavior for backward compatibility
  return url_userinfo_encode_with_scheme(str, 0);
}

// URL decode function for query parameters (+ becomes space)
char* url_decode_query_with_length_and_output_len(const char* str, size_t len, size_t* output_len) {
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
char* url_decode_with_length_and_output_len(const char* str, size_t len, size_t* output_len) {
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

char* url_decode_with_length(const char* str, size_t len) {
  return url_decode_with_length_and_output_len(str, len, NULL);
}

char* url_decode(const char* str) {
  size_t len = strlen(str);
  return url_decode_with_length(str, len);
}

// Fragment encoding for non-special schemes (spaces are preserved)
char* url_fragment_encode_nonspecial(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length - preserve spaces for non-special schemes
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // Check if character needs encoding per URL spec
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '|' ||
               c == '}' || c == '`') {
      // Need to percent-encode unsafe characters including spaces
      // WPT tests expect spaces to be encoded even for non-special schemes
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, copy as-is
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '|' ||
               c == '}' || c == '`') {
      // Need to percent-encode unsafe characters including spaces
      // WPT tests expect spaces to be encoded even for non-special schemes
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

// Path encoding for special schemes (encodes non-ASCII characters including Unicode)
char* url_path_encode_special(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // Check if character needs encoding per URL spec
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // According to WHATWG URL spec, already percent-encoded characters should be preserved
      // Only decode when specifically required by the normalization process
      // For URL paths, preserve existing percent-encoding to maintain compliance
      // Keep as 3-character percent sequence
      encoded_len += 3;
      i += 2;  // Skip the hex digits
    } else if (c < 33 || c > 126 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '|' ||
               c == '}' || c == '`') {
      // Need to percent-encode control characters, non-ASCII, and specific unsafe characters
      // This includes Unicode characters (> 126) which should be percent-encoded in paths
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Percent-encoded sequence found, check if it should be decoded
      unsigned char decoded_byte = (unsigned char)((hex_to_int(str[i + 1]) << 4) | hex_to_int(str[i + 2]));

      // According to WHATWG URL spec, already percent-encoded characters should be preserved
      // Keep percent-encoded sequences as-is to maintain compliance
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c < 33 || c > 126 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '|' ||
               c == '}' || c == '`') {
      // Need to percent-encode control characters, non-ASCII, and specific unsafe characters
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
// URL path encoding specifically for file URLs (preserves pipe characters)
char* url_path_encode_file(const char* str) {
  if (!str)
    return NULL;

  // Check if input contains pipe character

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // Check if character needs encoding per URL spec
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Percent-encoded sequence found, check if it should be decoded
      unsigned char decoded_byte = (unsigned char)((hex_to_int(str[i + 1]) << 4) | hex_to_int(str[i + 2]));

      // Decode unreserved characters: ALPHA / DIGIT / "-" / "_" / "~"
      // NOTE: "." is NOT decoded because %2e (dot) should not be normalized in paths per WHATWG spec
      if ((decoded_byte >= 'A' && decoded_byte <= 'Z') || (decoded_byte >= 'a' && decoded_byte <= 'z') ||
          (decoded_byte >= '0' && decoded_byte <= '9') || decoded_byte == '-' || decoded_byte == '_' ||
          decoded_byte == '~') {
        // This will be decoded to 1 character
        encoded_len += 1;
        i += 2;  // Skip the hex digits
      } else {
        // Keep as 3-character percent sequence
        encoded_len += 3;
        i += 2;  // Skip the hex digits
      }
    } else if (c < 33 || c > 126 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '}' ||
               c == '`') {
      // Need to percent-encode control characters, non-ASCII, and specific unsafe characters
      // NOTE: For file URLs, pipe character (|) is preserved, not encoded
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Percent-encoded sequence found, check if it should be decoded
      unsigned char decoded_byte = (unsigned char)((hex_to_int(str[i + 1]) << 4) | hex_to_int(str[i + 2]));

      // According to WHATWG URL spec, already percent-encoded characters should be preserved
      // Keep percent-encoded sequences as-is to maintain compliance
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c < 33 || c > 126 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '}' ||
               c == '`') {
      // Need to percent-encode control characters, non-ASCII, and specific unsafe characters
      // NOTE: For file URLs, pipe character (|) is preserved, not encoded
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

// URL component encoding specifically for file URL paths
// Preserves pipe characters (|) which should not be encoded in file URLs
char* url_component_encode_file_path(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // Check if character needs encoding per URL spec
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c <= 32 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`' ||
               c == '{' || c == '}') {
      // Encode control characters, space, and specific unsafe characters
      // NOTE: For file URLs, pipe character (|) is preserved, not encoded
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = malloc(encoded_len + 1);
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, copy as-is
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c <= 32 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`' ||
               c == '{' || c == '}') {
      // Encode control characters, space, and specific unsafe characters
      // NOTE: For file URLs, pipe character (|) is preserved, not encoded
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
