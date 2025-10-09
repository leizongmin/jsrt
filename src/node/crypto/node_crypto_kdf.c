#include "../../crypto/crypto_kdf.h"
#include "node_crypto_internal.h"

//==============================================================================
// Key Derivation Functions (Node.js pbkdf2, hkdf)
// Phase 6: KDF Implementation
//==============================================================================

// Helper function to parse hash algorithm name
static jsrt_crypto_algorithm_t parse_hash_algorithm(const char* hash_name) {
  if (!hash_name) {
    return JSRT_CRYPTO_ALG_UNKNOWN;
  }

  if (strcmp(hash_name, "sha1") == 0 || strcmp(hash_name, "SHA-1") == 0) {
    return JSRT_CRYPTO_ALG_SHA1;
  } else if (strcmp(hash_name, "sha256") == 0 || strcmp(hash_name, "SHA-256") == 0) {
    return JSRT_CRYPTO_ALG_SHA256;
  } else if (strcmp(hash_name, "sha384") == 0 || strcmp(hash_name, "SHA-384") == 0) {
    return JSRT_CRYPTO_ALG_SHA384;
  } else if (strcmp(hash_name, "sha512") == 0 || strcmp(hash_name, "SHA-512") == 0) {
    return JSRT_CRYPTO_ALG_SHA512;
  }

  return JSRT_CRYPTO_ALG_UNKNOWN;
}

// Helper to convert JSValue to buffer data
static int get_buffer_data(JSContext* ctx, JSValueConst val, uint8_t** data, size_t* len) {
  size_t buffer_len;
  uint8_t* buffer = JS_GetArrayBuffer(ctx, &buffer_len, val);

  if (!buffer) {
    // Try to get as typed array
    JSValue buffer_prop = JS_GetPropertyStr(ctx, val, "buffer");
    if (!JS_IsUndefined(buffer_prop)) {
      buffer = JS_GetArrayBuffer(ctx, &buffer_len, buffer_prop);
      JS_FreeValue(ctx, buffer_prop);
    }
  }

  if (!buffer) {
    // Try as string (UTF-8)
    const char* str = JS_ToCString(ctx, val);
    if (str) {
      *len = strlen(str);
      *data = malloc(*len);
      if (*data) {
        memcpy(*data, str, *len);
        JS_FreeCString(ctx, str);
        return 1;  // Indicates allocated memory
      }
      JS_FreeCString(ctx, str);
    }
    return -1;  // Error
  }

  // Copy buffer data
  *len = buffer_len;
  *data = malloc(buffer_len);
  if (!*data) {
    return -1;
  }
  memcpy(*data, buffer, buffer_len);
  return 1;  // Indicates allocated memory
}

//==============================================================================
// PBKDF2 Implementation
//==============================================================================

