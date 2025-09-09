// Static OpenSSL build - full SubtleCrypto implementation using unified backend
#include <quickjs.h>

// Only compile static OpenSSL implementation when OpenSSL is available
#ifdef JSRT_STATIC_OPENSSL
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/params.h>
#include <openssl/core_names.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../util/debug.h"
#include "crypto.h"  // Include for unified crypto functions
#include "crypto_subtle.h"
#include "crypto_backend.h"
#include "crypto_symmetric.h"

// Forward declarations for static implementations
static JSValue jsrt_rsa_encrypt(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg);
static JSValue jsrt_rsa_decrypt(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg);
static JSValue jsrt_rsa_sign(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg);
static JSValue jsrt_rsa_verify(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg);
static JSValue jsrt_generate_ec_key_pair(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg);
static JSValue jsrt_ecdsa_sign(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg);
static JSValue jsrt_import_pbkdf2_key(JSContext* ctx, JSValueConst* argv);
static JSValue jsrt_pbkdf2_derive_key(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg);
static JSValue jsrt_ecdsa_verify(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg);
static JSValue jsrt_ecdh_derive_key(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg);

// Helper function to get ArrayBuffer data with Typed Array support
static bool jsrt_get_array_buffer_data(JSContext* ctx, JSValueConst val, uint8_t** data, size_t* size) {
  *data = NULL;
  *size = 0;

  // Try direct ArrayBuffer first
  *data = JS_GetArrayBuffer(ctx, size, val);
  if (*data != NULL) {
    return true;
  }

  // Try getting from buffer property of Typed Array
  JSValue buffer = JS_GetPropertyStr(ctx, val, "buffer");
  if (!JS_IsException(buffer)) {
    JSValue byteOffset = JS_GetPropertyStr(ctx, val, "byteOffset");
    JSValue byteLength = JS_GetPropertyStr(ctx, val, "byteLength");
    
    if (!JS_IsException(byteOffset) && !JS_IsException(byteLength)) {
      size_t buf_size;
      uint8_t* buf_data = JS_GetArrayBuffer(ctx, &buf_size, buffer);
      
      if (buf_data != NULL) {
        int32_t offset, length;
        if (JS_ToInt32(ctx, &offset, byteOffset) == 0 && 
            JS_ToInt32(ctx, &length, byteLength) == 0 &&
            offset >= 0 && length >= 0 && 
            (size_t)(offset + length) <= buf_size) {
          *data = buf_data + offset;
          *size = length;
          
          JS_FreeValue(ctx, buffer);
          JS_FreeValue(ctx, byteOffset);
          JS_FreeValue(ctx, byteLength);
          return true;
        }
      }
    }
    JS_FreeValue(ctx, byteOffset);
    JS_FreeValue(ctx, byteLength);
  }
  JS_FreeValue(ctx, buffer);

  return false;
}

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
    else if (strcmp(alg_name, "AES-CBC") == 0)
      result = JSRT_CRYPTO_ALG_AES_CBC;
    else if (strcmp(alg_name, "AES-GCM") == 0)
      result = JSRT_CRYPTO_ALG_AES_GCM;
    else if (strcmp(alg_name, "AES-CTR") == 0)
      result = JSRT_CRYPTO_ALG_AES_CTR;
    else if (strcmp(alg_name, "RSA-OAEP") == 0)
      result = JSRT_CRYPTO_ALG_RSA_OAEP;
    else if (strcmp(alg_name, "RSA-PSS") == 0)
      result = JSRT_CRYPTO_ALG_RSA_PSS;
    else if (strcmp(alg_name, "RSASSA-PKCS1-v1_5") == 0)
      result = JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5;
    else if (strcmp(alg_name, "RSA-PKCS1-v1_5") == 0)
      result = JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5;
    else if (strcmp(alg_name, "ECDSA") == 0)
      result = JSRT_CRYPTO_ALG_ECDSA;
    else if (strcmp(alg_name, "ECDH") == 0)
      result = JSRT_CRYPTO_ALG_ECDH;
    else if (strcmp(alg_name, "HMAC") == 0)
      result = JSRT_CRYPTO_ALG_HMAC;
    else if (strcmp(alg_name, "PBKDF2") == 0)
      result = JSRT_CRYPTO_ALG_PBKDF2;
    else if (strcmp(alg_name, "HKDF") == 0)
      result = JSRT_CRYPTO_ALG_HKDF;

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
        else if (strcmp(alg_name, "AES-CBC") == 0)
          result = JSRT_CRYPTO_ALG_AES_CBC;
        else if (strcmp(alg_name, "AES-GCM") == 0)
          result = JSRT_CRYPTO_ALG_AES_GCM;
        else if (strcmp(alg_name, "AES-CTR") == 0)
          result = JSRT_CRYPTO_ALG_AES_CTR;
        else if (strcmp(alg_name, "RSA-OAEP") == 0)
          result = JSRT_CRYPTO_ALG_RSA_OAEP;
        else if (strcmp(alg_name, "RSA-PSS") == 0)
          result = JSRT_CRYPTO_ALG_RSA_PSS;
        else if (strcmp(alg_name, "RSASSA-PKCS1-v1_5") == 0)
          result = JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5;
        else if (strcmp(alg_name, "RSA-PKCS1-v1_5") == 0)
          result = JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5;
        else if (strcmp(alg_name, "ECDSA") == 0)
          result = JSRT_CRYPTO_ALG_ECDSA;
        else if (strcmp(alg_name, "ECDH") == 0)
          result = JSRT_CRYPTO_ALG_ECDH;
        else if (strcmp(alg_name, "HMAC") == 0)
          result = JSRT_CRYPTO_ALG_HMAC;
        else if (strcmp(alg_name, "PBKDF2") == 0)
          result = JSRT_CRYPTO_ALG_PBKDF2;
        else if (strcmp(alg_name, "HKDF") == 0)
          result = JSRT_CRYPTO_ALG_HKDF;

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

// AES encryption implementation using unified backend
JSValue jsrt_subtle_encrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "encrypt requires 3 arguments");
  }

  // Parse algorithm
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg != JSRT_CRYPTO_ALG_AES_CBC && alg != JSRT_CRYPTO_ALG_AES_GCM && alg != JSRT_CRYPTO_ALG_AES_CTR && alg != JSRT_CRYPTO_ALG_RSA_OAEP && alg != JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    return JS_ThrowTypeError(ctx, "Only AES-CBC, AES-GCM, AES-CTR, RSA-OAEP, and RSA-PKCS1-v1_5 encryption are currently supported in static build");
  }

  // Handle RSA encryption separately (no IV needed)
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP || alg == JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    return jsrt_rsa_encrypt(ctx, argv, alg);
  }

  // Get IV/counter from algorithm parameter (different property names for different algorithms)
  JSValue iv_val;
  if (alg == JSRT_CRYPTO_ALG_AES_CTR) {
    iv_val = JS_GetPropertyStr(ctx, argv[0], "counter");
    if (JS_IsException(iv_val)) {
      return JS_ThrowTypeError(ctx, "AES-CTR algorithm must have 'counter' property");
    }
  } else {
    iv_val = JS_GetPropertyStr(ctx, argv[0], "iv");
    if (JS_IsException(iv_val)) {
      return JS_ThrowTypeError(ctx, "Algorithm must have 'iv' property");
    }
  }

  uint8_t* iv_data = NULL;
  size_t iv_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, iv_val, &iv_data, &iv_size)) {
    JS_FreeValue(ctx, iv_val);
    return JS_ThrowTypeError(ctx, "Invalid IV data");
  }
  JS_FreeValue(ctx, iv_val);

  // Validate IV size based on algorithm
  if (alg == JSRT_CRYPTO_ALG_AES_CBC && iv_size != 16) {
    return JS_ThrowTypeError(ctx, "AES-CBC IV must be 16 bytes");
  } else if (alg == JSRT_CRYPTO_ALG_AES_GCM && iv_size != 12) {
    return JS_ThrowTypeError(ctx, "AES-GCM IV must be 12 bytes");
  } else if (alg == JSRT_CRYPTO_ALG_AES_CTR && iv_size != 16) {
    return JS_ThrowTypeError(ctx, "AES-CTR counter must be 16 bytes");
  }

  // Get key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_data = NULL;
  size_t key_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_data, &key_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Get plaintext data
  uint8_t* plaintext_data = NULL;
  size_t plaintext_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &plaintext_data, &plaintext_size)) {
    return JS_ThrowTypeError(ctx, "Invalid plaintext data");
  }

  // Setup encryption parameters based on algorithm
  if (!g_crypto_backend) {
    return JS_ThrowInternalError(ctx, "Crypto backend not available");
  }

  jsrt_symmetric_params_t params;
  params.key_data = key_data;
  params.key_length = key_size;

  if (alg == JSRT_CRYPTO_ALG_AES_CBC) {
    params.algorithm = JSRT_SYMMETRIC_AES_CBC;
    params.params.cbc.iv = iv_data;
    params.params.cbc.iv_length = iv_size;
  } else if (alg == JSRT_CRYPTO_ALG_AES_GCM) {
    params.algorithm = JSRT_SYMMETRIC_AES_GCM;
    params.params.gcm.iv = iv_data;
    params.params.gcm.iv_length = iv_size;
    
    // Get optional additional data
    JSValue aad_val = JS_GetPropertyStr(ctx, argv[0], "additionalData");
    if (!JS_IsUndefined(aad_val) && !JS_IsNull(aad_val)) {
      uint8_t* aad_data = NULL;
      size_t aad_size = 0;
      if (!jsrt_get_array_buffer_data(ctx, aad_val, &aad_data, &aad_size)) {
        JS_FreeValue(ctx, aad_val);
        return JS_ThrowTypeError(ctx, "Invalid additional data");
      }
      params.params.gcm.additional_data = aad_data;
      params.params.gcm.additional_data_length = aad_size;
    } else {
      params.params.gcm.additional_data = NULL;
      params.params.gcm.additional_data_length = 0;
    }
    JS_FreeValue(ctx, aad_val);
    
    // Get tag length (default to 16 bytes if not specified)
    JSValue tagLen_val = JS_GetPropertyStr(ctx, argv[0], "tagLength");
    if (!JS_IsUndefined(tagLen_val) && !JS_IsNull(tagLen_val)) {
      int32_t tag_len;
      if (JS_ToInt32(ctx, &tag_len, tagLen_val) != 0 || tag_len < 4 || tag_len > 16) {
        JS_FreeValue(ctx, tagLen_val);
        return JS_ThrowTypeError(ctx, "Invalid tag length (must be 4-16 bytes)");
      }
      params.params.gcm.tag_length = tag_len;
    } else {
      params.params.gcm.tag_length = 16;  // Default to 128-bit tag
    }
    JS_FreeValue(ctx, tagLen_val);
  } else if (alg == JSRT_CRYPTO_ALG_AES_CTR) {
    params.algorithm = JSRT_SYMMETRIC_AES_CTR;
    params.params.ctr.counter = iv_data;
    params.params.ctr.counter_length = iv_size;
    
    // Get counter length in bits (default to 64 if not specified)
    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    if (!JS_IsUndefined(length_val) && !JS_IsNull(length_val)) {
      int32_t counter_bits;
      if (JS_ToInt32(ctx, &counter_bits, length_val) != 0 || counter_bits < 1 || counter_bits > 128) {
        JS_FreeValue(ctx, length_val);
        return JS_ThrowTypeError(ctx, "Invalid counter length (must be 1-128 bits)");
      }
      params.params.ctr.length = counter_bits;
    } else {
      params.params.ctr.length = 64;  // Default to 64-bit counter
    }
    JS_FreeValue(ctx, length_val);
  }

  uint8_t* ciphertext = NULL;
  size_t ciphertext_length = 0;

  int result = g_crypto_backend->aes_encrypt(&params, plaintext_data, plaintext_size,
                                            &ciphertext, &ciphertext_length);

  if (result != 0) {
    return JS_ThrowInternalError(ctx, "AES encryption failed");
  }

  // Create ArrayBuffer with result
  JSValue array_buffer = JS_NewArrayBuffer(ctx, ciphertext, ciphertext_length, NULL, NULL, 0);

  // Create resolved promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(ciphertext);
    return promise;
  }

  // Resolve with the result
  JSValue args[1] = {array_buffer};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);

  return promise;
}

