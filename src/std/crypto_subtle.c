#include "crypto_subtle.h"

// Platform-specific includes for dynamic loading
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../util/debug.h"
#include "crypto.h"
#include "crypto_digest.h"
#include "crypto_ec.h"
#include "crypto_hmac.h"
#include "crypto_kdf.h"
#include "crypto_rsa.h"
#include "crypto_symmetric.h"

// Cross-platform dynamic loading abstractions
#ifdef _WIN32
extern HMODULE openssl_handle;
#define JSRT_DLSYM(handle, name) ((void*)GetProcAddress(handle, name))
#else

#define JSRT_DLSYM(handle, name) dlsym(handle, name)
#endif

// Base64URL encoding table (RFC 7515 - differs from standard Base64)
static const char base64url_encode_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Base64URL decoding table
static const uint8_t base64url_decode_table[256] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 62,  255, 255, 52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  255, 255, 255, 255, 255, 255, 255, 0,
    1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,
    23,  24,  25,  255, 255, 255, 255, 63,  255, 26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
    39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

// Base64URL encode function (no padding)
static char* base64url_encode(const uint8_t* input, size_t input_len, size_t* output_len) {
  if (!input || input_len == 0) {
    if (output_len)
      *output_len = 0;
    return NULL;
  }

  *output_len = 4 * ((input_len + 2) / 3);
  char* output = malloc(*output_len + 1);
  if (!output)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < input_len; i += 3) {
    uint32_t octet_a = input[i];
    uint32_t octet_b = (i + 1 < input_len) ? input[i + 1] : 0;
    uint32_t octet_c = (i + 2 < input_len) ? input[i + 2] : 0;

    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    output[j++] = base64url_encode_table[(triple >> 18) & 63];
    output[j++] = base64url_encode_table[(triple >> 12) & 63];
    if (i + 1 < input_len) {
      output[j++] = base64url_encode_table[(triple >> 6) & 63];
    }
    if (i + 2 < input_len) {
      output[j++] = base64url_encode_table[triple & 63];
    }
  }

  *output_len = j;  // Actual length without padding
  output[j] = '\0';
  return output;
}

// Base64URL decode function (no padding expected)
static uint8_t* base64url_decode(const char* input, size_t input_len, size_t* output_len) {
  if (!input || input_len == 0) {
    if (output_len)
      *output_len = 0;
    return NULL;
  }

  // Calculate output length
  *output_len = input_len * 3 / 4;
  if (input_len % 4 == 2)
    *output_len += 1;
  else if (input_len % 4 == 3)
    *output_len += 2;

  uint8_t* output = malloc(*output_len + 1);
  if (!output)
    return NULL;

  size_t output_pos = 0;
  uint32_t accumulator = 0;
  int bits_accumulated = 0;

  for (size_t i = 0; i < input_len; i++) {
    uint8_t value = base64url_decode_table[(unsigned char)input[i]];
    if (value == 255) {  // Invalid character
      free(output);
      return NULL;
    }

    accumulator = (accumulator << 6) | value;
    bits_accumulated += 6;

    if (bits_accumulated >= 8) {
      output[output_pos++] = (accumulator >> (bits_accumulated - 8)) & 255;
      bits_accumulated -= 8;
    }
  }

  *output_len = output_pos;
  return output;
}

// Algorithm name to enum mapping
static const struct {
  const char* name;
  jsrt_crypto_algorithm_t algorithm;
} algorithm_map[] = {{"SHA-1", JSRT_CRYPTO_ALG_SHA1},
                     {"SHA-256", JSRT_CRYPTO_ALG_SHA256},
                     {"SHA-384", JSRT_CRYPTO_ALG_SHA384},
                     {"SHA-512", JSRT_CRYPTO_ALG_SHA512},
                     {"AES-CBC", JSRT_CRYPTO_ALG_AES_CBC},
                     {"AES-GCM", JSRT_CRYPTO_ALG_AES_GCM},
                     {"AES-CTR", JSRT_CRYPTO_ALG_AES_CTR},
                     {"RSA-OAEP", JSRT_CRYPTO_ALG_RSA_OAEP},
                     {"RSA-PSS", JSRT_CRYPTO_ALG_RSA_PSS},
                     {"RSASSA-PKCS1-v1_5", JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5},
                     {"RSA-PKCS1-v1_5", JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5},
                     {"ECDSA", JSRT_CRYPTO_ALG_ECDSA},
                     {"ECDH", JSRT_CRYPTO_ALG_ECDH},
                     {"HMAC", JSRT_CRYPTO_ALG_HMAC},
                     {"PBKDF2", JSRT_CRYPTO_ALG_PBKDF2},
                     {"HKDF", JSRT_CRYPTO_ALG_HKDF},
                     {NULL, JSRT_CRYPTO_ALG_UNKNOWN}};

// Parse algorithm parameter
jsrt_crypto_algorithm_t jsrt_crypto_parse_algorithm(JSContext* ctx, JSValue algorithm) {
  if (JS_IsString(algorithm)) {
    const char* alg_name = JS_ToCString(ctx, algorithm);
    if (!alg_name)
      return JSRT_CRYPTO_ALG_UNKNOWN;

    for (int i = 0; algorithm_map[i].name != NULL; i++) {
      if (strcmp(alg_name, algorithm_map[i].name) == 0) {
        JS_FreeCString(ctx, alg_name);
        return algorithm_map[i].algorithm;
      }
    }
    JS_FreeCString(ctx, alg_name);
  } else if (JS_IsObject(algorithm)) {
    JSValue name_val = JS_GetPropertyStr(ctx, algorithm, "name");
    if (JS_IsString(name_val)) {
      const char* alg_name = JS_ToCString(ctx, name_val);
      if (alg_name) {
        for (int i = 0; algorithm_map[i].name != NULL; i++) {
          if (strcmp(alg_name, algorithm_map[i].name) == 0) {
            JS_FreeCString(ctx, alg_name);
            JS_FreeValue(ctx, name_val);
            return algorithm_map[i].algorithm;
          }
        }
        JS_FreeCString(ctx, alg_name);
      }
    }
    JS_FreeValue(ctx, name_val);
  }

  return JSRT_CRYPTO_ALG_UNKNOWN;
}

// Convert algorithm enum to string
const char* jsrt_crypto_algorithm_to_string(jsrt_crypto_algorithm_t alg) {
  for (int i = 0; algorithm_map[i].name != NULL; i++) {
    if (algorithm_map[i].algorithm == alg) {
      return algorithm_map[i].name;
    }
  }
  return "Unknown";
}

// Check if algorithm is supported
bool jsrt_crypto_is_algorithm_supported(jsrt_crypto_algorithm_t alg) {
  switch (alg) {
    // Digest algorithms
    case JSRT_CRYPTO_ALG_SHA1:
    case JSRT_CRYPTO_ALG_SHA256:
    case JSRT_CRYPTO_ALG_SHA384:
    case JSRT_CRYPTO_ALG_SHA512:
      return true;

    // Symmetric encryption algorithms
    case JSRT_CRYPTO_ALG_AES_CBC:
      return true;  // Implemented
    case JSRT_CRYPTO_ALG_AES_GCM:
      return true;  // Implemented
    case JSRT_CRYPTO_ALG_AES_CTR:
      return true;  // Implemented

    // Message authentication codes
    case JSRT_CRYPTO_ALG_HMAC:
      return true;  // Implemented

    // Asymmetric encryption algorithms
    case JSRT_CRYPTO_ALG_RSA_OAEP:
      return true;  // Implemented
    case JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5:
      return true;  // Implemented
    case JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5:
      return true;  // Implemented
    case JSRT_CRYPTO_ALG_RSA_PSS:
      return true;  // Fixed OpenSSL 3.x compatibility

    // Elliptic curve algorithms
    case JSRT_CRYPTO_ALG_ECDSA:
      return true;  // Implemented
    case JSRT_CRYPTO_ALG_ECDH:
      return true;  // Implemented

    // Key derivation functions
    case JSRT_CRYPTO_ALG_PBKDF2:
      return true;  // Implemented
    case JSRT_CRYPTO_ALG_HKDF:
      return true;  // Implemented

    default:
      return false;
  }
}

// Throw WebCrypto specific errors
JSValue jsrt_crypto_throw_error(JSContext* ctx, const char* name, const char* message) {
  JSValue error = JS_NewError(ctx);
  JS_SetPropertyStr(ctx, error, "name", JS_NewString(ctx, name));
  JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));
  return error;  // Return error object directly, don't throw here
}

// Memory management functions
JSRTCryptoKey* jsrt_crypto_key_new(void) {
  JSRTCryptoKey* key = malloc(sizeof(JSRTCryptoKey));
  if (key) {
    memset(key, 0, sizeof(JSRTCryptoKey));
  }
  return key;
}

void jsrt_crypto_key_free(JSRTCryptoKey* key) {
  if (key) {
    free(key->algorithm_name);
    free(key->key_type);
    // TODO: Free OpenSSL key structure
    free(key);
  }
}

JSRTCryptoAsyncOperation* jsrt_crypto_async_operation_new(void) {
  JSRTCryptoAsyncOperation* op = malloc(sizeof(JSRTCryptoAsyncOperation));
  if (op) {
    memset(op, 0, sizeof(JSRTCryptoAsyncOperation));
  }
  return op;
}

void jsrt_crypto_async_operation_free(JSRTCryptoAsyncOperation* op) {
  if (op) {
    free(op->input_data);
    free(op->output_data);
    free(op->error_message);
    if (op->op_data.cipher.iv) {
      free(op->op_data.cipher.iv);
    }
    free(op);
  }
}

// Create a resolved Promise with the given value
static JSValue create_resolved_promise(JSContext* ctx, JSValue value) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue promise_ctor = JS_GetPropertyStr(ctx, global, "Promise");
  JS_FreeValue(ctx, global);

  if (JS_IsUndefined(promise_ctor)) {
    // Fallback: return value directly if Promise is not available
    return JS_DupValue(ctx, value);
  }

  JSValue resolve_func = JS_GetPropertyStr(ctx, promise_ctor, "resolve");
  JSValue promise = JS_Call(ctx, resolve_func, promise_ctor, 1, &value);

  JS_FreeValue(ctx, promise_ctor);
  JS_FreeValue(ctx, resolve_func);

  return promise;
}

// Create a rejected Promise with the given error
static JSValue create_rejected_promise(JSContext* ctx, JSValue error) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue promise_ctor = JS_GetPropertyStr(ctx, global, "Promise");
  JS_FreeValue(ctx, global);

  if (JS_IsUndefined(promise_ctor)) {
    // Fallback: throw error directly if Promise is not available
    return JS_Throw(ctx, JS_DupValue(ctx, error));
  }

  JSValue reject_func = JS_GetPropertyStr(ctx, promise_ctor, "reject");
  JSValue promise = JS_Call(ctx, reject_func, promise_ctor, 1, &error);

  JS_FreeValue(ctx, promise_ctor);
  JS_FreeValue(ctx, reject_func);

  return promise;
}

// crypto.subtle.digest implementation - synchronous for now
JSValue jsrt_subtle_digest(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "digest requires 2 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Parse algorithm
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported algorithm");
    return create_rejected_promise(ctx, error);
  }

  if (!jsrt_crypto_is_algorithm_supported(alg)) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm not yet implemented");
    return create_rejected_promise(ctx, error);
  }

  // Get data to hash - handle both ArrayBuffer and TypedArray
  size_t data_size;
  uint8_t* data = NULL;

  // Try ArrayBuffer first
  data = JS_GetArrayBuffer(ctx, &data_size, argv[1]);

  // If not ArrayBuffer, try TypedArray
  if (!data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[1], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[1], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[1], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      // This is a TypedArray
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

      if (buffer_data) {
        uint32_t offset, length;
        JS_ToUint32(ctx, &offset, byteOffset_val);
        JS_ToUint32(ctx, &length, byteLength_val);

        data = buffer_data + offset;
        data_size = length;
      }
    }

    JS_FreeValue(ctx, buffer_val);
    JS_FreeValue(ctx, byteOffset_val);
    JS_FreeValue(ctx, byteLength_val);
  }

  if (!data) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Data must be an ArrayBuffer or TypedArray");
    return create_rejected_promise(ctx, error);
  }

  // Perform digest operation synchronously
  uint8_t* digest_output;
  size_t digest_length;

  int digest_result = jsrt_crypto_digest_data(alg, data, data_size, &digest_output, &digest_length);

  if (digest_result != 0) {
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Digest operation failed");
    return create_rejected_promise(ctx, error);
  }

  // Create result ArrayBuffer
  JSValue result = JS_NewArrayBuffer(ctx, digest_output, digest_length, NULL, NULL, 0);

  // Return resolved promise
  return create_resolved_promise(ctx, result);
}

