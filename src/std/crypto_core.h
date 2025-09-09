#ifndef JSRT_CRYPTO_CORE_H
#define JSRT_CRYPTO_CORE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "crypto_subtle.h"
#include "crypto_symmetric.h"
#include "crypto_rsa.h"

/**
 * @file crypto_core.h
 * @brief Unified crypto core implementation shared between static and dynamic backends
 * 
 * This file defines the unified interface for crypto operations that can work with
 * both statically linked OpenSSL and dynamically loaded OpenSSL functions.
 */

// Forward declarations
typedef struct jsrt_openssl_funcs jsrt_openssl_funcs_t;

// OpenSSL function wrapper types
typedef struct {
    // Hash functions
    const void* (*get_md)(jsrt_crypto_algorithm_t alg);
    int (*get_digest_size)(jsrt_crypto_algorithm_t alg);
    
    // Digest context functions
    void* (*md_ctx_new)(void);
    void (*md_ctx_free)(void* ctx);
    int (*digest_init_ex)(void* ctx, const void* type, void* impl);
    int (*digest_update)(void* ctx, const void* d, size_t cnt);
    int (*digest_final_ex)(void* ctx, unsigned char* md, unsigned int* s);
    
    // Random functions
    int (*rand_bytes)(unsigned char* buf, int num);
    
    // Cipher functions
    const void* (*get_cipher)(jsrt_symmetric_algorithm_t alg, size_t key_length);
    void* (*cipher_ctx_new)(void);
    void (*cipher_ctx_free)(void* ctx);
    int (*encrypt_init_ex)(void* ctx, const void* cipher, void* impl, const unsigned char* key, const unsigned char* iv);
    int (*encrypt_update)(void* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl);
    int (*encrypt_final_ex)(void* ctx, unsigned char* out, int* outl);
    int (*decrypt_init_ex)(void* ctx, const void* cipher, void* impl, const unsigned char* key, const unsigned char* iv);
    int (*decrypt_update)(void* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl);
    int (*decrypt_final_ex)(void* ctx, unsigned char* out, int* outl);
    int (*cipher_ctx_ctrl)(void* ctx, int type, int arg, void* ptr);
    
    // RSA/Asymmetric crypto functions
    void* (*pkey_new)(void);
    void (*pkey_free)(void* pkey);
    void* (*pkey_ctx_new_id)(int id, void* e);
    void* (*pkey_ctx_new)(void* pkey, void* e);
    void (*pkey_ctx_free)(void* ctx);
    int (*pkey_keygen_init)(void* ctx);
    int (*pkey_keygen)(void* ctx, void** pkey);
    int (*pkey_ctx_ctrl)(void* ctx, int keytype, int optype, int cmd, int p1, void* p2);
    int (*pkey_ctx_ctrl_str)(void* ctx, const char* type, const char* value);
    
    // RSA operations
    int (*pkey_encrypt_init)(void* ctx);
    int (*pkey_encrypt)(void* ctx, unsigned char* out, size_t* outlen, const unsigned char* in, size_t inlen);
    int (*pkey_decrypt_init)(void* ctx);
    int (*pkey_decrypt)(void* ctx, unsigned char* out, size_t* outlen, const unsigned char* in, size_t inlen);
    int (*pkey_sign_init)(void* ctx);
    int (*pkey_sign)(void* ctx, unsigned char* sig, size_t* siglen, const unsigned char* tbs, size_t tbslen);
    int (*pkey_verify_init)(void* ctx);
    int (*pkey_verify)(void* ctx, const unsigned char* sig, size_t siglen, const unsigned char* tbs, size_t tbslen);
    
    // Digest signing/verification
    int (*digest_sign_init)(void* ctx, void** pctx, const void* type, void* e, void* pkey);
    int (*digest_sign)(void* ctx, unsigned char* sigret, size_t* siglen, const unsigned char* tbs, size_t tbslen);
    int (*digest_verify_init)(void* ctx, void** pctx, const void* type, void* e, void* pkey);
    int (*digest_verify)(void* ctx, const unsigned char* sigret, size_t siglen, const unsigned char* tbs, size_t tbslen);
    
    // Key serialization
    int (*i2d_pubkey)(void* a, unsigned char** pp);
    int (*i2d_privatekey)(void* a, unsigned char** pp);
    void* (*d2i_pubkey)(void** a, const unsigned char** pp, long length);
    void* (*d2i_privatekey)(int type, void** a, const unsigned char** pp, long length);
    
} jsrt_crypto_openssl_funcs_t;

// Core crypto operations (algorithm implementations)
// These functions work with both static and dynamic OpenSSL

