#include "base64.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"

// Standard Base64 encoding table
static const char base64_encode_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64 decoding table - maps ASCII to 6-bit values, 255 for invalid chars
static const uint8_t base64_decode_table[256] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 62,  255, 255, 255, 63,  52,  53,  54,  55,  56,  57,  58,  59,
                                                 60,  61,  255, 255, 255, 254, 255, 255,  // 254 = padding
                                                 255, 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,
                                                 13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  255,
                                                 255, 255, 255, 255, 255, 26,  27,  28,  29,  30,  31,  32,  33,  34,
                                                 35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
                                                 49,  50,  51,  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                                 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

// btoa implementation - encodes a string to Base64
static JSValue JSRT_btoa(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "btoa requires 1 argument");
  }

  // Convert argument to string
  JSValue str_val = JS_ToString(ctx, argv[0]);
  if (JS_IsException(str_val)) {
    return JS_EXCEPTION;
  }

  // Use QuickJS to iterate through Unicode code points
  // We'll build an array of Latin-1 bytes first
  unsigned char* latin1_bytes = NULL;
  size_t byte_len = 0;
  size_t byte_capacity = 0;

  // Get string length (in Unicode code units)
  JSValue length_val = JS_GetPropertyStr(ctx, str_val, "length");
  if (JS_IsException(length_val)) {
    JS_FreeValue(ctx, str_val);
    return JS_EXCEPTION;
  }

  int32_t str_length;
  if (JS_ToInt32(ctx, &str_length, length_val) < 0) {
    JS_FreeValue(ctx, length_val);
    JS_FreeValue(ctx, str_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length_val);

  // Iterate through each character
  for (int32_t i = 0; i < str_length; i++) {
    // Get character code at position i
    JSValue index_val = JS_NewInt32(ctx, i);
    JSAtom charCodeAt_atom = JS_NewAtom(ctx, "charCodeAt");
    JSValue char_code = JS_Invoke(ctx, str_val, charCodeAt_atom, 1, &index_val);
    JS_FreeAtom(ctx, charCodeAt_atom);

    if (JS_IsException(char_code)) {
      JS_FreeValue(ctx, str_val);
      free(latin1_bytes);
      return JS_EXCEPTION;
    }

    int32_t code;
    if (JS_ToInt32(ctx, &code, char_code) < 0) {
      JS_FreeValue(ctx, char_code);
      JS_FreeValue(ctx, str_val);
      free(latin1_bytes);
      return JS_EXCEPTION;
    }
    JS_FreeValue(ctx, char_code);

    // Check if code point is outside Latin-1 range
    if (code > 255) {
      JS_FreeValue(ctx, str_val);
      free(latin1_bytes);
      return JS_ThrowTypeError(ctx, "The string to be encoded contains characters outside of the Latin1 range.");
    }

    // Expand buffer if needed
    if (byte_len >= byte_capacity) {
      byte_capacity = byte_capacity ? byte_capacity * 2 : 16;
      unsigned char* new_bytes = realloc(latin1_bytes, byte_capacity);
      if (!new_bytes) {
        JS_FreeValue(ctx, str_val);
        free(latin1_bytes);
        return JS_ThrowOutOfMemory(ctx);
      }
      latin1_bytes = new_bytes;
    }

    latin1_bytes[byte_len++] = (unsigned char)code;
  }

  JS_FreeValue(ctx, str_val);

  // Now encode the Latin-1 bytes to Base64
  size_t output_len = 4 * ((byte_len + 2) / 3);
  char* output = malloc(output_len + 1);
  if (!output) {
    free(latin1_bytes);
    return JS_ThrowOutOfMemory(ctx);
  }

  size_t j = 0;
  for (size_t i = 0; i < byte_len; i += 3) {
    uint32_t octet_a = i < byte_len ? latin1_bytes[i] : 0;
    uint32_t octet_b = i + 1 < byte_len ? latin1_bytes[i + 1] : 0;
    uint32_t octet_c = i + 2 < byte_len ? latin1_bytes[i + 2] : 0;

    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    output[j++] = base64_encode_table[(triple >> 18) & 63];
    output[j++] = base64_encode_table[(triple >> 12) & 63];
    output[j++] = i + 1 < byte_len ? base64_encode_table[(triple >> 6) & 63] : '=';
    output[j++] = i + 2 < byte_len ? base64_encode_table[triple & 63] : '=';
  }

  output[output_len] = '\0';
  free(latin1_bytes);

  JSValue result = JS_NewString(ctx, output);
  free(output);
  return result;
}

