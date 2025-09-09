#ifndef JSRT_CRYPTO_EC_H
#define JSRT_CRYPTO_EC_H

#include <quickjs.h>

// Elliptic curve algorithms
typedef enum { JSRT_EC_P256, JSRT_EC_P384, JSRT_EC_P521 } jsrt_ec_curve_t;

typedef enum { JSRT_ECDSA, JSRT_ECDH } jsrt_ec_algorithm_t;

// EC key generation parameters
typedef struct {
  jsrt_ec_algorithm_t algorithm;
  jsrt_ec_curve_t curve;
  const char* hash;  // For ECDSA
} jsrt_ec_keygen_params_t;

// EC sign parameters
typedef struct {
  const char* hash;
} jsrt_ecdsa_sign_params_t;

// EC derive parameters
typedef struct {
  void* public_key;
  size_t public_key_len;
} jsrt_ecdh_derive_params_t;

// Function declarations
JSValue jsrt_ec_generate_key(JSContext* ctx, const jsrt_ec_keygen_params_t* params);
JSValue jsrt_ec_sign(JSContext* ctx, void* key, const uint8_t* data, size_t data_len,
                     const jsrt_ecdsa_sign_params_t* params);
JSValue jsrt_ec_verify(JSContext* ctx, void* key, const uint8_t* signature, size_t sig_len, const uint8_t* data,
                       size_t data_len, const jsrt_ecdsa_sign_params_t* params);
JSValue jsrt_ec_derive_bits(JSContext* ctx, void* private_key, const jsrt_ecdh_derive_params_t* params);

// Helper functions
int jsrt_ec_curve_from_string(const char* curve_name, jsrt_ec_curve_t* curve);
const char* jsrt_ec_curve_to_string(jsrt_ec_curve_t curve);
int jsrt_ec_get_openssl_nid(jsrt_ec_curve_t curve);

// Memory management
void jsrt_evp_pkey_free_wrapper(void* pkey);

#endif  // JSRT_CRYPTO_EC_H