// crypto.subtle.encrypt implementation
JSValue jsrt_subtle_encrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "encrypt requires 3 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Parse algorithm (first argument)
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported algorithm");
    return create_rejected_promise(ctx, error);
  }

  if (!jsrt_crypto_is_algorithm_supported(alg)) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm not yet implemented");
    return create_rejected_promise(ctx, error);
  }

  // Validate that this is an encryption algorithm
  if (alg != JSRT_CRYPTO_ALG_AES_CBC && alg != JSRT_CRYPTO_ALG_AES_GCM && alg != JSRT_CRYPTO_ALG_AES_CTR &&
      alg != JSRT_CRYPTO_ALG_RSA_OAEP && alg != JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Algorithm not suitable for encryption");
    return create_rejected_promise(ctx, error);
  }

  // Get key data from CryptoKey object (second argument)
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "__keyData");
  if (JS_IsUndefined(key_data_val)) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid CryptoKey object");
    return create_rejected_promise(ctx, error);
  }

  size_t key_data_size;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get plaintext data (third argument) - handle both ArrayBuffer and TypedArray
  size_t plaintext_size;
  uint8_t* plaintext_data = NULL;

  // Try ArrayBuffer first
  plaintext_data = JS_GetArrayBuffer(ctx, &plaintext_size, argv[2]);

  // If not ArrayBuffer, try TypedArray
  if (!plaintext_data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[2], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[2], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[2], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

      if (buffer_data) {
        uint32_t offset, length;
        JS_ToUint32(ctx, &offset, byteOffset_val);
        JS_ToUint32(ctx, &length, byteLength_val);

        plaintext_data = buffer_data + offset;
        plaintext_size = length;
      }
    }

    JS_FreeValue(ctx, buffer_val);
    JS_FreeValue(ctx, byteOffset_val);
    JS_FreeValue(ctx, byteLength_val);
  }

  if (!plaintext_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Data must be an ArrayBuffer or TypedArray");
    return create_rejected_promise(ctx, error);
  }

  // For AES-CBC, get IV from algorithm parameters
  if (alg == JSRT_CRYPTO_ALG_AES_CBC) {
    JSValue iv_val = JS_GetPropertyStr(ctx, argv[0], "iv");
    if (JS_IsUndefined(iv_val)) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "AES-CBC requires IV parameter");
      return create_rejected_promise(ctx, error);
    }

    size_t iv_size;
    uint8_t* iv_data = NULL;

    // Try ArrayBuffer first
    iv_data = JS_GetArrayBuffer(ctx, &iv_size, iv_val);

    // If not ArrayBuffer, try TypedArray
    if (!iv_data) {
      JSValue iv_buffer_val = JS_GetPropertyStr(ctx, iv_val, "buffer");
      JSValue iv_byteOffset_val = JS_GetPropertyStr(ctx, iv_val, "byteOffset");
      JSValue iv_byteLength_val = JS_GetPropertyStr(ctx, iv_val, "byteLength");

      if (!JS_IsUndefined(iv_buffer_val) && !JS_IsUndefined(iv_byteOffset_val) && !JS_IsUndefined(iv_byteLength_val)) {
        size_t buffer_size;
        uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, iv_buffer_val);

        if (buffer_data) {
          uint32_t offset, length;
          JS_ToUint32(ctx, &offset, iv_byteOffset_val);
          JS_ToUint32(ctx, &length, iv_byteLength_val);

          iv_data = buffer_data + offset;
          iv_size = length;
        }
      }

      JS_FreeValue(ctx, iv_buffer_val);
      JS_FreeValue(ctx, iv_byteOffset_val);
      JS_FreeValue(ctx, iv_byteLength_val);
    }

    if (!iv_data || iv_size != JSRT_AES_CBC_IV_SIZE) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid IV for AES-CBC");
      return create_rejected_promise(ctx, error);
    }

    // Set up encryption parameters
    jsrt_symmetric_params_t* params = malloc(sizeof(jsrt_symmetric_params_t));
    if (!params) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
      return create_rejected_promise(ctx, error);
    }

    params->algorithm = JSRT_SYMMETRIC_AES_CBC;
    params->key_data = key_data;
    params->key_length = key_data_size;
    params->params.cbc.iv = iv_data;
    params->params.cbc.iv_length = iv_size;

    // Perform encryption
    uint8_t* ciphertext_data;
    size_t ciphertext_size;
    int result = jsrt_crypto_aes_encrypt(params, plaintext_data, plaintext_size, &ciphertext_data, &ciphertext_size);

    // Clean up
    free(params);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeValue(ctx, iv_val);

    if (result != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Encryption operation failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, ciphertext_data, ciphertext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // For AES-GCM, get IV and optional additionalData from algorithm parameters
  if (alg == JSRT_CRYPTO_ALG_AES_GCM) {
    JSValue iv_val = JS_GetPropertyStr(ctx, argv[0], "iv");
    if (JS_IsUndefined(iv_val)) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "AES-GCM requires IV parameter");
      return create_rejected_promise(ctx, error);
    }

    size_t iv_size;
    uint8_t* iv_data = NULL;

    // Try ArrayBuffer first
    iv_data = JS_GetArrayBuffer(ctx, &iv_size, iv_val);

    // If not ArrayBuffer, try TypedArray
    if (!iv_data) {
      JSValue iv_buffer_val = JS_GetPropertyStr(ctx, iv_val, "buffer");
      JSValue iv_byteOffset_val = JS_GetPropertyStr(ctx, iv_val, "byteOffset");
      JSValue iv_byteLength_val = JS_GetPropertyStr(ctx, iv_val, "byteLength");

      if (!JS_IsUndefined(iv_buffer_val) && !JS_IsUndefined(iv_byteOffset_val) && !JS_IsUndefined(iv_byteLength_val)) {
        size_t buffer_size;
        uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, iv_buffer_val);

        if (buffer_data) {
          uint32_t offset, length;
          JS_ToUint32(ctx, &offset, iv_byteOffset_val);
          JS_ToUint32(ctx, &length, iv_byteLength_val);

          iv_data = buffer_data + offset;
          iv_size = length;
        }
      }

      JS_FreeValue(ctx, iv_buffer_val);
      JS_FreeValue(ctx, iv_byteOffset_val);
      JS_FreeValue(ctx, iv_byteLength_val);
    }

    if (!iv_data) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid IV for AES-GCM");
      return create_rejected_promise(ctx, error);
    }

    // Get optional additionalData
    JSValue additionalData_val = JS_GetPropertyStr(ctx, argv[0], "additionalData");
    uint8_t* additional_data = NULL;
    size_t additional_data_size = 0;

    if (!JS_IsUndefined(additionalData_val) && !JS_IsNull(additionalData_val)) {
      // Try ArrayBuffer first
      additional_data = JS_GetArrayBuffer(ctx, &additional_data_size, additionalData_val);

      // If not ArrayBuffer, try TypedArray
      if (!additional_data) {
        JSValue aad_buffer_val = JS_GetPropertyStr(ctx, additionalData_val, "buffer");
        JSValue aad_byteOffset_val = JS_GetPropertyStr(ctx, additionalData_val, "byteOffset");
        JSValue aad_byteLength_val = JS_GetPropertyStr(ctx, additionalData_val, "byteLength");

        if (!JS_IsUndefined(aad_buffer_val) && !JS_IsUndefined(aad_byteOffset_val) &&
            !JS_IsUndefined(aad_byteLength_val)) {
          size_t buffer_size;
          uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, aad_buffer_val);

          if (buffer_data) {
            uint32_t offset, length;
            JS_ToUint32(ctx, &offset, aad_byteOffset_val);
            JS_ToUint32(ctx, &length, aad_byteLength_val);

            additional_data = buffer_data + offset;
            additional_data_size = length;
          }
        }

        JS_FreeValue(ctx, aad_buffer_val);
        JS_FreeValue(ctx, aad_byteOffset_val);
        JS_FreeValue(ctx, aad_byteLength_val);
      }
    }

    // Get optional tagLength (default to 16 bytes)
    JSValue tagLength_val = JS_GetPropertyStr(ctx, argv[0], "tagLength");
    uint32_t tag_length = JSRT_GCM_TAG_SIZE;  // Default 16 bytes
    if (!JS_IsUndefined(tagLength_val)) {
      JS_ToUint32(ctx, &tag_length, tagLength_val);
      if (tag_length < 12 || tag_length > 16 || tag_length % 4 != 0) {
        tag_length = JSRT_GCM_TAG_SIZE;  // Use default if invalid
      }
    }

    // Set up encryption parameters
    jsrt_symmetric_params_t* params = malloc(sizeof(jsrt_symmetric_params_t));
    if (!params) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JS_FreeValue(ctx, additionalData_val);
      JS_FreeValue(ctx, tagLength_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
      return create_rejected_promise(ctx, error);
    }

    params->algorithm = JSRT_SYMMETRIC_AES_GCM;
    params->key_data = key_data;
    params->key_length = key_data_size;
    params->params.gcm.iv = iv_data;
    params->params.gcm.iv_length = iv_size;
    params->params.gcm.additional_data = additional_data;
    params->params.gcm.additional_data_length = additional_data_size;
    params->params.gcm.tag_length = tag_length;

    // Perform encryption
    uint8_t* ciphertext_data;
    size_t ciphertext_size;
    int result = jsrt_crypto_aes_encrypt(params, plaintext_data, plaintext_size, &ciphertext_data, &ciphertext_size);

    // Clean up
    free(params);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeValue(ctx, iv_val);
    JS_FreeValue(ctx, additionalData_val);
    JS_FreeValue(ctx, tagLength_val);

    if (result != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Encryption operation failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, ciphertext_data, ciphertext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // For AES-CTR, get counter from algorithm parameters
  if (alg == JSRT_CRYPTO_ALG_AES_CTR) {
    JSValue counter_val = JS_GetPropertyStr(ctx, argv[0], "counter");
    if (JS_IsUndefined(counter_val)) {
      JS_FreeValue(ctx, counter_val);
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "AES-CTR requires counter parameter");
      return create_rejected_promise(ctx, error);
    }

    // Get counter data
    size_t counter_size;
    uint8_t* counter_data = JS_GetArrayBuffer(ctx, &counter_size, counter_val);
    if (!counter_data) {
      // Try TypedArray
      JSValue counter_buffer_val = JS_GetPropertyStr(ctx, counter_val, "buffer");
      if (!JS_IsUndefined(counter_buffer_val)) {
        size_t buffer_size;
        uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, counter_buffer_val);
        if (buffer_data) {
          JSValue offset_val = JS_GetPropertyStr(ctx, counter_val, "byteOffset");
          JSValue length_val = JS_GetPropertyStr(ctx, counter_val, "byteLength");
          uint32_t offset = 0, length = 0;
          JS_ToUint32(ctx, &offset, offset_val);
          JS_ToUint32(ctx, &length, length_val);
          counter_data = buffer_data + offset;
          counter_size = length;
          JS_FreeValue(ctx, offset_val);
          JS_FreeValue(ctx, length_val);
        }
        JS_FreeValue(ctx, counter_buffer_val);
      }
    }

    if (!counter_data) {
      JS_FreeValue(ctx, counter_val);
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid counter parameter");
      return create_rejected_promise(ctx, error);
    }

    // Validate counter size (should be 16 bytes for AES)
    if (counter_size != 16) {
      JS_FreeValue(ctx, counter_val);
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Counter must be 16 bytes");
      return create_rejected_promise(ctx, error);
    }

    // Get optional length parameter (counter length in bits)
    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    uint32_t counter_length_bits = 64;  // Default counter length
    if (!JS_IsUndefined(length_val)) {
      JS_ToUint32(ctx, &counter_length_bits, length_val);
    }
    JS_FreeValue(ctx, length_val);

    // Set up symmetric encryption parameters
    jsrt_symmetric_params_t* params = malloc(sizeof(jsrt_symmetric_params_t));
    params->algorithm = JSRT_SYMMETRIC_AES_CTR;
    params->key_data = key_data;
    params->key_length = key_data_size;
    params->params.ctr.counter = counter_data;
    params->params.ctr.counter_length = counter_size;
    params->params.ctr.length = counter_length_bits;

    // Perform encryption
    uint8_t* ciphertext_data;
    size_t ciphertext_size;
    int result = jsrt_crypto_aes_encrypt(params, plaintext_data, plaintext_size, &ciphertext_data, &ciphertext_size);

    // Clean up
    free(params);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeValue(ctx, counter_val);

    if (result != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Encryption operation failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, ciphertext_data, ciphertext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // RSA-OAEP encryption
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP) {
    // Get hash algorithm from algorithm object - can be either string or object with name property
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    const char* hash_name = NULL;

    if (JS_IsString(hash_val)) {
      // Hash is a string like "SHA-256"
      hash_name = JS_ToCString(ctx, hash_val);
    } else {
      // Hash is an object with name property
      JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
      hash_name = JS_ToCString(ctx, hash_name_val);
    }

    jsrt_rsa_hash_algorithm_t hash_alg = jsrt_crypto_parse_rsa_hash_algorithm(hash_name);
    if (hash_alg == JSRT_RSA_HASH_SHA1) {  // Use enum comparison instead of invalid value
      JS_FreeCString(ctx, hash_name);
      JS_FreeValue(ctx, hash_val);
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported hash algorithm");
      return create_rejected_promise(ctx, error);
    }

    // Get optional label parameter
    JSValue label_val = JS_GetPropertyStr(ctx, argv[0], "label");
    uint8_t* label_data = NULL;
    size_t label_size = 0;

    if (!JS_IsUndefined(label_val) && !JS_IsNull(label_val)) {
      label_data = JS_GetArrayBuffer(ctx, &label_size, label_val);
      if (!label_data) {
        // Try TypedArray
        JSValue label_buffer_val = JS_GetPropertyStr(ctx, label_val, "buffer");
        if (!JS_IsUndefined(label_buffer_val)) {
          size_t buffer_size;
          uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, label_buffer_val);
          if (buffer_data) {
            JSValue offset_val = JS_GetPropertyStr(ctx, label_val, "byteOffset");
            JSValue length_val = JS_GetPropertyStr(ctx, label_val, "byteLength");
            uint32_t offset = 0, length = 0;
            JS_ToUint32(ctx, &offset, offset_val);
            JS_ToUint32(ctx, &length, length_val);
            label_data = buffer_data + offset;
            label_size = length;
            JS_FreeValue(ctx, offset_val);
            JS_FreeValue(ctx, length_val);
          }
          JS_FreeValue(ctx, label_buffer_val);
        }
      }
    }

    // Get key type to determine if it's public or private key
    JSValue key_type_val = JS_GetPropertyStr(ctx, argv[1], "type");
    const char* key_type = JS_ToCString(ctx, key_type_val);

    // Create EVP_PKEY from stored DER data
    void* rsa_key = NULL;
    if (key_type && strcmp(key_type, "public") == 0) {
      rsa_key = jsrt_crypto_rsa_create_public_key_from_der(key_data, key_data_size);
    } else if (key_type && strcmp(key_type, "private") == 0) {
      rsa_key = jsrt_crypto_rsa_create_private_key_from_der(key_data, key_data_size);
    }

    JS_FreeCString(ctx, key_type);
    JS_FreeValue(ctx, key_type_val);

    if (!rsa_key) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to load RSA key");
      return create_rejected_promise(ctx, error);
    }

    // Set up RSA parameters
    jsrt_rsa_params_t params = {0};
    params.algorithm = JSRT_RSA_OAEP;
    params.hash_algorithm = hash_alg;
    params.rsa_key = rsa_key;  // Use the actual EVP_PKEY
    params.params.oaep.label = label_data;
    params.params.oaep.label_length = label_size;

    // Perform RSA encryption
    uint8_t* ciphertext_data = NULL;
    size_t ciphertext_size = 0;
    int result = jsrt_crypto_rsa_encrypt(&params, plaintext_data, plaintext_size, &ciphertext_data, &ciphertext_size);

    // Cleanup
    JS_FreeCString(ctx, hash_name);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, label_val);
    JS_FreeValue(ctx, key_data_val);

    // Free the created EVP_PKEY
    if (rsa_key) {
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
        if (EVP_PKEY_free) {
          EVP_PKEY_free(rsa_key);
        }
      }
    }

    if (result != 0 || !ciphertext_data) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "RSA encryption failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, ciphertext_data, ciphertext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // RSA-PKCS1-v1_5 encryption
  if (alg == JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    // Create EVP_PKEY from DER-encoded key data
    void* rsa_key = jsrt_crypto_rsa_create_public_key_from_der(key_data, key_data_size);
    if (!rsa_key) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid RSA public key");
      return create_rejected_promise(ctx, error);
    }

    // Setup RSA parameters for PKCS1 v1.5
    jsrt_rsa_params_t params;
    params.algorithm = JSRT_RSA_PKCS1_V1_5;
    params.hash_algorithm = JSRT_RSA_HASH_SHA256;  // Default hash (not used for PKCS1 v1.5 encryption)
    params.rsa_key = rsa_key;

    // Perform RSA encryption
    uint8_t* ciphertext_data = NULL;
    size_t ciphertext_size = 0;
    int result = jsrt_crypto_rsa_encrypt(&params, plaintext_data, plaintext_size, &ciphertext_data, &ciphertext_size);

    // Cleanup
    JS_FreeValue(ctx, key_data_val);

    // Free the created EVP_PKEY
    if (rsa_key) {
#ifdef _WIN32
      extern HMODULE openssl_handle;
#else
      extern void* openssl_handle;
#endif
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
        if (EVP_PKEY_free) {
          EVP_PKEY_free(rsa_key);
        }
      }
    }

    if (result != 0 || !ciphertext_data) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "RSA-PKCS1-v1_5 encryption failed");
      return create_rejected_promise(ctx, error);
    }

    // Return the ciphertext as ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, ciphertext_data, ciphertext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  JS_FreeValue(ctx, key_data_val);
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm mode not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_decrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "decrypt requires 3 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Parse algorithm (first argument)
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported algorithm");
    return create_rejected_promise(ctx, error);
  }

  if (!jsrt_crypto_is_algorithm_supported(alg)) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm not yet implemented");
    return create_rejected_promise(ctx, error);
  }

  // Validate that this is an encryption algorithm
  if (alg != JSRT_CRYPTO_ALG_AES_CBC && alg != JSRT_CRYPTO_ALG_AES_GCM && alg != JSRT_CRYPTO_ALG_AES_CTR &&
      alg != JSRT_CRYPTO_ALG_RSA_OAEP && alg != JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Algorithm not suitable for decryption");
    return create_rejected_promise(ctx, error);
  }

  // Get key data from CryptoKey object (second argument)
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "__keyData");
  if (JS_IsUndefined(key_data_val)) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid CryptoKey object");
    return create_rejected_promise(ctx, error);
  }

  size_t key_data_size;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get ciphertext data (third argument) - handle both ArrayBuffer and TypedArray
  size_t ciphertext_size;
  uint8_t* ciphertext_data = NULL;

  // Try ArrayBuffer first
  ciphertext_data = JS_GetArrayBuffer(ctx, &ciphertext_size, argv[2]);

  // If not ArrayBuffer, try TypedArray
  if (!ciphertext_data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[2], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[2], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[2], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

      if (buffer_data) {
        uint32_t offset, length;
        JS_ToUint32(ctx, &offset, byteOffset_val);
        JS_ToUint32(ctx, &length, byteLength_val);

        ciphertext_data = buffer_data + offset;
        ciphertext_size = length;
      }
    }

    JS_FreeValue(ctx, buffer_val);
    JS_FreeValue(ctx, byteOffset_val);
    JS_FreeValue(ctx, byteLength_val);
  }

  if (!ciphertext_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Data must be an ArrayBuffer or TypedArray");
    return create_rejected_promise(ctx, error);
  }

  // For AES-CBC, get IV from algorithm parameters
  if (alg == JSRT_CRYPTO_ALG_AES_CBC) {
    JSValue iv_val = JS_GetPropertyStr(ctx, argv[0], "iv");
    if (JS_IsUndefined(iv_val)) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "AES-CBC requires IV parameter");
      return create_rejected_promise(ctx, error);
    }

    size_t iv_size;
    uint8_t* iv_data = NULL;

    // Try ArrayBuffer first
    iv_data = JS_GetArrayBuffer(ctx, &iv_size, iv_val);

    // If not ArrayBuffer, try TypedArray
    if (!iv_data) {
      JSValue iv_buffer_val = JS_GetPropertyStr(ctx, iv_val, "buffer");
      JSValue iv_byteOffset_val = JS_GetPropertyStr(ctx, iv_val, "byteOffset");
      JSValue iv_byteLength_val = JS_GetPropertyStr(ctx, iv_val, "byteLength");

      if (!JS_IsUndefined(iv_buffer_val) && !JS_IsUndefined(iv_byteOffset_val) && !JS_IsUndefined(iv_byteLength_val)) {
        size_t buffer_size;
        uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, iv_buffer_val);

        if (buffer_data) {
          uint32_t offset, length;
          JS_ToUint32(ctx, &offset, iv_byteOffset_val);
          JS_ToUint32(ctx, &length, iv_byteLength_val);

          iv_data = buffer_data + offset;
          iv_size = length;
        }
      }

      JS_FreeValue(ctx, iv_buffer_val);
      JS_FreeValue(ctx, iv_byteOffset_val);
      JS_FreeValue(ctx, iv_byteLength_val);
    }

    if (!iv_data || iv_size != JSRT_AES_CBC_IV_SIZE) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid IV for AES-CBC");
      return create_rejected_promise(ctx, error);
    }

    // Set up decryption parameters
    jsrt_symmetric_params_t* params = malloc(sizeof(jsrt_symmetric_params_t));
    if (!params) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
      return create_rejected_promise(ctx, error);
    }

    params->algorithm = JSRT_SYMMETRIC_AES_CBC;
    params->key_data = key_data;
    params->key_length = key_data_size;
    params->params.cbc.iv = iv_data;
    params->params.cbc.iv_length = iv_size;

    // Perform decryption
    uint8_t* plaintext_data;
    size_t plaintext_size;
    int result = jsrt_crypto_aes_decrypt(params, ciphertext_data, ciphertext_size, &plaintext_data, &plaintext_size);

    // Clean up
    free(params);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeValue(ctx, iv_val);

    if (result != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Decryption operation failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, plaintext_data, plaintext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // For AES-GCM, get IV and optional additionalData from algorithm parameters
  if (alg == JSRT_CRYPTO_ALG_AES_GCM) {
    JSValue iv_val = JS_GetPropertyStr(ctx, argv[0], "iv");
    if (JS_IsUndefined(iv_val)) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "AES-GCM requires IV parameter");
      return create_rejected_promise(ctx, error);
    }

    size_t iv_size;
    uint8_t* iv_data = NULL;

    // Try ArrayBuffer first
    iv_data = JS_GetArrayBuffer(ctx, &iv_size, iv_val);

    // If not ArrayBuffer, try TypedArray
    if (!iv_data) {
      JSValue iv_buffer_val = JS_GetPropertyStr(ctx, iv_val, "buffer");
      JSValue iv_byteOffset_val = JS_GetPropertyStr(ctx, iv_val, "byteOffset");
      JSValue iv_byteLength_val = JS_GetPropertyStr(ctx, iv_val, "byteLength");

      if (!JS_IsUndefined(iv_buffer_val) && !JS_IsUndefined(iv_byteOffset_val) && !JS_IsUndefined(iv_byteLength_val)) {
        size_t buffer_size;
        uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, iv_buffer_val);

        if (buffer_data) {
          uint32_t offset, length;
          JS_ToUint32(ctx, &offset, iv_byteOffset_val);
          JS_ToUint32(ctx, &length, iv_byteLength_val);

          iv_data = buffer_data + offset;
          iv_size = length;
        }
      }

      JS_FreeValue(ctx, iv_buffer_val);
      JS_FreeValue(ctx, iv_byteOffset_val);
      JS_FreeValue(ctx, iv_byteLength_val);
    }

    if (!iv_data) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid IV for AES-GCM");
      return create_rejected_promise(ctx, error);
    }

    // Get optional additionalData
    JSValue additionalData_val = JS_GetPropertyStr(ctx, argv[0], "additionalData");
    uint8_t* additional_data = NULL;
    size_t additional_data_size = 0;

    if (!JS_IsUndefined(additionalData_val) && !JS_IsNull(additionalData_val)) {
      // Try ArrayBuffer first
      additional_data = JS_GetArrayBuffer(ctx, &additional_data_size, additionalData_val);

      // If not ArrayBuffer, try TypedArray
      if (!additional_data) {
        JSValue aad_buffer_val = JS_GetPropertyStr(ctx, additionalData_val, "buffer");
        JSValue aad_byteOffset_val = JS_GetPropertyStr(ctx, additionalData_val, "byteOffset");
        JSValue aad_byteLength_val = JS_GetPropertyStr(ctx, additionalData_val, "byteLength");

        if (!JS_IsUndefined(aad_buffer_val) && !JS_IsUndefined(aad_byteOffset_val) &&
            !JS_IsUndefined(aad_byteLength_val)) {
          size_t buffer_size;
          uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, aad_buffer_val);

          if (buffer_data) {
            uint32_t offset, length;
            JS_ToUint32(ctx, &offset, aad_byteOffset_val);
            JS_ToUint32(ctx, &length, aad_byteLength_val);

            additional_data = buffer_data + offset;
            additional_data_size = length;
          }
        }

        JS_FreeValue(ctx, aad_buffer_val);
        JS_FreeValue(ctx, aad_byteOffset_val);
        JS_FreeValue(ctx, aad_byteLength_val);
      }
    }

    // Get optional tagLength (default to 16 bytes)
    JSValue tagLength_val = JS_GetPropertyStr(ctx, argv[0], "tagLength");
    uint32_t tag_length = JSRT_GCM_TAG_SIZE;  // Default 16 bytes
    if (!JS_IsUndefined(tagLength_val)) {
      JS_ToUint32(ctx, &tag_length, tagLength_val);
      if (tag_length < 12 || tag_length > 16 || tag_length % 4 != 0) {
        tag_length = JSRT_GCM_TAG_SIZE;  // Use default if invalid
      }
    }

    // Set up decryption parameters
    jsrt_symmetric_params_t* params = malloc(sizeof(jsrt_symmetric_params_t));
    if (!params) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, iv_val);
      JS_FreeValue(ctx, additionalData_val);
      JS_FreeValue(ctx, tagLength_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
      return create_rejected_promise(ctx, error);
    }

    params->algorithm = JSRT_SYMMETRIC_AES_GCM;
    params->key_data = key_data;
    params->key_length = key_data_size;
    params->params.gcm.iv = iv_data;
    params->params.gcm.iv_length = iv_size;
    params->params.gcm.additional_data = additional_data;
    params->params.gcm.additional_data_length = additional_data_size;
    params->params.gcm.tag_length = tag_length;

    // Perform decryption
    uint8_t* plaintext_data;
    size_t plaintext_size;
    int result = jsrt_crypto_aes_decrypt(params, ciphertext_data, ciphertext_size, &plaintext_data, &plaintext_size);

    // Clean up
    free(params);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeValue(ctx, iv_val);
    JS_FreeValue(ctx, additionalData_val);
    JS_FreeValue(ctx, tagLength_val);

    if (result != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Decryption operation failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, plaintext_data, plaintext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // For AES-CTR, get counter from algorithm parameters
  if (alg == JSRT_CRYPTO_ALG_AES_CTR) {
    JSValue counter_val = JS_GetPropertyStr(ctx, argv[0], "counter");
    if (JS_IsUndefined(counter_val)) {
      JS_FreeValue(ctx, counter_val);
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "AES-CTR requires counter parameter");
      return create_rejected_promise(ctx, error);
    }

    // Get counter data
    size_t counter_size;
    uint8_t* counter_data = JS_GetArrayBuffer(ctx, &counter_size, counter_val);
    if (!counter_data) {
      // Try TypedArray
      JSValue counter_buffer_val = JS_GetPropertyStr(ctx, counter_val, "buffer");
      if (!JS_IsUndefined(counter_buffer_val)) {
        size_t buffer_size;
        uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, counter_buffer_val);
        if (buffer_data) {
          JSValue offset_val = JS_GetPropertyStr(ctx, counter_val, "byteOffset");
          JSValue length_val = JS_GetPropertyStr(ctx, counter_val, "byteLength");
          uint32_t offset = 0, length = 0;
          JS_ToUint32(ctx, &offset, offset_val);
          JS_ToUint32(ctx, &length, length_val);
          counter_data = buffer_data + offset;
          counter_size = length;
          JS_FreeValue(ctx, offset_val);
          JS_FreeValue(ctx, length_val);
        }
        JS_FreeValue(ctx, counter_buffer_val);
      }
    }

    if (!counter_data) {
      JS_FreeValue(ctx, counter_val);
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid counter parameter");
      return create_rejected_promise(ctx, error);
    }

    // Validate counter size (should be 16 bytes for AES)
    if (counter_size != 16) {
      JS_FreeValue(ctx, counter_val);
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Counter must be 16 bytes");
      return create_rejected_promise(ctx, error);
    }

    // Get optional length parameter (counter length in bits)
    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    uint32_t counter_length_bits = 64;  // Default counter length
    if (!JS_IsUndefined(length_val)) {
      JS_ToUint32(ctx, &counter_length_bits, length_val);
    }
    JS_FreeValue(ctx, length_val);

    // Set up symmetric decryption parameters
    jsrt_symmetric_params_t* params = malloc(sizeof(jsrt_symmetric_params_t));
    params->algorithm = JSRT_SYMMETRIC_AES_CTR;
    params->key_data = key_data;
    params->key_length = key_data_size;
    params->params.ctr.counter = counter_data;
    params->params.ctr.counter_length = counter_size;
    params->params.ctr.length = counter_length_bits;

    // Perform decryption
    uint8_t* plaintext_data;
    size_t plaintext_size;
    int result = jsrt_crypto_aes_decrypt(params, ciphertext_data, ciphertext_size, &plaintext_data, &plaintext_size);

    // Clean up
    free(params);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeValue(ctx, counter_val);

    if (result != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Decryption operation failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, plaintext_data, plaintext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // RSA-OAEP decryption
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP) {
    // Get hash algorithm from algorithm object - can be either string or object with name property
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    const char* hash_name = NULL;

    if (JS_IsString(hash_val)) {
      // Hash is a string like "SHA-256"
      hash_name = JS_ToCString(ctx, hash_val);
    } else {
      // Hash is an object with name property
      JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
      hash_name = JS_ToCString(ctx, hash_name_val);
    }

    jsrt_rsa_hash_algorithm_t hash_alg = jsrt_crypto_parse_rsa_hash_algorithm(hash_name);
    if (hash_alg == JSRT_RSA_HASH_SHA1) {  // Use enum comparison instead of invalid value
      JS_FreeCString(ctx, hash_name);
      JS_FreeValue(ctx, hash_val);
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported hash algorithm");
      return create_rejected_promise(ctx, error);
    }

    // Get optional label parameter
    JSValue label_val = JS_GetPropertyStr(ctx, argv[0], "label");
    uint8_t* label_data = NULL;
    size_t label_size = 0;

    if (!JS_IsUndefined(label_val) && !JS_IsNull(label_val)) {
      label_data = JS_GetArrayBuffer(ctx, &label_size, label_val);
      if (!label_data) {
        // Try TypedArray
        JSValue label_buffer_val = JS_GetPropertyStr(ctx, label_val, "buffer");
        if (!JS_IsUndefined(label_buffer_val)) {
          size_t buffer_size;
          uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, label_buffer_val);
          if (buffer_data) {
            JSValue offset_val = JS_GetPropertyStr(ctx, label_val, "byteOffset");
            JSValue length_val = JS_GetPropertyStr(ctx, label_val, "byteLength");
            uint32_t offset = 0, length = 0;
            JS_ToUint32(ctx, &offset, offset_val);
            JS_ToUint32(ctx, &length, length_val);
            label_data = buffer_data + offset;
            label_size = length;
            JS_FreeValue(ctx, offset_val);
            JS_FreeValue(ctx, length_val);
          }
          JS_FreeValue(ctx, label_buffer_val);
        }
      }
    }

    // Get key type to determine if it's public or private key
    JSValue key_type_val = JS_GetPropertyStr(ctx, argv[1], "type");
    const char* key_type = JS_ToCString(ctx, key_type_val);

    // Create EVP_PKEY from stored DER data
    void* rsa_key = NULL;
    if (key_type && strcmp(key_type, "public") == 0) {
      rsa_key = jsrt_crypto_rsa_create_public_key_from_der(key_data, key_data_size);
    } else if (key_type && strcmp(key_type, "private") == 0) {
      rsa_key = jsrt_crypto_rsa_create_private_key_from_der(key_data, key_data_size);
    }

    JS_FreeCString(ctx, key_type);
    JS_FreeValue(ctx, key_type_val);

    if (!rsa_key) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to load RSA key");
      return create_rejected_promise(ctx, error);
    }

    // Set up RSA parameters
    jsrt_rsa_params_t params = {0};
    params.algorithm = JSRT_RSA_OAEP;
    params.hash_algorithm = hash_alg;
    params.rsa_key = rsa_key;  // Use the actual EVP_PKEY
    params.params.oaep.label = label_data;
    params.params.oaep.label_length = label_size;

    // Perform RSA decryption
    uint8_t* plaintext_data = NULL;
    size_t plaintext_size = 0;
    int result = jsrt_crypto_rsa_decrypt(&params, ciphertext_data, ciphertext_size, &plaintext_data, &plaintext_size);

    // Cleanup
    JS_FreeCString(ctx, hash_name);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, label_val);
    JS_FreeValue(ctx, key_data_val);

    // Free the created EVP_PKEY
    if (rsa_key) {
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
        if (EVP_PKEY_free) {
          EVP_PKEY_free(rsa_key);
        }
      }
    }

    if (result != 0 || !plaintext_data) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "RSA decryption failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, plaintext_data, plaintext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // RSA-PKCS1-v1_5 decryption
  if (alg == JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    // Create EVP_PKEY from DER-encoded key data
    void* rsa_key = jsrt_crypto_rsa_create_private_key_from_der(key_data, key_data_size);
    if (!rsa_key) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid RSA private key");
      return create_rejected_promise(ctx, error);
    }

    // Setup RSA parameters for PKCS1 v1.5
    jsrt_rsa_params_t params;
    params.algorithm = JSRT_RSA_PKCS1_V1_5;
    params.hash_algorithm = JSRT_RSA_HASH_SHA256;  // Default hash (not used for PKCS1 v1.5 encryption)
    params.rsa_key = rsa_key;

    // Perform RSA decryption
    uint8_t* plaintext_data = NULL;
    size_t plaintext_size = 0;
    int result = jsrt_crypto_rsa_decrypt(&params, ciphertext_data, ciphertext_size, &plaintext_data, &plaintext_size);

    // Cleanup
    JS_FreeValue(ctx, key_data_val);

    // Free the created EVP_PKEY
    if (rsa_key) {
#ifdef _WIN32
      extern HMODULE openssl_handle;
#else
      extern void* openssl_handle;
#endif
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
        if (EVP_PKEY_free) {
          EVP_PKEY_free(rsa_key);
        }
      }
    }

    if (result != 0 || !plaintext_data) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "RSA-PKCS1-v1_5 decryption failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, plaintext_data, plaintext_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  JS_FreeValue(ctx, key_data_val);
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm mode not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_sign(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("JSRT_Crypto_Subtle: jsrt_subtle_sign called with %d arguments", argc);

  if (argc < 3) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "sign requires 3 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Parse algorithm (first argument)
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  JSRT_Debug("JSRT_Crypto_Subtle: Parsed algorithm: %d", alg);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported algorithm");
    return create_rejected_promise(ctx, error);
  }

  if (!jsrt_crypto_is_algorithm_supported(alg)) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm not yet implemented");
    return create_rejected_promise(ctx, error);
  }

  // Validate that this is a signing algorithm
  if (alg != JSRT_CRYPTO_ALG_HMAC && alg != JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5 && alg != JSRT_CRYPTO_ALG_RSA_PSS &&
      alg != JSRT_CRYPTO_ALG_ECDSA) {
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Algorithm not suitable for signing");
    return create_rejected_promise(ctx, error);
  }

  // Get key data from CryptoKey object (second argument)
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "__keyData");
  if (JS_IsUndefined(key_data_val)) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid CryptoKey object");
    return create_rejected_promise(ctx, error);
  }

  size_t key_data_size;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get data to sign - handle both ArrayBuffer and TypedArray
  size_t data_size;
  uint8_t* data = NULL;

  // Try ArrayBuffer first
  data = JS_GetArrayBuffer(ctx, &data_size, argv[2]);

  // If not ArrayBuffer, try TypedArray
  if (!data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[2], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[2], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[2], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

      if (buffer_data) {
        uint32_t offset, length;
        JS_ToUint32(ctx, &offset, byteOffset_val);
        JS_ToUint32(ctx, &length, byteLength_val);

        data = buffer_data + offset;
        data_size = length;
      }
    }

    JS_FreeValue(ctx, buffer_val);
    JS_FreeValue(ctx, byteOffset_val);
    JS_FreeValue(ctx, byteLength_val);
  }

  if (!data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Data must be an ArrayBuffer or TypedArray");
    return create_rejected_promise(ctx, error);
  }

  // For HMAC, get hash algorithm from the key's algorithm
  if (alg == JSRT_CRYPTO_ALG_HMAC) {
    JSValue key_algorithm_val = JS_GetPropertyStr(ctx, argv[1], "algorithm");
    JSValue hash_val = JS_GetPropertyStr(ctx, key_algorithm_val, "hash");

    const char* hash_name = JS_ToCString(ctx, hash_val);
    if (!hash_name) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, key_algorithm_val);
      JS_FreeValue(ctx, hash_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid hash algorithm for HMAC");
      return create_rejected_promise(ctx, error);
    }

    jsrt_hmac_algorithm_t hmac_alg = jsrt_crypto_parse_hmac_algorithm(hash_name);
    JS_FreeCString(ctx, hash_name);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, key_algorithm_val);

    // Set up HMAC parameters
    jsrt_hmac_params_t* params = malloc(sizeof(jsrt_hmac_params_t));
    if (!params) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
      return create_rejected_promise(ctx, error);
    }

    params->algorithm = hmac_alg;
    params->key_data = key_data;
    params->key_length = key_data_size;

    // Perform HMAC signing
    uint8_t* signature_data;
    size_t signature_size;
    int result = jsrt_crypto_hmac_sign(params, data, data_size, &signature_data, &signature_size);

    // Clean up
    free(params);
    JS_FreeValue(ctx, key_data_val);

    if (result != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "HMAC signing failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, signature_data, signature_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // RSA signature
  if (alg == JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5) {
    JSRT_Debug("JSRT_Crypto_Subtle: Starting RSA signature for RSASSA-PKCS1-v1_5");

    // Create RSA key from DER-encoded key data
    void* rsa_private_key = jsrt_crypto_rsa_create_private_key_from_der(key_data, key_data_size);
    if (!rsa_private_key) {
      JSRT_Debug("JSRT_Crypto_Subtle: Failed to create RSA private key from DER data");
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create RSA private key");
      return create_rejected_promise(ctx, error);
    }

    JSRT_Debug("JSRT_Crypto_Subtle: RSA private key created successfully");

    // Set up RSA signature parameters
    jsrt_rsa_params_t params;
    params.algorithm = JSRT_RSASSA_PKCS1_V1_5;
    params.rsa_key = rsa_private_key;
    params.hash_algorithm = JSRT_RSA_HASH_SHA256;  // Default to SHA-256

    // Perform RSA signature
    uint8_t* signature_data = NULL;
    size_t signature_size = 0;
    int sign_result = jsrt_crypto_rsa_sign(&params, data, data_size, &signature_data, &signature_size);

    // Cleanup the RSA key
    if (rsa_private_key) {
      // Note: Need to add proper OpenSSL EVP_PKEY_free here

      if (openssl_handle) {
        void (*EVP_PKEY_free)(void* pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
        if (EVP_PKEY_free) {
          EVP_PKEY_free(rsa_private_key);
        }
      }
    }

    JS_FreeValue(ctx, key_data_val);

    if (sign_result != 0 || !signature_data) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "RSA signature failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, signature_data, signature_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // RSA-PSS signature
  if (alg == JSRT_CRYPTO_ALG_RSA_PSS) {
    JSRT_Debug("JSRT_Crypto_Subtle: Starting RSA signature for RSA-PSS");

    // Create RSA key from DER-encoded key data
    void* rsa_private_key = jsrt_crypto_rsa_create_private_key_from_der(key_data, key_data_size);
    if (!rsa_private_key) {
      JSRT_Debug("JSRT_Crypto_Subtle: Failed to create RSA private key from DER data");
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create RSA private key");
      return create_rejected_promise(ctx, error);
    }

    JSRT_Debug("JSRT_Crypto_Subtle: RSA private key created successfully");

    // Set up RSA-PSS signature parameters
    jsrt_rsa_params_t params;
    params.algorithm = JSRT_RSA_PSS;
    params.rsa_key = rsa_private_key;
    params.hash_algorithm = JSRT_RSA_HASH_SHA256;  // Default to SHA-256

    // Parse salt length from algorithm if provided
    params.params.pss.salt_length = 0;  // Default to digest length
    JSValue salt_length_val = JS_GetPropertyStr(ctx, argv[0], "saltLength");
    if (!JS_IsUndefined(salt_length_val)) {
      uint32_t salt_length;
      if (JS_ToUint32(ctx, &salt_length, salt_length_val) == 0) {
        params.params.pss.salt_length = salt_length;
        JSRT_Debug("JSRT_Crypto_Subtle: RSA-PSS salt_length=%zu", params.params.pss.salt_length);
      }
    }
    JS_FreeValue(ctx, salt_length_val);

    // Perform RSA-PSS signature
    uint8_t* signature_data = NULL;
    size_t signature_size = 0;
    int sign_result = jsrt_crypto_rsa_sign(&params, data, data_size, &signature_data, &signature_size);

    // Cleanup the RSA key
    if (rsa_private_key) {
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void* pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
        if (EVP_PKEY_free) {
          EVP_PKEY_free(rsa_private_key);
        }
      }
    }

    JS_FreeValue(ctx, key_data_val);

    if (sign_result != 0 || !signature_data) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "RSA-PSS signature failed");
      return create_rejected_promise(ctx, error);
    }

    // Create result ArrayBuffer
    JSValue result_buffer = JS_NewArrayBuffer(ctx, signature_data, signature_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result_buffer);
  }

  // ECDSA signature
  if (alg == JSRT_CRYPTO_ALG_ECDSA) {
    JSRT_Debug("JSRT_Crypto_Subtle: Starting ECDSA signature");

    // Create EC key from DER-encoded key data
    const unsigned char* key_ptr = key_data;
    void* ec_private_key = NULL;

    if (openssl_handle) {
      void* (*d2i_AutoPrivateKey)(void** a, const unsigned char** pp, long length) =
          JSRT_DLSYM(openssl_handle, "d2i_AutoPrivateKey");
      if (d2i_AutoPrivateKey) {
        ec_private_key = d2i_AutoPrivateKey(NULL, &key_ptr, key_data_size);
      }
    }

    if (!ec_private_key) {
      JSRT_Debug("JSRT_Crypto_Subtle: Failed to create EC private key from DER data");
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create EC private key");
      return create_rejected_promise(ctx, error);
    }

    // Get hash algorithm from parameters
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    const char* hash_name = JS_ToCString(ctx, hash_val);
    if (!hash_name) {
      hash_name = "SHA-256";  // Default
    }

    // Setup ECDSA parameters
    jsrt_ecdsa_sign_params_t ecdsa_params;
    ecdsa_params.hash = hash_name;

    // Perform ECDSA signature
    JSValue signature_result = jsrt_ec_sign(ctx, ec_private_key, data, data_size, &ecdsa_params);

    // Cleanup
    if (hash_name && strcmp(hash_name, "SHA-256") != 0) {
      JS_FreeCString(ctx, hash_name);
    }
    JS_FreeValue(ctx, hash_val);

    // Free the EC key
    if (ec_private_key) {
      jsrt_evp_pkey_free_wrapper(ec_private_key);
    }

    JS_FreeValue(ctx, key_data_val);

    if (JS_IsException(signature_result)) {
      return create_rejected_promise(ctx, signature_result);
    }

    return create_resolved_promise(ctx, signature_result);
  }

  JS_FreeValue(ctx, key_data_val);
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm mode not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_verify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 4) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "verify requires 4 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Parse algorithm (first argument)
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported algorithm");
    return create_rejected_promise(ctx, error);
  }

  if (!jsrt_crypto_is_algorithm_supported(alg)) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm not yet implemented");
    return create_rejected_promise(ctx, error);
  }

  // Validate that this is a verification algorithm
  if (alg != JSRT_CRYPTO_ALG_HMAC && alg != JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5 && alg != JSRT_CRYPTO_ALG_RSA_PSS &&
      alg != JSRT_CRYPTO_ALG_ECDSA) {
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Algorithm not suitable for verification");
    return create_rejected_promise(ctx, error);
  }

  // Get key data from CryptoKey object (second argument)
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "__keyData");
  if (JS_IsUndefined(key_data_val)) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid CryptoKey object");
    return create_rejected_promise(ctx, error);
  }

  size_t key_data_size;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get signature to verify - handle both ArrayBuffer and TypedArray (third argument)
  size_t signature_size;
  uint8_t* signature_data = NULL;

  // Try ArrayBuffer first
  signature_data = JS_GetArrayBuffer(ctx, &signature_size, argv[2]);

  // If not ArrayBuffer, try TypedArray
  if (!signature_data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[2], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[2], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[2], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

      if (buffer_data) {
        uint32_t offset, length;
        JS_ToUint32(ctx, &offset, byteOffset_val);
        JS_ToUint32(ctx, &length, byteLength_val);

        signature_data = buffer_data + offset;
        signature_size = length;
      }
    }

    JS_FreeValue(ctx, buffer_val);
    JS_FreeValue(ctx, byteOffset_val);
    JS_FreeValue(ctx, byteLength_val);
  }

  if (!signature_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Signature must be an ArrayBuffer or TypedArray");
    return create_rejected_promise(ctx, error);
  }

  // Get data to verify - handle both ArrayBuffer and TypedArray (fourth argument)
  size_t data_size;
  uint8_t* data = NULL;

  // Try ArrayBuffer first
  data = JS_GetArrayBuffer(ctx, &data_size, argv[3]);

  // If not ArrayBuffer, try TypedArray
  if (!data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[3], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[3], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[3], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

      if (buffer_data) {
        uint32_t offset, length;
        JS_ToUint32(ctx, &offset, byteOffset_val);
        JS_ToUint32(ctx, &length, byteLength_val);

        data = buffer_data + offset;
        data_size = length;
      }
    }

    JS_FreeValue(ctx, buffer_val);
    JS_FreeValue(ctx, byteOffset_val);
    JS_FreeValue(ctx, byteLength_val);
  }

  if (!data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Data must be an ArrayBuffer or TypedArray");
    return create_rejected_promise(ctx, error);
  }

  // For HMAC, get hash algorithm from the key's algorithm
  if (alg == JSRT_CRYPTO_ALG_HMAC) {
    JSValue key_algorithm_val = JS_GetPropertyStr(ctx, argv[1], "algorithm");
    JSValue hash_val = JS_GetPropertyStr(ctx, key_algorithm_val, "hash");

    const char* hash_name = JS_ToCString(ctx, hash_val);
    if (!hash_name) {
      JS_FreeValue(ctx, key_data_val);
      JS_FreeValue(ctx, key_algorithm_val);
      JS_FreeValue(ctx, hash_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid hash algorithm for HMAC");
      return create_rejected_promise(ctx, error);
    }

    jsrt_hmac_algorithm_t hmac_alg = jsrt_crypto_parse_hmac_algorithm(hash_name);
    JS_FreeCString(ctx, hash_name);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, key_algorithm_val);

    // Set up HMAC parameters
    jsrt_hmac_params_t* params = malloc(sizeof(jsrt_hmac_params_t));
    if (!params) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
      return create_rejected_promise(ctx, error);
    }

    params->algorithm = hmac_alg;
    params->key_data = key_data;
    params->key_length = key_data_size;

    // Perform HMAC verification
    bool verification_result = jsrt_crypto_hmac_verify(params, data, data_size, signature_data, signature_size);

    // Clean up
    free(params);
    JS_FreeValue(ctx, key_data_val);

    // Return verification result as boolean
    JSValue result_bool = JS_NewBool(ctx, verification_result);
    return create_resolved_promise(ctx, result_bool);
  }

  // RSA signature verification
  if (alg == JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5) {
    // Create RSA key from DER-encoded key data
    void* rsa_public_key = jsrt_crypto_rsa_create_public_key_from_der(key_data, key_data_size);
    if (!rsa_public_key) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create RSA public key");
      return create_rejected_promise(ctx, error);
    }

    // Set up RSA verification parameters
    jsrt_rsa_params_t params;
    params.algorithm = JSRT_RSASSA_PKCS1_V1_5;
    params.rsa_key = rsa_public_key;
    params.hash_algorithm = JSRT_RSA_HASH_SHA256;  // Default to SHA-256

    // Perform RSA signature verification
    bool verification_result = jsrt_crypto_rsa_verify(&params, data, data_size, signature_data, signature_size);

    // Cleanup the RSA key
    if (rsa_public_key) {
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void* pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
        if (EVP_PKEY_free) {
          EVP_PKEY_free(rsa_public_key);
        }
      }
    }

    JS_FreeValue(ctx, key_data_val);

    // Return verification result as boolean
    JSValue result_bool = JS_NewBool(ctx, verification_result);
    return create_resolved_promise(ctx, result_bool);
  }

  // RSA-PSS signature verification
  if (alg == JSRT_CRYPTO_ALG_RSA_PSS) {
    JSRT_Debug("JSRT_Crypto_Subtle: Starting RSA verification for RSA-PSS");

    // Create RSA key from DER-encoded key data
    void* rsa_public_key = jsrt_crypto_rsa_create_public_key_from_der(key_data, key_data_size);
    if (!rsa_public_key) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create RSA public key");
      return create_rejected_promise(ctx, error);
    }

    // Set up RSA-PSS verification parameters
    jsrt_rsa_params_t params;
    params.algorithm = JSRT_RSA_PSS;
    params.rsa_key = rsa_public_key;
    params.hash_algorithm = JSRT_RSA_HASH_SHA256;  // Default to SHA-256

    // Parse salt length from algorithm if provided
    params.params.pss.salt_length = 0;  // Default to digest length
    JSValue salt_length_val = JS_GetPropertyStr(ctx, argv[0], "saltLength");
    if (!JS_IsUndefined(salt_length_val)) {
      uint32_t salt_length;
      if (JS_ToUint32(ctx, &salt_length, salt_length_val) == 0) {
        params.params.pss.salt_length = salt_length;
        JSRT_Debug("JSRT_Crypto_Subtle: RSA-PSS salt_length=%zu", params.params.pss.salt_length);
      }
    }
    JS_FreeValue(ctx, salt_length_val);

    // Perform RSA-PSS signature verification
    bool verification_result = jsrt_crypto_rsa_verify(&params, data, data_size, signature_data, signature_size);

    // Cleanup the RSA key
    if (rsa_public_key) {
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void* pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
        if (EVP_PKEY_free) {
          EVP_PKEY_free(rsa_public_key);
        }
      }
    }

    JS_FreeValue(ctx, key_data_val);

    // Return verification result as boolean
    JSValue result_bool = JS_NewBool(ctx, verification_result);
    return create_resolved_promise(ctx, result_bool);
  }

  // ECDSA verification
  if (alg == JSRT_CRYPTO_ALG_ECDSA) {
    JSRT_Debug("JSRT_Crypto_Subtle: Starting ECDSA verification");

    // Create EC key from DER-encoded key data (public key)
    const unsigned char* key_ptr = key_data;
    void* ec_public_key = NULL;

    if (openssl_handle) {
      void* (*d2i_PUBKEY)(void** a, const unsigned char** pp, long length) = JSRT_DLSYM(openssl_handle, "d2i_PUBKEY");
      if (d2i_PUBKEY) {
        ec_public_key = d2i_PUBKEY(NULL, &key_ptr, key_data_size);
      }
    }

    if (!ec_public_key) {
      JSRT_Debug("JSRT_Crypto_Subtle: Failed to create EC public key from DER data");
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create EC public key");
      return create_rejected_promise(ctx, error);
    }

    // Get hash algorithm from parameters
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    const char* hash_name = JS_ToCString(ctx, hash_val);
    if (!hash_name) {
      hash_name = "SHA-256";  // Default
    }

    // Setup ECDSA parameters
    jsrt_ecdsa_sign_params_t ecdsa_params;
    ecdsa_params.hash = hash_name;

    // Perform ECDSA verification
    JSValue verify_result =
        jsrt_ec_verify(ctx, ec_public_key, signature_data, signature_size, data, data_size, &ecdsa_params);

    // Cleanup
    if (hash_name && strcmp(hash_name, "SHA-256") != 0) {
      JS_FreeCString(ctx, hash_name);
    }
    JS_FreeValue(ctx, hash_val);

    // Free the EC key
    if (ec_public_key) {
      jsrt_evp_pkey_free_wrapper(ec_public_key);
    }

    JS_FreeValue(ctx, key_data_val);

    if (JS_IsException(verify_result)) {
      return create_rejected_promise(ctx, verify_result);
    }

    return create_resolved_promise(ctx, verify_result);
  }

  JS_FreeValue(ctx, key_data_val);
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm mode not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_generateKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "generateKey requires 3 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Parse algorithm (first argument)
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported algorithm");
    return create_rejected_promise(ctx, error);
  }

  // Parse extractable (second argument)
  bool extractable = JS_ToBool(ctx, argv[1]);

  // Parse keyUsages (third argument) - for now we just validate it's an array
  if (!JS_IsArray(ctx, argv[2])) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "keyUsages must be an array");
    return create_rejected_promise(ctx, error);
  }

  // Generate key based on algorithm
  if (alg == JSRT_CRYPTO_ALG_AES_CBC || alg == JSRT_CRYPTO_ALG_AES_GCM || alg == JSRT_CRYPTO_ALG_AES_CTR) {
    // Get key length from algorithm object
    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    int32_t key_length_bits = 256;  // Default to AES-256
    if (!JS_IsUndefined(length_val)) {
      JS_ToInt32(ctx, &key_length_bits, length_val);
    }
    JS_FreeValue(ctx, length_val);

    // Validate key length
    if (key_length_bits != 128 && key_length_bits != 192 && key_length_bits != 256) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid AES key length");
      return create_rejected_promise(ctx, error);
    }

    // Generate the key
    uint8_t* key_data;
    size_t key_data_length;
    if (jsrt_crypto_generate_aes_key(key_length_bits, &key_data, &key_data_length) != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to generate AES key");
      return create_rejected_promise(ctx, error);
    }

    // Create CryptoKey object with the raw key data
    JSValue key_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, key_obj, "type", JS_NewString(ctx, "secret"));
    JS_SetPropertyStr(ctx, key_obj, "extractable", JS_NewBool(ctx, extractable));
    JS_SetPropertyStr(ctx, key_obj, "algorithm", JS_DupValue(ctx, argv[0]));
    JS_SetPropertyStr(ctx, key_obj, "usages", JS_DupValue(ctx, argv[2]));

    // Store the key data as an internal property (ArrayBuffer)
    JSValue key_buffer = JS_NewArrayBuffer(ctx, key_data, key_data_length, NULL, NULL, 0);
    JS_SetPropertyStr(ctx, key_obj, "__keyData", key_buffer);

    return create_resolved_promise(ctx, key_obj);
  }

  // Generate HMAC key
  if (alg == JSRT_CRYPTO_ALG_HMAC) {
    // Get hash algorithm from algorithm object
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    if (JS_IsUndefined(hash_val)) {
      JS_FreeValue(ctx, hash_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "HMAC requires hash parameter");
      return create_rejected_promise(ctx, error);
    }

    // Parse hash algorithm
    const char* hash_name = JS_ToCString(ctx, hash_val);
    if (!hash_name) {
      JS_FreeValue(ctx, hash_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid hash algorithm for HMAC");
      return create_rejected_promise(ctx, error);
    }

    jsrt_hmac_algorithm_t hmac_alg = jsrt_crypto_parse_hmac_algorithm(hash_name);
    JS_FreeCString(ctx, hash_name);
    JS_FreeValue(ctx, hash_val);

    if (!jsrt_crypto_is_hmac_algorithm_supported(hmac_alg)) {
      JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "HMAC hash algorithm not supported");
      return create_rejected_promise(ctx, error);
    }

    // Generate the HMAC key
    uint8_t* key_data;
    size_t key_data_length;
    if (jsrt_crypto_generate_hmac_key(hmac_alg, &key_data, &key_data_length) != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to generate HMAC key");
      return create_rejected_promise(ctx, error);
    }

    // Create CryptoKey object with the raw key data
    JSValue key_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, key_obj, "type", JS_NewString(ctx, "secret"));
    JS_SetPropertyStr(ctx, key_obj, "extractable", JS_NewBool(ctx, extractable));
    JS_SetPropertyStr(ctx, key_obj, "algorithm", JS_DupValue(ctx, argv[0]));
    JS_SetPropertyStr(ctx, key_obj, "usages", JS_DupValue(ctx, argv[2]));

    // Store the raw key data as ArrayBuffer
    JSValue key_data_buffer = JS_NewArrayBuffer(ctx, key_data, key_data_length, NULL, NULL, 0);
    JS_SetPropertyStr(ctx, key_obj, "__keyData", key_data_buffer);

    return create_resolved_promise(ctx, key_obj);
  }

  // RSA key generation
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP || alg == JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5 || alg == JSRT_CRYPTO_ALG_RSA_PSS ||
      alg == JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5) {
    // Get modulus length from algorithm object
    JSValue modulus_length_val = JS_GetPropertyStr(ctx, argv[0], "modulusLength");
    int32_t modulus_length = 2048;  // Default to 2048 bits
    if (!JS_IsUndefined(modulus_length_val)) {
      JS_ToInt32(ctx, &modulus_length, modulus_length_val);
    }
    JS_FreeValue(ctx, modulus_length_val);

    // Validate modulus length
    if (modulus_length < 1024 || modulus_length > 4096) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid RSA key length");
      return create_rejected_promise(ctx, error);
    }

    // Get public exponent
    JSValue public_exponent_val = JS_GetPropertyStr(ctx, argv[0], "publicExponent");
    uint32_t public_exponent = 65537;  // Default to F4 (0x010001)
    if (!JS_IsUndefined(public_exponent_val)) {
      // Handle Uint8Array public exponent
      size_t exp_size;
      uint8_t* exp_data = JS_GetArrayBuffer(ctx, &exp_size, public_exponent_val);
      if (exp_data && exp_size <= 4) {
        public_exponent = 0;
        for (size_t i = 0; i < exp_size; i++) {
          public_exponent = (public_exponent << 8) | exp_data[i];
        }
      }
    }
    JS_FreeValue(ctx, public_exponent_val);

    // Get hash algorithm - can be either string or object with name property
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    const char* hash_name = NULL;

    if (JS_IsString(hash_val)) {
      // Hash is a string like "SHA-256"
      hash_name = JS_ToCString(ctx, hash_val);
    } else {
      // Hash is an object with name property
      JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
      hash_name = JS_ToCString(ctx, hash_name_val);
    }

    jsrt_rsa_hash_algorithm_t hash_alg = jsrt_crypto_parse_rsa_hash_algorithm(hash_name);
    // For now, we support SHA-256, SHA-384, and SHA-512. SHA-1 is not secure.
    if (hash_alg == JSRT_RSA_HASH_SHA1) {
      JS_FreeCString(ctx, hash_name);
      JS_FreeValue(ctx, hash_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "SHA-1 is not supported for RSA operations");
      return create_rejected_promise(ctx, error);
    }

    // Generate RSA key pair
    jsrt_rsa_keypair_t* keypair = NULL;
    int result = jsrt_crypto_generate_rsa_keypair(modulus_length, public_exponent, hash_alg, &keypair);

    JS_FreeCString(ctx, hash_name);
    JS_FreeValue(ctx, hash_val);

    if (result != 0 || !keypair) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to generate RSA key pair");
      return create_rejected_promise(ctx, error);
    }

    // Extract public and private key data
    uint8_t *public_key_data = NULL, *private_key_data = NULL;
    size_t public_key_size = 0, private_key_size = 0;

    if (jsrt_crypto_rsa_extract_public_key_data(keypair->public_key, &public_key_data, &public_key_size) != 0 ||
        jsrt_crypto_rsa_extract_private_key_data(keypair->private_key, &private_key_data, &private_key_size) != 0) {
      jsrt_crypto_rsa_keypair_free(keypair);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to extract RSA key data");
      return create_rejected_promise(ctx, error);
    }

    // Create CryptoKeyPair object
    JSValue keypair_obj = JS_NewObject(ctx);

    // Create public key object
    JSValue public_key_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, public_key_obj, "type", JS_NewString(ctx, "public"));
    JS_SetPropertyStr(ctx, public_key_obj, "extractable", JS_NewBool(ctx, true));  // Public keys are always extractable
    JS_SetPropertyStr(ctx, public_key_obj, "usages", JS_DupValue(ctx, argv[2]));

    // Set algorithm info for public key
    JSValue public_alg_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, public_alg_obj, "name", JS_NewString(ctx, jsrt_crypto_algorithm_to_string(alg)));
    JS_SetPropertyStr(ctx, public_alg_obj, "modulusLength", JS_NewInt32(ctx, modulus_length));

    // Set public exponent as Uint8Array
    JSValue exp_array = JS_NewArrayBufferCopy(ctx, (uint8_t*)&public_exponent, 4);
    JS_SetPropertyStr(ctx, public_alg_obj, "publicExponent", exp_array);

    JSValue hash_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, hash_obj, "name", JS_NewString(ctx, jsrt_crypto_rsa_hash_algorithm_to_string(hash_alg)));
    JS_SetPropertyStr(ctx, public_alg_obj, "hash", hash_obj);

    JS_SetPropertyStr(ctx, public_key_obj, "algorithm", public_alg_obj);

    // Store the raw key data
    JSValue public_key_buffer = JS_NewArrayBufferCopy(ctx, public_key_data, public_key_size);
    JS_SetPropertyStr(ctx, public_key_obj, "__keyData", public_key_buffer);

    // Create private key object
    JSValue private_key_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, private_key_obj, "type", JS_NewString(ctx, "private"));
    JS_SetPropertyStr(ctx, private_key_obj, "extractable", JS_NewBool(ctx, extractable));
    JS_SetPropertyStr(ctx, private_key_obj, "usages", JS_DupValue(ctx, argv[2]));

    // Set algorithm info for private key (same as public key)
    JS_SetPropertyStr(ctx, private_key_obj, "algorithm", JS_DupValue(ctx, public_alg_obj));

    // Store the raw key data
    JSValue private_key_buffer = JS_NewArrayBufferCopy(ctx, private_key_data, private_key_size);
    JS_SetPropertyStr(ctx, private_key_obj, "__keyData", private_key_buffer);

    // Set the key pair
    JS_SetPropertyStr(ctx, keypair_obj, "publicKey", public_key_obj);
    JS_SetPropertyStr(ctx, keypair_obj, "privateKey", private_key_obj);

    // Cleanup
    jsrt_crypto_rsa_keypair_free(keypair);
    free(public_key_data);
    free(private_key_data);

    return create_resolved_promise(ctx, keypair_obj);
  }

  // EC key generation (ECDSA/ECDH)
  if (alg == JSRT_CRYPTO_ALG_ECDSA || alg == JSRT_CRYPTO_ALG_ECDH) {
    // Get named curve from algorithm object
    JSValue curve_val = JS_GetPropertyStr(ctx, argv[0], "namedCurve");
    const char* curve_name = JS_ToCString(ctx, curve_val);
    JS_FreeValue(ctx, curve_val);

    if (!curve_name) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "namedCurve is required for EC algorithms");
      return create_rejected_promise(ctx, error);
    }

    // Parse curve
    jsrt_ec_curve_t curve;
    if (!jsrt_ec_curve_from_string(curve_name, &curve)) {
      JS_FreeCString(ctx, curve_name);
      JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported EC curve");
      return create_rejected_promise(ctx, error);
    }

    // Get hash algorithm for ECDSA (optional for ECDH)
    const char* hash_name = NULL;
    if (alg == JSRT_CRYPTO_ALG_ECDSA) {
      JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
      if (!JS_IsUndefined(hash_val)) {
        if (JS_IsString(hash_val)) {
          hash_name = JS_ToCString(ctx, hash_val);
        } else {
          JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
          hash_name = JS_ToCString(ctx, hash_name_val);
          JS_FreeValue(ctx, hash_name_val);
        }
      }
      JS_FreeValue(ctx, hash_val);
    }

    // Prepare parameters
    jsrt_ec_keygen_params_t params = {
        .algorithm = (alg == JSRT_CRYPTO_ALG_ECDSA) ? JSRT_ECDSA : JSRT_ECDH, .curve = curve, .hash = hash_name};

    // Generate EC key pair
    JSValue result = jsrt_ec_generate_key(ctx, &params);

    // Cleanup
    JS_FreeCString(ctx, curve_name);
    if (hash_name)
      JS_FreeCString(ctx, hash_name);

    if (JS_IsException(result)) {
      return create_rejected_promise(ctx, result);
    }

    return create_resolved_promise(ctx, result);
  }

  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm not supported for key generation");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_importKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 5) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "importKey requires 5 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Get format (first argument)
  const char* format = JS_ToCString(ctx, argv[0]);
  if (!format) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Invalid format parameter");
    return create_rejected_promise(ctx, error);
  }

  // Support multiple key formats
  bool is_raw = (strcmp(format, "raw") == 0);
  bool is_spki = (strcmp(format, "spki") == 0);
  bool is_pkcs8 = (strcmp(format, "pkcs8") == 0);
  bool is_jwk = (strcmp(format, "jwk") == 0);

  if (!is_raw && !is_spki && !is_pkcs8 && !is_jwk) {
    JS_FreeCString(ctx, format);
    JSValue error =
        jsrt_crypto_throw_error(ctx, "NotSupportedError", "Supported formats: 'raw', 'spki', 'pkcs8', 'jwk'");
    return create_rejected_promise(ctx, error);
  }
  JS_FreeCString(ctx, format);

  // Get key data (second argument)
  size_t key_data_size;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_data_size, argv[1]);
  if (!key_data) {
    // Try TypedArray
    JSValue key_buffer_val = JS_GetPropertyStr(ctx, argv[1], "buffer");
    if (!JS_IsUndefined(key_buffer_val)) {
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, key_buffer_val);
      if (buffer_data) {
        JSValue offset_val = JS_GetPropertyStr(ctx, argv[1], "byteOffset");
        JSValue length_val = JS_GetPropertyStr(ctx, argv[1], "byteLength");
        uint32_t offset = 0, length = 0;
        JS_ToUint32(ctx, &offset, offset_val);
        JS_ToUint32(ctx, &length, length_val);
        key_data = buffer_data + offset;
        key_data_size = length;
        JS_FreeValue(ctx, offset_val);
        JS_FreeValue(ctx, length_val);
      }
      JS_FreeValue(ctx, key_buffer_val);
    }
  }

  if (!key_data && !is_jwk) {
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get algorithm (third argument)
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[2]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported algorithm");
    return create_rejected_promise(ctx, error);
  }

  // Get extractable (fourth argument)
  bool extractable = JS_ToBool(ctx, argv[3]);

  // Get usages (fifth argument)
  // For now, just store the usages array as-is

  // Handle different key formats
  JSValue crypto_key = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, crypto_key, "extractable", JS_NewBool(ctx, extractable));
  JS_SetPropertyStr(ctx, crypto_key, "usages", JS_DupValue(ctx, argv[4]));
  JS_SetPropertyStr(ctx, crypto_key, "algorithm", JS_DupValue(ctx, argv[2]));

  if (is_raw) {
    // Handle raw format (existing logic)
    uint8_t* key_data_copy = malloc(key_data_size);
    if (!key_data_copy) {
      JS_FreeValue(ctx, crypto_key);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to allocate key data");
      return create_rejected_promise(ctx, error);
    }
    memcpy(key_data_copy, key_data, key_data_size);

    JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "secret"));
    JSValue key_buffer = JS_NewArrayBuffer(ctx, key_data_copy, key_data_size, NULL, NULL, 0);
    JS_SetPropertyStr(ctx, crypto_key, "__keyData", key_buffer);

    return create_resolved_promise(ctx, crypto_key);
  } else if (is_spki) {
    // Handle SPKI format (SubjectPublicKeyInfo for public keys)
    return jsrt_import_spki_key(ctx, key_data, key_data_size, alg, crypto_key);
  } else if (is_pkcs8) {
    // Handle PKCS8 format (PKCS#8 for private keys)
    return jsrt_import_pkcs8_key(ctx, key_data, key_data_size, alg, crypto_key);
  } else if (is_jwk) {
    // Handle JWK format (JSON Web Key) - keyData should be an object
    if (!JS_IsObject(argv[1])) {
      JS_FreeValue(ctx, crypto_key);
      JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "JWK keyData must be an object");
      return create_rejected_promise(ctx, error);
    }
    return jsrt_import_jwk_key(ctx, argv[1], alg, crypto_key);
  }

  // Should not reach here
  JS_FreeValue(ctx, crypto_key);
  JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Unknown key format");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_exportKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "exportKey requires 2 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Get format (first argument)
  const char* format = JS_ToCString(ctx, argv[0]);
  if (!format) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Invalid format parameter");
    return create_rejected_promise(ctx, error);
  }

  // Support basic formats
  bool is_raw = (strcmp(format, "raw") == 0);
  bool is_spki = (strcmp(format, "spki") == 0);
  bool is_pkcs8 = (strcmp(format, "pkcs8") == 0);

  if (!is_raw && !is_spki && !is_pkcs8) {
    JS_FreeCString(ctx, format);
    JSValue error =
        jsrt_crypto_throw_error(ctx, "NotSupportedError", "Supported export formats: 'raw', 'spki', 'pkcs8'");
    return create_rejected_promise(ctx, error);
  }

  // Get the key object (second argument)
  JSValue key_obj = argv[1];

  // Check if key is extractable
  JSValue extractable_val = JS_GetPropertyStr(ctx, key_obj, "extractable");
  bool extractable = JS_ToBool(ctx, extractable_val);
  JS_FreeValue(ctx, extractable_val);

  if (!extractable) {
    JS_FreeCString(ctx, format);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Key is not extractable");
    return create_rejected_promise(ctx, error);
  }

  // Get key data
  JSValue key_data_val = JS_GetPropertyStr(ctx, key_obj, "__keyData");
  if (JS_IsUndefined(key_data_val)) {
    JS_FreeCString(ctx, format);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Key data not found");
    return create_rejected_promise(ctx, error);
  }

  size_t key_data_size;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JS_FreeCString(ctx, format);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get key type
  JSValue type_val = JS_GetPropertyStr(ctx, key_obj, "type");
  const char* key_type = JS_ToCString(ctx, type_val);
  JS_FreeValue(ctx, type_val);

  if (is_raw) {
    // For raw format, just return the key data as-is (only for secret keys)
    if (strcmp(key_type, "secret") != 0) {
      JS_FreeCString(ctx, key_type);
      JS_FreeValue(ctx, key_data_val);
      JS_FreeCString(ctx, format);
      JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Raw format only supports secret keys");
      return create_rejected_promise(ctx, error);
    }

    // Create a copy of the key data
    uint8_t* exported_data = malloc(key_data_size);
    if (!exported_data) {
      JS_FreeCString(ctx, key_type);
      JS_FreeValue(ctx, key_data_val);
      JS_FreeCString(ctx, format);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
      return create_rejected_promise(ctx, error);
    }
    memcpy(exported_data, key_data, key_data_size);

    JS_FreeCString(ctx, key_type);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeCString(ctx, format);

    JSValue result = JS_NewArrayBuffer(ctx, exported_data, key_data_size, NULL, NULL, 0);
    return create_resolved_promise(ctx, result);
  } else if (is_spki && strcmp(key_type, "public") == 0) {
    // Export public key in SPKI format
    JSValue result = jsrt_export_spki_key(ctx, key_data, key_data_size);
    JS_FreeCString(ctx, key_type);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeCString(ctx, format);
    return result;
  } else if (is_pkcs8 && strcmp(key_type, "private") == 0) {
    // Export private key in PKCS8 format
    JSValue result = jsrt_export_pkcs8_key(ctx, key_data, key_data_size);
    JS_FreeCString(ctx, key_type);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeCString(ctx, format);
    return result;
  } else {
    // Format/key type mismatch
    JS_FreeCString(ctx, key_type);
    JS_FreeValue(ctx, key_data_val);
    JS_FreeCString(ctx, format);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Format does not match key type");
    return create_rejected_promise(ctx, error);
  }
}