JSValue jsrt_subtle_decrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "decrypt requires 3 arguments");
  }

  // Parse algorithm
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg != JSRT_CRYPTO_ALG_AES_CBC && alg != JSRT_CRYPTO_ALG_AES_GCM && alg != JSRT_CRYPTO_ALG_AES_CTR && alg != JSRT_CRYPTO_ALG_RSA_OAEP && alg != JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    return JS_ThrowTypeError(ctx, "Only AES-CBC, AES-GCM, AES-CTR, RSA-OAEP, and RSA-PKCS1-v1_5 decryption are currently supported in static build");
  }

  // Handle RSA decryption separately (no IV needed)
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP || alg == JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    return jsrt_rsa_decrypt(ctx, argv, alg);
  }

  // Get IV/counter from algorithm parameter (different property names for different algorithms)
  JSValue iv_val;
  if (alg == JSRT_CRYPTO_ALG_AES_CTR) {
    iv_val = JS_GetPropertyStr(ctx, argv[0], "counter");
    if (JS_IsException(iv_val)) {
      return JS_ThrowTypeError(ctx, "AES-CTR algorithm must have 'counter' property");
    }
  } else {
    iv_val = JS_GetPropertyStr(ctx, argv[0], "iv");
    if (JS_IsException(iv_val)) {
      return JS_ThrowTypeError(ctx, "Algorithm must have 'iv' property");
    }
  }

  uint8_t* iv_data = NULL;
  size_t iv_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, iv_val, &iv_data, &iv_size)) {
    JS_FreeValue(ctx, iv_val);
    return JS_ThrowTypeError(ctx, "Invalid IV data");
  }
  JS_FreeValue(ctx, iv_val);

  // Validate IV size based on algorithm
  if (alg == JSRT_CRYPTO_ALG_AES_CBC && iv_size != 16) {
    return JS_ThrowTypeError(ctx, "AES-CBC IV must be 16 bytes");
  } else if (alg == JSRT_CRYPTO_ALG_AES_GCM && iv_size != 12) {
    return JS_ThrowTypeError(ctx, "AES-GCM IV must be 12 bytes");
  } else if (alg == JSRT_CRYPTO_ALG_AES_CTR && iv_size != 16) {
    return JS_ThrowTypeError(ctx, "AES-CTR counter must be 16 bytes");
  }

  // Get key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_data = NULL;
  size_t key_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_data, &key_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Get ciphertext data
  uint8_t* ciphertext_data = NULL;
  size_t ciphertext_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &ciphertext_data, &ciphertext_size)) {
    return JS_ThrowTypeError(ctx, "Invalid ciphertext data");
  }

  // Setup decryption parameters based on algorithm
  if (!g_crypto_backend) {
    return JS_ThrowInternalError(ctx, "Crypto backend not available");
  }

  jsrt_symmetric_params_t params;
  params.key_data = key_data;
  params.key_length = key_size;

  if (alg == JSRT_CRYPTO_ALG_AES_CBC) {
    params.algorithm = JSRT_SYMMETRIC_AES_CBC;
    params.params.cbc.iv = iv_data;
    params.params.cbc.iv_length = iv_size;
  } else if (alg == JSRT_CRYPTO_ALG_AES_GCM) {
    params.algorithm = JSRT_SYMMETRIC_AES_GCM;
    params.params.gcm.iv = iv_data;
    params.params.gcm.iv_length = iv_size;
    
    // Get optional additional data
    JSValue aad_val = JS_GetPropertyStr(ctx, argv[0], "additionalData");
    if (!JS_IsUndefined(aad_val) && !JS_IsNull(aad_val)) {
      uint8_t* aad_data = NULL;
      size_t aad_size = 0;
      if (!jsrt_get_array_buffer_data(ctx, aad_val, &aad_data, &aad_size)) {
        JS_FreeValue(ctx, aad_val);
        return JS_ThrowTypeError(ctx, "Invalid additional data");
      }
      params.params.gcm.additional_data = aad_data;
      params.params.gcm.additional_data_length = aad_size;
    } else {
      params.params.gcm.additional_data = NULL;
      params.params.gcm.additional_data_length = 0;
    }
    JS_FreeValue(ctx, aad_val);
    
    // Get tag length (default to 16 bytes if not specified)
    JSValue tagLen_val = JS_GetPropertyStr(ctx, argv[0], "tagLength");
    if (!JS_IsUndefined(tagLen_val) && !JS_IsNull(tagLen_val)) {
      int32_t tag_len;
      if (JS_ToInt32(ctx, &tag_len, tagLen_val) != 0 || tag_len < 4 || tag_len > 16) {
        JS_FreeValue(ctx, tagLen_val);
        return JS_ThrowTypeError(ctx, "Invalid tag length (must be 4-16 bytes)");
      }
      params.params.gcm.tag_length = tag_len;
    } else {
      params.params.gcm.tag_length = 16;  // Default to 128-bit tag
    }
    JS_FreeValue(ctx, tagLen_val);
  } else if (alg == JSRT_CRYPTO_ALG_AES_CTR) {
    params.algorithm = JSRT_SYMMETRIC_AES_CTR;
    params.params.ctr.counter = iv_data;
    params.params.ctr.counter_length = iv_size;
    
    // Get counter length in bits (default to 64 if not specified)
    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    if (!JS_IsUndefined(length_val) && !JS_IsNull(length_val)) {
      int32_t counter_bits;
      if (JS_ToInt32(ctx, &counter_bits, length_val) != 0 || counter_bits < 1 || counter_bits > 128) {
        JS_FreeValue(ctx, length_val);
        return JS_ThrowTypeError(ctx, "Invalid counter length (must be 1-128 bits)");
      }
      params.params.ctr.length = counter_bits;
    } else {
      params.params.ctr.length = 64;  // Default to 64-bit counter
    }
    JS_FreeValue(ctx, length_val);
  }

  uint8_t* plaintext = NULL;
  size_t plaintext_length = 0;

  int result = g_crypto_backend->aes_decrypt(&params, ciphertext_data, ciphertext_size,
                                            &plaintext, &plaintext_length);

  if (result != 0) {
    return JS_ThrowInternalError(ctx, "AES decryption failed");
  }

  // Create ArrayBuffer with result
  JSValue array_buffer = JS_NewArrayBuffer(ctx, plaintext, plaintext_length, NULL, NULL, 0);

  // Create resolved promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(plaintext);
    return promise;
  }

  // Resolve with the result
  JSValue args[1] = {array_buffer};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);

  return promise;
}

JSValue jsrt_subtle_sign(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "sign requires 3 arguments");
  }

  // Parse algorithm
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg != JSRT_CRYPTO_ALG_HMAC && alg != JSRT_CRYPTO_ALG_RSA_PSS && alg != JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5 && alg != JSRT_CRYPTO_ALG_ECDSA) {
    return JS_ThrowTypeError(ctx, "Only HMAC, RSA-PSS, RSASSA-PKCS1-v1_5, and ECDSA signing are currently supported in static build");
  }

  // Handle RSA signing separately
  if (alg == JSRT_CRYPTO_ALG_RSA_PSS || alg == JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5) {
    return jsrt_rsa_sign(ctx, argv, alg);
  }

  // Handle ECDSA signing separately  
  if (alg == JSRT_CRYPTO_ALG_ECDSA) {
    return jsrt_ecdsa_sign(ctx, argv, alg);
  }

  // Get key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_data = NULL;
  size_t key_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_data, &key_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Get hash algorithm from key
  JSValue key_alg_val = JS_GetPropertyStr(ctx, argv[1], "algorithm");
  if (JS_IsException(key_alg_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key algorithm");
  }

  JSValue hash_val = JS_GetPropertyStr(ctx, key_alg_val, "hash");
  JS_FreeValue(ctx, key_alg_val);
  if (JS_IsException(hash_val)) {
    return JS_ThrowTypeError(ctx, "HMAC key must have hash algorithm");
  }

  const char* hash_name = JS_ToCString(ctx, hash_val);
  JS_FreeValue(ctx, hash_val);
  if (!hash_name) {
    return JS_ThrowTypeError(ctx, "Invalid hash algorithm name");
  }

  // Parse hash algorithm
  const EVP_MD* md = NULL;
  if (strcmp(hash_name, "SHA-1") == 0) {
    md = EVP_sha1();
  } else if (strcmp(hash_name, "SHA-256") == 0) {
    md = EVP_sha256();
  } else if (strcmp(hash_name, "SHA-384") == 0) {
    md = EVP_sha384();
  } else if (strcmp(hash_name, "SHA-512") == 0) {
    md = EVP_sha512();
  } else {
    JS_FreeCString(ctx, hash_name);
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm for HMAC");
  }
  JS_FreeCString(ctx, hash_name);

  // Get data to sign
  uint8_t* data = NULL;
  size_t data_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &data, &data_size)) {
    return JS_ThrowTypeError(ctx, "Invalid data to sign");
  }

  // Perform HMAC signing
  unsigned int signature_length = EVP_MD_size(md);
  uint8_t* signature = malloc(signature_length);
  if (!signature) {
    return JS_ThrowInternalError(ctx, "Memory allocation failed");
  }

  if (HMAC(md, key_data, (int)key_size, data, data_size, signature, &signature_length) == NULL) {
    free(signature);
    return JS_ThrowInternalError(ctx, "HMAC signing failed");
  }

  // Create ArrayBuffer with result
  JSValue array_buffer = JS_NewArrayBuffer(ctx, signature, signature_length, NULL, NULL, 0);

  // Create resolved promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(signature);
    return promise;
  }

  // Resolve with the result
  JSValue args[1] = {array_buffer};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);

  return promise;
}

JSValue jsrt_subtle_verify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 4) {
    return JS_ThrowTypeError(ctx, "verify requires 4 arguments");
  }

  // Parse algorithm
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg != JSRT_CRYPTO_ALG_HMAC && alg != JSRT_CRYPTO_ALG_RSA_PSS && alg != JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5 && alg != JSRT_CRYPTO_ALG_ECDSA) {
    return JS_ThrowTypeError(ctx, "Only HMAC, RSA-PSS, RSASSA-PKCS1-v1_5, and ECDSA verification are currently supported in static build");
  }

  // Handle RSA verification separately
  if (alg == JSRT_CRYPTO_ALG_RSA_PSS || alg == JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5) {
    return jsrt_rsa_verify(ctx, argv, alg);
  }

  // Handle ECDSA verification separately  
  if (alg == JSRT_CRYPTO_ALG_ECDSA) {
    return jsrt_ecdsa_verify(ctx, argv, alg);
  }

  // Get key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_data = NULL;
  size_t key_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_data, &key_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Get hash algorithm from key
  JSValue key_alg_val = JS_GetPropertyStr(ctx, argv[1], "algorithm");
  if (JS_IsException(key_alg_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key algorithm");
  }

  JSValue hash_val = JS_GetPropertyStr(ctx, key_alg_val, "hash");
  JS_FreeValue(ctx, key_alg_val);
  if (JS_IsException(hash_val)) {
    return JS_ThrowTypeError(ctx, "HMAC key must have hash algorithm");
  }

  const char* hash_name = JS_ToCString(ctx, hash_val);
  JS_FreeValue(ctx, hash_val);
  if (!hash_name) {
    return JS_ThrowTypeError(ctx, "Invalid hash algorithm name");
  }

  // Parse hash algorithm
  const EVP_MD* md = NULL;
  if (strcmp(hash_name, "SHA-1") == 0) {
    md = EVP_sha1();
  } else if (strcmp(hash_name, "SHA-256") == 0) {
    md = EVP_sha256();
  } else if (strcmp(hash_name, "SHA-384") == 0) {
    md = EVP_sha384();
  } else if (strcmp(hash_name, "SHA-512") == 0) {
    md = EVP_sha512();
  } else {
    JS_FreeCString(ctx, hash_name);
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm for HMAC");
  }
  JS_FreeCString(ctx, hash_name);

  // Get signature to verify
  uint8_t* signature_data = NULL;
  size_t signature_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &signature_data, &signature_size)) {
    return JS_ThrowTypeError(ctx, "Invalid signature data");
  }

  // Get data to verify
  uint8_t* data = NULL;
  size_t data_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[3], &data, &data_size)) {
    return JS_ThrowTypeError(ctx, "Invalid data to verify");
  }

  // Compute HMAC for comparison
  unsigned int computed_length = EVP_MD_size(md);
  uint8_t* computed_signature = malloc(computed_length);
  if (!computed_signature) {
    return JS_ThrowInternalError(ctx, "Memory allocation failed");
  }

  if (HMAC(md, key_data, (int)key_size, data, data_size, computed_signature, &computed_length) == NULL) {
    free(computed_signature);
    return JS_ThrowInternalError(ctx, "HMAC computation failed");
  }

  // Compare signatures using constant-time comparison
  bool is_valid = (signature_size == computed_length) && 
                  (CRYPTO_memcmp(signature_data, computed_signature, computed_length) == 0);

  free(computed_signature);

  // Create resolved promise with boolean result
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return promise;
  }

  // Resolve with the verification result
  JSValue result = JS_NewBool(ctx, is_valid);
  JSValue args[1] = {result};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);

  return promise;
}

