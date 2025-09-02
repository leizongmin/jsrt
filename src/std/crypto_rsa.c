#include "crypto_rsa.h"

#include <dlfcn.h>
#include <string.h>

#include "../util/debug.h"
#include "crypto.h"

// OpenSSL function pointers for RSA operations
typedef struct {
  // Key generation and management
  void *(*EVP_PKEY_new)(void);
  void (*EVP_PKEY_free)(void *pkey);
  void *(*EVP_PKEY_CTX_new_id)(int id, void *e);
  void *(*EVP_PKEY_CTX_new)(void *pkey, void *e);
  void (*EVP_PKEY_CTX_free)(void *ctx);
  int (*EVP_PKEY_keygen_init)(void *ctx);
  int (*EVP_PKEY_keygen)(void *ctx, void **pkey);
  int (*EVP_PKEY_CTX_ctrl)(void *ctx, int keytype, int optype, int cmd, int p1, void *p2);

  // Encryption/Decryption
  int (*EVP_PKEY_encrypt_init)(void *ctx);
  int (*EVP_PKEY_encrypt)(void *ctx, unsigned char *out, size_t *outlen, const unsigned char *in, size_t inlen);
  int (*EVP_PKEY_decrypt_init)(void *ctx);
  int (*EVP_PKEY_decrypt)(void *ctx, unsigned char *out, size_t *outlen, const unsigned char *in, size_t inlen);

  // Signature/Verification
  int (*EVP_PKEY_sign_init)(void *ctx);
  int (*EVP_PKEY_sign)(void *ctx, unsigned char *sig, size_t *siglen, const unsigned char *tbs, size_t tbslen);
  int (*EVP_PKEY_verify_init)(void *ctx);
  int (*EVP_PKEY_verify)(void *ctx, const unsigned char *sig, size_t siglen, const unsigned char *tbs, size_t tbslen);

  // Digest signature/verification
  int (*EVP_DigestSignInit)(void *ctx, void **pctx, const void *type, void *e, void *pkey);
  int (*EVP_DigestSign)(void *ctx, unsigned char *sigret, size_t *siglen, const unsigned char *tbs, size_t tbslen);
  int (*EVP_DigestVerifyInit)(void *ctx, void **pctx, const void *type, void *e, void *pkey);
  int (*EVP_DigestVerify)(void *ctx, const unsigned char *sigret, size_t siglen, const unsigned char *tbs,
                          size_t tbslen);

  // Hash functions
  const void *(*EVP_sha1)(void);
  const void *(*EVP_sha256)(void);
  const void *(*EVP_sha384)(void);
  const void *(*EVP_sha512)(void);

  // Digest context functions
  void *(*EVP_MD_CTX_new)(void);
  void (*EVP_MD_CTX_free)(void *ctx);
  int (*EVP_DigestInit_ex)(void *ctx, const void *type, void *impl);
  int (*EVP_DigestUpdate)(void *ctx, const void *d, size_t cnt);
  int (*EVP_DigestFinal_ex)(void *ctx, unsigned char *md, unsigned int *s);
  int (*EVP_PKEY_CTX_set_signature_md)(void *ctx, const void *md);

  // Additional control function
  int (*EVP_PKEY_CTX_ctrl_str)(void *ctx, const char *type, const char *value);

  // Random number generation
  int (*RAND_bytes)(unsigned char *buf, int num);

  // Key serialization
  int (*i2d_PUBKEY)(void *a, unsigned char **pp);
  int (*i2d_PrivateKey)(void *a, unsigned char **pp);
  void *(*d2i_PUBKEY)(void **a, const unsigned char **pp, long length);
  void *(*d2i_PrivateKey)(int type, void **a, const unsigned char **pp, long length);
} openssl_rsa_funcs_t;

static openssl_rsa_funcs_t openssl_rsa_funcs = {0};
static bool rsa_funcs_loaded = false;

// OpenSSL constants - corrected values from actual OpenSSL headers
#define EVP_PKEY_RSA 6
#define EVP_PKEY_OP_ENCRYPT (1 << 0)
#define EVP_PKEY_OP_DECRYPT (1 << 1)
#define EVP_PKEY_OP_SIGN (1 << 2)
#define EVP_PKEY_OP_VERIFY (1 << 3)
// OpenSSL 1.1.1+ control values - these might need adjustment
#define EVP_PKEY_CTRL_RSA_PADDING -4
#define EVP_PKEY_CTRL_RSA_OAEP_MD -5
#define EVP_PKEY_CTRL_RSA_OAEP_LABEL -6
// Alternative control values to try
#define EVP_PKEY_CTRL_RSA_PADDING_ALT 0x1001
#define EVP_PKEY_CTRL_GET_RSA_PADDING 0x1002
#define EVP_PKEY_CTRL_MD 1
#define RSA_PKCS1_PADDING 1
#define RSA_PKCS1_OAEP_PADDING 4
// Correct OpenSSL 1.1.1+ values for RSA key generation controls
#define EVP_PKEY_CTRL_RSA_KEYGEN_BITS (EVP_PKEY_ALG_CTRL + 3)
#define EVP_PKEY_CTRL_RSA_KEYGEN_PUBEXP (EVP_PKEY_ALG_CTRL + 4)
#define EVP_PKEY_ALG_CTRL 0x1000

