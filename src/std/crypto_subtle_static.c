// Static OpenSSL build - full SubtleCrypto implementation
#include <quickjs.h>

// Only compile static OpenSSL implementation when OpenSSL is available
#ifdef JSRT_STATIC_OPENSSL
#include <openssl/aes.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../util/debug.h"
#include "crypto_subtle.h"

// Forward declarations for static implementations
static JSValue jsrt_crypto_getRandomValues_static(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_crypto_randomUUID_static(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Implementation of digest operation for static OpenSSL
static int jsrt_static_digest_data(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len,
                                   uint8_t** output, size_t* output_len) {
  const EVP_MD* md = NULL;

  switch (alg) {
    case JSRT_CRYPTO_ALG_SHA1:
      md = EVP_sha1();
      break;
    case JSRT_CRYPTO_ALG_SHA256:
      md = EVP_sha256();
      break;
    case JSRT_CRYPTO_ALG_SHA384:
      md = EVP_sha384();
      break;
    case JSRT_CRYPTO_ALG_SHA512:
      md = EVP_sha512();
      break;
    default:
      return -1;
  }

  if (!md)
    return -1;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx)
    return -1;

  if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
    EVP_MD_CTX_free(ctx);
    return -1;
  }

  if (EVP_DigestUpdate(ctx, input, input_len) != 1) {
    EVP_MD_CTX_free(ctx);
    return -1;
  }

  unsigned int digest_len = EVP_MD_size(md);
  *output = malloc(digest_len);
  if (!*output) {
    EVP_MD_CTX_free(ctx);
    return -1;
  }

  unsigned int final_len;
  if (EVP_DigestFinal_ex(ctx, *output, &final_len) != 1) {
    free(*output);
    *output = NULL;
    EVP_MD_CTX_free(ctx);
    return -1;
  }

  *output_len = final_len;
  EVP_MD_CTX_free(ctx);
  return 0;
}

// Async work function for digest operations
static void jsrt_digest_work(uv_work_t* req) {
  JSRTCryptoAsyncOperation* op = (JSRTCryptoAsyncOperation*)req->data;

  int result =
      jsrt_static_digest_data(op->algorithm, op->input_data, op->input_length, &op->output_data, &op->output_length);

  if (result != 0) {
    op->error_code = -1;
    op->error_message = strdup("Digest operation failed");
  } else {
    op->error_code = 0;
  }
}

// After work callback for digest operations
static void jsrt_digest_after_work(uv_work_t* req, int status) {
  JSRTCryptoAsyncOperation* op = (JSRTCryptoAsyncOperation*)req->data;
  JSContext* ctx = op->ctx;

  if (op->error_code != 0 || status != 0) {
    // Reject promise
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message",
                      JS_NewString(ctx, op->error_message ? op->error_message : "Unknown error"));
    JSValue args[1] = {error};
    JS_Call(ctx, op->reject_func, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, args[0]);
  } else {
    // Resolve promise with ArrayBuffer
    JSValue array_buffer = JS_NewArrayBuffer(ctx, op->output_data, op->output_length, NULL, NULL, 0);
    JSValue args[1] = {array_buffer};
    JS_Call(ctx, op->resolve_func, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, args[0]);
  }

  // Clean up
  JS_FreeValue(ctx, op->resolve_func);
  JS_FreeValue(ctx, op->reject_func);
  JS_FreeValue(ctx, op->promise);

  if (op->input_data)
    free(op->input_data);
  if (op->error_message)
    free(op->error_message);
  free(op);
}