// Helper function to generate HMAC keys
static JSValue jsrt_generate_hmac_key(JSContext* ctx, JSValueConst* argv) {
  // Get hash algorithm from HMAC algorithm object
  JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
  if (JS_IsException(hash_val)) {
    return JS_ThrowTypeError(ctx, "HMAC algorithm must have 'hash' property");
  }

  const char* hash_name = JS_ToCString(ctx, hash_val);
  JS_FreeValue(ctx, hash_val);
  if (!hash_name) {
    return JS_ThrowTypeError(ctx, "Invalid hash algorithm name");
  }

  // Determine key length based on hash algorithm (recommended lengths)
  size_t key_length;
  if (strcmp(hash_name, "SHA-1") == 0) {
    key_length = 20;  // SHA-1 output size
  } else if (strcmp(hash_name, "SHA-256") == 0) {
    key_length = 32;  // SHA-256 output size
  } else if (strcmp(hash_name, "SHA-384") == 0) {
    key_length = 48;  // SHA-384 output size
  } else if (strcmp(hash_name, "SHA-512") == 0) {
    key_length = 64;  // SHA-512 output size
  } else {
    JS_FreeCString(ctx, hash_name);
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm for HMAC");
  }

  // Generate random key data
  uint8_t* key_data = malloc(key_length);
  if (!key_data) {
    JS_FreeCString(ctx, hash_name);
    return JS_ThrowInternalError(ctx, "Memory allocation failed");
  }

  if (RAND_bytes(key_data, (int)key_length) != 1) {
    free(key_data);
    JS_FreeCString(ctx, hash_name);
    return JS_ThrowInternalError(ctx, "Failed to generate random key data");
  }

  // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(key_data);
    JS_FreeCString(ctx, hash_name);
    return promise;
  }

  // Create CryptoKey object
  JSValue crypto_key = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "secret"));
  JS_SetPropertyStr(ctx, crypto_key, "extractable", JS_NewBool(ctx, true));
  
  // Create algorithm object
  JSValue alg_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, alg_obj, "name", JS_NewString(ctx, "HMAC"));
  JS_SetPropertyStr(ctx, alg_obj, "hash", JS_NewString(ctx, hash_name));
  JS_SetPropertyStr(ctx, crypto_key, "algorithm", alg_obj);
  
  // Store key data as ArrayBuffer
  JSValue key_buffer = JS_NewArrayBuffer(ctx, key_data, key_length, NULL, NULL, 0);
  JS_SetPropertyStr(ctx, crypto_key, "_keyData", key_buffer);

  // Resolve promise with the CryptoKey
  JSValue args[1] = {crypto_key};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);
  JS_FreeCString(ctx, hash_name);

  return promise;
}

// ECDSA/ECDH key pair generation implementation
static JSValue jsrt_generate_ec_key_pair(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Parse curve name from algorithm object
  JSValue named_curve_val = JS_GetPropertyStr(ctx, argv[0], "namedCurve");
  if (JS_IsException(named_curve_val)) {
    // Return rejected promise for WebCrypto API consistency
    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (!JS_IsException(promise)) {
      JSValue error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "EC algorithm must have 'namedCurve' property"));
      JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx, resolving_funcs[0]);
      JS_FreeValue(ctx, resolving_funcs[1]);
      JS_FreeValue(ctx, error);
    }
    return promise;
  }

  const char* curve_name = JS_ToCString(ctx, named_curve_val);
  if (!curve_name) {
    JS_FreeValue(ctx, named_curve_val);
    // Return rejected promise for WebCrypto API consistency
    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (!JS_IsException(promise)) {
      JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, 
              (JSValue[]){JS_NewError(ctx)});
      JS_FreeValue(ctx, resolving_funcs[0]);
      JS_FreeValue(ctx, resolving_funcs[1]);
    }
    return promise;
  }
  JS_FreeValue(ctx, named_curve_val);

  // Map WebCrypto curve names to OpenSSL curve names
  int nid;
  if (strcmp(curve_name, "P-256") == 0) {
    nid = NID_X9_62_prime256v1;  // secp256r1
  } else if (strcmp(curve_name, "P-384") == 0) {
    nid = NID_secp384r1;
  } else if (strcmp(curve_name, "P-521") == 0) {
    nid = NID_secp521r1;
  } else {
    JS_FreeCString(ctx, curve_name);
    // Return rejected promise for WebCrypto API consistency
    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (!JS_IsException(promise)) {
      JSValue error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Unsupported elliptic curve"));
      JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx, resolving_funcs[0]);
      JS_FreeValue(ctx, resolving_funcs[1]);
      JS_FreeValue(ctx, error);
    }
    return promise;
  }
  JS_FreeCString(ctx, curve_name);

  // Create EC key generation context
  EVP_PKEY_CTX* pkey_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  if (!pkey_ctx) {
    return JS_ThrowInternalError(ctx, "Failed to create EC key generation context");
  }

  if (EVP_PKEY_keygen_init(pkey_ctx) <= 0) {
    EVP_PKEY_CTX_free(pkey_ctx);
    return JS_ThrowInternalError(ctx, "Failed to initialize EC key generation");
  }

  // Set the curve using OSSL_PARAM (OpenSSL 3.x compatible)
  OSSL_PARAM params[] = {
    OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, (char*)OBJ_nid2sn(nid), 0),
    OSSL_PARAM_END
  };

  if (EVP_PKEY_CTX_set_params(pkey_ctx, params) <= 0) {
    EVP_PKEY_CTX_free(pkey_ctx);
    return JS_ThrowInternalError(ctx, "Failed to set EC curve parameters");
  }

  // Generate the key pair
  EVP_PKEY* pkey = NULL;
  if (EVP_PKEY_keygen(pkey_ctx, &pkey) <= 0) {
    EVP_PKEY_CTX_free(pkey_ctx);
    return JS_ThrowInternalError(ctx, "Failed to generate EC key pair");
  }
  EVP_PKEY_CTX_free(pkey_ctx);

  // Serialize public key to PEM format
  BIO* pub_bio = BIO_new(BIO_s_mem());
  if (!pub_bio) {
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to create BIO for public key");
  }

  if (PEM_write_bio_PUBKEY(pub_bio, pkey) != 1) {
    BIO_free(pub_bio);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to write public key to PEM format");
  }

  char* pub_pem_data;
  long pub_pem_size = BIO_get_mem_data(pub_bio, &pub_pem_data);
  
  uint8_t* pub_pem_copy = js_malloc(ctx, pub_pem_size);
  memcpy(pub_pem_copy, pub_pem_data, pub_pem_size);
  JSValue public_key_data = JS_NewArrayBufferCopy(ctx, pub_pem_copy, pub_pem_size);
  js_free(ctx, pub_pem_copy);
  BIO_free(pub_bio);

  if (JS_IsException(public_key_data)) {
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to create public key ArrayBuffer");
  }

  // Serialize private key to PEM format
  BIO* priv_bio = BIO_new(BIO_s_mem());
  if (!priv_bio) {
    EVP_PKEY_free(pkey);
    JS_FreeValue(ctx, public_key_data);
    return JS_ThrowInternalError(ctx, "Failed to create BIO for private key");
  }

  if (PEM_write_bio_PKCS8PrivateKey(priv_bio, pkey, NULL, NULL, 0, NULL, NULL) != 1) {
    BIO_free(priv_bio);
    EVP_PKEY_free(pkey);
    JS_FreeValue(ctx, public_key_data);
    return JS_ThrowInternalError(ctx, "Failed to write private key to PEM format");
  }

  char* priv_pem_data;
  long priv_pem_size = BIO_get_mem_data(priv_bio, &priv_pem_data);
  
  uint8_t* priv_pem_copy = js_malloc(ctx, priv_pem_size);
  memcpy(priv_pem_copy, priv_pem_data, priv_pem_size);
  JSValue private_key_data = JS_NewArrayBufferCopy(ctx, priv_pem_copy, priv_pem_size);
  js_free(ctx, priv_pem_copy);
  BIO_free(priv_bio);
  EVP_PKEY_free(pkey);

  if (JS_IsException(private_key_data)) {
    JS_FreeValue(ctx, public_key_data);
    return JS_ThrowInternalError(ctx, "Failed to create private key ArrayBuffer");
  }

  // Create CryptoKey objects
  JSValue public_key = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, public_key, "type", JS_NewString(ctx, "public"));
  JS_SetPropertyStr(ctx, public_key, "extractable", JS_TRUE);
  JS_SetPropertyStr(ctx, public_key, "usages", JS_NewArray(ctx));
  JS_SetPropertyStr(ctx, public_key, "_keyData", public_key_data);

  // Set algorithm object for public key
  JSValue pub_alg = JS_NewObject(ctx);
  if (alg == JSRT_CRYPTO_ALG_ECDSA) {
    JS_SetPropertyStr(ctx, pub_alg, "name", JS_NewString(ctx, "ECDSA"));
  } else {
    JS_SetPropertyStr(ctx, pub_alg, "name", JS_NewString(ctx, "ECDH"));
  }
  JSValue curve_prop = JS_GetPropertyStr(ctx, argv[0], "namedCurve");
  JS_SetPropertyStr(ctx, pub_alg, "namedCurve", curve_prop);
  JS_SetPropertyStr(ctx, public_key, "algorithm", pub_alg);

  JSValue private_key = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, private_key, "type", JS_NewString(ctx, "private"));
  JS_SetPropertyStr(ctx, private_key, "extractable", JS_FALSE);
  JS_SetPropertyStr(ctx, private_key, "usages", JS_NewArray(ctx));
  JS_SetPropertyStr(ctx, private_key, "_keyData", private_key_data);

  // Set algorithm object for private key
  JSValue priv_alg = JS_NewObject(ctx);
  if (alg == JSRT_CRYPTO_ALG_ECDSA) {
    JS_SetPropertyStr(ctx, priv_alg, "name", JS_NewString(ctx, "ECDSA"));
  } else {
    JS_SetPropertyStr(ctx, priv_alg, "name", JS_NewString(ctx, "ECDH"));
  }
  JSValue curve_prop2 = JS_GetPropertyStr(ctx, argv[0], "namedCurve");
  JS_SetPropertyStr(ctx, priv_alg, "namedCurve", curve_prop2);
  JS_SetPropertyStr(ctx, private_key, "algorithm", priv_alg);

  // Create and return the key pair object
  JSValue key_pair = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, key_pair, "publicKey", public_key);
  JS_SetPropertyStr(ctx, key_pair, "privateKey", private_key);

  // Wrap in a resolved promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeValue(ctx, key_pair);
    return promise;
  }

  JSValue args[1] = {key_pair};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);

  return promise;
}

