#include "crypto_symmetric.h"

// Platform-specific includes for dynamic loading
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"
#include "crypto.h"

// Cross-platform dynamic loading abstractions
#ifdef _WIN32
extern HMODULE openssl_handle;
#define JSRT_DLSYM(handle, name) ((void*)GetProcAddress(handle, name))
#else
extern void* openssl_handle;
#define JSRT_DLSYM(handle, name) dlsym(handle, name)
#endif

static openssl_symmetric_funcs_t openssl_symmetric_funcs = {0};
static bool symmetric_funcs_loaded = false;

// OpenSSL constants
#define EVP_CTRL_GCM_SET_IVLEN 0x9
#define EVP_CTRL_GCM_GET_TAG 0x10
#define EVP_CTRL_GCM_SET_TAG 0x11
#define EVP_CTRL_GCM_SET_IV_FIXED -1

// Load OpenSSL symmetric encryption functions
static bool load_symmetric_functions(void) {
  if (symmetric_funcs_loaded) {
    return openssl_symmetric_funcs.EVP_aes_256_cbc != NULL;
  }

  if (!openssl_handle) {
    JSRT_Debug("JSRT_Crypto_Symmetric: OpenSSL handle not available");
    return false;
  }

  // Load cipher functions
  openssl_symmetric_funcs.EVP_aes_128_cbc = JSRT_DLSYM(openssl_handle, "EVP_aes_128_cbc");
  openssl_symmetric_funcs.EVP_aes_192_cbc = JSRT_DLSYM(openssl_handle, "EVP_aes_192_cbc");
  openssl_symmetric_funcs.EVP_aes_256_cbc = JSRT_DLSYM(openssl_handle, "EVP_aes_256_cbc");
  openssl_symmetric_funcs.EVP_aes_128_gcm = JSRT_DLSYM(openssl_handle, "EVP_aes_128_gcm");
  openssl_symmetric_funcs.EVP_aes_192_gcm = JSRT_DLSYM(openssl_handle, "EVP_aes_192_gcm");
  openssl_symmetric_funcs.EVP_aes_256_gcm = JSRT_DLSYM(openssl_handle, "EVP_aes_256_gcm");
  openssl_symmetric_funcs.EVP_aes_128_ctr = JSRT_DLSYM(openssl_handle, "EVP_aes_128_ctr");
  openssl_symmetric_funcs.EVP_aes_192_ctr = JSRT_DLSYM(openssl_handle, "EVP_aes_192_ctr");
  openssl_symmetric_funcs.EVP_aes_256_ctr = JSRT_DLSYM(openssl_handle, "EVP_aes_256_ctr");

  // Load context management functions
  openssl_symmetric_funcs.EVP_CIPHER_CTX_new = JSRT_DLSYM(openssl_handle, "EVP_CIPHER_CTX_new");
  openssl_symmetric_funcs.EVP_CIPHER_CTX_free = JSRT_DLSYM(openssl_handle, "EVP_CIPHER_CTX_free");

  // Load encryption functions
  openssl_symmetric_funcs.EVP_EncryptInit_ex = JSRT_DLSYM(openssl_handle, "EVP_EncryptInit_ex");
  openssl_symmetric_funcs.EVP_EncryptUpdate = JSRT_DLSYM(openssl_handle, "EVP_EncryptUpdate");
  openssl_symmetric_funcs.EVP_EncryptFinal_ex = JSRT_DLSYM(openssl_handle, "EVP_EncryptFinal_ex");

  // Load decryption functions
  openssl_symmetric_funcs.EVP_DecryptInit_ex = JSRT_DLSYM(openssl_handle, "EVP_DecryptInit_ex");
  openssl_symmetric_funcs.EVP_DecryptUpdate = JSRT_DLSYM(openssl_handle, "EVP_DecryptUpdate");
  openssl_symmetric_funcs.EVP_DecryptFinal_ex = JSRT_DLSYM(openssl_handle, "EVP_DecryptFinal_ex");

  // Load GCM specific functions
  openssl_symmetric_funcs.EVP_CIPHER_CTX_ctrl = JSRT_DLSYM(openssl_handle, "EVP_CIPHER_CTX_ctrl");

  // Load random function for key generation
  openssl_symmetric_funcs.RAND_bytes = JSRT_DLSYM(openssl_handle, "RAND_bytes");

  symmetric_funcs_loaded = true;

  // Check if all required functions were loaded
  bool success = openssl_symmetric_funcs.EVP_aes_256_cbc && openssl_symmetric_funcs.EVP_CIPHER_CTX_new &&
                 openssl_symmetric_funcs.EVP_CIPHER_CTX_free && openssl_symmetric_funcs.EVP_EncryptInit_ex &&
                 openssl_symmetric_funcs.EVP_EncryptUpdate && openssl_symmetric_funcs.EVP_EncryptFinal_ex &&
                 openssl_symmetric_funcs.EVP_DecryptInit_ex && openssl_symmetric_funcs.EVP_DecryptUpdate &&
                 openssl_symmetric_funcs.EVP_DecryptFinal_ex;

  if (success) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Successfully loaded OpenSSL symmetric encryption functions");
  } else {
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to load some OpenSSL symmetric encryption functions");
  }

  return success;
}

