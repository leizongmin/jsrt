#include "crypto_ec.h"

#include <stdlib.h>
#include <string.h>

#include "crypto.h"
#include "crypto_subtle.h"

// Platform-specific includes for dynamic loading
#ifdef _WIN32
#include <windows.h>
#define JSRT_DLSYM(handle, name) ((void*)GetProcAddress(handle, name))
#else
#include <dlfcn.h>
#define JSRT_DLSYM(handle, name) dlsym(handle, name)
#endif

// OpenSSL EC functions
typedef void* (*EVP_PKEY_CTX_new_id_fn)(int id, void* e);
typedef int (*EVP_PKEY_keygen_init_fn)(void* ctx);
typedef int (*EVP_PKEY_CTX_set_ec_paramgen_curve_nid_fn)(void* ctx, int nid);
typedef int (*EVP_PKEY_keygen_fn)(void* ctx, void** ppkey);
typedef int (*EVP_DigestSignInit_fn)(void* ctx, void** pctx, const void* type, void* e, void* pkey);
typedef int (*EVP_DigestSign_fn)(void* ctx, unsigned char* sigret, size_t* siglen, const unsigned char* tbs,
                                 size_t tbslen);
typedef int (*EVP_DigestVerifyInit_fn)(void* ctx, void** pctx, const void* type, void* e, void* pkey);
typedef int (*EVP_DigestVerify_fn)(void* ctx, const unsigned char* sigret, size_t siglen, const unsigned char* tbs,
                                   size_t tbslen);
typedef int (*EVP_PKEY_derive_init_fn)(void* ctx);
typedef int (*EVP_PKEY_derive_set_peer_fn)(void* ctx, void* peer);
typedef int (*EVP_PKEY_derive_fn)(void* ctx, unsigned char* key, size_t* keylen);
typedef void* (*EVP_PKEY_CTX_new_fn)(void* pkey, void* e);
typedef int (*EVP_PKEY_paramgen_init_fn)(void* ctx);
typedef int (*EVP_PKEY_paramgen_fn)(void* ctx, void** ppkey);
typedef void* (*EC_KEY_new_by_curve_name_fn)(int nid);
typedef void (*EC_KEY_free_fn)(void* key);
typedef int (*EVP_PKEY_set1_EC_KEY_fn)(void* pkey, void* key);
typedef void* (*EVP_PKEY_new_fn)(void);

// OpenSSL NID constants for elliptic curves
#define NID_X9_62_prime256v1 415  // P-256
#define NID_secp384r1 715         // P-384
#define NID_secp521r1 716         // P-521

// EVP_PKEY type for EC
#define EVP_PKEY_EC 408

// Control value for setting EC curve
#define EVP_PKEY_ALG_CTRL 0x1000
#define EVP_PKEY_CTRL_EC_PARAMGEN_CURVE_NID (EVP_PKEY_ALG_CTRL + 1)

// Additional OpenSSL function types
typedef void (*EVP_PKEY_CTX_free_fn)(void* ctx);
typedef void (*EVP_PKEY_free_fn)(void* pkey);
typedef void* (*EVP_MD_CTX_new_fn)(void);
typedef void (*EVP_MD_CTX_free_fn)(void* ctx);
typedef const void* (*EVP_get_digestbyname_fn)(const char* name);
typedef int (*i2d_PUBKEY_fn)(void* pkey, unsigned char** ppout);
typedef int (*i2d_PrivateKey_fn)(void* pkey, unsigned char** ppout);
typedef void* (*d2i_PUBKEY_fn)(void** a, const unsigned char** pp, long length);
typedef void* (*d2i_AutoPrivateKey_fn)(void** a, const unsigned char** pp, long length);

