#include "crypto_backend.h"
#include "../util/debug.h"
#include "crypto_symmetric.h"

#include <stdio.h>

// Include existing dynamic implementation headers  
extern int jsrt_crypto_digest_data(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len,
                                   uint8_t** output, size_t* output_len);

// Dynamic backend initialization
static bool dynamic_backend_init(void) {
  JSRT_Debug("Initializing dynamic OpenSSL crypto backend");
  // The dynamic backend initialization is handled by existing crypto setup
  return true;
}

// Dynamic backend cleanup
static void dynamic_backend_cleanup(void) {
  JSRT_Debug("Cleaning up dynamic OpenSSL crypto backend");
  // Cleanup is handled by existing crypto teardown
}

// Dynamic digest wrapper (using existing implementation)
static int dynamic_digest_data(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len,
                              uint8_t** output, size_t* output_len) {
  return jsrt_crypto_digest_data(alg, input, input_len, output, output_len);
}

// Dynamic AES key generation wrapper
static int dynamic_generate_aes_key(size_t key_length_bits, uint8_t** key_data, size_t* key_data_length) {
  return jsrt_crypto_generate_aes_key(key_length_bits, key_data, key_data_length);
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

// Dynamic random number generation (using external OpenSSL)
extern void* openssl_handle;
static int dynamic_get_random_bytes(uint8_t* buffer, size_t length) {
  // Use dynamic loading to get RAND_bytes function
  if (!openssl_handle) {
    JSRT_Debug("OpenSSL handle not available");
    return -1;
  }

#ifdef _WIN32
  #include <windows.h>
  #define JSRT_DLSYM(handle, name) ((void*)GetProcAddress(handle, name))
#else
  #include <dlfcn.h>
  #define JSRT_DLSYM(handle, name) dlsym(handle, name)
#endif

  int (*RAND_bytes_func)(unsigned char*, int) = JSRT_DLSYM(openssl_handle, "RAND_bytes");
  if (!RAND_bytes_func) {
    JSRT_Debug("RAND_bytes function not found in OpenSSL library");
    return -1;
  }

  return (RAND_bytes_func(buffer, (int)length) == 1) ? 0 : -1;
}

// Dynamic UUID generation
static int dynamic_random_uuid(char* uuid_str, size_t uuid_str_size) {
  if (uuid_str_size < 37) {  // UUID string length + null terminator
    return -1;
  }

  unsigned char random_bytes[16];
  if (dynamic_get_random_bytes(random_bytes, 16) != 0) {
    return -1;
  }

  // Set version bits (4 bits): version 4 (random)
  random_bytes[6] = (random_bytes[6] & 0x0F) | 0x40;

  // Set variant bits (2 bits): variant 1 (RFC 4122)
  random_bytes[8] = (random_bytes[8] & 0x3F) | 0x80;

  // Format as UUID string
  snprintf(uuid_str, uuid_str_size, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           random_bytes[0], random_bytes[1], random_bytes[2], random_bytes[3], random_bytes[4], random_bytes[5],
           random_bytes[6], random_bytes[7], random_bytes[8], random_bytes[9], random_bytes[10], random_bytes[11],
           random_bytes[12], random_bytes[13], random_bytes[14], random_bytes[15]);

  return 0;
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