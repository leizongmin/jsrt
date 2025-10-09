#include "../../crypto/crypto_subtle.h"
#include "../node_modules.h"
#include "node_crypto_internal.h"

//==============================================================================
// KeyObject Class Implementation
// Wraps WebCrypto CryptoKey with Node.js-compatible API
//==============================================================================

// KeyObject opaque structure (wraps CryptoKey)
typedef struct {
  JSContext* ctx;
  JSValue crypto_key;  // WebCrypto CryptoKey object
} JSNodeKeyObject;

static JSClassID js_node_keyobject_class_id;

// KeyObject finalizer
static void js_node_keyobject_finalizer(JSRuntime* rt, JSValue val) {
  JSNodeKeyObject* keyobj = JS_GetOpaque(val, js_node_keyobject_class_id);
  if (keyobj) {
    if (!JS_IsUndefined(keyobj->crypto_key)) {
      JS_FreeValueRT(rt, keyobj->crypto_key);
    }
    js_free_rt(rt, keyobj);
  }
}

static JSClassDef js_node_keyobject_class = {
    .class_name = "KeyObject",
    .finalizer = js_node_keyobject_finalizer,
};

// Initialize KeyObject class
void js_node_keyobject_init_class(JSRuntime* rt) {
  JS_NewClassID(&js_node_keyobject_class_id);
  JS_NewClass(rt, js_node_keyobject_class_id, &js_node_keyobject_class);
}

//==============================================================================
// KeyObject Getters
//==============================================================================

// keyObject.type getter
static JSValue js_keyobject_get_type(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeKeyObject* keyobj = JS_GetOpaque(this_val, js_node_keyobject_class_id);
  if (!keyobj) {
    return JS_ThrowTypeError(ctx, "Invalid KeyObject");
  }

  JSValue type = JS_GetPropertyStr(ctx, keyobj->crypto_key, "type");
  return type;
}

// keyObject.asymmetricKeyType getter
static JSValue js_keyobject_get_asymmetric_key_type(JSContext* ctx, JSValueConst this_val, int argc,
                                                    JSValueConst* argv) {
  JSNodeKeyObject* keyobj = JS_GetOpaque(this_val, js_node_keyobject_class_id);
  if (!keyobj) {
    return JS_ThrowTypeError(ctx, "Invalid KeyObject");
  }

  // Check if it's a secret key first
  JSValue type_val = JS_GetPropertyStr(ctx, keyobj->crypto_key, "type");
  const char* type = JS_ToCString(ctx, type_val);
  JS_FreeValue(ctx, type_val);

  if (strcmp(type, "secret") == 0) {
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
  }
  JS_FreeCString(ctx, type);

  // Get algorithm name from CryptoKey
  JSValue alg_val = JS_GetPropertyStr(ctx, keyobj->crypto_key, "algorithm");
  JSValue name_val = JS_GetPropertyStr(ctx, alg_val, "name");
  const char* alg_name = JS_ToCString(ctx, name_val);
  JS_FreeValue(ctx, name_val);
  JS_FreeValue(ctx, alg_val);

  // Map WebCrypto algorithm names to Node.js asymmetric key types
  JSValue result = JS_UNDEFINED;
  if (strstr(alg_name, "RSA") != NULL) {
    result = JS_NewString(ctx, "rsa");
  } else if (strcmp(alg_name, "ECDSA") == 0 || strcmp(alg_name, "ECDH") == 0) {
    result = JS_NewString(ctx, "ec");
  }

  JS_FreeCString(ctx, alg_name);
  return result;
}

