#include "../../crypto/crypto_ec.h"
#include "../../crypto/crypto_rsa.h"
#include "node_crypto_internal.h"

//==============================================================================
// Sign/Verify Class Implementation (Node.js createSign/createVerify API)
// Uses buffering strategy: accumulate data in update(), sign/verify in final
//==============================================================================

JSClassID js_node_sign_class_id;
JSClassID js_node_verify_class_id;

//==============================================================================
// Sign Class Implementation
//==============================================================================

static void js_node_sign_finalizer(JSRuntime* rt, JSValue val) {
  JSNodeSign* sign_ctx = JS_GetOpaque(val, js_node_sign_class_id);
  if (sign_ctx) {
    if (sign_ctx->buffer) {
      js_free_rt(rt, sign_ctx->buffer);
    }
    js_free_rt(rt, sign_ctx);
  }
}

static JSClassDef js_node_sign_class = {
    "Sign",
    .finalizer = js_node_sign_finalizer,
};

// Sign.update(data, [inputEncoding])
static JSValue js_node_sign_update(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeSign* sign_ctx = JS_GetOpaque2(ctx, this_val, js_node_sign_class_id);
  if (!sign_ctx) {
    return JS_EXCEPTION;
  }

  if (sign_ctx->finalized) {
    return JS_ThrowTypeError(ctx, "Sign already called");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "sign.update() requires data argument");
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

  // Append to buffer
  size_t new_len = sign_ctx->buffer_len + data_len;
  if (new_len > sign_ctx->buffer_capacity) {
    size_t new_capacity = new_len * 2;
    if (new_capacity < 1024)
      new_capacity = 1024;
    uint8_t* new_buffer = js_realloc(ctx, sign_ctx->buffer, new_capacity);
    if (!new_buffer) {
      return JS_ThrowOutOfMemory(ctx);
    }
    sign_ctx->buffer = new_buffer;
    sign_ctx->buffer_capacity = new_capacity;
  }

  memcpy(sign_ctx->buffer + sign_ctx->buffer_len, data, data_len);
  sign_ctx->buffer_len = new_len;

  return JS_DupValue(ctx, this_val);
}

