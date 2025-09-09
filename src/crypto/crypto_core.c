#include "crypto_core.h"
#include "../util/debug.h"

#include <string.h>
#include <stdio.h>

// Platform-specific includes for dynamic loading
#ifdef _WIN32
  #include <windows.h>
  #define JSRT_DLSYM(handle, name) ((void*)GetProcAddress(handle, name))
#else
  #include <dlfcn.h>
  #define JSRT_DLSYM(handle, name) dlsym(handle, name)
#endif

// Static OpenSSL includes (only when static linking is enabled)
#ifdef JSRT_STATIC_OPENSSL
  #include <openssl/evp.h>
  #include <openssl/rand.h>
  #include <openssl/rsa.h>
  #include <openssl/aes.h>
  #include <openssl/x509.h>
  #include <openssl/pem.h>
#endif

// OpenSSL constants (avoid redefinition when statically linking)
#ifndef EVP_PKEY_RSA
#define EVP_PKEY_RSA 6
#endif
#ifndef EVP_PKEY_OP_ENCRYPT
#define EVP_PKEY_OP_ENCRYPT (1 << 0)
#endif
#ifndef EVP_PKEY_OP_DECRYPT
#define EVP_PKEY_OP_DECRYPT (1 << 1)
#endif
#ifndef EVP_PKEY_OP_SIGN
#define EVP_PKEY_OP_SIGN (1 << 2)
#endif
#ifndef EVP_PKEY_OP_VERIFY
#define EVP_PKEY_OP_VERIFY (1 << 3)
#endif

// RSA padding constants
#ifndef RSA_PKCS1_PADDING
#define RSA_PKCS1_PADDING 1
#endif
#ifndef RSA_PKCS1_OAEP_PADDING
#define RSA_PKCS1_OAEP_PADDING 4
#endif
#ifndef RSA_PKCS1_PSS_PADDING
#define RSA_PKCS1_PSS_PADDING 6
#endif

// GCM control constants
#ifndef EVP_CTRL_GCM_SET_IVLEN
#define EVP_CTRL_GCM_SET_IVLEN 0x9
#endif
#ifndef EVP_CTRL_GCM_GET_TAG
#define EVP_CTRL_GCM_GET_TAG 0x10
#endif
#ifndef EVP_CTRL_GCM_SET_TAG
#define EVP_CTRL_GCM_SET_TAG 0x11
#endif

// =============================================================================
// Static OpenSSL Function Setup
// =============================================================================

#ifdef JSRT_STATIC_OPENSSL
// Static linking implementations
static const void* static_get_md(jsrt_crypto_algorithm_t alg) {
    switch (alg) {
        case JSRT_CRYPTO_ALG_SHA1:
            return EVP_sha1();
        case JSRT_CRYPTO_ALG_SHA256:
            return EVP_sha256();
        case JSRT_CRYPTO_ALG_SHA384:
            return EVP_sha384();
        case JSRT_CRYPTO_ALG_SHA512:
            return EVP_sha512();
        default:
            return NULL;
    }
}

static int static_get_digest_size(jsrt_crypto_algorithm_t alg) {
    const EVP_MD* md = (const EVP_MD*)static_get_md(alg);
    if (md) {
        return EVP_MD_size(md);
    }
    // Fallback to known sizes
    switch (alg) {
        case JSRT_CRYPTO_ALG_SHA1: return 20;
        case JSRT_CRYPTO_ALG_SHA256: return 32;
        case JSRT_CRYPTO_ALG_SHA384: return 48;
        case JSRT_CRYPTO_ALG_SHA512: return 64;
        default: return 0;
    }
}

static const void* static_get_cipher(jsrt_symmetric_algorithm_t alg, size_t key_length) {
    switch (alg) {
        case JSRT_SYMMETRIC_AES_CBC:
            switch (key_length) {
                case 16: return EVP_aes_128_cbc();
                case 24: return EVP_aes_192_cbc();
                case 32: return EVP_aes_256_cbc();
            }
            break;
        case JSRT_SYMMETRIC_AES_GCM:
            switch (key_length) {
                case 16: return EVP_aes_128_gcm();
                case 24: return EVP_aes_192_gcm();
                case 32: return EVP_aes_256_gcm();
            }
            break;
        case JSRT_SYMMETRIC_AES_CTR:
            switch (key_length) {
                case 16: return EVP_aes_128_ctr();
                case 24: return EVP_aes_192_ctr();
                case 32: return EVP_aes_256_ctr();
            }
            break;
    }
    return NULL;
}
#endif