static JSValue jsrt_generate_rsa_key_pair(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Parse RSA parameters from algorithm object
  JSValue modulus_length_val = JS_GetPropertyStr(ctx, argv[0], "modulusLength");
  if (JS_IsException(modulus_length_val)) {
    return JS_ThrowTypeError(ctx, "RSA algorithm must have 'modulusLength' property");
  }

  int32_t modulus_length;
  if (JS_ToInt32(ctx, &modulus_length, modulus_length_val) < 0) {
    JS_FreeValue(ctx, modulus_length_val);
    return JS_ThrowTypeError(ctx, "Invalid modulus length");
  }
  JS_FreeValue(ctx, modulus_length_val);

  // Validate modulus length (common RSA key sizes)
  if (modulus_length < 1024 || modulus_length > 8192 || (modulus_length % 256) != 0) {
    return JS_ThrowTypeError(ctx, "Invalid RSA modulus length. Must be between 1024-8192 bits and divisible by 256");
  }

  // Get public exponent (default is 65537)
  JSValue public_exponent_val = JS_GetPropertyStr(ctx, argv[0], "publicExponent");
  if (JS_IsException(public_exponent_val)) {
    return JS_ThrowTypeError(ctx, "RSA algorithm must have 'publicExponent' property");
  }

  uint8_t* public_exponent_data = NULL;
  size_t public_exponent_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, public_exponent_val, &public_exponent_data, &public_exponent_size)) {
    JS_FreeValue(ctx, public_exponent_val);
    return JS_ThrowTypeError(ctx, "Invalid public exponent data");
  }
  JS_FreeValue(ctx, public_exponent_val);

  // Convert public exponent to OpenSSL BIGNUM
  BIGNUM* public_exp = BN_new();
  if (!public_exp) {
    return JS_ThrowInternalError(ctx, "Failed to allocate public exponent");
  }

  if (!BN_bin2bn(public_exponent_data, (int)public_exponent_size, public_exp)) {
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to convert public exponent");
  }

  // Get hash algorithm for PSS/OAEP (optional for RSA-PKCS1)
  const char* hash_name = "SHA-256";  // Default
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP || alg == JSRT_CRYPTO_ALG_RSA_PSS) {
    JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
    if (!JS_IsException(hash_val)) {
      const char* temp_hash = JS_ToCString(ctx, hash_val);
      if (temp_hash) {
        hash_name = temp_hash;
      }
      JS_FreeValue(ctx, hash_val);
    }
  }

  // Generate RSA key pair using OpenSSL EVP API
  EVP_PKEY_CTX* pkey_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  if (!pkey_ctx) {
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to create RSA key generation context");
  }

  if (EVP_PKEY_keygen_init(pkey_ctx) <= 0) {
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to initialize RSA key generation");
  }

  // For OpenSSL 3.x, use parameter setting API
  OSSL_PARAM params[3];
  OSSL_PARAM *p = params;
  
  // Convert BIGNUM to buffer for OSSL_PARAM
  int bn_size = BN_num_bytes(public_exp);
  uint8_t* bn_buf = malloc(bn_size);
  if (!bn_buf) {
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for public exponent buffer");
  }
  BN_bn2bin(public_exp, bn_buf);
  
  *p++ = OSSL_PARAM_construct_int(OSSL_PKEY_PARAM_RSA_BITS, &modulus_length);
  *p++ = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_E, bn_buf, bn_size);
  *p = OSSL_PARAM_construct_end();

  if (EVP_PKEY_CTX_set_params(pkey_ctx, params) <= 0) {
    free(bn_buf);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to set RSA parameters");
  }

  // Generate the key pair
  EVP_PKEY* pkey = NULL;
  if (EVP_PKEY_keygen(pkey_ctx, &pkey) <= 0) {
    free(bn_buf);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to generate RSA key pair");
  }

  // Clean up temporary buffer
  free(bn_buf);

  // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return promise;
  }

  // Create key pair object
  JSValue key_pair = JS_NewObject(ctx);

  // Create public key object
  JSValue public_key = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, public_key, "type", JS_NewString(ctx, "public"));
  JS_SetPropertyStr(ctx, public_key, "extractable", JS_NewBool(ctx, true));

  // Create algorithm object for public key
  JSValue pub_alg_obj = JS_NewObject(ctx);
  const char* alg_name = "RSA-OAEP";
  switch (alg) {
    case JSRT_CRYPTO_ALG_RSA_PSS:
      alg_name = "RSA-PSS";
      break;
    case JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5:
      alg_name = "RSASSA-PKCS1-v1_5";
      break;
    case JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5:
      alg_name = "RSA-PKCS1-v1_5";
      break;
    default:
      alg_name = "RSA-OAEP";
      break;
  }
  JS_SetPropertyStr(ctx, pub_alg_obj, "name", JS_NewString(ctx, alg_name));
  JS_SetPropertyStr(ctx, pub_alg_obj, "modulusLength", JS_NewInt32(ctx, modulus_length));
  JS_SetPropertyStr(ctx, pub_alg_obj, "publicExponent", JS_NewArrayBuffer(ctx, public_exponent_data, public_exponent_size, NULL, NULL, 0));
  
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP || alg == JSRT_CRYPTO_ALG_RSA_PSS) {
    JSValue hash_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, hash_obj, "name", JS_NewString(ctx, hash_name));
    JS_SetPropertyStr(ctx, pub_alg_obj, "hash", hash_obj);
  }
  
  JS_SetPropertyStr(ctx, public_key, "algorithm", pub_alg_obj);

  // Serialize public key to DER format using BIO
  BIO* pub_bio = BIO_new(BIO_s_mem());
  if (!pub_bio) {
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to create BIO for public key");
  }

  if (PEM_write_bio_PUBKEY(pub_bio, pkey) != 1) {
    BIO_free(pub_bio);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to serialize public key to PEM");
  }

  char* pub_der_data_temp;
  long pub_der_len = BIO_get_mem_data(pub_bio, &pub_der_data_temp);
  if (pub_der_len <= 0) {
    BIO_free(pub_bio);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to get public key DER data");
  }

  uint8_t* pub_der_data = malloc(pub_der_len);
  if (!pub_der_data) {
    BIO_free(pub_bio);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for public key DER");
  }

  memcpy(pub_der_data, pub_der_data_temp, pub_der_len);
  BIO_free(pub_bio);

  JSValue pub_key_data = JS_NewArrayBuffer(ctx, pub_der_data, pub_der_len, NULL, NULL, 0);
  JS_SetPropertyStr(ctx, public_key, "_keyData", pub_key_data);

  // Create private key object
  JSValue private_key = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, private_key, "type", JS_NewString(ctx, "private"));
  JS_SetPropertyStr(ctx, private_key, "extractable", JS_NewBool(ctx, false));  // Private keys typically not extractable
  
  // Create algorithm object for private key (same as public key)
  JSValue priv_alg_obj = JS_DupValue(ctx, pub_alg_obj);
  JS_SetPropertyStr(ctx, private_key, "algorithm", priv_alg_obj);

  // Serialize private key to PKCS#8 DER format using BIO
  BIO* priv_bio = BIO_new(BIO_s_mem());
  if (!priv_bio) {
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to create BIO for private key");
  }

  if (PEM_write_bio_PKCS8PrivateKey(priv_bio, pkey, NULL, NULL, 0, NULL, NULL) != 1) {
    BIO_free(priv_bio);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to serialize private key to PEM");
  }

  char* priv_der_data_temp;
  long priv_der_len = BIO_get_mem_data(priv_bio, &priv_der_data_temp);
  if (priv_der_len <= 0) {
    BIO_free(priv_bio);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to get private key DER data");
  }

  uint8_t* priv_der_data = malloc(priv_der_len);
  if (!priv_der_data) {
    BIO_free(priv_bio);
    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(pkey_ctx);
    BN_free(public_exp);
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for private key DER");
  }

  memcpy(priv_der_data, priv_der_data_temp, priv_der_len);
  BIO_free(priv_bio);

  JSValue priv_key_data = JS_NewArrayBuffer(ctx, priv_der_data, priv_der_len, NULL, NULL, 0);
  JS_SetPropertyStr(ctx, private_key, "_keyData", priv_key_data);

  // Add keys to key pair object
  JS_SetPropertyStr(ctx, key_pair, "publicKey", public_key);
  JS_SetPropertyStr(ctx, key_pair, "privateKey", private_key);

  // Resolve promise with the key pair
  JSValue args[1] = {key_pair};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  // Clean up
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);
  EVP_PKEY_CTX_free(pkey_ctx);
  BN_free(public_exp);

  return promise;
}

