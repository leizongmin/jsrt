#include "crypto_backend.h"
#include "../util/debug.h"

// Global backend instance
jsrt_crypto_backend_t* g_crypto_backend = NULL;

// Backend initialization
bool jsrt_crypto_backend_init(jsrt_crypto_backend_type_t type) {
  if (g_crypto_backend) {
    JSRT_Debug("Crypto backend already initialized");
    return true;
  }

  switch (type) {
    case JSRT_CRYPTO_BACKEND_DYNAMIC:
#ifdef JSRT_STATIC_OPENSSL
      JSRT_Debug("Dynamic backend not available in static OpenSSL builds");
      return false;
#else
      g_crypto_backend = jsrt_crypto_backend_create_dynamic();
#endif
      break;
    case JSRT_CRYPTO_BACKEND_STATIC:
#ifdef JSRT_STATIC_OPENSSL
      g_crypto_backend = jsrt_crypto_backend_create_static();
#else
      JSRT_Debug("Static backend not available without JSRT_STATIC_OPENSSL");
      return false;
#endif
      break;
    default:
      JSRT_Debug("Unknown crypto backend type: %d", type);
      return false;
  }

  if (!g_crypto_backend) {
    JSRT_Debug("Failed to create crypto backend");
    return false;
  }

  if (!g_crypto_backend->init()) {
    JSRT_Debug("Failed to initialize crypto backend");
    free(g_crypto_backend);
    g_crypto_backend = NULL;
    return false;
  }

  JSRT_Debug("Crypto backend initialized successfully (type: %s)", 
             type == JSRT_CRYPTO_BACKEND_DYNAMIC ? "dynamic" : "static");
  return true;
}

// Backend cleanup
void jsrt_crypto_backend_cleanup(void) {
  if (g_crypto_backend) {
    g_crypto_backend->cleanup();
    free(g_crypto_backend);
    g_crypto_backend = NULL;
  }
}

// Unified crypto operations (wrapper functions)
int jsrt_crypto_unified_digest(jsrt_crypto_algorithm_t alg, const uint8_t* input, size_t input_len,
                              uint8_t** output, size_t* output_len) {
  if (!g_crypto_backend || !g_crypto_backend->digest) {
    return -1;
  }
  return g_crypto_backend->digest(alg, input, input_len, output, output_len);
}

int jsrt_crypto_unified_generate_aes_key(size_t key_length_bits, uint8_t** key_data, size_t* key_data_length) {
  if (!g_crypto_backend || !g_crypto_backend->generate_aes_key) {
    return -1;
  }
  return g_crypto_backend->generate_aes_key(key_length_bits, key_data, key_data_length);
}

int jsrt_crypto_unified_aes_encrypt(jsrt_symmetric_params_t* params, const uint8_t* plaintext, size_t plaintext_length,
                                   uint8_t** ciphertext, size_t* ciphertext_length) {
  if (!g_crypto_backend || !g_crypto_backend->aes_encrypt) {
    return -1;
  }
  return g_crypto_backend->aes_encrypt(params, plaintext, plaintext_length, ciphertext, ciphertext_length);
}

int jsrt_crypto_unified_aes_decrypt(jsrt_symmetric_params_t* params, const uint8_t* ciphertext, size_t ciphertext_length,
                                   uint8_t** plaintext, size_t* plaintext_length) {
  if (!g_crypto_backend || !g_crypto_backend->aes_decrypt) {
    return -1;
  }
  return g_crypto_backend->aes_decrypt(params, ciphertext, ciphertext_length, plaintext, plaintext_length);
}

int jsrt_crypto_unified_get_random_bytes(uint8_t* buffer, size_t length) {
  if (!g_crypto_backend || !g_crypto_backend->get_random_bytes) {
    return -1;
  }
  return g_crypto_backend->get_random_bytes(buffer, length);
}

int jsrt_crypto_unified_random_uuid(char* uuid_str, size_t uuid_str_size) {
  if (!g_crypto_backend || !g_crypto_backend->random_uuid) {
    return -1;
  }
  return g_crypto_backend->random_uuid(uuid_str, uuid_str_size);
}

const char* jsrt_crypto_unified_get_version(void) {
  if (!g_crypto_backend || !g_crypto_backend->get_version) {
    return "unknown";
  }
  return g_crypto_backend->get_version();
}