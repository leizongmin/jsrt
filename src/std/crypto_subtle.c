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
#include "crypto_rsa.h"
#include "crypto_symmetric.h"

// Cross-platform dynamic loading abstractions
#ifdef _WIN32
extern HMODULE openssl_handle;
#define JSRT_DLSYM(handle, name) ((void *)GetProcAddress(handle, name))
#else

#define JSRT_DLSYM(handle, name) dlsym(handle, name)
#endif

// Algorithm name to enum mapping
static const struct {
  const char *name;
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
      return false;  // TODO: Fix OpenSSL 3.x compatibility

    // Elliptic curve algorithms
    case JSRT_CRYPTO_ALG_ECDSA:
      return true;  // Implemented
    case JSRT_CRYPTO_ALG_ECDH:
      return true;  // Implemented

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

  // RSA-OAEP encryption
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP) {
    // Get hash algorithm from algorithm object - can be either string or object with name property
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    const char *hash_name = NULL;

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
    uint8_t *label_data = NULL;
    size_t label_size = 0;

    if (!JS_IsUndefined(label_val) && !JS_IsNull(label_val)) {
      label_data = JS_GetArrayBuffer(ctx, &label_size, label_val);
      if (!label_data) {
        // Try TypedArray
        JSValue label_buffer_val = JS_GetPropertyStr(ctx, label_val, "buffer");
        if (!JS_IsUndefined(label_buffer_val)) {
          size_t buffer_size;
          uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, label_buffer_val);
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
    const char *key_type = JS_ToCString(ctx, key_type_val);

    // Create EVP_PKEY from stored DER data
    void *rsa_key = NULL;
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
    uint8_t *ciphertext_data = NULL;
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
        void (*EVP_PKEY_free)(void *) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
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
    void *rsa_key = jsrt_crypto_rsa_create_public_key_from_der(key_data, key_data_size);
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
    uint8_t *ciphertext_data = NULL;
    size_t ciphertext_size = 0;
    int result = jsrt_crypto_rsa_encrypt(&params, plaintext_data, plaintext_size, &ciphertext_data, &ciphertext_size);

    // Cleanup
    JS_FreeValue(ctx, key_data_val);

    // Free the created EVP_PKEY
    if (rsa_key) {
#ifdef _WIN32
      extern HMODULE openssl_handle;
#else
      extern void *openssl_handle;
#endif
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void *) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
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

  // RSA-OAEP decryption
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP) {
    // Get hash algorithm from algorithm object - can be either string or object with name property
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    const char *hash_name = NULL;

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
    uint8_t *label_data = NULL;
    size_t label_size = 0;

    if (!JS_IsUndefined(label_val) && !JS_IsNull(label_val)) {
      label_data = JS_GetArrayBuffer(ctx, &label_size, label_val);
      if (!label_data) {
        // Try TypedArray
        JSValue label_buffer_val = JS_GetPropertyStr(ctx, label_val, "buffer");
        if (!JS_IsUndefined(label_buffer_val)) {
          size_t buffer_size;
          uint8_t *buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, label_buffer_val);
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
    const char *key_type = JS_ToCString(ctx, key_type_val);

    // Create EVP_PKEY from stored DER data
    void *rsa_key = NULL;
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
    uint8_t *plaintext_data = NULL;
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
        void (*EVP_PKEY_free)(void *) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
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
    void *rsa_key = jsrt_crypto_rsa_create_private_key_from_der(key_data, key_data_size);
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
    uint8_t *plaintext_data = NULL;
    size_t plaintext_size = 0;
    int result = jsrt_crypto_rsa_decrypt(&params, ciphertext_data, ciphertext_size, &plaintext_data, &plaintext_size);

    // Cleanup
    JS_FreeValue(ctx, key_data_val);

    // Free the created EVP_PKEY
    if (rsa_key) {
#ifdef _WIN32
      extern HMODULE openssl_handle;
#else
      extern void *openssl_handle;
#endif
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void *) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
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

JSValue jsrt_subtle_sign(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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
  uint8_t *key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get data to sign - handle both ArrayBuffer and TypedArray
  size_t data_size;
  uint8_t *data = NULL;

  // Try ArrayBuffer first
  data = JS_GetArrayBuffer(ctx, &data_size, argv[2]);

  // If not ArrayBuffer, try TypedArray
  if (!data) {
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

    const char *hash_name = JS_ToCString(ctx, hash_val);
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
    jsrt_hmac_params_t *params = malloc(sizeof(jsrt_hmac_params_t));
    if (!params) {
      JS_FreeValue(ctx, key_data_val);
      JSValue error = jsrt_crypto_throw_error(ctx, "OperationError", "Memory allocation failed");
      return create_rejected_promise(ctx, error);
    }

    params->algorithm = hmac_alg;
    params->key_data = key_data;
    params->key_length = key_data_size;

    // Perform HMAC signing
    uint8_t *signature_data;
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
    void *rsa_private_key = jsrt_crypto_rsa_create_private_key_from_der(key_data, key_data_size);
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
    uint8_t *signature_data = NULL;
    size_t signature_size = 0;
    int sign_result = jsrt_crypto_rsa_sign(&params, data, data_size, &signature_data, &signature_size);

    // Cleanup the RSA key
    if (rsa_private_key) {
      // Note: Need to add proper OpenSSL EVP_PKEY_free here

      if (openssl_handle) {
        void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
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
    void *rsa_private_key = jsrt_crypto_rsa_create_private_key_from_der(key_data, key_data_size);
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
    uint8_t *signature_data = NULL;
    size_t signature_size = 0;
    int sign_result = jsrt_crypto_rsa_sign(&params, data, data_size, &signature_data, &signature_size);

    // Cleanup the RSA key
    if (rsa_private_key) {
      if (openssl_handle) {
        void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
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
    const unsigned char *key_ptr = key_data;
    void *ec_private_key = NULL;

    if (openssl_handle) {
      void *(*d2i_AutoPrivateKey)(void **a, const unsigned char **pp, long length) =
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
    const char *hash_name = JS_ToCString(ctx, hash_val);
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
    if (ec_private_key && openssl_handle) {
      void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
      if (EVP_PKEY_free) {
        EVP_PKEY_free(ec_private_key);
      }
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

JSValue jsrt_subtle_verify(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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
  uint8_t *key_data = JS_GetArrayBuffer(ctx, &key_data_size, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid key data");
    return create_rejected_promise(ctx, error);
  }

  // Get signature to verify - handle both ArrayBuffer and TypedArray (third argument)
  size_t signature_size;
  uint8_t *signature_data = NULL;

  // Try ArrayBuffer first
  signature_data = JS_GetArrayBuffer(ctx, &signature_size, argv[2]);

  // If not ArrayBuffer, try TypedArray
  if (!signature_data) {
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
  uint8_t *data = NULL;

  // Try ArrayBuffer first
  data = JS_GetArrayBuffer(ctx, &data_size, argv[3]);

  // If not ArrayBuffer, try TypedArray
  if (!data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[3], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[3], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[3], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
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
    JS_FreeValue(ctx, key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "TypeError", "Data must be an ArrayBuffer or TypedArray");
    return create_rejected_promise(ctx, error);
  }

  // For HMAC, get hash algorithm from the key's algorithm
  if (alg == JSRT_CRYPTO_ALG_HMAC) {
    JSValue key_algorithm_val = JS_GetPropertyStr(ctx, argv[1], "algorithm");
    JSValue hash_val = JS_GetPropertyStr(ctx, key_algorithm_val, "hash");

    const char *hash_name = JS_ToCString(ctx, hash_val);
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
    jsrt_hmac_params_t *params = malloc(sizeof(jsrt_hmac_params_t));
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
    void *rsa_public_key = jsrt_crypto_rsa_create_public_key_from_der(key_data, key_data_size);
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
        void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
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
    void *rsa_public_key = jsrt_crypto_rsa_create_public_key_from_der(key_data, key_data_size);
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
        void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
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
    const unsigned char *key_ptr = key_data;
    void *ec_public_key = NULL;

    if (openssl_handle) {
      void *(*d2i_PUBKEY)(void **a, const unsigned char **pp, long length) = JSRT_DLSYM(openssl_handle, "d2i_PUBKEY");
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
    const char *hash_name = JS_ToCString(ctx, hash_val);
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
    if (ec_public_key && openssl_handle) {
      void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
      if (EVP_PKEY_free) {
        EVP_PKEY_free(ec_public_key);
      }
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
    const char *hash_name = JS_ToCString(ctx, hash_val);
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
    uint8_t *key_data;
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
      uint8_t *exp_data = JS_GetArrayBuffer(ctx, &exp_size, public_exponent_val);
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
    const char *hash_name = NULL;

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
    jsrt_rsa_keypair_t *keypair = NULL;
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
    JSValue exp_array = JS_NewArrayBufferCopy(ctx, (uint8_t *)&public_exponent, 4);
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
    const char *curve_name = JS_ToCString(ctx, curve_val);
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
    const char *hash_name = NULL;
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
    if (hash_name) JS_FreeCString(ctx, hash_name);

    if (JS_IsException(result)) {
      return create_rejected_promise(ctx, result);
    }

    return create_resolved_promise(ctx, result);
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

  // Check if algorithm is ECDH
  if (alg != JSRT_CRYPTO_ALG_ECDH) {
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Algorithm not suitable for key derivation");
    return create_rejected_promise(ctx, error);
  }

  // Get private key data from CryptoKey object (second argument)
  JSValue priv_key_data_val = JS_GetPropertyStr(ctx, argv[1], "__keyData");
  if (JS_IsUndefined(priv_key_data_val)) {
    JS_FreeValue(ctx, priv_key_data_val);
    JSValue error = jsrt_crypto_throw_error(ctx, "InvalidAccessError", "Invalid private CryptoKey object");
    return create_rejected_promise(ctx, error);
  }

  size_t priv_key_data_size;
  uint8_t *priv_key_data = JS_GetArrayBuffer(ctx, &priv_key_data_size, priv_key_data_val);
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
  uint8_t *pub_key_data = JS_GetArrayBuffer(ctx, &pub_key_data_size, pub_key_data_val);
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
  const unsigned char *priv_key_ptr = priv_key_data;
  const unsigned char *pub_key_ptr = pub_key_data;
  void *ec_private_key = NULL;
  void *ec_public_key = NULL;

  if (openssl_handle) {
    void *(*d2i_AutoPrivateKey)(void **a, const unsigned char **pp, long length) =
        JSRT_DLSYM(openssl_handle, "d2i_AutoPrivateKey");
    void *(*d2i_PUBKEY)(void **a, const unsigned char **pp, long length) = JSRT_DLSYM(openssl_handle, "d2i_PUBKEY");

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
    if (ec_private_key && openssl_handle) {
      void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
      if (EVP_PKEY_free) EVP_PKEY_free(ec_private_key);
    }
    if (ec_public_key && openssl_handle) {
      void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
      if (EVP_PKEY_free) EVP_PKEY_free(ec_public_key);
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
  if (ec_private_key && openssl_handle) {
    void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (EVP_PKEY_free) EVP_PKEY_free(ec_private_key);
  }
  if (ec_public_key && openssl_handle) {
    void (*EVP_PKEY_free)(void *pkey) = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    if (EVP_PKEY_free) EVP_PKEY_free(ec_public_key);
  }

  if (JS_IsException(derived_bits)) {
    return create_rejected_promise(ctx, derived_bits);
  }

  // If the requested length is different from what we got, truncate or error
  size_t derived_size;
  uint8_t *derived_data = JS_GetArrayBuffer(ctx, &derived_size, derived_bits);
  if (derived_data && length_bytes < derived_size) {
    // Truncate to requested length
    JSValue truncated = JS_NewArrayBufferCopy(ctx, derived_data, length_bytes);
    JS_FreeValue(ctx, derived_bits);
    return create_resolved_promise(ctx, truncated);
  }

  return create_resolved_promise(ctx, derived_bits);
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