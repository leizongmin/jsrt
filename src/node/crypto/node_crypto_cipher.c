#include "node_crypto_internal.h"

//==============================================================================
// Cipher Class Implementation (Node.js createCipheriv/createDecipheriv API)
// Uses OpenSSL EVP streaming API directly for true streaming support
//==============================================================================

JSClassID js_node_cipher_class_id;

static void js_node_cipher_finalizer(JSRuntime* rt, JSValue val) {
  JSNodeCipher* cipher = JS_GetOpaque(val, js_node_cipher_class_id);
  if (cipher) {
    if (cipher->evp_ctx && cipher->openssl_funcs) {
      cipher->openssl_funcs->EVP_CIPHER_CTX_free(cipher->evp_ctx);
    }
    if (cipher->key_data) {
      js_free_rt(rt, cipher->key_data);
    }
    if (cipher->iv_data) {
      js_free_rt(rt, cipher->iv_data);
    }
    if (cipher->aad_data) {
      js_free_rt(rt, cipher->aad_data);
    }
    js_free_rt(rt, cipher);
  }
}

static JSClassDef js_node_cipher_class = {
    "Cipher",
    .finalizer = js_node_cipher_finalizer,
};

// Cipher.update(data, [inputEncoding], [outputEncoding])
static JSValue js_node_cipher_update(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeCipher* cipher = JS_GetOpaque2(ctx, this_val, js_node_cipher_class_id);
  if (!cipher) {
    return JS_EXCEPTION;
  }

  if (cipher->finalized) {
    return JS_ThrowTypeError(ctx, "Cipher already finalized");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "cipher.update() requires data argument");
  }

  // Get data buffer
  size_t data_len;
  uint8_t* data = JS_GetArrayBuffer(ctx, &data_len, argv[0]);

  if (!data) {
    // Try to get as typed array
    JSValue buffer = JS_GetPropertyStr(ctx, argv[0], "buffer");
    if (!JS_IsUndefined(buffer)) {
      data = JS_GetArrayBuffer(ctx, &data_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!data) {
    return JS_ThrowTypeError(ctx, "data must be a Buffer or TypedArray");
  }

  // Allocate output buffer (worst case: input + block size for padding)
  size_t max_output_len = data_len + JSRT_AES_BLOCK_SIZE;
  uint8_t* output = js_malloc(ctx, max_output_len);
  if (!output) {
    return JS_ThrowOutOfMemory(ctx);
  }

  int output_len = 0;
  int result;

  if (cipher->is_encrypt) {
    result = cipher->openssl_funcs->EVP_EncryptUpdate(cipher->evp_ctx, output, &output_len, data, data_len);
  } else {
    result = cipher->openssl_funcs->EVP_DecryptUpdate(cipher->evp_ctx, output, &output_len, data, data_len);
  }

  if (result != 1) {
    js_free(ctx, output);
    return JS_ThrowInternalError(ctx, "Cipher update failed");
  }

  // Handle output encoding
  const char* encoding = NULL;
  if (argc > 2 && JS_IsString(argv[2])) {
    encoding = JS_ToCString(ctx, argv[2]);
  }

  JSValue js_result;
  if (encoding && strcmp(encoding, "hex") == 0) {
    // Hex encoding
    char* hex = js_malloc(ctx, output_len * 2 + 1);
    if (!hex) {
      js_free(ctx, output);
      if (encoding)
        JS_FreeCString(ctx, encoding);
      return JS_ThrowOutOfMemory(ctx);
    }
    for (int i = 0; i < output_len; i++) {
      sprintf(hex + i * 2, "%02x", output[i]);
    }
    hex[output_len * 2] = '\0';  // Null-terminate the hex string
    js_result = JS_NewString(ctx, hex);
    js_free(ctx, hex);
    js_free(ctx, output);
  } else if (encoding && strcmp(encoding, "base64") == 0) {
    // Base64 encoding
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, output, output_len);
    js_free(ctx, output);

    JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
    JSValue uint8_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

    JSValue btoa = JS_GetPropertyStr(ctx, global, "btoa");
    JSValue spread = JS_Eval(ctx, "(...args) => String.fromCharCode(...args)", 42, NULL, 0);
    JSValue args[] = {uint8_array};
    JSValue str = JS_Call(ctx, spread, JS_UNDEFINED, 1, args);
    JSValue base64_args[] = {str};
    js_result = JS_Call(ctx, btoa, JS_UNDEFINED, 1, base64_args);

    JS_FreeValue(ctx, spread);
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, uint8_array);
    JS_FreeValue(ctx, uint8_array_ctor);
    JS_FreeValue(ctx, array_buffer);
    JS_FreeValue(ctx, btoa);
    JS_FreeValue(ctx, global);
  } else {
    // Return as Buffer (Uint8Array)
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, output, output_len);
    js_free(ctx, output);

    JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
    js_result = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);
    JS_FreeValue(ctx, uint8_array_ctor);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, array_buffer);
  }

  if (encoding) {
    JS_FreeCString(ctx, encoding);
  }

  return js_result;
}

