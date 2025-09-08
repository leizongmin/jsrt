#include "crypto_digest.h"

#ifndef JSRT_STATIC_OPENSSL
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

// OpenSSL function pointers for digest operations
typedef struct {
  const void* (*EVP_sha1)(void);
  const void* (*EVP_sha256)(void);
  const void* (*EVP_sha384)(void);
  const void* (*EVP_sha512)(void);
  void* (*EVP_MD_CTX_new)(void);
  void (*EVP_MD_CTX_free)(void* ctx);
  int (*EVP_DigestInit_ex)(void* ctx, const void* type, void* impl);
  int (*EVP_DigestUpdate)(void* ctx, const void* d, size_t cnt);
  int (*EVP_DigestFinal_ex)(void* ctx, unsigned char* md, unsigned int* s);
  int (*EVP_MD_size)(const void* md);
} openssl_digest_funcs_t;

static openssl_digest_funcs_t openssl_digest_funcs = {0};
static bool digest_funcs_loaded = false;

// Load OpenSSL digest functions
static bool load_digest_functions(void) {
  if (digest_funcs_loaded) {
    return openssl_digest_funcs.EVP_sha256 != NULL;
  }

  if (!openssl_handle) {
    JSRT_Debug("JSRT_Crypto_Digest: OpenSSL handle not available");
    return false;
  }

  // Load digest algorithm functions
  openssl_digest_funcs.EVP_sha1 = JSRT_DLSYM(openssl_handle, "EVP_sha1");
  openssl_digest_funcs.EVP_sha256 = JSRT_DLSYM(openssl_handle, "EVP_sha256");
  openssl_digest_funcs.EVP_sha384 = JSRT_DLSYM(openssl_handle, "EVP_sha384");
  openssl_digest_funcs.EVP_sha512 = JSRT_DLSYM(openssl_handle, "EVP_sha512");

  // Load context management functions
  openssl_digest_funcs.EVP_MD_CTX_new = JSRT_DLSYM(openssl_handle, "EVP_MD_CTX_new");
  openssl_digest_funcs.EVP_MD_CTX_free = JSRT_DLSYM(openssl_handle, "EVP_MD_CTX_free");

  // Load digest operation functions
  openssl_digest_funcs.EVP_DigestInit_ex = JSRT_DLSYM(openssl_handle, "EVP_DigestInit_ex");
  openssl_digest_funcs.EVP_DigestUpdate = JSRT_DLSYM(openssl_handle, "EVP_DigestUpdate");
  openssl_digest_funcs.EVP_DigestFinal_ex = JSRT_DLSYM(openssl_handle, "EVP_DigestFinal_ex");
  openssl_digest_funcs.EVP_MD_size = JSRT_DLSYM(openssl_handle, "EVP_MD_size");

  digest_funcs_loaded = true;

  // Check if all required functions were loaded (EVP_MD_size is optional, we have fallback)
  bool success = openssl_digest_funcs.EVP_sha256 && openssl_digest_funcs.EVP_MD_CTX_new &&
                 openssl_digest_funcs.EVP_MD_CTX_free && openssl_digest_funcs.EVP_DigestInit_ex &&
                 openssl_digest_funcs.EVP_DigestUpdate && openssl_digest_funcs.EVP_DigestFinal_ex;

  if (success) {
    JSRT_Debug("JSRT_Crypto_Digest: Successfully loaded OpenSSL digest functions");
  } else {
    JSRT_Debug("JSRT_Crypto_Digest: Failed to load some OpenSSL digest functions");
  }

  return success;
}

// Get OpenSSL EVP_MD for algorithm
static const void* get_openssl_md(jsrt_crypto_algorithm_t alg) {
  if (!load_digest_functions()) {
    return NULL;
  }

  switch (alg) {
    case JSRT_CRYPTO_ALG_SHA1:
      return openssl_digest_funcs.EVP_sha1 ? openssl_digest_funcs.EVP_sha1() : NULL;
    case JSRT_CRYPTO_ALG_SHA256:
      return openssl_digest_funcs.EVP_sha256 ? openssl_digest_funcs.EVP_sha256() : NULL;
    case JSRT_CRYPTO_ALG_SHA384:
      return openssl_digest_funcs.EVP_sha384 ? openssl_digest_funcs.EVP_sha384() : NULL;
    case JSRT_CRYPTO_ALG_SHA512:
      return openssl_digest_funcs.EVP_sha512 ? openssl_digest_funcs.EVP_sha512() : NULL;
    default:
      return NULL;
  }
}

// Get digest size for algorithm
static size_t get_digest_size(jsrt_crypto_algorithm_t alg) {
  const void* md = get_openssl_md(alg);
  if (!md || !openssl_digest_funcs.EVP_MD_size) {
    // Fallback to known sizes
    switch (alg) {
      case JSRT_CRYPTO_ALG_SHA1:
        return 20;
      case JSRT_CRYPTO_ALG_SHA256:
        return 32;
      case JSRT_CRYPTO_ALG_SHA384:
        return 48;
      case JSRT_CRYPTO_ALG_SHA512:
        return 64;
      default:
        return 0;
    }
  }

  return openssl_digest_funcs.EVP_MD_size(md);
}

// Perform digest operation (blocking, for worker thread)
int jsrt_crypto_digest_data(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len, uint8_t** output,
                            size_t* output_len) {
  if (!load_digest_functions()) {
    JSRT_Debug("JSRT_Crypto_Digest: OpenSSL digest functions not available");
    return -1;
  }

  const void* md = get_openssl_md(alg);
  if (!md) {
    JSRT_Debug("JSRT_Crypto_Digest: Unsupported algorithm");
    return -1;
  }

  // Create digest context
  void* ctx = openssl_digest_funcs.EVP_MD_CTX_new();
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_Digest: Failed to create digest context");
    return -1;
  }

  // Initialize digest
  if (openssl_digest_funcs.EVP_DigestInit_ex(ctx, md, NULL) != 1) {
    openssl_digest_funcs.EVP_MD_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Digest: Failed to initialize digest");
    return -1;
  }

  // Update with input data
  if (openssl_digest_funcs.EVP_DigestUpdate(ctx, input, input_len) != 1) {
    openssl_digest_funcs.EVP_MD_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Digest: Failed to update digest");
    return -1;
  }

  // Get digest size and allocate output buffer
  size_t digest_size = get_digest_size(alg);
  if (digest_size == 0) {
    openssl_digest_funcs.EVP_MD_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Digest: Invalid digest size");
    return -1;
  }

  *output = malloc(digest_size);
  if (!*output) {
    openssl_digest_funcs.EVP_MD_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Digest: Failed to allocate output buffer");
    return -1;
  }

  // Finalize digest
  unsigned int final_size;
  if (openssl_digest_funcs.EVP_DigestFinal_ex(ctx, *output, &final_size) != 1) {
    free(*output);
    *output = NULL;
    openssl_digest_funcs.EVP_MD_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_Digest: Failed to finalize digest");
    return -1;
  }

  *output_len = final_size;

  // Clean up context
  openssl_digest_funcs.EVP_MD_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_Digest: Successfully computed %s digest (%zu bytes)", jsrt_crypto_algorithm_to_string(alg),
             *output_len);
  return 0;
}

#else
// Static OpenSSL mode - disable digest functions for now
int jsrt_crypto_digest(jsrt_crypto_algorithm_t alg, const unsigned char* input, size_t input_len, unsigned char* output,
                       size_t* output_len) {
  (void)alg;
  (void)input;
  (void)input_len;
  (void)output;
  (void)output_len;
  return -1;  // Not implemented in static mode
}
#endif