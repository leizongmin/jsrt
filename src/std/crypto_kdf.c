#include "crypto_kdf.h"

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

// OpenSSL function pointers for KDF
typedef struct {
  // PBKDF2 functions
  int (*PKCS5_PBKDF2_HMAC)(const char* pass, int passlen, const unsigned char* salt, int saltlen, int iter,
                           const void* digest, int keylen, unsigned char* out);

  // HKDF functions (OpenSSL 1.1.0+)
  int (*EVP_PKEY_derive_init)(void* ctx);
  int (*EVP_PKEY_derive_set_peer)(void* ctx, void* peer);
  int (*EVP_PKEY_derive)(void* ctx, unsigned char* key, size_t* keylen);
  void* (*EVP_PKEY_CTX_new_id)(int id, void* e);
  void (*EVP_PKEY_CTX_free)(void* ctx);
  int (*EVP_PKEY_CTX_ctrl)(void* ctx, int keytype, int optype, int cmd, int p1, void* p2);

  // Hash functions for KDF
  const void* (*EVP_sha1)(void);
  const void* (*EVP_sha256)(void);
  const void* (*EVP_sha384)(void);
  const void* (*EVP_sha512)(void);
} openssl_kdf_funcs_t;

static openssl_kdf_funcs_t openssl_kdf_funcs = {0};
static bool kdf_funcs_loaded = false;

// Load OpenSSL KDF functions
static bool load_kdf_functions(void) {
  if (kdf_funcs_loaded) {
    return openssl_kdf_funcs.PKCS5_PBKDF2_HMAC != NULL;
  }

  if (!openssl_handle) {
    JSRT_Debug("JSRT_Crypto_KDF: OpenSSL handle not available");
    return false;
  }

  // Load PBKDF2 functions
  openssl_kdf_funcs.PKCS5_PBKDF2_HMAC = JSRT_DLSYM(openssl_handle, "PKCS5_PBKDF2_HMAC");

  // Load hash functions
  openssl_kdf_funcs.EVP_sha1 = JSRT_DLSYM(openssl_handle, "EVP_sha1");
  openssl_kdf_funcs.EVP_sha256 = JSRT_DLSYM(openssl_handle, "EVP_sha256");
  openssl_kdf_funcs.EVP_sha384 = JSRT_DLSYM(openssl_handle, "EVP_sha384");
  openssl_kdf_funcs.EVP_sha512 = JSRT_DLSYM(openssl_handle, "EVP_sha512");

  // Load HKDF functions (optional, may not be available in older OpenSSL)
  openssl_kdf_funcs.EVP_PKEY_CTX_new_id = JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_new_id");
  openssl_kdf_funcs.EVP_PKEY_CTX_free = JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_free");
  openssl_kdf_funcs.EVP_PKEY_derive_init = JSRT_DLSYM(openssl_handle, "EVP_PKEY_derive_init");
  openssl_kdf_funcs.EVP_PKEY_derive = JSRT_DLSYM(openssl_handle, "EVP_PKEY_derive");
  openssl_kdf_funcs.EVP_PKEY_CTX_ctrl = JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_ctrl");

  kdf_funcs_loaded = true;

  JSRT_Debug("JSRT_Crypto_KDF: OpenSSL KDF functions loaded - PBKDF2: %s, HKDF: %s",
             openssl_kdf_funcs.PKCS5_PBKDF2_HMAC ? "available" : "unavailable",
             openssl_kdf_funcs.EVP_PKEY_CTX_new_id ? "available" : "unavailable");

  return openssl_kdf_funcs.PKCS5_PBKDF2_HMAC != NULL;
}

// Get OpenSSL hash function for algorithm
static const void* get_openssl_hash_function(jsrt_crypto_algorithm_t alg) {
  if (!load_kdf_functions()) {
    return NULL;
  }

  switch (alg) {
    case JSRT_CRYPTO_ALG_SHA1:
      return openssl_kdf_funcs.EVP_sha1 ? openssl_kdf_funcs.EVP_sha1() : NULL;
    case JSRT_CRYPTO_ALG_SHA256:
      return openssl_kdf_funcs.EVP_sha256 ? openssl_kdf_funcs.EVP_sha256() : NULL;
    case JSRT_CRYPTO_ALG_SHA384:
      return openssl_kdf_funcs.EVP_sha384 ? openssl_kdf_funcs.EVP_sha384() : NULL;
    case JSRT_CRYPTO_ALG_SHA512:
      return openssl_kdf_funcs.EVP_sha512 ? openssl_kdf_funcs.EVP_sha512() : NULL;
    default:
      return NULL;
  }
}

