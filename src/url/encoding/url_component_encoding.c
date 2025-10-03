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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '`' || c >= 127) {
      // Need to percent-encode unsafe characters including backtick for fragments
      // Per WPT tests: ^, {, |, } should NOT be encoded in fragments
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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '`' || c >= 127) {
      // Need to percent-encode unsafe characters including backtick for fragments
      // Per WPT tests: ^, {, |, } should NOT be encoded in fragments
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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '`' || c >= 127) {
      // Need to percent-encode unsafe characters including spaces and Unicode characters
      // Per WPT tests: ^, {, |, } should NOT be encoded in fragments
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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '`' || c >= 127) {
      // Need to percent-encode unsafe characters including spaces and Unicode characters
      // Per WPT tests: ^, {, |, } should NOT be encoded in fragments
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
      // NOTE: Single quote ('), pipe (|) are preserved per WPT specification for special scheme paths
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
      // NOTE: Single quote ('), pipe (|) are preserved per WPT specification for special scheme paths
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

// Special encoding for non-special scheme paths (opaque paths)
// Per WHATWG URL spec: opaque paths are very permissive - preserve almost all printable chars
// Only encode control characters (< 0x20), DEL (0x7F), and non-ASCII (>= 0x80)
// Spaces, tab, newline should already be removed by preprocessing
char* url_nonspecial_path_encode(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);

  // Tab, newline, CR should already be removed by preprocessing
  // Calculate encoded length - opaque paths preserve most printable characters
  size_t encoded_len = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];

    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c < 0x20 || c == '"' || c == '<' || c == '>' || c == '`' || c == 0x7F || c >= 0x80) {
      // Encode: control characters, ", <, >, `, DEL, and non-ASCII
      // Per WPT tests, these specific characters must be encoded even in opaque paths
      // But preserve: \, ^, {, |, }, and most other printable ASCII
      encoded_len += 3;  // %XX
    } else {
      // Preserve other printable ASCII characters
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
    } else if (c < 0x20 || c == '"' || c == '<' || c == '>' || c == '`' || c == 0x7F || c >= 0x80) {
      // Encode: control characters, ", <, >, `, DEL, and non-ASCII
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    } else {
      // Preserve other printable ASCII characters
      encoded[j++] = c;
    }
  }
  encoded[j] = '\0';

  return encoded;
}

// Path encoding for non-special schemes WITH authority (foo://host/path)
// Different from opaque paths - encodes spaces and special chars like ^{}
char* url_path_encode_nonspecial_with_authority(const char* str) {
  if (!str)
    return NULL;

  size_t len = strlen(str);
  size_t encoded_len = 0;

  // Calculate encoded length
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];

    if (c == '%' && i + 2 < len && hex_to_int(str[i + 1]) >= 0 && hex_to_int(str[i + 2]) >= 0) {
      // Already percent-encoded sequence, keep as-is
      encoded_len += 3;
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '^' || c == '`' || c == '{' || c == '}' ||
               c == 127 || c >= 128) {
      // Encode spaces, control chars, and special chars ^{}
      // NOTE: backslash (\) is preserved in non-special scheme paths (no normalization)
      // NOTE: pipe (|) is preserved per WPT tests
      // NOTE: single quote (') is preserved per WPT tests
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
    } else if (c <= 32 || c == '"' || c == '<' || c == '>' || c == '^' || c == '`' || c == '{' || c == '}' ||
               c == 127 || c >= 128) {
      // Encode spaces, control chars, and special chars ^{}
      // NOTE: backslash (\) is preserved in non-special scheme paths (no normalization)
      // NOTE: pipe (|) is preserved per WPT tests
      // NOTE: single quote (') is preserved per WPT tests
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
    if (!scheme_clean) {
      return NULL;
    }
    size_t scheme_len = strlen(scheme_clean);  // Cache length
    if (scheme_len > 0 && scheme_clean[scheme_len - 1] == ':') {
      scheme_clean[scheme_len - 1] = '\0';
    }

    // Per WPT tests: userinfo encoding rules
    // For ALL schemes: always encode '[', ']', '<', '>', '^', '|', '`', '{', '}'
    // For ALL schemes: always encode ';', '<', '=', '>', '@' per WPT urltestdata.json
    // ws/wss are NOT exceptions - wss is a special scheme and follows special scheme rules
    encode_closing_bracket = 1;  // Always encode ']' per WPT requirements
    encode_at_symbol = 1;        // Always encode '@', ';', '='
    encode_colon = 1;            // Always encode ':'

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
    } else if (c < 0x20 || c == 0x7F || c > 0x7F) {
      // Only encode: control characters (0x00-0x1F), DEL (0x7F), and non-ASCII (0x80+)
      // Per WPT, printable ASCII characters like space, !"$%&'()*+,-.;=_`{}~ should NOT be encoded
      add_len = 3;  // %XX
    } else {
      // Printable ASCII characters (0x20-0x7E except 0x7F) remain as-is
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
    } else if (c < 0x20 || c == 0x7F || c > 0x7F) {
      // Encode: control characters (0x00-0x1F), DEL (0x7F), and non-ASCII (0x80+)
      encoded[j++] = '%';
      encoded[j++] = hex_chars[c >> 4];
      encoded[j++] = hex_chars[c & 15];
    } else {
      // Printable ASCII characters (0x20-0x7E except 0x7F) remain as-is
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
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || (is_special && c == '\'')) {
      // Encode control characters, space, non-ASCII characters, and specific unsafe characters
      // Per WPT tests: \, ^, _, `, {, |, } should NOT be encoded in query strings
      // For special schemes: encode single quotes (')
      // For non-special schemes: do NOT encode single quotes (')
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
    } else if (c <= 32 || c >= 127 || c == '"' || c == '<' || c == '>' || (is_special && c == '\'')) {
      // Encode control characters, space, non-ASCII characters, and specific unsafe characters
      // Per WPT tests: \, ^, _, `, {, |, } should NOT be encoded in query strings
      // For special schemes: encode single quotes (')
      // For non-special schemes: do NOT encode single quotes (')
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