JSValue jsrt_subtle_deriveKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 5) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "deriveKey requires 5 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Parse algorithm (first argument)
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported algorithm");
    return create_rejected_promise(ctx, error);
  }

  // PBKDF2 implementation
  if (alg == JSRT_CRYPTO_ALG_PBKDF2) {
    // Get base key (second argument)
    JSValue base_key_data_val = JS_GetPropertyStr(ctx, argv[1], "__keyData");
    if (JS_IsUndefined(base_key_data_val)) {
      JS_FreeValue(ctx, base_key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid base key object");
      return create_rejected_promise(ctx, error);
    }

    size_t base_key_data_size;
    uint8_t* base_key_data = JS_GetArrayBuffer(ctx, &base_key_data_size, base_key_data_val);
    if (!base_key_data) {
      JS_FreeValue(ctx, base_key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid base key data");
      return create_rejected_promise(ctx, error);
    }

    // Get salt from algorithm parameters
    JSValue salt_val = JS_GetPropertyStr(ctx, argv[0], "salt");
    if (JS_IsUndefined(salt_val)) {
      JS_FreeValue(ctx, salt_val);
      JS_FreeValue(ctx, base_key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "PBKDF2 requires salt parameter");
      return create_rejected_promise(ctx, error);
    }

    // Get salt data
    size_t salt_size;
    uint8_t* salt_data = JS_GetArrayBuffer(ctx, &salt_size, salt_val);
    if (!salt_data) {
      // Try TypedArray
      JSValue salt_buffer_val = JS_GetPropertyStr(ctx, salt_val, "buffer");
      if (!JS_IsUndefined(salt_buffer_val)) {
        size_t buffer_size;
        uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, salt_buffer_val);
        if (buffer_data) {
          JSValue offset_val = JS_GetPropertyStr(ctx, salt_val, "byteOffset");
          JSValue length_val = JS_GetPropertyStr(ctx, salt_val, "byteLength");
          uint32_t offset = 0, length = 0;
          JS_ToUint32(ctx, &offset, offset_val);
          JS_ToUint32(ctx, &length, length_val);
          salt_data = buffer_data + offset;
          salt_size = length;
          JS_FreeValue(ctx, offset_val);
          JS_FreeValue(ctx, length_val);
        }
        JS_FreeValue(ctx, salt_buffer_val);
      }
    }

    if (!salt_data) {
      JS_FreeValue(ctx, salt_val);
      JS_FreeValue(ctx, base_key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Invalid salt parameter");
      return create_rejected_promise(ctx, error);
    }

    // Get iterations from algorithm parameters
    JSValue iterations_val = JS_GetPropertyStr(ctx, argv[0], "iterations");
    uint32_t iterations = 1000;  // Default
    if (!JS_IsUndefined(iterations_val)) {
      JS_ToUint32(ctx, &iterations, iterations_val);
    }
    JS_FreeValue(ctx, iterations_val);

    // Get hash algorithm from algorithm parameters
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    jsrt_crypto_algorithm_t hash_alg = JSRT_CRYPTO_ALG_SHA256;  // Default
    if (!JS_IsUndefined(hash_val)) {
      hash_alg = jsrt_crypto_parse_algorithm(ctx, hash_val);
    }
    JS_FreeValue(ctx, hash_val);

    // Get derived key algorithm (third argument)
    jsrt_crypto_algorithm_t derived_alg = jsrt_crypto_parse_algorithm(ctx, argv[2]);
    if (derived_alg == JSRT_CRYPTO_ALG_UNKNOWN) {
      JS_FreeValue(ctx, salt_val);
      JS_FreeValue(ctx, base_key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported derived key algorithm");
      return create_rejected_promise(ctx, error);
    }

    // Get key length from derived algorithm
    JSValue derived_length_val = JS_GetPropertyStr(ctx, argv[2], "length");
    uint32_t key_length_bits = 256;  // Default
    if (!JS_IsUndefined(derived_length_val)) {
      JS_ToUint32(ctx, &key_length_bits, derived_length_val);
    }
    JS_FreeValue(ctx, derived_length_val);

    size_t key_length_bytes = key_length_bits / 8;

    // Set up PBKDF2 parameters
    jsrt_pbkdf2_params_t pbkdf2_params = {0};
    pbkdf2_params.hash_algorithm = hash_alg;
    pbkdf2_params.salt = salt_data;
    pbkdf2_params.salt_length = salt_size;
    pbkdf2_params.iterations = iterations;

    // Derive the key
    uint8_t* derived_key_data;
    int result = jsrt_crypto_pbkdf2_derive_key(&pbkdf2_params, base_key_data, base_key_data_size, key_length_bytes,
                                               &derived_key_data);

    // Clean up
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, base_key_data_val);

    if (result != 0) {
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Key derivation failed");
      return create_rejected_promise(ctx, error);
    }

    // Get extractable flag (fourth argument)
    bool extractable = JS_ToBool(ctx, argv[3]);

    // Create CryptoKey object
    JSValue crypto_key = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "secret"));
    JS_SetPropertyStr(ctx, crypto_key, "extractable", JS_NewBool(ctx, extractable));

    // Set usages (fifth argument)
    JS_SetPropertyStr(ctx, crypto_key, "usages", JS_DupValue(ctx, argv[4]));

    // Set algorithm
    JS_SetPropertyStr(ctx, crypto_key, "algorithm", JS_DupValue(ctx, argv[2]));

    // Store the derived key data as ArrayBuffer
    JSValue key_buffer = JS_NewArrayBuffer(ctx, derived_key_data, key_length_bytes, NULL, NULL, 0);
    JS_SetPropertyStr(ctx, crypto_key, "__keyData", key_buffer);

    return create_resolved_promise(ctx, crypto_key);
  }

  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm not supported for key derivation");
  return create_rejected_promise(ctx, error);
}