// PBKDF2 key derivation
int jsrt_crypto_pbkdf2_derive_key(jsrt_pbkdf2_params_t* params, const uint8_t* password, size_t password_length,
                                  size_t key_length, uint8_t** derived_key) {
  if (!load_kdf_functions() || !openssl_kdf_funcs.PKCS5_PBKDF2_HMAC) {
    JSRT_Debug("JSRT_Crypto_KDF: PBKDF2 functions not available");
    return -1;
  }

  // Get hash function
  const void* hash_func = get_openssl_hash_function(params->hash_algorithm);
  if (!hash_func) {
    JSRT_Debug("JSRT_Crypto_KDF: Unsupported hash algorithm for PBKDF2: %d", params->hash_algorithm);
    return -1;
  }

  // Allocate output buffer
  *derived_key = malloc(key_length);
  if (!*derived_key) {
    JSRT_Debug("JSRT_Crypto_KDF: Failed to allocate derived key buffer");
    return -1;
  }

  // Perform PBKDF2
  int result =
      openssl_kdf_funcs.PKCS5_PBKDF2_HMAC((const char*)password, password_length, params->salt, params->salt_length,
                                          params->iterations, hash_func, key_length, *derived_key);

  if (result != 1) {
    free(*derived_key);
    *derived_key = NULL;
    JSRT_Debug("JSRT_Crypto_KDF: PBKDF2 derivation failed");
    return -1;
  }

  JSRT_Debug("JSRT_Crypto_KDF: Successfully derived %zu bytes using PBKDF2 with %u iterations", key_length,
             params->iterations);
  return 0;
}

// HKDF key derivation (simplified implementation for now)
int jsrt_crypto_hkdf_derive_key(jsrt_hkdf_params_t* params, const uint8_t* input_key_material,
                                size_t input_key_material_length, size_t key_length, uint8_t** derived_key) {
  // For now, return not implemented since HKDF is more complex
  // and requires OpenSSL 1.1.1+ with specific EVP_PKEY_HKDF support
  JSRT_Debug("JSRT_Crypto_KDF: HKDF not yet implemented");
  return -1;
}

// Helper functions
jsrt_kdf_algorithm_t jsrt_crypto_parse_kdf_algorithm(const char* algorithm_name) {
  if (strcmp(algorithm_name, "PBKDF2") == 0) {
    return JSRT_KDF_PBKDF2;
  } else if (strcmp(algorithm_name, "HKDF") == 0) {
    return JSRT_KDF_HKDF;
  } else {
    return (jsrt_kdf_algorithm_t)-1;  // Invalid
  }
}

const char* jsrt_crypto_kdf_algorithm_to_string(jsrt_kdf_algorithm_t alg) {
  switch (alg) {
    case JSRT_KDF_PBKDF2:
      return "PBKDF2";
    case JSRT_KDF_HKDF:
      return "HKDF";
    default:
      return "Unknown";
  }
}

bool jsrt_crypto_is_kdf_algorithm_supported(jsrt_kdf_algorithm_t alg) {
  switch (alg) {
    case JSRT_KDF_PBKDF2:
      return load_kdf_functions() && openssl_kdf_funcs.PKCS5_PBKDF2_HMAC != NULL;
    case JSRT_KDF_HKDF:
      return false;  // Not yet implemented
    default:
      return false;
  }
}

void jsrt_crypto_kdf_params_free(jsrt_kdf_params_t* params) {
  if (params) {
    switch (params->algorithm) {
      case JSRT_KDF_PBKDF2:
        free(params->params.pbkdf2.salt);
        break;
      case JSRT_KDF_HKDF:
        free(params->params.hkdf.salt);
        free(params->params.hkdf.info);
        break;
    }
    free(params);
  }
}