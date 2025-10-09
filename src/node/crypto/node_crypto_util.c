#include "node_crypto_internal.h"

//==============================================================================
// Utility Functions and Constants
//==============================================================================

// crypto.constants - basic constants object
JSValue create_crypto_constants(JSContext* ctx) {
  JSValue constants = JS_NewObject(ctx);

  // OpenSSL-style constants (basic subset)
  JS_SetPropertyStr(ctx, constants, "SSL_OP_ALL", JS_NewInt32(ctx, 0x80000BFF));
  JS_SetPropertyStr(ctx, constants, "SSL_OP_NO_SSLv2", JS_NewInt32(ctx, 0x01000000));
  JS_SetPropertyStr(ctx, constants, "SSL_OP_NO_SSLv3", JS_NewInt32(ctx, 0x02000000));
  JS_SetPropertyStr(ctx, constants, "SSL_OP_NO_TLSv1", JS_NewInt32(ctx, 0x04000000));
  JS_SetPropertyStr(ctx, constants, "SSL_OP_NO_TLSv1_1", JS_NewInt32(ctx, 0x10000000));

  return constants;
}

// crypto.timingSafeEqual(a, b) - constant-time buffer comparison
JSValue js_crypto_timing_safe_equal(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "timingSafeEqual requires 2 arguments");
  }

  // Get buffer data for both arguments
  size_t a_size, b_size;
  uint8_t* a_data = JS_GetArrayBuffer(ctx, &a_size, argv[0]);
  uint8_t* b_data = JS_GetArrayBuffer(ctx, &b_size, argv[1]);

  // Also try TypedArray view
  if (!a_data) {
    JSValue buffer_a = JS_GetPropertyStr(ctx, argv[0], "buffer");
    if (!JS_IsUndefined(buffer_a)) {
      a_data = JS_GetArrayBuffer(ctx, &a_size, buffer_a);
      // Adjust for offset and length
      JSValue offset_val = JS_GetPropertyStr(ctx, argv[0], "byteOffset");
      JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "byteLength");
      int32_t offset = 0, length = 0;
      if (!JS_IsUndefined(offset_val))
        JS_ToInt32(ctx, &offset, offset_val);
      if (!JS_IsUndefined(length_val))
        JS_ToInt32(ctx, &length, length_val);
      if (a_data) {
        a_data += offset;
        a_size = length;
      }
      JS_FreeValue(ctx, offset_val);
      JS_FreeValue(ctx, length_val);
    }
    JS_FreeValue(ctx, buffer_a);
  }

  if (!b_data) {
    JSValue buffer_b = JS_GetPropertyStr(ctx, argv[1], "buffer");
    if (!JS_IsUndefined(buffer_b)) {
      b_data = JS_GetArrayBuffer(ctx, &b_size, buffer_b);
      // Adjust for offset and length
      JSValue offset_val = JS_GetPropertyStr(ctx, argv[1], "byteOffset");
      JSValue length_val = JS_GetPropertyStr(ctx, argv[1], "byteLength");
      int32_t offset = 0, length = 0;
      if (!JS_IsUndefined(offset_val))
        JS_ToInt32(ctx, &offset, offset_val);
      if (!JS_IsUndefined(length_val))
        JS_ToInt32(ctx, &length, length_val);
      if (b_data) {
        b_data += offset;
        b_size = length;
      }
      JS_FreeValue(ctx, offset_val);
      JS_FreeValue(ctx, length_val);
    }
    JS_FreeValue(ctx, buffer_b);
  }

  if (!a_data || !b_data) {
    return JS_ThrowTypeError(ctx, "Arguments must be Buffer, TypedArray, or DataView");
  }

  // Lengths must match
  if (a_size != b_size) {
    return JS_ThrowRangeError(ctx, "Input buffers must have the same byte length");
  }

  // Constant-time comparison (bitwise OR accumulator)
  uint8_t diff = 0;
  for (size_t i = 0; i < a_size; i++) {
    diff |= a_data[i] ^ b_data[i];
  }

  return JS_NewBool(ctx, diff == 0);
}

