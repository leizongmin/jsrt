#ifndef JSRT_CRYPTO_RSA_H
#define JSRT_CRYPTO_RSA_H

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "crypto_subtle.h"

// RSA algorithm types
typedef enum { JSRT_RSA_OAEP = 0, JSRT_RSA_PKCS1_V1_5, JSRT_RSASSA_PKCS1_V1_5, JSRT_RSA_PSS } jsrt_rsa_algorithm_t;

// RSA hash algorithm types
typedef enum {
  JSRT_RSA_HASH_SHA1 = 0,
  JSRT_RSA_HASH_SHA256,
  JSRT_RSA_HASH_SHA384,
  JSRT_RSA_HASH_SHA512
} jsrt_rsa_hash_algorithm_t;

// RSA key pair
typedef struct {
  void* public_key;   // OpenSSL EVP_PKEY for public key
  void* private_key;  // OpenSSL EVP_PKEY for private key
  size_t modulus_length_bits;
  uint32_t public_exponent;
  jsrt_rsa_hash_algorithm_t hash_algorithm;
} jsrt_rsa_keypair_t;

// RSA encryption/decryption parameters
typedef struct {
  jsrt_rsa_algorithm_t algorithm;
  jsrt_rsa_hash_algorithm_t hash_algorithm;

  // Key data
  void* rsa_key;  // OpenSSL EVP_PKEY

  // Algorithm-specific parameters
  union {
    struct {
      uint8_t* label;
      size_t label_length;
    } oaep;

    struct {
      size_t salt_length;  // For PSS padding
    } pss;
  } params;
} jsrt_rsa_params_t;

// Function declarations

// RSA key pair generation
int jsrt_crypto_generate_rsa_keypair(size_t modulus_length_bits, uint32_t public_exponent,
                                     jsrt_rsa_hash_algorithm_t hash_alg, jsrt_rsa_keypair_t** keypair);

// RSA encryption/decryption
int jsrt_crypto_rsa_encrypt(jsrt_rsa_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                            uint8_t** ciphertext, size_t* ciphertext_length);

int jsrt_crypto_rsa_decrypt(jsrt_rsa_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                            uint8_t** plaintext, size_t* plaintext_length);

// RSA signature/verification
int jsrt_crypto_rsa_sign(jsrt_rsa_params_t* params, const uint8_t* data, size_t data_length, uint8_t** signature,
                         size_t* signature_length);

bool jsrt_crypto_rsa_verify(jsrt_rsa_params_t* params, const uint8_t* data, size_t data_length,
                            const uint8_t* signature, size_t signature_length);

// Helper functions
jsrt_rsa_algorithm_t jsrt_crypto_parse_rsa_algorithm(const char* algorithm_name);
jsrt_rsa_hash_algorithm_t jsrt_crypto_parse_rsa_hash_algorithm(const char* hash_name);
const char* jsrt_crypto_rsa_algorithm_to_string(jsrt_rsa_algorithm_t alg);
const char* jsrt_crypto_rsa_hash_algorithm_to_string(jsrt_rsa_hash_algorithm_t hash_alg);
bool jsrt_crypto_is_rsa_algorithm_supported(jsrt_rsa_algorithm_t alg);
bool jsrt_crypto_is_rsa_hash_supported(jsrt_rsa_hash_algorithm_t hash_alg);

// Memory management
void jsrt_crypto_rsa_keypair_free(jsrt_rsa_keypair_t* keypair);
void jsrt_crypto_rsa_params_free(jsrt_rsa_params_t* params);

// Key extraction functions
int jsrt_crypto_rsa_extract_public_key_data(void* public_key, uint8_t** key_data, size_t* key_data_length);
int jsrt_crypto_rsa_extract_private_key_data(void* private_key, uint8_t** key_data, size_t* key_data_length);

// Create EVP_PKEY from DER-encoded key data
void* jsrt_crypto_rsa_create_public_key_from_der(const uint8_t* key_data, size_t key_data_length);
void* jsrt_crypto_rsa_create_private_key_from_der(const uint8_t* key_data, size_t key_data_length);

#endif  // JSRT_CRYPTO_RSA_H