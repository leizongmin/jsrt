#ifndef JSRT_CRYPTO_HMAC_H
#define JSRT_CRYPTO_HMAC_H

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "crypto_subtle.h"

// HMAC algorithm types
typedef enum { JSRT_HMAC_SHA1 = 0, JSRT_HMAC_SHA256, JSRT_HMAC_SHA384, JSRT_HMAC_SHA512 } jsrt_hmac_algorithm_t;

// HMAC parameters
typedef struct {
  jsrt_hmac_algorithm_t algorithm;

  // Key parameters
  uint8_t* key_data;
  size_t key_length;

  // Hash algorithm info
  const char* hash_name;
  size_t hash_size;  // Output hash size in bytes
} jsrt_hmac_params_t;

// Function declarations

// HMAC key generation
int jsrt_crypto_generate_hmac_key(jsrt_hmac_algorithm_t alg, uint8_t** key_data, size_t* key_data_length);

// HMAC sign (compute MAC)
int jsrt_crypto_hmac_sign(jsrt_hmac_params_t* params, const uint8_t* data, size_t data_length, uint8_t** signature,
                          size_t* signature_length);

// HMAC verify
bool jsrt_crypto_hmac_verify(jsrt_hmac_params_t* params, const uint8_t* data, size_t data_length,
                             const uint8_t* signature, size_t signature_length);

// Helper functions
jsrt_hmac_algorithm_t jsrt_crypto_parse_hmac_algorithm(const char* hash_name);
const char* jsrt_crypto_hmac_algorithm_to_string(jsrt_hmac_algorithm_t alg);
bool jsrt_crypto_is_hmac_algorithm_supported(jsrt_hmac_algorithm_t alg);
size_t jsrt_crypto_get_hmac_hash_size(jsrt_hmac_algorithm_t alg);

// Parameter management
void jsrt_crypto_hmac_params_free(jsrt_hmac_params_t* params);

#endif  // JSRT_CRYPTO_HMAC_H