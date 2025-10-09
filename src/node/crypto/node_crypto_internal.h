#ifndef __JSRT_NODE_CRYPTO_INTERNAL_H__
#define __JSRT_NODE_CRYPTO_INTERNAL_H__

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../crypto/crypto_digest.h"
#include "../../crypto/crypto_ec.h"
#include "../../crypto/crypto_hmac.h"
#include "../../crypto/crypto_rsa.h"
#include "../../crypto/crypto_subtle.h"
#include "../../crypto/crypto_symmetric.h"
#include "../../util/debug.h"

// Forward declarations for class IDs
extern JSClassID js_node_hash_class_id;
extern JSClassID js_node_hmac_class_id;
extern JSClassID js_node_cipher_class_id;
extern JSClassID js_node_sign_class_id;
extern JSClassID js_node_verify_class_id;
extern JSClassID js_node_ecdh_class_id;

// Hash class structure
typedef struct {
  JSContext* ctx;
  jsrt_crypto_algorithm_t algorithm;
  uint8_t* buffer;
  size_t buffer_len;
  size_t buffer_capacity;
  bool finalized;
} JSNodeHash;

// HMAC class structure
typedef struct {
  JSContext* ctx;
  jsrt_hmac_algorithm_t algorithm;
  uint8_t* key_data;
  size_t key_length;
  uint8_t* buffer;
  size_t buffer_len;
  size_t buffer_capacity;
  bool finalized;
} JSNodeHmac;

// Cipher/Decipher class structure
typedef struct {
  JSContext* ctx;
  jsrt_symmetric_algorithm_t algorithm;
  void* evp_ctx;  // OpenSSL EVP_CIPHER_CTX
  openssl_symmetric_funcs_t* openssl_funcs;
  uint8_t* key_data;
  size_t key_length;
  uint8_t* iv_data;
  size_t iv_length;
  uint8_t* aad_data;  // Additional Authenticated Data for GCM
  size_t aad_length;
  uint8_t auth_tag[16];  // Authentication tag for GCM
  bool is_encrypt;
  bool finalized;
} JSNodeCipher;

// Sign class structure
typedef struct {
  JSContext* ctx;
  jsrt_rsa_algorithm_t algorithm;
  jsrt_rsa_hash_algorithm_t hash_algorithm;
  uint8_t* buffer;
  size_t buffer_len;
  size_t buffer_capacity;
  bool finalized;
} JSNodeSign;

// Verify class structure
typedef struct {
  JSContext* ctx;
  jsrt_rsa_algorithm_t algorithm;
  jsrt_rsa_hash_algorithm_t hash_algorithm;
  uint8_t* buffer;
  size_t buffer_len;
  size_t buffer_capacity;
  bool finalized;
} JSNodeVerify;

// Hash API functions
JSValue js_crypto_create_hash(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_node_hash_init_class(JSRuntime* rt);

// HMAC API functions
JSValue js_crypto_create_hmac(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_node_hmac_init_class(JSRuntime* rt);

// Cipher API functions
JSValue js_crypto_create_cipheriv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_create_decipheriv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_node_cipher_init_class(JSRuntime* rt);

// Helper to get OpenSSL symmetric functions (from crypto_symmetric.c)
openssl_symmetric_funcs_t* jsrt_get_openssl_symmetric_funcs(void);

// OpenSSL GCM control constants
#define EVP_CTRL_GCM_SET_IVLEN 0x9
#define EVP_CTRL_GCM_GET_TAG 0x10
#define EVP_CTRL_GCM_SET_TAG 0x11

// Sign/Verify API functions
JSValue js_crypto_create_sign(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_create_verify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_node_sign_init_class(JSRuntime* rt);
void js_node_verify_init_class(JSRuntime* rt);

// Random API functions
JSValue js_crypto_random_bytes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_random_uuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// KDF API functions
JSValue js_crypto_pbkdf2(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_pbkdf2_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_hkdf(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_hkdf_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_scrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_scrypt_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// ECDH API functions
JSValue js_crypto_create_ecdh(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_node_ecdh_init_class(JSRuntime* rt);

// Constants
JSValue create_crypto_constants(JSContext* ctx);

// Utility function: Simple base64 encoding from binary data
static inline char* node_crypto_base64_encode(const uint8_t* data, size_t len) {
  static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t output_len = 4 * ((len + 2) / 3);
  char* output = malloc(output_len + 1);
  if (!output)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t octet_a = i < len ? data[i] : 0;
    uint32_t octet_b = i + 1 < len ? data[i + 1] : 0;
    uint32_t octet_c = i + 2 < len ? data[i + 2] : 0;
    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    output[j++] = base64_table[(triple >> 18) & 63];
    output[j++] = base64_table[(triple >> 12) & 63];
    output[j++] = i + 1 < len ? base64_table[(triple >> 6) & 63] : '=';
    output[j++] = i + 2 < len ? base64_table[triple & 63] : '=';
  }
  output[output_len] = '\0';
  return output;
}

#endif  // __JSRT_NODE_CRYPTO_INTERNAL_H__