/**
 * @brief Unified digest operation
 * @param funcs OpenSSL function pointers (static or dynamic)
 * @param alg Hash algorithm to use
 * @param input Input data to hash
 * @param input_len Length of input data
 * @param output Output buffer (allocated by function)
 * @param output_len Length of output data
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_digest(const jsrt_crypto_openssl_funcs_t* funcs, 
                           jsrt_crypto_algorithm_t alg, 
                           const uint8_t* input, size_t input_len,
                           uint8_t** output, size_t* output_len);

/**
 * @brief Unified AES key generation
 * @param funcs OpenSSL function pointers
 * @param key_length_bits Key length in bits (128, 192, or 256)
 * @param key_data Output key data (allocated by function)
 * @param key_data_length Output key length
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_generate_aes_key(const jsrt_crypto_openssl_funcs_t* funcs,
                                     size_t key_length_bits, 
                                     uint8_t** key_data, size_t* key_data_length);

/**
 * @brief Unified AES encryption
 * @param funcs OpenSSL function pointers
 * @param params Encryption parameters (algorithm, key, IV, etc.)
 * @param plaintext Input plaintext
 * @param plaintext_length Length of plaintext
 * @param ciphertext Output ciphertext (allocated by function)
 * @param ciphertext_length Length of ciphertext
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_aes_encrypt(const jsrt_crypto_openssl_funcs_t* funcs,
                                jsrt_symmetric_params_t* params, 
                                const uint8_t* plaintext, size_t plaintext_length,
                                uint8_t** ciphertext, size_t* ciphertext_length);

/**
 * @brief Unified AES decryption
 * @param funcs OpenSSL function pointers
 * @param params Decryption parameters (algorithm, key, IV, etc.)
 * @param ciphertext Input ciphertext
 * @param ciphertext_length Length of ciphertext
 * @param plaintext Output plaintext (allocated by function)
 * @param plaintext_length Length of plaintext
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_aes_decrypt(const jsrt_crypto_openssl_funcs_t* funcs,
                                jsrt_symmetric_params_t* params, 
                                const uint8_t* ciphertext, size_t ciphertext_length,
                                uint8_t** plaintext, size_t* plaintext_length);

/**
 * @brief Unified random number generation
 * @param funcs OpenSSL function pointers
 * @param buffer Output buffer
 * @param length Number of random bytes to generate
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_get_random_bytes(const jsrt_crypto_openssl_funcs_t* funcs,
                                     uint8_t* buffer, size_t length);

/**
 * @brief Unified UUID generation
 * @param funcs OpenSSL function pointers
 * @param uuid_str Output UUID string buffer (must be at least 37 bytes)
 * @param uuid_str_size Size of UUID string buffer
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_random_uuid(const jsrt_crypto_openssl_funcs_t* funcs,
                                char* uuid_str, size_t uuid_str_size);

/**
 * @brief Unified RSA key generation
 * @param funcs OpenSSL function pointers
 * @param modulus_length_bits RSA key size in bits
 * @param public_exponent Public exponent (usually 65537)
 * @param hash_alg Hash algorithm for signatures
 * @param keypair Output key pair (allocated by function)
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_generate_rsa_keypair(const jsrt_crypto_openssl_funcs_t* funcs,
                                         size_t modulus_length_bits, 
                                         uint32_t public_exponent,
                                         jsrt_rsa_hash_algorithm_t hash_alg, 
                                         jsrt_rsa_keypair_t** keypair);

/**
 * @brief Unified RSA encryption
 * @param funcs OpenSSL function pointers
 * @param params RSA encryption parameters
 * @param plaintext Input plaintext
 * @param plaintext_length Length of plaintext
 * @param ciphertext Output ciphertext (allocated by function)
 * @param ciphertext_length Length of ciphertext
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_rsa_encrypt(const jsrt_crypto_openssl_funcs_t* funcs,
                                jsrt_rsa_params_t* params, 
                                const uint8_t* plaintext, size_t plaintext_length,
                                uint8_t** ciphertext, size_t* ciphertext_length);

/**
 * @brief Unified RSA decryption
 * @param funcs OpenSSL function pointers
 * @param params RSA decryption parameters
 * @param ciphertext Input ciphertext
 * @param ciphertext_length Length of ciphertext
 * @param plaintext Output plaintext (allocated by function)
 * @param plaintext_length Length of plaintext
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_rsa_decrypt(const jsrt_crypto_openssl_funcs_t* funcs,
                                jsrt_rsa_params_t* params, 
                                const uint8_t* ciphertext, size_t ciphertext_length,
                                uint8_t** plaintext, size_t* plaintext_length);

/**
 * @brief Unified RSA signing
 * @param funcs OpenSSL function pointers
 * @param params RSA signing parameters
 * @param data Input data to sign
 * @param data_length Length of input data
 * @param signature Output signature (allocated by function)
 * @param signature_length Length of signature
 * @return 0 on success, -1 on error
 */
int jsrt_crypto_core_rsa_sign(const jsrt_crypto_openssl_funcs_t* funcs,
                             jsrt_rsa_params_t* params, 
                             const uint8_t* data, size_t data_length,
                             uint8_t** signature, size_t* signature_length);

/**
 * @brief Unified RSA signature verification
 * @param funcs OpenSSL function pointers
 * @param params RSA verification parameters
 * @param data Original data that was signed
 * @param data_length Length of original data
 * @param signature Signature to verify
 * @param signature_length Length of signature
 * @return true if signature is valid, false otherwise
 */
bool jsrt_crypto_core_rsa_verify(const jsrt_crypto_openssl_funcs_t* funcs,
                                jsrt_rsa_params_t* params, 
                                const uint8_t* data, size_t data_length,
                                const uint8_t* signature, size_t signature_length);

// Function pointer setup helpers

/**
 * @brief Setup OpenSSL function pointers for static linking
 * @param funcs Function pointer structure to fill
 * @return true on success, false on failure
 */
bool jsrt_crypto_core_setup_static_funcs(jsrt_crypto_openssl_funcs_t* funcs);

/**
 * @brief Setup OpenSSL function pointers for dynamic loading
 * @param funcs Function pointer structure to fill
 * @param openssl_handle Dynamic library handle
 * @return true on success, false on failure
 */
bool jsrt_crypto_core_setup_dynamic_funcs(jsrt_crypto_openssl_funcs_t* funcs, void* openssl_handle);

#endif // JSRT_CRYPTO_CORE_H