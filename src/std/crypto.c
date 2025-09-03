#include "crypto.h"

// Platform-specific includes for dynamic loading
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../util/debug.h"
#include "crypto_subtle.h"

// OpenSSL function pointers - make openssl_handle accessible to other modules
#ifdef _WIN32
HMODULE openssl_handle = NULL;
#else
void* openssl_handle = NULL;
#endif
static char* openssl_version = NULL;

// Function pointer types for OpenSSL functions we need
typedef int (*RAND_bytes_func)(unsigned char* buf, int num);
typedef const char* (*OpenSSL_version_func)(int type);

// Function pointers
static RAND_bytes_func openssl_RAND_bytes = NULL;
static OpenSSL_version_func openssl_OpenSSL_version = NULL;

// OpenSSL version constants
#define OPENSSL_VERSION 0

// Try to load OpenSSL dynamically
static bool load_openssl() {
  if (openssl_handle != NULL) {
    return true;  // Already loaded
  }

  // Try different OpenSSL library names based on platform
  const char* openssl_names[] = {
#ifdef _WIN32
      "libssl-3-x64.dll",    // Windows OpenSSL 3.x 64-bit
      "libssl-1_1-x64.dll",  // Windows OpenSSL 1.1.x 64-bit
      "libssl-3.dll",        // Windows OpenSSL 3.x
      "libssl-1_1.dll",      // Windows OpenSSL 1.1.x
      "ssleay32.dll",        // Windows legacy OpenSSL
#elif __APPLE__
      "/opt/homebrew/lib/libssl.3.dylib",  // macOS Homebrew OpenSSL 3.x (full path)
      "/opt/homebrew/lib/libssl.dylib",    // macOS Homebrew OpenSSL (full path)
      "/usr/local/lib/libssl.3.dylib",     // macOS local install OpenSSL 3.x
      "/usr/local/lib/libssl.dylib",       // macOS local install OpenSSL
      "libssl.3.dylib",                    // macOS with Homebrew OpenSSL 3.x
      "libssl.1.1.dylib",                  // macOS with Homebrew OpenSSL 1.1.x
      "libssl.dylib",                      // macOS system OpenSSL
#else
      "libssl.so.3",    // Linux OpenSSL 3.x
      "libssl.so.1.1",  // Linux OpenSSL 1.1.x
      "libssl.so",      // Linux generic
#endif
      NULL};

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
#ifdef _WIN32
    JSRT_Debug("JSRT_Crypto: Failed to load OpenSSL library: Error %lu", GetLastError());
#else
    JSRT_Debug("JSRT_Crypto: Failed to load OpenSSL library: %s", dlerror());
#endif
    return false;
  }

  // Load required functions
#ifdef _WIN32
  openssl_RAND_bytes = (RAND_bytes_func)GetProcAddress(openssl_handle, "RAND_bytes");
  openssl_OpenSSL_version = (OpenSSL_version_func)GetProcAddress(openssl_handle, "OpenSSL_version");
#else
  openssl_RAND_bytes = (RAND_bytes_func)dlsym(openssl_handle, "RAND_bytes");
  openssl_OpenSSL_version = (OpenSSL_version_func)dlsym(openssl_handle, "OpenSSL_version");
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

  if (openssl_OpenSSL_version != NULL) {
    const char* version_str = openssl_OpenSSL_version(OPENSSL_VERSION);
    if (version_str) {
      openssl_version = strdup(version_str);
      JSRT_Debug("JSRT_Crypto: OpenSSL version: %s", openssl_version);
    }
  }

  return true;
}

// Fallback random bytes using system random (not cryptographically secure)
static bool fallback_random_bytes(unsigned char* buf, int num) {
  static bool seeded = false;
  if (!seeded) {
    srand((unsigned int)time(NULL));
    seeded = true;
  }

  for (int i = 0; i < num; i++) {
    buf[i] = (unsigned char)(rand() % 256);
  }
  return true;
}