// Helper function for HKDF deriveBits
static JSValue jsrt_subtle_derive_hkdf_bits(JSContext* ctx, JSValueConst algorithm, JSValueConst base_key,
                                            JSValueConst length_arg) {
  // Get key material from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, base_key, "__keyData");
  if (JS_IsUndefined(key_data_val)) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid CryptoKey object");
    return create_rejected_promise(ctx, error);
  }

  size_t key_data_size;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get length parameter (in bits)
  uint32_t length_bits = 256;  // Default
  if (!JS_IsUndefined(length_arg)) {
    if (JS_ToUint32(ctx, &length_bits, length_arg) < 0) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Invalid length parameter");
      return create_rejected_promise(ctx, error);
    }
  }

  size_t length_bytes = (length_bits + 7) / 8;

  // Parse HKDF parameters from algorithm object
  jsrt_hkdf_params_t hkdf_params = {0};

  // Get hash algorithm
  JSValue hash_val = JS_GetPropertyStr(ctx, algorithm, "hash");
  if (JS_IsString(hash_val)) {
    const char* hash_name = JS_ToCString(ctx, hash_val);
    if (hash_name) {
      if (strcmp(hash_name, "SHA-1") == 0) {
        hkdf_params.hash_algorithm = JSRT_CRYPTO_ALG_SHA1;
      } else if (strcmp(hash_name, "SHA-256") == 0) {
        hkdf_params.hash_algorithm = JSRT_CRYPTO_ALG_SHA256;
      } else if (strcmp(hash_name, "SHA-384") == 0) {
        hkdf_params.hash_algorithm = JSRT_CRYPTO_ALG_SHA384;
      } else if (strcmp(hash_name, "SHA-512") == 0) {
        hkdf_params.hash_algorithm = JSRT_CRYPTO_ALG_SHA512;
      } else {
        hkdf_params.hash_algorithm = JSRT_CRYPTO_ALG_SHA256;  // Default
      }
      JS_FreeCString(ctx, hash_name);
    }
  } else {
    hkdf_params.hash_algorithm = JSRT_CRYPTO_ALG_SHA256;  // Default
  }
  JS_FreeValue(ctx, hash_val);

  // Get salt parameter (optional)
  JSValue salt_val = JS_GetPropertyStr(ctx, algorithm, "salt");
  if (JS_IsObject(salt_val)) {
    size_t salt_size;
    uint8_t* salt_data = JS_GetArrayBuffer(ctx, &salt_size, salt_val);
    if (salt_data && salt_size > 0) {
      hkdf_params.salt = malloc(salt_size);
      if (hkdf_params.salt) {
        memcpy(hkdf_params.salt, salt_data, salt_size);
        hkdf_params.salt_length = salt_size;
      }
    }
  }
  JS_FreeValue(ctx, salt_val);

  // Get info parameter (optional)
  JSValue info_val = JS_GetPropertyStr(ctx, algorithm, "info");
  if (JS_IsObject(info_val)) {
    size_t info_size;
    uint8_t* info_data = JS_GetArrayBuffer(ctx, &info_size, info_val);
    if (info_data && info_size > 0) {
      hkdf_params.info = malloc(info_size);
      if (hkdf_params.info) {
        memcpy(hkdf_params.info, info_data, info_size);
        hkdf_params.info_length = info_size;
      }
    }
  }
  JS_FreeValue(ctx, info_val);

  // Perform HKDF
  uint8_t* derived_key = NULL;
  int result = jsrt_crypto_hkdf_derive_key(&hkdf_params, key_data, key_data_size, length_bytes, &derived_key);

  // Cleanup
  JS_FreeValue(ctx, key_data_val);
  free(hkdf_params.salt);
  free(hkdf_params.info);

  if (result != 0 || !derived_key) {
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "HKDF derivation failed");
    return create_rejected_promise(ctx, error);
  }

  // Create ArrayBuffer for result
  JSValue result_buffer = JS_NewArrayBufferCopy(ctx, derived_key, length_bytes);
  free(derived_key);

  return create_resolved_promise(ctx, result_buffer);
}

