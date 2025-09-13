#include <ctype.h>
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
    } else if (c < 32 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '|' ||
               c == '}') {
      // Only encode control characters and specific unsafe characters
      // Do NOT encode Unicode characters (>= 127) to preserve them in URLs per WPT
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
    } else if (c < 32 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '|' ||
               c == '}') {
      // Only encode control characters and specific unsafe characters
      // Preserve Unicode characters (>= 127) and single quotes per WPT
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
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' ||
               c == '|' || c == '}' || c == '`') {
      // Need to percent-encode unsafe characters including backtick for fragments
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
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' ||
               c == '|' || c == '}' || c == '`') {
      // Need to percent-encode unsafe characters including backtick for fragments
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
char* url_nonspecial_path_encode(const char* str) {
  if (!str)
    return NULL;

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

// Userinfo encoding per WHATWG URL spec (less aggressive than url_encode_with_len)
// According to WPT tests, some characters like & and ( should not be percent-encoded in userinfo
char* url_userinfo_encode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    // Characters allowed in userinfo without encoding (per WPT tests)
    // Based on WPT test expectations, most printable characters should be preserved
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*' || c == '&' || c == '(' || c == ')' || c == '!' || c == '$' || c == '\'' ||
        c == ',' || c == ';' || c == '=' || c == '+' || c == '"' || c == '%' || c == '<' || c == '>' || c == '@' ||
        c == '[' || c == '\\' || c == ']' || c == '^' || c == '`' || c == '{' || c == '|' || c == '}') {
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
        c == ',' || c == ';' || c == '=' || c == '+' || c == '"' || c == '%' || c == '<' || c == '>' || c == '@' ||
        c == '[' || c == '\\' || c == ']' || c == '^' || c == '`' || c == '{' || c == '|' || c == '}') {
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