#include "node_crypto_internal.h"

//==============================================================================
// HMAC Class Implementation (Node.js createHmac API)
//==============================================================================

JSClassID js_node_hmac_class_id;

static void js_node_hmac_finalizer(JSRuntime* rt, JSValue val) {
  JSNodeHmac* hmac = JS_GetOpaque(val, js_node_hmac_class_id);
  if (hmac) {
    if (hmac->key_data) {
      js_free_rt(rt, hmac->key_data);
    }
    if (hmac->buffer) {
      js_free_rt(rt, hmac->buffer);
    }
    js_free_rt(rt, hmac);
  }
}

static JSClassDef js_node_hmac_class = {
    "Hmac",
    .finalizer = js_node_hmac_finalizer,
};

// Hmac.update(data, [inputEncoding])
static JSValue js_node_hmac_update(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeHmac* hmac = JS_GetOpaque2(ctx, this_val, js_node_hmac_class_id);
  if (!hmac) {
    return JS_EXCEPTION;
  }

  if (hmac->finalized) {
    return JS_ThrowTypeError(ctx, "Digest already called");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "hmac.update() requires data argument");
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
  size_t new_len = hmac->buffer_len + data_len;
  if (new_len > hmac->buffer_capacity) {
    size_t new_capacity = new_len * 2;
    uint8_t* new_buffer = js_realloc(ctx, hmac->buffer, new_capacity);
    if (!new_buffer) {
      return JS_ThrowOutOfMemory(ctx);
    }
    hmac->buffer = new_buffer;
    hmac->buffer_capacity = new_capacity;
  }

  memcpy(hmac->buffer + hmac->buffer_len, data, data_len);
  hmac->buffer_len = new_len;

  return JS_DupValue(ctx, this_val);
}

// Hmac.digest([outputEncoding])
static JSValue js_node_hmac_digest(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeHmac* hmac = JS_GetOpaque2(ctx, this_val, js_node_hmac_class_id);
  if (!hmac) {
    return JS_EXCEPTION;
  }

  if (hmac->finalized) {
    return JS_ThrowTypeError(ctx, "Digest already called");
  }

  hmac->finalized = true;

  // Prepare HMAC parameters
  jsrt_hmac_params_t params = {0};
  params.algorithm = hmac->algorithm;
  params.key_data = hmac->key_data;
  params.key_length = hmac->key_length;

  // Compute HMAC
  uint8_t* signature = NULL;
  size_t signature_length = 0;

  int result = jsrt_crypto_hmac_sign(&params, hmac->buffer, hmac->buffer_len, &signature, &signature_length);

  if (result != 0 || !signature) {
    if (signature)
      free(signature);
    return JS_ThrowInternalError(ctx, "HMAC computation failed");
  }

  // Handle output encoding
  const char* encoding = NULL;
  if (argc > 0 && JS_IsString(argv[0])) {
    encoding = JS_ToCString(ctx, argv[0]);
  }

  JSValue js_result;
  if (encoding && strcmp(encoding, "hex") == 0) {
    // Hex encoding
    char* hex = js_malloc(ctx, signature_length * 2 + 1);
    if (!hex) {
      free(signature);
      if (encoding)
        JS_FreeCString(ctx, encoding);
      return JS_ThrowOutOfMemory(ctx);
    }
    for (size_t i = 0; i < signature_length; i++) {
      sprintf(hex + i * 2, "%02x", signature[i]);
    }
    js_result = JS_NewString(ctx, hex);
    js_free(ctx, hex);
    free(signature);
  } else if (encoding && strcmp(encoding, "base64") == 0) {
    // Base64 encoding - create buffer first then encode
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, signature, signature_length);
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
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, signature, signature_length);
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

// crypto.createHmac(algorithm, key, [options])
JSValue js_crypto_create_hmac(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "createHmac() requires algorithm and key arguments");
  }

  const char* algorithm = JS_ToCString(ctx, argv[0]);
  if (!algorithm) {
    return JS_EXCEPTION;
  }

  // Map algorithm name to jsrt_hmac_algorithm_t
  jsrt_hmac_algorithm_t alg;
  if (strcmp(algorithm, "sha256") == 0 || strcmp(algorithm, "SHA-256") == 0) {
    alg = JSRT_HMAC_SHA256;
  } else if (strcmp(algorithm, "sha384") == 0 || strcmp(algorithm, "SHA-384") == 0) {
    alg = JSRT_HMAC_SHA384;
  } else if (strcmp(algorithm, "sha512") == 0 || strcmp(algorithm, "SHA-512") == 0) {
    alg = JSRT_HMAC_SHA512;
  } else if (strcmp(algorithm, "sha1") == 0 || strcmp(algorithm, "SHA-1") == 0) {
    alg = JSRT_HMAC_SHA1;
  } else {
    JS_FreeCString(ctx, algorithm);
    return JS_ThrowTypeError(ctx, "Unsupported HMAC algorithm");
  }

  JS_FreeCString(ctx, algorithm);

  // Get key data
  size_t key_len;
  uint8_t* key_data = JS_GetArrayBuffer(ctx, &key_len, argv[1]);

  if (!key_data) {
    // Try to get as typed array
    JSValue buffer = JS_GetPropertyStr(ctx, argv[1], "buffer");
    if (!JS_IsUndefined(buffer)) {
      key_data = JS_GetArrayBuffer(ctx, &key_len, buffer);
      JS_FreeValue(ctx, buffer);
    }
  }

  if (!key_data) {
    return JS_ThrowTypeError(ctx, "key must be a Buffer or TypedArray");
  }

  // Create Hmac object
  JSNodeHmac* hmac = js_mallocz(ctx, sizeof(JSNodeHmac));
  if (!hmac) {
    return JS_ThrowOutOfMemory(ctx);
  }

  hmac->ctx = ctx;
  hmac->algorithm = alg;
  hmac->key_data = js_malloc(ctx, key_len);
  if (!hmac->key_data) {
    js_free(ctx, hmac);
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(hmac->key_data, key_data, key_len);
  hmac->key_length = key_len;

  hmac->buffer_capacity = 1024;
  hmac->buffer = js_malloc(ctx, hmac->buffer_capacity);
  if (!hmac->buffer) {
    js_free(ctx, hmac->key_data);
    js_free(ctx, hmac);
    return JS_ThrowOutOfMemory(ctx);
  }
  hmac->buffer_len = 0;
  hmac->finalized = false;

  // Create JS object
  JSValue obj = JS_NewObjectClass(ctx, js_node_hmac_class_id);
  if (JS_IsException(obj)) {
    js_free(ctx, hmac->buffer);
    js_free(ctx, hmac->key_data);
    js_free(ctx, hmac);
    return obj;
  }

  JS_SetOpaque(obj, hmac);

  // Add methods
  JS_SetPropertyStr(ctx, obj, "update", JS_NewCFunction(ctx, js_node_hmac_update, "update", 2));
  JS_SetPropertyStr(ctx, obj, "digest", JS_NewCFunction(ctx, js_node_hmac_digest, "digest", 1));

  return obj;
}

// Initialize HMAC class
void js_node_hmac_init_class(JSRuntime* rt) {
  JS_NewClassID(&js_node_hmac_class_id);
  JS_NewClass(rt, js_node_hmac_class_id, &js_node_hmac_class);
}