// Get OpenSSL cipher for algorithm and key size
static const void* get_openssl_cipher(jsrt_symmetric_algorithm_t alg, size_t key_length) {
  if (!load_symmetric_functions()) {
    return NULL;
  }

  switch (alg) {
    case JSRT_SYMMETRIC_AES_CBC:
      switch (key_length) {
        case JSRT_AES_128_KEY_SIZE:
          return openssl_symmetric_funcs.EVP_aes_128_cbc ? openssl_symmetric_funcs.EVP_aes_128_cbc() : NULL;
        case JSRT_AES_192_KEY_SIZE:
          return openssl_symmetric_funcs.EVP_aes_192_cbc ? openssl_symmetric_funcs.EVP_aes_192_cbc() : NULL;
        case JSRT_AES_256_KEY_SIZE:
          return openssl_symmetric_funcs.EVP_aes_256_cbc ? openssl_symmetric_funcs.EVP_aes_256_cbc() : NULL;
      }
      break;

    case JSRT_SYMMETRIC_AES_GCM:
      switch (key_length) {
        case JSRT_AES_128_KEY_SIZE:
          return openssl_symmetric_funcs.EVP_aes_128_gcm ? openssl_symmetric_funcs.EVP_aes_128_gcm() : NULL;
        case JSRT_AES_192_KEY_SIZE:
          return openssl_symmetric_funcs.EVP_aes_192_gcm ? openssl_symmetric_funcs.EVP_aes_192_gcm() : NULL;
        case JSRT_AES_256_KEY_SIZE:
          return openssl_symmetric_funcs.EVP_aes_256_gcm ? openssl_symmetric_funcs.EVP_aes_256_gcm() : NULL;
      }
      break;

    case JSRT_SYMMETRIC_AES_CTR:
      switch (key_length) {
        case JSRT_AES_128_KEY_SIZE:
          return openssl_symmetric_funcs.EVP_aes_128_ctr ? openssl_symmetric_funcs.EVP_aes_128_ctr() : NULL;
        case JSRT_AES_192_KEY_SIZE:
          return openssl_symmetric_funcs.EVP_aes_192_ctr ? openssl_symmetric_funcs.EVP_aes_192_ctr() : NULL;
        case JSRT_AES_256_KEY_SIZE:
          return openssl_symmetric_funcs.EVP_aes_256_ctr ? openssl_symmetric_funcs.EVP_aes_256_ctr() : NULL;
      }
      break;
  }

  return NULL;
}

// Generate AES key
int jsrt_crypto_generate_aes_key(size_t key_length_bits, uint8_t** key_data, size_t* key_data_length) {
  if (!load_symmetric_functions() || !openssl_symmetric_funcs.RAND_bytes) {
    JSRT_Debug("JSRT_Crypto_Symmetric: OpenSSL functions not available for key generation");
    return -1;
  }

  size_t key_bytes = key_length_bits / 8;
  if (key_bytes != JSRT_AES_128_KEY_SIZE && key_bytes != JSRT_AES_192_KEY_SIZE && key_bytes != JSRT_AES_256_KEY_SIZE) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Invalid AES key length: %zu bits", key_length_bits);
    return -1;
  }

  *key_data = malloc(key_bytes);
  if (!*key_data) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to allocate key buffer");
    return -1;
  }

  if (openssl_symmetric_funcs.RAND_bytes(*key_data, key_bytes) != 1) {
    free(*key_data);
    *key_data = NULL;
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to generate random key");
    return -1;
  }

  *key_data_length = key_bytes;
  JSRT_Debug("JSRT_Crypto_Symmetric: Successfully generated %zu-bit AES key", key_length_bits);
  return 0;
}