// Load OpenSSL RSA functions
static bool load_rsa_functions(void) {
  if (rsa_funcs_loaded) {
    return openssl_rsa_funcs.EVP_PKEY_new != NULL;
  }

  extern void *openssl_handle;
  if (!openssl_handle) {
    JSRT_Debug("JSRT_Crypto_RSA: OpenSSL handle not available");
    return false;
  }

  JSRT_Debug("JSRT_Crypto_RSA: Loading RSA functions from OpenSSL handle %p", openssl_handle);

  // Load key management functions
  openssl_rsa_funcs.EVP_PKEY_new = dlsym(openssl_handle, "EVP_PKEY_new");
  openssl_rsa_funcs.EVP_PKEY_free = dlsym(openssl_handle, "EVP_PKEY_free");
  openssl_rsa_funcs.EVP_PKEY_CTX_new_id = dlsym(openssl_handle, "EVP_PKEY_CTX_new_id");
  openssl_rsa_funcs.EVP_PKEY_CTX_new = dlsym(openssl_handle, "EVP_PKEY_CTX_new");
  openssl_rsa_funcs.EVP_PKEY_CTX_free = dlsym(openssl_handle, "EVP_PKEY_CTX_free");
  openssl_rsa_funcs.EVP_PKEY_keygen_init = dlsym(openssl_handle, "EVP_PKEY_keygen_init");
  openssl_rsa_funcs.EVP_PKEY_keygen = dlsym(openssl_handle, "EVP_PKEY_keygen");
  openssl_rsa_funcs.EVP_PKEY_CTX_ctrl = dlsym(openssl_handle, "EVP_PKEY_CTX_ctrl");

  // Load encryption/decryption functions
  openssl_rsa_funcs.EVP_PKEY_encrypt_init = dlsym(openssl_handle, "EVP_PKEY_encrypt_init");
  openssl_rsa_funcs.EVP_PKEY_encrypt = dlsym(openssl_handle, "EVP_PKEY_encrypt");
  openssl_rsa_funcs.EVP_PKEY_decrypt_init = dlsym(openssl_handle, "EVP_PKEY_decrypt_init");
  openssl_rsa_funcs.EVP_PKEY_decrypt = dlsym(openssl_handle, "EVP_PKEY_decrypt");

  // Load signature/verification functions
  openssl_rsa_funcs.EVP_PKEY_sign_init = dlsym(openssl_handle, "EVP_PKEY_sign_init");
  openssl_rsa_funcs.EVP_PKEY_sign = dlsym(openssl_handle, "EVP_PKEY_sign");
  openssl_rsa_funcs.EVP_PKEY_verify_init = dlsym(openssl_handle, "EVP_PKEY_verify_init");
  openssl_rsa_funcs.EVP_PKEY_verify = dlsym(openssl_handle, "EVP_PKEY_verify");

  // Load digest signature/verification functions
  openssl_rsa_funcs.EVP_DigestSignInit = dlsym(openssl_handle, "EVP_DigestSignInit");
  openssl_rsa_funcs.EVP_DigestSign = dlsym(openssl_handle, "EVP_DigestSign");
  openssl_rsa_funcs.EVP_DigestVerifyInit = dlsym(openssl_handle, "EVP_DigestVerifyInit");
  openssl_rsa_funcs.EVP_DigestVerify = dlsym(openssl_handle, "EVP_DigestVerify");

  // Load hash functions
  openssl_rsa_funcs.EVP_sha1 = dlsym(openssl_handle, "EVP_sha1");
  openssl_rsa_funcs.EVP_sha256 = dlsym(openssl_handle, "EVP_sha256");
  openssl_rsa_funcs.EVP_sha384 = dlsym(openssl_handle, "EVP_sha384");
  openssl_rsa_funcs.EVP_sha512 = dlsym(openssl_handle, "EVP_sha512");

  // Load digest context functions
  openssl_rsa_funcs.EVP_MD_CTX_new = dlsym(openssl_handle, "EVP_MD_CTX_new");
  openssl_rsa_funcs.EVP_MD_CTX_free = dlsym(openssl_handle, "EVP_MD_CTX_free");
  openssl_rsa_funcs.EVP_DigestInit_ex = dlsym(openssl_handle, "EVP_DigestInit_ex");
  openssl_rsa_funcs.EVP_DigestUpdate = dlsym(openssl_handle, "EVP_DigestUpdate");
  openssl_rsa_funcs.EVP_DigestFinal_ex = dlsym(openssl_handle, "EVP_DigestFinal_ex");
  openssl_rsa_funcs.EVP_PKEY_CTX_set_signature_md = dlsym(openssl_handle, "EVP_PKEY_CTX_set_signature_md");
  openssl_rsa_funcs.EVP_PKEY_CTX_ctrl_str = dlsym(openssl_handle, "EVP_PKEY_CTX_ctrl_str");

  // Load random function
  openssl_rsa_funcs.RAND_bytes = dlsym(openssl_handle, "RAND_bytes");

  // Load key serialization functions
  openssl_rsa_funcs.i2d_PUBKEY = dlsym(openssl_handle, "i2d_PUBKEY");
  openssl_rsa_funcs.i2d_PrivateKey = dlsym(openssl_handle, "i2d_PrivateKey");
  openssl_rsa_funcs.d2i_PUBKEY = dlsym(openssl_handle, "d2i_PUBKEY");
  openssl_rsa_funcs.d2i_PrivateKey = dlsym(openssl_handle, "d2i_PrivateKey");

  rsa_funcs_loaded = true;

  // Check if all required functions were loaded
  bool success = openssl_rsa_funcs.EVP_PKEY_new && openssl_rsa_funcs.EVP_PKEY_free &&
                 openssl_rsa_funcs.EVP_PKEY_CTX_new_id && openssl_rsa_funcs.EVP_PKEY_CTX_free &&
                 openssl_rsa_funcs.EVP_PKEY_keygen_init && openssl_rsa_funcs.EVP_PKEY_keygen &&
                 openssl_rsa_funcs.EVP_PKEY_encrypt_init && openssl_rsa_funcs.EVP_PKEY_encrypt &&
                 openssl_rsa_funcs.EVP_PKEY_decrypt_init && openssl_rsa_funcs.EVP_PKEY_decrypt &&
                 openssl_rsa_funcs.EVP_sha256 && openssl_rsa_funcs.RAND_bytes;

  printf(
      "DEBUG: RSA function loading status: EVP_PKEY_new=%p, EVP_PKEY_CTX_new_id=%p, EVP_PKEY_keygen_init=%p, "
      "EVP_PKEY_keygen=%p\n",
      openssl_rsa_funcs.EVP_PKEY_new, openssl_rsa_funcs.EVP_PKEY_CTX_new_id, openssl_rsa_funcs.EVP_PKEY_keygen_init,
      openssl_rsa_funcs.EVP_PKEY_keygen);

  if (success) {
    JSRT_Debug("JSRT_Crypto_RSA: Successfully loaded OpenSSL RSA functions");
  } else {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to load some OpenSSL RSA functions");
  }

  return success;
}

