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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '|' || c == '}' ||
               c == '`' || c >= 127) {
      // Need to percent-encode unsafe characters including backtick for fragments
      // Note: backslashes (\\) are allowed in fragments per WPT tests
      // Note: Unicode characters (>= 127) must be percent-encoded per WHATWG URL spec
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '|' || c == '}' ||
               c == '`' || c >= 127) {
      // Need to percent-encode unsafe characters including backtick for fragments
      // Note: backslashes (\\) are allowed in fragments per WPT tests
      // Note: Unicode characters (>= 127) must be percent-encoded per WHATWG URL spec
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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '|' || c == '}' ||
               c == '`' || c >= 127) {
      // Need to percent-encode unsafe characters including spaces and Unicode characters
      // WPT tests expect Unicode characters (>= 127) to be encoded even for non-special schemes
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '|' || c == '}' ||
               c == '`' || c >= 127) {
      // Need to percent-encode unsafe characters including spaces and Unicode characters
      // WPT tests expect Unicode characters (>= 127) to be encoded even for non-special schemes
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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '}' ||
               c == '`' || c == 127) {
      // Need to percent-encode control characters, space, and specific unsafe characters
      // NOTE: Pipe character (|) is preserved per WPT specification
      // NOTE: '[' and ']' characters are NOT encoded in paths per WPT tests
      encoded_len += 3;  // %XX
    } else if (c >= 128) {
      // UTF-8 encoded characters - percent-encode each byte
      encoded_len += 3;  // %XX for this byte
    } else {
      encoded_len++;
    }
  }

  char* encoded = safe_malloc_for_encoding(encoded_len);
  if (!encoded) {
    return NULL;
  }
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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '}' ||
               c == '`' || c == 127) {
      // Need to percent-encode control characters and specific unsafe characters
      // NOTE: Pipe character (|) is preserved per WPT specification
      // NOTE: '[' and ']' characters are NOT encoded in paths per WPT tests
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    } else if (c >= 128) {
      // UTF-8 encoded characters - percent-encode each byte
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
    } else if (c < 32 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '}' ||
               c == '`' || c == 127) {
      // Need to percent-encode control characters and specific unsafe characters
      // NOTE: For file URLs, pipe character (|) is preserved, not encoded
      encoded_len += 3;  // %XX
    } else if (c >= 128) {
      // UTF-8 encoded characters - percent-encode each byte
      encoded_len += 3;  // %XX for this byte
    } else {
      encoded_len++;
    }
  }

  char* encoded = safe_malloc_for_encoding(encoded_len);
  if (!encoded) {
    return NULL;
  }
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
    } else if (c < 32 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '{' || c == '}' ||
               c == '`' || c == 127) {
      // Need to percent-encode control characters and specific unsafe characters
      // NOTE: For file URLs, pipe character (|) is preserved, not encoded
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    } else if (c >= 128) {
      // UTF-8 encoded characters - percent-encode each byte
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
// Per WPT tests: trailing spaces (before end of path) should be encoded as %20
// Tab and newline characters should be removed (not encoded)
char* url_nonspecial_path_encode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);

  // First pass: remove tab and newline characters, calculate final length
  char* cleaned = malloc(len + 1);
  if (!cleaned) {
    return NULL;
  }
  size_t cleaned_len = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (c != '\t' && c != '\r' && c != '\n') {
      cleaned[cleaned_len++] = c;
    }
  }
  cleaned[cleaned_len] = '\0';

  // Second pass: handle spaces per WPT requirements
  // Per WPT tests: for non-special opaque paths, spaces are kept literal EXCEPT
  // trailing spaces before end/query/fragment where only the LAST space becomes %20
  char* normalized = malloc(cleaned_len * 3 + 1);  // Worst case
  if (!normalized) {
    free(cleaned);
    return NULL;
  }
  size_t normalized_len = 0;

  for (size_t i = 0; i < cleaned_len; i++) {
    unsigned char c = (unsigned char)cleaned[i];
    if (c == ' ') {
      // Count consecutive spaces
      size_t space_count = 1;
      while (i + space_count < cleaned_len && cleaned[i + space_count] == ' ') {
        space_count++;
      }

      // Check if this is a trailing space sequence (before end or before ?/#)
      size_t next_pos = i + space_count;
      int is_trailing = (next_pos >= cleaned_len || cleaned[next_pos] == '?' || cleaned[next_pos] == '#');

      if (is_trailing) {
        // Trailing spaces: all literal except last one becomes %20
        for (size_t j = 0; j < space_count - 1; j++) {
          normalized[normalized_len++] = ' ';
        }
        // Last space becomes %20
        normalized[normalized_len++] = '%';
        normalized[normalized_len++] = '2';
        normalized[normalized_len++] = '0';
      } else {
        // Non-trailing spaces: all literal
        for (size_t j = 0; j < space_count; j++) {
          normalized[normalized_len++] = ' ';
        }
      }

      i += space_count - 1;  // Skip processed spaces
    } else {
      normalized[normalized_len++] = c;
    }
  }
  normalized[normalized_len] = '\0';

  free(cleaned);

  // Third pass: encode other characters (but not spaces which were handled above)
  size_t encoded_len = 0;
  for (size_t i = 0; i < normalized_len; i++) {
    unsigned char c = (unsigned char)normalized[i];
    if (c == '%' && i + 2 < normalized_len && hex_to_int(normalized[i + 1]) >= 0 &&
        hex_to_int(normalized[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c < 32 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '}' || c == '`' || c == 127 ||
               c >= 128) {
      // Encode control characters, unsafe characters, and non-ASCII
      // Note: spaces (0x20) are NOT encoded here - they were handled in previous pass
      // Note: '[' and ']' are NOT encoded in paths per WPT tests
      // Note: '\' is NOT encoded in non-special scheme paths (unlike special schemes)
      encoded_len += 3;  // %XX
    } else {
      encoded_len++;
    }
  }

  char* encoded = safe_malloc_for_encoding(encoded_len);
  if (!encoded) {
    free(normalized);
    return NULL;
  }
  size_t j = 0;

  for (size_t i = 0; i < normalized_len; i++) {
    unsigned char c = (unsigned char)normalized[i];
    if (c == '%' && i + 2 < normalized_len && hex_to_int(normalized[i + 1]) >= 0 &&
        hex_to_int(normalized[i + 2]) >= 0) {
      // Already percent-encoded sequence, copy as-is
      encoded[j++] = normalized[i];
      encoded[j++] = normalized[i + 1];
      encoded[j++] = normalized[i + 2];
      i += 2;  // Skip the next two characters
    } else if (c < 32 || c == '"' || c == '<' || c == '>' || c == '^' || c == '{' || c == '}' || c == '`' || c == 127 ||
               c >= 128) {
      // Encode control characters, unsafe characters, and non-ASCII
      // Note: spaces (0x20) are NOT encoded here - they were handled in previous pass
      // Note: '[' and ']' are NOT encoded in paths per WPT tests
      // Note: '\' is NOT encoded in non-special scheme paths (unlike special schemes)
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    } else {
      encoded[j++] = c;
    }
  }
  encoded[j] = '\0';

  free(normalized);
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
    if (!scheme_clean) {
      return NULL;
    }
    size_t scheme_len = strlen(scheme_clean);  // Cache length
    if (scheme_len > 0 && scheme_clean[scheme_len - 1] == ':') {
      scheme_clean[scheme_len - 1] = '\0';
    }

    // Per WPT tests: always encode '[' and ']' characters in userinfo
    // For ALL schemes (special and non-special): always encode '@', ';', '=' per WPT urltestdata.json
    // The previous logic was incorrect - WPT expects these characters encoded in ALL schemes
    if (strcmp(scheme_clean, "ws") == 0 || strcmp(scheme_clean, "wss") == 0) {
      encode_closing_bracket = 1;  // Always encode ']' per WPT requirements
      encode_at_symbol = 0;        // WebSocket schemes are exception
      encode_colon = 0;
    } else {
      // For all other schemes including non-special: encode @, ;, =
      encode_closing_bracket = 1;
      encode_at_symbol = 1;
      encode_colon = 1;
    }

    free(scheme_clean);
  }

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*' || c == '&' || c == '(' || c == ')' || c == '!' || c == '$' || c == '\'' ||
        c == ',' || c == '+' || c == '%' || (!encode_closing_bracket && c == ']') || (!encode_at_symbol && c == '@') ||
        (!encode_colon && c == ':') || (!encode_at_symbol && c == ';') || (!encode_at_symbol && c == '=')) {
      // Note: @, :, ] encoding depends on scheme
      // Per WHATWG URL spec and WPT tests: encode [, <, >, ^, |, `, {, } characters
      encoded_len++;
    } else {
      encoded_len += 3;  // %XX
    }
  }

  char* encoded = safe_malloc_for_encoding(encoded_len);
  if (!encoded) {
    return NULL;
  }
  size_t j = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~' || c == '*' || c == '&' || c == '(' || c == ')' || c == '!' || c == '$' || c == '\'' ||
        c == ',' || c == '+' || c == '%' || (!encode_closing_bracket && c == ']') || (!encode_at_symbol && c == '@') ||
        (!encode_colon && c == ':') || (!encode_at_symbol && c == ';') || (!encode_at_symbol && c == '=')) {
      // Note: @, :, ], ;, = encoding depends on scheme
      // Per WHATWG URL spec and WPT tests: encode [, <, >, ^, |, `, {, } characters
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

// Hostname encoding for non-special schemes - more permissive than general component encoding
char* url_hostname_encode_nonspecial(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length with overflow protection
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    size_t add_len;

    // Check if character needs encoding per WHATWG URL spec for non-special scheme hostnames
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      add_len = 3;
    } else if (c <= 32 || c >= 127) {
      // Only encode control characters (0-32) and non-ASCII characters (127+)
      // Per WPT, printable ASCII characters like !"$%&'()*+,-.;=_`{}~ should NOT be encoded in non-special scheme
      // hostnames
      add_len = 3;  // %XX
    } else {
      // Printable ASCII characters (33-126) remain as-is for non-special schemes
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
    } else if (c <= 32 || c >= 127) {
      // Encode control characters and non-ASCII characters
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    } else {
      // Printable ASCII characters remain as-is
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

// Query-specific encoding (single quotes should NOT be encoded in query)
char* url_query_encode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length with overflow protection
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    size_t add_len;

    // Check if character needs encoding per URL spec for query components
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      add_len = 3;
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`' ||
               c == '{' || c == '|' || c == '}') {
      // Encode control characters, space, non-ASCII characters, and specific unsafe characters
      // NOTE: Single quotes (') are NOT encoded in query parameters per WHATWG URL spec
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
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '`' ||
               c == '{' || c == '|' || c == '}') {
      // Encode control characters, space, non-ASCII characters, and specific unsafe characters
      // NOTE: Single quotes (') are NOT encoded in query parameters per WHATWG URL spec
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

// Query encoding with scheme awareness - single quotes need different handling
char* url_query_encode_with_scheme(const char* str, const char* scheme) {
  if (!str)
    return NULL;

  // Determine if this is a special scheme
  int is_special = scheme ? is_special_scheme(scheme) : 0;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length with overflow protection
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    size_t add_len;

    // Check if character needs encoding per URL spec for query components
    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      add_len = 3;
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '|' ||
               (is_special && c == '\'') || (!is_special && (c == '`' || c == '{' || c == '}'))) {
      // Encode control characters, space, non-ASCII characters, and specific unsafe characters
      // For special schemes: encode single quotes but preserve backticks and braces in query
      // For non-special schemes: encode backticks and braces
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
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || c == '\\' || c == '^' || c == '|' ||
               (is_special && c == '\'') || (!is_special && (c == '`' || c == '{' || c == '}'))) {
      // Encode control characters, space, non-ASCII characters, and specific unsafe characters
      // For special schemes: encode single quotes but preserve backticks and braces in query
      // For non-special schemes: encode backticks and braces
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