static JSValue jsrt_rsa_encrypt(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Get plaintext data
  uint8_t* plaintext_data = NULL;
  size_t plaintext_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &plaintext_data, &plaintext_size)) {
    return JS_ThrowTypeError(ctx, "Invalid plaintext data");
  }

  // Get public key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_der_data = NULL;
  size_t key_der_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_der_data, &key_der_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Parse key from DER/PEM data
  BIO* key_bio = BIO_new_mem_buf(key_der_data, (int)key_der_size);
  if (!key_bio) {
    return JS_ThrowInternalError(ctx, "Failed to create BIO for key data");
  }

  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(key_bio, NULL, NULL, NULL);
  BIO_free(key_bio);

  if (!pkey) {
    return JS_ThrowInternalError(ctx, "Failed to parse RSA public key");
  }

  // Verify this is an RSA key
  if (EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA) {
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Key is not an RSA key");
  }

  // Get hash algorithm from algorithm object  
  const char* hash_name = "sha256";  // Default OpenSSL name
  JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
  if (!JS_IsException(hash_val)) {
    // Check if hash is a string directly
    const char* temp_hash = JS_ToCString(ctx, hash_val);
    if (temp_hash) {
      // Convert WebCrypto hash names to OpenSSL names
      if (strcmp(temp_hash, "SHA-1") == 0) {
        hash_name = "sha1";
      } else if (strcmp(temp_hash, "SHA-256") == 0) {
        hash_name = "sha256"; 
      } else if (strcmp(temp_hash, "SHA-384") == 0) {
        hash_name = "sha384";
      } else if (strcmp(temp_hash, "SHA-512") == 0) {
        hash_name = "sha512";
      }
      JS_FreeCString(ctx, temp_hash);
    } else {
      // Try to get name property from hash object
      JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
      if (!JS_IsException(hash_name_val)) {
        const char* temp_hash_name = JS_ToCString(ctx, hash_name_val);
        if (temp_hash_name) {
          // Convert WebCrypto hash names to OpenSSL names
          if (strcmp(temp_hash_name, "SHA-1") == 0) {
            hash_name = "sha1";
          } else if (strcmp(temp_hash_name, "SHA-256") == 0) {
            hash_name = "sha256";
          } else if (strcmp(temp_hash_name, "SHA-384") == 0) {
            hash_name = "sha384";
          } else if (strcmp(temp_hash_name, "SHA-512") == 0) {
            hash_name = "sha512";
          }
          JS_FreeCString(ctx, temp_hash_name);
        }
        JS_FreeValue(ctx, hash_name_val);
      }
    }
    JS_FreeValue(ctx, hash_val);
  }

  // Get optional label
  uint8_t* label_data = NULL;
  size_t label_size = 0;
  JSValue label_val = JS_GetPropertyStr(ctx, argv[0], "label");
  if (!JS_IsUndefined(label_val) && !JS_IsNull(label_val)) {
    if (!jsrt_get_array_buffer_data(ctx, label_val, &label_data, &label_size)) {
      JS_FreeValue(ctx, label_val);
      EVP_PKEY_free(pkey);
      return JS_ThrowTypeError(ctx, "Invalid label data");
    }
  }
  JS_FreeValue(ctx, label_val);

  // Create encryption context
  EVP_PKEY_CTX* encrypt_ctx = EVP_PKEY_CTX_new(pkey, NULL);
  if (!encrypt_ctx) {
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to create RSA encryption context");
  }

  if (EVP_PKEY_encrypt_init(encrypt_ctx) <= 0) {
    EVP_PKEY_CTX_free(encrypt_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to initialize RSA encryption");
  }

  // Set padding based on algorithm
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP) {
    // Set OAEP padding and hash algorithm
    if (EVP_PKEY_CTX_set_rsa_padding(encrypt_ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
      EVP_PKEY_CTX_free(encrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA OAEP padding");
    }

    // Set hash algorithm
    const EVP_MD* md = EVP_get_digestbyname(hash_name);
    if (!md) {
      EVP_PKEY_CTX_free(encrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowTypeError(ctx, "Unsupported hash algorithm");
    }

    if (EVP_PKEY_CTX_set_rsa_oaep_md(encrypt_ctx, md) <= 0) {
      EVP_PKEY_CTX_free(encrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA OAEP hash algorithm");
    }

    // Set MGF1 hash algorithm (same as main hash for simplicity)
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(encrypt_ctx, md) <= 0) {
      EVP_PKEY_CTX_free(encrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA MGF1 hash algorithm");
    }

    // Set label if provided
    if (label_data && label_size > 0) {
      // Create a copy of label data since EVP_PKEY_CTX_set0_rsa_oaep_label takes ownership
      uint8_t* label_copy = malloc(label_size);
      if (!label_copy) {
        EVP_PKEY_CTX_free(encrypt_ctx);
        EVP_PKEY_free(pkey);
        return JS_ThrowInternalError(ctx, "Failed to allocate memory for label copy");
      }
      memcpy(label_copy, label_data, label_size);
      
      if (EVP_PKEY_CTX_set0_rsa_oaep_label(encrypt_ctx, label_copy, label_size) <= 0) {
        free(label_copy);
        EVP_PKEY_CTX_free(encrypt_ctx);
        EVP_PKEY_free(pkey);
        return JS_ThrowInternalError(ctx, "Failed to set RSA OAEP label");
      }
    }
  } else if (alg == JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    // Set PKCS1-v1_5 padding (no hash algorithm or label needed)
    if (EVP_PKEY_CTX_set_rsa_padding(encrypt_ctx, RSA_PKCS1_PADDING) <= 0) {
      EVP_PKEY_CTX_free(encrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PKCS1-v1_5 padding");
    }
  }

  // Get output length
  size_t ciphertext_len = 0;
  if (EVP_PKEY_encrypt(encrypt_ctx, NULL, &ciphertext_len, plaintext_data, plaintext_size) <= 0) {
    EVP_PKEY_CTX_free(encrypt_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to get RSA encryption output length");
  }

  // Allocate output buffer
  uint8_t* ciphertext = malloc(ciphertext_len);
  if (!ciphertext) {
    EVP_PKEY_CTX_free(encrypt_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for ciphertext");
  }

  // Perform encryption
  if (EVP_PKEY_encrypt(encrypt_ctx, ciphertext, &ciphertext_len, plaintext_data, plaintext_size) <= 0) {
    free(ciphertext);
    EVP_PKEY_CTX_free(encrypt_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "RSA encryption failed");
  }

  // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(ciphertext);
    EVP_PKEY_CTX_free(encrypt_ctx);
    EVP_PKEY_free(pkey);
    return promise;
  }

  // Create ArrayBuffer for ciphertext
  JSValue ciphertext_buffer = JS_NewArrayBuffer(ctx, ciphertext, ciphertext_len, NULL, NULL, 0);

  // Resolve promise with the ciphertext
  JSValue args[1] = {ciphertext_buffer};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  // Clean up
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);
  EVP_PKEY_CTX_free(encrypt_ctx);
  EVP_PKEY_free(pkey);

  return promise;
}

static JSValue jsrt_rsa_decrypt(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Get ciphertext data
  uint8_t* ciphertext_data = NULL;
  size_t ciphertext_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &ciphertext_data, &ciphertext_size)) {
    return JS_ThrowTypeError(ctx, "Invalid ciphertext data");
  }

  // Get private key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_der_data = NULL;
  size_t key_der_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_der_data, &key_der_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Parse key from PEM data
  BIO* key_bio = BIO_new_mem_buf(key_der_data, (int)key_der_size);
  if (!key_bio) {
    return JS_ThrowInternalError(ctx, "Failed to create BIO for key data");
  }

  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, NULL, NULL, NULL);
  BIO_free(key_bio);

  if (!pkey) {
    return JS_ThrowInternalError(ctx, "Failed to parse RSA private key");
  }

  // Verify this is an RSA key
  if (EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA) {
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Key is not an RSA key");
  }

  // Get hash algorithm from algorithm object  
  const char* hash_name = "sha256";  // Default OpenSSL name
  JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
  if (!JS_IsException(hash_val)) {
    // Check if hash is a string directly
    const char* temp_hash = JS_ToCString(ctx, hash_val);
    if (temp_hash) {
      // Convert WebCrypto hash names to OpenSSL names
      if (strcmp(temp_hash, "SHA-1") == 0) {
        hash_name = "sha1";
      } else if (strcmp(temp_hash, "SHA-256") == 0) {
        hash_name = "sha256"; 
      } else if (strcmp(temp_hash, "SHA-384") == 0) {
        hash_name = "sha384";
      } else if (strcmp(temp_hash, "SHA-512") == 0) {
        hash_name = "sha512";
      }
      JS_FreeCString(ctx, temp_hash);
    } else {
      // Try to get name property from hash object
      JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
      if (!JS_IsException(hash_name_val)) {
        const char* temp_hash_name = JS_ToCString(ctx, hash_name_val);
        if (temp_hash_name) {
          // Convert WebCrypto hash names to OpenSSL names
          if (strcmp(temp_hash_name, "SHA-1") == 0) {
            hash_name = "sha1";
          } else if (strcmp(temp_hash_name, "SHA-256") == 0) {
            hash_name = "sha256";
          } else if (strcmp(temp_hash_name, "SHA-384") == 0) {
            hash_name = "sha384";
          } else if (strcmp(temp_hash_name, "SHA-512") == 0) {
            hash_name = "sha512";
          }
          JS_FreeCString(ctx, temp_hash_name);
        }
        JS_FreeValue(ctx, hash_name_val);
      }
    }
    JS_FreeValue(ctx, hash_val);
  }

  // Get optional label
  uint8_t* label_data = NULL;
  size_t label_size = 0;
  JSValue label_val = JS_GetPropertyStr(ctx, argv[0], "label");
  if (!JS_IsUndefined(label_val) && !JS_IsNull(label_val)) {
    if (!jsrt_get_array_buffer_data(ctx, label_val, &label_data, &label_size)) {
      JS_FreeValue(ctx, label_val);
      EVP_PKEY_free(pkey);
      return JS_ThrowTypeError(ctx, "Invalid label data");
    }
  }
  JS_FreeValue(ctx, label_val);

  // Create decryption context
  EVP_PKEY_CTX* decrypt_ctx = EVP_PKEY_CTX_new(pkey, NULL);
  if (!decrypt_ctx) {
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to create RSA decryption context");
  }

  if (EVP_PKEY_decrypt_init(decrypt_ctx) <= 0) {
    EVP_PKEY_CTX_free(decrypt_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to initialize RSA decryption");
  }

  // Set padding based on algorithm
  if (alg == JSRT_CRYPTO_ALG_RSA_OAEP) {
    // Set OAEP padding and hash algorithm
    if (EVP_PKEY_CTX_set_rsa_padding(decrypt_ctx, RSA_PKCS1_OAEP_PADDING) <= 0) {
      EVP_PKEY_CTX_free(decrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA OAEP padding");
    }

    // Set hash algorithm
    const EVP_MD* md = EVP_get_digestbyname(hash_name);
    if (!md) {
      EVP_PKEY_CTX_free(decrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowTypeError(ctx, "Unsupported hash algorithm");
    }

    if (EVP_PKEY_CTX_set_rsa_oaep_md(decrypt_ctx, md) <= 0) {
      EVP_PKEY_CTX_free(decrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA OAEP hash algorithm");
    }

    // Set MGF1 hash algorithm (same as main hash for simplicity)
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(decrypt_ctx, md) <= 0) {
      EVP_PKEY_CTX_free(decrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA MGF1 hash algorithm");
    }

    // Set label if provided
    if (label_data && label_size > 0) {
      // Create a copy of label data since EVP_PKEY_CTX_set0_rsa_oaep_label takes ownership
      uint8_t* label_copy = malloc(label_size);
      if (!label_copy) {
        EVP_PKEY_CTX_free(decrypt_ctx);
        EVP_PKEY_free(pkey);
        return JS_ThrowInternalError(ctx, "Failed to allocate memory for label copy");
      }
      memcpy(label_copy, label_data, label_size);
      
      if (EVP_PKEY_CTX_set0_rsa_oaep_label(decrypt_ctx, label_copy, label_size) <= 0) {
        free(label_copy);
        EVP_PKEY_CTX_free(decrypt_ctx);
        EVP_PKEY_free(pkey);
        return JS_ThrowInternalError(ctx, "Failed to set RSA OAEP label");
      }
    }
  } else if (alg == JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    // Set PKCS1-v1_5 padding (no hash algorithm or label needed)
    if (EVP_PKEY_CTX_set_rsa_padding(decrypt_ctx, RSA_PKCS1_PADDING) <= 0) {
      EVP_PKEY_CTX_free(decrypt_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PKCS1-v1_5 padding");
    }
  }

  // Get output length
  size_t plaintext_len = 0;
  if (EVP_PKEY_decrypt(decrypt_ctx, NULL, &plaintext_len, ciphertext_data, ciphertext_size) <= 0) {
    EVP_PKEY_CTX_free(decrypt_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to get RSA decryption output length");
  }

  // Allocate output buffer
  uint8_t* plaintext = malloc(plaintext_len);
  if (!plaintext) {
    EVP_PKEY_CTX_free(decrypt_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for plaintext");
  }

  // Perform decryption
  if (EVP_PKEY_decrypt(decrypt_ctx, plaintext, &plaintext_len, ciphertext_data, ciphertext_size) <= 0) {
    free(plaintext);
    EVP_PKEY_CTX_free(decrypt_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "RSA decryption failed");
  }

  // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(plaintext);
    EVP_PKEY_CTX_free(decrypt_ctx);
    EVP_PKEY_free(pkey);
    return promise;
  }

  // Create ArrayBuffer for plaintext
  JSValue plaintext_buffer = JS_NewArrayBuffer(ctx, plaintext, plaintext_len, NULL, NULL, 0);

  // Resolve promise with the plaintext
  JSValue args[1] = {plaintext_buffer};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  // Clean up
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);
  EVP_PKEY_CTX_free(decrypt_ctx);
  EVP_PKEY_free(pkey);

  return promise;
}

static JSValue jsrt_rsa_sign(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Get data to sign
  uint8_t* data_to_sign = NULL;
  size_t data_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &data_to_sign, &data_size)) {
    return JS_ThrowTypeError(ctx, "Invalid data to sign");
  }

  // Get private key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_der_data = NULL;
  size_t key_der_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_der_data, &key_der_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Parse private key from PEM data
  BIO* key_bio = BIO_new_mem_buf(key_der_data, (int)key_der_size);
  if (!key_bio) {
    return JS_ThrowInternalError(ctx, "Failed to create BIO for key data");
  }

  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, NULL, NULL, NULL);
  BIO_free(key_bio);

  if (!pkey) {
    return JS_ThrowInternalError(ctx, "Failed to parse RSA private key");
  }

  // Verify this is an RSA key
  if (EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA) {
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Key is not an RSA key");
  }

  // Get hash algorithm from algorithm object  
  const char* hash_name = "sha256";  // Default OpenSSL name
  JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
  if (!JS_IsException(hash_val)) {
    // Check if hash is a string directly
    const char* temp_hash = JS_ToCString(ctx, hash_val);
    if (temp_hash) {
      // Convert WebCrypto hash names to OpenSSL names
      if (strcmp(temp_hash, "SHA-1") == 0) {
        hash_name = "sha1";
      } else if (strcmp(temp_hash, "SHA-256") == 0) {
        hash_name = "sha256"; 
      } else if (strcmp(temp_hash, "SHA-384") == 0) {
        hash_name = "sha384";
      } else if (strcmp(temp_hash, "SHA-512") == 0) {
        hash_name = "sha512";
      }
      JS_FreeCString(ctx, temp_hash);
    } else {
      // Try to get name property from hash object
      JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
      if (!JS_IsException(hash_name_val)) {
        const char* temp_hash_name = JS_ToCString(ctx, hash_name_val);
        if (temp_hash_name) {
          // Convert WebCrypto hash names to OpenSSL names
          if (strcmp(temp_hash_name, "SHA-1") == 0) {
            hash_name = "sha1";
          } else if (strcmp(temp_hash_name, "SHA-256") == 0) {
            hash_name = "sha256";
          } else if (strcmp(temp_hash_name, "SHA-384") == 0) {
            hash_name = "sha384";
          } else if (strcmp(temp_hash_name, "SHA-512") == 0) {
            hash_name = "sha512";
          }
          JS_FreeCString(ctx, temp_hash_name);
        }
        JS_FreeValue(ctx, hash_name_val);
      }
    }
    JS_FreeValue(ctx, hash_val);
  }

  // Create digest signing context using EVP_MD_CTX for proper message digest signing
  EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
  if (!md_ctx) {
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to create message digest context");
  }

  // Get hash algorithm
  const EVP_MD* md = EVP_get_digestbyname(hash_name);
  if (!md) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm");
  }

  if (EVP_DigestSignInit(md_ctx, NULL, md, NULL, pkey) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to initialize digest signing");
  }

  // Get the signing context to set RSA-specific parameters
  EVP_PKEY_CTX* sign_ctx = EVP_MD_CTX_pkey_ctx(md_ctx);
  if (!sign_ctx) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to get signing context");
  }

  // Set padding based on algorithm
  if (alg == JSRT_CRYPTO_ALG_RSA_PSS) {
    // RSA-PSS padding
    if (EVP_PKEY_CTX_set_rsa_padding(sign_ctx, RSA_PKCS1_PSS_PADDING) <= 0) {
      EVP_MD_CTX_free(md_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PSS padding");
    }
    
    // Get salt length from algorithm object, default to digest length
    int salt_length = RSA_PSS_SALTLEN_DIGEST;
    JSValue salt_len_val = JS_GetPropertyStr(ctx, argv[0], "saltLength");
    if (!JS_IsUndefined(salt_len_val) && !JS_IsNull(salt_len_val)) {
      int32_t salt_len;
      if (JS_ToInt32(ctx, &salt_len, salt_len_val) == 0) {
        salt_length = salt_len;
      }
    }
    JS_FreeValue(ctx, salt_len_val);
    
    if (EVP_PKEY_CTX_set_rsa_pss_saltlen(sign_ctx, salt_length) <= 0) {
      EVP_MD_CTX_free(md_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PSS salt length");
    }
  } else if (alg == JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5) {
    // PKCS1 v1.5 padding
    if (EVP_PKEY_CTX_set_rsa_padding(sign_ctx, RSA_PKCS1_PADDING) <= 0) {
      EVP_MD_CTX_free(md_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PKCS1 padding");
    }
  }

  // For PSS, set MGF1 hash as well
  if (alg == JSRT_CRYPTO_ALG_RSA_PSS) {
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(sign_ctx, md) <= 0) {
      EVP_MD_CTX_free(md_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PSS MGF1 hash");
    }
  }

  // Update the digest with the data
  if (EVP_DigestSignUpdate(md_ctx, data_to_sign, data_size) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to update digest");
  }

  // Get signature length
  size_t signature_len = 0;
  if (EVP_DigestSignFinal(md_ctx, NULL, &signature_len) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to get RSA signature length");
  }

  // Allocate signature buffer
  uint8_t* signature = malloc(signature_len);
  if (!signature) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for signature");
  }

  // Perform signing
  if (EVP_DigestSignFinal(md_ctx, signature, &signature_len) <= 0) {
    free(signature);
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "RSA signing failed");
  }

  // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(signature);
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return promise;
  }

  // Create ArrayBuffer for signature
  JSValue signature_buffer = JS_NewArrayBuffer(ctx, signature, signature_len, NULL, NULL, 0);

  // Resolve promise with the signature
  JSValue args[1] = {signature_buffer};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  // Clean up
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);
  EVP_MD_CTX_free(md_ctx);
  EVP_PKEY_free(pkey);

  return promise;
}

