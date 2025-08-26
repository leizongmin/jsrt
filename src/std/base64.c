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
static const uint8_t base64_decode_table[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,  62, 255, 255, 255,  63,
     52,  53,  54,  55,  56,  57,  58,  59,  60,  61, 255, 255, 255, 254, 255, 255,  // 254 = padding
    255,   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
     15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 255, 255, 255, 255, 255,
    255,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
     41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

// btoa implementation - encodes a string to Base64
static JSValue JSRT_btoa(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "btoa requires 1 argument");
  }

  const char *input_str = JS_ToCString(ctx, argv[0]);
  if (!input_str) {
    return JS_EXCEPTION;
  }

  size_t input_len = strlen(input_str);

  // Validate input - btoa should only accept valid Latin-1 (0-255) characters
  for (size_t i = 0; i < input_len; i++) {
    unsigned char c = (unsigned char)input_str[i];
    if (c > 255) {
      JS_FreeCString(ctx, input_str);
      return JS_ThrowTypeError(ctx, "The string to be encoded contains characters outside of the Latin1 range.");
    }
  }

  // Calculate output length
  size_t output_len = 4 * ((input_len + 2) / 3);
  char *output = malloc(output_len + 1);
  if (!output) {
    JS_FreeCString(ctx, input_str);
    return JS_ThrowOutOfMemory(ctx);
  }

  size_t j = 0;
  for (size_t i = 0; i < input_len; i += 3) {
    uint32_t octet_a = i < input_len ? (unsigned char)input_str[i] : 0;
    uint32_t octet_b = i + 1 < input_len ? (unsigned char)input_str[i + 1] : 0;
    uint32_t octet_c = i + 2 < input_len ? (unsigned char)input_str[i + 2] : 0;

    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    output[j++] = base64_encode_table[(triple >> 18) & 63];
    output[j++] = base64_encode_table[(triple >> 12) & 63];
    output[j++] = i + 1 < input_len ? base64_encode_table[(triple >> 6) & 63] : '=';
    output[j++] = i + 2 < input_len ? base64_encode_table[triple & 63] : '=';
  }

  output[output_len] = '\0';
  JS_FreeCString(ctx, input_str);

  JSValue result = JS_NewString(ctx, output);
  free(output);
  return result;
}

// atob implementation - decodes a Base64 string
static JSValue JSRT_atob(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "atob requires 1 argument");
  }

  const char *input_str = JS_ToCString(ctx, argv[0]);
  if (!input_str) {
    return JS_EXCEPTION;
  }

  size_t input_len = strlen(input_str);

  // Base64 input length must be a multiple of 4
  if (input_len % 4 != 0) {
    JS_FreeCString(ctx, input_str);
    return JS_ThrowTypeError(ctx, "The string to be decoded is not correctly encoded.");
  }

  // Calculate output length
  size_t output_len = input_len / 4 * 3;
  if (input_len >= 1 && input_str[input_len - 1] == '=') output_len--;
  if (input_len >= 2 && input_str[input_len - 2] == '=') output_len--;

  unsigned char *output = malloc(output_len + 1);
  if (!output) {
    JS_FreeCString(ctx, input_str);
    return JS_ThrowOutOfMemory(ctx);
  }

  size_t j = 0;
  for (size_t i = 0; i < input_len; i += 4) {
    uint8_t sextet_a = base64_decode_table[(unsigned char)input_str[i]];
    uint8_t sextet_b = base64_decode_table[(unsigned char)input_str[i + 1]];
    uint8_t sextet_c = base64_decode_table[(unsigned char)input_str[i + 2]];
    uint8_t sextet_d = base64_decode_table[(unsigned char)input_str[i + 3]];

    // Check for invalid characters
    if (sextet_a == 255 || sextet_b == 255 || (sextet_c == 255 && input_str[i + 2] != '=') || (sextet_d == 255 && input_str[i + 3] != '=')) {
      JS_FreeCString(ctx, input_str);
      free(output);
      return JS_ThrowTypeError(ctx, "The string to be decoded contains invalid characters.");
    }

    // Handle padding
    if (input_str[i + 2] == '=') sextet_c = 0;
    if (input_str[i + 3] == '=') sextet_d = 0;

    uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;

    if (j < output_len) output[j++] = (triple >> 16) & 255;
    if (j < output_len) output[j++] = (triple >> 8) & 255;  
    if (j < output_len) output[j++] = triple & 255;
  }

  output[output_len] = '\0';
  JS_FreeCString(ctx, input_str);

  JSValue result = JS_NewStringLen(ctx, (const char *)output, output_len);
  free(output);
  return result;
}

// Setup Base64 global functions
void JSRT_RuntimeSetupStdBase64(JSRT_Runtime *rt) {
  JS_SetPropertyStr(rt->ctx, rt->global, "btoa", JS_NewCFunction(rt->ctx, JSRT_btoa, "btoa", 1));
  JS_SetPropertyStr(rt->ctx, rt->global, "atob", JS_NewCFunction(rt->ctx, JSRT_atob, "atob", 1));
}