// Cipher.final([outputEncoding])
static JSValue js_node_cipher_final(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeCipher* cipher = JS_GetOpaque2(ctx, this_val, js_node_cipher_class_id);
  if (!cipher) {
    return JS_EXCEPTION;
  }

  if (cipher->finalized) {
    return JS_ThrowTypeError(ctx, "Cipher already finalized");
  }

  cipher->finalized = true;

  // Allocate output buffer for final block
  uint8_t* output = js_malloc(ctx, JSRT_AES_BLOCK_SIZE + JSRT_GCM_TAG_SIZE);
  if (!output) {
    return JS_ThrowOutOfMemory(ctx);
  }

  int output_len = 0;
  int result;

  if (cipher->is_encrypt) {
    result = cipher->openssl_funcs->EVP_EncryptFinal_ex(cipher->evp_ctx, output, &output_len);
  } else {
    result = cipher->openssl_funcs->EVP_DecryptFinal_ex(cipher->evp_ctx, output, &output_len);
  }

  if (result != 1) {
    js_free(ctx, output);
    return JS_ThrowInternalError(ctx, cipher->is_encrypt
                                          ? "Encryption finalization failed"
                                          : "Decryption failed (authentication error or invalid padding)");
  }

  // For GCM encryption, append auth tag
  if (cipher->is_encrypt && cipher->algorithm == JSRT_SYMMETRIC_AES_GCM) {
    if (cipher->openssl_funcs->EVP_CIPHER_CTX_ctrl(cipher->evp_ctx, EVP_CTRL_GCM_GET_TAG, JSRT_GCM_TAG_SIZE,
                                                   cipher->auth_tag) != 1) {
      js_free(ctx, output);
      return JS_ThrowInternalError(ctx, "Failed to get GCM authentication tag");
    }
  }

  // Handle output encoding
  const char* encoding = NULL;
  if (argc > 0 && JS_IsString(argv[0])) {
    encoding = JS_ToCString(ctx, argv[0]);
  }

  JSValue js_result;
  if (encoding && strcmp(encoding, "hex") == 0) {
    // Hex encoding
    char* hex = js_malloc(ctx, output_len * 2 + 1);
    if (!hex) {
      js_free(ctx, output);
      if (encoding)
        JS_FreeCString(ctx, encoding);
      return JS_ThrowOutOfMemory(ctx);
    }
    for (int i = 0; i < output_len; i++) {
      sprintf(hex + i * 2, "%02x", output[i]);
    }
    hex[output_len * 2] = '\0';  // Null-terminate the hex string
    js_result = JS_NewString(ctx, hex);
    js_free(ctx, hex);
    js_free(ctx, output);
  } else if (encoding && strcmp(encoding, "base64") == 0) {
    // Base64 encoding
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, output, output_len);
    js_free(ctx, output);

    JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
    JSValue uint8_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

    JSValue btoa = JS_GetPropertyStr(ctx, global, "btoa");
    JSValue spread = JS_Eval(ctx, "(...args) => String.fromCharCode(...args)", 42, NULL, 0);
    JSValue args[] = {uint8_array};
    JSValue str = JS_Call(ctx, spread, JS_UNDEFINED, 1, args);
    JSValue base64_args[] = {str};
    js_result = JS_Call(ctx, btoa, JS_UNDEFINED, 1, base64_args);

    JS_FreeValue(ctx, spread);
    JS_FreeValue(ctx, str);
    JS_FreeValue(ctx, uint8_array);
    JS_FreeValue(ctx, uint8_array_ctor);
    JS_FreeValue(ctx, array_buffer);
    JS_FreeValue(ctx, btoa);
    JS_FreeValue(ctx, global);
  } else {
    // Return as Buffer (Uint8Array)
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, output, output_len);
    js_free(ctx, output);

    JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
    js_result = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);
    JS_FreeValue(ctx, uint8_array_ctor);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, array_buffer);
  }

  if (encoding) {
    JS_FreeCString(ctx, encoding);
  }

  return js_result;
}

