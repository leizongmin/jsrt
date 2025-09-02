#include "crypto_subtle.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../util/debug.h"
#include "crypto.h"
#include "crypto_digest.h"

// Algorithm name to enum mapping
static const struct {
  const char *name;
  jsrt_crypto_algorithm_t algorithm;
} algorithm_map[] = {{"SHA-1", JSRT_CRYPTO_ALG_SHA1},      {"SHA-256", JSRT_CRYPTO_ALG_SHA256},
                     {"SHA-384", JSRT_CRYPTO_ALG_SHA384},  {"SHA-512", JSRT_CRYPTO_ALG_SHA512},
                     {"AES-CBC", JSRT_CRYPTO_ALG_AES_CBC}, {"AES-GCM", JSRT_CRYPTO_ALG_AES_GCM},
                     {"AES-CTR", JSRT_CRYPTO_ALG_AES_CTR}, {"RSA-OAEP", JSRT_CRYPTO_ALG_RSA_OAEP},
                     {"RSA-PSS", JSRT_CRYPTO_ALG_RSA_PSS}, {"RSASSA-PKCS1-v1_5", JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5},
                     {"ECDSA", JSRT_CRYPTO_ALG_ECDSA},     {"ECDH", JSRT_CRYPTO_ALG_ECDH},
                     {"HMAC", JSRT_CRYPTO_ALG_HMAC},       {"PBKDF2", JSRT_CRYPTO_ALG_PBKDF2},
                     {"HKDF", JSRT_CRYPTO_ALG_HKDF},       {NULL, JSRT_CRYPTO_ALG_UNKNOWN}};

// Parse algorithm parameter
jsrt_crypto_algorithm_t jsrt_crypto_parse_algorithm(JSContext *ctx, JSValue algorithm) {
  if (JS_IsString(algorithm)) {
    const char *alg_name = JS_ToCString(ctx, algorithm);
    if (!alg_name) return JSRT_CRYPTO_ALG_UNKNOWN;

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
      const char *alg_name = JS_ToCString(ctx, name_val);
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
const char *jsrt_crypto_algorithm_to_string(jsrt_crypto_algorithm_t alg) {
  for (int i = 0; algorithm_map[i].name != NULL; i++) {
    if (algorithm_map[i].algorithm == alg) {
      return algorithm_map[i].name;
    }
  }
  return "Unknown";
}

// Check if algorithm is supported
bool jsrt_crypto_is_algorithm_supported(jsrt_crypto_algorithm_t alg) {
  // For now, only digest algorithms are supported
  switch (alg) {
    case JSRT_CRYPTO_ALG_SHA1:
    case JSRT_CRYPTO_ALG_SHA256:
    case JSRT_CRYPTO_ALG_SHA384:
    case JSRT_CRYPTO_ALG_SHA512:
      return true;
    default:
      return false;
  }
}

// Throw WebCrypto specific errors
JSValue jsrt_crypto_throw_error(JSContext *ctx, const char *name, const char *message) {
  JSValue error = JS_NewError(ctx);
  JS_SetPropertyStr(ctx, error, "name", JS_NewString(ctx, name));
  JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));
  return error;  // Return error object directly, don't throw here
}

// Memory management functions
JSRTCryptoKey *jsrt_crypto_key_new(void) {
  JSRTCryptoKey *key = malloc(sizeof(JSRTCryptoKey));
  if (key) {
    memset(key, 0, sizeof(JSRTCryptoKey));
  }
  return key;
}

void jsrt_crypto_key_free(JSRTCryptoKey *key) {
  if (key) {
    free(key->algorithm_name);
    free(key->key_type);
    // TODO: Free OpenSSL key structure
    free(key);
  }
}

JSRTCryptoAsyncOperation *jsrt_crypto_async_operation_new(void) {
  JSRTCryptoAsyncOperation *op = malloc(sizeof(JSRTCryptoAsyncOperation));
  if (op) {
    memset(op, 0, sizeof(JSRTCryptoAsyncOperation));
  }
  return op;
}

void jsrt_crypto_async_operation_free(JSRTCryptoAsyncOperation *op) {
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
static JSValue create_resolved_promise(JSContext *ctx, JSValue value) {
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
static JSValue create_rejected_promise(JSContext *ctx, JSValue error) {
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
JSValue jsrt_subtle_digest(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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
  uint8_t *data = NULL;

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
      uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

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
  uint8_t *digest_output;
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

// Stub implementations for other SubtleCrypto methods
JSValue jsrt_subtle_encrypt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "encrypt not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_decrypt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "decrypt not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_sign(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "sign not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_verify(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "verify not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_generateKey(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "generateKey not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_importKey(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "importKey not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_exportKey(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "exportKey not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_deriveKey(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "deriveKey not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_deriveBits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "deriveBits not yet implemented");
  return create_rejected_promise(ctx, error);
}

// Create SubtleCrypto object
JSValue JSRT_CreateSubtleCrypto(JSContext *ctx) {
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
void JSRT_SetupSubtleCrypto(JSRT_Runtime *rt) { JSRT_Debug("JSRT_SetupSubtleCrypto: initializing SubtleCrypto API"); }