// AES-CBC encryption
static int jsrt_crypto_aes_cbc_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext,
                                       size_t plaintext_length, uint8_t** ciphertext, size_t* ciphertext_length) {
  const void* cipher = get_openssl_cipher(JSRT_SYMMETRIC_AES_CBC, params->key_length);
  if (!cipher) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Unsupported AES-CBC key length: %zu", params->key_length);
    return -1;
  }

  void* ctx = openssl_symmetric_funcs.EVP_CIPHER_CTX_new();
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to create cipher context");
    return -1;
  }

  // Initialize encryption
  if (openssl_symmetric_funcs.EVP_EncryptInit_ex(ctx, cipher, NULL, params->key_data, params->params.cbc.iv) != 1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to initialize AES-CBC encryption");
    return -1;
  }

  // Allocate output buffer (plaintext + padding)
  size_t max_ciphertext_len = plaintext_length + JSRT_AES_BLOCK_SIZE;
  *ciphertext = malloc(max_ciphertext_len);
  if (!*ciphertext) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to allocate ciphertext buffer");
    return -1;
  }

  // Encrypt
  int len;
  if (openssl_symmetric_funcs.EVP_EncryptUpdate(ctx, *ciphertext, &len, plaintext, plaintext_length) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to encrypt data");
    return -1;
  }

  *ciphertext_length = len;

  // Finalize encryption (handles padding)
  int final_len;
  if (openssl_symmetric_funcs.EVP_EncryptFinal_ex(ctx, *ciphertext + len, &final_len) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to finalize encryption");
    return -1;
  }

  *ciphertext_length += final_len;
  openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_Symmetric: Successfully encrypted %zu bytes to %zu bytes (AES-CBC)", plaintext_length,
             *ciphertext_length);
  return 0;
}

// AES-GCM encryption
static int jsrt_crypto_aes_gcm_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext,
                                       size_t plaintext_length, uint8_t** ciphertext, size_t* ciphertext_length) {
  const void* cipher = get_openssl_cipher(JSRT_SYMMETRIC_AES_GCM, params->key_length);
  if (!cipher) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Unsupported AES-GCM key length: %zu", params->key_length);
    return -1;
  }

  void* ctx = openssl_symmetric_funcs.EVP_CIPHER_CTX_new();
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to create cipher context");
    return -1;
  }

  // Initialize encryption
  if (openssl_symmetric_funcs.EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to initialize AES-GCM encryption");
    return -1;
  }

  // Set IV length
  if (openssl_symmetric_funcs.EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, params->params.gcm.iv_length, NULL) !=
      1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to set GCM IV length");
    return -1;
  }

  // Initialize key and IV
  if (openssl_symmetric_funcs.EVP_EncryptInit_ex(ctx, NULL, NULL, params->key_data, params->params.gcm.iv) != 1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to set AES-GCM key and IV");
    return -1;
  }

  // Set additional authenticated data if provided
  if (params->params.gcm.additional_data && params->params.gcm.additional_data_length > 0) {
    int len;
    if (openssl_symmetric_funcs.EVP_EncryptUpdate(ctx, NULL, &len, params->params.gcm.additional_data,
                                                  params->params.gcm.additional_data_length) != 1) {
      openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
      JSRT_Debug("JSRT_Crypto_Symmetric: Failed to set additional authenticated data");
      return -1;
    }
  }

  // Allocate output buffer (plaintext + tag)
  size_t max_ciphertext_len = plaintext_length + params->params.gcm.tag_length;
  *ciphertext = malloc(max_ciphertext_len);
  if (!*ciphertext) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to allocate ciphertext buffer");
    return -1;
  }

  // Encrypt plaintext
  int len;
  if (openssl_symmetric_funcs.EVP_EncryptUpdate(ctx, *ciphertext, &len, plaintext, plaintext_length) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to encrypt data");
    return -1;
  }

  *ciphertext_length = len;

  // Finalize encryption
  int final_len;
  if (openssl_symmetric_funcs.EVP_EncryptFinal_ex(ctx, *ciphertext + len, &final_len) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to finalize encryption");
    return -1;
  }

  *ciphertext_length += final_len;

  // Get authentication tag
  if (openssl_symmetric_funcs.EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, params->params.gcm.tag_length,
                                                  *ciphertext + *ciphertext_length) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to get authentication tag");
    return -1;
  }

  *ciphertext_length += params->params.gcm.tag_length;
  openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_Symmetric: Successfully encrypted %zu bytes to %zu bytes (AES-GCM)", plaintext_length,
             *ciphertext_length);
  return 0;
}

