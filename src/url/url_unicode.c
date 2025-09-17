#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "../../deps/libuv/src/idna.h"
#include "url.h"

// Unicode normalization for hostnames
// Convert fullwidth characters to halfwidth and apply case normalization
char* normalize_hostname_unicode(const char* hostname) {
  if (!hostname) {
    return NULL;
  }

  size_t len = strlen(hostname);
  // Allocate buffer (worst case: each byte might expand)
  char* normalized = malloc(len * 2 + 1);
  if (!normalized) {
    return NULL;
  }

  size_t write_pos = 0;
  size_t read_pos = 0;

  while (read_pos < len) {
    unsigned char c = (unsigned char)hostname[read_pos];

    // Handle UTF-8 sequences
    if (c >= 0x80) {
      // Multi-byte UTF-8 character
      if ((c & 0xE0) == 0xC0 && read_pos + 1 < len) {
        // 2-byte sequence
        unsigned char c2 = (unsigned char)hostname[read_pos + 1];
        if ((c2 & 0xC0) == 0x80) {
          // Calculate the Unicode codepoint for 2-byte UTF-8
          unsigned int codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);

          // U+00AD (soft hyphen) - should be removed per IDNA spec
          if (codepoint == 0x00AD) {
            read_pos += 2;  // Skip the soft hyphen entirely
            continue;
          }

          // Other 2-byte UTF-8 sequences - copy as-is
        }
      } else if ((c & 0xF0) == 0xE0 && read_pos + 2 < len) {
        // 3-byte sequence - handle fullwidth Latin characters
        unsigned char c2 = (unsigned char)hostname[read_pos + 1];
        unsigned char c3 = (unsigned char)hostname[read_pos + 2];
        if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
          unsigned int codepoint = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);

          // Handle specific fullwidth characters that appear in WPT tests
          if (codepoint == 0x3002) {  // U+3002 IDEOGRAPHIC FULL STOP 。
            normalized[write_pos++] = '.';
            read_pos += 3;
            continue;
          }

          // Fullwidth Latin capital letters: U+FF21-U+FF3A (Ａ-Ｚ) → A-Z
          if (codepoint >= 0xFF21 && codepoint <= 0xFF3A) {
            normalized[write_pos++] = (char)('a' + (codepoint - 0xFF21));  // Convert to lowercase
            read_pos += 3;
            continue;
          }
          // Fullwidth Latin small letters: U+FF41-U+FF5A (ａ-ｚ) → a-z
          else if (codepoint >= 0xFF41 && codepoint <= 0xFF5A) {
            normalized[write_pos++] = (char)('a' + (codepoint - 0xFF41));
            read_pos += 3;
            continue;
          }
          // Fullwidth digits: U+FF10-U+FF19 (０-９) → 0-9
          else if (codepoint >= 0xFF10 && codepoint <= 0xFF19) {
            normalized[write_pos++] = (char)('0' + (codepoint - 0xFF10));
            read_pos += 3;
            continue;
          }
          // Other fullwidth symbols
          else if (codepoint == 0xFF0E) {  // U+FF0E FULLWIDTH FULL STOP ．
            normalized[write_pos++] = '.';
            read_pos += 3;
            continue;
          } else if (codepoint == 0xFF0D) {  // U+FF0D FULLWIDTH HYPHEN-MINUS －
            normalized[write_pos++] = '-';
            read_pos += 3;
            continue;
          }
        }
      }

      // If we get here, copy the original UTF-8 sequence
      if ((c & 0xE0) == 0xC0 && read_pos + 1 < len) {
        normalized[write_pos++] = hostname[read_pos++];
        normalized[write_pos++] = hostname[read_pos++];
      } else if ((c & 0xF0) == 0xE0 && read_pos + 2 < len) {
        normalized[write_pos++] = hostname[read_pos++];
        normalized[write_pos++] = hostname[read_pos++];
        normalized[write_pos++] = hostname[read_pos++];
      } else if ((c & 0xF8) == 0xF0 && read_pos + 3 < len) {
        normalized[write_pos++] = hostname[read_pos++];
        normalized[write_pos++] = hostname[read_pos++];
        normalized[write_pos++] = hostname[read_pos++];
        normalized[write_pos++] = hostname[read_pos++];
      } else {
        // Invalid UTF-8 or incomplete sequence
        normalized[write_pos++] = hostname[read_pos++];
      }
    } else {
      // ASCII character - convert to lowercase
      normalized[write_pos++] = tolower(c);
      read_pos++;
    }
  }

  normalized[write_pos] = '\0';

  // Shrink buffer to actual size
  char* final_result = realloc(normalized, write_pos + 1);
  return final_result ? final_result : normalized;
}

// Convert Unicode hostname to ASCII using IDNA 2008
// Returns ASCII representation (punycode) for Unicode domains, or copy for ASCII domains
char* hostname_to_ascii(const char* hostname) {
  if (!hostname || strlen(hostname) == 0) {
    return strdup("");
  }

  // Check if hostname contains only ASCII characters
  int has_unicode = 0;
  for (size_t i = 0; hostname[i] != '\0'; i++) {
    if ((unsigned char)hostname[i] > 127) {
      has_unicode = 1;
      break;
    }
  }

  // If hostname is pure ASCII, just return a copy (lowercased)
  if (!has_unicode) {
    char* ascii_copy = strdup(hostname);
    if (!ascii_copy)
      return NULL;

    // Convert to lowercase per WHATWG URL spec
    for (size_t i = 0; ascii_copy[i] != '\0'; i++) {
      ascii_copy[i] = tolower(ascii_copy[i]);
    }
    return ascii_copy;
  }

  // Use libuv's IDNA implementation to convert Unicode to ASCII
  size_t hostname_len = strlen(hostname);

  // Allocate buffer for ASCII output (conservative estimate)
  // Punycode typically expands size, so we use 4x the input size as buffer
  size_t ascii_buffer_size = hostname_len * 4 + 64;
  char* ascii_buffer = malloc(ascii_buffer_size);
  if (!ascii_buffer) {
    return NULL;
  }

  // Convert using libuv's IDNA function
  ssize_t result = uv__idna_toascii(hostname, hostname + hostname_len, ascii_buffer, ascii_buffer + ascii_buffer_size);

  if (result < 0) {
    // IDNA conversion failed - this means invalid Unicode domain
    free(ascii_buffer);
    return NULL;
  }

  // Shrink buffer to actual size
  char* final_ascii = realloc(ascii_buffer, result);
  return final_ascii ? final_ascii : ascii_buffer;
}