// =============================================================================
// Function Pointer Setup
// =============================================================================

bool jsrt_crypto_core_setup_static_funcs(jsrt_crypto_openssl_funcs_t* funcs) {
#ifdef JSRT_STATIC_OPENSSL
    if (!funcs) {
        return false;
    }
    
    // Hash functions
    funcs->get_md = static_get_md;
    funcs->get_digest_size = static_get_digest_size;
    
    // Digest context functions
    funcs->md_ctx_new = (void* (*)(void))EVP_MD_CTX_new;
    funcs->md_ctx_free = (void (*)(void*))EVP_MD_CTX_free;
    funcs->digest_init_ex = (int (*)(void*, const void*, void*))EVP_DigestInit_ex;
    funcs->digest_update = (int (*)(void*, const void*, size_t))EVP_DigestUpdate;
    funcs->digest_final_ex = (int (*)(void*, unsigned char*, unsigned int*))EVP_DigestFinal_ex;
    
    // Random functions
    funcs->rand_bytes = RAND_bytes;
    
    // Cipher functions
    funcs->get_cipher = static_get_cipher;
    funcs->cipher_ctx_new = (void* (*)(void))EVP_CIPHER_CTX_new;
    funcs->cipher_ctx_free = (void (*)(void*))EVP_CIPHER_CTX_free;
    funcs->encrypt_init_ex = (int (*)(void*, const void*, void*, const unsigned char*, const unsigned char*))EVP_EncryptInit_ex;
    funcs->encrypt_update = (int (*)(void*, unsigned char*, int*, const unsigned char*, int))EVP_EncryptUpdate;
    funcs->encrypt_final_ex = (int (*)(void*, unsigned char*, int*))EVP_EncryptFinal_ex;
    funcs->decrypt_init_ex = (int (*)(void*, const void*, void*, const unsigned char*, const unsigned char*))EVP_DecryptInit_ex;
    funcs->decrypt_update = (int (*)(void*, unsigned char*, int*, const unsigned char*, int))EVP_DecryptUpdate;
    funcs->decrypt_final_ex = (int (*)(void*, unsigned char*, int*))EVP_DecryptFinal_ex;
    funcs->cipher_ctx_ctrl = (int (*)(void*, int, int, void*))EVP_CIPHER_CTX_ctrl;
    
    // RSA functions
    funcs->pkey_new = (void* (*)(void))EVP_PKEY_new;
    funcs->pkey_free = (void (*)(void*))EVP_PKEY_free;
    funcs->pkey_ctx_new_id = (void* (*)(int, void*))EVP_PKEY_CTX_new_id;
    funcs->pkey_ctx_new = (void* (*)(void*, void*))EVP_PKEY_CTX_new;
    funcs->pkey_ctx_free = (void (*)(void*))EVP_PKEY_CTX_free;
    funcs->pkey_keygen_init = (int (*)(void*))EVP_PKEY_keygen_init;
    funcs->pkey_keygen = (int (*)(void*, void**))EVP_PKEY_keygen;
    funcs->pkey_ctx_ctrl = (int (*)(void*, int, int, int, int, void*))EVP_PKEY_CTX_ctrl;
    funcs->pkey_ctx_ctrl_str = (int (*)(void*, const char*, const char*))EVP_PKEY_CTX_ctrl_str;
    
    funcs->pkey_encrypt_init = (int (*)(void*))EVP_PKEY_encrypt_init;
    funcs->pkey_encrypt = (int (*)(void*, unsigned char*, size_t*, const unsigned char*, size_t))EVP_PKEY_encrypt;
    funcs->pkey_decrypt_init = (int (*)(void*))EVP_PKEY_decrypt_init;
    funcs->pkey_decrypt = (int (*)(void*, unsigned char*, size_t*, const unsigned char*, size_t))EVP_PKEY_decrypt;
    funcs->pkey_sign_init = (int (*)(void*))EVP_PKEY_sign_init;
    funcs->pkey_sign = (int (*)(void*, unsigned char*, size_t*, const unsigned char*, size_t))EVP_PKEY_sign;
    funcs->pkey_verify_init = (int (*)(void*))EVP_PKEY_verify_init;
    funcs->pkey_verify = (int (*)(void*, const unsigned char*, size_t, const unsigned char*, size_t))EVP_PKEY_verify;
    
    // Digest signing/verification
    funcs->digest_sign_init = (int (*)(void*, void**, const void*, void*, void*))EVP_DigestSignInit;
    funcs->digest_sign = (int (*)(void*, unsigned char*, size_t*, const unsigned char*, size_t))EVP_DigestSign;
    funcs->digest_verify_init = (int (*)(void*, void**, const void*, void*, void*))EVP_DigestVerifyInit;
    funcs->digest_verify = (int (*)(void*, const unsigned char*, size_t, const unsigned char*, size_t))EVP_DigestVerify;
    
    // Key serialization
    funcs->i2d_pubkey = (int (*)(void*, unsigned char**))i2d_PUBKEY;
    funcs->i2d_privatekey = (int (*)(void*, unsigned char**))i2d_PrivateKey;
    funcs->d2i_pubkey = (void* (*)(void**, const unsigned char**, long))d2i_PUBKEY;
    funcs->d2i_privatekey = (void* (*)(int, void**, const unsigned char**, long))d2i_PrivateKey;
    
    JSRT_Debug("JSRT_Crypto_Core: Static OpenSSL functions setup completed");
    return true;
#else
    JSRT_Debug("JSRT_Crypto_Core: Static OpenSSL not available (JSRT_STATIC_OPENSSL not defined)");
    return false;
#endif
}

