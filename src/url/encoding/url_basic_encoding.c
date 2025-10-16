#include <ctype.h>
#include <stdint.h>  // For SIZE_MAX
#include <stdio.h>
#include "../url.h"

static const char hex_chars[] = "0123456789ABCDEF";

// Safe allocation with overflow protection
static char* safe_malloc_for_encoding(size_t encoded_len) {
  // Check for overflow in final allocation
  if (encoded_len > SIZE_MAX - 1) {
    return NULL;  // Would overflow when adding null terminator
  }
  return malloc(encoded_len + 1);
}

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

  // Calculate encoded length with overflow protection
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    size_t add_len;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*') {
      add_len = 1;
    } else if (c == ' ') {
      add_len = 1;  // space becomes +
    } else {
      add_len = 3;  // %XX
    }

    // Check for integer overflow
    if (encoded_len > SIZE_MAX - add_len) {
      return NULL;  // Would overflow
    }
    encoded_len += add_len;
  }

  char* encoded = safe_malloc_for_encoding(encoded_len);
  if (!encoded) {
    return NULL;
  }
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

  // Calculate encoded length with overflow protection
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    size_t add_len;

    // Check if character needs encoding per URL spec
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      add_len = 3;
    } else if (c <= 32 || c >= 127 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '\\' || c == '^' ||
               c == '`' || c == '{' || c == '|' || c == '}') {
      // Encode control characters, space, non-ASCII characters, and specific unsafe characters
      // Include single quotes and backticks per WPT requirements
      add_len = 3;  // %XX
    } else {
      add_len = 1;
    }

    // Check for integer overflow
    if (encoded_len > SIZE_MAX - add_len) {
      return NULL;  // Would overflow
    }
    encoded_len += add_len;
  }

  char* encoded = safe_malloc_for_encoding(encoded_len);
  if (!encoded) {
    return NULL;
  }
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, copy as-is
      encoded[j++] = str[i];
      encoded[j++] = str[i + 1];
      encoded[j++] = str[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c <= 32 || c >= 127 || c == '"' || c == '\'' || c == '<' || c == '>' || c == '\\' || c == '^' ||
               c == '`' || c == '{' || c == '|' || c == '}') {
      // Encode control characters, space, non-ASCII characters, and specific unsafe characters
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

// UTF-8 validation helper function
static int is_valid_utf8_sequence(const unsigned char* bytes, size_t start, size_t max_len, size_t* seq_len) {
  if (start >= max_len)
    return 0;

  unsigned char c = bytes[start];

  if (c < 0x80) {
    // ASCII
    *seq_len = 1;
    return 1;
  } else if ((c & 0xE0) == 0xC0) {
    // 2-byte sequence
    if (start + 1 >= max_len)
      return 0;
    *seq_len = 2;
    return (bytes[start + 1] & 0xC0) == 0x80;
  } else if ((c & 0xF0) == 0xE0) {
    // 3-byte sequence
    if (start + 2 >= max_len)
      return 0;
    *seq_len = 3;
    return (bytes[start + 1] & 0xC0) == 0x80 && (bytes[start + 2] & 0xC0) == 0x80;
  } else if ((c & 0xF8) == 0xF0) {
    // 4-byte sequence
    if (start + 3 >= max_len)
      return 0;
    *seq_len = 4;
    return (bytes[start + 1] & 0xC0) == 0x80 && (bytes[start + 2] & 0xC0) == 0x80 && (bytes[start + 3] & 0xC0) == 0x80;
  }

  return 0;  // Invalid UTF-8 start byte
}

// Helper function to strip Unicode zero-width characters from hostname
static char* strip_unicode_zero_width_from_hostname(const char* input) {
  if (!input)
    return NULL;

  size_t input_len = strlen(input);
  char* result = malloc(input_len + 1);
  if (!result)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < input_len; i++) {
    unsigned char c = (unsigned char)input[i];

    // Check for UTF-8 sequences that represent zero-width characters
    if (c >= 0x80) {
      size_t utf8_len;
      if (is_valid_utf8_sequence((const unsigned char*)input, i, input_len, &utf8_len)) {
        // Check for specific zero-width characters in hostname context
        if (utf8_len == 3 && i + 2 < input_len) {
          unsigned char c2 = (unsigned char)input[i + 1];
          unsigned char c3 = (unsigned char)input[i + 2];

          if (c == 0xE2) {
            // Zero-width characters: U+200B, U+200C, U+200D, U+200E, U+200F
            if (c2 == 0x80 && (c3 == 0x8B || c3 == 0x8C || c3 == 0x8D || c3 == 0x8E || c3 == 0x8F)) {
              i += utf8_len - 1;  // Skip this character
              continue;
            }
            // Word joiner (U+2060): 0xE2 0x81 0xA0
            if (c2 == 0x81 && c3 == 0xA0) {
              i += utf8_len - 1;  // Skip this character
              continue;
            }
          } else if (c == 0xEF) {
            // Zero Width No-Break Space / BOM (U+FEFF): 0xEF 0xBB 0xBF
            if (c2 == 0xBB && c3 == 0xBF) {
              i += utf8_len - 1;  // Skip this character
              continue;
            }
          }
        }

        // Copy the entire valid UTF-8 sequence
        for (size_t k = 0; k < utf8_len && i + k < input_len; k++) {
          result[j++] = input[i + k];
        }
        i += utf8_len - 1;  // Will be incremented by loop
      } else {
        // Invalid UTF-8, copy as-is
        result[j++] = c;
      }
    } else {
      // ASCII character, copy as-is
      result[j++] = c;
    }
  }

  result[j] = '\0';
  return result;
}