// Dynamic function pointers
static EVP_PKEY_CTX_new_id_fn evp_pkey_ctx_new_id = NULL;
static EVP_PKEY_keygen_init_fn evp_pkey_keygen_init = NULL;
static EVP_PKEY_keygen_fn evp_pkey_keygen = NULL;
static EVP_DigestSignInit_fn evp_digestsigninit = NULL;
static EVP_DigestSign_fn evp_digestsign = NULL;
static EVP_DigestVerifyInit_fn evp_digestverifyinit = NULL;
static EVP_DigestVerify_fn evp_digestverify = NULL;
static EVP_PKEY_derive_init_fn evp_pkey_derive_init = NULL;
static EVP_PKEY_derive_set_peer_fn evp_pkey_derive_set_peer = NULL;
static EVP_PKEY_derive_fn evp_pkey_derive = NULL;
static EVP_PKEY_CTX_new_fn evp_pkey_ctx_new = NULL;
static EVP_PKEY_paramgen_init_fn evp_pkey_paramgen_init = NULL;
static EVP_PKEY_paramgen_fn evp_pkey_paramgen = NULL;
static EC_KEY_new_by_curve_name_fn ec_key_new_by_curve_name = NULL;
static EC_KEY_free_fn ec_key_free = NULL;
static EVP_PKEY_set1_EC_KEY_fn evp_pkey_set1_ec_key = NULL;
static EVP_PKEY_new_fn evp_pkey_new = NULL;
static EVP_PKEY_CTX_free_fn evp_pkey_ctx_free = NULL;
static EVP_PKEY_free_fn evp_pkey_free = NULL;
static EVP_MD_CTX_new_fn evp_md_ctx_new = NULL;
static EVP_MD_CTX_free_fn evp_md_ctx_free = NULL;
static EVP_get_digestbyname_fn evp_get_digestbyname = NULL;
static i2d_PUBKEY_fn i2d_pubkey = NULL;
static i2d_PrivateKey_fn i2d_privatekey = NULL;
static d2i_PUBKEY_fn d2i_pubkey = NULL;
static d2i_AutoPrivateKey_fn d2i_autoprivatekey = NULL;

// EVP_PKEY_CTX_ctrl function
typedef int (*EVP_PKEY_CTX_ctrl_fn)(void* ctx, int keytype, int optype, int cmd, int p1, void* p2);
static EVP_PKEY_CTX_ctrl_fn evp_pkey_ctx_ctrl = NULL;

// Forward declarations for helper functions
static void jsrt_evp_pkey_ctx_free(void* ctx);
static void jsrt_evp_pkey_free(void* pkey);
static void* jsrt_evp_md_ctx_new(void);
static void jsrt_evp_md_ctx_free(void* ctx);
static const void* jsrt_get_digest_algorithm(const char* name);
static JSValue jsrt_create_key_pair(JSContext* ctx, void* pkey, const char* algorithm, const char* curve,
                                    const char* hash);

// Helper function to check if OpenSSL is loaded
static int jsrt_crypto_init(void) {
  return openssl_handle != NULL;
}