// Dynamic MD getter functions - needed for dynamic loading
static void* (*dynamic_evp_sha1)(void) = NULL;
static void* (*dynamic_evp_sha256)(void) = NULL;
static void* (*dynamic_evp_sha384)(void) = NULL;
static void* (*dynamic_evp_sha512)(void) = NULL;

// Dynamic cipher getter functions
static void* (*dynamic_evp_aes_128_cbc)(void) = NULL;
static void* (*dynamic_evp_aes_192_cbc)(void) = NULL;
static void* (*dynamic_evp_aes_256_cbc)(void) = NULL;
static void* (*dynamic_evp_aes_128_gcm)(void) = NULL;
static void* (*dynamic_evp_aes_192_gcm)(void) = NULL;
static void* (*dynamic_evp_aes_256_gcm)(void) = NULL;
static void* (*dynamic_evp_aes_128_ctr)(void) = NULL;
static void* (*dynamic_evp_aes_192_ctr)(void) = NULL;
static void* (*dynamic_evp_aes_256_ctr)(void) = NULL;

static const void* dynamic_get_md(jsrt_crypto_algorithm_t alg) {
    switch (alg) {
        case JSRT_CRYPTO_ALG_SHA1:
            return dynamic_evp_sha1 ? dynamic_evp_sha1() : NULL;
        case JSRT_CRYPTO_ALG_SHA256:
            return dynamic_evp_sha256 ? dynamic_evp_sha256() : NULL;
        case JSRT_CRYPTO_ALG_SHA384:
            return dynamic_evp_sha384 ? dynamic_evp_sha384() : NULL;
        case JSRT_CRYPTO_ALG_SHA512:
            return dynamic_evp_sha512 ? dynamic_evp_sha512() : NULL;
        default:
            return NULL;
    }
}

static int dynamic_get_digest_size(jsrt_crypto_algorithm_t alg) {
    // For dynamic loading, use fallback sizes since we can't easily call EVP_MD_size
    switch (alg) {
        case JSRT_CRYPTO_ALG_SHA1: return 20;
        case JSRT_CRYPTO_ALG_SHA256: return 32;
        case JSRT_CRYPTO_ALG_SHA384: return 48;
        case JSRT_CRYPTO_ALG_SHA512: return 64;
        default: return 0;
    }
}