// crypto.getRandomValues(typedArray)
static JSValue jsrt_crypto_getRandomValues(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "crypto.getRandomValues requires 1 argument");
  }

  JSValue arg = argv[0];

  // Check if it's a typed array object
  if (!JS_IsObject(arg)) {
    return JS_ThrowTypeError(ctx, "crypto.getRandomValues argument must be a typed array");
  }

  // Get byteLength property to determine the size
  JSValue byteLength_val = JS_GetPropertyStr(ctx, arg, "byteLength");
  if (JS_IsException(byteLength_val)) {
    return JS_ThrowTypeError(ctx, "crypto.getRandomValues argument must be a typed array");
  }

  uint32_t byte_length;
  if (JS_ToUint32(ctx, &byte_length, byteLength_val) < 0) {
    JS_FreeValue(ctx, byteLength_val);
    return JS_ThrowTypeError(ctx, "Invalid byteLength");
  }
  JS_FreeValue(ctx, byteLength_val);

  if (byte_length == 0) {
    return JS_DupValue(ctx, arg);  // Return the original array
  }

  if (byte_length > 65536) {
    return JS_ThrowRangeError(ctx, "crypto.getRandomValues array length exceeds quota (65536 bytes)");
  }

  // Generate random bytes
  unsigned char* random_data = malloc(byte_length);
  if (!random_data) {
    return JS_ThrowInternalError(ctx, "Failed to allocate memory for random data");
  }

  bool success = false;
  if (openssl_RAND_bytes != NULL) {
    success = (openssl_RAND_bytes(random_data, (int)byte_length) == 1);
  }

  if (!success) {
    // Use fallback random (warn that it's not cryptographically secure)
    JSRT_Debug("JSRT_Crypto: Using fallback random number generator (not cryptographically secure)");
    fallback_random_bytes(random_data, (int)byte_length);
  }

  // Copy random data to the typed array by setting each element
  for (uint32_t i = 0; i < byte_length; i++) {
    JSValue byte_val = JS_NewUint32(ctx, random_data[i]);
    JS_SetPropertyUint32(ctx, arg, i, byte_val);
  }

  free(random_data);
  return JS_DupValue(ctx, arg);
}

// crypto.randomUUID()
static JSValue jsrt_crypto_randomUUID(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  unsigned char random_bytes[16];
  bool success = false;

  // Generate 16 random bytes for UUID
  if (openssl_RAND_bytes != NULL) {
    success = (openssl_RAND_bytes(random_bytes, 16) == 1);
  }

  if (!success) {
    // Use fallback random
    JSRT_Debug("JSRT_Crypto: Using fallback random number generator for UUID (not cryptographically secure)");
    fallback_random_bytes(random_bytes, 16);
  }

  // Set version bits (4 bits): version 4 (random)
  random_bytes[6] = (random_bytes[6] & 0x0F) | 0x40;

  // Set variant bits (2 bits): variant 1 (RFC 4122)
  random_bytes[8] = (random_bytes[8] & 0x3F) | 0x80;

  // Format as UUID string: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
  char uuid_str[37];
  snprintf(uuid_str, sizeof(uuid_str), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           random_bytes[0], random_bytes[1], random_bytes[2], random_bytes[3], random_bytes[4], random_bytes[5],
           random_bytes[6], random_bytes[7], random_bytes[8], random_bytes[9], random_bytes[10], random_bytes[11],
           random_bytes[12], random_bytes[13], random_bytes[14], random_bytes[15]);

  return JS_NewString(ctx, uuid_str);
}

// Get OpenSSL version for process.versions.openssl
const char* JSRT_GetOpenSSLVersion() {
  if (openssl_version != NULL) {
    return openssl_version;
  }
  return NULL;
}

// Setup crypto module in runtime
void JSRT_RuntimeSetupStdCrypto(JSRT_Runtime* rt) {
  bool openssl_loaded = load_openssl();

  if (!openssl_loaded) {
    JSRT_Debug("JSRT_RuntimeSetupStdCrypto: OpenSSL not available, crypto API not registered");
    return;
  }

  // Create crypto object
  JSValue crypto_obj = JS_NewObject(rt->ctx);

  // crypto.getRandomValues()
  JS_SetPropertyStr(rt->ctx, crypto_obj, "getRandomValues",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_getRandomValues, "getRandomValues", 1));

  // crypto.randomUUID()
  JS_SetPropertyStr(rt->ctx, crypto_obj, "randomUUID",
                    JS_NewCFunction(rt->ctx, jsrt_crypto_randomUUID, "randomUUID", 0));

  // crypto.subtle (SubtleCrypto API)
  JSValue subtle_obj = JSRT_CreateSubtleCrypto(rt->ctx);
  JS_SetPropertyStr(rt->ctx, crypto_obj, "subtle", subtle_obj);

  // Register crypto object to globalThis
  JS_SetPropertyStr(rt->ctx, rt->global, "crypto", crypto_obj);

  // Initialize SubtleCrypto
  JSRT_SetupSubtleCrypto(rt);

  JSRT_Debug("JSRT_RuntimeSetupStdCrypto: initialized WebCrypto API with OpenSSL support");
}