static JSValue jsrt_rsa_verify(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Get signature data
  uint8_t* signature_data = NULL;
  size_t signature_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &signature_data, &signature_size)) {
    return JS_ThrowTypeError(ctx, "Invalid signature data");
  }

  // Get data to verify
  uint8_t* data_to_verify = NULL;
  size_t data_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[3], &data_to_verify, &data_size)) {
    return JS_ThrowTypeError(ctx, "Invalid data to verify");
  }

  // Get public key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_der_data = NULL;
  size_t key_der_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_der_data, &key_der_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Parse public key from PEM data
  BIO* key_bio = BIO_new_mem_buf(key_der_data, (int)key_der_size);
  if (!key_bio) {
    return JS_ThrowInternalError(ctx, "Failed to create BIO for key data");
  }

  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(key_bio, NULL, NULL, NULL);
  BIO_free(key_bio);

  if (!pkey) {
    return JS_ThrowInternalError(ctx, "Failed to parse RSA public key");
  }

  // Verify this is an RSA key
  if (EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA) {
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Key is not an RSA key");
  }

  // Get hash algorithm from algorithm object  
  const char* hash_name = "sha256";  // Default OpenSSL name
  JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
  if (!JS_IsException(hash_val)) {
    // Check if hash is a string directly
    const char* temp_hash = JS_ToCString(ctx, hash_val);
    if (temp_hash) {
      // Convert WebCrypto hash names to OpenSSL names
      if (strcmp(temp_hash, "SHA-1") == 0) {
        hash_name = "sha1";
      } else if (strcmp(temp_hash, "SHA-256") == 0) {
        hash_name = "sha256"; 
      } else if (strcmp(temp_hash, "SHA-384") == 0) {
        hash_name = "sha384";
      } else if (strcmp(temp_hash, "SHA-512") == 0) {
        hash_name = "sha512";
      }
      JS_FreeCString(ctx, temp_hash);
    } else {
      // Try to get name property from hash object
      JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
      if (!JS_IsException(hash_name_val)) {
        const char* temp_hash_name = JS_ToCString(ctx, hash_name_val);
        if (temp_hash_name) {
          // Convert WebCrypto hash names to OpenSSL names
          if (strcmp(temp_hash_name, "SHA-1") == 0) {
            hash_name = "sha1";
          } else if (strcmp(temp_hash_name, "SHA-256") == 0) {
            hash_name = "sha256";
          } else if (strcmp(temp_hash_name, "SHA-384") == 0) {
            hash_name = "sha384";
          } else if (strcmp(temp_hash_name, "SHA-512") == 0) {
            hash_name = "sha512";
          }
          JS_FreeCString(ctx, temp_hash_name);
        }
        JS_FreeValue(ctx, hash_name_val);
      }
    }
    JS_FreeValue(ctx, hash_val);
  }

  // Create digest verification context using EVP_MD_CTX
  EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
  if (!md_ctx) {
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to create message digest context");
  }

  // Get hash algorithm
  const EVP_MD* md = EVP_get_digestbyname(hash_name);
  if (!md) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm");
  }

  if (EVP_DigestVerifyInit(md_ctx, NULL, md, NULL, pkey) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to initialize digest verification");
  }

  // Get the verification context to set RSA-specific parameters
  EVP_PKEY_CTX* verify_ctx = EVP_MD_CTX_pkey_ctx(md_ctx);
  if (!verify_ctx) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to get verification context");
  }

  // Set padding based on algorithm
  if (alg == JSRT_CRYPTO_ALG_RSA_PSS) {
    // RSA-PSS padding
    if (EVP_PKEY_CTX_set_rsa_padding(verify_ctx, RSA_PKCS1_PSS_PADDING) <= 0) {
      EVP_MD_CTX_free(md_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PSS padding");
    }
    
    // Get salt length from algorithm object, default to digest length
    int salt_length = RSA_PSS_SALTLEN_DIGEST;
    JSValue salt_len_val = JS_GetPropertyStr(ctx, argv[0], "saltLength");
    if (!JS_IsUndefined(salt_len_val) && !JS_IsNull(salt_len_val)) {
      int32_t salt_len;
      if (JS_ToInt32(ctx, &salt_len, salt_len_val) == 0) {
        salt_length = salt_len;
      }
    }
    JS_FreeValue(ctx, salt_len_val);
    
    if (EVP_PKEY_CTX_set_rsa_pss_saltlen(verify_ctx, salt_length) <= 0) {
      EVP_MD_CTX_free(md_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PSS salt length");
    }
  } else if (alg == JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5) {
    // PKCS1 v1.5 padding
    if (EVP_PKEY_CTX_set_rsa_padding(verify_ctx, RSA_PKCS1_PADDING) <= 0) {
      EVP_MD_CTX_free(md_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PKCS1 padding");
    }
  }

  // For PSS, set MGF1 hash as well
  if (alg == JSRT_CRYPTO_ALG_RSA_PSS) {
    if (EVP_PKEY_CTX_set_rsa_mgf1_md(verify_ctx, md) <= 0) {
      EVP_MD_CTX_free(md_ctx);
      EVP_PKEY_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to set RSA PSS MGF1 hash");
    }
  }

  // Update the digest with the data
  if (EVP_DigestVerifyUpdate(md_ctx, data_to_verify, data_size) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to update digest");
  }

  // Perform verification
  int verify_result = EVP_DigestVerifyFinal(md_ctx, signature_data, signature_size);

  // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    EVP_PKEY_CTX_free(verify_ctx);
    EVP_PKEY_free(pkey);
    return promise;
  }

  // Create result boolean
  JSValue result = JS_NewBool(ctx, verify_result == 1);

  // Resolve promise with the result
  JSValue args[1] = {result};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  // Clean up
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);
  EVP_PKEY_CTX_free(verify_ctx);
  EVP_PKEY_free(pkey);

  return promise;
}

// ECDSA signing implementation
static JSValue jsrt_ecdsa_sign(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Get hash algorithm from algorithm object
  const char* hash_name = "sha256";  // Default OpenSSL name
  JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
  if (!JS_IsException(hash_val)) {
    const char* temp_hash = JS_ToCString(ctx, hash_val);
    if (temp_hash) {
      // Convert WebCrypto hash names to OpenSSL names
      if (strcmp(temp_hash, "SHA-1") == 0) {
        hash_name = "sha1";
      } else if (strcmp(temp_hash, "SHA-256") == 0) {
        hash_name = "sha256"; 
      } else if (strcmp(temp_hash, "SHA-384") == 0) {
        hash_name = "sha384";
      } else if (strcmp(temp_hash, "SHA-512") == 0) {
        hash_name = "sha512";
      }
      JS_FreeCString(ctx, temp_hash);
    } else {
      // Try to get name property from hash object
      JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
      if (!JS_IsException(hash_name_val)) {
        const char* temp_hash_name = JS_ToCString(ctx, hash_name_val);
        if (temp_hash_name) {
          if (strcmp(temp_hash_name, "SHA-1") == 0) {
            hash_name = "sha1";
          } else if (strcmp(temp_hash_name, "SHA-256") == 0) {
            hash_name = "sha256";
          } else if (strcmp(temp_hash_name, "SHA-384") == 0) {
            hash_name = "sha384";
          } else if (strcmp(temp_hash_name, "SHA-512") == 0) {
            hash_name = "sha512";
          }
          JS_FreeCString(ctx, temp_hash_name);
        }
        JS_FreeValue(ctx, hash_name_val);
      }
    }
    JS_FreeValue(ctx, hash_val);
  }

  // Get message data
  uint8_t* message_data = NULL;
  size_t message_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &message_data, &message_size)) {
    return JS_ThrowTypeError(ctx, "Invalid message data");
  }

  // Get private key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_data = NULL;
  size_t key_data_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_data, &key_data_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Parse key from PEM data
  BIO* key_bio = BIO_new_mem_buf(key_data, (int)key_data_size);
  if (!key_bio) {
    return JS_ThrowInternalError(ctx, "Failed to create BIO for key data");
  }

  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, NULL, NULL, NULL);
  BIO_free(key_bio);

  if (!pkey) {
    return JS_ThrowInternalError(ctx, "Failed to parse ECDSA private key");
  }

  // Verify this is an EC key
  if (EVP_PKEY_base_id(pkey) != EVP_PKEY_EC) {
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Key is not an EC key");
  }

  // Create message digest context for signing
  EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
  if (!md_ctx) {
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to create message digest context");
  }

  // Get message digest algorithm
  const EVP_MD* md = EVP_get_digestbyname(hash_name);
  if (!md) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm");
  }

  // Initialize signing
  if (EVP_DigestSignInit(md_ctx, NULL, md, NULL, pkey) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to initialize ECDSA signing");
  }

  // Update with message data
  if (EVP_DigestSignUpdate(md_ctx, message_data, message_size) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to update ECDSA signing");
  }

  // Get signature length
  size_t signature_len = 0;
  if (EVP_DigestSignFinal(md_ctx, NULL, &signature_len) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to get ECDSA signature length");
  }

  // Allocate signature buffer
  uint8_t* signature = malloc(signature_len);
  if (!signature) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for signature");
  }

  // Generate signature
  if (EVP_DigestSignFinal(md_ctx, signature, &signature_len) <= 0) {
    free(signature);
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to generate ECDSA signature");
  }

  // Clean up
  EVP_MD_CTX_free(md_ctx);
  EVP_PKEY_free(pkey);

  // Create ArrayBuffer from signature
  JSValue array_buffer = JS_NewArrayBufferCopy(ctx, signature, signature_len);
  free(signature);

  if (JS_IsException(array_buffer)) {
    return JS_ThrowInternalError(ctx, "Failed to create signature ArrayBuffer");
  }

  // Wrap in a resolved promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeValue(ctx, array_buffer);
    return promise;
  }

  JSValue args[1] = {array_buffer};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);

  return promise;
}

// ECDSA verification implementation
static JSValue jsrt_ecdsa_verify(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Get hash algorithm from algorithm object
  const char* hash_name = "sha256";  // Default OpenSSL name
  JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
  if (!JS_IsException(hash_val)) {
    const char* temp_hash = JS_ToCString(ctx, hash_val);
    if (temp_hash) {
      // Convert WebCrypto hash names to OpenSSL names
      if (strcmp(temp_hash, "SHA-1") == 0) {
        hash_name = "sha1";
      } else if (strcmp(temp_hash, "SHA-256") == 0) {
        hash_name = "sha256"; 
      } else if (strcmp(temp_hash, "SHA-384") == 0) {
        hash_name = "sha384";
      } else if (strcmp(temp_hash, "SHA-512") == 0) {
        hash_name = "sha512";
      }
      JS_FreeCString(ctx, temp_hash);
    } else {
      // Try to get name property from hash object
      JSValue hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
      if (!JS_IsException(hash_name_val)) {
        const char* temp_hash_name = JS_ToCString(ctx, hash_name_val);
        if (temp_hash_name) {
          if (strcmp(temp_hash_name, "SHA-1") == 0) {
            hash_name = "sha1";
          } else if (strcmp(temp_hash_name, "SHA-256") == 0) {
            hash_name = "sha256";
          } else if (strcmp(temp_hash_name, "SHA-384") == 0) {
            hash_name = "sha384";
          } else if (strcmp(temp_hash_name, "SHA-512") == 0) {
            hash_name = "sha512";
          }
          JS_FreeCString(ctx, temp_hash_name);
        }
        JS_FreeValue(ctx, hash_name_val);
      }
    }
    JS_FreeValue(ctx, hash_val);
  }

  // Get signature data
  uint8_t* signature_data = NULL;
  size_t signature_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[2], &signature_data, &signature_size)) {
    return JS_ThrowTypeError(ctx, "Invalid signature data");
  }

  // Get message data
  uint8_t* message_data = NULL;
  size_t message_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, argv[3], &message_data, &message_size)) {
    return JS_ThrowTypeError(ctx, "Invalid message data");
  }

  // Get public key data from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid key object");
  }

  uint8_t* key_data = NULL;
  size_t key_data_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, key_data_val, &key_data, &key_data_size)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid key data");
  }
  JS_FreeValue(ctx, key_data_val);

  // Parse key from PEM data
  BIO* key_bio = BIO_new_mem_buf(key_data, (int)key_data_size);
  if (!key_bio) {
    return JS_ThrowInternalError(ctx, "Failed to create BIO for key data");
  }

  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(key_bio, NULL, NULL, NULL);
  BIO_free(key_bio);

  if (!pkey) {
    return JS_ThrowInternalError(ctx, "Failed to parse ECDSA public key");
  }

  // Verify this is an EC key
  if (EVP_PKEY_base_id(pkey) != EVP_PKEY_EC) {
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Key is not an EC key");
  }

  // Create message digest context for verification
  EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
  if (!md_ctx) {
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to create message digest context");
  }

  // Get message digest algorithm
  const EVP_MD* md = EVP_get_digestbyname(hash_name);
  if (!md) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm");
  }

  // Initialize verification
  if (EVP_DigestVerifyInit(md_ctx, NULL, md, NULL, pkey) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to initialize ECDSA verification");
  }

  // Update with message data
  if (EVP_DigestVerifyUpdate(md_ctx, message_data, message_size) <= 0) {
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return JS_ThrowInternalError(ctx, "Failed to update ECDSA verification");
  }

  // Verify signature
  int verify_result = EVP_DigestVerifyFinal(md_ctx, signature_data, signature_size);
  
  // Clean up
  EVP_MD_CTX_free(md_ctx);
  EVP_PKEY_free(pkey);

  // Create result value (true if verification succeeded, false otherwise)
  JSValue result = JS_NewBool(ctx, verify_result == 1);

  // Wrap in a resolved promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeValue(ctx, result);
    return promise;
  }

  JSValue args[1] = {result};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);

  return promise;
}

