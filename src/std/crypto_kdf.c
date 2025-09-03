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

  // HKDF functions (OpenSSL 1.1.1+)
  int (*EVP_PKEY_derive_init)(void* ctx);
  int (*EVP_PKEY_derive_set_peer)(void* ctx, void* peer);
  int (*EVP_PKEY_derive)(void* ctx, unsigned char* key, size_t* keylen);
  void* (*EVP_PKEY_CTX_new_id)(int id, void* e);
  void (*EVP_PKEY_CTX_free)(void* ctx);
  int (*EVP_PKEY_CTX_ctrl)(void* ctx, int keytype, int optype, int cmd, int p1, void* p2);
  int (*EVP_PKEY_CTX_ctrl_str)(void* ctx, const char* type, const char* value);

  // Hash functions for KDF
  const void* (*EVP_sha1)(void);
  const void* (*EVP_sha256)(void);
  const void* (*EVP_sha384)(void);
  const void* (*EVP_sha512)(void);
} openssl_kdf_funcs_t;

static openssl_kdf_funcs_t openssl_kdf_funcs = {0};
static bool kdf_funcs_loaded = false;

// OpenSSL HKDF constants (verified for OpenSSL 3.x)
#define EVP_PKEY_HKDF 1036
#define EVP_PKEY_OP_DERIVE 2048
#define EVP_PKEY_CTRL_HKDF_MD 4099
#define EVP_PKEY_CTRL_HKDF_SALT 4100
#define EVP_PKEY_CTRL_HKDF_KEY 4101
#define EVP_PKEY_CTRL_HKDF_INFO 4102

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
  openssl_kdf_funcs.EVP_PKEY_CTX_ctrl_str = JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_ctrl_str");

  kdf_funcs_loaded = true;

  bool hkdf_available = openssl_kdf_funcs.EVP_PKEY_CTX_new_id && openssl_kdf_funcs.EVP_PKEY_derive_init &&
                        openssl_kdf_funcs.EVP_PKEY_derive && openssl_kdf_funcs.EVP_PKEY_CTX_ctrl;

  JSRT_Debug("JSRT_Crypto_KDF: OpenSSL KDF functions loaded - PBKDF2: %s, HKDF: %s",
             openssl_kdf_funcs.PKCS5_PBKDF2_HMAC ? "available" : "unavailable",
             hkdf_available ? "available" : "unavailable");

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