// Sign.sign(privateKey, [outputEncoding])
static JSValue js_node_sign_sign(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeSign* sign_ctx = JS_GetOpaque2(ctx, this_val, js_node_sign_class_id);
  if (!sign_ctx) {
    return JS_EXCEPTION;
  }

  if (sign_ctx->finalized) {
    return JS_ThrowTypeError(ctx, "Sign already called");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "sign.sign() requires privateKey argument");
  }

  sign_ctx->finalized = true;

  // Extract private key from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[0], "__keyData");
  if (JS_IsUndefined(key_data_val)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid private key");
  }

  size_t key_data_len;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_data_len, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid private key data");
  }

  // Get algorithm from key
  JSValue alg_obj = JS_GetPropertyStr(ctx, argv[0], "algorithm");
  JSValue alg_name = JS_GetPropertyStr(ctx, alg_obj, "name");
  const char* algorithm_name = JS_ToCString(ctx, alg_name);
  JS_FreeValue(ctx, alg_name);
  JS_FreeValue(ctx, alg_obj);

  uint8_t* signature = NULL;
  size_t signature_len = 0;
  JSValue js_result = JS_UNDEFINED;

  // Determine if RSA or ECDSA based on algorithm name
  if (algorithm_name && (strstr(algorithm_name, "RSA") != NULL)) {
    // RSA signing
    void* pkey = jsrt_crypto_rsa_create_private_key_from_der(key_data, key_data_len);
    if (!pkey) {
      JS_FreeCString(ctx, algorithm_name);
      JS_FreeValue(ctx, key_data_val);
      return JS_ThrowInternalError(ctx, "Failed to load RSA private key");
    }

    jsrt_rsa_params_t params = {0};
    params.rsa_key = pkey;
    params.algorithm = sign_ctx->algorithm;
    params.hash_algorithm = sign_ctx->hash_algorithm;

    int result = jsrt_crypto_rsa_sign(&params, sign_ctx->buffer, sign_ctx->buffer_len, &signature, &signature_len);

    // Free the key directly
    jsrt_evp_pkey_free_wrapper(pkey);

    if (result != 0 || !signature) {
      if (signature)
        free(signature);
      JS_FreeCString(ctx, algorithm_name);
      JS_FreeValue(ctx, key_data_val);
      return JS_ThrowInternalError(ctx, "RSA signing failed");
    }
  } else if (algorithm_name && (strstr(algorithm_name, "ECDSA") != NULL)) {
    // ECDSA signing
    void* pkey = jsrt_ec_create_private_key_from_der(key_data, key_data_len);
    if (!pkey) {
      JS_FreeCString(ctx, algorithm_name);
      JS_FreeValue(ctx, key_data_val);
      return JS_ThrowInternalError(ctx, "Failed to load EC private key");
    }

    // Get hash name
    const char* hash_name = jsrt_crypto_rsa_hash_algorithm_to_string(sign_ctx->hash_algorithm);

    jsrt_ecdsa_sign_params_t params = {0};
    params.hash = hash_name;

    JSValue sig_result = jsrt_ec_sign(ctx, pkey, sign_ctx->buffer, sign_ctx->buffer_len, &params);

    // Free the key
    jsrt_evp_pkey_free_wrapper(pkey);

    if (JS_IsException(sig_result)) {
      JS_FreeCString(ctx, algorithm_name);
      JS_FreeValue(ctx, key_data_val);
      return sig_result;
    }

    // Extract signature from result
    signature = JS_GetArrayBuffer(ctx, &signature_len, sig_result);
    if (!signature) {
      JS_FreeValue(ctx, sig_result);
      JS_FreeCString(ctx, algorithm_name);
      JS_FreeValue(ctx, key_data_val);
      return JS_ThrowInternalError(ctx, "Failed to extract ECDSA signature");
    }

    // Copy signature data
    uint8_t* signature_copy = malloc(signature_len);
    if (!signature_copy) {
      JS_FreeValue(ctx, sig_result);
      JS_FreeCString(ctx, algorithm_name);
      JS_FreeValue(ctx, key_data_val);
      return JS_ThrowOutOfMemory(ctx);
    }
    memcpy(signature_copy, signature, signature_len);
    signature = signature_copy;
    JS_FreeValue(ctx, sig_result);
  } else {
    JS_FreeCString(ctx, algorithm_name);
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Unsupported key type");
  }

  JS_FreeCString(ctx, algorithm_name);
  JS_FreeValue(ctx, key_data_val);

  // Handle output encoding
  const char* encoding = NULL;
  if (argc > 1 && JS_IsString(argv[1])) {
    encoding = JS_ToCString(ctx, argv[1]);
  }

  if (encoding && strcmp(encoding, "hex") == 0) {
    // Hex encoding
    char* hex = js_malloc(ctx, signature_len * 2 + 1);
    if (!hex) {
      free(signature);
      if (encoding)
        JS_FreeCString(ctx, encoding);
      return JS_ThrowOutOfMemory(ctx);
    }
    for (size_t i = 0; i < signature_len; i++) {
      sprintf(hex + i * 2, "%02x", signature[i]);
    }
    js_result = JS_NewString(ctx, hex);
    js_free(ctx, hex);
    free(signature);
  } else if (encoding && strcmp(encoding, "base64") == 0) {
    // Base64 encoding - create buffer first then encode
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, signature, signature_len);
    free(signature);

    JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
    JSValue uint8_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

    // Use btoa for base64 encoding
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
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, signature, signature_len);
    free(signature);

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

//==============================================================================
// Verify Class Implementation
//==============================================================================

static void js_node_verify_finalizer(JSRuntime* rt, JSValue val) {
  JSNodeVerify* verify_ctx = JS_GetOpaque(val, js_node_verify_class_id);
  if (verify_ctx) {
    if (verify_ctx->buffer) {
      js_free_rt(rt, verify_ctx->buffer);
    }
    js_free_rt(rt, verify_ctx);
  }
}