// Helper function for PBKDF2 deriveBits
static JSValue jsrt_subtle_derive_pbkdf2_bits(JSContext* ctx, JSValueConst algorithm, JSValueConst base_key,
                                              JSValueConst length_arg) {
  // TODO: Implement PBKDF2 deriveBits if needed
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "PBKDF2 deriveBits not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_deriveBits(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "deriveBits requires 3 arguments");
    return create_rejected_promise(ctx, error);
  }

  // Parse algorithm (first argument)
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Unsupported algorithm");
    return create_rejected_promise(ctx, error);
  }

  // Check if algorithm supports key derivation
  if (alg != JSRT_CRYPTO_ALG_ECDH && alg != JSRT_CRYPTO_ALG_HKDF && alg != JSRT_CRYPTO_ALG_PBKDF2) {
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Algorithm not suitable for key derivation");
    return create_rejected_promise(ctx, error);
  }

  // Handle different key derivation algorithms
  if (alg == JSRT_CRYPTO_ALG_HKDF) {
    // HKDF derivation
    return jsrt_subtle_derive_hkdf_bits(ctx, argv[0], argv[1], argv[2]);
  } else if (alg == JSRT_CRYPTO_ALG_PBKDF2) {
    // PBKDF2 derivation
    return jsrt_subtle_derive_pbkdf2_bits(ctx, argv[0], argv[1], argv[2]);
  }

  // ECDH derivation (original implementation continues below)
  // Get private key data from CryptoKey object (second argument)
  JSValue priv_key_data_val = JS_GetPropertyStr(ctx, argv[1], "__keyData");
  if (JS_IsUndefined(priv_key_data_val)) {
    JS_FreeValue(ctx, priv_key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid private CryptoKey object");
    return create_rejected_promise(ctx, error);
  }

  size_t priv_key_data_size;
  uint8_t* priv_key_data = JS_GetArrayBuffer(ctx, &priv_key_data_size, priv_key_data_val);
  if (!priv_key_data) {
    JS_FreeValue(ctx, priv_key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid private key data");
    return create_rejected_promise(ctx, error);
  }

  // Get public key from algorithm parameter
  JSValue pub_key_val = JS_GetPropertyStr(ctx, argv[0], "public");
  if (JS_IsUndefined(pub_key_val)) {
    JS_FreeValue(ctx, priv_key_data_val);
    JS_FreeValue(ctx, pub_key_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "ECDH requires public key parameter");
    return create_rejected_promise(ctx, error);
  }

  // Get public key data
  JSValue pub_key_data_val = JS_GetPropertyStr(ctx, pub_key_val, "__keyData");
  if (JS_IsUndefined(pub_key_data_val)) {
    JS_FreeValue(ctx, priv_key_data_val);
    JS_FreeValue(ctx, pub_key_val);
    JS_FreeValue(ctx, pub_key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid public CryptoKey object");
    return create_rejected_promise(ctx, error);
  }

  size_t pub_key_data_size;
  uint8_t* pub_key_data = JS_GetArrayBuffer(ctx, &pub_key_data_size, pub_key_data_val);
  if (!pub_key_data) {
    JS_FreeValue(ctx, priv_key_data_val);
    JS_FreeValue(ctx, pub_key_val);
    JS_FreeValue(ctx, pub_key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid public key data");
    return create_rejected_promise(ctx, error);
  }

  // Get length parameter (third argument) - number of bits to derive
  uint32_t length_bits = 256;  // Default
  if (!JS_IsUndefined(argv[2])) {
    JS_ToUint32(ctx, &length_bits, argv[2]);
  }

  // Convert bits to bytes (round up)
  size_t length_bytes = (length_bits + 7) / 8;

  // Create EC keys from DER-encoded key data
  const unsigned char* priv_key_ptr = priv_key_data;
  const unsigned char* pub_key_ptr = pub_key_data;
  void* ec_private_key = NULL;
  void* ec_public_key = NULL;

  if (openssl_handle) {
    void* (*d2i_AutoPrivateKey)(void** a, const unsigned char** pp, long length) =
        JSRT_DLSYM(openssl_handle, "d2i_AutoPrivateKey");
    void* (*d2i_PUBKEY)(void** a, const unsigned char** pp, long length) = JSRT_DLSYM(openssl_handle, "d2i_PUBKEY");

    if (d2i_AutoPrivateKey) {
      ec_private_key = d2i_AutoPrivateKey(NULL, &priv_key_ptr, priv_key_data_size);
    }
    if (d2i_PUBKEY) {
      ec_public_key = d2i_PUBKEY(NULL, &pub_key_ptr, pub_key_data_size);
    }
  }

  JS_FreeValue(ctx, priv_key_data_val);
  JS_FreeValue(ctx, pub_key_val);
  JS_FreeValue(ctx, pub_key_data_val);

  if (!ec_private_key || !ec_public_key) {
    if (ec_private_key) {
      jsrt_evp_pkey_free_wrapper(ec_private_key);
    }
    if (ec_public_key) {
      jsrt_evp_pkey_free_wrapper(ec_public_key);
    }
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create EC keys");
    return create_rejected_promise(ctx, error);
  }

  // Setup ECDH parameters
  jsrt_ecdh_derive_params_t ecdh_params;
  ecdh_params.public_key = ec_public_key;
  ecdh_params.public_key_len = pub_key_data_size;

  // Perform ECDH key derivation
  JSValue derived_bits = jsrt_ec_derive_bits(ctx, ec_private_key, &ecdh_params);

  // Free the EC keys
  if (ec_private_key) {
    jsrt_evp_pkey_free_wrapper(ec_private_key);
  }
  if (ec_public_key) {
    jsrt_evp_pkey_free_wrapper(ec_public_key);
  }

  if (JS_IsException(derived_bits)) {
    return create_rejected_promise(ctx, derived_bits);
  }

  // If the requested length is different from what we got, truncate or error
  size_t derived_size;
  uint8_t* derived_data = JS_GetArrayBuffer(ctx, &derived_size, derived_bits);
  if (derived_data && length_bytes < derived_size) {
    // Truncate to requested length
    JSValue truncated = JS_NewArrayBufferCopy(ctx, derived_data, length_bytes);
    JS_FreeValue(ctx, derived_bits);
    return create_resolved_promise(ctx, truncated);
  }

  return create_resolved_promise(ctx, derived_bits);
}

// SPKI (SubjectPublicKeyInfo) key import function
JSValue jsrt_import_spki_key(JSContext* ctx, const uint8_t* key_data, size_t key_data_size, jsrt_crypto_algorithm_t alg,
                             JSValue crypto_key) {
  // Get OpenSSL handle
  extern void* openssl_handle;
  if (!openssl_handle) {
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "OpenSSL not available");
    return create_rejected_promise(ctx, error);
  }

  // Load OpenSSL functions
  void* (*d2i_PUBKEY)(void** a, const unsigned char** pp, long length) = JSRT_DLSYM(openssl_handle, "d2i_PUBKEY");
  void (*EVP_PKEY_free)(void* pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
  int (*i2d_PUBKEY)(void* a, unsigned char** pp) = JSRT_DLSYM(openssl_handle, "i2d_PUBKEY");

  if (!d2i_PUBKEY || !EVP_PKEY_free || !i2d_PUBKEY) {
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Required OpenSSL functions not available");
    return create_rejected_promise(ctx, error);
  }

  // Parse SPKI data to create EVP_PKEY
  const unsigned char* key_ptr = key_data;
  void* pkey = d2i_PUBKEY(NULL, &key_ptr, key_data_size);
  if (!pkey) {
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Invalid SPKI key data");
    return create_rejected_promise(ctx, error);
  }

  // Convert back to DER to store in key object
  unsigned char* der_data = NULL;
  int der_len = i2d_PUBKEY(pkey, &der_data);
  if (der_len <= 0 || !der_data) {
    EVP_PKEY_free(pkey);
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to serialize public key");
    return create_rejected_promise(ctx, error);
  }

  // Create a copy of the DER data for the key object
  uint8_t* der_copy = malloc(der_len);
  if (!der_copy) {
    EVP_PKEY_free(pkey);
    void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
    if (OPENSSL_free)
      OPENSSL_free(der_data);
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
    return create_rejected_promise(ctx, error);
  }
  memcpy(der_copy, der_data, der_len);

  // Clean up OpenSSL resources
  EVP_PKEY_free(pkey);
  void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
  if (OPENSSL_free)
    OPENSSL_free(der_data);

  // Set key properties
  JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "public"));
  JSValue key_buffer = JS_NewArrayBuffer(ctx, der_copy, der_len, NULL, NULL, 0);
  JS_SetPropertyStr(ctx, crypto_key, "__keyData", key_buffer);

  JSRT_Debug("JSRT_Crypto: Successfully imported SPKI public key (%d bytes)", der_len);
  return create_resolved_promise(ctx, crypto_key);
}

// PKCS8 private key import function
JSValue jsrt_import_pkcs8_key(JSContext* ctx, const uint8_t* key_data, size_t key_data_size,
                              jsrt_crypto_algorithm_t alg, JSValue crypto_key) {
  // Get OpenSSL handle
  extern void* openssl_handle;
  if (!openssl_handle) {
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "OpenSSL not available");
    return create_rejected_promise(ctx, error);
  }

  // Load OpenSSL functions
  void* (*d2i_PrivateKey)(int type, void** a, const unsigned char** pp, long length) =
      JSRT_DLSYM(openssl_handle, "d2i_PrivateKey");
  void* (*d2i_PKCS8_PRIV_KEY_INFO)(void** a, const unsigned char** pp, long length) =
      JSRT_DLSYM(openssl_handle, "d2i_PKCS8_PRIV_KEY_INFO");
  void* (*EVP_PKCS82PKEY)(void* p8) = JSRT_DLSYM(openssl_handle, "EVP_PKCS82PKEY");
  void (*EVP_PKEY_free)(void* pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
  void (*PKCS8_PRIV_KEY_INFO_free)(void* a) = JSRT_DLSYM(openssl_handle, "PKCS8_PRIV_KEY_INFO_free");
  int (*i2d_PrivateKey)(void* a, unsigned char** pp) = JSRT_DLSYM(openssl_handle, "i2d_PrivateKey");

  if (!d2i_PKCS8_PRIV_KEY_INFO || !EVP_PKCS82PKEY || !EVP_PKEY_free || !PKCS8_PRIV_KEY_INFO_free || !i2d_PrivateKey) {
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Required OpenSSL functions not available");
    return create_rejected_promise(ctx, error);
  }

  // Parse PKCS8 data
  const unsigned char* key_ptr = key_data;
  void* p8inf = d2i_PKCS8_PRIV_KEY_INFO(NULL, &key_ptr, key_data_size);
  if (!p8inf) {
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Invalid PKCS8 key data");
    return create_rejected_promise(ctx, error);
  }

  // Convert PKCS8 to EVP_PKEY
  void* pkey = EVP_PKCS82PKEY(p8inf);
  PKCS8_PRIV_KEY_INFO_free(p8inf);

  if (!pkey) {
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Failed to parse PKCS8 private key");
    return create_rejected_promise(ctx, error);
  }

  // Convert to DER format for storage
  unsigned char* der_data = NULL;
  int der_len = i2d_PrivateKey(pkey, &der_data);
  if (der_len <= 0 || !der_data) {
    EVP_PKEY_free(pkey);
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to serialize private key");
    return create_rejected_promise(ctx, error);
  }

  // Create a copy of the DER data
  uint8_t* der_copy = malloc(der_len);
  if (!der_copy) {
    EVP_PKEY_free(pkey);
    void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
    if (OPENSSL_free)
      OPENSSL_free(der_data);
    JS_FreeValue(ctx, crypto_key);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
    return create_rejected_promise(ctx, error);
  }
  memcpy(der_copy, der_data, der_len);

  // Clean up OpenSSL resources
  EVP_PKEY_free(pkey);
  void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
  if (OPENSSL_free)
    OPENSSL_free(der_data);

  // Set key properties
  JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "private"));
  JSValue key_buffer = JS_NewArrayBuffer(ctx, der_copy, der_len, NULL, NULL, 0);
  JS_SetPropertyStr(ctx, crypto_key, "__keyData", key_buffer);

  JSRT_Debug("JSRT_Crypto: Successfully imported PKCS8 private key (%d bytes)", der_len);
  return create_resolved_promise(ctx, crypto_key);
}

