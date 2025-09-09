#include "crypto_backend.h"
#include "../util/debug.h"

#ifdef JSRT_STATIC_OPENSSL
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <string.h>

// Static backend initialization
static bool static_backend_init(void) {
  JSRT_Debug("Initializing static OpenSSL crypto backend");
  return true;
}

// Static backend cleanup
static void static_backend_cleanup(void) {
  JSRT_Debug("Cleaning up static OpenSSL crypto backend");
}

// Static digest implementation (reuse from crypto_subtle_static.c)
static int static_digest_data(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len,
                             uint8_t** output, size_t* output_len) {
  const EVP_MD* md = NULL;

  switch (alg) {
    case JSRT_CRYPTO_ALG_SHA1:
      md = EVP_sha1();
      break;
    case JSRT_CRYPTO_ALG_SHA256:
      md = EVP_sha256();
      break;
    case JSRT_CRYPTO_ALG_SHA384:
      md = EVP_sha384();
      break;
    case JSRT_CRYPTO_ALG_SHA512:
      md = EVP_sha512();
      break;
    default:
      return -1;
  }

  if (!md)
    return -1;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx)
    return -1;

  if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
    EVP_MD_CTX_free(ctx);
    return -1;
  }

  if (EVP_DigestUpdate(ctx, input, input_len) != 1) {
    EVP_MD_CTX_free(ctx);
    return -1;
  }

  unsigned int digest_len = EVP_MD_size(md);
  *output = malloc(digest_len);
  if (!*output) {
    EVP_MD_CTX_free(ctx);
    return -1;
  }

  unsigned int final_len;
  if (EVP_DigestFinal_ex(ctx, *output, &final_len) != 1) {
    free(*output);
    *output = NULL;
    EVP_MD_CTX_free(ctx);
    return -1;
  }

  *output_len = final_len;
  EVP_MD_CTX_free(ctx);
  return 0;
}

// Static AES key generation
static int static_generate_aes_key(size_t key_length_bits, uint8_t** key_data, size_t* key_data_length) {
  size_t key_bytes = key_length_bits / 8;
  
  // Validate key length
  if (key_bytes != 16 && key_bytes != 24 && key_bytes != 32) {
    JSRT_Debug("Invalid AES key length: %zu bits", key_length_bits);
    return -1;
  }

  *key_data = malloc(key_bytes);
  if (!*key_data) {
    return -1;
  }

  if (RAND_bytes(*key_data, (int)key_bytes) != 1) {
    free(*key_data);
    *key_data = NULL;
    return -1;
  }

  *key_data_length = key_bytes;
  return 0;
}

