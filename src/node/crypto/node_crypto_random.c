#include "node_crypto_internal.h"

//==============================================================================
// Random Functions (Node.js randomBytes, randomUUID)
//==============================================================================

// crypto.randomBytes(size[, callback])
JSValue js_crypto_random_bytes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "crypto.randomBytes() requires size argument");
  }

  int32_t size;
  if (JS_ToInt32(ctx, &size, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  if (size < 0) {
    return JS_ThrowRangeError(ctx, "Size must be non-negative");
  }

  if (size > 65536) {  // 64KB limit for safety
    return JS_ThrowRangeError(ctx, "Size too large");
  }

  // Use WebCrypto getRandomValues if available
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue crypto_obj = JS_GetPropertyStr(ctx, global, "crypto");
  JS_FreeValue(ctx, global);

  if (!JS_IsUndefined(crypto_obj)) {
    JSValue get_random_values = JS_GetPropertyStr(ctx, crypto_obj, "getRandomValues");
    if (JS_IsFunction(ctx, get_random_values)) {
      // Create Uint8Array for WebCrypto
      JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
      JSValue array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &argv[0]);

      if (!JS_IsException(array)) {
        // Fill with random data using WebCrypto
        JSValue result = JS_Call(ctx, get_random_values, crypto_obj, 1, &array);

        if (!JS_IsException(result)) {
          // The result is already a Uint8Array, which is what we want for Buffer
          JS_FreeValue(ctx, array);
          JS_FreeValue(ctx, uint8_array_ctor);
          JS_FreeValue(ctx, get_random_values);
          JS_FreeValue(ctx, crypto_obj);

          return result;
        }
        JS_FreeValue(ctx, result);
      }
      JS_FreeValue(ctx, array);
      JS_FreeValue(ctx, uint8_array_ctor);
    }
    JS_FreeValue(ctx, get_random_values);
  }
  JS_FreeValue(ctx, crypto_obj);

  // Fallback: simple pseudo-random implementation
  uint8_t* data = malloc(size);
  if (!data) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Simple PRNG - not cryptographically secure, just for basic functionality
  for (int32_t i = 0; i < size; i++) {
    data[i] = rand() & 0xFF;
  }

  // Create ArrayBuffer and then Uint8Array from data
  JSValue array_buffer = JS_NewArrayBuffer(ctx, data, size, NULL, NULL, false);

  if (JS_IsException(array_buffer)) {
    free(data);
    return array_buffer;
  }

  // Create Uint8Array from ArrayBuffer (this is our Buffer format)
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global_obj, "Uint8Array");
  JSValue uint8_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, global_obj);
  JS_FreeValue(ctx, array_buffer);

  return uint8_array;
}

// crypto.randomUUID()
JSValue js_crypto_random_uuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Generate a RFC 4122 version 4 UUID
  uint8_t bytes[16];

  // Fill with random data (fallback implementation)
  for (int i = 0; i < 16; i++) {
    bytes[i] = rand() & 0xFF;
  }

  // Set version to 4
  bytes[6] = (bytes[6] & 0x0f) | 0x40;
  // Set variant
  bytes[8] = (bytes[8] & 0x3f) | 0x80;

  // Format as UUID string
  char uuid_str[37];
  snprintf(uuid_str, sizeof(uuid_str), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", bytes[0],
           bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7], bytes[8], bytes[9], bytes[10],
           bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);

  return JS_NewString(ctx, uuid_str);
}