// keyObject.asymmetricKeyDetails getter
static JSValue js_keyobject_get_asymmetric_key_details(JSContext* ctx, JSValueConst this_val, int argc,
                                                       JSValueConst* argv) {
  JSNodeKeyObject* keyobj = JS_GetOpaque(this_val, js_node_keyobject_class_id);
  if (!keyobj) {
    return JS_ThrowTypeError(ctx, "Invalid KeyObject");
  }

  // Check if it's a secret key first
  JSValue type_val = JS_GetPropertyStr(ctx, keyobj->crypto_key, "type");
  const char* type = JS_ToCString(ctx, type_val);
  JS_FreeValue(ctx, type_val);

  if (strcmp(type, "secret") == 0) {
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
  }
  JS_FreeCString(ctx, type);

  // Get algorithm details from CryptoKey
  JSValue alg_val = JS_GetPropertyStr(ctx, keyobj->crypto_key, "algorithm");
  JSValue name_val = JS_GetPropertyStr(ctx, alg_val, "name");
  const char* alg_name = JS_ToCString(ctx, name_val);
  JS_FreeValue(ctx, name_val);

  JSValue details = JS_NewObject(ctx);

  // RSA key details
  if (strstr(alg_name, "RSA") != NULL) {
    JSValue modulus_length = JS_GetPropertyStr(ctx, alg_val, "modulusLength");
    JSValue public_exponent = JS_GetPropertyStr(ctx, alg_val, "publicExponent");

    if (!JS_IsUndefined(modulus_length)) {
      JS_SetPropertyStr(ctx, details, "modulusLength", JS_DupValue(ctx, modulus_length));
    }
    if (!JS_IsUndefined(public_exponent)) {
      // Convert Uint8Array to BigInt for publicExponent
      JS_SetPropertyStr(ctx, details, "publicExponent", JS_DupValue(ctx, public_exponent));
    }

    JS_FreeValue(ctx, modulus_length);
    JS_FreeValue(ctx, public_exponent);
  }
  // EC key details
  else if (strcmp(alg_name, "ECDSA") == 0 || strcmp(alg_name, "ECDH") == 0) {
    JSValue named_curve = JS_GetPropertyStr(ctx, alg_val, "namedCurve");
    if (!JS_IsUndefined(named_curve)) {
      JS_SetPropertyStr(ctx, details, "namedCurve", JS_DupValue(ctx, named_curve));
    }
    JS_FreeValue(ctx, named_curve);
  }

  JS_FreeCString(ctx, alg_name);
  JS_FreeValue(ctx, alg_val);
  return details;
}

// keyObject.symmetricKeySize getter
static JSValue js_keyobject_get_symmetric_key_size(JSContext* ctx, JSValueConst this_val, int argc,
                                                   JSValueConst* argv) {
  JSNodeKeyObject* keyobj = JS_GetOpaque(this_val, js_node_keyobject_class_id);
  if (!keyobj) {
    return JS_ThrowTypeError(ctx, "Invalid KeyObject");
  }

  // Check if it's a secret key
  JSValue type_val = JS_GetPropertyStr(ctx, keyobj->crypto_key, "type");
  const char* type = JS_ToCString(ctx, type_val);
  JS_FreeValue(ctx, type_val);

  if (strcmp(type, "secret") != 0) {
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
  }
  JS_FreeCString(ctx, type);

  // Get key data size
  JSValue key_data = JS_GetPropertyStr(ctx, keyobj->crypto_key, "__keyData");
  if (JS_IsUndefined(key_data)) {
    JS_FreeValue(ctx, key_data);
    return JS_UNDEFINED;
  }

  size_t key_size;
  uint8_t* key_bytes = JS_GetArrayBuffer(ctx, &key_size, key_data);
  JS_FreeValue(ctx, key_data);

  if (!key_bytes) {
    return JS_UNDEFINED;
  }

  return JS_NewInt32(ctx, (int32_t)key_size);
}

//==============================================================================
// keyObject.export(options) - Export key in various formats
//==============================================================================

