#ifndef JSRT_CRYPTO_BACKEND_H
#define JSRT_CRYPTO_BACKEND_H

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "crypto_subtle.h"
#include "crypto_symmetric.h"

// Backend implementation type
typedef enum {
  JSRT_CRYPTO_BACKEND_DYNAMIC,  // Dynamic loading of OpenSSL
  JSRT_CRYPTO_BACKEND_STATIC    // Static linking with OpenSSL
} jsrt_crypto_backend_type_t;

// Backend interface for crypto operations
typedef struct {
  // Backend type
  jsrt_crypto_backend_type_t type;
  
  // Initialization/cleanup
  bool (*init)(void);
  void (*cleanup)(void);
  
  // Digest operations
  int (*digest)(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len,
               uint8_t** output, size_t* output_len);
  
  // Symmetric key operations
  int (*generate_aes_key)(size_t key_length_bits, uint8_t** key_data, size_t* key_data_length);
  int (*aes_encrypt)(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                    uint8_t** ciphertext, size_t* ciphertext_length);
  int (*aes_decrypt)(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                    uint8_t** plaintext, size_t* plaintext_length);
  
  // Random number generation
  int (*get_random_bytes)(uint8_t* buffer, size_t length);
  int (*random_uuid)(char* uuid_str, size_t uuid_str_size);
  
  // Version info
  const char* (*get_version)(void);
  
} jsrt_crypto_backend_t;

// Global backend instance
extern jsrt_crypto_backend_t* g_crypto_backend;

// Backend factory functions
jsrt_crypto_backend_t* jsrt_crypto_backend_create_dynamic(void);
jsrt_crypto_backend_t* jsrt_crypto_backend_create_static(void);

// Backend management
bool jsrt_crypto_backend_init(jsrt_crypto_backend_type_t type);
void jsrt_crypto_backend_cleanup(void);

// Unified crypto operations (uses current backend)
int jsrt_crypto_unified_digest(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len,
                              uint8_t** output, size_t* output_len);
int jsrt_crypto_unified_generate_aes_key(size_t key_length_bits, uint8_t** key_data, size_t* key_data_length);
int jsrt_crypto_unified_aes_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                                   uint8_t** ciphertext, size_t* ciphertext_length);
int jsrt_crypto_unified_aes_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                                   uint8_t** plaintext, size_t* plaintext_length);
int jsrt_crypto_unified_get_random_bytes(uint8_t* buffer, size_t length);
int jsrt_crypto_unified_random_uuid(char* uuid_str, size_t uuid_str_size);
const char* jsrt_crypto_unified_get_version(void);

#endif // JSRT_CRYPTO_BACKEND_H