// crypto.pbkdf2Sync(password, salt, iterations, keylen, digest)
JSValue js_crypto_pbkdf2_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 5) {
    return JS_ThrowTypeError(ctx, "pbkdf2Sync requires 5 arguments: password, salt, iterations, keylen, digest");
  }

  // Get password
  uint8_t* password = NULL;
  size_t password_len = 0;
  int password_allocated = get_buffer_data(ctx, argv[0], &password, &password_len);
  if (password_allocated < 0) {
    return JS_ThrowTypeError(ctx, "password must be a string or Buffer");
  }

  // Get salt
  uint8_t* salt = NULL;
  size_t salt_len = 0;
  int salt_allocated = get_buffer_data(ctx, argv[1], &salt, &salt_len);
  if (salt_allocated < 0) {
    if (password_allocated > 0)
      free(password);
    return JS_ThrowTypeError(ctx, "salt must be a string or Buffer");
  }

  // Get iterations
  int32_t iterations;
  if (JS_ToInt32(ctx, &iterations, argv[2]) < 0 || iterations <= 0) {
    if (password_allocated > 0)
      free(password);
    if (salt_allocated > 0)
      free(salt);
    return JS_ThrowTypeError(ctx, "iterations must be a positive number");
  }

  // Get keylen
  int32_t keylen;
  if (JS_ToInt32(ctx, &keylen, argv[3]) < 0 || keylen <= 0) {
    if (password_allocated > 0)
      free(password);
    if (salt_allocated > 0)
      free(salt);
    return JS_ThrowTypeError(ctx, "keylen must be a positive number");
  }

  // Get digest algorithm
  const char* digest_name = JS_ToCString(ctx, argv[4]);
  if (!digest_name) {
    if (password_allocated > 0)
      free(password);
    if (salt_allocated > 0)
      free(salt);
    return JS_EXCEPTION;
  }

  jsrt_crypto_algorithm_t hash_alg = parse_hash_algorithm(digest_name);
  JS_FreeCString(ctx, digest_name);

  if (hash_alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    if (password_allocated > 0)
      free(password);
    if (salt_allocated > 0)
      free(salt);
    return JS_ThrowTypeError(ctx, "Unsupported digest algorithm");
  }

  // Setup PBKDF2 parameters
  jsrt_pbkdf2_params_t params;
  params.hash_algorithm = hash_alg;
  params.salt = salt;
  params.salt_length = salt_len;
  params.iterations = iterations;

  // Derive key
  uint8_t* derived_key = NULL;
  int result = jsrt_crypto_pbkdf2_derive_key(&params, password, password_len, keylen, &derived_key);

  // Clean up input data
  if (password_allocated > 0)
    free(password);
  if (salt_allocated > 0)
    free(salt);

  if (result != 0 || !derived_key) {
    if (derived_key)
      free(derived_key);
    return JS_ThrowInternalError(ctx, "PBKDF2 key derivation failed");
  }

  // Create Uint8Array result
  JSValue array_buffer = JS_NewArrayBuffer(ctx, derived_key, keylen, NULL, NULL, false);
  if (JS_IsException(array_buffer)) {
    free(derived_key);
    return array_buffer;
  }

  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JSValue result_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, array_buffer);

  return result_array;
}

// crypto.pbkdf2(password, salt, iterations, keylen, digest, callback)
JSValue js_crypto_pbkdf2(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 6) {
    return JS_ThrowTypeError(ctx, "pbkdf2 requires 6 arguments: password, salt, iterations, keylen, digest, callback");
  }

  if (!JS_IsFunction(ctx, argv[5])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Call sync version
  JSValue result = js_crypto_pbkdf2_sync(ctx, this_val, 5, argv);

  // Invoke callback asynchronously using setTimeout trick
  JSValue callback = JS_DupValue(ctx, argv[5]);

  if (JS_IsException(result)) {
    // Error case - call callback with error
    JSValue error = result;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue set_timeout = JS_GetPropertyStr(ctx, global, "setTimeout");

    if (JS_IsFunction(ctx, set_timeout)) {
      // Create wrapper function that calls callback(error)
      const char* wrapper_code = "(callback, error) => callback(error)";
      JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<pbkdf2>", 0);

      if (JS_IsFunction(ctx, wrapper)) {
        JSValue bound_args[] = {callback, error};
        JSValue bound = JS_Call(ctx, wrapper, JS_UNDEFINED, 2, bound_args);

        JSValue timeout_args[] = {bound, JS_NewInt32(ctx, 0)};
        JS_Call(ctx, set_timeout, JS_UNDEFINED, 2, timeout_args);

        JS_FreeValue(ctx, bound);
        JS_FreeValue(ctx, timeout_args[1]);
      }
      JS_FreeValue(ctx, wrapper);
    }

    JS_FreeValue(ctx, set_timeout);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, callback);
    JS_FreeValue(ctx, error);
    return JS_UNDEFINED;
  }

  // Success case - call callback(null, result)
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue set_timeout = JS_GetPropertyStr(ctx, global, "setTimeout");

  if (JS_IsFunction(ctx, set_timeout)) {
    // Create wrapper function that calls callback(null, result)
    const char* wrapper_code = "(callback, result) => callback(null, result)";
    JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<pbkdf2>", 0);

    if (JS_IsFunction(ctx, wrapper)) {
      JSValue bound_args[] = {callback, result};
      JSValue bound = JS_Call(ctx, wrapper, JS_UNDEFINED, 2, bound_args);

      JSValue timeout_args[] = {bound, JS_NewInt32(ctx, 0)};
      JS_Call(ctx, set_timeout, JS_UNDEFINED, 2, timeout_args);

      JS_FreeValue(ctx, bound);
      JS_FreeValue(ctx, timeout_args[1]);
    }
    JS_FreeValue(ctx, wrapper);
  }

  JS_FreeValue(ctx, set_timeout);
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, callback);
  JS_FreeValue(ctx, result);

  return JS_UNDEFINED;
}