static JSValue js_keyobject_export(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeKeyObject* keyobj = JS_GetOpaque(this_val, js_node_keyobject_class_id);
  if (!keyobj) {
    return JS_ThrowTypeError(ctx, "Invalid KeyObject");
  }

  // Default options
  const char* format = "pem";
  const char* type = NULL;

  // Parse options object if provided
  if (argc > 0 && JS_IsObject(argv[0])) {
    JSValue format_val = JS_GetPropertyStr(ctx, argv[0], "format");
    if (!JS_IsUndefined(format_val)) {
      format = JS_ToCString(ctx, format_val);
    }
    JS_FreeValue(ctx, format_val);

    JSValue type_val = JS_GetPropertyStr(ctx, argv[0], "type");
    if (!JS_IsUndefined(type_val)) {
      type = JS_ToCString(ctx, type_val);
    }
    JS_FreeValue(ctx, type_val);
  }

  // Get key type
  JSValue key_type_val = JS_GetPropertyStr(ctx, keyobj->crypto_key, "type");
  const char* key_type = JS_ToCString(ctx, key_type_val);
  JS_FreeValue(ctx, key_type_val);

  JSValue result = JS_UNDEFINED;

  // Handle different export formats
  if (strcmp(format, "pem") == 0) {
    // Export as PEM string (requires conversion from DER)
    const char* webcrypto_format = NULL;

    if (strcmp(key_type, "public") == 0) {
      webcrypto_format = "spki";
    } else if (strcmp(key_type, "private") == 0) {
      webcrypto_format = "pkcs8";
    } else if (strcmp(key_type, "secret") == 0) {
      // Secret keys can't be exported as PEM
      JS_FreeCString(ctx, key_type);
      return JS_ThrowTypeError(ctx, "Secret keys cannot be exported in PEM format");
    }

    if (webcrypto_format) {
      // Call WebCrypto exportKey
      JSValue global = JS_GetGlobalObject(ctx);
      JSValue crypto = JS_GetPropertyStr(ctx, global, "crypto");
      JSValue subtle = JS_GetPropertyStr(ctx, crypto, "subtle");
      JSValue export_key = JS_GetPropertyStr(ctx, subtle, "exportKey");

      JSValue args[2];
      args[0] = JS_NewString(ctx, webcrypto_format);
      args[1] = JS_DupValue(ctx, keyobj->crypto_key);

      JSValue promise = JS_Call(ctx, export_key, subtle, 2, args);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);

      // For now, return the promise (caller needs to await)
      // TODO: Consider adding sync version by blocking on promise
      result = promise;

      JS_FreeValue(ctx, export_key);
      JS_FreeValue(ctx, subtle);
      JS_FreeValue(ctx, crypto);
      JS_FreeValue(ctx, global);
    }
  } else if (strcmp(format, "der") == 0) {
    // Export as DER buffer
    const char* webcrypto_format = NULL;

    if (strcmp(key_type, "public") == 0) {
      webcrypto_format = "spki";
    } else if (strcmp(key_type, "private") == 0) {
      webcrypto_format = "pkcs8";
    } else if (strcmp(key_type, "secret") == 0) {
      webcrypto_format = "raw";
    }

    if (webcrypto_format) {
      // Call WebCrypto exportKey
      JSValue global = JS_GetGlobalObject(ctx);
      JSValue crypto = JS_GetPropertyStr(ctx, global, "crypto");
      JSValue subtle = JS_GetPropertyStr(ctx, crypto, "subtle");
      JSValue export_key = JS_GetPropertyStr(ctx, subtle, "exportKey");

      JSValue args[2];
      args[0] = JS_NewString(ctx, webcrypto_format);
      args[1] = JS_DupValue(ctx, keyobj->crypto_key);

      JSValue promise = JS_Call(ctx, export_key, subtle, 2, args);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);

      result = promise;

      JS_FreeValue(ctx, export_key);
      JS_FreeValue(ctx, subtle);
      JS_FreeValue(ctx, crypto);
      JS_FreeValue(ctx, global);
    }
  } else if (strcmp(format, "jwk") == 0) {
    // Export as JWK object
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue crypto = JS_GetPropertyStr(ctx, global, "crypto");
    JSValue subtle = JS_GetPropertyStr(ctx, crypto, "subtle");
    JSValue export_key = JS_GetPropertyStr(ctx, subtle, "exportKey");

    JSValue args[2];
    args[0] = JS_NewString(ctx, "jwk");
    args[1] = JS_DupValue(ctx, keyobj->crypto_key);

    JSValue promise = JS_Call(ctx, export_key, subtle, 2, args);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);

    result = promise;

    JS_FreeValue(ctx, export_key);
    JS_FreeValue(ctx, subtle);
    JS_FreeValue(ctx, crypto);
    JS_FreeValue(ctx, global);
  } else {
    result = JS_ThrowTypeError(ctx, "Unsupported export format");
  }

  JS_FreeCString(ctx, key_type);
  return result;
}

//==============================================================================
// Internal helper: Create KeyObject from CryptoKey (JSCFunction wrapper)
//==============================================================================

JSValue js_node_keyobject_from_cryptokey_wrapper(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "__createKeyObjectFromCryptoKey requires 1 argument");
  }
  return js_node_keyobject_from_cryptokey(ctx, argv[0]);
}