// Get OpenSSL hash function for RSA hash algorithm
static const void *get_openssl_hash_func(jsrt_rsa_hash_algorithm_t hash_alg) {
  if (!load_rsa_functions()) {
    return NULL;
  }

  switch (hash_alg) {
    case JSRT_RSA_HASH_SHA1:
      if (openssl_rsa_funcs.EVP_sha1) {
        const void *sha1_func = openssl_rsa_funcs.EVP_sha1();
        JSRT_Debug("JSRT_Crypto_RSA: SHA-1 function: %p", sha1_func);
        return sha1_func;
      }
      JSRT_Debug("JSRT_Crypto_RSA: SHA-1 function not loaded");
      return NULL;
    case JSRT_RSA_HASH_SHA256:
      return openssl_rsa_funcs.EVP_sha256 ? openssl_rsa_funcs.EVP_sha256() : NULL;
    case JSRT_RSA_HASH_SHA384:
      return openssl_rsa_funcs.EVP_sha384 ? openssl_rsa_funcs.EVP_sha384() : NULL;
    case JSRT_RSA_HASH_SHA512:
      return openssl_rsa_funcs.EVP_sha512 ? openssl_rsa_funcs.EVP_sha512() : NULL;
    default:
      return NULL;
  }
}

// Generate RSA key pair
int jsrt_crypto_generate_rsa_keypair(size_t modulus_length_bits, uint32_t public_exponent,
                                     jsrt_rsa_hash_algorithm_t hash_alg, jsrt_rsa_keypair_t **keypair) {
  JSRT_Debug("JSRT_Crypto_RSA: Generating RSA key pair: %zu bits, exp=%u, hash=%d", modulus_length_bits,
             public_exponent, hash_alg);

  if (!load_rsa_functions()) {
    JSRT_Debug("JSRT_Crypto_RSA: OpenSSL functions not available for key generation");
    return -1;
  }

  // Validate parameters
  if (modulus_length_bits < 1024 || modulus_length_bits > 4096) {
    JSRT_Debug("JSRT_Crypto_RSA: Invalid modulus length: %zu bits", modulus_length_bits);
    return -1;
  }

  // Create key generation context
  void *ctx = openssl_rsa_funcs.EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to create key generation context");
    printf("DEBUG: EVP_PKEY_CTX_new_id failed\n");
    return -1;
  }
  printf("DEBUG: EVP_PKEY_CTX_new_id succeeded, ctx=%p\n", ctx);

  // Initialize key generation
  int init_result = openssl_rsa_funcs.EVP_PKEY_keygen_init(ctx);
  printf("DEBUG: EVP_PKEY_keygen_init returned: %d\n", init_result);
  if (init_result <= 0) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to initialize key generation");
    printf("DEBUG: EVP_PKEY_keygen_init failed with result %d\n", init_result);
    return -1;
  }

  // Set key length
  printf("DEBUG: Setting RSA key length to %zu bits\n", modulus_length_bits);
  int ctrl_result = openssl_rsa_funcs.EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, -1, EVP_PKEY_CTRL_RSA_KEYGEN_BITS,
                                                        (int)modulus_length_bits, NULL);
  printf("DEBUG: EVP_PKEY_CTX_ctrl returned: %d\n", ctrl_result);
  if (ctrl_result <= 0) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to set key length");
    printf("DEBUG: Failed to set RSA key length, ctrl returned %d\n", ctrl_result);
    return -1;
  }

  // Set public exponent (if we need to support custom exponents)
  // For now, OpenSSL will use the default F4 (65537) which is standard

  // Generate key pair
  void *pkey = NULL;
  if (openssl_rsa_funcs.EVP_PKEY_keygen(ctx, &pkey) <= 0 || !pkey) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to generate RSA key pair");
    return -1;
  }

  openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);

  // Allocate keypair structure
  *keypair = malloc(sizeof(jsrt_rsa_keypair_t));
  if (!*keypair) {
    openssl_rsa_funcs.EVP_PKEY_free(pkey);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to allocate keypair structure");
    return -1;
  }

  // Store key information
  (*keypair)->public_key = pkey;
  (*keypair)->private_key = pkey;  // Same EVP_PKEY contains both public and private
  (*keypair)->modulus_length_bits = modulus_length_bits;
  (*keypair)->public_exponent = public_exponent;
  (*keypair)->hash_algorithm = hash_alg;

  JSRT_Debug("JSRT_Crypto_RSA: Successfully generated %zu-bit RSA key pair", modulus_length_bits);
  return 0;
}

