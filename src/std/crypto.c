#include "crypto.h"

#ifdef JSRT_STATIC_OPENSSL
// Static OpenSSL mode - use statically linked OpenSSL functions
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../util/debug.h"
#include "crypto_subtle.h"

static char* openssl_version = NULL;

// Static linking initialization
static bool load_openssl(void) {
  // For static linking, OpenSSL is always available
  if (openssl_version == NULL) {
    openssl_version = strdup(OPENSSL_VERSION_TEXT);
    JSRT_Debug("JSRT_Crypto: OpenSSL version (static): %s", openssl_version);
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

#else
// Dynamic OpenSSL mode - use dynamically loaded OpenSSL functions

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

#ifdef _WIN32
// Windows-specific helper function for better OpenSSL library loading
static HMODULE try_load_openssl_windows_enhanced() {
  // MSYS2 OpenSSL crypto library names (RAND_bytes is in libcrypto, not libssl)
  const char* dll_names[] = {"libcrypto-3-x64.dll",  // OpenSSL 3.x x64 crypto from MSYS2 (prioritized)
                             "libcrypto-3.dll",      // OpenSSL 3.x crypto from MSYS2 (most common)
                             "libcrypto-1_1.dll",    // OpenSSL 1.1.x crypto from MSYS2
                             "libeay32.dll",         // Legacy OpenSSL crypto
                             "libcrypto.dll",        // Generic crypto naming
                             NULL};

  HMODULE handle = NULL;

  // Method 1: Try loading using system DLL search order (includes PATH)
  for (int i = 0; dll_names[i] != NULL; i++) {
    JSRT_Debug("JSRT_Crypto: Attempting to load %s using system search", dll_names[i]);
    handle = LoadLibraryA(dll_names[i]);
    if (handle != NULL) {
      JSRT_Debug("JSRT_Crypto: Successfully loaded %s via system search", dll_names[i]);
      return handle;
    }
    DWORD error = GetLastError();
    JSRT_Debug("JSRT_Crypto: Failed to load %s: Error %lu", dll_names[i], error);
  }

  // Method 2: Try specific MSYS2 paths (DLL files are in bin directories)
  const char* msys2_paths[] = {"C:\\msys64\\ucrt64\\bin\\", "C:\\msys64\\mingw64\\bin\\", "C:\\msys64\\usr\\bin\\",
                               NULL};

  for (int i = 0; msys2_paths[i] != NULL; i++) {
    for (int j = 0; dll_names[j] != NULL; j++) {
      char full_path[MAX_PATH];
      snprintf(full_path, sizeof(full_path), "%s%s", msys2_paths[i], dll_names[j]);

      JSRT_Debug("JSRT_Crypto: Attempting to load from path: %s", full_path);
      handle = LoadLibraryA(full_path);
      if (handle != NULL) {
        JSRT_Debug("JSRT_Crypto: Successfully loaded OpenSSL from %s", full_path);
        return handle;
      }
      DWORD error = GetLastError();
      JSRT_Debug("JSRT_Crypto: Failed to load %s: Error %lu", full_path, error);
    }
  }

  // Method 3: Search PATH directories manually for better diagnostics
  char* path_env = getenv("PATH");
  if (path_env != NULL) {
    JSRT_Debug("JSRT_Crypto: Searching PATH for OpenSSL libraries");
    char* path_copy = _strdup(path_env);
    if (path_copy != NULL) {
      char* token = strtok(path_copy, ";");
      while (token != NULL) {
        for (int j = 0; dll_names[j] != NULL; j++) {
          char full_path[MAX_PATH];
          snprintf(full_path, sizeof(full_path), "%s\\%s", token, dll_names[j]);

          // Check if file exists before trying to load
          DWORD attrs = GetFileAttributesA(full_path);
          if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            JSRT_Debug("JSRT_Crypto: Found %s in PATH, attempting to load", full_path);
            handle = LoadLibraryA(full_path);
            if (handle != NULL) {
              JSRT_Debug("JSRT_Crypto: Successfully loaded OpenSSL from PATH: %s", full_path);
              free(path_copy);
              return handle;
            }
          }
        }
        token = strtok(NULL, ";");
      }
      free(path_copy);
    }
  }

  return NULL;
}
#endif

// Try to load OpenSSL dynamically
static bool load_openssl() {
  if (openssl_handle != NULL) {
    return true;  // Already loaded
  }

  // Try different OpenSSL library names based on platform
  const char* openssl_names[] = {
#ifdef _WIN32
      // For Windows, we'll use the enhanced loading function
      // These names are here for reference but won't be used in the loop
      "libcrypto-3.dll",    // MSYS2 OpenSSL 3.x crypto (most common)
      "libcrypto-1_1.dll",  // MSYS2 OpenSSL 1.1.x crypto
      "libeay32.dll",       // Windows legacy OpenSSL crypto
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

#ifdef _WIN32
  // Use enhanced Windows-specific loading strategy
  openssl_handle = try_load_openssl_windows_enhanced();
#else
  // Unix/macOS loading
  for (int i = 0; openssl_names[i] != NULL; i++) {
    openssl_handle = dlopen(openssl_names[i], RTLD_LAZY);
    if (openssl_handle != NULL) {
      JSRT_Debug("JSRT_Crypto: Successfully loaded OpenSSL from %s", openssl_names[i]);
      break;
    }
  }
#endif

  if (openssl_handle == NULL) {
#ifdef _WIN32
    DWORD error = GetLastError();
    JSRT_Debug("JSRT_Crypto: Failed to load OpenSSL library: Error %lu", error);
    // Enhanced error reporting for Windows
    fprintf(stderr, "JSRT: OpenSSL library not found on Windows.\n");
    fprintf(stderr, "JSRT: Searched for: libcrypto-3-x64.dll, libcrypto-3.dll, libcrypto-1_1.dll, libeay32.dll\n");
    fprintf(stderr, "JSRT: Searched in: system PATH, C:\\msys64\\ucrt64\\bin\\, C:\\msys64\\mingw64\\bin\\\n");
    fprintf(stderr, "JSRT: Install with: pacman -S mingw-w64-ucrt-x86_64-openssl\n");
    fprintf(stderr, "JSRT: Or ensure OpenSSL DLLs are in PATH or application directory\n");
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

// Helper function to check if a value is a valid integer TypedArray for getRandomValues
static bool is_valid_integer_typed_array(JSContext* ctx, JSValue arg, const char** error_msg) {
  if (!JS_IsObject(arg)) {
    *error_msg = "Argument must be a typed array";
    return false;
  }

  // First, check if it has the basic TypedArray properties (byteLength, buffer, etc.)
  JSValue byteLength_val = JS_GetPropertyStr(ctx, arg, "byteLength");
  JSValue buffer_val = JS_GetPropertyStr(ctx, arg, "buffer");

  if (JS_IsException(byteLength_val) || JS_IsException(buffer_val) || JS_IsUndefined(byteLength_val) ||
      JS_IsUndefined(buffer_val)) {
    JS_FreeValue(ctx, byteLength_val);
    JS_FreeValue(ctx, buffer_val);
    *error_msg = "Argument must be a typed array";
    return false;
  }

  JS_FreeValue(ctx, byteLength_val);
  JS_FreeValue(ctx, buffer_val);

  // Get the global object to access TypedArray constructors
  JSValue global = JS_GetGlobalObject(ctx);

  // Check if it's an instance of allowed integer TypedArray types
  // We need to check instanceof for each allowed type
  const char* allowed_types[] = {"Int8Array",         "Int16Array",  "Int32Array",  "BigInt64Array", "Uint8Array",
                                 "Uint8ClampedArray", "Uint16Array", "Uint32Array", "BigUint64Array"};

  bool is_valid_integer = false;

  for (size_t i = 0; i < sizeof(allowed_types) / sizeof(allowed_types[0]); i++) {
    JSValue ctor = JS_GetPropertyStr(ctx, global, allowed_types[i]);
    if (!JS_IsException(ctor) && !JS_IsUndefined(ctor)) {
      // Check instanceof
      int is_instance = JS_IsInstanceOf(ctx, arg, ctor);
      JS_FreeValue(ctx, ctor);
      if (is_instance > 0) {
        is_valid_integer = true;
        break;
      }
      if (is_instance < 0) {
        // Error occurred, continue checking other types
        JS_GetException(ctx);  // Clear the exception
      }
    } else {
      JS_FreeValue(ctx, ctor);
    }
  }

  // If not a valid integer array, check if it's a forbidden float array or DataView
  if (!is_valid_integer) {
    const char* forbidden_types[] = {"Float16Array", "Float32Array", "Float64Array", "DataView"};

    bool is_forbidden = false;
    for (size_t i = 0; i < sizeof(forbidden_types) / sizeof(forbidden_types[0]); i++) {
      JSValue ctor = JS_GetPropertyStr(ctx, global, forbidden_types[i]);
      if (!JS_IsException(ctor) && !JS_IsUndefined(ctor)) {
        int is_instance = JS_IsInstanceOf(ctx, arg, ctor);
        JS_FreeValue(ctx, ctor);
        if (is_instance > 0) {
          is_forbidden = true;
          break;
        }
        if (is_instance < 0) {
          JS_GetException(ctx);  // Clear the exception
        }
      } else {
        JS_FreeValue(ctx, ctor);
      }
    }

    if (is_forbidden) {
      JS_FreeValue(ctx, global);
      *error_msg = "TypeMismatchError";
      return false;
    }
  }

  JS_FreeValue(ctx, global);

  if (!is_valid_integer) {
    *error_msg = "Argument must be a typed array";
    return false;
  }

  return true;
}

// crypto.getRandomValues(typedArray)
static JSValue jsrt_crypto_getRandomValues(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "crypto.getRandomValues requires 1 argument");
  }

  JSValue arg = argv[0];

  // Validate that it's a proper integer TypedArray
  const char* error_msg;
  if (!is_valid_integer_typed_array(ctx, arg, &error_msg)) {
    if (strcmp(error_msg, "TypeMismatchError") == 0) {
      // For WPT compliance, throw DOMException with TypeMismatchError
      // Create a DOMException with name "TypeMismatchError"
      JSValue dom_exception_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "DOMException");
      if (!JS_IsException(dom_exception_ctor)) {
        JSValue args[2];
        args[0] = JS_NewString(ctx, "The operation is not supported");
        args[1] = JS_NewString(ctx, "TypeMismatchError");
        JSValue exception = JS_CallConstructor(ctx, dom_exception_ctor, 2, args);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, dom_exception_ctor);
        if (!JS_IsException(exception)) {
          JS_Throw(ctx, exception);
          return JS_EXCEPTION;
        }
      }
      // Fallback if DOMException not available
      return JS_ThrowTypeError(ctx, "The operation is not supported");
    } else {
      return JS_ThrowTypeError(ctx, error_msg);
    }
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
#ifdef JSRT_STATIC_OPENSSL
  // Use statically linked RAND_bytes
  success = (RAND_bytes(random_data, (int)byte_length) == 1);
#else
  // Use dynamically loaded RAND_bytes
  if (openssl_RAND_bytes != NULL) {
    success = (openssl_RAND_bytes(random_data, (int)byte_length) == 1);
  }
#endif

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
#ifdef JSRT_STATIC_OPENSSL
  // Use statically linked RAND_bytes
  success = (RAND_bytes(random_bytes, 16) == 1);
#else
  // Use dynamically loaded RAND_bytes
  if (openssl_RAND_bytes != NULL) {
    success = (openssl_RAND_bytes(random_bytes, 16) == 1);
  }
#endif

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

#endif