// URL decode function for query parameters (+ becomes space)
char* url_decode_query_with_length_and_output_len(const char* str, size_t len, size_t* output_len) {
  char* decoded = malloc(len * 3 + 1);  // Allocate more space for potential replacement characters
  if (!decoded) {
    return NULL;
  }
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

// URL decode function specifically for hostnames with validation
// Returns NULL if the decoded hostname contains forbidden characters
// Decode hostname with scheme-aware validation
char* url_decode_hostname_with_scheme(const char* str, const char* scheme) {
  if (!str)
    return NULL;

  // First strip Unicode zero-width characters from hostname
  char* cleaned_hostname = strip_unicode_zero_width_from_hostname(str);
  if (!cleaned_hostname)
    return NULL;

  JSRT_Debug("url_decode_hostname_with_scheme: input='%s', cleaned='%s', scheme='%s'", str, cleaned_hostname,
             scheme ? scheme : "NULL");

  // Determine if this is a special scheme
  int is_special = scheme ? is_special_scheme(scheme) : 0;

  size_t len = strlen(cleaned_hostname);
  char* decoded = malloc(len * 3 + 1);
  if (!decoded) {
    free(cleaned_hostname);
    return NULL;
  }
  size_t i = 0, j = 0;

  while (i < len) {
    if (cleaned_hostname[i] == '%') {
      // Check if we have enough characters for complete percent encoding
      if (i + 2 < len) {
        int h1 = hex_to_int(cleaned_hostname[i + 1]);
        int h2 = hex_to_int(cleaned_hostname[i + 2]);
        if (h1 >= 0 && h2 >= 0) {
          // Valid percent encoding found
          unsigned char byte = (unsigned char)((h1 << 4) | h2);

          // For non-special schemes, preserve percent-encoded control characters
          // Per WHATWG URL spec, non-special schemes keep control chars encoded
          if (!is_special) {
            // Preserve percent-encoded control characters (0x00-0x1F, 0x7F) and delimiters
            if (byte < 0x20 || byte == 0x7F || byte == ' ' || byte == '#' || byte == '/' || byte == ':' ||
                byte == '?' || byte == '@' || byte == '[' || byte == '\\' || byte == ']') {
              // Keep these characters percent-encoded for non-special schemes
              decoded[j++] = '%';
              decoded[j++] = cleaned_hostname[i + 1];
              decoded[j++] = cleaned_hostname[i + 2];
              i += 3;
              continue;
            }
            // Also preserve alphanumeric and safe characters as encoded
            if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
                byte == '-' || byte == '.' || byte == '_' || byte == '~' || byte == '|') {
              // These characters don't need encoding but preserve as encoded for non-special schemes
              decoded[j++] = '%';
              decoded[j++] = cleaned_hostname[i + 1];
              decoded[j++] = cleaned_hostname[i + 2];
              i += 3;
              continue;
            }
          } else {
            // For special schemes, reject forbidden characters
            // Control characters (0x00-0x1F, 0x7F) and certain delimiter characters are forbidden
            if (byte < 0x20 || byte == 0x7F || byte == ' ' || byte == '#' || byte == '/' || byte == ':' ||
                byte == '?' || byte == '@' || byte == '[' || byte == '\\' || byte == ']') {
              // Forbidden character found in special scheme hostname, reject
              free(decoded);
              free(cleaned_hostname);
              return NULL;
            }
          }

          // For special schemes, or for non-special schemes with characters that must be decoded
          decoded[j++] = byte;
          i += 3;
          continue;
        }
      }
      // Incomplete or invalid percent encoding - treat as literal character
      decoded[j++] = cleaned_hostname[i++];
    } else {
      decoded[j++] = cleaned_hostname[i++];
    }
  }
  decoded[j] = '\0';
  free(cleaned_hostname);
  JSRT_Debug("url_decode_hostname_with_scheme: output='%s'", decoded);
  return decoded;
}

// Backward compatibility wrapper
char* url_decode_hostname(const char* str) {
  return url_decode_hostname_with_scheme(str, NULL);
}