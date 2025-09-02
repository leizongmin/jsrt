#ifndef __JSRT_STD_CRYPTO_SUBTLE_H__
#define __JSRT_STD_CRYPTO_SUBTLE_H__

#include <quickjs.h>

#include "../runtime.h"

// SubtleCrypto API setup
JSValue JSRT_CreateSubtleCrypto(JSContext *ctx);
void JSRT_SetupSubtleCrypto(JSRT_Runtime *rt);

// Core SubtleCrypto operations
JSValue jsrt_subtle_digest(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue jsrt_subtle_encrypt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue jsrt_subtle_decrypt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue jsrt_subtle_sign(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue jsrt_subtle_verify(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue jsrt_subtle_generateKey(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue jsrt_subtle_importKey(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue jsrt_subtle_exportKey(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue jsrt_subtle_deriveKey(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue jsrt_subtle_deriveBits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// Algorithm support checking
typedef enum {
  JSRT_CRYPTO_ALG_SHA1,
  JSRT_CRYPTO_ALG_SHA256,
  JSRT_CRYPTO_ALG_SHA384,
  JSRT_CRYPTO_ALG_SHA512,
  JSRT_CRYPTO_ALG_AES_CBC,
  JSRT_CRYPTO_ALG_AES_GCM,
  JSRT_CRYPTO_ALG_AES_CTR,
  JSRT_CRYPTO_ALG_RSA_OAEP,
  JSRT_CRYPTO_ALG_RSA_PSS,
  JSRT_CRYPTO_ALG_RSASSA_PKCS1_V1_5,
  JSRT_CRYPTO_ALG_RSA_PKCS1_V1_5,
  JSRT_CRYPTO_ALG_ECDSA,
  JSRT_CRYPTO_ALG_ECDH,
  JSRT_CRYPTO_ALG_HMAC,
  JSRT_CRYPTO_ALG_PBKDF2,
  JSRT_CRYPTO_ALG_HKDF,
  JSRT_CRYPTO_ALG_UNKNOWN
} jsrt_crypto_algorithm_t;

// CryptoKey representation
typedef struct {
  char *algorithm_name;
  char *key_type;  // "public", "private", "secret"
  bool extractable;
  JSValue usages;     // Array of strings
  void *openssl_key;  // OpenSSL key structure
  size_t key_length;
} JSRTCryptoKey;

// Async operation context
typedef struct {
  JSContext *ctx;
  JSValue promise;
  JSValue resolve_func;
  JSValue reject_func;
  uv_work_t work_req;

  // Operation type and data
  enum {
    JSRT_CRYPTO_OP_DIGEST,
    JSRT_CRYPTO_OP_ENCRYPT,
    JSRT_CRYPTO_OP_DECRYPT,
    JSRT_CRYPTO_OP_SIGN,
    JSRT_CRYPTO_OP_VERIFY,
    JSRT_CRYPTO_OP_GENERATE_KEY,
    JSRT_CRYPTO_OP_IMPORT_KEY,
    JSRT_CRYPTO_OP_EXPORT_KEY
  } operation_type;

  // Common operation data
  jsrt_crypto_algorithm_t algorithm;
  uint8_t *input_data;
  size_t input_length;
  uint8_t *output_data;
  size_t output_length;

  // Operation-specific data
  union {
    struct {
      // Digest specific
      const char *hash_name;
    } digest;

    struct {
      // Encrypt/Decrypt specific
      JSRTCryptoKey *key;
      uint8_t *iv;
      size_t iv_length;
    } cipher;

    struct {
      // Key generation specific
      int key_length;
      bool extractable;
      JSValue usages;
    } keygen;
  } op_data;

  // Error information
  char *error_message;
  int error_code;
} JSRTCryptoAsyncOperation;

// Utility functions
jsrt_crypto_algorithm_t jsrt_crypto_parse_algorithm(JSContext *ctx, JSValue algorithm);
const char *jsrt_crypto_algorithm_to_string(jsrt_crypto_algorithm_t alg);
bool jsrt_crypto_is_algorithm_supported(jsrt_crypto_algorithm_t alg);

// Error handling
JSValue jsrt_crypto_throw_error(JSContext *ctx, const char *name, const char *message);

// Memory management
JSRTCryptoKey *jsrt_crypto_key_new(void);
void jsrt_crypto_key_free(JSRTCryptoKey *key);
JSRTCryptoAsyncOperation *jsrt_crypto_async_operation_new(void);
void jsrt_crypto_async_operation_free(JSRTCryptoAsyncOperation *op);

#endif