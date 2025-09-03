#include "crypto_hmac.h"

// Platform-specific includes for dynamic loading
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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

// OpenSSL function pointers for HMAC
typedef struct {
  // HMAC functions
  unsigned char* (*HMAC)(const void* evp_md, const void* key, int key_len, const unsigned char* d, size_t n,
                         unsigned char* md, unsigned int* md_len);

  // Hash algorithm functions
  const void* (*EVP_sha1)(void);
  const void* (*EVP_sha256)(void);
  const void* (*EVP_sha384)(void);
  const void* (*EVP_sha512)(void);

  // Random number generation (for key generation)
  int (*RAND_bytes)(unsigned char* buf, int num);
} openssl_hmac_funcs_t;

static openssl_hmac_funcs_t openssl_hmac_funcs = {0};
static bool hmac_funcs_loaded = false;

// Load OpenSSL HMAC functions
static bool load_hmac_functions(void) {
  if (hmac_funcs_loaded) {
    return openssl_hmac_funcs.HMAC != NULL;
  }

  if (!openssl_handle) {
    JSRT_Debug("JSRT_Crypto_HMAC: OpenSSL handle not available");
    return false;
  }

  // Load HMAC function
  openssl_hmac_funcs.HMAC = JSRT_DLSYM(openssl_handle, "HMAC");

  // Load hash algorithm functions
  openssl_hmac_funcs.EVP_sha1 = JSRT_DLSYM(openssl_handle, "EVP_sha1");
  openssl_hmac_funcs.EVP_sha256 = JSRT_DLSYM(openssl_handle, "EVP_sha256");
  openssl_hmac_funcs.EVP_sha384 = JSRT_DLSYM(openssl_handle, "EVP_sha384");
  openssl_hmac_funcs.EVP_sha512 = JSRT_DLSYM(openssl_handle, "EVP_sha512");

  // Load random function for key generation
  openssl_hmac_funcs.RAND_bytes = JSRT_DLSYM(openssl_handle, "RAND_bytes");

  hmac_funcs_loaded = true;

  // Check if all required functions were loaded
  bool success = openssl_hmac_funcs.HMAC && openssl_hmac_funcs.EVP_sha1 && openssl_hmac_funcs.EVP_sha256 &&
                 openssl_hmac_funcs.EVP_sha384 && openssl_hmac_funcs.EVP_sha512 && openssl_hmac_funcs.RAND_bytes;

  if (success) {
    JSRT_Debug("JSRT_Crypto_HMAC: Successfully loaded OpenSSL HMAC functions");
  } else {
    JSRT_Debug("JSRT_Crypto_HMAC: Failed to load some OpenSSL HMAC functions");
  }

  return success;
}

// Get OpenSSL hash function for HMAC algorithm
static const void* get_openssl_hash_func(jsrt_hmac_algorithm_t alg) {
  if (!load_hmac_functions()) {
    return NULL;
  }

  switch (alg) {
    case JSRT_HMAC_SHA1:
      return openssl_hmac_funcs.EVP_sha1 ? openssl_hmac_funcs.EVP_sha1() : NULL;
    case JSRT_HMAC_SHA256:
      return openssl_hmac_funcs.EVP_sha256 ? openssl_hmac_funcs.EVP_sha256() : NULL;
    case JSRT_HMAC_SHA384:
      return openssl_hmac_funcs.EVP_sha384 ? openssl_hmac_funcs.EVP_sha384() : NULL;
    case JSRT_HMAC_SHA512:
      return openssl_hmac_funcs.EVP_sha512 ? openssl_hmac_funcs.EVP_sha512() : NULL;
    default:
      return NULL;
  }
}

// Generate HMAC key
int jsrt_crypto_generate_hmac_key(jsrt_hmac_algorithm_t alg, uint8_t** key_data, size_t* key_data_length) {
  if (!load_hmac_functions() || !openssl_hmac_funcs.RAND_bytes) {
    JSRT_Debug("JSRT_Crypto_HMAC: OpenSSL functions not available for key generation");
    return -1;
  }

  // Use hash output size as default key length (good security practice)
  size_t key_bytes = jsrt_crypto_get_hmac_hash_size(alg);
  if (key_bytes == 0) {
    JSRT_Debug("JSRT_Crypto_HMAC: Invalid HMAC algorithm: %d", alg);
    return -1;
  }

  *key_data = malloc(key_bytes);
  if (!*key_data) {
    JSRT_Debug("JSRT_Crypto_HMAC: Failed to allocate key buffer");
    return -1;
  }

  if (openssl_hmac_funcs.RAND_bytes(*key_data, key_bytes) != 1) {
    free(*key_data);
    *key_data = NULL;
    JSRT_Debug("JSRT_Crypto_HMAC: Failed to generate random key");
    return -1;
  }

  *key_data_length = key_bytes;
  JSRT_Debug("JSRT_Crypto_HMAC: Successfully generated %zu-byte HMAC key for %s", key_bytes,
             jsrt_crypto_hmac_algorithm_to_string(alg));
  return 0;
}