// Export public key in SPKI format
JSValue jsrt_export_spki_key(JSContext* ctx, const uint8_t* key_data, size_t key_data_size) {
  // Get OpenSSL handle
  extern void* openssl_handle;
  if (!openssl_handle) {
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "OpenSSL not available");
    return create_rejected_promise(ctx, error);
  }

  // The key_data should already be in DER format from internal storage
  // For SPKI export, we need to convert from our internal DER format to SPKI format
  // In our implementation, RSA/EC public keys are already stored as SPKI DER format
  // So we can directly return them

  // Create a copy of the key data
  uint8_t* spki_data = malloc(key_data_size);
  if (!spki_data) {
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
    return create_rejected_promise(ctx, error);
  }
  memcpy(spki_data, key_data, key_data_size);

  JSValue result = JS_NewArrayBuffer(ctx, spki_data, key_data_size, NULL, NULL, 0);
  JSRT_Debug("JSRT_Crypto: Successfully exported SPKI public key (%zu bytes)", key_data_size);
  return create_resolved_promise(ctx, result);
}

// Export private key in PKCS8 format
JSValue jsrt_export_pkcs8_key(JSContext* ctx, const uint8_t* key_data, size_t key_data_size) {
  // Get OpenSSL handle
  extern void* openssl_handle;
  if (!openssl_handle) {
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "OpenSSL not available");
    return create_rejected_promise(ctx, error);
  }

  // Load OpenSSL functions for converting DER private key to PKCS8
  void* (*d2i_PrivateKey)(int type, void** a, const unsigned char** pp, long length) =
      JSRT_DLSYM(openssl_handle, "d2i_PrivateKey");
  void* (*EVP_PKEY2PKCS8)(void* pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY2PKCS8");
  int (*i2d_PKCS8_PRIV_KEY_INFO)(void* a, unsigned char** pp) = JSRT_DLSYM(openssl_handle, "i2d_PKCS8_PRIV_KEY_INFO");
  void (*EVP_PKEY_free)(void* pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
  void (*PKCS8_PRIV_KEY_INFO_free)(void* a) = JSRT_DLSYM(openssl_handle, "PKCS8_PRIV_KEY_INFO_free");

  if (!d2i_PrivateKey || !EVP_PKEY2PKCS8 || !i2d_PKCS8_PRIV_KEY_INFO || !EVP_PKEY_free || !PKCS8_PRIV_KEY_INFO_free) {
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Required OpenSSL functions not available");
    return create_rejected_promise(ctx, error);
  }

  // Parse the DER private key
  const unsigned char* key_ptr = key_data;
  void* pkey = d2i_PrivateKey(0, NULL, &key_ptr, key_data_size);
  if (!pkey) {
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to parse private key");
    return create_rejected_promise(ctx, error);
  }

  // Convert to PKCS8
  void* p8inf = EVP_PKEY2PKCS8(pkey);
  if (!p8inf) {
    EVP_PKEY_free(pkey);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to convert to PKCS8 format");
    return create_rejected_promise(ctx, error);
  }

  // Serialize PKCS8 to DER
  unsigned char* pkcs8_data = NULL;
  int pkcs8_len = i2d_PKCS8_PRIV_KEY_INFO(p8inf, &pkcs8_data);
  if (pkcs8_len <= 0 || !pkcs8_data) {
    PKCS8_PRIV_KEY_INFO_free(p8inf);
    EVP_PKEY_free(pkey);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Failed to serialize PKCS8 key");
    return create_rejected_promise(ctx, error);
  }

  // Create a copy for JavaScript
  uint8_t* pkcs8_copy = malloc(pkcs8_len);
  if (!pkcs8_copy) {
    void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
    if (OPENSSL_free)
      OPENSSL_free(pkcs8_data);
    PKCS8_PRIV_KEY_INFO_free(p8inf);
    EVP_PKEY_free(pkey);
    JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
    return create_rejected_promise(ctx, error);
  }
  memcpy(pkcs8_copy, pkcs8_data, pkcs8_len);

  // Clean up OpenSSL resources
  void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
  if (OPENSSL_free)
    OPENSSL_free(pkcs8_data);
  PKCS8_PRIV_KEY_INFO_free(p8inf);
  EVP_PKEY_free(pkey);

  JSValue result = JS_NewArrayBuffer(ctx, pkcs8_copy, pkcs8_len, NULL, NULL, 0);
  JSRT_Debug("JSRT_Crypto: Successfully exported PKCS8 private key (%d bytes)", pkcs8_len);
  return create_resolved_promise(ctx, result);
}

// Helper function to extract integer from Big-endian bytes and base64url encode
static JSValue bigint_to_base64url_string(JSContext* ctx, const uint8_t* data, size_t len) {
  // Remove leading zeros
  while (len > 1 && data[0] == 0) {
    data++;
    len--;
  }

  size_t encoded_len;
  char* encoded = base64url_encode(data, len, &encoded_len);
  if (!encoded) {
    return JS_ThrowOutOfMemory(ctx);
  }

  JSValue result = JS_NewStringLen(ctx, encoded, encoded_len);
  free(encoded);
  return result;
}

// Helper function to decode base64url string to bytes
static uint8_t* base64url_string_to_bigint(JSContext* ctx, JSValue str_val, size_t* out_len) {
  const char* str = JS_ToCString(ctx, str_val);
  if (!str)
    return NULL;

  size_t str_len = strlen(str);
  uint8_t* result = base64url_decode(str, str_len, out_len);

  JS_FreeCString(ctx, str);
  return result;
}

// Convert base64url string to binary data (same as bigint but clearer naming)
static uint8_t* base64url_string_to_binary(JSContext* ctx, JSValue str_val, size_t* out_len) {
  return base64url_string_to_bigint(ctx, str_val, out_len);
}

// JWK import implementation for RSA keys
static JSValue jsrt_import_jwk_rsa_key(JSContext* ctx, JSValue jwk_obj, jsrt_crypto_algorithm_t alg, JSValue crypto_key,
                                       bool extractable, JSValue usages) {
  extern void* openssl_handle;
  if (!openssl_handle) {
    return jsrt_crypto_throw_error(ctx, "NotSupportedError", "OpenSSL not available");
  }

  // Get required parameters: n (modulus), e (exponent)
  JSValue n_val = JS_GetPropertyStr(ctx, jwk_obj, "n");
  JSValue e_val = JS_GetPropertyStr(ctx, jwk_obj, "e");

  if (JS_IsUndefined(n_val) || JS_IsUndefined(e_val)) {
    JS_FreeValue(ctx, n_val);
    JS_FreeValue(ctx, e_val);
    return jsrt_crypto_throw_error(ctx, "DataError", "Missing required RSA parameters 'n' or 'e'");
  }

  // Decode base64url parameters
  size_t n_len, e_len;
  uint8_t* n_data = base64url_string_to_bigint(ctx, n_val, &n_len);
  uint8_t* e_data = base64url_string_to_bigint(ctx, e_val, &e_len);

  JS_FreeValue(ctx, n_val);
  JS_FreeValue(ctx, e_val);

  if (!n_data || !e_data) {
    if (n_data)
      free(n_data);
    if (e_data)
      free(e_data);
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Invalid base64url encoding in RSA parameters");
    return create_rejected_promise(ctx, error);
  }

  // Load OpenSSL functions
  void* (*EVP_PKEY_new)(void) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_new");
  void* (*RSA_new)(void) = JSRT_DLSYM(openssl_handle, "RSA_new");
  void* (*BN_bin2bn)(const unsigned char*, int, void*) = JSRT_DLSYM(openssl_handle, "BN_bin2bn");
  int (*RSA_set0_key)(void*, void*, void*, void*) = JSRT_DLSYM(openssl_handle, "RSA_set0_key");
  int (*EVP_PKEY_assign)(void*, int, void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_assign");

  if (!EVP_PKEY_new || !RSA_new || !BN_bin2bn || !RSA_set0_key || !EVP_PKEY_assign) {
    free(n_data);
    free(e_data);
    return jsrt_crypto_throw_error(ctx, "NotSupportedError", "Missing OpenSSL RSA functions");
  }

  // Create RSA structure
  void* rsa = RSA_new();
  if (!rsa) {
    free(n_data);
    free(e_data);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create RSA key");
  }

  // Convert byte arrays to BIGNUMs
  void* n_bn = BN_bin2bn(n_data, n_len, NULL);
  void* e_bn = BN_bin2bn(e_data, e_len, NULL);

  free(n_data);
  free(e_data);

  if (!n_bn || !e_bn) {
    // Cleanup BIGNUMs if needed
    void (*RSA_free)(void*) = JSRT_DLSYM(openssl_handle, "RSA_free");
    if (RSA_free)
      RSA_free(rsa);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to convert RSA parameters");
  }

  // Set RSA key parameters (n, e, d)
  if (RSA_set0_key(rsa, n_bn, e_bn, NULL) != 1) {
    void (*RSA_free)(void*) = JSRT_DLSYM(openssl_handle, "RSA_free");
    if (RSA_free)
      RSA_free(rsa);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to set RSA key parameters");
  }

  // Create EVP_PKEY and assign RSA key
  void* pkey = EVP_PKEY_new();
  if (!pkey || EVP_PKEY_assign(pkey, 6 /* EVP_PKEY_RSA */, rsa) != 1) {
    void (*RSA_free)(void*) = JSRT_DLSYM(openssl_handle, "RSA_free");
    void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (RSA_free)
      RSA_free(rsa);
    if (pkey && EVP_PKEY_free)
      EVP_PKEY_free(pkey);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create EVP_PKEY");
  }

  // Serialize key to DER format for storage
  int (*i2d_PUBKEY)(void*, unsigned char**) = JSRT_DLSYM(openssl_handle, "i2d_PUBKEY");
  if (!i2d_PUBKEY) {
    void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (EVP_PKEY_free)
      EVP_PKEY_free(pkey);
    return jsrt_crypto_throw_error(ctx, "NotSupportedError", "Missing OpenSSL serialization functions");
  }

  unsigned char* der_data = NULL;
  int der_len = i2d_PUBKEY(pkey, &der_data);
  if (der_len <= 0) {
    void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (EVP_PKEY_free)
      EVP_PKEY_free(pkey);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to serialize RSA key");
  }

  // Create copy of DER data
  uint8_t* der_copy = malloc(der_len);
  if (!der_copy) {
    void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
    void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (OPENSSL_free)
      OPENSSL_free(der_data);
    if (EVP_PKEY_free)
      EVP_PKEY_free(pkey);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
  }
  memcpy(der_copy, der_data, der_len);

  // Clean up OpenSSL resources
  void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
  void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
  if (OPENSSL_free)
    OPENSSL_free(der_data);
  if (EVP_PKEY_free)
    EVP_PKEY_free(pkey);

  // Set up crypto key object
  JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "public"));
  JS_SetPropertyStr(ctx, crypto_key, "extractable", JS_NewBool(ctx, extractable));
  JS_SetPropertyStr(ctx, crypto_key, "usages", JS_DupValue(ctx, usages));

  // Store the DER data
  JSValue key_buffer = JS_NewArrayBuffer(ctx, der_copy, der_len, NULL, NULL, 0);
  JS_SetPropertyStr(ctx, crypto_key, "__keyData", key_buffer);

  return create_resolved_promise(ctx, crypto_key);
}