static const void* dynamic_get_cipher(jsrt_symmetric_algorithm_t alg, size_t key_length) {
    switch (alg) {
        case JSRT_SYMMETRIC_AES_CBC:
            switch (key_length) {
                case 16: return dynamic_evp_aes_128_cbc ? dynamic_evp_aes_128_cbc() : NULL;
                case 24: return dynamic_evp_aes_192_cbc ? dynamic_evp_aes_192_cbc() : NULL;
                case 32: return dynamic_evp_aes_256_cbc ? dynamic_evp_aes_256_cbc() : NULL;
            }
            break;
        case JSRT_SYMMETRIC_AES_GCM:
            switch (key_length) {
                case 16: return dynamic_evp_aes_128_gcm ? dynamic_evp_aes_128_gcm() : NULL;
                case 24: return dynamic_evp_aes_192_gcm ? dynamic_evp_aes_192_gcm() : NULL;
                case 32: return dynamic_evp_aes_256_gcm ? dynamic_evp_aes_256_gcm() : NULL;
            }
            break;
        case JSRT_SYMMETRIC_AES_CTR:
            switch (key_length) {
                case 16: return dynamic_evp_aes_128_ctr ? dynamic_evp_aes_128_ctr() : NULL;
                case 24: return dynamic_evp_aes_192_ctr ? dynamic_evp_aes_192_ctr() : NULL;
                case 32: return dynamic_evp_aes_256_ctr ? dynamic_evp_aes_256_ctr() : NULL;
            }
            break;
    }
    return NULL;
}