// Internal helper: Create KeyObject from CryptoKey
JSValue js_node_keyobject_from_cryptokey(JSContext* ctx, JSValue crypto_key) {
  JSNodeKeyObject* keyobj = js_mallocz(ctx, sizeof(JSNodeKeyObject));
  if (!keyobj) {
    return JS_ThrowOutOfMemory(ctx);
  }

  keyobj->ctx = ctx;
  keyobj->crypto_key = JS_DupValue(ctx, crypto_key);

  JSValue obj = JS_NewObjectClass(ctx, js_node_keyobject_class_id);
  JS_SetOpaque(obj, keyobj);

  // Add getter properties
  JS_DefinePropertyGetSet(ctx, obj, JS_NewAtom(ctx, "type"), JS_NewCFunction(ctx, js_keyobject_get_type, "get type", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);

  JS_DefinePropertyGetSet(ctx, obj, JS_NewAtom(ctx, "asymmetricKeyType"),
                          JS_NewCFunction(ctx, js_keyobject_get_asymmetric_key_type, "get asymmetricKeyType", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);

  JS_DefinePropertyGetSet(ctx, obj, JS_NewAtom(ctx, "asymmetricKeyDetails"),
                          JS_NewCFunction(ctx, js_keyobject_get_asymmetric_key_details, "get asymmetricKeyDetails", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);

  JS_DefinePropertyGetSet(ctx, obj, JS_NewAtom(ctx, "symmetricKeySize"),
                          JS_NewCFunction(ctx, js_keyobject_get_symmetric_key_size, "get symmetricKeySize", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);

  // Add export method
  JS_SetPropertyStr(ctx, obj, "export", JS_NewCFunction(ctx, js_keyobject_export, "export", 1));

  return obj;
}

//==============================================================================
// KeyObject Factory Functions
//==============================================================================

// createSecretKey(key, encoding)
JSValue js_crypto_create_secret_key(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "createSecretKey requires at least 1 argument");
  }

  // Parse key input (Buffer or TypedArray)
  size_t key_size;
  uint8_t* key_data = NULL;

  // Try ArrayBuffer first
  key_data = JS_GetArrayBuffer(ctx, &key_size, argv[0]);

  // If not ArrayBuffer, try TypedArray
  if (!key_data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, argv[0], "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, argv[0], "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, argv[0], "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

      if (buffer_data) {
        uint32_t offset, length;
        JS_ToUint32(ctx, &offset, byteOffset_val);
        JS_ToUint32(ctx, &length, byteLength_val);

        key_data = buffer_data + offset;
        key_size = length;
      }
    }

    JS_FreeValue(ctx, buffer_val);
    JS_FreeValue(ctx, byteOffset_val);
    JS_FreeValue(ctx, byteLength_val);
  }

  if (!key_data) {
    return JS_ThrowTypeError(ctx, "Key must be a Buffer or TypedArray");
  }

  // Create CryptoKey using WebCrypto importKey with raw format
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue crypto = JS_GetPropertyStr(ctx, global, "crypto");
  JSValue subtle = JS_GetPropertyStr(ctx, crypto, "subtle");
  JSValue import_key = JS_GetPropertyStr(ctx, subtle, "importKey");

  // Create a copy of key data for import
  uint8_t* key_copy = js_malloc(ctx, key_size);
  if (!key_copy) {
    JS_FreeValue(ctx, import_key);
    JS_FreeValue(ctx, subtle);
    JS_FreeValue(ctx, crypto);
    JS_FreeValue(ctx, global);
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(key_copy, key_data, key_size);

  JSValue key_buffer = JS_NewArrayBuffer(ctx, key_copy, key_size, NULL, NULL, 0);

  // Algorithm object for generic secret key
  JSValue alg = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, alg, "name", JS_NewString(ctx, "HMAC"));
  JS_SetPropertyStr(ctx, alg, "hash", JS_NewString(ctx, "SHA-256"));

  // Usages array
  JSValue usages = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, usages, 0, JS_NewString(ctx, "sign"));
  JS_SetPropertyUint32(ctx, usages, 1, JS_NewString(ctx, "verify"));

  JSValue args[5];
  args[0] = JS_NewString(ctx, "raw");
  args[1] = key_buffer;
  args[2] = alg;
  args[3] = JS_NewBool(ctx, true);  // extractable
  args[4] = usages;

  JSValue crypto_key_promise = JS_Call(ctx, import_key, subtle, 5, args);

  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, args[2]);
  JS_FreeValue(ctx, args[3]);
  JS_FreeValue(ctx, args[4]);
  JS_FreeValue(ctx, import_key);
  JS_FreeValue(ctx, subtle);
  JS_FreeValue(ctx, crypto);
  JS_FreeValue(ctx, global);

  // Return promise that resolves to KeyObject
  // Wrap the promise result with KeyObject
  const char* wrapper_code =
      "(async (promise) => {"
      "  const cryptoKey = await promise;"
      "  return globalThis.__createKeyObjectFromCryptoKey(cryptoKey);"
      "})";

  JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<createSecretKey>", JS_EVAL_TYPE_GLOBAL);
  JSValue wrapper_args[1] = {crypto_key_promise};
  JSValue result = JS_Call(ctx, wrapper, JS_UNDEFINED, 1, wrapper_args);

  JS_FreeValue(ctx, wrapper);
  JS_FreeValue(ctx, crypto_key_promise);

  return result;
}