//==============================================================================
// HKDF Implementation
//==============================================================================

// crypto.hkdfSync(digest, ikm, salt, info, keylen)
JSValue js_crypto_hkdf_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 5) {
    return JS_ThrowTypeError(ctx, "hkdfSync requires 5 arguments: digest, ikm, salt, info, keylen");
  }

  // Get digest algorithm
  const char* digest_name = JS_ToCString(ctx, argv[0]);
  if (!digest_name) {
    return JS_EXCEPTION;
  }

  jsrt_crypto_algorithm_t hash_alg = parse_hash_algorithm(digest_name);
  JS_FreeCString(ctx, digest_name);

  if (hash_alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    return JS_ThrowTypeError(ctx, "Unsupported digest algorithm");
  }

  // Get ikm (input key material)
  uint8_t* ikm = NULL;
  size_t ikm_len = 0;
  int ikm_allocated = get_buffer_data(ctx, argv[1], &ikm, &ikm_len);
  if (ikm_allocated < 0) {
    return JS_ThrowTypeError(ctx, "ikm must be a string or Buffer");
  }

  // Get salt
  uint8_t* salt = NULL;
  size_t salt_len = 0;
  int salt_allocated = get_buffer_data(ctx, argv[2], &salt, &salt_len);
  if (salt_allocated < 0) {
    if (ikm_allocated > 0)
      free(ikm);
    return JS_ThrowTypeError(ctx, "salt must be a string or Buffer");
  }

  // Get info
  uint8_t* info = NULL;
  size_t info_len = 0;
  int info_allocated = get_buffer_data(ctx, argv[3], &info, &info_len);
  if (info_allocated < 0) {
    if (ikm_allocated > 0)
      free(ikm);
    if (salt_allocated > 0)
      free(salt);
    return JS_ThrowTypeError(ctx, "info must be a string or Buffer");
  }

  // Get keylen
  int32_t keylen;
  if (JS_ToInt32(ctx, &keylen, argv[4]) < 0 || keylen <= 0) {
    if (ikm_allocated > 0)
      free(ikm);
    if (salt_allocated > 0)
      free(salt);
    if (info_allocated > 0)
      free(info);
    return JS_ThrowTypeError(ctx, "keylen must be a positive number");
  }

  // Setup HKDF parameters
  jsrt_hkdf_params_t params;
  params.hash_algorithm = hash_alg;
  params.salt = salt;
  params.salt_length = salt_len;
  params.info = info;
  params.info_length = info_len;

  // Derive key
  uint8_t* derived_key = NULL;
  int result = jsrt_crypto_hkdf_derive_key(&params, ikm, ikm_len, keylen, &derived_key);

  // Clean up input data
  if (ikm_allocated > 0)
    free(ikm);
  if (salt_allocated > 0)
    free(salt);
  if (info_allocated > 0)
    free(info);

  if (result != 0 || !derived_key) {
    if (derived_key)
      free(derived_key);
    return JS_ThrowInternalError(ctx, "HKDF key derivation failed");
  }

  // Create Uint8Array result
  JSValue array_buffer = JS_NewArrayBuffer(ctx, derived_key, keylen, NULL, NULL, false);
  if (JS_IsException(array_buffer)) {
    free(derived_key);
    return array_buffer;
  }

  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JSValue result_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, array_buffer);

  return result_array;
}