bool jsrt_crypto_core_setup_dynamic_funcs(jsrt_crypto_openssl_funcs_t* funcs, void* openssl_handle) {
    if (!funcs || !openssl_handle) {
        JSRT_Debug("JSRT_Crypto_Core: Invalid parameters for dynamic function setup");
        return false;
    }
    
    // Load hash function getters
    dynamic_evp_sha1 = JSRT_DLSYM(openssl_handle, "EVP_sha1");
    dynamic_evp_sha256 = JSRT_DLSYM(openssl_handle, "EVP_sha256");
    dynamic_evp_sha384 = JSRT_DLSYM(openssl_handle, "EVP_sha384");
    dynamic_evp_sha512 = JSRT_DLSYM(openssl_handle, "EVP_sha512");
    
    // Load cipher function getters
    dynamic_evp_aes_128_cbc = JSRT_DLSYM(openssl_handle, "EVP_aes_128_cbc");
    dynamic_evp_aes_192_cbc = JSRT_DLSYM(openssl_handle, "EVP_aes_192_cbc");
    dynamic_evp_aes_256_cbc = JSRT_DLSYM(openssl_handle, "EVP_aes_256_cbc");
    dynamic_evp_aes_128_gcm = JSRT_DLSYM(openssl_handle, "EVP_aes_128_gcm");
    dynamic_evp_aes_192_gcm = JSRT_DLSYM(openssl_handle, "EVP_aes_192_gcm");
    dynamic_evp_aes_256_gcm = JSRT_DLSYM(openssl_handle, "EVP_aes_256_gcm");
    dynamic_evp_aes_128_ctr = JSRT_DLSYM(openssl_handle, "EVP_aes_128_ctr");
    dynamic_evp_aes_192_ctr = JSRT_DLSYM(openssl_handle, "EVP_aes_192_ctr");
    dynamic_evp_aes_256_ctr = JSRT_DLSYM(openssl_handle, "EVP_aes_256_ctr");
    
    // Set up getter functions
    funcs->get_md = dynamic_get_md;
    funcs->get_digest_size = dynamic_get_digest_size;
    funcs->get_cipher = dynamic_get_cipher;
    
    // Digest context functions
    funcs->md_ctx_new = JSRT_DLSYM(openssl_handle, "EVP_MD_CTX_new");
    funcs->md_ctx_free = JSRT_DLSYM(openssl_handle, "EVP_MD_CTX_free");
    funcs->digest_init_ex = JSRT_DLSYM(openssl_handle, "EVP_DigestInit_ex");
    funcs->digest_update = JSRT_DLSYM(openssl_handle, "EVP_DigestUpdate");
    funcs->digest_final_ex = JSRT_DLSYM(openssl_handle, "EVP_DigestFinal_ex");
    
    // Random functions
    funcs->rand_bytes = JSRT_DLSYM(openssl_handle, "RAND_bytes");
    
    // Cipher functions
    funcs->cipher_ctx_new = JSRT_DLSYM(openssl_handle, "EVP_CIPHER_CTX_new");
    funcs->cipher_ctx_free = JSRT_DLSYM(openssl_handle, "EVP_CIPHER_CTX_free");
    funcs->encrypt_init_ex = JSRT_DLSYM(openssl_handle, "EVP_EncryptInit_ex");
    funcs->encrypt_update = JSRT_DLSYM(openssl_handle, "EVP_EncryptUpdate");
    funcs->encrypt_final_ex = JSRT_DLSYM(openssl_handle, "EVP_EncryptFinal_ex");
    funcs->decrypt_init_ex = JSRT_DLSYM(openssl_handle, "EVP_DecryptInit_ex");
    funcs->decrypt_update = JSRT_DLSYM(openssl_handle, "EVP_DecryptUpdate");
    funcs->decrypt_final_ex = JSRT_DLSYM(openssl_handle, "EVP_DecryptFinal_ex");
    funcs->cipher_ctx_ctrl = JSRT_DLSYM(openssl_handle, "EVP_CIPHER_CTX_ctrl");
    
    // RSA functions
    funcs->pkey_new = JSRT_DLSYM(openssl_handle, "EVP_PKEY_new");
    funcs->pkey_free = JSRT_DLSYM(openssl_handle, "EVP_PKEY_free");
    funcs->pkey_ctx_new_id = JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_new_id");
    funcs->pkey_ctx_new = JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_new");
    funcs->pkey_ctx_free = JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_free");
    funcs->pkey_keygen_init = JSRT_DLSYM(openssl_handle, "EVP_PKEY_keygen_init");
    funcs->pkey_keygen = JSRT_DLSYM(openssl_handle, "EVP_PKEY_keygen");
    funcs->pkey_ctx_ctrl = JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_ctrl");
    funcs->pkey_ctx_ctrl_str = JSRT_DLSYM(openssl_handle, "EVP_PKEY_CTX_ctrl_str");
    
    funcs->pkey_encrypt_init = JSRT_DLSYM(openssl_handle, "EVP_PKEY_encrypt_init");
    funcs->pkey_encrypt = JSRT_DLSYM(openssl_handle, "EVP_PKEY_encrypt");
    funcs->pkey_decrypt_init = JSRT_DLSYM(openssl_handle, "EVP_PKEY_decrypt_init");
    funcs->pkey_decrypt = JSRT_DLSYM(openssl_handle, "EVP_PKEY_decrypt");
    funcs->pkey_sign_init = JSRT_DLSYM(openssl_handle, "EVP_PKEY_sign_init");
    funcs->pkey_sign = JSRT_DLSYM(openssl_handle, "EVP_PKEY_sign");
    funcs->pkey_verify_init = JSRT_DLSYM(openssl_handle, "EVP_PKEY_verify_init");
    funcs->pkey_verify = JSRT_DLSYM(openssl_handle, "EVP_PKEY_verify");
    
    // Digest signing/verification
    funcs->digest_sign_init = JSRT_DLSYM(openssl_handle, "EVP_DigestSignInit");
    funcs->digest_sign = JSRT_DLSYM(openssl_handle, "EVP_DigestSign");
    funcs->digest_verify_init = JSRT_DLSYM(openssl_handle, "EVP_DigestVerifyInit");
    funcs->digest_verify = JSRT_DLSYM(openssl_handle, "EVP_DigestVerify");
    
    // Key serialization
    funcs->i2d_pubkey = JSRT_DLSYM(openssl_handle, "i2d_PUBKEY");
    funcs->i2d_privatekey = JSRT_DLSYM(openssl_handle, "i2d_PrivateKey");
    funcs->d2i_pubkey = JSRT_DLSYM(openssl_handle, "d2i_PUBKEY");
    funcs->d2i_privatekey = JSRT_DLSYM(openssl_handle, "d2i_PrivateKey");
    
    // We need to load specific hash and cipher functions dynamically
    // This requires storing the dynamic library handle and implementing getters
    // For now, we'll implement the core algorithms and add the getters later
    
    bool success = funcs->md_ctx_new && funcs->digest_init_ex && funcs->digest_update && 
                   funcs->digest_final_ex && funcs->rand_bytes && funcs->cipher_ctx_new;
                   
    if (success) {
        JSRT_Debug("JSRT_Crypto_Core: Dynamic OpenSSL functions setup completed");
    } else {
        JSRT_Debug("JSRT_Crypto_Core: Failed to setup some dynamic OpenSSL functions");
    }
    
    return success;
}

// =============================================================================
// Core Algorithm Implementations
// =============================================================================