// createPublicKey(key)
JSValue js_crypto_create_public_key(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "createPublicKey requires at least 1 argument");
  }

  JSValue key_input = argv[0];

  // Case 1: Input is already a KeyObject - just return it if it's a public key
  if (JS_IsObject(key_input)) {
    JSNodeKeyObject* existing_keyobj = JS_GetOpaque(key_input, js_node_keyobject_class_id);
    if (existing_keyobj) {
      // Verify it's a public key
      JSValue type_val = JS_GetPropertyStr(ctx, existing_keyobj->crypto_key, "type");
      const char* type = JS_ToCString(ctx, type_val);
      JS_FreeValue(ctx, type_val);

      if (strcmp(type, "public") == 0) {
        JS_FreeCString(ctx, type);
        return JS_DupValue(ctx, key_input);
      }
      JS_FreeCString(ctx, type);
      return JS_ThrowTypeError(ctx, "Input KeyObject is not a public key");
    }

    // Case 2: Input is a JWK object
    JSValue kty_val = JS_GetPropertyStr(ctx, key_input, "kty");
    if (!JS_IsUndefined(kty_val)) {
      JS_FreeValue(ctx, kty_val);

      // Determine algorithm from JWK
      JSValue alg_val = JS_GetPropertyStr(ctx, key_input, "alg");
      const char* alg_str = NULL;
      if (!JS_IsUndefined(alg_val)) {
        alg_str = JS_ToCString(ctx, alg_val);
      }
      JS_FreeValue(ctx, alg_val);

      // Map JWK alg to WebCrypto algorithm
      const char* webcrypto_alg = "RSASSA-PKCS1-v1_5";  // Default
      const char* hash = "SHA-256";

      if (alg_str) {
        if (strcmp(alg_str, "RS256") == 0) {
          webcrypto_alg = "RSASSA-PKCS1-v1_5";
          hash = "SHA-256";
        } else if (strcmp(alg_str, "RS384") == 0) {
          webcrypto_alg = "RSASSA-PKCS1-v1_5";
          hash = "SHA-384";
        } else if (strcmp(alg_str, "RS512") == 0) {
          webcrypto_alg = "RSASSA-PKCS1-v1_5";
          hash = "SHA-512";
        } else if (strcmp(alg_str, "ES256") == 0) {
          webcrypto_alg = "ECDSA";
          hash = "SHA-256";
        } else if (strcmp(alg_str, "ES384") == 0) {
          webcrypto_alg = "ECDSA";
          hash = "SHA-384";
        } else if (strcmp(alg_str, "ES512") == 0) {
          webcrypto_alg = "ECDSA";
          hash = "SHA-512";
        }
        JS_FreeCString(ctx, alg_str);
      }

      // Call WebCrypto importKey with JWK
      JSValue global = JS_GetGlobalObject(ctx);
      JSValue crypto = JS_GetPropertyStr(ctx, global, "crypto");
      JSValue subtle = JS_GetPropertyStr(ctx, crypto, "subtle");
      JSValue import_key = JS_GetPropertyStr(ctx, subtle, "importKey");

      JSValue alg = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx, alg, "name", JS_NewString(ctx, webcrypto_alg));
      JS_SetPropertyStr(ctx, alg, "hash", JS_NewString(ctx, hash));

      JSValue usages = JS_NewArray(ctx);
      JS_SetPropertyUint32(ctx, usages, 0, JS_NewString(ctx, "verify"));

      JSValue args[5];
      args[0] = JS_NewString(ctx, "jwk");
      args[1] = JS_DupValue(ctx, key_input);
      args[2] = alg;
      args[3] = JS_NewBool(ctx, true);  // extractable
      args[4] = usages;

      JSValue crypto_key_promise = JS_Call(ctx, import_key, subtle, 5, args);

      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
      JS_FreeValue(ctx, args[2]);
      JS_FreeValue(ctx, args[3]);
      JS_FreeValue(ctx, args[4]);
      JS_FreeValue(ctx, import_key);
      JS_FreeValue(ctx, subtle);
      JS_FreeValue(ctx, crypto);
      JS_FreeValue(ctx, global);

      // Wrap promise result with KeyObject
      const char* wrapper_code =
          "(async (promise) => {"
          "  const cryptoKey = await promise;"
          "  return globalThis.__createKeyObjectFromCryptoKey(cryptoKey);"
          "})";

      JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<createPublicKey>", JS_EVAL_TYPE_GLOBAL);
      JSValue wrapper_args[1] = {crypto_key_promise};
      JSValue result = JS_Call(ctx, wrapper, JS_UNDEFINED, 1, wrapper_args);

      JS_FreeValue(ctx, wrapper);
      JS_FreeValue(ctx, crypto_key_promise);

      return result;
    }
  }

  // Case 3: Input is a string (could be PEM)
  if (JS_IsString(key_input)) {
    const char* key_str = JS_ToCString(ctx, key_input);
    if (!key_str) {
      return JS_ThrowTypeError(ctx, "Invalid key string");
    }

    // Check if it's PEM format
    if (strstr(key_str, "-----BEGIN") != NULL) {
      // Simple PEM to DER conversion (remove header/footer and decode base64)
      // For now, return not fully implemented
      JS_FreeCString(ctx, key_str);
      return JS_ThrowTypeError(ctx, "PEM format not yet fully supported - use DER or JWK instead");
    }

    JS_FreeCString(ctx, key_str);
  }

  // Case 4: Input is a Buffer/TypedArray (DER format)
  size_t der_size;
  uint8_t* der_data = JS_GetArrayBuffer(ctx, &der_size, key_input);

  // If not ArrayBuffer, try TypedArray
  if (!der_data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, key_input, "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, key_input, "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, key_input, "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

      if (buffer_data) {
        uint32_t offset, length;
        JS_ToUint32(ctx, &offset, byteOffset_val);
        JS_ToUint32(ctx, &length, byteLength_val);

        der_data = buffer_data + offset;
        der_size = length;
      }
    }

    JS_FreeValue(ctx, buffer_val);
    JS_FreeValue(ctx, byteOffset_val);
    JS_FreeValue(ctx, byteLength_val);
  }

  if (!der_data) {
    return JS_ThrowTypeError(ctx, "Key must be a KeyObject, JWK, PEM string, or DER buffer");
  }

  // Import DER format (SPKI) as public key
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue crypto = JS_GetPropertyStr(ctx, global, "crypto");
  JSValue subtle = JS_GetPropertyStr(ctx, crypto, "subtle");
  JSValue import_key = JS_GetPropertyStr(ctx, subtle, "importKey");

  // Create a copy of DER data for import
  uint8_t* der_copy = js_malloc(ctx, der_size);
  if (!der_copy) {
    JS_FreeValue(ctx, import_key);
    JS_FreeValue(ctx, subtle);
    JS_FreeValue(ctx, crypto);
    JS_FreeValue(ctx, global);
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(der_copy, der_data, der_size);

  JSValue der_buffer = JS_NewArrayBuffer(ctx, der_copy, der_size, NULL, NULL, 0);

  // Try RSA first (most common for public keys)
  JSValue alg = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, alg, "name", JS_NewString(ctx, "RSASSA-PKCS1-v1_5"));
  JS_SetPropertyStr(ctx, alg, "hash", JS_NewString(ctx, "SHA-256"));

  JSValue usages = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, usages, 0, JS_NewString(ctx, "verify"));

  JSValue args[5];
  args[0] = JS_NewString(ctx, "spki");
  args[1] = der_buffer;
  args[2] = alg;
  args[3] = JS_NewBool(ctx, true);  // extractable
  args[4] = usages;

  JSValue crypto_key_promise = JS_Call(ctx, import_key, subtle, 5, args);

  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, args[2]);
  JS_FreeValue(ctx, args[3]);
  JS_FreeValue(ctx, args[4]);
  JS_FreeValue(ctx, import_key);
  JS_FreeValue(ctx, subtle);
  JS_FreeValue(ctx, crypto);
  JS_FreeValue(ctx, global);

  // Wrap promise result with KeyObject
  const char* wrapper_code =
      "(async (promise) => {"
      "  try {"
      "    const cryptoKey = await promise;"
      "    return globalThis.__createKeyObjectFromCryptoKey(cryptoKey);"
      "  } catch (e) {"
      "    throw new TypeError('Failed to import public key: ' + e.message);"
      "  }"
      "})";

  JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<createPublicKey>", JS_EVAL_TYPE_GLOBAL);
  JSValue wrapper_args[1] = {crypto_key_promise};
  JSValue result = JS_Call(ctx, wrapper, JS_UNDEFINED, 1, wrapper_args);

  JS_FreeValue(ctx, wrapper);
  JS_FreeValue(ctx, crypto_key_promise);

  return result;
}