// atob implementation - decodes a Base64 string
static JSValue JSRT_atob(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "atob requires 1 argument");
  }

  const char* input_str = JS_ToCString(ctx, argv[0]);
  if (!input_str) {
    return JS_EXCEPTION;
  }

  // Trim whitespace from input (HTML5 spec requirement)
  const char* start = input_str;
  const char* end = input_str + strlen(input_str);

  // Skip leading whitespace
  while (start < end && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' || *start == '\f')) {
    start++;
  }

  // Skip trailing whitespace
  while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r' || end[-1] == '\f')) {
    end--;
  }

  size_t input_len = end - start;

  // Create a trimmed copy of the string
  char* trimmed_input = malloc(input_len + 1);
  if (!trimmed_input) {
    JS_FreeCString(ctx, input_str);
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(trimmed_input, start, input_len);
  trimmed_input[input_len] = '\0';

  // Add implicit padding if needed (HTML5 spec allows this)
  size_t padded_len = input_len;
  char* padded_input = trimmed_input;

  if (input_len % 4 != 0) {
    padded_len = ((input_len + 3) / 4) * 4;  // Round up to next multiple of 4
    padded_input = malloc(padded_len + 1);
    if (!padded_input) {
      JS_FreeCString(ctx, input_str);
      free(trimmed_input);
      return JS_ThrowOutOfMemory(ctx);
    }

    // Copy original and add padding
    memcpy(padded_input, trimmed_input, input_len);
    for (size_t i = input_len; i < padded_len; i++) {
      padded_input[i] = '=';
    }
    padded_input[padded_len] = '\0';

    // Update our working variables
    input_len = padded_len;
    free(trimmed_input);
    trimmed_input = padded_input;
  }

  // Validate padding - must be correct amount and in correct positions
  size_t padding_count = 0;
  size_t first_padding_pos = input_len;  // Position of first padding character

  for (size_t i = 0; i < input_len; i++) {
    if (trimmed_input[i] == '=') {
      if (padding_count == 0) {
        first_padding_pos = i;
      }
      padding_count++;
    } else if (padding_count > 0) {
      // Non-padding character after padding started
      JS_FreeCString(ctx, input_str);
      free(trimmed_input);
      return JS_ThrowTypeError(ctx, "The string to be decoded is not correctly encoded.");
    }
  }

  // Check if padding is at the end
  if (padding_count > 0 && first_padding_pos + padding_count != input_len) {
    JS_FreeCString(ctx, input_str);
    free(trimmed_input);
    return JS_ThrowTypeError(ctx, "The string to be decoded is not correctly encoded.");
  }

  // Check padding correctness based on data length
  size_t data_chars = input_len - padding_count;

  // Valid combinations:
  // 4 data chars, 0 padding (4 total)
  // 3 data chars, 1 padding (4 total)
  // 2 data chars, 2 padding (4 total)
  // 1 data char is never valid (would need 3 padding, but that's not allowed)
  // 0 data chars is never valid

  // Special case: empty string is valid
  if (input_len == 0) {
    JS_FreeCString(ctx, input_str);
    free(trimmed_input);
    return JS_NewString(ctx, "");
  }

  if (padding_count >= 3 || data_chars == 1) {
    JS_FreeCString(ctx, input_str);
    free(trimmed_input);
    return JS_ThrowTypeError(ctx, "The string to be decoded is not correctly encoded.");
  }

  // Additionally, check the correct padding for data length
  if ((data_chars == 2 && padding_count != 2) || (data_chars == 3 && padding_count != 1)) {
    JS_FreeCString(ctx, input_str);
    free(trimmed_input);
    return JS_ThrowTypeError(ctx, "The string to be decoded is not correctly encoded.");
  }

  // Calculate output length
  size_t output_len = input_len / 4 * 3;
  if (input_len >= 1 && trimmed_input[input_len - 1] == '=')
    output_len--;
  if (input_len >= 2 && trimmed_input[input_len - 2] == '=')
    output_len--;

  unsigned char* output = malloc(output_len + 1);
  if (!output) {
    JS_FreeCString(ctx, input_str);
    free(trimmed_input);
    return JS_ThrowOutOfMemory(ctx);
  }

  size_t j = 0;
  for (size_t i = 0; i < input_len; i += 4) {
    uint8_t sextet_a = base64_decode_table[(unsigned char)trimmed_input[i]];
    uint8_t sextet_b = base64_decode_table[(unsigned char)trimmed_input[i + 1]];
    uint8_t sextet_c = base64_decode_table[(unsigned char)trimmed_input[i + 2]];
    uint8_t sextet_d = base64_decode_table[(unsigned char)trimmed_input[i + 3]];

    // Check for invalid characters
    if (sextet_a == 255 || sextet_b == 255 || (sextet_c == 255 && trimmed_input[i + 2] != '=') ||
        (sextet_d == 255 && trimmed_input[i + 3] != '=')) {
      JS_FreeCString(ctx, input_str);
      free(trimmed_input);
      free(output);
      return JS_ThrowTypeError(ctx, "The string to be decoded contains invalid characters.");
    }

    // Handle padding
    if (trimmed_input[i + 2] == '=')
      sextet_c = 0;
    if (trimmed_input[i + 3] == '=')
      sextet_d = 0;

    uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;

    if (j < output_len)
      output[j++] = (triple >> 16) & 255;
    if (j < output_len)
      output[j++] = (triple >> 8) & 255;
    if (j < output_len)
      output[j++] = triple & 255;
  }

  output[output_len] = '\0';
  JS_FreeCString(ctx, input_str);
  free(trimmed_input);

  // Convert binary output to JavaScript string where each byte becomes a Unicode code point
  // Use String.fromCharCode.apply(null, array_of_codes) for efficiency
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue string_constructor = JS_GetPropertyStr(ctx, global, "String");
  JSValue from_char_code = JS_GetPropertyStr(ctx, string_constructor, "fromCharCode");

  // Create array of character codes
  JSValue char_codes = JS_NewArray(ctx);
  for (size_t i = 0; i < output_len; i++) {
    JSValue code = JS_NewInt32(ctx, output[i]);
    JS_SetPropertyUint32(ctx, char_codes, i, code);
  }

  // Call String.fromCharCode.apply(null, char_codes)
  JSValue apply_args[2] = {JS_NULL, char_codes};
  JSValue apply_func = JS_GetPropertyStr(ctx, from_char_code, "apply");
  JSValue result = JS_Call(ctx, apply_func, from_char_code, 2, apply_args);

  // Cleanup
  JS_FreeValue(ctx, apply_func);
  JS_FreeValue(ctx, char_codes);
  JS_FreeValue(ctx, from_char_code);
  JS_FreeValue(ctx, string_constructor);
  JS_FreeValue(ctx, global);
  free(output);

  return result;
}

// Setup Base64 global functions
void JSRT_RuntimeSetupStdBase64(JSRT_Runtime* rt) {
  JS_SetPropertyStr(rt->ctx, rt->global, "btoa", JS_NewCFunction(rt->ctx, JSRT_btoa, "btoa", 1));
  JS_SetPropertyStr(rt->ctx, rt->global, "atob", JS_NewCFunction(rt->ctx, JSRT_atob, "atob", 1));
}