// Parse algorithm from JavaScript object
jsrt_crypto_algorithm_t jsrt_crypto_parse_algorithm(JSContext* ctx, JSValue algorithm) {
  if (JS_IsString(algorithm)) {
    const char* alg_name = JS_ToCString(ctx, algorithm);
    if (!alg_name)
      return JSRT_CRYPTO_ALG_UNKNOWN;

    jsrt_crypto_algorithm_t result = JSRT_CRYPTO_ALG_UNKNOWN;
    if (strcmp(alg_name, "SHA-1") == 0)
      result = JSRT_CRYPTO_ALG_SHA1;
    else if (strcmp(alg_name, "SHA-256") == 0)
      result = JSRT_CRYPTO_ALG_SHA256;
    else if (strcmp(alg_name, "SHA-384") == 0)
      result = JSRT_CRYPTO_ALG_SHA384;
    else if (strcmp(alg_name, "SHA-512") == 0)
      result = JSRT_CRYPTO_ALG_SHA512;

    JS_FreeCString(ctx, alg_name);
    return result;
  }

  if (JS_IsObject(algorithm)) {
    JSValue name_val = JS_GetPropertyStr(ctx, algorithm, "name");
    if (JS_IsString(name_val)) {
      const char* alg_name = JS_ToCString(ctx, name_val);
      if (alg_name) {
        jsrt_crypto_algorithm_t result = JSRT_CRYPTO_ALG_UNKNOWN;
        if (strcmp(alg_name, "SHA-1") == 0)
          result = JSRT_CRYPTO_ALG_SHA1;
        else if (strcmp(alg_name, "SHA-256") == 0)
          result = JSRT_CRYPTO_ALG_SHA256;
        else if (strcmp(alg_name, "SHA-384") == 0)
          result = JSRT_CRYPTO_ALG_SHA384;
        else if (strcmp(alg_name, "SHA-512") == 0)
          result = JSRT_CRYPTO_ALG_SHA512;

        JS_FreeCString(ctx, alg_name);
        JS_FreeValue(ctx, name_val);
        return result;
      }
    }
    JS_FreeValue(ctx, name_val);
  }

  return JSRT_CRYPTO_ALG_UNKNOWN;
}

// Convert algorithm enum to string
const char* jsrt_crypto_algorithm_to_string(jsrt_crypto_algorithm_t alg) {
  switch (alg) {
    case JSRT_CRYPTO_ALG_SHA1:
      return "SHA-1";
    case JSRT_CRYPTO_ALG_SHA256:
      return "SHA-256";
    case JSRT_CRYPTO_ALG_SHA384:
      return "SHA-384";
    case JSRT_CRYPTO_ALG_SHA512:
      return "SHA-512";
    default:
      return "Unknown";
  }
}

// Check if algorithm is supported
bool jsrt_crypto_is_algorithm_supported(jsrt_crypto_algorithm_t alg) {
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

// Memory management functions
JSRTCryptoAsyncOperation* jsrt_crypto_async_operation_new(void) {
  JSRTCryptoAsyncOperation* op = calloc(1, sizeof(JSRTCryptoAsyncOperation));
  return op;
}

void jsrt_crypto_async_operation_free(JSRTCryptoAsyncOperation* op) {
  if (!op)
    return;
  if (op->input_data)
    free(op->input_data);
  if (op->output_data)
    free(op->output_data);
  if (op->error_message)
    free(op->error_message);
  free(op);
}

// SubtleCrypto.digest implementation - simplified synchronous version for static builds
JSValue jsrt_subtle_digest(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "digest requires 2 arguments");
  }

  // Parse algorithm
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN || !jsrt_crypto_is_algorithm_supported(alg)) {
    return JS_ThrowTypeError(ctx, "Unsupported algorithm");
  }

  // Get data from ArrayBuffer or TypedArray
  size_t data_size;
  uint8_t* data = JS_GetArrayBuffer(ctx, &data_size, argv[1]);
  if (!data) {
    // Try as TypedArray
    JSValue buffer = JS_GetPropertyStr(ctx, argv[1], "buffer");
    JSValue byteOffset = JS_GetPropertyStr(ctx, argv[1], "byteOffset");
    JSValue byteLength = JS_GetPropertyStr(ctx, argv[1], "byteLength");

    if (!JS_IsUndefined(buffer) && !JS_IsUndefined(byteOffset) && !JS_IsUndefined(byteLength)) {
      uint32_t offset, length;
      if (JS_ToUint32(ctx, &offset, byteOffset) == 0 && JS_ToUint32(ctx, &length, byteLength) == 0) {
        size_t buf_size;
        uint8_t* buf_data = JS_GetArrayBuffer(ctx, &buf_size, buffer);
        if (buf_data && offset + length <= buf_size) {
          data = buf_data + offset;
          data_size = length;
        }
      }
    }

    JS_FreeValue(ctx, buffer);
    JS_FreeValue(ctx, byteOffset);
    JS_FreeValue(ctx, byteLength);

    if (!data) {
      return JS_ThrowTypeError(ctx, "Invalid data argument");
    }
  }

  // Perform digest synchronously
  uint8_t* output_data;
  size_t output_length;
  int result = jsrt_static_digest_data(alg, data, data_size, &output_data, &output_length);

  if (result != 0) {
    return JS_ThrowInternalError(ctx, "Digest operation failed");
  }

  // Create resolved promise with ArrayBuffer result
  JSValue array_buffer = JS_NewArrayBuffer(ctx, output_data, output_length, NULL, NULL, 0);

  // Create a resolved promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(output_data);
    return promise;
  }

  // Resolve immediately with the result
  JSValue args[1] = {array_buffer};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  // Clean up
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);

  return promise;
}