// AES-GCM decryption
static int jsrt_crypto_aes_gcm_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext,
                                       size_t ciphertext_length, uint8_t** plaintext, size_t* plaintext_length) {
  const void* cipher = get_openssl_cipher(JSRT_SYMMETRIC_AES_GCM, params->key_length);
  if (!cipher) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Unsupported AES-GCM key length: %zu", params->key_length);
    return -1;
  }

  // Verify ciphertext length includes tag
  if (ciphertext_length < params->params.gcm.tag_length) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Ciphertext too short for GCM tag");
    return -1;
  }

  size_t actual_ciphertext_length = ciphertext_length - params->params.gcm.tag_length;
  const uint8_t* tag = ciphertext + actual_ciphertext_length;

  void* ctx = openssl_symmetric_funcs.EVP_CIPHER_CTX_new();
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to create cipher context");
    return -1;
  }

  // Initialize decryption
  if (openssl_symmetric_funcs.EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to initialize AES-GCM decryption");
    return -1;
  }

  // Set IV length
  if (openssl_symmetric_funcs.EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, params->params.gcm.iv_length, NULL) !=
      1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to set GCM IV length");
    return -1;
  }

  // Initialize key and IV
  if (openssl_symmetric_funcs.EVP_DecryptInit_ex(ctx, NULL, NULL, params->key_data, params->params.gcm.iv) != 1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to set AES-GCM key and IV");
    return -1;
  }

  // Set additional authenticated data if provided
  if (params->params.gcm.additional_data && params->params.gcm.additional_data_length > 0) {
    int len;
    if (openssl_symmetric_funcs.EVP_DecryptUpdate(ctx, NULL, &len, params->params.gcm.additional_data,
                                                  params->params.gcm.additional_data_length) != 1) {
      openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
      JSRT_Debug("JSRT_Crypto_Symmetric: Failed to set additional authenticated data");
      return -1;
    }
  }

  // Allocate output buffer
  *plaintext = malloc(actual_ciphertext_length);
  if (!*plaintext) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to allocate plaintext buffer");
    return -1;
  }

  // Decrypt ciphertext
  int len;
  if (openssl_symmetric_funcs.EVP_DecryptUpdate(ctx, *plaintext, &len, ciphertext, actual_ciphertext_length) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to decrypt data");
    return -1;
  }

  *plaintext_length = len;

  // Set expected tag for authentication
  if (openssl_symmetric_funcs.EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, params->params.gcm.tag_length,
                                                  (void*)tag) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to set authentication tag");
    return -1;
  }

  // Finalize decryption and verify tag
  int final_len;
  int ret = openssl_symmetric_funcs.EVP_DecryptFinal_ex(ctx, *plaintext + len, &final_len);
  if (ret <= 0) {
    free(*plaintext);
    *plaintext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to finalize decryption (authentication failed)");
    return -1;
  }

  *plaintext_length += final_len;
  openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_Symmetric: Successfully decrypted %zu bytes to %zu bytes (AES-GCM)", ciphertext_length,
             *plaintext_length);
  return 0;
}

// AES-CTR encryption
static int jsrt_crypto_aes_ctr_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext,
                                       size_t plaintext_length, uint8_t** ciphertext, size_t* ciphertext_length) {
  const void* cipher = get_openssl_cipher(JSRT_SYMMETRIC_AES_CTR, params->key_length);
  if (!cipher) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Unsupported AES-CTR key length: %zu", params->key_length);
    return -1;
  }

  void* ctx = openssl_symmetric_funcs.EVP_CIPHER_CTX_new();
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to create cipher context");
    return -1;
  }

  // Initialize encryption
  if (openssl_symmetric_funcs.EVP_EncryptInit_ex(ctx, cipher, NULL, params->key_data, params->params.ctr.counter) !=
      1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to initialize AES-CTR encryption");
    return -1;
  }

  // Allocate output buffer (CTR mode doesn't expand the size)
  *ciphertext = malloc(plaintext_length);
  if (!*ciphertext) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to allocate ciphertext buffer");
    return -1;
  }

  // Encrypt
  int len;
  if (openssl_symmetric_funcs.EVP_EncryptUpdate(ctx, *ciphertext, &len, plaintext, plaintext_length) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to encrypt data");
    return -1;
  }

  *ciphertext_length = len;

  // Finalize encryption (CTR mode doesn't use padding)
  int final_len;
  if (openssl_symmetric_funcs.EVP_EncryptFinal_ex(ctx, *ciphertext + len, &final_len) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to finalize encryption");
    return -1;
  }

  *ciphertext_length += final_len;
  openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_Symmetric: Successfully encrypted %zu bytes to %zu bytes (AES-CTR)", plaintext_length,
             *ciphertext_length);
  return 0;
}