static JSClassDef js_node_verify_class = {
    "Verify",
    .finalizer = js_node_verify_finalizer,
};

// Verify.update(data, [inputEncoding])
static JSValue js_node_verify_update(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeVerify* verify_ctx = JS_GetOpaque2(ctx, this_val, js_node_verify_class_id);
  if (!verify_ctx) {
    return JS_EXCEPTION;
  }

  if (verify_ctx->finalized) {
    return JS_ThrowTypeError(ctx, "Verify already called");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "verify.update() requires data argument");
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

  // Append to buffer
  size_t new_len = verify_ctx->buffer_len + data_len;
  if (new_len > verify_ctx->buffer_capacity) {
    size_t new_capacity = new_len * 2;
    if (new_capacity < 1024)
      new_capacity = 1024;
    uint8_t* new_buffer = js_realloc(ctx, verify_ctx->buffer, new_capacity);
    if (!new_buffer) {
      return JS_ThrowOutOfMemory(ctx);
    }
    verify_ctx->buffer = new_buffer;
    verify_ctx->buffer_capacity = new_capacity;
  }

  memcpy(verify_ctx->buffer + verify_ctx->buffer_len, data, data_len);
  verify_ctx->buffer_len = new_len;

  return JS_DupValue(ctx, this_val);
}

// Verify.verify(publicKey, signature, [signatureEncoding])
static JSValue js_node_verify_verify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeVerify* verify_ctx = JS_GetOpaque2(ctx, this_val, js_node_verify_class_id);
  if (!verify_ctx) {
    return JS_EXCEPTION;
  }

  if (verify_ctx->finalized) {
    return JS_ThrowTypeError(ctx, "Verify already called");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "verify.verify() requires publicKey and signature arguments");
  }

  verify_ctx->finalized = true;

  // Extract public key from CryptoKey object
  JSValue key_data_val = JS_GetPropertyStr(ctx, argv[0], "__keyData");
  if (JS_IsUndefined(key_data_val)) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid public key");
  }

  size_t key_data_len;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_data_len, key_data_val);
  if (!key_data) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Invalid public key data");
  }

  // Get signature data
  size_t sig_len;
  uint8_t* sig_data = JS_GetArrayBuffer(ctx, &sig_len, argv[1]);
  if (!sig_data) {
    // Try to get as typed array
    JSValue buffer = JS_GetPropertyStr(ctx, argv[1], "buffer");
    if (!JS_IsUndefined(buffer)) {
      sig_data = JS_GetArrayBuffer(ctx, &sig_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!sig_data) {
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "signature must be a Buffer or TypedArray");
  }

  // Get algorithm from key
  JSValue alg_obj = JS_GetPropertyStr(ctx, argv[0], "algorithm");
  JSValue alg_name = JS_GetPropertyStr(ctx, alg_obj, "name");
  const char* algorithm_name = JS_ToCString(ctx, alg_name);
  JS_FreeValue(ctx, alg_name);
  JS_FreeValue(ctx, alg_obj);

  bool verification_result = false;

  // Determine if RSA or ECDSA based on algorithm name
  if (algorithm_name && (strstr(algorithm_name, "RSA") != NULL)) {
    // RSA verification
    void* pkey = jsrt_crypto_rsa_create_public_key_from_der(key_data, key_data_len);
    if (!pkey) {
      JS_FreeCString(ctx, algorithm_name);
      JS_FreeValue(ctx, key_data_val);
      return JS_ThrowInternalError(ctx, "Failed to load RSA public key");
    }

    jsrt_rsa_params_t params = {0};
    params.rsa_key = pkey;
    params.algorithm = verify_ctx->algorithm;
    params.hash_algorithm = verify_ctx->hash_algorithm;

    verification_result =
        jsrt_crypto_rsa_verify(&params, verify_ctx->buffer, verify_ctx->buffer_len, sig_data, sig_len);

    // Free the key directly
    jsrt_evp_pkey_free_wrapper(pkey);
  } else if (algorithm_name && (strstr(algorithm_name, "ECDSA") != NULL)) {
    // ECDSA verification
    void* pkey = jsrt_ec_create_public_key_from_der(key_data, key_data_len);
    if (!pkey) {
      JS_FreeCString(ctx, algorithm_name);
      JS_FreeValue(ctx, key_data_val);
      return JS_ThrowInternalError(ctx, "Failed to load EC public key");
    }

    // Get hash name
    const char* hash_name = jsrt_crypto_rsa_hash_algorithm_to_string(verify_ctx->hash_algorithm);

    jsrt_ecdsa_sign_params_t params = {0};
    params.hash = hash_name;

    JSValue verify_result =
        jsrt_ec_verify(ctx, pkey, sig_data, sig_len, verify_ctx->buffer, verify_ctx->buffer_len, &params);

    // Free the key
    jsrt_evp_pkey_free_wrapper(pkey);

    if (JS_IsException(verify_result)) {
      JS_FreeCString(ctx, algorithm_name);
      JS_FreeValue(ctx, key_data_val);
      return verify_result;
    }

    // Extract boolean result
    verification_result = JS_ToBool(ctx, verify_result);
    JS_FreeValue(ctx, verify_result);
  } else {
    JS_FreeCString(ctx, algorithm_name);
    JS_FreeValue(ctx, key_data_val);
    return JS_ThrowTypeError(ctx, "Unsupported key type");
  }

  JS_FreeCString(ctx, algorithm_name);
  JS_FreeValue(ctx, key_data_val);

  return JS_NewBool(ctx, verification_result);
}