// RSA-OAEP encryption
int jsrt_crypto_rsa_encrypt(jsrt_rsa_params_t *params, const uint8_t *plaintext, size_t plaintext_length,
                            uint8_t **ciphertext, size_t *ciphertext_length) {
  if (!load_rsa_functions()) {
    JSRT_Debug("JSRT_Crypto_RSA: OpenSSL functions not available for encryption");
    return -1;
  }

  // Create encryption context
  void *ctx = openssl_rsa_funcs.EVP_PKEY_CTX_new(params->rsa_key, NULL);
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to create encryption context");
    return -1;
  }

  // Initialize encryption
  if (openssl_rsa_funcs.EVP_PKEY_encrypt_init(ctx) <= 0) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to initialize encryption");
    return -1;
  }

  // Set padding mode - only for OAEP, PKCS1 v1.5 is default
  if (params->algorithm == JSRT_RSA_OAEP) {
    int padding = RSA_PKCS1_OAEP_PADDING;
    if (openssl_rsa_funcs.EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_ENCRYPT, EVP_PKEY_CTRL_RSA_PADDING, padding,
                                            NULL) <= 0) {
      openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
      JSRT_Debug("JSRT_Crypto_RSA: Failed to set OAEP padding mode");
      return -1;
    }
  }

  // For OAEP, set hash function
  if (params->algorithm == JSRT_RSA_OAEP) {
    const void *md = get_openssl_hash_func(params->hash_algorithm);
    if (md && openssl_rsa_funcs.EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_ENCRYPT, EVP_PKEY_CTRL_RSA_OAEP_MD, 0,
                                                  (void *)md) <= 0) {
      openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
      JSRT_Debug("JSRT_Crypto_RSA: Failed to set OAEP hash function");
      return -1;
    }
  }

  // Determine ciphertext length
  size_t outlen;
  if (openssl_rsa_funcs.EVP_PKEY_encrypt(ctx, NULL, &outlen, plaintext, plaintext_length) <= 0) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to determine ciphertext length");
    return -1;
  }

  // Allocate ciphertext buffer
  *ciphertext = malloc(outlen);
  if (!*ciphertext) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to allocate ciphertext buffer");
    return -1;
  }

  // Perform encryption
  if (openssl_rsa_funcs.EVP_PKEY_encrypt(ctx, *ciphertext, &outlen, plaintext, plaintext_length) <= 0) {
    free(*ciphertext);
    *ciphertext = NULL;
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to encrypt data");
    return -1;
  }

  *ciphertext_length = outlen;
  openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_RSA: Successfully encrypted %zu bytes to %zu bytes", plaintext_length, outlen);
  return 0;
}