// AES-CTR decryption
static int jsrt_crypto_aes_ctr_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext,
                                       size_t ciphertext_length, uint8_t** plaintext, size_t* plaintext_length) {
  const void* cipher = get_openssl_cipher(JSRT_SYMMETRIC_AES_CTR, params->key_length);
  if (!cipher) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Unsupported AES-CTR key length: %zu", params->key_length);
    return -1;
  }

  void* ctx = openssl_symmetric_funcs.EVP_CIPHER_CTX_new();
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to create cipher context");
    return -1;
  }

  // Initialize decryption (CTR mode is symmetric - encryption and decryption are the same)
  if (openssl_symmetric_funcs.EVP_DecryptInit_ex(ctx, cipher, NULL, params->key_data, params->params.ctr.counter) !=
      1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to initialize AES-CTR decryption");
    return -1;
  }

  // Allocate output buffer
  *plaintext = malloc(ciphertext_length);
  if (!*plaintext) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to allocate plaintext buffer");
    return -1;
  }

  // Decrypt
  int len;
  if (openssl_symmetric_funcs.EVP_DecryptUpdate(ctx, *plaintext, &len, ciphertext, ciphertext_length) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to decrypt data");
    return -1;
  }

  *plaintext_length = len;

  // Finalize decryption
  int final_len;
  if (openssl_symmetric_funcs.EVP_DecryptFinal_ex(ctx, *plaintext + len, &final_len) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to finalize decryption");
    return -1;
  }

  *plaintext_length += final_len;
  openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_Symmetric: Successfully decrypted %zu bytes to %zu bytes (AES-CTR)", ciphertext_length,
             *plaintext_length);
  return 0;
}

// AES-CBC decryption
static int jsrt_crypto_aes_cbc_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext,
                                       size_t ciphertext_length, uint8_t** plaintext, size_t* plaintext_length) {
  const void* cipher = get_openssl_cipher(JSRT_SYMMETRIC_AES_CBC, params->key_length);
  if (!cipher) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Unsupported AES-CBC key length: %zu", params->key_length);
    return -1;
  }

  void* ctx = openssl_symmetric_funcs.EVP_CIPHER_CTX_new();
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to create cipher context");
    return -1;
  }

  // Initialize decryption
  if (openssl_symmetric_funcs.EVP_DecryptInit_ex(ctx, cipher, NULL, params->key_data, params->params.cbc.iv) != 1) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to initialize AES-CBC decryption");
    return -1;
  }

  // Allocate output buffer
  *plaintext = malloc(ciphertext_length + JSRT_AES_BLOCK_SIZE);
  if (!*plaintext) {
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to allocate plaintext buffer");
    return -1;
  }

  // Decrypt
  int len;
  if (openssl_symmetric_funcs.EVP_DecryptUpdate(ctx, *plaintext, &len, ciphertext, ciphertext_length) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to decrypt data");
    return -1;
  }

  *plaintext_length = len;

  // Finalize decryption (handles padding removal)
  int final_len;
  if (openssl_symmetric_funcs.EVP_DecryptFinal_ex(ctx, *plaintext + len, &final_len) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Symmetric: Failed to finalize decryption (possibly incorrect padding)");
    return -1;
  }

  *plaintext_length += final_len;
  openssl_symmetric_funcs.EVP_CIPHER_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_Symmetric: Successfully decrypted %zu bytes to %zu bytes (AES-CBC)", ciphertext_length,
             *plaintext_length);
  return 0;
}

