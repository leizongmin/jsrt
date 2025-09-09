// Unified crypto setup for both static and dynamic OpenSSL builds
#include "crypto.h"
#include "../util/debug.h"

#ifdef JSRT_STATIC_OPENSSL
#include <openssl/opensslv.h>
#include <openssl/rand.h>
// External functions from other crypto modules  
extern JSValue JSRT_CreateSubtleCrypto(JSContext* ctx);
extern void JSRT_SetupSubtleCrypto(JSRT_Runtime* rt);
#else
// Dynamic loading - define global variables for OpenSSL
#ifdef _WIN32
#include <windows.h>
HMODULE openssl_handle = NULL;
#else
#include <dlfcn.h>
void* openssl_handle = NULL;
#endif

// Function pointer for RAND_bytes
int (*openssl_RAND_bytes)(unsigned char *buf, int num) = NULL;

// External functions from other crypto modules  
extern JSValue JSRT_CreateSubtleCrypto(JSContext* ctx);
extern void JSRT_SetupSubtleCrypto(JSRT_Runtime* rt);

// Forward declare the dynamic loading function
static bool load_openssl_dynamic(void);
#endif

// OpenSSL version string
static char* openssl_version = NULL;

#ifndef JSRT_STATIC_OPENSSL
// Dynamic OpenSSL loading implementation
static bool load_openssl_dynamic(void) {
  if (openssl_handle != NULL) {
    return true;  // Already loaded
  }

  // Try different OpenSSL library names based on platform
  // Note: Loading libssl instead of libcrypto because SSL/TLS functionality requires libssl
  // and libssl includes the crypto functions we need (like RAND_bytes)
  const char* openssl_names[] = {
#ifdef _WIN32
    "libssl-3.dll",
    "libssl-1_1.dll", 
    "libssl.dll",
#elif __APPLE__
    "/opt/homebrew/lib/libssl.3.dylib",  // Homebrew OpenSSL 3.x
    "/opt/homebrew/lib/libssl.1.1.dylib", // Homebrew OpenSSL 1.1.x
    "/usr/local/lib/libssl.3.dylib",     // MacPorts OpenSSL 3.x
    "/usr/local/lib/libssl.1.1.dylib",   // MacPorts OpenSSL 1.1.x
    "libssl.3.dylib",
    "libssl.1.1.dylib",
    "libssl.dylib",
#else
    "libssl.so.3",    // Linux OpenSSL 3.x
    "libssl.so.1.1",  // Linux OpenSSL 1.1.x
    "libssl.so",      // Linux generic
#endif
    NULL
  };

  for (int i = 0; openssl_names[i] != NULL; i++) {
#ifdef _WIN32
    openssl_handle = LoadLibraryA(openssl_names[i]);
#else
    openssl_handle = dlopen(openssl_names[i], RTLD_LAZY);
#endif
    if (openssl_handle != NULL) {
      JSRT_Debug("JSRT_Crypto: Successfully loaded OpenSSL from %s", openssl_names[i]);
      break;
    }
  }

  if (openssl_handle == NULL) {
    JSRT_Debug("JSRT_Crypto: Failed to load OpenSSL library");
    return false;
  }

  // Load function pointers
#ifdef _WIN32
  openssl_RAND_bytes = (int (*)(unsigned char*, int))GetProcAddress(openssl_handle, "RAND_bytes");
#else
  openssl_RAND_bytes = (int (*)(unsigned char*, int))dlsym(openssl_handle, "RAND_bytes");
#endif

  if (openssl_RAND_bytes == NULL) {
    JSRT_Debug("JSRT_Crypto: Failed to load RAND_bytes function");
#ifdef _WIN32
    FreeLibrary(openssl_handle);
#else
    dlclose(openssl_handle);
#endif
    openssl_handle = NULL;
    return false;
  }

  JSRT_Debug("JSRT_Crypto: Dynamic OpenSSL loaded successfully");
  return true;
}
#endif

// Initialize OpenSSL and get version
static bool load_openssl(void) {
#ifdef JSRT_STATIC_OPENSSL
  // Static linking - OpenSSL is always available
  if (openssl_version == NULL) {
    openssl_version = strdup(OPENSSL_VERSION_TEXT);
    JSRT_Debug("JSRT_Crypto: OpenSSL version (static): %s", openssl_version);
  }
  return true;
#else
  // Dynamic loading - load OpenSSL
  return load_openssl_dynamic();
#endif
}

// Get OpenSSL version for process.versions.openssl
const char* JSRT_GetOpenSSLVersion() {
  if (openssl_version != NULL) {
    return openssl_version;
  }
#ifdef JSRT_STATIC_OPENSSL
  return OPENSSL_VERSION_TEXT;
#else
  return NULL;  // Not available for dynamic builds without loading
#endif
}

// Main crypto setup function
void JSRT_RuntimeSetupStdCrypto(JSRT_Runtime* rt) {
  bool openssl_loaded = load_openssl();

  if (!openssl_loaded) {
    JSRT_Debug("JSRT_RuntimeSetupStdCrypto: OpenSSL not available, crypto API not registered");
    return;
  }

  // Create crypto object
  JSValue crypto_obj = JS_NewObject(rt->ctx);

  // crypto.getRandomValues() - using unified implementation
  JS_SetPropertyStr(rt->ctx, crypto_obj, "getRandomValues",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_getRandomValues, "getRandomValues", 1));

  // crypto.randomUUID() - using unified implementation
  JS_SetPropertyStr(rt->ctx, crypto_obj, "randomUUID",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_randomUUID, "randomUUID", 0));

  // crypto.subtle (SubtleCrypto API)
  JSValue subtle_obj = JSRT_CreateSubtleCrypto(rt->ctx);
  JS_SetPropertyStr(rt->ctx, crypto_obj, "subtle", subtle_obj);

  // Register crypto object to globalThis
  JS_SetPropertyStr(rt->ctx, rt->global, "crypto", crypto_obj);

  // Initialize SubtleCrypto (handles platform-specific setup)
  JSRT_SetupSubtleCrypto(rt);

  JSRT_Debug("JSRT_RuntimeSetupStdCrypto: initialized WebCrypto API with OpenSSL support");
}