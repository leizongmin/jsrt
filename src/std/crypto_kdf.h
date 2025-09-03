#ifndef JSRT_CRYPTO_KDF_H
#define JSRT_CRYPTO_KDF_H

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "crypto_subtle.h"

// Key derivation algorithm types
typedef enum { JSRT_KDF_PBKDF2 = 0, JSRT_KDF_HKDF } jsrt_kdf_algorithm_t;

// PBKDF2 parameters
typedef struct {
  jsrt_crypto_algorithm_t hash_algorithm;  // Hash function to use
  uint8_t* salt;                           // Salt value
  size_t salt_length;                      // Salt length
  uint32_t iterations;                     // Number of iterations
} jsrt_pbkdf2_params_t;

// HKDF parameters
typedef struct {
  jsrt_crypto_algorithm_t hash_algorithm;  // Hash function to use
  uint8_t* salt;                           // Salt value (optional)
  size_t salt_length;                      // Salt length
  uint8_t* info;                           // Info parameter (optional)
  size_t info_length;                      // Info length
} jsrt_hkdf_params_t;

// KDF parameters union
typedef struct {
  jsrt_kdf_algorithm_t algorithm;
  union {
    jsrt_pbkdf2_params_t pbkdf2;
    jsrt_hkdf_params_t hkdf;
  } params;
} jsrt_kdf_params_t;

// Function declarations

// PBKDF2 key derivation
int jsrt_crypto_pbkdf2_derive_key(jsrt_pbkdf2_params_t* params, const uint8_t* password, size_t password_length,
                                  size_t key_length, uint8_t** derived_key);

// HKDF key derivation
int jsrt_crypto_hkdf_derive_key(jsrt_hkdf_params_t* params, const uint8_t* input_key_material,
                                size_t input_key_material_length, size_t key_length, uint8_t** derived_key);

// Helper functions
jsrt_kdf_algorithm_t jsrt_crypto_parse_kdf_algorithm(const char* algorithm_name);
const char* jsrt_crypto_kdf_algorithm_to_string(jsrt_kdf_algorithm_t alg);
bool jsrt_crypto_is_kdf_algorithm_supported(jsrt_kdf_algorithm_t alg);

// Parameter management
void jsrt_crypto_kdf_params_free(jsrt_kdf_params_t* params);

#endif  // JSRT_CRYPTO_KDF_H