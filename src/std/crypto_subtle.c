#include "crypto_subtle.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../util/debug.h"
#include "crypto.h"
#include "crypto_digest.h"
#include "crypto_symmetric.h"

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
      return false;  // TODO: Implement

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

// crypto.subtle.encrypt implementation
JSValue jsrt_subtle_encrypt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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
  if (alg != JSRT_CRYPTO_ALG_AES_CBC && alg != JSRT_CRYPTO_ALG_AES_GCM && alg != JSRT_CRYPTO_ALG_AES_CTR) {
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
  uint8_t *key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get plaintext data (third argument) - handle both ArrayBuffer and TypedArray
  size_t plaintext_size;
  uint8_t *plaintext_data = NULL;

  // Try ArrayBuffer first
  plaintext_data = JS_GetArrayBuffer(ctx, &plaintext_size, argv[2]);

  // If not ArrayBuffer, try TypedArray
  if (!plaintext_data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[2], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[2], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[2], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

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
    uint8_t *iv_data = NULL;

    // Try ArrayBuffer first
    iv_data = JS_GetArrayBuffer(ctx, &iv_size, iv_val);

    // If not ArrayBuffer, try TypedArray
    if (!iv_data) {
      JSValue iv_buffer_val = JS_GetPropertyStr(ctx, iv_val, "buffer");
      JSValue iv_byteOffset_val = JS_GetPropertyStr(ctx, iv_val, "byteOffset");
      JSValue iv_byteLength_val = JS_GetPropertyStr(ctx, iv_val, "byteLength");

      if (!JS_IsUndefined(iv_buffer_val) && !JS_IsUndefined(iv_byteOffset_val) && !JS_IsUndefined(iv_byteLength_val)) {
        size_t buffer_size;
        uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, iv_buffer_val);

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
    jsrt_symmetric_params_t *params = malloc(sizeof(jsrt_symmetric_params_t));
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
    uint8_t *ciphertext_data;
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
    uint8_t *iv_data = NULL;

    // Try ArrayBuffer first
    iv_data = JS_GetArrayBuffer(ctx, &iv_size, iv_val);

    // If not ArrayBuffer, try TypedArray
    if (!iv_data) {
      JSValue iv_buffer_val = JS_GetPropertyStr(ctx, iv_val, "buffer");
      JSValue iv_byteOffset_val = JS_GetPropertyStr(ctx, iv_val, "byteOffset");
      JSValue iv_byteLength_val = JS_GetPropertyStr(ctx, iv_val, "byteLength");

      if (!JS_IsUndefined(iv_buffer_val) && !JS_IsUndefined(iv_byteOffset_val) && !JS_IsUndefined(iv_byteLength_val)) {
        size_t buffer_size;
        uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, iv_buffer_val);

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
    uint8_t *additional_data = NULL;
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
          uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, aad_buffer_val);

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
    jsrt_symmetric_params_t *params = malloc(sizeof(jsrt_symmetric_params_t));
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
    uint8_t *ciphertext_data;
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

  JS_FreeValue(ctx, key_data_val);
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm mode not yet implemented");
  return create_rejected_promise(ctx, error);
}

JSValue jsrt_subtle_decrypt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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
  if (alg != JSRT_CRYPTO_ALG_AES_CBC && alg != JSRT_CRYPTO_ALG_AES_GCM && alg != JSRT_CRYPTO_ALG_AES_CTR) {
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
  uint8_t *key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get ciphertext data (third argument) - handle both ArrayBuffer and TypedArray
  size_t ciphertext_size;
  uint8_t *ciphertext_data = NULL;

  // Try ArrayBuffer first
  ciphertext_data = JS_GetArrayBuffer(ctx, &ciphertext_size, argv[2]);

  // If not ArrayBuffer, try TypedArray
  if (!ciphertext_data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[2], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[2], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[2], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

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
    uint8_t *iv_data = NULL;

    // Try ArrayBuffer first
    iv_data = JS_GetArrayBuffer(ctx, &iv_size, iv_val);

    // If not ArrayBuffer, try TypedArray
    if (!iv_data) {
      JSValue iv_buffer_val = JS_GetPropertyStr(ctx, iv_val, "buffer");
      JSValue iv_byteOffset_val = JS_GetPropertyStr(ctx, iv_val, "byteOffset");
      JSValue iv_byteLength_val = JS_GetPropertyStr(ctx, iv_val, "byteLength");

      if (!JS_IsUndefined(iv_buffer_val) && !JS_IsUndefined(iv_byteOffset_val) && !JS_IsUndefined(iv_byteLength_val)) {
        size_t buffer_size;
        uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, iv_buffer_val);

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
    jsrt_symmetric_params_t *params = malloc(sizeof(jsrt_symmetric_params_t));
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
    uint8_t *plaintext_data;
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
    uint8_t *iv_data = NULL;

    // Try ArrayBuffer first
    iv_data = JS_GetArrayBuffer(ctx, &iv_size, iv_val);

    // If not ArrayBuffer, try TypedArray
    if (!iv_data) {
      JSValue iv_buffer_val = JS_GetPropertyStr(ctx, iv_val, "buffer");
      JSValue iv_byteOffset_val = JS_GetPropertyStr(ctx, iv_val, "byteOffset");
      JSValue iv_byteLength_val = JS_GetPropertyStr(ctx, iv_val, "byteLength");

      if (!JS_IsUndefined(iv_buffer_val) && !JS_IsUndefined(iv_byteOffset_val) && !JS_IsUndefined(iv_byteLength_val)) {
        size_t buffer_size;
        uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, iv_buffer_val);

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
    uint8_t *additional_data = NULL;
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
          uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, aad_buffer_val);

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
    jsrt_symmetric_params_t *params = malloc(sizeof(jsrt_symmetric_params_t));
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
    uint8_t *plaintext_data;
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

  JS_FreeValue(ctx, key_data_val);
  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm mode not yet implemented");
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
    uint8_t *key_data;
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

  JSValue error = jsrt_crypto_throw_error(ctx, "NotSupportedError", "Algorithm not supported for key generation");
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