// HKDF key derivation
int jsrt_crypto_hkdf_derive_key(jsrt_hkdf_params_t* params, const uint8_t* input_key_material,
                                size_t input_key_material_length, size_t key_length, uint8_t** derived_key) {
  JSRT_Debug("JSRT_Crypto_KDF: Starting HKDF derivation with %zu bytes input, %zu bytes output",
             input_key_material_length, key_length);

  if (!load_kdf_functions()) {
    JSRT_Debug("JSRT_Crypto_KDF: OpenSSL functions not available for HKDF");
    return -1;
  }

  // Check if all required HKDF functions are available
  if (!openssl_kdf_funcs.EVP_PKEY_CTX_new_id || !openssl_kdf_funcs.EVP_PKEY_derive_init ||
      !openssl_kdf_funcs.EVP_PKEY_derive || !openssl_kdf_funcs.EVP_PKEY_CTX_ctrl) {
    JSRT_Debug("JSRT_Crypto_KDF: HKDF functions not available in OpenSSL");
    return -1;
  }

  // Get hash function
  const void* hash_func = get_openssl_hash_function(params->hash_algorithm);
  if (!hash_func) {
    JSRT_Debug("JSRT_Crypto_KDF: Unsupported hash algorithm for HKDF: %d", params->hash_algorithm);
    return -1;
  }

  // Create HKDF context
  void* ctx = openssl_kdf_funcs.EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_KDF: Failed to create HKDF context");
    return -1;
  }

  // Initialize derivation
  if (openssl_kdf_funcs.EVP_PKEY_derive_init(ctx) <= 0) {
    openssl_kdf_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_KDF: Failed to initialize HKDF derivation");
    return -1;
  }

  // Set hash function - use string-based method for OpenSSL 3.x compatibility
  const char* hash_name = NULL;
  switch (params->hash_algorithm) {
    case JSRT_CRYPTO_ALG_SHA1:
      hash_name = "SHA1";
      break;
    case JSRT_CRYPTO_ALG_SHA256:
      hash_name = "SHA256";
      break;
    case JSRT_CRYPTO_ALG_SHA384:
      hash_name = "SHA384";
      break;
    case JSRT_CRYPTO_ALG_SHA512:
      hash_name = "SHA512";
      break;
    default:
      hash_name = "SHA256";
      break;
  }

  int hash_result = -1;

  // Try string-based control first (OpenSSL 3.x preferred method)
  if (openssl_kdf_funcs.EVP_PKEY_CTX_ctrl_str) {
    hash_result = openssl_kdf_funcs.EVP_PKEY_CTX_ctrl_str(ctx, "digest", hash_name);
    JSRT_Debug("JSRT_Crypto_KDF: String-based hash setting result: %d", hash_result);
  }

  // Fallback to numeric method if string method failed
  if (hash_result <= 0) {
    JSRT_Debug("JSRT_Crypto_KDF: String method failed, trying numeric method");
    if (openssl_kdf_funcs.EVP_PKEY_CTX_ctrl) {
      hash_result = openssl_kdf_funcs.EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_HKDF, EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_HKDF_MD,
                                                        0, (void*)hash_func);
      JSRT_Debug("JSRT_Crypto_KDF: Numeric hash setting result: %d", hash_result);
    }
  }

  if (hash_result <= 0) {
    openssl_kdf_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_KDF: Failed to set HKDF hash function (both methods failed)");
    return -1;
  }

  // Set input key material
  if (openssl_kdf_funcs.EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_HKDF, EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_HKDF_KEY,
                                          input_key_material_length, (void*)input_key_material) <= 0) {
    openssl_kdf_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_KDF: Failed to set HKDF input key material");
    return -1;
  }

  // Set salt (optional)
  if (params->salt && params->salt_length > 0) {
    if (openssl_kdf_funcs.EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_HKDF, EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_HKDF_SALT,
                                            params->salt_length, params->salt) <= 0) {
      openssl_kdf_funcs.EVP_PKEY_CTX_free(ctx);
      JSRT_Debug("JSRT_Crypto_KDF: Failed to set HKDF salt");
      return -1;
    }
    JSRT_Debug("JSRT_Crypto_KDF: Set HKDF salt (%zu bytes)", params->salt_length);
  }

  // Set info parameter (optional)
  if (params->info && params->info_length > 0) {
    if (openssl_kdf_funcs.EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_HKDF, EVP_PKEY_OP_DERIVE, EVP_PKEY_CTRL_HKDF_INFO,
                                            params->info_length, params->info) <= 0) {
      openssl_kdf_funcs.EVP_PKEY_CTX_free(ctx);
      JSRT_Debug("JSRT_Crypto_KDF: Failed to set HKDF info parameter");
      return -1;
    }
    JSRT_Debug("JSRT_Crypto_KDF: Set HKDF info (%zu bytes)", params->info_length);
  }

  // Allocate output buffer
  *derived_key = malloc(key_length);
  if (!*derived_key) {
    openssl_kdf_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_KDF: Failed to allocate HKDF output buffer");
    return -1;
  }

  // Derive key
  size_t derived_length = key_length;
  if (openssl_kdf_funcs.EVP_PKEY_derive(ctx, *derived_key, &derived_length) <= 0 || derived_length != key_length) {
    free(*derived_key);
    *derived_key = NULL;
    openssl_kdf_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_KDF: HKDF key derivation failed");
    return -1;
  }

  openssl_kdf_funcs.EVP_PKEY_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_KDF: Successfully derived %zu bytes using HKDF", key_length);
  return 0;
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
      return load_kdf_functions() && openssl_kdf_funcs.EVP_PKEY_CTX_new_id && openssl_kdf_funcs.EVP_PKEY_derive_init &&
             openssl_kdf_funcs.EVP_PKEY_derive && openssl_kdf_funcs.EVP_PKEY_CTX_ctrl;
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