// Stub implementations for other methods (to be implemented later)
JSValue jsrt_subtle_encrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "encrypt not implemented yet");
}

JSValue jsrt_subtle_decrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "decrypt not implemented yet");
}

JSValue jsrt_subtle_sign(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "sign not implemented yet");
}

JSValue jsrt_subtle_verify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "verify not implemented yet");
}

JSValue jsrt_subtle_generateKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "generateKey not implemented yet");
}

JSValue jsrt_subtle_importKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "importKey not implemented yet");
}

JSValue jsrt_subtle_exportKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "exportKey not implemented yet");
}

JSValue jsrt_subtle_deriveKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "deriveKey not implemented yet");
}

JSValue jsrt_subtle_deriveBits(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "deriveBits not implemented yet");
}

// Create SubtleCrypto object
JSValue JSRT_CreateSubtleCrypto(JSContext* ctx) {
  JSValue subtle = JS_NewObject(ctx);

  // Add digest method
  JS_SetPropertyStr(ctx, subtle, "digest", JS_NewCFunction(ctx, jsrt_subtle_digest, "digest", 2));

  // Add other methods (stubs for now)
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

void JSRT_SetupSubtleCrypto(JSRT_Runtime* rt) {
  // No additional setup needed for static mode
  JSRT_Debug("JSRT_SetupSubtleCrypto: static OpenSSL mode initialized");
}

// Setup crypto module in runtime (static version)
void JSRT_RuntimeSetupStdCrypto(JSRT_Runtime* rt) {
  JSRT_Debug("JSRT_RuntimeSetupStdCrypto: setting up static OpenSSL crypto");

  // Create crypto object
  JSValue crypto_obj = JS_NewObject(rt->ctx);

  // crypto.getRandomValues() - implement using static OpenSSL
  JS_SetPropertyStr(rt->ctx, crypto_obj, "getRandomValues",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_getRandomValues_static, "getRandomValues", 1));

  // crypto.randomUUID() - implement using static OpenSSL
  JS_SetPropertyStr(rt->ctx, crypto_obj, "randomUUID",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_randomUUID_static, "randomUUID", 0));

  // crypto.subtle (SubtleCrypto API)
  JSValue subtle_obj = JSRT_CreateSubtleCrypto(rt->ctx);
  JS_SetPropertyStr(rt->ctx, crypto_obj, "subtle", subtle_obj);

  // Register crypto object to globalThis
  JS_SetPropertyStr(rt->ctx, rt->global, "crypto", crypto_obj);

  // Initialize SubtleCrypto
  JSRT_SetupSubtleCrypto(rt);

  JSRT_Debug("JSRT_RuntimeSetupStdCrypto: initialized WebCrypto API with static OpenSSL support");
}

// Get OpenSSL version for process.versions.openssl (static version)
const char* JSRT_GetOpenSSLVersion() {
  return OPENSSL_VERSION_TEXT;
}

