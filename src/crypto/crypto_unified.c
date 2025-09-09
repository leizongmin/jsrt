// Unified crypto functions that work for both static and dynamic OpenSSL builds
#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef JSRT_STATIC_OPENSSL
#include <openssl/rand.h>
#endif

#include "../util/debug.h"
#include "crypto.h"

// Platform-specific includes for dynamic loading (dynamic builds only)
#ifndef JSRT_STATIC_OPENSSL
#ifdef _WIN32
#include <windows.h>
typedef HMODULE jsrt_handle_t;
#else
#include <dlfcn.h>
typedef void* jsrt_handle_t;
#endif

// External reference to dynamic OpenSSL function pointers
extern int (*openssl_RAND_bytes)(unsigned char* buf, int num);
#endif

// Fallback random number generator for when OpenSSL is not available
static bool fallback_random_bytes(unsigned char* buf, int num) {
  // Use system random or fallback
  FILE* urandom = fopen("/dev/urandom", "rb");
  if (urandom) {
    size_t read_count = fread(buf, 1, num, urandom);
    fclose(urandom);
    return read_count == (size_t)num;
  }

  // Fallback to poor quality random
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
    *error_msg = "Argument must be an integer typed array";
    return false;
  }

  return true;
}

// crypto.getRandomValues(typedArray) - unified implementation for both static and dynamic OpenSSL
JSValue jsrt_crypto_getRandomValues(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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
      return JS_ThrowTypeError(ctx, "%s", error_msg);
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

// crypto.randomUUID() - unified implementation for both static and dynamic OpenSSL
JSValue jsrt_crypto_randomUUID(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  unsigned char random_bytes[16];
  bool success = false;

  // Generate 16 random bytes for UUID
#ifdef JSRT_STATIC_OPENSSL
  success = (RAND_bytes(random_bytes, 16) == 1);
#else
  if (openssl_RAND_bytes != NULL) {
    success = (openssl_RAND_bytes(random_bytes, 16) == 1);
  }
#endif

  if (!success) {
    // Use fallback random (warn that it's not cryptographically secure)
    JSRT_Debug("JSRT_Crypto: Using fallback random number generator (not cryptographically secure)");
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