// createPrivateKey(key)
JSValue js_crypto_create_private_key(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "createPrivateKey requires at least 1 argument");
  }

  JSValue key_input = argv[0];

  // Case 1: Input is already a KeyObject - just return it if it's a private key
  if (JS_IsObject(key_input)) {
    JSNodeKeyObject* existing_keyobj = JS_GetOpaque(key_input, js_node_keyobject_class_id);
    if (existing_keyobj) {
      // Verify it's a private key
      JSValue type_val = JS_GetPropertyStr(ctx, existing_keyobj->crypto_key, "type");
      const char* type = JS_ToCString(ctx, type_val);
      JS_FreeValue(ctx, type_val);

      if (strcmp(type, "private") == 0) {
        JS_FreeCString(ctx, type);
        return JS_DupValue(ctx, key_input);
      }
      JS_FreeCString(ctx, type);
      return JS_ThrowTypeError(ctx, "Input KeyObject is not a private key");
    }

    // Case 2: Input is a JWK object
    JSValue kty_val = JS_GetPropertyStr(ctx, key_input, "kty");
    if (!JS_IsUndefined(kty_val)) {
      JS_FreeValue(ctx, kty_val);

      // Determine algorithm from JWK
      JSValue alg_val = JS_GetPropertyStr(ctx, key_input, "alg");
      const char* alg_str = NULL;
      if (!JS_IsUndefined(alg_val)) {
        alg_str = JS_ToCString(ctx, alg_val);
      }
      JS_FreeValue(ctx, alg_val);

      // Map JWK alg to WebCrypto algorithm
      const char* webcrypto_alg = "RSASSA-PKCS1-v1_5";  // Default
      const char* hash = "SHA-256";

      if (alg_str) {
        if (strcmp(alg_str, "RS256") == 0) {
          webcrypto_alg = "RSASSA-PKCS1-v1_5";
          hash = "SHA-256";
        } else if (strcmp(alg_str, "RS384") == 0) {
          webcrypto_alg = "RSASSA-PKCS1-v1_5";
          hash = "SHA-384";
        } else if (strcmp(alg_str, "RS512") == 0) {
          webcrypto_alg = "RSASSA-PKCS1-v1_5";
          hash = "SHA-512";
        } else if (strcmp(alg_str, "ES256") == 0) {
          webcrypto_alg = "ECDSA";
          hash = "SHA-256";
        } else if (strcmp(alg_str, "ES384") == 0) {
          webcrypto_alg = "ECDSA";
          hash = "SHA-384";
        } else if (strcmp(alg_str, "ES512") == 0) {
          webcrypto_alg = "ECDSA";
          hash = "SHA-512";
        }
        JS_FreeCString(ctx, alg_str);
      }

      // Call WebCrypto importKey with JWK
      JSValue global = JS_GetGlobalObject(ctx);
      JSValue crypto = JS_GetPropertyStr(ctx, global, "crypto");
      JSValue subtle = JS_GetPropertyStr(ctx, crypto, "subtle");
      JSValue import_key = JS_GetPropertyStr(ctx, subtle, "importKey");

      JSValue alg = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx, alg, "name", JS_NewString(ctx, webcrypto_alg));
      JS_SetPropertyStr(ctx, alg, "hash", JS_NewString(ctx, hash));

      JSValue usages = JS_NewArray(ctx);
      JS_SetPropertyUint32(ctx, usages, 0, JS_NewString(ctx, "sign"));

      JSValue args[5];
      args[0] = JS_NewString(ctx, "jwk");
      args[1] = JS_DupValue(ctx, key_input);
      args[2] = alg;
      args[3] = JS_NewBool(ctx, true);  // extractable
      args[4] = usages;

      JSValue crypto_key_promise = JS_Call(ctx, import_key, subtle, 5, args);

      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
      JS_FreeValue(ctx, args[2]);
      JS_FreeValue(ctx, args[3]);
      JS_FreeValue(ctx, args[4]);
      JS_FreeValue(ctx, import_key);
      JS_FreeValue(ctx, subtle);
      JS_FreeValue(ctx, crypto);
      JS_FreeValue(ctx, global);

      // Wrap promise result with KeyObject
      const char* wrapper_code =
          "(async (promise) => {"
          "  const cryptoKey = await promise;"
          "  return globalThis.__createKeyObjectFromCryptoKey(cryptoKey);"
          "})";

      JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<createPrivateKey>", JS_EVAL_TYPE_GLOBAL);
      JSValue wrapper_args[1] = {crypto_key_promise};
      JSValue result = JS_Call(ctx, wrapper, JS_UNDEFINED, 1, wrapper_args);

      JS_FreeValue(ctx, wrapper);
      JS_FreeValue(ctx, crypto_key_promise);

      return result;
    }
  }

  // Case 3: Input is a string (could be PEM)
  if (JS_IsString(key_input)) {
    const char* key_str = JS_ToCString(ctx, key_input);
    if (!key_str) {
      return JS_ThrowTypeError(ctx, "Invalid key string");
    }

    // Check if it's PEM format
    if (strstr(key_str, "-----BEGIN") != NULL) {
      // Simple PEM to DER conversion (remove header/footer and decode base64)
      // For now, return not fully implemented
      JS_FreeCString(ctx, key_str);
      return JS_ThrowTypeError(ctx, "PEM format not yet fully supported - use DER or JWK instead");
    }

    JS_FreeCString(ctx, key_str);
  }

  // Case 4: Input is a Buffer/TypedArray (DER format)
  size_t der_size;
  uint8_t* der_data = JS_GetArrayBuffer(ctx, &der_size, key_input);

  // If not ArrayBuffer, try TypedArray
  if (!der_data) {
    JSValue buffer_val = JS_GetPropertyStr(ctx, key_input, "buffer");
    JSValue byteOffset_val = JS_GetPropertyStr(ctx, key_input, "byteOffset");
    JSValue byteLength_val = JS_GetPropertyStr(ctx, key_input, "byteLength");

    if (!JS_IsUndefined(buffer_val) && !JS_IsUndefined(byteOffset_val) && !JS_IsUndefined(byteLength_val)) {
      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);

      if (buffer_data) {
        uint32_t offset, length;
        JS_ToUint32(ctx, &offset, byteOffset_val);
        JS_ToUint32(ctx, &length, byteLength_val);

        der_data = buffer_data + offset;
        der_size = length;
      }
    }

    JS_FreeValue(ctx, buffer_val);
    JS_FreeValue(ctx, byteOffset_val);
    JS_FreeValue(ctx, byteLength_val);
  }

  if (!der_data) {
    return JS_ThrowTypeError(ctx, "Key must be a KeyObject, JWK, PEM string, or DER buffer");
  }

  // Import DER format (PKCS8) as private key
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue crypto = JS_GetPropertyStr(ctx, global, "crypto");
  JSValue subtle = JS_GetPropertyStr(ctx, crypto, "subtle");
  JSValue import_key = JS_GetPropertyStr(ctx, subtle, "importKey");

  // Create a copy of DER data for import
  uint8_t* der_copy = js_malloc(ctx, der_size);
  if (!der_copy) {
    JS_FreeValue(ctx, import_key);
    JS_FreeValue(ctx, subtle);
    JS_FreeValue(ctx, crypto);
    JS_FreeValue(ctx, global);
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(der_copy, der_data, der_size);

  JSValue der_buffer = JS_NewArrayBuffer(ctx, der_copy, der_size, NULL, NULL, 0);

  // Try RSA first (most common for private keys)
  JSValue alg = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, alg, "name", JS_NewString(ctx, "RSASSA-PKCS1-v1_5"));
  JS_SetPropertyStr(ctx, alg, "hash", JS_NewString(ctx, "SHA-256"));

  JSValue usages = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, usages, 0, JS_NewString(ctx, "sign"));

  JSValue args[5];
  args[0] = JS_NewString(ctx, "pkcs8");
  args[1] = der_buffer;
  args[2] = alg;
  args[3] = JS_NewBool(ctx, true);  // extractable
  args[4] = usages;

  JSValue crypto_key_promise = JS_Call(ctx, import_key, subtle, 5, args);

  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, args[2]);
  JS_FreeValue(ctx, args[3]);
  JS_FreeValue(ctx, args[4]);
  JS_FreeValue(ctx, import_key);
  JS_FreeValue(ctx, subtle);
  JS_FreeValue(ctx, crypto);
  JS_FreeValue(ctx, global);

  // Wrap promise result with KeyObject
  const char* wrapper_code =
      "(async (promise) => {"
      "  try {"
      "    const cryptoKey = await promise;"
      "    return globalThis.__createKeyObjectFromCryptoKey(cryptoKey);"
      "  } catch (e) {"
      "    throw new TypeError('Failed to import private key: ' + e.message);"
      "  }"
      "})";

  JSValue wrapper = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<createPrivateKey>", JS_EVAL_TYPE_GLOBAL);
  JSValue wrapper_args[1] = {crypto_key_promise};
  JSValue result = JS_Call(ctx, wrapper, JS_UNDEFINED, 1, wrapper_args);

  JS_FreeValue(ctx, wrapper);
  JS_FreeValue(ctx, crypto_key_promise);

  return result;
}