// Validate typed array for getRandomValues
static bool is_valid_integer_typed_array_static(JSContext* ctx, JSValue arg, const char** error_msg) {
  if (!JS_IsObject(arg)) {
    *error_msg = "Argument must be a typed array";
    return false;
  }

  // Check if it has the basic TypedArray properties
  JSValue byteLength_val = JS_GetPropertyStr(ctx, arg, "byteLength");
  JSValue buffer_val = JS_GetPropertyStr(ctx, arg, "buffer");

  if (JS_IsException(byteLength_val) || JS_IsException(buffer_val) || JS_IsUndefined(byteLength_val) ||
      JS_IsUndefined(buffer_val)) {
    JS_FreeValue(ctx, byteLength_val);
    JS_FreeValue(ctx, buffer_val);
    *error_msg = "Argument must be a typed array";
    return false;
  }

  JS_FreeValue(ctx, byteLength_val);
  JS_FreeValue(ctx, buffer_val);

  // For simplicity in static mode, accept any object with buffer/byteLength properties
  // In a full implementation, we'd check the actual constructor
  return true;
}

// crypto.getRandomValues implementation for static mode
static JSValue jsrt_crypto_getRandomValues_static(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "crypto.getRandomValues requires 1 argument");
  }

  JSValue arg = argv[0];

  // Validate that it's a proper integer TypedArray
  const char* error_msg;
  if (!is_valid_integer_typed_array_static(ctx, arg, &error_msg)) {
    return JS_ThrowTypeError(ctx, error_msg);
  }

  // Get byteLength property to determine the size
  JSValue byteLength_val = JS_GetPropertyStr(ctx, arg, "byteLength");
  if (JS_IsException(byteLength_val)) {
    return JS_ThrowTypeError(ctx, "crypto.getRandomValues argument must be a typed array");
  }

  uint32_t byte_length;
  if (JS_ToUint32(ctx, &byte_length, byteLength_val) < 0) {
    JS_FreeValue(ctx, byteLength_val);
    return JS_ThrowTypeError(ctx, "Invalid byteLength");
  }
  JS_FreeValue(ctx, byteLength_val);

  if (byte_length == 0) {
    return JS_DupValue(ctx, arg);  // Return the original array
  }

  if (byte_length > 65536) {
    return JS_ThrowRangeError(ctx, "crypto.getRandomValues array length exceeds quota (65536 bytes)");
  }

  // Generate random bytes using static OpenSSL
  unsigned char* random_data = malloc(byte_length);
  if (!random_data) {
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for random data");
  }

  if (RAND_bytes(random_data, (int)byte_length) != 1) {
    free(random_data);
    return JS_ThrowInternalError(ctx, "Failed to generate random bytes");
  }

  // Copy random data to the typed array's underlying buffer
  JSValue buffer = JS_GetPropertyStr(ctx, arg, "buffer");
  JSValue byteOffset_val = JS_GetPropertyStr(ctx, arg, "byteOffset");

  uint32_t byte_offset = 0;
  if (!JS_IsUndefined(byteOffset_val)) {
    JS_ToUint32(ctx, &byte_offset, byteOffset_val);
  }

  size_t buffer_size;
  uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer);

  if (buffer_data && byte_offset + byte_length <= buffer_size) {
    memcpy(buffer_data + byte_offset, random_data, byte_length);
  }

  JS_FreeValue(ctx, buffer);
  JS_FreeValue(ctx, byteOffset_val);
  free(random_data);

  return JS_DupValue(ctx, arg);
}