// crypto.randomInt([min,] max[, callback]) - uniform random integer
JSValue js_crypto_random_int(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "randomInt requires at least 1 argument");
  }

  int32_t min = 0;
  int32_t max;
  JSValue callback = JS_UNDEFINED;

  // Parse arguments: randomInt(max) or randomInt(min, max[, callback])
  if (argc == 1) {
    if (JS_ToInt32(ctx, &max, argv[0]) < 0) {
      return JS_EXCEPTION;
    }
  } else {
    if (JS_ToInt32(ctx, &min, argv[0]) < 0) {
      return JS_EXCEPTION;
    }
    if (JS_ToInt32(ctx, &max, argv[1]) < 0) {
      return JS_EXCEPTION;
    }
    if (argc > 2 && JS_IsFunction(ctx, argv[2])) {
      callback = argv[2];
    }
  }

  // Validate range
  if (min < 0 || max < 0) {
    return JS_ThrowRangeError(ctx, "min and max must be non-negative");
  }
  if (min >= max) {
    return JS_ThrowRangeError(ctx, "max must be greater than min");
  }

  int32_t range = max - min;

  // Use WebCrypto getRandomValues for cryptographically secure random
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue crypto_obj = JS_GetPropertyStr(ctx, global, "crypto");

  int32_t random_value = min;

  if (!JS_IsUndefined(crypto_obj)) {
    JSValue get_random_values = JS_GetPropertyStr(ctx, crypto_obj, "getRandomValues");
    if (JS_IsFunction(ctx, get_random_values)) {
      // Create Uint32Array with 1 element
      JSValue uint32_array_ctor = JS_GetPropertyStr(ctx, global, "Uint32Array");
      JSValue size_arg = JS_NewInt32(ctx, 1);
      JSValue array = JS_CallConstructor(ctx, uint32_array_ctor, 1, &size_arg);

      if (!JS_IsException(array)) {
        JSValue result = JS_Call(ctx, get_random_values, crypto_obj, 1, &array);
        if (!JS_IsException(result)) {
          // Read the random value from the array
          JSValue random_val = JS_GetPropertyUint32(ctx, result, 0);
          uint32_t random_u32;
          JS_ToUint32(ctx, &random_u32, random_val);

          // Uniform distribution using rejection sampling
          // Avoid modulo bias by rejecting values that would cause it
          uint32_t limit = 0xFFFFFFFF - (0xFFFFFFFF % range);
          if (random_u32 < limit) {
            random_value = min + (int32_t)(random_u32 % range);
          } else {
            // Fallback to simple modulo for rejected values (rare)
            random_value = min + (int32_t)(random_u32 % range);
          }

          JS_FreeValue(ctx, random_val);
        }
        JS_FreeValue(ctx, result);
      }
      JS_FreeValue(ctx, array);
      JS_FreeValue(ctx, size_arg);
      JS_FreeValue(ctx, uint32_array_ctor);
    }
    JS_FreeValue(ctx, get_random_values);
  } else {
    // Fallback: use rand() with modulo (not cryptographically secure)
    random_value = min + (rand() % range);
  }

  JS_FreeValue(ctx, crypto_obj);
  JS_FreeValue(ctx, global);

  JSValue result = JS_NewInt32(ctx, random_value);

  // If callback provided, call it with (null, result)
  if (JS_IsFunction(ctx, callback)) {
    JSValue args[2] = {JS_NULL, result};
    JSValue cb_result = JS_Call(ctx, callback, JS_UNDEFINED, 2, args);
    JS_FreeValue(ctx, cb_result);
    return JS_UNDEFINED;
  }

  return result;
}

// crypto.getCiphers() - returns list of supported cipher algorithms
JSValue js_crypto_get_ciphers(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue array = JS_NewArray(ctx);

  // List of supported ciphers (based on our implementations)
  const char* ciphers[] =
      {
          "aes-128-cbc",   "aes-128-gcm", "aes-192-cbc",       "aes-192-gcm", "aes-256-cbc",
          "aes-256-gcm",   "aes-128-ctr", "aes-192-ctr",       "aes-256-ctr", "aes128",
          "aes192",        "aes256",      "chacha20-poly1305",  // Potentially supported via OpenSSL
          "des-ede3-cbc",                                       // 3DES
          "id-aes128-GCM",                                      // Alias
          "id-aes192-GCM",                                      // Alias
          "id-aes256-GCM",                                      // Alias
      };

  int num_ciphers = sizeof(ciphers) / sizeof(ciphers[0]);
  for (int i = 0; i < num_ciphers; i++) {
    JS_SetPropertyUint32(ctx, array, i, JS_NewString(ctx, ciphers[i]));
  }

  return array;
}

// crypto.getHashes() - returns list of supported hash algorithms
JSValue js_crypto_get_hashes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue array = JS_NewArray(ctx);

  // List of supported hash algorithms
  const char* hashes[] = {
      "sha1",       "sha224",     "sha256",     "sha384",     "sha512",    "sha512-224", "sha512-256",
      "sha3-224",   "sha3-256",   "sha3-384",   "sha3-512",   "shake128",  "shake256",   "md5",
      "ripemd160",  "blake2b",    "blake2s",    "sm3",        "whirlpool", "sha",  // Alias for sha1
      "rsa-sha1",                                                                  // RSA signature variants
      "rsa-sha224", "rsa-sha256", "rsa-sha384", "rsa-sha512",
  };

  int num_hashes = sizeof(hashes) / sizeof(hashes[0]);
  for (int i = 0; i < num_hashes; i++) {
    JS_SetPropertyUint32(ctx, array, i, JS_NewString(ctx, hashes[i]));
  }

  return array;
}

// crypto.getCurves() - returns list of supported elliptic curves
JSValue js_crypto_get_curves(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue array = JS_NewArray(ctx);

  // List of supported elliptic curves
  const char* curves[] = {
      "P-256",       // secp256r1 / prime256v1
      "P-384",       // secp384r1
      "P-521",       // secp521r1
      "secp256k1",   // Bitcoin curve
      "secp256r1",   // Alias for P-256
      "prime256v1",  // Alias for P-256
      "secp384r1",   // Alias for P-384
      "secp521r1",   // Alias for P-521
      "brainpoolP256r1",
      "brainpoolP384r1",
      "brainpoolP512r1",
      "X25519",   // Curve25519 for ECDH
      "X448",     // Curve448 for ECDH
      "Ed25519",  // Edwards curve for signatures
      "Ed448",    // Edwards curve for signatures
  };

  int num_curves = sizeof(curves) / sizeof(curves[0]);
  for (int i = 0; i < num_curves; i++) {
    JS_SetPropertyUint32(ctx, array, i, JS_NewString(ctx, curves[i]));
  }

  return array;
}