// HMAC sign (compute MAC)
int jsrt_crypto_hmac_sign(jsrt_hmac_params_t* params, const uint8_t* data, size_t data_length, uint8_t** signature,
                          size_t* signature_length) {
  if (!load_hmac_functions() || !openssl_hmac_funcs.HMAC) {
    JSRT_Debug("JSRT_Crypto_HMAC: OpenSSL HMAC functions not available");
    return -1;
  }

  const void* hash_func = get_openssl_hash_func(params->algorithm);
  if (!hash_func) {
    JSRT_Debug("JSRT_Crypto_HMAC: Unsupported HMAC algorithm: %d", params->algorithm);
    return -1;
  }

  // Allocate signature buffer
  size_t max_sig_len = jsrt_crypto_get_hmac_hash_size(params->algorithm);
  *signature = malloc(max_sig_len);
  if (!*signature) {
    JSRT_Debug("JSRT_Crypto_HMAC: Failed to allocate signature buffer");
    return -1;
  }

  // Compute HMAC
  unsigned int sig_len = 0;
  unsigned char* result =
      openssl_hmac_funcs.HMAC(hash_func, params->key_data, params->key_length, data, data_length, *signature, &sig_len);

  if (!result || sig_len == 0) {
    free(*signature);
    *signature = NULL;
    JSRT_Debug("JSRT_Crypto_HMAC: Failed to compute HMAC signature");
    return -1;
  }

  *signature_length = sig_len;
  JSRT_Debug("JSRT_Crypto_HMAC: Successfully computed HMAC signature (%u bytes) for %zu bytes of data", sig_len,
             data_length);
  return 0;
}

// HMAC verify
bool jsrt_crypto_hmac_verify(jsrt_hmac_params_t* params, const uint8_t* data, size_t data_length,
                             const uint8_t* signature, size_t signature_length) {
  // Compute HMAC for the data
  uint8_t* computed_signature;
  size_t computed_length;

  int result = jsrt_crypto_hmac_sign(params, data, data_length, &computed_signature, &computed_length);
  if (result != 0) {
    JSRT_Debug("JSRT_Crypto_HMAC: Failed to compute HMAC for verification");
    return false;
  }

  // Compare signatures (constant-time comparison)
  bool match = (signature_length == computed_length);
  if (match) {
    // Use constant-time comparison to prevent timing attacks
    volatile unsigned char result_acc = 0;
    for (size_t i = 0; i < computed_length; i++) {
      result_acc |= signature[i] ^ computed_signature[i];
    }
    match = (result_acc == 0);
  }

  free(computed_signature);

  JSRT_Debug("JSRT_Crypto_HMAC: Signature verification %s", match ? "succeeded" : "failed");
  return match;
}

// Helper functions
jsrt_hmac_algorithm_t jsrt_crypto_parse_hmac_algorithm(const char* hash_name) {
  if (strcmp(hash_name, "SHA-1") == 0) {
    return JSRT_HMAC_SHA1;
  } else if (strcmp(hash_name, "SHA-256") == 0) {
    return JSRT_HMAC_SHA256;
  } else if (strcmp(hash_name, "SHA-384") == 0) {
    return JSRT_HMAC_SHA384;
  } else if (strcmp(hash_name, "SHA-512") == 0) {
    return JSRT_HMAC_SHA512;
  }
  return JSRT_HMAC_SHA256;  // Default fallback
}

const char* jsrt_crypto_hmac_algorithm_to_string(jsrt_hmac_algorithm_t alg) {
  switch (alg) {
    case JSRT_HMAC_SHA1:
      return "HMAC-SHA-1";
    case JSRT_HMAC_SHA256:
      return "HMAC-SHA-256";
    case JSRT_HMAC_SHA384:
      return "HMAC-SHA-384";
    case JSRT_HMAC_SHA512:
      return "HMAC-SHA-512";
    default:
      return "HMAC-Unknown";
  }
}

bool jsrt_crypto_is_hmac_algorithm_supported(jsrt_hmac_algorithm_t alg) {
  switch (alg) {
    case JSRT_HMAC_SHA1:
    case JSRT_HMAC_SHA256:
    case JSRT_HMAC_SHA384:
    case JSRT_HMAC_SHA512:
      return true;
    default:
      return false;
  }
}

size_t jsrt_crypto_get_hmac_hash_size(jsrt_hmac_algorithm_t alg) {
  switch (alg) {
    case JSRT_HMAC_SHA1:
      return 20;  // 160 bits
    case JSRT_HMAC_SHA256:
      return 32;  // 256 bits
    case JSRT_HMAC_SHA384:
      return 48;  // 384 bits
    case JSRT_HMAC_SHA512:
      return 64;  // 512 bits
    default:
      return 0;
  }
}

void jsrt_crypto_hmac_params_free(jsrt_hmac_params_t* params) {
  if (params) {
    free(params->key_data);
    free(params);
  }
}