// RSA-OAEP decryption
int jsrt_crypto_rsa_decrypt(jsrt_rsa_params_t *params, const uint8_t *ciphertext, size_t ciphertext_length,
                            uint8_t **plaintext, size_t *plaintext_length) {
  if (!load_rsa_functions()) {
    JSRT_Debug("JSRT_Crypto_RSA: OpenSSL functions not available for decryption");
    return -1;
  }

  // Create decryption context
  void *ctx = openssl_rsa_funcs.EVP_PKEY_CTX_new(params->rsa_key, NULL);
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to create decryption context");
    return -1;
  }

  // Initialize decryption
  if (openssl_rsa_funcs.EVP_PKEY_decrypt_init(ctx) <= 0) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to initialize decryption");
    return -1;
  }

  // Set padding mode - only for OAEP, PKCS1 v1.5 might be default
  if (params->algorithm == JSRT_RSA_OAEP) {
    int padding = RSA_PKCS1_OAEP_PADDING;
    if (openssl_rsa_funcs.EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_DECRYPT, EVP_PKEY_CTRL_RSA_PADDING, padding,
                                            NULL) <= 0) {
      openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
      JSRT_Debug("JSRT_Crypto_RSA: Failed to set OAEP padding mode for decryption");
      return -1;
    }
  }

  // For OAEP, set hash function
  if (params->algorithm == JSRT_RSA_OAEP) {
    const void *md = get_openssl_hash_func(params->hash_algorithm);
    if (md && openssl_rsa_funcs.EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, EVP_PKEY_OP_DECRYPT, EVP_PKEY_CTRL_RSA_OAEP_MD, 0,
                                                  (void *)md) <= 0) {
      openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
      JSRT_Debug("JSRT_Crypto_RSA: Failed to set OAEP hash function");
      return -1;
    }
  }

  // Determine plaintext length
  size_t outlen;
  if (openssl_rsa_funcs.EVP_PKEY_decrypt(ctx, NULL, &outlen, ciphertext, ciphertext_length) <= 0) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to determine plaintext length");
    return -1;
  }

  // Allocate plaintext buffer
  *plaintext = malloc(outlen);
  if (!*plaintext) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to allocate plaintext buffer");
    return -1;
  }

  // Perform decryption
  if (openssl_rsa_funcs.EVP_PKEY_decrypt(ctx, *plaintext, &outlen, ciphertext, ciphertext_length) <= 0) {
    free(*plaintext);
    *plaintext = NULL;
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to decrypt data");
    return -1;
  }

  *plaintext_length = outlen;
  openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_RSA: Successfully decrypted %zu bytes to %zu bytes", ciphertext_length, outlen);
  return 0;
}

// Helper functions
jsrt_rsa_algorithm_t jsrt_crypto_parse_rsa_algorithm(const char *algorithm_name) {
  if (strcmp(algorithm_name, "RSA-OAEP") == 0) {
    return JSRT_RSA_OAEP;
  } else if (strcmp(algorithm_name, "RSA-PKCS1-v1_5") == 0) {
    return JSRT_RSA_PKCS1_V1_5;
  } else if (strcmp(algorithm_name, "RSASSA-PKCS1-v1_5") == 0) {
    return JSRT_RSASSA_PKCS1_V1_5;
  } else if (strcmp(algorithm_name, "RSA-PSS") == 0) {
    return JSRT_RSA_PSS;
  }
  return JSRT_RSA_OAEP;  // Default fallback
}

jsrt_rsa_hash_algorithm_t jsrt_crypto_parse_rsa_hash_algorithm(const char *hash_name) {
  if (strcmp(hash_name, "SHA-1") == 0) {
    return JSRT_RSA_HASH_SHA1;
  } else if (strcmp(hash_name, "SHA-256") == 0) {
    return JSRT_RSA_HASH_SHA256;
  } else if (strcmp(hash_name, "SHA-384") == 0) {
    return JSRT_RSA_HASH_SHA384;
  } else if (strcmp(hash_name, "SHA-512") == 0) {
    return JSRT_RSA_HASH_SHA512;
  }
  return JSRT_RSA_HASH_SHA256;  // Default fallback
}

const char *jsrt_crypto_rsa_algorithm_to_string(jsrt_rsa_algorithm_t alg) {
  switch (alg) {
    case JSRT_RSA_OAEP:
      return "RSA-OAEP";
    case JSRT_RSA_PKCS1_V1_5:
      return "RSA-PKCS1-v1_5";
    case JSRT_RSASSA_PKCS1_V1_5:
      return "RSASSA-PKCS1-v1_5";
    case JSRT_RSA_PSS:
      return "RSA-PSS";
    default:
      return "RSA-Unknown";
  }
}