// Static AES-CBC encryption
static int static_aes_cbc_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                                 uint8_t** ciphertext, size_t* ciphertext_length) {
  const EVP_CIPHER* cipher = NULL;
  
  // Select cipher based on key length
  switch (params->key_length) {
    case 16:
      cipher = EVP_aes_128_cbc();
      break;
    case 24:
      cipher = EVP_aes_192_cbc();
      break;
    case 32:
      cipher = EVP_aes_256_cbc();
      break;
    default:
      return -1;
  }

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return -1;

  if (EVP_EncryptInit_ex(ctx, cipher, NULL, params->key_data, params->params.cbc.iv) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Calculate maximum output size (plaintext + block size)
  size_t max_ciphertext_len = plaintext_length + EVP_CIPHER_block_size(cipher);
  *ciphertext = malloc(max_ciphertext_len);
  if (!*ciphertext) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  int len;
  int ciphertext_len = 0;

  // Encrypt the plaintext
  if (EVP_EncryptUpdate(ctx, *ciphertext, &len, plaintext, (int)plaintext_length) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  ciphertext_len = len;

  // Finalize the encryption
  if (EVP_EncryptFinal_ex(ctx, *ciphertext + len, &len) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  ciphertext_len += len;

  *ciphertext_length = ciphertext_len;
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}

// Static AES-CBC decryption
static int static_aes_cbc_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                                 uint8_t** plaintext, size_t* plaintext_length) {
  const EVP_CIPHER* cipher = NULL;
  
  // Select cipher based on key length
  switch (params->key_length) {
    case 16:
      cipher = EVP_aes_128_cbc();
      break;
    case 24:
      cipher = EVP_aes_192_cbc();
      break;
    case 32:
      cipher = EVP_aes_256_cbc();
      break;
    default:
      return -1;
  }

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return -1;

  if (EVP_DecryptInit_ex(ctx, cipher, NULL, params->key_data, params->params.cbc.iv) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Allocate plaintext buffer
  *plaintext = malloc(ciphertext_length);
  if (!*plaintext) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  int len;
  int plaintext_len = 0;

  // Decrypt the ciphertext
  if (EVP_DecryptUpdate(ctx, *plaintext, &len, ciphertext, (int)ciphertext_length) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  plaintext_len = len;

  // Finalize the decryption
  if (EVP_DecryptFinal_ex(ctx, *plaintext + len, &len) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  plaintext_len += len;

  *plaintext_length = plaintext_len;
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}

// Static AES-GCM encryption
static int static_aes_gcm_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                                 uint8_t** ciphertext, size_t* ciphertext_length) {
  const EVP_CIPHER* cipher = NULL;
  
  // Select cipher based on key length
  switch (params->key_length) {
    case 16:
      cipher = EVP_aes_128_gcm();
      break;
    case 24:
      cipher = EVP_aes_192_gcm();
      break;
    case 32:
      cipher = EVP_aes_256_gcm();
      break;
    default:
      return -1;
  }

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return -1;

  // Initialize GCM mode
  if (EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Set IV length
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)params->params.gcm.iv_length, NULL) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Initialize key and IV
  if (EVP_EncryptInit_ex(ctx, NULL, NULL, params->key_data, params->params.gcm.iv) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Allocate output buffer (plaintext + tag)
  size_t max_output_len = plaintext_length + params->params.gcm.tag_length;
  *ciphertext = malloc(max_output_len);
  if (!*ciphertext) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  int len;
  int total_len = 0;

  // Process additional authenticated data if present
  if (params->params.gcm.additional_data && params->params.gcm.additional_data_length > 0) {
    if (EVP_EncryptUpdate(ctx, NULL, &len, params->params.gcm.additional_data, (int)params->params.gcm.additional_data_length) != 1) {
      free(*ciphertext);
      *ciphertext = NULL;
      EVP_CIPHER_CTX_free(ctx);
      return -1;
    }
  }

  // Encrypt the plaintext
  if (EVP_EncryptUpdate(ctx, *ciphertext, &len, plaintext, (int)plaintext_length) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  total_len = len;

  // Finalize encryption
  if (EVP_EncryptFinal_ex(ctx, *ciphertext + total_len, &len) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  total_len += len;

  // Get the tag and append it to ciphertext
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, (int)params->params.gcm.tag_length, *ciphertext + total_len) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  total_len += (int)params->params.gcm.tag_length;

  *ciphertext_length = total_len;
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}

// Static AES-GCM decryption
static int static_aes_gcm_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                                 uint8_t** plaintext, size_t* plaintext_length) {
  const EVP_CIPHER* cipher = NULL;
  
  // Select cipher based on key length
  switch (params->key_length) {
    case 16:
      cipher = EVP_aes_128_gcm();
      break;
    case 24:
      cipher = EVP_aes_192_gcm();
      break;
    case 32:
      cipher = EVP_aes_256_gcm();
      break;
    default:
      return -1;
  }

  // Verify ciphertext is long enough to contain tag
  if (ciphertext_length < params->params.gcm.tag_length) {
    return -1;
  }

  size_t actual_ciphertext_length = ciphertext_length - params->params.gcm.tag_length;
  const uint8_t* tag = ciphertext + actual_ciphertext_length;

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return -1;

  // Initialize GCM mode
  if (EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Set IV length
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)params->params.gcm.iv_length, NULL) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Initialize key and IV
  if (EVP_DecryptInit_ex(ctx, NULL, NULL, params->key_data, params->params.gcm.iv) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Allocate output buffer
  *plaintext = malloc(actual_ciphertext_length);
  if (!*plaintext) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  int len;
  int total_len = 0;

  // Process additional authenticated data if present
  if (params->params.gcm.additional_data && params->params.gcm.additional_data_length > 0) {
    if (EVP_DecryptUpdate(ctx, NULL, &len, params->params.gcm.additional_data, (int)params->params.gcm.additional_data_length) != 1) {
      free(*plaintext);
      *plaintext = NULL;
      EVP_CIPHER_CTX_free(ctx);
      return -1;
    }
  }

  // Decrypt the ciphertext
  if (EVP_DecryptUpdate(ctx, *plaintext, &len, ciphertext, (int)actual_ciphertext_length) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  total_len = len;

  // Set expected tag
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)params->params.gcm.tag_length, (void*)tag) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Finalize decryption and verify tag
  if (EVP_DecryptFinal_ex(ctx, *plaintext + total_len, &len) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1; // Tag verification failed
  }
  total_len += len;

  *plaintext_length = total_len;
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}

// Static AES-CTR encryption
static int static_aes_ctr_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                                 uint8_t** ciphertext, size_t* ciphertext_length) {
  const EVP_CIPHER* cipher = NULL;
  
  // Select cipher based on key length
  switch (params->key_length) {
    case 16:
      cipher = EVP_aes_128_ctr();
      break;
    case 24:
      cipher = EVP_aes_192_ctr();
      break;
    case 32:
      cipher = EVP_aes_256_ctr();
      break;
    default:
      return -1;
  }

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return -1;

  // Initialize CTR mode
  if (EVP_EncryptInit_ex(ctx, cipher, NULL, params->key_data, params->params.ctr.counter) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Allocate output buffer (same size as input for CTR mode)
  *ciphertext = malloc(plaintext_length);
  if (!*ciphertext) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  int len;
  int total_len = 0;

  // Encrypt the plaintext
  if (EVP_EncryptUpdate(ctx, *ciphertext, &len, plaintext, (int)plaintext_length) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  total_len = len;

  // Finalize encryption (CTR mode usually doesn't need this, but for completeness)
  if (EVP_EncryptFinal_ex(ctx, *ciphertext + total_len, &len) != 1) {
    free(*ciphertext);
    *ciphertext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  total_len += len;

  *ciphertext_length = total_len;
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}

// Static AES-CTR decryption (same as encryption for CTR mode)
static int static_aes_ctr_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                                 uint8_t** plaintext, size_t* plaintext_length) {
  const EVP_CIPHER* cipher = NULL;
  
  // Select cipher based on key length
  switch (params->key_length) {
    case 16:
      cipher = EVP_aes_128_ctr();
      break;
    case 24:
      cipher = EVP_aes_192_ctr();
      break;
    case 32:
      cipher = EVP_aes_256_ctr();
      break;
    default:
      return -1;
  }

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return -1;

  // Initialize CTR mode
  if (EVP_DecryptInit_ex(ctx, cipher, NULL, params->key_data, params->params.ctr.counter) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  // Allocate output buffer (same size as input for CTR mode)
  *plaintext = malloc(ciphertext_length);
  if (!*plaintext) {
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }

  int len;
  int total_len = 0;

  // Decrypt the ciphertext
  if (EVP_DecryptUpdate(ctx, *plaintext, &len, ciphertext, (int)ciphertext_length) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  total_len = len;

  // Finalize decryption
  if (EVP_DecryptFinal_ex(ctx, *plaintext + total_len, &len) != 1) {
    free(*plaintext);
    *plaintext = NULL;
    EVP_CIPHER_CTX_free(ctx);
    return -1;
  }
  total_len += len;

  *plaintext_length = total_len;
  EVP_CIPHER_CTX_free(ctx);
  return 0;
}

// Main AES encryption dispatcher
static int static_aes_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                             uint8_t** ciphertext, size_t* ciphertext_length) {
  switch (params->algorithm) {
    case JSRT_SYMMETRIC_AES_CBC:
      return static_aes_cbc_encrypt(params, plaintext, plaintext_length, ciphertext, ciphertext_length);
    case JSRT_SYMMETRIC_AES_GCM:
      return static_aes_gcm_encrypt(params, plaintext, plaintext_length, ciphertext, ciphertext_length);
    case JSRT_SYMMETRIC_AES_CTR:
      return static_aes_ctr_encrypt(params, plaintext, plaintext_length, ciphertext, ciphertext_length);
    default:
      return -1;
  }
}

// Main AES decryption dispatcher
static int static_aes_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                             uint8_t** plaintext, size_t* plaintext_length) {
  switch (params->algorithm) {
    case JSRT_SYMMETRIC_AES_CBC:
      return static_aes_cbc_decrypt(params, ciphertext, ciphertext_length, plaintext, plaintext_length);
    case JSRT_SYMMETRIC_AES_GCM:
      return static_aes_gcm_decrypt(params, ciphertext, ciphertext_length, plaintext, plaintext_length);
    case JSRT_SYMMETRIC_AES_CTR:
      return static_aes_ctr_decrypt(params, ciphertext, ciphertext_length, plaintext, plaintext_length);
    default:
      return -1;
  }
}

// Static random number generation
static int static_get_random_bytes(uint8_t* buffer, size_t length) {
  return (RAND_bytes(buffer, (int)length) == 1) ? 0 : -1;
}

// Static UUID generation
static int static_random_uuid(char* uuid_str, size_t uuid_str_size) {
  if (uuid_str_size < 37) {  // UUID string length + null terminator
    return -1;
  }

  unsigned char random_bytes[16];
  if (RAND_bytes(random_bytes, 16) != 1) {
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

// Static version info
static const char* static_get_version(void) {
  return OPENSSL_VERSION_TEXT;
}

// Create static backend
jsrt_crypto_backend_t* jsrt_crypto_backend_create_static(void) {
  jsrt_crypto_backend_t* backend = malloc(sizeof(jsrt_crypto_backend_t));
  if (!backend) {
    return NULL;
  }

  backend->type = JSRT_CRYPTO_BACKEND_STATIC;
  backend->init = static_backend_init;
  backend->cleanup = static_backend_cleanup;
  backend->digest = static_digest_data;
  backend->generate_aes_key = static_generate_aes_key;
  backend->aes_encrypt = static_aes_encrypt;
  backend->aes_decrypt = static_aes_decrypt;
  backend->get_random_bytes = static_get_random_bytes;
  backend->random_uuid = static_random_uuid;
  backend->get_version = static_get_version;

  return backend;
}

#else  // !JSRT_STATIC_OPENSSL

// Fallback for when static OpenSSL is not available
jsrt_crypto_backend_t* jsrt_crypto_backend_create_static(void) {
  JSRT_Debug("Static OpenSSL backend not available (JSRT_STATIC_OPENSSL not defined)");
  return NULL;
}

#endif  // JSRT_STATIC_OPENSSL