// Initialize EC crypto functions
static int jsrt_ec_init(void) {
  static int initialized = 0;
  if (initialized)
    return 1;

  if (!jsrt_crypto_init()) {
    return 0;
  }

  if (!openssl_handle)
    return 0;

  // Load EC-specific functions
  evp_pkey_ctx_new_id = (EVP_PKEY_CTX_new_id_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_new_id");
  evp_pkey_keygen_init = (EVP_PKEY_keygen_init_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_keygen_init");
  evp_pkey_keygen = (EVP_PKEY_keygen_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_keygen");
  evp_digestsigninit = (EVP_DigestSignInit_fn)JSRT_DLSYM(openssl_handle, "EVP_DigestSignInit");
  evp_digestsign = (EVP_DigestSign_fn)JSRT_DLSYM(openssl_handle, "EVP_DigestSign");
  evp_digestverifyinit = (EVP_DigestVerifyInit_fn)JSRT_DLSYM(openssl_handle, "EVP_DigestVerifyInit");
  evp_digestverify = (EVP_DigestVerify_fn)JSRT_DLSYM(openssl_handle, "EVP_DigestVerify");
  evp_pkey_derive_init = (EVP_PKEY_derive_init_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_derive_init");
  evp_pkey_derive_set_peer = (EVP_PKEY_derive_set_peer_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_derive_set_peer");
  evp_pkey_derive = (EVP_PKEY_derive_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_derive");
  evp_pkey_ctx_new = (EVP_PKEY_CTX_new_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_new");
  evp_pkey_paramgen_init = (EVP_PKEY_paramgen_init_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_paramgen_init");
  evp_pkey_paramgen = (EVP_PKEY_paramgen_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_paramgen");
  evp_pkey_ctx_ctrl = (EVP_PKEY_CTX_ctrl_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_ctrl");
  evp_pkey_ctx_free = (EVP_PKEY_CTX_free_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_free");
  evp_pkey_free = (EVP_PKEY_free_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
  evp_md_ctx_new = (EVP_MD_CTX_new_fn)JSRT_DLSYM(openssl_handle, "EVP_MD_CTX_new");
  evp_md_ctx_free = (EVP_MD_CTX_free_fn)JSRT_DLSYM(openssl_handle, "EVP_MD_CTX_free");
  evp_get_digestbyname = (EVP_get_digestbyname_fn)JSRT_DLSYM(openssl_handle, "EVP_get_digestbyname");
  i2d_pubkey = (i2d_PUBKEY_fn)JSRT_DLSYM(openssl_handle, "i2d_PUBKEY");
  i2d_privatekey = (i2d_PrivateKey_fn)JSRT_DLSYM(openssl_handle, "i2d_PrivateKey");
  d2i_pubkey = (d2i_PUBKEY_fn)JSRT_DLSYM(openssl_handle, "d2i_PUBKEY");
  d2i_autoprivatekey = (d2i_AutoPrivateKey_fn)JSRT_DLSYM(openssl_handle, "d2i_AutoPrivateKey");
  ec_key_new_by_curve_name = (EC_KEY_new_by_curve_name_fn)JSRT_DLSYM(openssl_handle, "EC_KEY_new_by_curve_name");
  ec_key_free = (EC_KEY_free_fn)JSRT_DLSYM(openssl_handle, "EC_KEY_free");
  evp_pkey_set1_ec_key = (EVP_PKEY_set1_EC_KEY_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_set1_EC_KEY");
  evp_pkey_new = (EVP_PKEY_new_fn)JSRT_DLSYM(openssl_handle, "EVP_PKEY_new");

  if (!evp_pkey_ctx_new_id || !evp_pkey_keygen_init || !evp_pkey_keygen || !evp_digestsigninit || !evp_digestsign ||
      !evp_digestverifyinit || !evp_digestverify || !evp_pkey_ctx_ctrl) {
    return 0;
  }

  initialized = 1;
  return 1;
}

// Convert curve name to enum
int jsrt_ec_curve_from_string(const char* curve_name, jsrt_ec_curve_t* curve) {
  if (!curve_name || !curve)
    return 0;

  if (strcmp(curve_name, "P-256") == 0) {
    *curve = JSRT_EC_P256;
  } else if (strcmp(curve_name, "P-384") == 0) {
    *curve = JSRT_EC_P384;
  } else if (strcmp(curve_name, "P-521") == 0) {
    *curve = JSRT_EC_P521;
  } else {
    return 0;
  }

  return 1;
}

// Convert curve enum to string
const char* jsrt_ec_curve_to_string(jsrt_ec_curve_t curve) {
  switch (curve) {
    case JSRT_EC_P256:
      return "P-256";
    case JSRT_EC_P384:
      return "P-384";
    case JSRT_EC_P521:
      return "P-521";
    default:
      return NULL;
  }
}

// Get OpenSSL NID for curve
int jsrt_ec_get_openssl_nid(jsrt_ec_curve_t curve) {
  switch (curve) {
    case JSRT_EC_P256:
      return NID_X9_62_prime256v1;
    case JSRT_EC_P384:
      return NID_secp384r1;
    case JSRT_EC_P521:
      return NID_secp521r1;
    default:
      return 0;
  }
}

// Generate EC key pair
JSValue jsrt_ec_generate_key(JSContext* ctx, const jsrt_ec_keygen_params_t* params) {
  if (!params) {
    return JS_ThrowTypeError(ctx, "Invalid parameters");
  }

  if (!jsrt_ec_init()) {
    return JS_ThrowInternalError(ctx, "Failed to initialize EC crypto");
  }

  void* pkey_ctx = NULL;
  void* pkey = NULL;
  void* ec_key = NULL;
  JSValue result = JS_UNDEFINED;

  // Try method 1: Direct EC key generation with EVP_PKEY_CTX
  pkey_ctx = evp_pkey_ctx_new_id(EVP_PKEY_EC, NULL);
  if (pkey_ctx) {
    if (evp_pkey_keygen_init(pkey_ctx) > 0) {
      int nid = jsrt_ec_get_openssl_nid(params->curve);

      // Try to set curve using EVP_PKEY_CTX_ctrl
      if (evp_pkey_ctx_ctrl(pkey_ctx, EVP_PKEY_EC, -1, EVP_PKEY_CTRL_EC_PARAMGEN_CURVE_NID, nid, NULL) > 0) {
        if (evp_pkey_keygen(pkey_ctx, &pkey) > 0) {
          // Success - create key pair object
          result = jsrt_create_key_pair(ctx, pkey, params->algorithm == JSRT_ECDSA ? "ECDSA" : "ECDH",
                                        jsrt_ec_curve_to_string(params->curve), params->hash);
          goto cleanup;
        }
      }
    }
  }

  // Method 1 failed, clean up
  if (pkey_ctx) {
    jsrt_evp_pkey_ctx_free(pkey_ctx);
    pkey_ctx = NULL;
  }

  // Try method 2: Using EC_KEY (older OpenSSL compatibility)
  if (ec_key_new_by_curve_name && evp_pkey_set1_ec_key && evp_pkey_new) {
    int nid = jsrt_ec_get_openssl_nid(params->curve);
    ec_key = ec_key_new_by_curve_name(nid);

    if (ec_key) {
      pkey = evp_pkey_new();
      if (pkey && evp_pkey_set1_ec_key(pkey, ec_key) > 0) {
        // Generate the key
        void* keygen_ctx = evp_pkey_ctx_new(pkey, NULL);
        if (keygen_ctx) {
          if (evp_pkey_keygen_init(keygen_ctx) > 0) {
            void* new_pkey = NULL;
            if (evp_pkey_keygen(keygen_ctx, &new_pkey) > 0) {
              result = jsrt_create_key_pair(ctx, new_pkey, params->algorithm == JSRT_ECDSA ? "ECDSA" : "ECDH",
                                            jsrt_ec_curve_to_string(params->curve), params->hash);
              jsrt_evp_pkey_ctx_free(keygen_ctx);
              goto cleanup;
            }
          }
          jsrt_evp_pkey_ctx_free(keygen_ctx);
        }
      }
    }
  }

  // If we get here, both methods failed
  result = JS_ThrowInternalError(ctx, "Failed to generate EC key pair");

cleanup:
  if (pkey_ctx) {
    jsrt_evp_pkey_ctx_free(pkey_ctx);
  }
  if (ec_key && ec_key_free) {
    ec_key_free(ec_key);
  }
  if (pkey && JS_IsException(result)) {
    jsrt_evp_pkey_free(pkey);
  }

  return result;
}

// Sign with ECDSA
JSValue jsrt_ec_sign(JSContext* ctx, void* key, const uint8_t* data, size_t data_len,
                     const jsrt_ecdsa_sign_params_t* params) {
  if (!key || !data || !params) {
    return JS_ThrowTypeError(ctx, "Invalid parameters");
  }

  if (!jsrt_ec_init()) {
    return JS_ThrowInternalError(ctx, "Failed to initialize EC crypto");
  }

  // Get hash algorithm
  const void* md = jsrt_get_digest_algorithm(params->hash);
  if (!md) {
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm");
  }

  // Create signing context
  void* md_ctx = jsrt_evp_md_ctx_new();
  if (!md_ctx) {
    return JS_ThrowInternalError(ctx, "Failed to create digest context");
  }

  // Initialize signing
  if (evp_digestsigninit(md_ctx, NULL, md, NULL, key) <= 0) {
    jsrt_evp_md_ctx_free(md_ctx);
    return JS_ThrowInternalError(ctx, "Failed to initialize ECDSA signing");
  }

  // Determine signature size
  size_t sig_len = 0;
  if (evp_digestsign(md_ctx, NULL, &sig_len, data, data_len) <= 0) {
    jsrt_evp_md_ctx_free(md_ctx);
    return JS_ThrowInternalError(ctx, "Failed to determine signature size");
  }

  // Allocate signature buffer
  uint8_t* signature = js_malloc(ctx, sig_len);
  if (!signature) {
    jsrt_evp_md_ctx_free(md_ctx);
    return JS_EXCEPTION;
  }

  // Perform signing
  if (evp_digestsign(md_ctx, signature, &sig_len, data, data_len) <= 0) {
    js_free(ctx, signature);
    jsrt_evp_md_ctx_free(md_ctx);
    return JS_ThrowInternalError(ctx, "Failed to sign data with ECDSA");
  }

  jsrt_evp_md_ctx_free(md_ctx);

  // Create ArrayBuffer with signature
  JSValue result = JS_NewArrayBufferCopy(ctx, signature, sig_len);
  js_free(ctx, signature);

  return result;
}

// Verify ECDSA signature
JSValue jsrt_ec_verify(JSContext* ctx, void* key, const uint8_t* signature, size_t sig_len, const uint8_t* data,
                       size_t data_len, const jsrt_ecdsa_sign_params_t* params) {
  if (!key || !signature || !data || !params) {
    return JS_ThrowTypeError(ctx, "Invalid parameters");
  }

  if (!jsrt_ec_init()) {
    return JS_ThrowInternalError(ctx, "Failed to initialize EC crypto");
  }

  // Get hash algorithm
  const void* md = jsrt_get_digest_algorithm(params->hash);
  if (!md) {
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm");
  }

  // Create verification context
  void* md_ctx = jsrt_evp_md_ctx_new();
  if (!md_ctx) {
    return JS_ThrowInternalError(ctx, "Failed to create digest context");
  }

  // Initialize verification
  if (evp_digestverifyinit(md_ctx, NULL, md, NULL, key) <= 0) {
    jsrt_evp_md_ctx_free(md_ctx);
    return JS_ThrowInternalError(ctx, "Failed to initialize ECDSA verification");
  }

  // Perform verification
  int result = evp_digestverify(md_ctx, signature, sig_len, data, data_len);
  jsrt_evp_md_ctx_free(md_ctx);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to verify ECDSA signature");
  }

  return JS_NewBool(ctx, result == 1);
}

// Derive bits using ECDH
JSValue jsrt_ec_derive_bits(JSContext* ctx, void* private_key, const jsrt_ecdh_derive_params_t* params) {
  if (!private_key || !params || !params->public_key) {
    return JS_ThrowTypeError(ctx, "Invalid parameters");
  }

  if (!jsrt_ec_init()) {
    return JS_ThrowInternalError(ctx, "Failed to initialize EC crypto");
  }

  // Create derivation context
  void* derive_ctx = evp_pkey_ctx_new(private_key, NULL);
  if (!derive_ctx) {
    return JS_ThrowInternalError(ctx, "Failed to create derivation context");
  }

  // Initialize key derivation
  if (evp_pkey_derive_init(derive_ctx) <= 0) {
    jsrt_evp_pkey_ctx_free(derive_ctx);
    return JS_ThrowInternalError(ctx, "Failed to initialize ECDH derivation");
  }

  // Set peer public key
  if (evp_pkey_derive_set_peer(derive_ctx, params->public_key) <= 0) {
    jsrt_evp_pkey_ctx_free(derive_ctx);
    return JS_ThrowInternalError(ctx, "Failed to set peer public key");
  }

  // Determine derived key size
  size_t key_len = 0;
  if (evp_pkey_derive(derive_ctx, NULL, &key_len) <= 0) {
    jsrt_evp_pkey_ctx_free(derive_ctx);
    return JS_ThrowInternalError(ctx, "Failed to determine derived key size");
  }

  // Allocate buffer for derived key
  uint8_t* derived_key = js_malloc(ctx, key_len);
  if (!derived_key) {
    jsrt_evp_pkey_ctx_free(derive_ctx);
    return JS_EXCEPTION;
  }

  // Perform key derivation
  if (evp_pkey_derive(derive_ctx, derived_key, &key_len) <= 0) {
    js_free(ctx, derived_key);
    jsrt_evp_pkey_ctx_free(derive_ctx);
    return JS_ThrowInternalError(ctx, "Failed to derive key with ECDH");
  }

  jsrt_evp_pkey_ctx_free(derive_ctx);

  // Create ArrayBuffer with derived key
  JSValue result = JS_NewArrayBufferCopy(ctx, derived_key, key_len);
  js_free(ctx, derived_key);

  return result;
}

// Helper functions for memory management and key handling
static void jsrt_evp_pkey_ctx_free(void* ctx) {
  if (ctx && evp_pkey_ctx_free) {
    evp_pkey_ctx_free(ctx);
  }
}

static void jsrt_evp_pkey_free(void* pkey) {
  if (pkey && evp_pkey_free) {
    evp_pkey_free(pkey);
  }
}

// Public wrapper for external use
void jsrt_evp_pkey_free_wrapper(void* pkey) {
  if (!jsrt_ec_init()) {
    return;
  }
  jsrt_evp_pkey_free(pkey);
}

static void* jsrt_evp_md_ctx_new(void) {
  if (evp_md_ctx_new) {
    return evp_md_ctx_new();
  }
  return NULL;
}

static void jsrt_evp_md_ctx_free(void* ctx) {
  if (ctx && evp_md_ctx_free) {
    evp_md_ctx_free(ctx);
  }
}

static const void* jsrt_get_digest_algorithm(const char* name) {
  if (!name || !evp_get_digestbyname) {
    return NULL;
  }

  // Map common names to OpenSSL names
  const char* openssl_name = name;
  if (strcmp(name, "SHA-256") == 0) {
    openssl_name = "SHA256";
  } else if (strcmp(name, "SHA-384") == 0) {
    openssl_name = "SHA384";
  } else if (strcmp(name, "SHA-512") == 0) {
    openssl_name = "SHA512";
  } else if (strcmp(name, "SHA-1") == 0) {
    openssl_name = "SHA1";
  }

  return evp_get_digestbyname(openssl_name);
}

// Create a CryptoKeyPair JavaScript object from an EC key pair
static JSValue jsrt_create_key_pair(JSContext* ctx, void* pkey, const char* algorithm, const char* curve,
                                    const char* hash) {
  if (!pkey) {
    return JS_ThrowInternalError(ctx, "Invalid key pair");
  }

  // Extract public and private key data
  unsigned char* pub_der = NULL;
  int pub_len = 0;
  unsigned char* priv_der = NULL;
  int priv_len = 0;

  // Serialize public key
  if (i2d_pubkey) {
    pub_len = i2d_pubkey(pkey, &pub_der);
    if (pub_len <= 0) {
      jsrt_evp_pkey_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to serialize public key");
    }
  }

  // Serialize private key
  if (i2d_privatekey) {
    priv_len = i2d_privatekey(pkey, &priv_der);
    if (priv_len <= 0) {
      if (pub_der) {
        if (openssl_handle) {
          void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
          if (OPENSSL_free) {
            OPENSSL_free(pub_der);
          } else {
            free(pub_der);
          }
        } else {
          free(pub_der);
        }
      }
      jsrt_evp_pkey_free(pkey);
      return JS_ThrowInternalError(ctx, "Failed to serialize private key");
    }
  }

  // Create CryptoKeyPair object
  JSValue keypair_obj = JS_NewObject(ctx);

  // Create public key object
  JSValue public_key_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, public_key_obj, "type", JS_NewString(ctx, "public"));
  JS_SetPropertyStr(ctx, public_key_obj, "extractable", JS_NewBool(ctx, 1));

  JSValue public_alg_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, public_alg_obj, "name", JS_NewString(ctx, algorithm));
  JS_SetPropertyStr(ctx, public_alg_obj, "namedCurve", JS_NewString(ctx, curve));
  if (hash) {
    JSValue hash_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, hash_obj, "name", JS_NewString(ctx, hash));
    JS_SetPropertyStr(ctx, public_alg_obj, "hash", hash_obj);
  }
  JS_SetPropertyStr(ctx, public_key_obj, "algorithm", public_alg_obj);

  // Store public key data
  if (pub_der && pub_len > 0) {
    JSValue pub_buffer = JS_NewArrayBufferCopy(ctx, pub_der, pub_len);
    JS_SetPropertyStr(ctx, public_key_obj, "__keyData", pub_buffer);
  }

  // Create private key object
  JSValue private_key_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, private_key_obj, "type", JS_NewString(ctx, "private"));
  JS_SetPropertyStr(ctx, private_key_obj, "extractable", JS_NewBool(ctx, 0));
  JS_SetPropertyStr(ctx, private_key_obj, "algorithm", JS_DupValue(ctx, public_alg_obj));

  // Store private key data
  if (priv_der && priv_len > 0) {
    JSValue priv_buffer = JS_NewArrayBufferCopy(ctx, priv_der, priv_len);
    JS_SetPropertyStr(ctx, private_key_obj, "__keyData", priv_buffer);
  }

  // Set the key pair
  JS_SetPropertyStr(ctx, keypair_obj, "publicKey", public_key_obj);
  JS_SetPropertyStr(ctx, keypair_obj, "privateKey", private_key_obj);

  // Cleanup - OpenSSL allocates with OPENSSL_malloc, need to use OPENSSL_free
  if (pub_der) {
    if (openssl_handle) {
      void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
      if (OPENSSL_free) {
        OPENSSL_free(pub_der);
      } else {
        free(pub_der);  // Fallback
      }
    } else {
      free(pub_der);
    }
  }
  if (priv_der) {
    if (openssl_handle) {
      void (*OPENSSL_free)(void*) = JSRT_DLSYM(openssl_handle, "OPENSSL_free");
      if (OPENSSL_free) {
        OPENSSL_free(priv_der);
      } else {
        free(priv_der);  // Fallback
      }
    } else {
      free(priv_der);
    }
  }
  jsrt_evp_pkey_free(pkey);

  return keypair_obj;
}