int jsrt_crypto_core_digest(const jsrt_crypto_openssl_funcs_t* funcs, 
                           jsrt_crypto_algorithm_t alg, 
                           const uint8_t* input, size_t input_len,
                           uint8_t** output, size_t* output_len) {
    
    if (!funcs || !funcs->get_md || !funcs->md_ctx_new || !funcs->digest_init_ex ||
        !funcs->digest_update || !funcs->digest_final_ex) {
        JSRT_Debug("JSRT_Crypto_Core: Required digest functions not available");
        return -1;
    }
    
    const void* md = funcs->get_md(alg);
    if (!md) {
        JSRT_Debug("JSRT_Crypto_Core: Unsupported hash algorithm: %d", alg);
        return -1;
    }
    
    // Create digest context
    void* ctx = funcs->md_ctx_new();
    if (!ctx) {
        JSRT_Debug("JSRT_Crypto_Core: Failed to create digest context");
        return -1;
    }
    
    // Initialize digest
    if (funcs->digest_init_ex(ctx, md, NULL) != 1) {
        funcs->md_ctx_free(ctx);
        JSRT_Debug("JSRT_Crypto_Core: Failed to initialize digest");
        return -1;
    }
    
    // Update with input data
    if (funcs->digest_update(ctx, input, input_len) != 1) {
        funcs->md_ctx_free(ctx);
        JSRT_Debug("JSRT_Crypto_Core: Failed to update digest");
        return -1;
    }
    
    // Get digest size and allocate output buffer
    int digest_size = funcs->get_digest_size(alg);
    if (digest_size <= 0) {
        funcs->md_ctx_free(ctx);
        JSRT_Debug("JSRT_Crypto_Core: Invalid digest size");
        return -1;
    }
    
    *output = malloc(digest_size);
    if (!*output) {
        funcs->md_ctx_free(ctx);
        JSRT_Debug("JSRT_Crypto_Core: Failed to allocate output buffer");
        return -1;
    }
    
    // Finalize digest
    unsigned int final_size;
    if (funcs->digest_final_ex(ctx, *output, &final_size) != 1) {
        free(*output);
        *output = NULL;
        funcs->md_ctx_free(ctx);
        JSRT_Debug("JSRT_Crypto_Core: Failed to finalize digest");
        return -1;
    }
    
    *output_len = final_size;
    funcs->md_ctx_free(ctx);
    
    JSRT_Debug("JSRT_Crypto_Core: Successfully computed digest (%zu bytes)", *output_len);
    return 0;
}

int jsrt_crypto_core_generate_aes_key(const jsrt_crypto_openssl_funcs_t* funcs,
                                     size_t key_length_bits, 
                                     uint8_t** key_data, size_t* key_data_length) {
    
    if (!funcs || !funcs->rand_bytes) {
        JSRT_Debug("JSRT_Crypto_Core: Random function not available");
        return -1;
    }
    
    size_t key_bytes = key_length_bits / 8;
    if (key_bytes != 16 && key_bytes != 24 && key_bytes != 32) {
        JSRT_Debug("JSRT_Crypto_Core: Invalid AES key length: %zu bits", key_length_bits);
        return -1;
    }
    
    *key_data = malloc(key_bytes);
    if (!*key_data) {
        JSRT_Debug("JSRT_Crypto_Core: Failed to allocate key buffer");
        return -1;
    }
    
    if (funcs->rand_bytes(*key_data, key_bytes) != 1) {
        free(*key_data);
        *key_data = NULL;
        JSRT_Debug("JSRT_Crypto_Core: Failed to generate random key");
        return -1;
    }
    
    *key_data_length = key_bytes;
    JSRT_Debug("JSRT_Crypto_Core: Successfully generated %zu-bit AES key", key_length_bits);
    return 0;
}

int jsrt_crypto_core_get_random_bytes(const jsrt_crypto_openssl_funcs_t* funcs,
                                     uint8_t* buffer, size_t length) {
    
    if (!funcs || !funcs->rand_bytes) {
        JSRT_Debug("JSRT_Crypto_Core: Random function not available");
        return -1;
    }
    
    if (funcs->rand_bytes(buffer, (int)length) != 1) {
        JSRT_Debug("JSRT_Crypto_Core: Failed to generate random bytes");
        return -1;
    }
    
    return 0;
}

int jsrt_crypto_core_random_uuid(const jsrt_crypto_openssl_funcs_t* funcs,
                                char* uuid_str, size_t uuid_str_size) {
    
    if (uuid_str_size < 37) {  // UUID string length + null terminator
        return -1;
    }
    
    unsigned char random_bytes[16];
    if (jsrt_crypto_core_get_random_bytes(funcs, random_bytes, 16) != 0) {
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

// Additional core algorithm implementations will be added here
// (AES encrypt/decrypt, RSA operations, etc.)
//
// For now, let's implement the basic structure and integrate with the backend