// ECDSA JWK import implementation
static JSValue jsrt_import_jwk_ec_key(JSContext* ctx, JSValue jwk_obj, jsrt_crypto_algorithm_t alg, JSValue crypto_key,
                                      bool extractable, JSValue usages) {
  extern void* openssl_handle;
  if (!openssl_handle) {
    return jsrt_crypto_throw_error(ctx, "NotSupportedError", "OpenSSL not available");
  }

  // Get required parameters: crv (curve), x, y coordinates
  JSValue crv_val = JS_GetPropertyStr(ctx, jwk_obj, "crv");
  JSValue x_val = JS_GetPropertyStr(ctx, jwk_obj, "x");
  JSValue y_val = JS_GetPropertyStr(ctx, jwk_obj, "y");

  if (JS_IsUndefined(crv_val) || JS_IsUndefined(x_val) || JS_IsUndefined(y_val)) {
    JS_FreeValue(ctx, crv_val);
    JS_FreeValue(ctx, x_val);
    JS_FreeValue(ctx, y_val);
    return jsrt_crypto_throw_error(ctx, "DataError", "Missing required EC parameters 'crv', 'x' or 'y'");
  }

  const char* crv = JS_ToCString(ctx, crv_val);
  if (!crv) {
    JS_FreeValue(ctx, crv_val);
    JS_FreeValue(ctx, x_val);
    JS_FreeValue(ctx, y_val);
    return jsrt_crypto_throw_error(ctx, "DataError", "Invalid 'crv' parameter in EC JWK");
  }

  // Map curve name to OpenSSL NID
  int curve_nid = 0;
  if (strcmp(crv, "P-256") == 0) {
    curve_nid = 415;  // NID_X9_62_prime256v1
  } else if (strcmp(crv, "P-384") == 0) {
    curve_nid = 715;  // NID_secp384r1
  } else if (strcmp(crv, "P-521") == 0) {
    curve_nid = 716;  // NID_secp521r1
  } else {
    JS_FreeCString(ctx, crv);
    JS_FreeValue(ctx, crv_val);
    JS_FreeValue(ctx, x_val);
    JS_FreeValue(ctx, y_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Unsupported curve in EC JWK");
    return create_rejected_promise(ctx, error);
  }

  JS_FreeCString(ctx, crv);
  JS_FreeValue(ctx, crv_val);

  // Decode base64url coordinates
  size_t x_len, y_len;
  uint8_t* x_data = base64url_string_to_bigint(ctx, x_val, &x_len);
  uint8_t* y_data = base64url_string_to_bigint(ctx, y_val, &y_len);

  JS_FreeValue(ctx, x_val);
  JS_FreeValue(ctx, y_val);

  if (!x_data || !y_data) {
    if (x_data)
      free(x_data);
    if (y_data)
      free(y_data);
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Invalid base64url encoding in EC parameters");
    return create_rejected_promise(ctx, error);
  }

  // Load OpenSSL functions
  void* (*EVP_PKEY_new)(void) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_new");
  void* (*EC_KEY_new_by_curve_name)(int) = JSRT_DLSYM(openssl_handle, "EC_KEY_new_by_curve_name");
  void* (*BN_bin2bn)(const unsigned char*, int, void*) = JSRT_DLSYM(openssl_handle, "BN_bin2bn");
  int (*EC_KEY_set_public_key_affine_coordinates)(void*, const void*, const void*) =
      JSRT_DLSYM(openssl_handle, "EC_KEY_set_public_key_affine_coordinates");
  int (*EVP_PKEY_assign)(void*, int, void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_assign");

  if (!EVP_PKEY_new || !EC_KEY_new_by_curve_name || !BN_bin2bn || !EC_KEY_set_public_key_affine_coordinates ||
      !EVP_PKEY_assign) {
    free(x_data);
    free(y_data);
    return jsrt_crypto_throw_error(ctx, "NotSupportedError", "Missing OpenSSL EC functions");
  }

  // Create EC key structure
  void* ec_key = EC_KEY_new_by_curve_name(curve_nid);
  if (!ec_key) {
    free(x_data);
    free(y_data);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create EC key");
  }

  // Convert coordinates to BIGNUMs
  void* x_bn = BN_bin2bn(x_data, x_len, NULL);
  void* y_bn = BN_bin2bn(y_data, y_len, NULL);

  free(x_data);
  free(y_data);

  if (!x_bn || !y_bn) {
    void (*EC_KEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EC_KEY_free");
    if (EC_KEY_free)
      EC_KEY_free(ec_key);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to convert EC coordinates");
  }

  // Set public key coordinates
  if (EC_KEY_set_public_key_affine_coordinates(ec_key, x_bn, y_bn) != 1) {
    void (*EC_KEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EC_KEY_free");
    void (*BN_free)(void*) = JSRT_DLSYM(openssl_handle, "BN_free");
    if (EC_KEY_free)
      EC_KEY_free(ec_key);
    if (BN_free) {
      BN_free(x_bn);
      BN_free(y_bn);
    }
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to set EC public key coordinates");
  }

  // BIGNUMs are now owned by EC_KEY, don't free them manually

  // Create EVP_PKEY and assign EC key
  void* pkey = EVP_PKEY_new();
  if (!pkey || EVP_PKEY_assign(pkey, 408 /* EVP_PKEY_EC */, ec_key) != 1) {
    void (*EC_KEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EC_KEY_free");
    void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (EC_KEY_free)
      EC_KEY_free(ec_key);
    if (pkey && EVP_PKEY_free)
      EVP_PKEY_free(pkey);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to create EVP_PKEY");
  }

  // Serialize key to DER format for storage
  int (*i2d_PUBKEY)(void*, unsigned char**) = JSRT_DLSYM(openssl_handle, "i2d_PUBKEY");
  if (!i2d_PUBKEY) {
    void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (EVP_PKEY_free)
      EVP_PKEY_free(pkey);
    return jsrt_crypto_throw_error(ctx, "NotSupportedError", "Missing OpenSSL serialization functions");
  }

  unsigned char* der_data = NULL;
  int der_len = i2d_PUBKEY(pkey, &der_data);
  if (der_len <= 0) {
    void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (EVP_PKEY_free)
      EVP_PKEY_free(pkey);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Failed to serialize EC key");
  }

  // Create copy of DER data
  uint8_t* der_copy = malloc(der_len);
  if (!der_copy) {
    void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
    void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (OPENSSL_free)
      OPENSSL_free(der_data);
    if (EVP_PKEY_free)
      EVP_PKEY_free(pkey);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
  }
  memcpy(der_copy, der_data, der_len);

  // Clean up OpenSSL resources
  void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
  void (*EVP_PKEY_free)(void*) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
  if (OPENSSL_free)
    OPENSSL_free(der_data);
  if (EVP_PKEY_free)
    EVP_PKEY_free(pkey);

  // Set up crypto key object
  JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "public"));
  JS_SetPropertyStr(ctx, crypto_key, "extractable", JS_NewBool(ctx, extractable));
  JS_SetPropertyStr(ctx, crypto_key, "usages", JS_DupValue(ctx, usages));

  // Update algorithm object with namedCurve
  JSValue algorithm = JS_GetPropertyStr(ctx, crypto_key, "algorithm");
  if (!JS_IsUndefined(algorithm)) {
    const char* curve_name = NULL;
    if (curve_nid == 415)
      curve_name = "P-256";
    else if (curve_nid == 715)
      curve_name = "P-384";
    else if (curve_nid == 716)
      curve_name = "P-521";

    if (curve_name) {
      JS_SetPropertyStr(ctx, algorithm, "namedCurve", JS_NewString(ctx, curve_name));
    }
    JS_FreeValue(ctx, algorithm);
  }

  // Store the DER data
  JSValue key_buffer = JS_NewArrayBuffer(ctx, der_copy, der_len, NULL, NULL, 0);
  JS_SetPropertyStr(ctx, crypto_key, "__keyData", key_buffer);

  return create_resolved_promise(ctx, crypto_key);
}

// Symmetric key (oct) JWK import implementation
static JSValue jsrt_import_jwk_oct_key(JSContext* ctx, JSValue jwk_obj, jsrt_crypto_algorithm_t alg, JSValue crypto_key,
                                       bool extractable, JSValue usages) {
  // Get required parameter: k (key value)
  JSValue k_val = JS_GetPropertyStr(ctx, jwk_obj, "k");
  if (JS_IsUndefined(k_val)) {
    JS_FreeValue(ctx, k_val);
    return jsrt_crypto_throw_error(ctx, "DataError", "Missing required 'k' parameter in symmetric JWK");
  }

  // Decode base64url key data
  size_t k_len;
  uint8_t* k_data = base64url_string_to_binary(ctx, k_val, &k_len);

  JS_FreeValue(ctx, k_val);

  if (!k_data) {
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Invalid base64url encoding in symmetric key");
    return create_rejected_promise(ctx, error);
  }

  // Validate key length for HMAC
  if (alg == JSRT_CRYPTO_ALG_HMAC) {
    // HMAC keys should be at least 16 bytes for security
    if (k_len < 16) {
      free(k_data);
      JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "HMAC key too short (minimum 16 bytes)");
      return create_rejected_promise(ctx, error);
    }
  }

  // For AES, validate key length
  if (alg == JSRT_CRYPTO_ALG_AES_CBC || alg == JSRT_CRYPTO_ALG_AES_GCM || alg == JSRT_CRYPTO_ALG_AES_CTR) {
    if (k_len != 16 && k_len != 24 && k_len != 32) {
      free(k_data);
      JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Invalid AES key length (must be 16, 24, or 32 bytes)");
      return create_rejected_promise(ctx, error);
    }
  }

  // Create copy of key data for storage
  uint8_t* key_copy = malloc(k_len);
  if (!key_copy) {
    free(k_data);
    return jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
  }
  memcpy(key_copy, k_data, k_len);
  free(k_data);

  // Set up crypto key object
  JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "secret"));
  JS_SetPropertyStr(ctx, crypto_key, "extractable", JS_NewBool(ctx, extractable));
  JS_SetPropertyStr(ctx, crypto_key, "usages", JS_DupValue(ctx, usages));

  // Store the raw key data
  JSValue key_buffer = JS_NewArrayBuffer(ctx, key_copy, k_len, NULL, NULL, 0);
  JS_SetPropertyStr(ctx, crypto_key, "__keyData", key_buffer);

  return create_resolved_promise(ctx, crypto_key);
}