//==============================================================================
// Factory Functions
//==============================================================================

// Parse algorithm string to determine RSA/ECDSA and hash
static int parse_sign_algorithm(const char* algorithm, jsrt_rsa_algorithm_t* rsa_alg,
                                jsrt_rsa_hash_algorithm_t* hash_alg) {
  // Node.js sign algorithms: RSA-SHA256, RSA-SHA384, RSA-SHA512, RSA-PSS, etc.
  // ECDSA: ecdsa-with-SHA256, ecdsa-with-SHA384, ecdsa-with-SHA512

  if (strstr(algorithm, "RSA-PSS") != NULL) {
    *rsa_alg = JSRT_RSA_PSS;
  } else if (strstr(algorithm, "RSA") != NULL) {
    *rsa_alg = JSRT_RSASSA_PKCS1_V1_5;
  } else if (strstr(algorithm, "ecdsa") != NULL || strstr(algorithm, "ECDSA") != NULL) {
    *rsa_alg = JSRT_RSASSA_PKCS1_V1_5;  // Use same enum for simplicity
  } else {
    return -1;  // Unknown algorithm
  }

  // Determine hash algorithm
  if (strstr(algorithm, "SHA1") != NULL || strstr(algorithm, "sha1") != NULL) {
    *hash_alg = JSRT_RSA_HASH_SHA1;
  } else if (strstr(algorithm, "SHA256") != NULL || strstr(algorithm, "sha256") != NULL) {
    *hash_alg = JSRT_RSA_HASH_SHA256;
  } else if (strstr(algorithm, "SHA384") != NULL || strstr(algorithm, "sha384") != NULL) {
    *hash_alg = JSRT_RSA_HASH_SHA384;
  } else if (strstr(algorithm, "SHA512") != NULL || strstr(algorithm, "sha512") != NULL) {
    *hash_alg = JSRT_RSA_HASH_SHA512;
  } else {
    *hash_alg = JSRT_RSA_HASH_SHA256;  // Default
  }

  return 0;
}