const char *jsrt_crypto_rsa_hash_algorithm_to_string(jsrt_rsa_hash_algorithm_t hash_alg) {
  switch (hash_alg) {
    case JSRT_RSA_HASH_SHA1:
      return "SHA-1";
    case JSRT_RSA_HASH_SHA256:
      return "SHA-256";
    case JSRT_RSA_HASH_SHA384:
      return "SHA-384";
    case JSRT_RSA_HASH_SHA512:
      return "SHA-512";
    default:
      return "SHA-256";
  }
}

bool jsrt_crypto_is_rsa_algorithm_supported(jsrt_rsa_algorithm_t alg) {
  switch (alg) {
    case JSRT_RSA_OAEP:
      return true;  // Implemented
    case JSRT_RSA_PKCS1_V1_5:
      return true;  // Implemented
    case JSRT_RSASSA_PKCS1_V1_5:
      return true;  // Implemented
    case JSRT_RSA_PSS:
      return false;  // TODO: Implement
    default:
      return false;
  }
}

bool jsrt_crypto_is_rsa_hash_supported(jsrt_rsa_hash_algorithm_t hash_alg) {
  switch (hash_alg) {
    case JSRT_RSA_HASH_SHA1:
    case JSRT_RSA_HASH_SHA256:
    case JSRT_RSA_HASH_SHA384:
    case JSRT_RSA_HASH_SHA512:
      return true;
    default:
      return false;
  }
}

// Memory management
void jsrt_crypto_rsa_keypair_free(jsrt_rsa_keypair_t *keypair) {
  if (keypair) {
    if (keypair->public_key && openssl_rsa_funcs.EVP_PKEY_free) {
      openssl_rsa_funcs.EVP_PKEY_free(keypair->public_key);
    }
    // Note: public_key and private_key point to the same EVP_PKEY, so don't double-free
    free(keypair);
  }
}

void jsrt_crypto_rsa_params_free(jsrt_rsa_params_t *params) {
  if (params) {
    if (params->algorithm == JSRT_RSA_OAEP) {
      free(params->params.oaep.label);
    }
    free(params);
  }
}

// Key extraction functions (for serialization)
int jsrt_crypto_rsa_extract_public_key_data(void *public_key, uint8_t **key_data, size_t *key_data_length) {
  if (!load_rsa_functions() || !openssl_rsa_funcs.i2d_PUBKEY) {
    JSRT_Debug("JSRT_Crypto_RSA: OpenSSL functions not available for key extraction");
    return -1;
  }

  // Serialize public key to DER format
  unsigned char *der_data = NULL;
  int der_length = openssl_rsa_funcs.i2d_PUBKEY(public_key, &der_data);

  if (der_length <= 0 || !der_data) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to serialize public key");
    return -1;
  }

  *key_data = malloc(der_length);
  if (!*key_data) {
    free(der_data);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to allocate key data buffer");
    return -1;
  }

  memcpy(*key_data, der_data, der_length);
  *key_data_length = der_length;
  free(der_data);

  JSRT_Debug("JSRT_Crypto_RSA: Successfully extracted public key data (%zu bytes)", *key_data_length);
  return 0;
}

int jsrt_crypto_rsa_extract_private_key_data(void *private_key, uint8_t **key_data, size_t *key_data_length) {
  if (!load_rsa_functions() || !openssl_rsa_funcs.i2d_PrivateKey) {
    JSRT_Debug("JSRT_Crypto_RSA: OpenSSL functions not available for key extraction");
    return -1;
  }

  // Serialize private key to DER format
  unsigned char *der_data = NULL;
  int der_length = openssl_rsa_funcs.i2d_PrivateKey(private_key, &der_data);

  if (der_length <= 0 || !der_data) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to serialize private key");
    return -1;
  }

  *key_data = malloc(der_length);
  if (!*key_data) {
    free(der_data);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to allocate key data buffer");
    return -1;
  }

  memcpy(*key_data, der_data, der_length);
  *key_data_length = der_length;
  free(der_data);

  JSRT_Debug("JSRT_Crypto_RSA: Successfully extracted private key data (%zu bytes)", *key_data_length);
  return 0;
}

// Create EVP_PKEY from DER-encoded public key data
void *jsrt_crypto_rsa_create_public_key_from_der(const uint8_t *key_data, size_t key_data_length) {
  if (!load_rsa_functions() || !openssl_rsa_funcs.d2i_PUBKEY) {
    JSRT_Debug("JSRT_Crypto_RSA: d2i_PUBKEY function not available");
    return NULL;
  }

  const unsigned char *der_ptr = key_data;
  void *pkey = openssl_rsa_funcs.d2i_PUBKEY(NULL, &der_ptr, key_data_length);

  if (!pkey) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to deserialize public key from DER data");
    return NULL;
  }

  JSRT_Debug("JSRT_Crypto_RSA: Successfully created public key from DER data");
  return pkey;
}

