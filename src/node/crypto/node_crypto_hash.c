#include "node_crypto_internal.h"

//==============================================================================
// Hash Class Implementation (Node.js createHash API)
// Uses buffering strategy: accumulate data in update(), compute in digest()
//==============================================================================

JSClassID js_node_hash_class_id;

static void js_node_hash_finalizer(JSRuntime* rt, JSValue val) {
  JSNodeHash* hash = JS_GetOpaque(val, js_node_hash_class_id);
  if (hash) {
    if (hash->buffer) {
      js_free_rt(rt, hash->buffer);
    }
    js_free_rt(rt, hash);
  }
}

static JSClassDef js_node_hash_class = {
    "Hash",
    .finalizer = js_node_hash_finalizer,
};

// Hash.update(data, [inputEncoding])
static JSValue js_node_hash_update(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeHash* hash = JS_GetOpaque2(ctx, this_val, js_node_hash_class_id);
  if (!hash) {
    return JS_EXCEPTION;
  }

  if (hash->finalized) {
    return JS_ThrowTypeError(ctx, "Digest already called");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "hash.update() requires data argument");
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
  size_t new_len = hash->buffer_len + data_len;
  if (new_len > hash->buffer_capacity) {
    size_t new_capacity = new_len * 2;
    if (new_capacity < 1024)
      new_capacity = 1024;
    uint8_t* new_buffer = js_realloc(ctx, hash->buffer, new_capacity);
    if (!new_buffer) {
      return JS_ThrowOutOfMemory(ctx);
    }
    hash->buffer = new_buffer;
    hash->buffer_capacity = new_capacity;
  }

  memcpy(hash->buffer + hash->buffer_len, data, data_len);
  hash->buffer_len = new_len;

  return JS_DupValue(ctx, this_val);
}

// Hash.digest([outputEncoding])
static JSValue js_node_hash_digest(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeHash* hash = JS_GetOpaque2(ctx, this_val, js_node_hash_class_id);
  if (!hash) {
    return JS_EXCEPTION;
  }

  if (hash->finalized) {
    return JS_ThrowTypeError(ctx, "Digest already called");
  }

  hash->finalized = true;

  // Compute digest using jsrt_crypto_digest_data
  uint8_t* digest_data = NULL;
  size_t digest_size = 0;

  int result = jsrt_crypto_digest_data(hash->algorithm, hash->buffer, hash->buffer_len, &digest_data, &digest_size);

  if (result != 0 || !digest_data) {
    if (digest_data)
      free(digest_data);
    return JS_ThrowInternalError(ctx, "Digest computation failed");
  }

  // Handle output encoding
  const char* encoding = NULL;
  if (argc > 0 && JS_IsString(argv[0])) {
    encoding = JS_ToCString(ctx, argv[0]);
  }

  JSValue js_result;
  if (encoding && strcmp(encoding, "hex") == 0) {
    // Hex encoding
    char* hex = js_malloc(ctx, digest_size * 2 + 1);
    if (!hex) {
      free(digest_data);
      if (encoding)
        JS_FreeCString(ctx, encoding);
      return JS_ThrowOutOfMemory(ctx);
    }
    for (size_t i = 0; i < digest_size; i++) {
      sprintf(hex + i * 2, "%02x", digest_data[i]);
    }
    js_result = JS_NewString(ctx, hex);
    js_free(ctx, hex);
    free(digest_data);
  } else if (encoding && (strcmp(encoding, "base64") == 0)) {
    // Base64 encoding - create buffer first then encode
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, digest_data, digest_size);
    free(digest_data);

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
    JSValue array_buffer = JS_NewArrayBufferCopy(ctx, digest_data, digest_size);
    free(digest_data);

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

// crypto.createHash(algorithm, [options])
JSValue js_crypto_create_hash(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "createHash() requires algorithm argument");
  }

  const char* algorithm = JS_ToCString(ctx, argv[0]);
  if (!algorithm) {
    return JS_EXCEPTION;
  }

  // Map algorithm name to jsrt_crypto_algorithm_t
  jsrt_crypto_algorithm_t alg = JSRT_CRYPTO_ALG_UNKNOWN;
  if (strcmp(algorithm, "sha256") == 0 || strcmp(algorithm, "SHA-256") == 0) {
    alg = JSRT_CRYPTO_ALG_SHA256;
  } else if (strcmp(algorithm, "sha384") == 0 || strcmp(algorithm, "SHA-384") == 0) {
    alg = JSRT_CRYPTO_ALG_SHA384;
  } else if (strcmp(algorithm, "sha512") == 0 || strcmp(algorithm, "SHA-512") == 0) {
    alg = JSRT_CRYPTO_ALG_SHA512;
  } else if (strcmp(algorithm, "sha1") == 0 || strcmp(algorithm, "SHA-1") == 0) {
    alg = JSRT_CRYPTO_ALG_SHA1;
  }

  JS_FreeCString(ctx, algorithm);

  if (alg == JSRT_CRYPTO_ALG_UNKNOWN) {
    return JS_ThrowTypeError(ctx, "Unsupported hash algorithm");
  }

  // Create Hash object
  JSNodeHash* hash = js_mallocz(ctx, sizeof(JSNodeHash));
  if (!hash) {
    return JS_ThrowOutOfMemory(ctx);
  }

  hash->ctx = ctx;
  hash->algorithm = alg;
  hash->buffer_capacity = 1024;
  hash->buffer = js_malloc(ctx, hash->buffer_capacity);
  if (!hash->buffer) {
    js_free(ctx, hash);
    return JS_ThrowOutOfMemory(ctx);
  }
  hash->buffer_len = 0;
  hash->finalized = false;

  // Create JS object
  JSValue obj = JS_NewObjectClass(ctx, js_node_hash_class_id);
  if (JS_IsException(obj)) {
    js_free(ctx, hash->buffer);
    js_free(ctx, hash);
    return obj;
  }

  JS_SetOpaque(obj, hash);

  // Add methods
  JS_SetPropertyStr(ctx, obj, "update", JS_NewCFunction(ctx, js_node_hash_update, "update", 2));
  JS_SetPropertyStr(ctx, obj, "digest", JS_NewCFunction(ctx, js_node_hash_digest, "digest", 1));

  return obj;
}

// Initialize Hash class
void js_node_hash_init_class(JSRuntime* rt) {
  JS_NewClassID(&js_node_hash_class_id);
  JS_NewClass(rt, js_node_hash_class_id, &js_node_hash_class);
}