// crypto.hkdf(digest, ikm, salt, info, keylen, callback)
JSValue js_crypto_hkdf(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 6) {
    return JS_ThrowTypeError(ctx, "hkdf requires 6 arguments: digest, ikm, salt, info, keylen, callback");
  }

  if (!JS_IsFunction(ctx, argv[5])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Call sync version
  JSValue result = js_crypto_hkdf_sync(ctx, this_val, 5, argv);

  // Invoke callback asynchronously using setTimeout trick
  JSValue callback = JS_DupValue(ctx, argv[5]);

  if (JS_IsException(result)) {
    // Error case - call callback with error
    JSValue error = result;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue set_timeout = JS_GetPropertyStr(ctx, global, "setTimeout");

    if (JS_IsFunction(ctx, set_timeout)) {
      // Create wrapper function that calls callback(error)
      const char* wrapper_code = "(callback, error) => callback(error)";
      JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<hkdf>", 0);

      if (JS_IsFunction(ctx, wrapper)) {
        JSValue bound_args[] = {callback, error};
        JSValue bound = JS_Call(ctx, wrapper, JS_UNDEFINED, 2, bound_args);

        JSValue timeout_args[] = {bound, JS_NewInt32(ctx, 0)};
        JS_Call(ctx, set_timeout, JS_UNDEFINED, 2, timeout_args);

        JS_FreeValue(ctx, bound);
        JS_FreeValue(ctx, timeout_args[1]);
      }
      JS_FreeValue(ctx, wrapper);
    }

    JS_FreeValue(ctx, set_timeout);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, callback);
    JS_FreeValue(ctx, error);
    return JS_UNDEFINED;
  }

  // Success case - call callback(null, result)
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue set_timeout = JS_GetPropertyStr(ctx, global, "setTimeout");

  if (JS_IsFunction(ctx, set_timeout)) {
    // Create wrapper function that calls callback(null, result)
    const char* wrapper_code = "(callback, result) => callback(null, result)";
    JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<hkdf>", 0);

    if (JS_IsFunction(ctx, wrapper)) {
      JSValue bound_args[] = {callback, result};
      JSValue bound = JS_Call(ctx, wrapper, JS_UNDEFINED, 2, bound_args);

      JSValue timeout_args[] = {bound, JS_NewInt32(ctx, 0)};
      JS_Call(ctx, set_timeout, JS_UNDEFINED, 2, timeout_args);

      JS_FreeValue(ctx, bound);
      JS_FreeValue(ctx, timeout_args[1]);
    }
    JS_FreeValue(ctx, wrapper);
  }

  JS_FreeValue(ctx, set_timeout);
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, callback);
  JS_FreeValue(ctx, result);

  return JS_UNDEFINED;
}

//==============================================================================
// Scrypt Implementation (Check OpenSSL availability)
//==============================================================================

// Scrypt is not implemented yet - OpenSSL 3.x support needs investigation
// For now, provide stub functions that throw "not implemented" error

JSValue js_crypto_scrypt_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "scrypt is not yet implemented");
}

JSValue js_crypto_scrypt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 4) {
    return JS_ThrowTypeError(ctx, "scrypt requires at least 4 arguments");
  }

  if (!JS_IsFunction(ctx, argv[argc - 1])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Call callback with error asynchronously
  JSValue callback = JS_DupValue(ctx, argv[argc - 1]);
  JSValue error = JS_ThrowInternalError(ctx, "scrypt is not yet implemented");

  JSValue global = JS_GetGlobalObject(ctx);
  JSValue set_timeout = JS_GetPropertyStr(ctx, global, "setTimeout");

  if (JS_IsFunction(ctx, set_timeout)) {
    const char* wrapper_code = "(callback, error) => callback(error)";
    JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<scrypt>", 0);

    if (JS_IsFunction(ctx, wrapper)) {
      JSValue bound_args[] = {callback, error};
      JSValue bound = JS_Call(ctx, wrapper, JS_UNDEFINED, 2, bound_args);

      JSValue timeout_args[] = {bound, JS_NewInt32(ctx, 0)};
      JS_Call(ctx, set_timeout, JS_UNDEFINED, 2, timeout_args);

      JS_FreeValue(ctx, bound);
      JS_FreeValue(ctx, timeout_args[1]);
    }
    JS_FreeValue(ctx, wrapper);
  }

  JS_FreeValue(ctx, set_timeout);
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, callback);
  JS_FreeValue(ctx, error);

  return JS_UNDEFINED;
}