// crypto.createSign(algorithm, [options])
JSValue js_crypto_create_sign(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "createSign() requires algorithm argument");
  }

  const char* algorithm = JS_ToCString(ctx, argv[0]);
  if (!algorithm) {
    return JS_EXCEPTION;
  }

  // Parse algorithm
  jsrt_rsa_algorithm_t rsa_alg;
  jsrt_rsa_hash_algorithm_t hash_alg;
  if (parse_sign_algorithm(algorithm, &rsa_alg, &hash_alg) != 0) {
    JS_FreeCString(ctx, algorithm);
    return JS_ThrowTypeError(ctx, "Unsupported sign algorithm");
  }

  JS_FreeCString(ctx, algorithm);

  // Create Sign object
  JSNodeSign* sign_ctx = js_mallocz(ctx, sizeof(JSNodeSign));
  if (!sign_ctx) {
    return JS_ThrowOutOfMemory(ctx);
  }

  sign_ctx->ctx = ctx;
  sign_ctx->algorithm = rsa_alg;
  sign_ctx->hash_algorithm = hash_alg;
  sign_ctx->buffer_capacity = 1024;
  sign_ctx->buffer = js_malloc(ctx, sign_ctx->buffer_capacity);
  if (!sign_ctx->buffer) {
    js_free(ctx, sign_ctx);
    return JS_ThrowOutOfMemory(ctx);
  }
  sign_ctx->buffer_len = 0;
  sign_ctx->finalized = false;

  // Create JS object
  JSValue obj = JS_NewObjectClass(ctx, js_node_sign_class_id);
  if (JS_IsException(obj)) {
    js_free(ctx, sign_ctx->buffer);
    js_free(ctx, sign_ctx);
    return obj;
  }

  JS_SetOpaque(obj, sign_ctx);

  // Add methods
  JS_SetPropertyStr(ctx, obj, "update", JS_NewCFunction(ctx, js_node_sign_update, "update", 2));
  JS_SetPropertyStr(ctx, obj, "sign", JS_NewCFunction(ctx, js_node_sign_sign, "sign", 2));

  return obj;
}

// crypto.createVerify(algorithm, [options])
JSValue js_crypto_create_verify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "createVerify() requires algorithm argument");
  }

  const char* algorithm = JS_ToCString(ctx, argv[0]);
  if (!algorithm) {
    return JS_EXCEPTION;
  }

  // Parse algorithm
  jsrt_rsa_algorithm_t rsa_alg;
  jsrt_rsa_hash_algorithm_t hash_alg;
  if (parse_sign_algorithm(algorithm, &rsa_alg, &hash_alg) != 0) {
    JS_FreeCString(ctx, algorithm);
    return JS_ThrowTypeError(ctx, "Unsupported verify algorithm");
  }

  JS_FreeCString(ctx, algorithm);

  // Create Verify object
  JSNodeVerify* verify_ctx = js_mallocz(ctx, sizeof(JSNodeVerify));
  if (!verify_ctx) {
    return JS_ThrowOutOfMemory(ctx);
  }

  verify_ctx->ctx = ctx;
  verify_ctx->algorithm = rsa_alg;
  verify_ctx->hash_algorithm = hash_alg;
  verify_ctx->buffer_capacity = 1024;
  verify_ctx->buffer = js_malloc(ctx, verify_ctx->buffer_capacity);
  if (!verify_ctx->buffer) {
    js_free(ctx, verify_ctx);
    return JS_ThrowOutOfMemory(ctx);
  }
  verify_ctx->buffer_len = 0;
  verify_ctx->finalized = false;

  // Create JS object
  JSValue obj = JS_NewObjectClass(ctx, js_node_verify_class_id);
  if (JS_IsException(obj)) {
    js_free(ctx, verify_ctx->buffer);
    js_free(ctx, verify_ctx);
    return obj;
  }

  JS_SetOpaque(obj, verify_ctx);

  // Add methods
  JS_SetPropertyStr(ctx, obj, "update", JS_NewCFunction(ctx, js_node_verify_update, "update", 2));
  JS_SetPropertyStr(ctx, obj, "verify", JS_NewCFunction(ctx, js_node_verify_verify, "verify", 3));

  return obj;
}

// Initialize Sign class
void js_node_sign_init_class(JSRuntime* rt) {
  JS_NewClassID(&js_node_sign_class_id);
  JS_NewClass(rt, js_node_sign_class_id, &js_node_sign_class);
}

// Initialize Verify class
void js_node_verify_init_class(JSRuntime* rt) {
  JS_NewClassID(&js_node_verify_class_id);
  JS_NewClass(rt, js_node_verify_class_id, &js_node_verify_class);
}