// ECDH key derivation implementation
static JSValue jsrt_ecdh_derive_key(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Get the target length from the third argument
  int32_t length;
  if (JS_ToInt32(ctx, &length, argv[2]) < 0) {
    return JS_ThrowTypeError(ctx, "Invalid length parameter");
  }
  
  if (length <= 0 || length % 8 != 0) {
    return JS_ThrowTypeError(ctx, "Length must be a positive multiple of 8");
  }
  
  size_t byte_length = length / 8;

  // Get the private key from the base key (argv[1])
  JSValue private_key_data_val = JS_GetPropertyStr(ctx, argv[1], "_keyData");
  if (JS_IsException(private_key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid private key object");
  }

  uint8_t* private_key_data = NULL;
  size_t private_key_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, private_key_data_val, &private_key_data, &private_key_size)) {
    JS_FreeValue(ctx, private_key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid private key data");
  }
  JS_FreeValue(ctx, private_key_data_val);

  // Get the public key from the algorithm object
  JSValue public_key_val = JS_GetPropertyStr(ctx, argv[0], "public");
  if (JS_IsException(public_key_val)) {
    return JS_ThrowTypeError(ctx, "Algorithm must have 'public' property");
  }

  JSValue public_key_data_val = JS_GetPropertyStr(ctx, public_key_val, "_keyData");
  JS_FreeValue(ctx, public_key_val);
  if (JS_IsException(public_key_data_val)) {
    return JS_ThrowTypeError(ctx, "Invalid public key object");
  }

  uint8_t* public_key_data = NULL;
  size_t public_key_size = 0;
  if (!jsrt_get_array_buffer_data(ctx, public_key_data_val, &public_key_data, &public_key_size)) {
    JS_FreeValue(ctx, public_key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid public key data");
  }
  JS_FreeValue(ctx, public_key_data_val);

  // Parse private key from PEM data
  BIO* private_bio = BIO_new_mem_buf(private_key_data, (int)private_key_size);
  if (!private_bio) {
    return JS_ThrowInternalError(ctx, "Failed to create BIO for private key");
  }

  EVP_PKEY* private_pkey = PEM_read_bio_PrivateKey(private_bio, NULL, NULL, NULL);
  BIO_free(private_bio);

  if (!private_pkey) {
    return JS_ThrowInternalError(ctx, "Failed to parse ECDH private key");
  }

  // Verify this is an EC key
  if (EVP_PKEY_base_id(private_pkey) != EVP_PKEY_EC) {
    EVP_PKEY_free(private_pkey);
    return JS_ThrowTypeError(ctx, "Private key is not an EC key");
  }

  // Parse public key from PEM data
  BIO* public_bio = BIO_new_mem_buf(public_key_data, (int)public_key_size);
  if (!public_bio) {
    EVP_PKEY_free(private_pkey);
    return JS_ThrowInternalError(ctx, "Failed to create BIO for public key");
  }

  EVP_PKEY* public_pkey = PEM_read_bio_PUBKEY(public_bio, NULL, NULL, NULL);
  BIO_free(public_bio);

  if (!public_pkey) {
    EVP_PKEY_free(private_pkey);
    return JS_ThrowInternalError(ctx, "Failed to parse ECDH public key");
  }

  // Verify this is an EC key
  if (EVP_PKEY_base_id(public_pkey) != EVP_PKEY_EC) {
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    return JS_ThrowTypeError(ctx, "Public key is not an EC key");
  }

  // Create derivation context
  EVP_PKEY_CTX* derive_ctx = EVP_PKEY_CTX_new(private_pkey, NULL);
  if (!derive_ctx) {
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    return JS_ThrowInternalError(ctx, "Failed to create ECDH derivation context");
  }

  if (EVP_PKEY_derive_init(derive_ctx) <= 0) {
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    return JS_ThrowInternalError(ctx, "Failed to initialize ECDH derivation");
  }

  if (EVP_PKEY_derive_set_peer(derive_ctx, public_pkey) <= 0) {
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    return JS_ThrowInternalError(ctx, "Failed to set ECDH peer key");
  }

  // Get the shared secret length
  size_t secret_len = 0;
  if (EVP_PKEY_derive(derive_ctx, NULL, &secret_len) <= 0) {
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    return JS_ThrowInternalError(ctx, "Failed to get ECDH shared secret length");
  }

  // Allocate buffer for shared secret
  uint8_t* secret = malloc(secret_len);
  if (!secret) {
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for shared secret");
  }

  // Derive the shared secret
  if (EVP_PKEY_derive(derive_ctx, secret, &secret_len) <= 0) {
    free(secret);
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    return JS_ThrowInternalError(ctx, "Failed to derive ECDH shared secret");
  }

  // Clean up
  EVP_PKEY_CTX_free(derive_ctx);
  EVP_PKEY_free(private_pkey);
  EVP_PKEY_free(public_pkey);

  // Truncate or pad to requested length
  uint8_t* result_bits = malloc(byte_length);
  if (!result_bits) {
    free(secret);
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for result");
  }

  if (byte_length <= secret_len) {
    // Truncate to requested length
    memcpy(result_bits, secret, byte_length);
  } else {
    // Pad with zeros if needed (should not happen with ECDH typically)
    memcpy(result_bits, secret, secret_len);
    memset(result_bits + secret_len, 0, byte_length - secret_len);
  }

  free(secret);

  // Create ArrayBuffer from derived bits
  JSValue array_buffer = JS_NewArrayBufferCopy(ctx, result_bits, byte_length);
  free(result_bits);

  if (JS_IsException(array_buffer)) {
    return JS_ThrowInternalError(ctx, "Failed to create derived bits ArrayBuffer");
  }

  // Wrap in a resolved promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeValue(ctx, array_buffer);
    return promise;
  }

  JSValue args[1] = {array_buffer};
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, args[0]);

  return promise;
}

JSValue jsrt_subtle_generateKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "generateKey requires 3 arguments");
  }

  // Parse algorithm
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  
  // Provide specific error messages for different unsupported algorithms
  switch (alg) {
    case JSRT_CRYPTO_ALG_AES_CBC:
    case JSRT_CRYPTO_ALG_AES_GCM:
    case JSRT_CRYPTO_ALG_AES_CTR:
      // These are supported, continue
      break;
    case JSRT_CRYPTO_ALG_RSA_OAEP:
    case JSRT_CRYPTO_ALG_RSA_PSS:
    case JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5:
    case JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5:
      // RSA key generation is supported, continue
      break;
    case JSRT_CRYPTO_ALG_ECDSA:
    case JSRT_CRYPTO_ALG_ECDH:
      // ECDSA/ECDH key generation is supported, continue
      break;
    case JSRT_CRYPTO_ALG_HMAC:
      // HMAC is supported, continue
      break;
    case JSRT_CRYPTO_ALG_PBKDF2:
    case JSRT_CRYPTO_ALG_HKDF:
      return JS_ThrowTypeError(ctx, "Key derivation functions not yet implemented in static build");
    default:
      return JS_ThrowTypeError(ctx, "Unsupported algorithm for key generation in static build");
  }

  // Handle different algorithm types
  if (alg == JSRT_CRYPTO_ALG_HMAC) {
    return jsrt_generate_hmac_key(ctx, argv);
  } else if (alg == JSRT_CRYPTO_ALG_RSA_OAEP || alg == JSRT_CRYPTO_ALG_RSA_PSS || 
             alg == JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5 || alg == JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5) {
    return jsrt_generate_rsa_key_pair(ctx, argv, alg);
  } else if (alg == JSRT_CRYPTO_ALG_ECDSA || alg == JSRT_CRYPTO_ALG_ECDH) {
    return jsrt_generate_ec_key_pair(ctx, argv, alg);
  } else {
    // AES key generation
    // Get key length from algorithm object
    JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
    if (JS_IsException(length_val)) {
      return length_val;
    }
    
    int32_t key_length;
    if (JS_ToInt32(ctx, &key_length, length_val) < 0) {
      JS_FreeValue(ctx, length_val);
      return JS_ThrowTypeError(ctx, "Invalid key length");
    }
    JS_FreeValue(ctx, length_val);

    // Validate key length
    if (key_length != 128 && key_length != 192 && key_length != 256) {
      return JS_ThrowTypeError(ctx, "Invalid AES key length. Must be 128, 192, or 256 bits");
    }

    // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return promise;
  }

  // Generate AES key using unified backend
  uint8_t* key_data;
  size_t key_data_length;
  int result = jsrt_crypto_unified_generate_aes_key(key_length, &key_data, &key_data_length);
  
  if (result != 0) {
    // Reject promise
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to generate AES key"));
    JSValue args[1] = {error};
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, args[0]);
  } else {
    // Create CryptoKey object
    JSValue crypto_key = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "secret"));
    JS_SetPropertyStr(ctx, crypto_key, "extractable", JS_NewBool(ctx, true));
    
    // Create algorithm object
    JSValue alg_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, alg_obj, "name", JS_NewString(ctx, "AES-CBC"));
    JS_SetPropertyStr(ctx, alg_obj, "length", JS_NewInt32(ctx, key_length));
    JS_SetPropertyStr(ctx, crypto_key, "algorithm", alg_obj);
    
    // Store key data (simplified - in real implementation this should be more secure)
    JSValue key_buffer = JS_NewArrayBuffer(ctx, key_data, key_data_length, NULL, NULL, 0);
    JS_SetPropertyStr(ctx, crypto_key, "_keyData", key_buffer);
    
    // Resolve promise with CryptoKey
    JSValue args[1] = {crypto_key};
    JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, args[0]);
  }

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);

  return promise;
  } // End of AES key generation path
}

JSValue jsrt_subtle_importKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 5) {
    return JS_ThrowTypeError(ctx, "importKey requires 5 arguments");
  }
  
  // Parse the algorithm from the third argument to provide specific error messages
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[2]);
  
  switch (alg) {
    case JSRT_CRYPTO_ALG_AES_CBC:
      return JS_ThrowTypeError(ctx, "AES-CBC key import not yet implemented in static build");
    case JSRT_CRYPTO_ALG_AES_GCM:
      return JS_ThrowTypeError(ctx, "AES-GCM key import not yet implemented in static build");
    case JSRT_CRYPTO_ALG_AES_CTR:
      return JS_ThrowTypeError(ctx, "AES-CTR key import not yet implemented in static build");
    case JSRT_CRYPTO_ALG_RSA_OAEP:
    case JSRT_CRYPTO_ALG_RSA_PSS:
    case JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5:
    case JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5:
      return JS_ThrowTypeError(ctx, "RSA key import not yet implemented in static build");
    case JSRT_CRYPTO_ALG_ECDSA:
    case JSRT_CRYPTO_ALG_ECDH:
      return JS_ThrowTypeError(ctx, "ECDSA/ECDH key import not yet implemented in static build");
    case JSRT_CRYPTO_ALG_HMAC:
      return JS_ThrowTypeError(ctx, "HMAC key import not yet implemented in static build");
    case JSRT_CRYPTO_ALG_PBKDF2:
      return jsrt_import_pbkdf2_key(ctx, argv);
    case JSRT_CRYPTO_ALG_HKDF:
      return JS_ThrowTypeError(ctx, "HKDF key import not supported");
    default:
      return JS_ThrowTypeError(ctx, "Unsupported algorithm for key import in static build");
  }
}

static JSValue jsrt_import_pbkdf2_key(JSContext* ctx, JSValueConst* argv) {
  // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }
  
  // Extract format (argv[0]), keyData (argv[1]), algorithm (argv[2]), extractable (argv[3]), usages (argv[4])
  const char* format = JS_ToCString(ctx, argv[0]);
  if (!format) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid key format"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    return promise;
  }
  
  // Check if format is 'raw' for PBKDF2
  if (strcmp(format, "raw") != 0) {
    JS_FreeCString(ctx, format);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "PBKDF2 only supports 'raw' format"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    return promise;
  }
  JS_FreeCString(ctx, format);
  
  // Extract key data
  JSValue key_data = argv[1];
  size_t key_size;
  uint8_t* raw_key_data = NULL;
  if (!jsrt_get_array_buffer_data(ctx, key_data, &raw_key_data, &key_size)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid key data"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    return promise;
  }
  
  // For PBKDF2, we just store the raw key data as-is
  uint8_t* pbkdf2_key_copy = js_malloc(ctx, key_size);
  if (!pbkdf2_key_copy) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Memory allocation failed"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    return promise;
  }
  memcpy(pbkdf2_key_copy, raw_key_data, key_size);
  
  JSValue pbkdf2_key_data = JS_NewArrayBufferCopy(ctx, pbkdf2_key_copy, key_size);
  js_free(ctx, pbkdf2_key_copy);
  
  if (JS_IsException(pbkdf2_key_data)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to create key data"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    return promise;
  }
  
  // Create CryptoKey object
  JSValue crypto_key = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "secret"));
  JS_SetPropertyStr(ctx, crypto_key, "extractable", JS_DupValue(ctx, argv[3])); // extractable
  JS_SetPropertyStr(ctx, crypto_key, "usages", JS_DupValue(ctx, argv[4])); // usages
  JS_SetPropertyStr(ctx, crypto_key, "_keyData", pbkdf2_key_data);
  
  // Create algorithm object
  JSValue algorithm = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, algorithm, "name", JS_NewString(ctx, "PBKDF2"));
  JS_SetPropertyStr(ctx, crypto_key, "algorithm", algorithm);
  
  // Resolve the promise with the crypto key
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &crypto_key);
  
  // Clean up
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, crypto_key);
  
  return promise;
}

