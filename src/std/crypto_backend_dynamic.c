#include "crypto_backend.h"
#include "crypto_core.h"
#include "../util/debug.h"
#include "crypto_symmetric.h"

#include <stdio.h>

// Dynamic backend function pointers
static jsrt_crypto_openssl_funcs_t dynamic_openssl_funcs = {0};
static bool dynamic_funcs_initialized = false;

// Include existing dynamic implementation headers  
extern int jsrt_crypto_digest_data(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len,
                                   uint8_t** output, size_t* output_len);

// Dynamic backend initialization
static bool dynamic_backend_init(void) {
  JSRT_Debug("Initializing dynamic OpenSSL crypto backend");
  
  if (!dynamic_funcs_initialized) {
    // Get the dynamic OpenSSL handle from existing crypto setup
    extern void* openssl_handle;  // Defined in crypto.c or similar
    
    if (!openssl_handle) {
      JSRT_Debug("OpenSSL handle not available for dynamic backend");
      return false;
    }
    
    if (!jsrt_crypto_core_setup_dynamic_funcs(&dynamic_openssl_funcs, openssl_handle)) {
      JSRT_Debug("Failed to setup dynamic OpenSSL functions");
      return false;
    }
    dynamic_funcs_initialized = true;
  }
  
  return true;
}

// Dynamic backend cleanup
static void dynamic_backend_cleanup(void) {
  JSRT_Debug("Cleaning up dynamic OpenSSL crypto backend");
  // Cleanup is handled by existing crypto teardown
}

// Dynamic digest wrapper (using unified crypto_core)
static int dynamic_digest_data(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len,
                              uint8_t** output, size_t* output_len) {
  if (!dynamic_funcs_initialized) {
    JSRT_Debug("Dynamic OpenSSL functions not initialized");
    return -1;
  }
  
  return jsrt_crypto_core_digest(&dynamic_openssl_funcs, alg, input, input_len, output, output_len);
}

// Dynamic AES key generation wrapper (using unified crypto_core)
static int dynamic_generate_aes_key(size_t key_length_bits, uint8_t** key_data, size_t* key_data_length) {
  if (!dynamic_funcs_initialized) {
    JSRT_Debug("Dynamic OpenSSL functions not initialized");
    return -1;
  }
  
  return jsrt_crypto_core_generate_aes_key(&dynamic_openssl_funcs, key_length_bits, key_data, key_data_length);
}

// Dynamic AES encryption wrapper
static int dynamic_aes_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                              uint8_t** ciphertext, size_t* ciphertext_length) {
  return jsrt_crypto_aes_encrypt(params, plaintext, plaintext_length, ciphertext, ciphertext_length);
}

// Dynamic AES decryption wrapper
static int dynamic_aes_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                              uint8_t** plaintext, size_t* plaintext_length) {
  return jsrt_crypto_aes_decrypt(params, ciphertext, ciphertext_length, plaintext, plaintext_length);
}

// Dynamic random number generation (using unified crypto_core)
static int dynamic_get_random_bytes(uint8_t* buffer, size_t length) {
  if (!dynamic_funcs_initialized) {
    JSRT_Debug("Dynamic OpenSSL functions not initialized");
    return -1;
  }
  
  return jsrt_crypto_core_get_random_bytes(&dynamic_openssl_funcs, buffer, length);
}

// Dynamic UUID generation (using unified crypto_core)
static int dynamic_random_uuid(char* uuid_str, size_t uuid_str_size) {
  if (!dynamic_funcs_initialized) {
    JSRT_Debug("Dynamic OpenSSL functions not initialized");
    return -1;
  }
  
  return jsrt_crypto_core_random_uuid(&dynamic_openssl_funcs, uuid_str, uuid_str_size);
}

// Dynamic version info (using external OpenSSL)
extern const char* JSRT_GetOpenSSLVersion(void);
static const char* dynamic_get_version(void) {
  return JSRT_GetOpenSSLVersion();
}

// Create dynamic backend
jsrt_crypto_backend_t* jsrt_crypto_backend_create_dynamic(void) {
  jsrt_crypto_backend_t* backend = malloc(sizeof(jsrt_crypto_backend_t));
  if (!backend) {
    return NULL;
  }

  backend->type = JSRT_CRYPTO_BACKEND_DYNAMIC;
  backend->init = dynamic_backend_init;
  backend->cleanup = dynamic_backend_cleanup;
  backend->digest = dynamic_digest_data;
  backend->generate_aes_key = dynamic_generate_aes_key;
  backend->aes_encrypt = dynamic_aes_encrypt;
  backend->aes_decrypt = dynamic_aes_decrypt;
  backend->get_random_bytes = dynamic_get_random_bytes;
  backend->random_uuid = dynamic_random_uuid;
  backend->get_version = dynamic_get_version;

  return backend;
}