// Main encryption function
int jsrt_crypto_aes_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                            uint8_t** ciphertext, size_t* ciphertext_length) {
  if (!load_symmetric_functions()) {
    JSRT_Debug("JSRT_Crypto_Symmetric: OpenSSL symmetric functions not available");
    return -1;
  }

  switch (params->algorithm) {
    case JSRT_SYMMETRIC_AES_CBC:
      return jsrt_crypto_aes_cbc_encrypt(params, plaintext, plaintext_length, ciphertext, ciphertext_length);
    case JSRT_SYMMETRIC_AES_GCM:
      return jsrt_crypto_aes_gcm_encrypt(params, plaintext, plaintext_length, ciphertext, ciphertext_length);
    case JSRT_SYMMETRIC_AES_CTR:
      return jsrt_crypto_aes_ctr_encrypt(params, plaintext, plaintext_length, ciphertext, ciphertext_length);
    default:
      JSRT_Debug("JSRT_Crypto_Symmetric: Unsupported algorithm: %d", params->algorithm);
      return -1;
  }
}

// Main decryption function
int jsrt_crypto_aes_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                            uint8_t** plaintext, size_t* plaintext_length) {
  if (!load_symmetric_functions()) {
    JSRT_Debug("JSRT_Crypto_Symmetric: OpenSSL symmetric functions not available");
    return -1;
  }

  switch (params->algorithm) {
    case JSRT_SYMMETRIC_AES_CBC:
      return jsrt_crypto_aes_cbc_decrypt(params, ciphertext, ciphertext_length, plaintext, plaintext_length);
    case JSRT_SYMMETRIC_AES_GCM:
      return jsrt_crypto_aes_gcm_decrypt(params, ciphertext, ciphertext_length, plaintext, plaintext_length);
    case JSRT_SYMMETRIC_AES_CTR:
      return jsrt_crypto_aes_ctr_decrypt(params, ciphertext, ciphertext_length, plaintext, plaintext_length);
    default:
      JSRT_Debug("JSRT_Crypto_Symmetric: Unsupported algorithm: %d", params->algorithm);
      return -1;
  }
}

// Helper functions
jsrt_symmetric_algorithm_t jsrt_crypto_parse_symmetric_algorithm(const char* algorithm_name) {
  if (strcmp(algorithm_name, "AES-CBC") == 0) {
    return JSRT_SYMMETRIC_AES_CBC;
  } else if (strcmp(algorithm_name, "AES-GCM") == 0) {
    return JSRT_SYMMETRIC_AES_GCM;
  } else if (strcmp(algorithm_name, "AES-CTR") == 0) {
    return JSRT_SYMMETRIC_AES_CTR;
  }
  return JSRT_SYMMETRIC_AES_CBC;  // Invalid, but need to return something
}

const char* jsrt_crypto_symmetric_algorithm_to_string(jsrt_symmetric_algorithm_t alg) {
  switch (alg) {
    case JSRT_SYMMETRIC_AES_CBC:
      return "AES-CBC";
    case JSRT_SYMMETRIC_AES_GCM:
      return "AES-GCM";
    case JSRT_SYMMETRIC_AES_CTR:
      return "AES-CTR";
    default:
      return "Unknown";
  }
}

bool jsrt_crypto_is_symmetric_algorithm_supported(jsrt_symmetric_algorithm_t alg) {
  switch (alg) {
    case JSRT_SYMMETRIC_AES_CBC:
      return true;  // Implemented
    case JSRT_SYMMETRIC_AES_GCM:
      return true;  // Implemented
    case JSRT_SYMMETRIC_AES_CTR:
      return true;  // Implemented
    default:
      return false;
  }
}

size_t jsrt_crypto_get_aes_key_size(jsrt_crypto_algorithm_t alg, int key_length_bits) {
  switch (key_length_bits) {
    case 128:
      return JSRT_AES_128_KEY_SIZE;
    case 192:
      return JSRT_AES_192_KEY_SIZE;
    case 256:
      return JSRT_AES_256_KEY_SIZE;
    default:
      return 0;  // Invalid key size
  }
}

void jsrt_crypto_symmetric_params_free(jsrt_symmetric_params_t* params) {
  if (params) {
    free(params->key_data);

    switch (params->algorithm) {
      case JSRT_SYMMETRIC_AES_CBC:
        free(params->params.cbc.iv);
        break;
      case JSRT_SYMMETRIC_AES_GCM:
        free(params->params.gcm.iv);
        free(params->params.gcm.additional_data);
        break;
      case JSRT_SYMMETRIC_AES_CTR:
        free(params->params.ctr.counter);
        break;
    }

    free(params);
  }
}

// Helper function to get OpenSSL symmetric function pointers (for node:crypto)
openssl_symmetric_funcs_t* jsrt_get_openssl_symmetric_funcs(void) {
  if (!load_symmetric_functions()) {
    return NULL;
  }
  return &openssl_symmetric_funcs;
}