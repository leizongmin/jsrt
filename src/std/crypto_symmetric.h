#ifndef JSRT_CRYPTO_SYMMETRIC_H
#define JSRT_CRYPTO_SYMMETRIC_H

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "crypto_subtle.h"

// AES key lengths in bits
#define JSRT_AES_128_KEY_SIZE 16  // 128 bits
#define JSRT_AES_192_KEY_SIZE 24  // 192 bits
#define JSRT_AES_256_KEY_SIZE 32  // 256 bits

// AES block size
#define JSRT_AES_BLOCK_SIZE 16  // 128 bits

// GCM tag size
#define JSRT_GCM_TAG_SIZE 16  // 128 bits

// Maximum IV size for different modes
#define JSRT_AES_CBC_IV_SIZE 16  // 128 bits
#define JSRT_AES_GCM_IV_SIZE 12  // 96 bits (recommended)
#define JSRT_AES_CTR_IV_SIZE 16  // 128 bits (counter block)

// Symmetric encryption algorithm types
typedef enum { JSRT_SYMMETRIC_AES_CBC = 0, JSRT_SYMMETRIC_AES_GCM, JSRT_SYMMETRIC_AES_CTR } jsrt_symmetric_algorithm_t;

// Symmetric encryption parameters
typedef struct {
  jsrt_symmetric_algorithm_t algorithm;

  // Key parameters
  uint8_t* key_data;
  size_t key_length;

  // Algorithm-specific parameters
  union {
    struct {
      uint8_t* iv;
      size_t iv_length;
    } cbc;

    struct {
      uint8_t* iv;
      size_t iv_length;
      uint8_t* additional_data;
      size_t additional_data_length;
      size_t tag_length;  // Usually 16 bytes
    } gcm;

    struct {
      uint8_t* counter;
      size_t counter_length;
      uint32_t length;  // Counter length in bits (usually 64)
    } ctr;
  } params;
} jsrt_symmetric_params_t;

// Function declarations

// AES key generation
int jsrt_crypto_generate_aes_key(size_t key_length_bits, uint8_t** key_data, size_t* key_data_length);

// AES encryption/decryption functions
int jsrt_crypto_aes_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                            uint8_t** ciphertext, size_t* ciphertext_length);

int jsrt_crypto_aes_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                            uint8_t** plaintext, size_t* plaintext_length);

// Helper functions
jsrt_symmetric_algorithm_t jsrt_crypto_parse_symmetric_algorithm(const char* algorithm_name);
const char* jsrt_crypto_symmetric_algorithm_to_string(jsrt_symmetric_algorithm_t alg);
bool jsrt_crypto_is_symmetric_algorithm_supported(jsrt_symmetric_algorithm_t alg);
size_t jsrt_crypto_get_aes_key_size(jsrt_crypto_algorithm_t alg, int key_length_bits);

// Parameter management
void jsrt_crypto_symmetric_params_free(jsrt_symmetric_params_t* params);

#endif  // JSRT_CRYPTO_SYMMETRIC_H