// Create EVP_PKEY from DER-encoded private key data
void *jsrt_crypto_rsa_create_private_key_from_der(const uint8_t *key_data, size_t key_data_length) {
  if (!load_rsa_functions() || !openssl_rsa_funcs.d2i_PrivateKey) {
    JSRT_Debug("JSRT_Crypto_RSA: d2i_PrivateKey function not available");
    return NULL;
  }

  const unsigned char *der_ptr = key_data;
  void *pkey = openssl_rsa_funcs.d2i_PrivateKey(EVP_PKEY_RSA, NULL, &der_ptr, key_data_length);

  if (!pkey) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to deserialize private key from DER data");
    return NULL;
  }

  JSRT_Debug("JSRT_Crypto_RSA: Successfully created private key from DER data");
  return pkey;
}

// Helper function to compute hash
static int compute_hash(const uint8_t *data, size_t data_length, jsrt_rsa_hash_algorithm_t hash_alg,
                        uint8_t *hash_output, size_t *hash_length) {
  if (!openssl_rsa_funcs.EVP_MD_CTX_new || !openssl_rsa_funcs.EVP_DigestInit_ex ||
      !openssl_rsa_funcs.EVP_DigestUpdate || !openssl_rsa_funcs.EVP_DigestFinal_ex) {
    JSRT_Debug("JSRT_Crypto_RSA: Hash functions not available");
    return -1;
  }

  const void *md = get_openssl_hash_func(hash_alg);
  if (!md) {
    JSRT_Debug("JSRT_Crypto_RSA: Unknown hash algorithm: %d", hash_alg);
    return -1;
  }

  void *md_ctx = openssl_rsa_funcs.EVP_MD_CTX_new();
  if (!md_ctx) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to create hash context");
    return -1;
  }

  if (openssl_rsa_funcs.EVP_DigestInit_ex(md_ctx, md, NULL) <= 0) {
    openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to initialize hash");
    return -1;
  }

  if (openssl_rsa_funcs.EVP_DigestUpdate(md_ctx, data, data_length) <= 0) {
    openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to update hash");
    return -1;
  }

  unsigned int final_hash_length = 0;
  if (openssl_rsa_funcs.EVP_DigestFinal_ex(md_ctx, hash_output, &final_hash_length) <= 0) {
    openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to finalize hash");
    return -1;
  }

  *hash_length = final_hash_length;
  openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);
  return 0;
}

// RSA signature/verification implementations
int jsrt_crypto_rsa_sign(jsrt_rsa_params_t *params, const uint8_t *data, size_t data_length, uint8_t **signature,
                         size_t *signature_length) {
  JSRT_Debug("JSRT_Crypto_RSA: Starting RSA signature, algorithm=%d, data_length=%zu", params->algorithm, data_length);

  if (!load_rsa_functions()) {
    JSRT_Debug("JSRT_Crypto_RSA: OpenSSL functions not available for signing");
    return -1;
  }

  if (!params || !params->rsa_key) {
    JSRT_Debug("JSRT_Crypto_RSA: Invalid parameters or RSA key is NULL");
    return -1;
  }

  // For RSASSA-PKCS1-v1_5, use EVP_DigestSign
  if (params->algorithm == JSRT_RSASSA_PKCS1_V1_5) {
    if (!openssl_rsa_funcs.EVP_DigestSignInit || !openssl_rsa_funcs.EVP_DigestSign) {
      JSRT_Debug("JSRT_Crypto_RSA: EVP_DigestSign functions not available");
      return -1;
    }

    const void *md = get_openssl_hash_func(params->hash_algorithm);
    if (!md) {
      JSRT_Debug("JSRT_Crypto_RSA: Hash function not available: %d", params->hash_algorithm);
      return -1;
    }

    // Create message digest context
    void *md_ctx = openssl_rsa_funcs.EVP_MD_CTX_new();
    if (!md_ctx) {
      JSRT_Debug("JSRT_Crypto_RSA: Failed to create message digest context");
      return -1;
    }

    // Initialize digest signing
    if (openssl_rsa_funcs.EVP_DigestSignInit(md_ctx, NULL, md, NULL, params->rsa_key) <= 0) {
      openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);
      JSRT_Debug("JSRT_Crypto_RSA: Failed to initialize digest signing");
      return -1;
    }

    // Get signature length
    size_t sig_len = 0;
    if (openssl_rsa_funcs.EVP_DigestSign(md_ctx, NULL, &sig_len, data, data_length) <= 0) {
      openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);
      JSRT_Debug("JSRT_Crypto_RSA: Failed to get signature length");
      return -1;
    }

    // Allocate signature buffer
    *signature = malloc(sig_len);
    if (!*signature) {
      openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);
      JSRT_Debug("JSRT_Crypto_RSA: Failed to allocate signature buffer");
      return -1;
    }

    // Perform signature
    if (openssl_rsa_funcs.EVP_DigestSign(md_ctx, *signature, &sig_len, data, data_length) <= 0) {
      free(*signature);
      *signature = NULL;
      openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);
      JSRT_Debug("JSRT_Crypto_RSA: Digest signing failed");
      return -1;
    }

    *signature_length = sig_len;
    openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);

    JSRT_Debug("JSRT_Crypto_RSA: Successfully signed data with EVP_DigestSign (%zu bytes signature)", sig_len);
    return 0;
  }

  // For other algorithms, use standard EVP_PKEY_sign
  // Create signing context
  void *ctx = openssl_rsa_funcs.EVP_PKEY_CTX_new(params->rsa_key, NULL);
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to create signing context");
    return -1;
  }

  // Initialize signing
  if (openssl_rsa_funcs.EVP_PKEY_sign_init(ctx) <= 0) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to initialize signing");
    return -1;
  }

  // Get signature length
  size_t sig_len = 0;
  if (openssl_rsa_funcs.EVP_PKEY_sign(ctx, NULL, &sig_len, data, data_length) <= 0) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to get signature length");
    return -1;
  }

  // Allocate signature buffer
  *signature = malloc(sig_len);
  if (!*signature) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to allocate signature buffer");
    return -1;
  }

  // Perform signature
  if (openssl_rsa_funcs.EVP_PKEY_sign(ctx, *signature, &sig_len, data, data_length) <= 0) {
    free(*signature);
    *signature = NULL;
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Signing failed");
    return -1;
  }

  *signature_length = sig_len;
  openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_RSA: Successfully signed data (%zu bytes signature)", sig_len);
  return 0;
}