static JSValue jsrt_ecdh_derive_key_to_key(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }
  
  // Extract public key from algorithm object
  JSValue algorithm = argv[0];
  JSValue public_key_val = JS_GetPropertyStr(ctx, algorithm, "public");
  if (JS_IsException(public_key_val)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Missing public key in ECDH algorithm"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    return promise;
  }
  
  // Extract private key
  JSValue base_key = argv[1];
  JSValue private_key_data = JS_GetPropertyStr(ctx, base_key, "_keyData");
  if (JS_IsException(private_key_data)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid base key for ECDH"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    return promise;
  }
  
  // Extract target key algorithm
  JSValue derived_key_algorithm = argv[2];
  const char* target_alg_name = JS_ToCString(ctx, JS_GetPropertyStr(ctx, derived_key_algorithm, "name"));
  if (!target_alg_name) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid derived key algorithm"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    return promise;
  }
  
  // Extract target key length
  JSValue length_val = JS_GetPropertyStr(ctx, derived_key_algorithm, "length");
  int32_t key_length = 0;
  if (JS_ToInt32(ctx, &key_length, length_val) < 0) {
    key_length = 256; // default
  }
  
  // Parse key data from base key and public key (same logic as deriveBits)
  size_t private_der_size;
  uint8_t* private_der_data = NULL;
  if (!jsrt_get_array_buffer_data(ctx, private_key_data, &private_der_data, &private_der_size)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid private key data"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  JSValue public_key_data = JS_GetPropertyStr(ctx, public_key_val, "_keyData");
  size_t public_der_size;
  uint8_t* public_der_data = NULL;
  if (!jsrt_get_array_buffer_data(ctx, public_key_data, &public_der_data, &public_der_size)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid public key data"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  // Load private key from PEM data
  BIO* private_bio = BIO_new_mem_buf(private_der_data, (int)private_der_size);
  if (!private_bio) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to create BIO for private key"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  EVP_PKEY* private_pkey = PEM_read_bio_PrivateKey(private_bio, NULL, NULL, NULL);
  BIO_free(private_bio);
  
  if (!private_pkey) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to load private key"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  // Load public key from PEM data
  BIO* public_bio = BIO_new_mem_buf(public_der_data, (int)public_der_size);
  if (!public_bio) {
    EVP_PKEY_free(private_pkey);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to create BIO for public key"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  EVP_PKEY* public_pkey = PEM_read_bio_PUBKEY(public_bio, NULL, NULL, NULL);
  BIO_free(public_bio);
  
  if (!public_pkey) {
    EVP_PKEY_free(private_pkey);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to load public key"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  // Create derivation context
  EVP_PKEY_CTX* derive_ctx = EVP_PKEY_CTX_new(private_pkey, NULL);
  if (!derive_ctx) {
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to create derivation context"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  if (EVP_PKEY_derive_init(derive_ctx) <= 0) {
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to initialize derivation"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  if (EVP_PKEY_derive_set_peer(derive_ctx, public_pkey) <= 0) {
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to set peer public key"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  // Get the length of the shared secret
  size_t secret_len = 0;
  if (EVP_PKEY_derive(derive_ctx, NULL, &secret_len) <= 0) {
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to get shared secret length"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  // Derive the shared secret
  uint8_t* secret = js_malloc(ctx, secret_len);
  if (!secret) {
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Memory allocation failed"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  if (EVP_PKEY_derive(derive_ctx, secret, &secret_len) <= 0) {
    js_free(ctx, secret);
    EVP_PKEY_CTX_free(derive_ctx);
    EVP_PKEY_free(private_pkey);
    EVP_PKEY_free(public_pkey);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to derive shared secret"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  // Clean up OpenSSL resources
  EVP_PKEY_CTX_free(derive_ctx);
  EVP_PKEY_free(private_pkey);
  EVP_PKEY_free(public_pkey);
  
  // Use shared secret to create AES key - for simplicity, we'll use the shared secret directly
  // In a full implementation, you'd want to use a KDF like HKDF
  size_t aes_key_bytes = key_length / 8;  // Convert bits to bytes
  if (aes_key_bytes > secret_len) {
    aes_key_bytes = secret_len;  // Use available secret length
  }
  
  // Create key data for the derived AES key
  uint8_t* aes_key_copy = js_malloc(ctx, aes_key_bytes);
  if (!aes_key_copy) {
    js_free(ctx, secret);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Memory allocation failed for AES key"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  memcpy(aes_key_copy, secret, aes_key_bytes);
  js_free(ctx, secret);
  
  JSValue aes_key_data = JS_NewArrayBufferCopy(ctx, aes_key_copy, aes_key_bytes);
  js_free(ctx, aes_key_copy);
  
  if (JS_IsException(aes_key_data)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to create AES key data"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, public_key_val);
    JS_FreeValue(ctx, private_key_data);
    JS_FreeValue(ctx, public_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  // Parse extractable and usages
  JSValue extractable = argv[3];
  JSValue usages = argv[4];
  
  // Create CryptoKey object for the derived AES key
  JSValue derived_key = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, derived_key, "type", JS_NewString(ctx, "secret"));
  JS_SetPropertyStr(ctx, derived_key, "extractable", JS_DupValue(ctx, extractable));
  JS_SetPropertyStr(ctx, derived_key, "usages", JS_DupValue(ctx, usages));
  JS_SetPropertyStr(ctx, derived_key, "key_data", aes_key_data);
  
  // Create algorithm object for the derived key
  JSValue derived_alg = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, derived_alg, "name", JS_NewString(ctx, target_alg_name));
  JS_SetPropertyStr(ctx, derived_alg, "length", JS_NewInt32(ctx, key_length));
  JS_SetPropertyStr(ctx, derived_key, "algorithm", derived_alg);
  
  // Resolve the promise with the derived key
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &derived_key);
  
  // Clean up
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, derived_key);
  JS_FreeValue(ctx, public_key_val);
  JS_FreeValue(ctx, private_key_data);
  JS_FreeValue(ctx, public_key_data);
  JS_FreeValue(ctx, length_val);
  JS_FreeCString(ctx, target_alg_name);
  
  return promise;
}

static JSValue jsrt_pbkdf2_derive_key(JSContext* ctx, JSValueConst* argv, jsrt_crypto_algorithm_t alg) {
  // Create promise for async operation
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }
  
  // Extract PBKDF2 parameters from algorithm object (argv[0])
  JSValue algorithm = argv[0];
  
  // Extract salt
  JSValue salt_val = JS_GetPropertyStr(ctx, algorithm, "salt");
  if (JS_IsException(salt_val)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Missing salt in PBKDF2 algorithm"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    return promise;
  }
  
  size_t salt_size;
  uint8_t* salt_data = NULL;
  if (!jsrt_get_array_buffer_data(ctx, salt_val, &salt_data, &salt_size)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid salt data"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    return promise;
  }
  
  // Extract iterations
  JSValue iterations_val = JS_GetPropertyStr(ctx, algorithm, "iterations");
  int32_t iterations = 0;
  if (JS_ToInt32(ctx, &iterations, iterations_val) < 0 || iterations <= 0) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid iterations count"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, iterations_val);
    return promise;
  }
  
  // Extract hash algorithm
  JSValue hash_val = JS_GetPropertyStr(ctx, algorithm, "hash");
  const char* hash_name = JS_ToCString(ctx, hash_val);
  if (!hash_name) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid hash algorithm"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, iterations_val);
    JS_FreeValue(ctx, hash_val);
    return promise;
  }
  
  // Map WebCrypto hash names to OpenSSL digest names
  const EVP_MD* digest = NULL;
  if (strcmp(hash_name, "SHA-256") == 0) {
    digest = EVP_sha256();
  } else if (strcmp(hash_name, "SHA-384") == 0) {
    digest = EVP_sha384();
  } else if (strcmp(hash_name, "SHA-512") == 0) {
    digest = EVP_sha512();
  } else if (strcmp(hash_name, "SHA-1") == 0) {
    digest = EVP_sha1();
  } else {
    JS_FreeCString(ctx, hash_name);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Unsupported hash algorithm"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, iterations_val);
    JS_FreeValue(ctx, hash_val);
    return promise;
  }
  JS_FreeCString(ctx, hash_name);
  
  // Extract base key (argv[1])
  JSValue base_key = argv[1];
  JSValue base_key_data = JS_GetPropertyStr(ctx, base_key, "_keyData");
  if (JS_IsException(base_key_data)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid base key for PBKDF2"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, iterations_val);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, base_key_data);
    return promise;
  }
  
  size_t password_size;
  uint8_t* password_data = NULL;
  if (!jsrt_get_array_buffer_data(ctx, base_key_data, &password_data, &password_size)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid password data"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, iterations_val);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, base_key_data);
    return promise;
  }
  
  // Extract derived key algorithm (argv[2])
  JSValue derived_key_algorithm = argv[2];
  const char* target_alg_name = JS_ToCString(ctx, JS_GetPropertyStr(ctx, derived_key_algorithm, "name"));
  if (!target_alg_name) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid derived key algorithm"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, iterations_val);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, base_key_data);
    return promise;
  }
  
  // Extract target key length
  JSValue length_val = JS_GetPropertyStr(ctx, derived_key_algorithm, "length");
  int32_t key_length_bits = 0;
  if (JS_ToInt32(ctx, &key_length_bits, length_val) < 0 || key_length_bits <= 0) {
    key_length_bits = 256; // default to 256 bits
  }
  size_t key_length_bytes = key_length_bits / 8;
  
  // Perform PBKDF2 key derivation
  uint8_t* derived_key_data = js_malloc(ctx, key_length_bytes);
  if (!derived_key_data) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Memory allocation failed"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, iterations_val);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, base_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  int result = PKCS5_PBKDF2_HMAC((const char*)password_data, password_size,
                                salt_data, salt_size,
                                iterations, digest,
                                key_length_bytes, derived_key_data);
  
  if (result != 1) {
    js_free(ctx, derived_key_data);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "PBKDF2 key derivation failed"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, iterations_val);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, base_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  // Create derived key ArrayBuffer
  JSValue derived_key_buffer = JS_NewArrayBufferCopy(ctx, derived_key_data, key_length_bytes);
  js_free(ctx, derived_key_data);
  
  if (JS_IsException(derived_key_buffer)) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to create derived key data"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, salt_val);
    JS_FreeValue(ctx, iterations_val);
    JS_FreeValue(ctx, hash_val);
    JS_FreeValue(ctx, base_key_data);
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, target_alg_name);
    return promise;
  }
  
  // Parse extractable and usages
  JSValue extractable = argv[3];
  JSValue usages = argv[4];
  
  // Create CryptoKey object for the derived key
  JSValue crypto_key = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, crypto_key, "type", JS_NewString(ctx, "secret"));
  JS_SetPropertyStr(ctx, crypto_key, "extractable", JS_DupValue(ctx, extractable));
  JS_SetPropertyStr(ctx, crypto_key, "usages", JS_DupValue(ctx, usages));
  JS_SetPropertyStr(ctx, crypto_key, "_keyData", derived_key_buffer);
  
  // Create algorithm object for the derived key
  JSValue derived_alg = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, derived_alg, "name", JS_NewString(ctx, target_alg_name));
  JS_SetPropertyStr(ctx, derived_alg, "length", JS_NewInt32(ctx, key_length_bits));
  JS_SetPropertyStr(ctx, crypto_key, "algorithm", derived_alg);
  
  // Resolve the promise with the derived key
  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &crypto_key);
  
  // Clean up
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeValue(ctx, crypto_key);
  JS_FreeValue(ctx, salt_val);
  JS_FreeValue(ctx, iterations_val);
  JS_FreeValue(ctx, hash_val);
  JS_FreeValue(ctx, base_key_data);
  JS_FreeValue(ctx, length_val);
  JS_FreeCString(ctx, target_alg_name);
  
  return promise;
}

JSValue jsrt_subtle_exportKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowTypeError(ctx, "exportKey not implemented yet");
}

JSValue jsrt_subtle_deriveKey(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 5) {
    return JS_ThrowTypeError(ctx, "deriveKey requires 5 arguments");
  }
  
  // Parse algorithm  
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    return JS_ThrowTypeError(ctx, "Unsupported algorithm for deriveKey");
  }
  
  if (alg == JSRT_CRYPTO_ALG_ECDH) {
    return jsrt_ecdh_derive_key_to_key(ctx, argv, alg);
  }
  
  if (alg == JSRT_CRYPTO_ALG_PBKDF2) {
    return jsrt_pbkdf2_derive_key(ctx, argv, alg);
  }
  
  return JS_ThrowTypeError(ctx, "Unsupported algorithm for deriveKey");
}

JSValue jsrt_subtle_deriveBits(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "deriveBits requires 3 arguments");
  }

  // Parse algorithm
  jsrt_crypto_algorithm_t alg = jsrt_crypto_parse_algorithm(ctx, argv[0]);
  if (alg != JSRT_CRYPTO_ALG_ECDH) {
    return JS_ThrowTypeError(ctx, "Only ECDH deriveBits is currently supported in static build");
  }

  // Handle ECDH derivation
  return jsrt_ecdh_derive_key(ctx, argv, alg);
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

  // Initialize the unified crypto backend for static builds
  if (!jsrt_crypto_backend_init(JSRT_CRYPTO_BACKEND_STATIC)) {
    JSRT_Debug("JSRT_RuntimeSetupStdCrypto: Failed to initialize static crypto backend");
    return;
  }

  // Create crypto object
  JSValue crypto_obj = JS_NewObject(rt->ctx);

  // crypto.getRandomValues() - use unified implementation 
  JS_SetPropertyStr(rt->ctx, crypto_obj, "getRandomValues",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_getRandomValues, "getRandomValues", 1));

  // crypto.randomUUID() - use unified implementation
  JS_SetPropertyStr(rt->ctx, crypto_obj, "randomUUID",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_randomUUID, "randomUUID", 0));

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




#else  // !JSRT_STATIC_OPENSSL

// Fallback implementations when OpenSSL is not available
#include "../util/debug.h"
#include "crypto_subtle.h"

// Forward declarations for stub implementations
static JSValue jsrt_crypto_getRandomValues_stub(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_crypto_randomUUID_stub(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

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

  // crypto.getRandomValues() - use unified implementation (with fallback)
  JS_SetPropertyStr(rt->ctx, crypto_obj, "getRandomValues",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_getRandomValues, "getRandomValues", 1));

  // crypto.randomUUID() - use unified implementation (with fallback)
  JS_SetPropertyStr(rt->ctx, crypto_obj, "randomUUID",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_randomUUID, "randomUUID", 0));

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