// JWK import implementation
JSValue jsrt_import_jwk_key(JSContext* ctx, JSValue jwk_object, jsrt_crypto_algorithm_t alg, JSValue crypto_key) {
  // Get extractable and usages from crypto_key
  JSValue extractable_val = JS_GetPropertyStr(ctx, crypto_key, "extractable");
  JSValue usages_val = JS_GetPropertyStr(ctx, crypto_key, "usages");

  bool extractable = JS_ToBool(ctx, extractable_val);

  JS_FreeValue(ctx, extractable_val);

  // Get key type from JWK
  JSValue kty_val = JS_GetPropertyStr(ctx, jwk_object, "kty");
  if (JS_IsUndefined(kty_val)) {
    JS_FreeValue(ctx, usages_val);
    JS_FreeValue(ctx, kty_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Missing required 'kty' parameter in JWK");
    return create_rejected_promise(ctx, error);
  }

  const char* kty = JS_ToCString(ctx, kty_val);
  if (!kty) {
    JS_FreeValue(ctx, usages_val);
    JS_FreeValue(ctx, kty_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Invalid 'kty' parameter in JWK");
    return create_rejected_promise(ctx, error);
  }

  JSValue result;

  if (strcmp(kty, "RSA") == 0) {
    // Handle RSA keys
    if (alg != JSRT_CRYPTO_ALG_RSA_OAEP && alg != JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5 && alg != JSRT_CRYPTO_ALG_RSA_PSS) {
      JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Algorithm mismatch: expected RSA algorithm");
      result = create_rejected_promise(ctx, error);
    } else {
      result = jsrt_import_jwk_rsa_key(ctx, jwk_object, alg, crypto_key, extractable, usages_val);
    }
  } else if (strcmp(kty, "EC") == 0) {
    // Handle ECDSA keys
    if (alg != JSRT_CRYPTO_ALG_ECDSA && alg != JSRT_CRYPTO_ALG_ECDH) {
      JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Algorithm mismatch: expected ECDSA or ECDH algorithm");
      result = create_rejected_promise(ctx, error);
    } else {
      result = jsrt_import_jwk_ec_key(ctx, jwk_object, alg, crypto_key, extractable, usages_val);
    }
  } else if (strcmp(kty, "oct") == 0) {
    // Handle symmetric keys
    if (alg != JSRT_CRYPTO_ALG_HMAC && alg != JSRT_CRYPTO_ALG_AES_CBC && alg != JSRT_CRYPTO_ALG_AES_GCM &&
        alg != JSRT_CRYPTO_ALG_AES_CTR) {
      JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Algorithm mismatch: expected HMAC or AES algorithm");
      result = create_rejected_promise(ctx, error);
    } else {
      result = jsrt_import_jwk_oct_key(ctx, jwk_object, alg, crypto_key, extractable, usages_val);
    }
  } else {
    JSValue error = jsrt_crypto_throw_error(ctx, "DataError", "Unsupported key type in JWK");
    result = create_rejected_promise(ctx, error);
  }

  JS_FreeCString(ctx, kty);
  JS_FreeValue(ctx, kty_val);
  JS_FreeValue(ctx, usages_val);

  return result;
}

// JWK export implementation
JSValue jsrt_export_jwk_key(JSContext* ctx, JSValue crypto_key) {
  // TODO: Implement JWK export
  return jsrt_crypto_throw_error(ctx, "NotSupportedError", "JWK export not yet implemented");
}

// Create SubtleCrypto object
JSValue JSRT_CreateSubtleCrypto(JSContext* ctx) {
  JSValue subtle = JS_NewObject(ctx);

  // Add methods to subtle crypto object
  JS_SetPropertyStr(ctx, subtle, "digest", JS_NewCFunction(ctx, jsrt_subtle_digest, "digest", 2));
  JS_SetPropertyStr(ctx, subtle, "encrypt", JS_NewCFunction(ctx, jsrt_subtle_encrypt, "encrypt", 3));
  JS_SetPropertyStr(ctx, subtle, "decrypt", JS_NewCFunction(ctx, jsrt_subtle_decrypt, "decrypt", 3));
  JS_SetPropertyStr(ctx, subtle, "sign", JS_NewCFunction(ctx, jsrt_subtle_sign, "sign", 3));
  JS_SetPropertyStr(ctx, subtle, "verify", JS_NewCFunction(ctx, jsrt_subtle_verify, "verify", 4));
  JS_SetPropertyStr(ctx, subtle, "generateKey", JS_NewCFunction(ctx, jsrt_subtle_generateKey, "generateKey", 3));
  JS_SetPropertyStr(ctx, subtle, "importKey", JS_NewCFunction(ctx, jsrt_subtle_importKey, "importKey", 5));
  JS_SetPropertyStr(ctx, subtle, "exportKey", JS_NewCFunction(ctx, jsrt_subtle_exportKey, "exportKey", 2));
  JS_SetPropertyStr(ctx, subtle, "deriveKey", JS_NewCFunction(ctx, jsrt_subtle_deriveKey, "deriveKey", 5));
  JS_SetPropertyStr(ctx, subtle, "deriveBits", JS_NewCFunction(ctx, jsrt_subtle_deriveBits, "deriveBits", 3));

  return subtle;
}

// Setup SubtleCrypto in runtime (called from main crypto setup)
void JSRT_SetupSubtleCrypto(JSRT_Runtime* rt) {
  JSRT_Debug("JSRT_SetupSubtleCrypto: initializing SubtleCrypto API");
}