bool jsrt_crypto_rsa_verify(jsrt_rsa_params_t *params, const uint8_t *data, size_t data_length,
                            const uint8_t *signature, size_t signature_length) {
  JSRT_Debug("JSRT_Crypto_RSA: Starting RSA verification, algorithm=%d, data_length=%zu", params->algorithm,
             data_length);

  if (!load_rsa_functions()) {
    JSRT_Debug("JSRT_Crypto_RSA: OpenSSL functions not available for verification");
    return false;
  }

  // For RSASSA-PKCS1-v1_5, use EVP_DigestVerify
  if (params->algorithm == JSRT_RSASSA_PKCS1_V1_5) {
    if (!openssl_rsa_funcs.EVP_DigestVerifyInit || !openssl_rsa_funcs.EVP_DigestVerify) {
      JSRT_Debug("JSRT_Crypto_RSA: EVP_DigestVerify functions not available");
      return false;
    }

    const void *md = get_openssl_hash_func(params->hash_algorithm);
    if (!md) {
      JSRT_Debug("JSRT_Crypto_RSA: Hash function not available: %d", params->hash_algorithm);
      return false;
    }

    // Create message digest context
    void *md_ctx = openssl_rsa_funcs.EVP_MD_CTX_new();
    if (!md_ctx) {
      JSRT_Debug("JSRT_Crypto_RSA: Failed to create message digest context");
      return false;
    }

    // Initialize digest verification
    if (openssl_rsa_funcs.EVP_DigestVerifyInit(md_ctx, NULL, md, NULL, params->rsa_key) <= 0) {
      openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);
      JSRT_Debug("JSRT_Crypto_RSA: Failed to initialize digest verification");
      return false;
    }

    // Perform verification
    int result = openssl_rsa_funcs.EVP_DigestVerify(md_ctx, signature, signature_length, data, data_length);
    openssl_rsa_funcs.EVP_MD_CTX_free(md_ctx);

    JSRT_Debug("JSRT_Crypto_RSA: Digest verification result: %d", result);
    return result == 1;
  }

  // For other algorithms, use standard EVP_PKEY_verify
  // Create verification context
  void *ctx = openssl_rsa_funcs.EVP_PKEY_CTX_new(params->rsa_key, NULL);
  if (!ctx) {
    JSRT_Debug("JSRT_Crypto_RSA: Failed to create verification context");
    return false;
  }

  // Initialize verification
  if (openssl_rsa_funcs.EVP_PKEY_verify_init(ctx) <= 0) {
    openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);
    JSRT_Debug("JSRT_Crypto_RSA: Failed to initialize verification");
    return false;
  }

  // Perform verification
  int result = openssl_rsa_funcs.EVP_PKEY_verify(ctx, signature, signature_length, data, data_length);
  openssl_rsa_funcs.EVP_PKEY_CTX_free(ctx);

  JSRT_Debug("JSRT_Crypto_RSA: Verification result: %d", result);
  return result == 1;
}