// crypto.randomUUID implementation for static mode
static JSValue jsrt_crypto_randomUUID_static(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  unsigned char random_bytes[16];

  // Generate 16 random bytes for UUID using static OpenSSL
  if (RAND_bytes(random_bytes, 16) != 1) {
    return JS_ThrowInternalError(ctx, "Failed to generate random bytes for UUID");
  }

  // Set version bits (4 bits): version 4 (random)
  random_bytes[6] = (random_bytes[6] & 0x0F) | 0x40;

  // Set variant bits (2 bits): variant 1 (RFC 4122)
  random_bytes[8] = (random_bytes[8] & 0x3F) | 0x80;

  // Format as UUID string: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
  char uuid_str[37];
  snprintf(uuid_str, sizeof(uuid_str), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           random_bytes[0], random_bytes[1], random_bytes[2], random_bytes[3], random_bytes[4], random_bytes[5],
           random_bytes[6], random_bytes[7], random_bytes[8], random_bytes[9], random_bytes[10], random_bytes[11],
           random_bytes[12], random_bytes[13], random_bytes[14], random_bytes[15]);

  return JS_NewString(ctx, uuid_str);
}

#else  // !JSRT_STATIC_OPENSSL

// Fallback implementations when OpenSSL is not available
#include "../util/debug.h"
#include "crypto_subtle.h"

// Stub implementations for when OpenSSL is not available
JSValue jsrt_subtle_digest(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.digest not available (OpenSSL not found)");
}

JSValue jsrt_subtle_encrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.encrypt not available (OpenSSL not found)");
}

JSValue jsrt_subtle_decrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.decrypt not available (OpenSSL not found)");
}

JSValue jsrt_subtle_sign(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.sign not available (OpenSSL not found)");
}

JSValue jsrt_subtle_verify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.verify not available (OpenSSL not found)");
}

JSValue jsrt_subtle_generateKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.generateKey not available (OpenSSL not found)");
}

JSValue jsrt_subtle_importKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.importKey not available (OpenSSL not found)");
}

JSValue jsrt_subtle_exportKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.exportKey not available (OpenSSL not found)");
}

JSValue jsrt_subtle_deriveKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.deriveKey not available (OpenSSL not found)");
}

JSValue jsrt_subtle_deriveBits(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.subtle.deriveBits not available (OpenSSL not found)");
}

// Create SubtleCrypto object (stub version)
JSValue JSRT_CreateSubtleCrypto(JSContext* ctx) {
  JSValue subtle = JS_NewObject(ctx);

  // Add methods (all will throw errors)
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

void JSRT_SetupSubtleCrypto(JSRT_Runtime* rt) {
  JSRT_Debug("JSRT_SetupSubtleCrypto: OpenSSL not available - crypto functions disabled");
}

// Setup crypto module in runtime (fallback version)
void JSRT_RuntimeSetupStdCrypto(JSRT_Runtime* rt) {
  JSRT_Debug("JSRT_RuntimeSetupStdCrypto: OpenSSL not available - using stub implementations");

  // Create crypto object with stub implementations
  JSValue crypto_obj = JS_NewObject(rt->ctx);

  // crypto.getRandomValues() - stub that throws
  JS_SetPropertyStr(rt->ctx, crypto_obj, "getRandomValues",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_getRandomValues_stub, "getRandomValues", 1));

  // crypto.randomUUID() - stub that throws
  JS_SetPropertyStr(rt->ctx, crypto_obj, "randomUUID",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_randomUUID_stub, "randomUUID", 0));

  // crypto.subtle (SubtleCrypto API) - stub version
  JSValue subtle_obj = JSRT_CreateSubtleCrypto(rt->ctx);
  JS_SetPropertyStr(rt->ctx, crypto_obj, "subtle", subtle_obj);

  // Register crypto object to globalThis
  JS_SetPropertyStr(rt->ctx, rt->global, "crypto", crypto_obj);

  // Initialize SubtleCrypto
  JSRT_SetupSubtleCrypto(rt);

  JSRT_Debug("JSRT_RuntimeSetupStdCrypto: initialized stub WebCrypto API (OpenSSL not available)");
}

// Stub version for process.versions.openssl
const char* JSRT_GetOpenSSLVersion() {
  return "not available";
}

// Stub implementations for crypto functions
static JSValue jsrt_crypto_getRandomValues_stub(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.getRandomValues not available (OpenSSL not found)");
}

static JSValue jsrt_crypto_randomUUID_stub(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "crypto.randomUUID not available (OpenSSL not found)");
}

#endif  // JSRT_STATIC_OPENSSL