// Cipher.setAAD(buffer, [options]) - GCM only
static JSValue js_node_cipher_set_aad(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeCipher* cipher = JS_GetOpaque2(ctx, this_val, js_node_cipher_class_id);
  if (!cipher) {
    return JS_EXCEPTION;
  }

  if (cipher->algorithm != JSRT_SYMMETRIC_AES_GCM) {
    return JS_ThrowTypeError(ctx, "setAAD is only supported for GCM mode");
  }

  if (cipher->finalized) {
    return JS_ThrowTypeError(ctx, "Cipher already finalized");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "cipher.setAAD() requires buffer argument");
  }

  // Get AAD buffer
  size_t aad_len;
  uint8_t* aad = JS_GetArrayBuffer(ctx, &aad_len, argv[0]);

  if (!aad) {
    // Try to get as typed array
    JSValue buffer = JS_GetPropertyStr(ctx, argv[0], "buffer");
    if (!JS_IsUndefined(buffer)) {
      aad = JS_GetArrayBuffer(ctx, &aad_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!aad) {
    return JS_ThrowTypeError(ctx, "AAD must be a Buffer or TypedArray");
  }

  // Copy AAD data
  if (cipher->aad_data) {
    js_free(ctx, cipher->aad_data);
  }
  cipher->aad_data = js_malloc(ctx, aad_len);
  if (!cipher->aad_data) {
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(cipher->aad_data, aad, aad_len);
  cipher->aad_length = aad_len;

  // Set AAD in OpenSSL context (only for encryption)
  int len;
  int result;
  if (cipher->is_encrypt) {
    result =
        cipher->openssl_funcs->EVP_EncryptUpdate(cipher->evp_ctx, NULL, &len, cipher->aad_data, cipher->aad_length);
  } else {
    result =
        cipher->openssl_funcs->EVP_DecryptUpdate(cipher->evp_ctx, NULL, &len, cipher->aad_data, cipher->aad_length);
  }

  if (result != 1) {
    return JS_ThrowInternalError(ctx, "Failed to set additional authenticated data");
  }

  return JS_DupValue(ctx, this_val);
}

// Cipher.getAuthTag() - GCM encryption only
static JSValue js_node_cipher_get_auth_tag(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeCipher* cipher = JS_GetOpaque2(ctx, this_val, js_node_cipher_class_id);
  if (!cipher) {
    return JS_EXCEPTION;
  }

  if (cipher->algorithm != JSRT_SYMMETRIC_AES_GCM) {
    return JS_ThrowTypeError(ctx, "getAuthTag is only supported for GCM mode");
  }

  if (!cipher->is_encrypt) {
    return JS_ThrowTypeError(ctx, "getAuthTag is only for encryption");
  }

  if (!cipher->finalized) {
    return JS_ThrowTypeError(ctx, "Must call final() before getAuthTag()");
  }

  // Return auth tag as Buffer
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue array_buffer = JS_NewArrayBufferCopy(ctx, cipher->auth_tag, JSRT_GCM_TAG_SIZE);

  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JSValue result = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, array_buffer);

  return result;
}

// Decipher.setAuthTag(buffer) - GCM decryption only
static JSValue js_node_cipher_set_auth_tag(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeCipher* cipher = JS_GetOpaque2(ctx, this_val, js_node_cipher_class_id);
  if (!cipher) {
    return JS_EXCEPTION;
  }

  if (cipher->algorithm != JSRT_SYMMETRIC_AES_GCM) {
    return JS_ThrowTypeError(ctx, "setAuthTag is only supported for GCM mode");
  }

  if (cipher->is_encrypt) {
    return JS_ThrowTypeError(ctx, "setAuthTag is only for decryption");
  }

  if (cipher->finalized) {
    return JS_ThrowTypeError(ctx, "Cannot set auth tag after finalization");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "cipher.setAuthTag() requires buffer argument");
  }

  // Get auth tag buffer
  size_t tag_len;
  uint8_t* tag = JS_GetArrayBuffer(ctx, &tag_len, argv[0]);

  if (!tag) {
    // Try to get as typed array
    JSValue buffer = JS_GetPropertyStr(ctx, argv[0], "buffer");
    if (!JS_IsUndefined(buffer)) {
      tag = JS_GetArrayBuffer(ctx, &tag_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!tag) {
    return JS_ThrowTypeError(ctx, "Auth tag must be a Buffer or TypedArray");
  }

  if (tag_len != JSRT_GCM_TAG_SIZE) {
    return JS_ThrowTypeError(ctx, "Invalid auth tag length");
  }

  // Copy auth tag
  memcpy(cipher->auth_tag, tag, JSRT_GCM_TAG_SIZE);

  // Set auth tag in OpenSSL context
  if (cipher->openssl_funcs->EVP_CIPHER_CTX_ctrl(cipher->evp_ctx, EVP_CTRL_GCM_SET_TAG, JSRT_GCM_TAG_SIZE,
                                                 cipher->auth_tag) != 1) {
    return JS_ThrowInternalError(ctx, "Failed to set authentication tag");
  }

  return JS_DupValue(ctx, this_val);
}

// Helper to get OpenSSL cipher
static const void* get_openssl_cipher_for_node(openssl_symmetric_funcs_t* funcs, jsrt_symmetric_algorithm_t alg,
                                               size_t key_length) {
  switch (alg) {
    case JSRT_SYMMETRIC_AES_CBC:
      switch (key_length) {
        case JSRT_AES_128_KEY_SIZE:
          return funcs->EVP_aes_128_cbc ? funcs->EVP_aes_128_cbc() : NULL;
        case JSRT_AES_192_KEY_SIZE:
          return funcs->EVP_aes_192_cbc ? funcs->EVP_aes_192_cbc() : NULL;
        case JSRT_AES_256_KEY_SIZE:
          return funcs->EVP_aes_256_cbc ? funcs->EVP_aes_256_cbc() : NULL;
      }
      break;

    case JSRT_SYMMETRIC_AES_GCM:
      switch (key_length) {
        case JSRT_AES_128_KEY_SIZE:
          return funcs->EVP_aes_128_gcm ? funcs->EVP_aes_128_gcm() : NULL;
        case JSRT_AES_192_KEY_SIZE:
          return funcs->EVP_aes_192_gcm ? funcs->EVP_aes_192_gcm() : NULL;
        case JSRT_AES_256_KEY_SIZE:
          return funcs->EVP_aes_256_gcm ? funcs->EVP_aes_256_gcm() : NULL;
      }
      break;

    case JSRT_SYMMETRIC_AES_CTR:
      switch (key_length) {
        case JSRT_AES_128_KEY_SIZE:
          return funcs->EVP_aes_128_ctr ? funcs->EVP_aes_128_ctr() : NULL;
        case JSRT_AES_192_KEY_SIZE:
          return funcs->EVP_aes_192_ctr ? funcs->EVP_aes_192_ctr() : NULL;
        case JSRT_AES_256_KEY_SIZE:
          return funcs->EVP_aes_256_ctr ? funcs->EVP_aes_256_ctr() : NULL;
      }
      break;
  }

  return NULL;
}

// crypto.createCipheriv(algorithm, key, iv, [options])
JSValue js_crypto_create_cipheriv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "createCipheriv() requires algorithm, key, and iv arguments");
  }

  const char* algorithm = JS_ToCString(ctx, argv[0]);
  if (!algorithm) {
    return JS_EXCEPTION;
  }

  // Parse algorithm (e.g., "aes-256-cbc", "aes-128-gcm")
  jsrt_symmetric_algorithm_t alg = JSRT_SYMMETRIC_AES_CBC;
  if (strstr(algorithm, "gcm")) {
    alg = JSRT_SYMMETRIC_AES_GCM;
  } else if (strstr(algorithm, "ctr")) {
    alg = JSRT_SYMMETRIC_AES_CTR;
  } else if (strstr(algorithm, "cbc")) {
    alg = JSRT_SYMMETRIC_AES_CBC;
  } else {
    JS_FreeCString(ctx, algorithm);
    return JS_ThrowTypeError(ctx, "Unsupported cipher algorithm");
  }

  JS_FreeCString(ctx, algorithm);

  // Get key
  size_t key_len;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_len, argv[1]);
  if (!key_data) {
    JSValue buffer = JS_GetPropertyStr(ctx, argv[1], "buffer");
    if (!JS_IsUndefined(buffer)) {
      key_data = JS_GetArrayBuffer(ctx, &key_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!key_data ||
      (key_len != JSRT_AES_128_KEY_SIZE && key_len != JSRT_AES_192_KEY_SIZE && key_len != JSRT_AES_256_KEY_SIZE)) {
    return JS_ThrowTypeError(ctx, "Invalid key: must be 16, 24, or 32 bytes");
  }

  // Get IV
  size_t iv_len;
  uint8_t* iv_data = JS_GetArrayBuffer(ctx, &iv_len, argv[2]);
  if (!iv_data) {
    JSValue buffer = JS_GetPropertyStr(ctx, argv[2], "buffer");
    if (!JS_IsUndefined(buffer)) {
      iv_data = JS_GetArrayBuffer(ctx, &iv_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!iv_data) {
    return JS_ThrowTypeError(ctx, "Invalid IV");
  }

  // Load OpenSSL functions
  openssl_symmetric_funcs_t* funcs = jsrt_get_openssl_symmetric_funcs();
  if (!funcs || !funcs->EVP_CIPHER_CTX_new) {
    return JS_ThrowInternalError(ctx, "OpenSSL symmetric functions not available");
  }

  // Get cipher
  const void* cipher = get_openssl_cipher_for_node(funcs, alg, key_len);
  if (!cipher) {
    return JS_ThrowTypeError(ctx, "Unsupported cipher algorithm or key length");
  }

  // Create EVP context
  void* evp_ctx = funcs->EVP_CIPHER_CTX_new();
  if (!evp_ctx) {
    return JS_ThrowInternalError(ctx, "Failed to create cipher context");
  }

  // Initialize encryption
  int result;
  if (alg == JSRT_SYMMETRIC_AES_GCM) {
    // GCM requires special initialization
    result = funcs->EVP_EncryptInit_ex(evp_ctx, cipher, NULL, NULL, NULL);
    if (result == 1) {
      result = funcs->EVP_CIPHER_CTX_ctrl(evp_ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL);
    }
    if (result == 1) {
      result = funcs->EVP_EncryptInit_ex(evp_ctx, NULL, NULL, key_data, iv_data);
    }
  } else {
    result = funcs->EVP_EncryptInit_ex(evp_ctx, cipher, NULL, key_data, iv_data);
  }

  if (result != 1) {
    funcs->EVP_CIPHER_CTX_free(evp_ctx);
    return JS_ThrowInternalError(ctx, "Failed to initialize cipher");
  }

  // Create Cipher object
  JSNodeCipher* cipher_obj = js_mallocz(ctx, sizeof(JSNodeCipher));
  if (!cipher_obj) {
    funcs->EVP_CIPHER_CTX_free(evp_ctx);
    return JS_ThrowOutOfMemory(ctx);
  }

  cipher_obj->ctx = ctx;
  cipher_obj->algorithm = alg;
  cipher_obj->evp_ctx = evp_ctx;
  cipher_obj->openssl_funcs = funcs;
  cipher_obj->is_encrypt = true;
  cipher_obj->finalized = false;

  // Copy key and IV for reference
  cipher_obj->key_data = js_malloc(ctx, key_len);
  cipher_obj->iv_data = js_malloc(ctx, iv_len);
  if (!cipher_obj->key_data || !cipher_obj->iv_data) {
    if (cipher_obj->key_data)
      js_free(ctx, cipher_obj->key_data);
    if (cipher_obj->iv_data)
      js_free(ctx, cipher_obj->iv_data);
    funcs->EVP_CIPHER_CTX_free(evp_ctx);
    js_free(ctx, cipher_obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  memcpy(cipher_obj->key_data, key_data, key_len);
  cipher_obj->key_length = key_len;
  memcpy(cipher_obj->iv_data, iv_data, iv_len);
  cipher_obj->iv_length = iv_len;

  // Create JS object
  JSValue obj = JS_NewObjectClass(ctx, js_node_cipher_class_id);
  if (JS_IsException(obj)) {
    js_free(ctx, cipher_obj->key_data);
    js_free(ctx, cipher_obj->iv_data);
    funcs->EVP_CIPHER_CTX_free(evp_ctx);
    js_free(ctx, cipher_obj);
    return obj;
  }

  JS_SetOpaque(obj, cipher_obj);

  // Add methods
  JS_SetPropertyStr(ctx, obj, "update", JS_NewCFunction(ctx, js_node_cipher_update, "update", 3));
  JS_SetPropertyStr(ctx, obj, "final", JS_NewCFunction(ctx, js_node_cipher_final, "final", 1));

  if (alg == JSRT_SYMMETRIC_AES_GCM) {
    JS_SetPropertyStr(ctx, obj, "setAAD", JS_NewCFunction(ctx, js_node_cipher_set_aad, "setAAD", 2));
    JS_SetPropertyStr(ctx, obj, "getAuthTag", JS_NewCFunction(ctx, js_node_cipher_get_auth_tag, "getAuthTag", 0));
  }

  return obj;
}

// crypto.createDecipheriv(algorithm, key, iv, [options])
JSValue js_crypto_create_decipheriv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "createDecipheriv() requires algorithm, key, and iv arguments");
  }

  const char* algorithm = JS_ToCString(ctx, argv[0]);
  if (!algorithm) {
    return JS_EXCEPTION;
  }

  // Parse algorithm
  jsrt_symmetric_algorithm_t alg = JSRT_SYMMETRIC_AES_CBC;
  if (strstr(algorithm, "gcm")) {
    alg = JSRT_SYMMETRIC_AES_GCM;
  } else if (strstr(algorithm, "ctr")) {
    alg = JSRT_SYMMETRIC_AES_CTR;
  } else if (strstr(algorithm, "cbc")) {
    alg = JSRT_SYMMETRIC_AES_CBC;
  } else {
    JS_FreeCString(ctx, algorithm);
    return JS_ThrowTypeError(ctx, "Unsupported decipher algorithm");
  }

  JS_FreeCString(ctx, algorithm);

  // Get key
  size_t key_len;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_len, argv[1]);
  if (!key_data) {
    JSValue buffer = JS_GetPropertyStr(ctx, argv[1], "buffer");
    if (!JS_IsUndefined(buffer)) {
      key_data = JS_GetArrayBuffer(ctx, &key_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!key_data ||
      (key_len != JSRT_AES_128_KEY_SIZE && key_len != JSRT_AES_192_KEY_SIZE && key_len != JSRT_AES_256_KEY_SIZE)) {
    return JS_ThrowTypeError(ctx, "Invalid key: must be 16, 24, or 32 bytes");
  }

  // Get IV
  size_t iv_len;
  uint8_t* iv_data = JS_GetArrayBuffer(ctx, &iv_len, argv[2]);
  if (!iv_data) {
    JSValue buffer = JS_GetPropertyStr(ctx, argv[2], "buffer");
    if (!JS_IsUndefined(buffer)) {
      iv_data = JS_GetArrayBuffer(ctx, &iv_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!iv_data) {
    return JS_ThrowTypeError(ctx, "Invalid IV");
  }

  // Load OpenSSL functions
  openssl_symmetric_funcs_t* funcs = jsrt_get_openssl_symmetric_funcs();
  if (!funcs || !funcs->EVP_CIPHER_CTX_new) {
    return JS_ThrowInternalError(ctx, "OpenSSL symmetric functions not available");
  }

  // Get cipher
  const void* cipher = get_openssl_cipher_for_node(funcs, alg, key_len);
  if (!cipher) {
    return JS_ThrowTypeError(ctx, "Unsupported decipher algorithm or key length");
  }

  // Create EVP context
  void* evp_ctx = funcs->EVP_CIPHER_CTX_new();
  if (!evp_ctx) {
    return JS_ThrowInternalError(ctx, "Failed to create decipher context");
  }

  // Initialize decryption
  int result;
  if (alg == JSRT_SYMMETRIC_AES_GCM) {
    // GCM requires special initialization
    result = funcs->EVP_DecryptInit_ex(evp_ctx, cipher, NULL, NULL, NULL);
    if (result == 1) {
      result = funcs->EVP_CIPHER_CTX_ctrl(evp_ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL);
    }
    if (result == 1) {
      result = funcs->EVP_DecryptInit_ex(evp_ctx, NULL, NULL, key_data, iv_data);
    }
  } else {
    result = funcs->EVP_DecryptInit_ex(evp_ctx, cipher, NULL, key_data, iv_data);
  }

  if (result != 1) {
    funcs->EVP_CIPHER_CTX_free(evp_ctx);
    return JS_ThrowInternalError(ctx, "Failed to initialize decipher");
  }

  // Create Decipher object
  JSNodeCipher* cipher_obj = js_mallocz(ctx, sizeof(JSNodeCipher));
  if (!cipher_obj) {
    funcs->EVP_CIPHER_CTX_free(evp_ctx);
    return JS_ThrowOutOfMemory(ctx);
  }

  cipher_obj->ctx = ctx;
  cipher_obj->algorithm = alg;
  cipher_obj->evp_ctx = evp_ctx;
  cipher_obj->openssl_funcs = funcs;
  cipher_obj->is_encrypt = false;
  cipher_obj->finalized = false;

  // Copy key and IV for reference
  cipher_obj->key_data = js_malloc(ctx, key_len);
  cipher_obj->iv_data = js_malloc(ctx, iv_len);
  if (!cipher_obj->key_data || !cipher_obj->iv_data) {
    if (cipher_obj->key_data)
      js_free(ctx, cipher_obj->key_data);
    if (cipher_obj->iv_data)
      js_free(ctx, cipher_obj->iv_data);
    funcs->EVP_CIPHER_CTX_free(evp_ctx);
    js_free(ctx, cipher_obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  memcpy(cipher_obj->key_data, key_data, key_len);
  cipher_obj->key_length = key_len;
  memcpy(cipher_obj->iv_data, iv_data, iv_len);
  cipher_obj->iv_length = iv_len;

  // Create JS object
  JSValue obj = JS_NewObjectClass(ctx, js_node_cipher_class_id);
  if (JS_IsException(obj)) {
    js_free(ctx, cipher_obj->key_data);
    js_free(ctx, cipher_obj->iv_data);
    funcs->EVP_CIPHER_CTX_free(evp_ctx);
    js_free(ctx, cipher_obj);
    return obj;
  }

  JS_SetOpaque(obj, cipher_obj);

  // Add methods
  JS_SetPropertyStr(ctx, obj, "update", JS_NewCFunction(ctx, js_node_cipher_update, "update", 3));
  JS_SetPropertyStr(ctx, obj, "final", JS_NewCFunction(ctx, js_node_cipher_final, "final", 1));

  if (alg == JSRT_SYMMETRIC_AES_GCM) {
    JS_SetPropertyStr(ctx, obj, "setAAD", JS_NewCFunction(ctx, js_node_cipher_set_aad, "setAAD", 2));
    JS_SetPropertyStr(ctx, obj, "setAuthTag", JS_NewCFunction(ctx, js_node_cipher_set_auth_tag, "setAuthTag", 1));
  }

  return obj;
}

// Initialize Cipher class
void js_node_cipher_init_class(JSRuntime* rt) {
  JS_NewClassID(&js_node_cipher_class_id);
  JS_NewClass(rt, js_node_cipher_class_id, &js_node_cipher_class);
}
