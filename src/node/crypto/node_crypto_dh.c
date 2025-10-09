#include "node_crypto_internal.h"

//==============================================================================
// ECDH Class Implementation (Phase 7)
//==============================================================================

// Forward declarations
static JSValue js_ecdh_get_public_key(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// ECDH class structure
typedef struct {
  JSContext* ctx;
  jsrt_ec_curve_t curve;
  void* private_key;        // EVP_PKEY* for private key
  void* public_key;         // EVP_PKEY* for public key
  uint8_t* public_key_der;  // DER-encoded public key
  size_t public_key_der_len;
  uint8_t* private_key_der;  // DER-encoded private key
  size_t private_key_der_len;
  bool keys_generated;
} JSNodeECDH;

// ECDH class ID
JSClassID js_node_ecdh_class_id;

// ECDH finalizer
static void js_node_ecdh_finalizer(JSRuntime* rt, JSValue val) {
  JSNodeECDH* ecdh = JS_GetOpaque(val, js_node_ecdh_class_id);
  if (ecdh) {
    if (ecdh->private_key) {
      jsrt_evp_pkey_free_wrapper(ecdh->private_key);
      ecdh->private_key = NULL;
    }
    if (ecdh->public_key) {
      jsrt_evp_pkey_free_wrapper(ecdh->public_key);
      ecdh->public_key = NULL;
    }
    if (ecdh->public_key_der) {
      js_free(ecdh->ctx, ecdh->public_key_der);
      ecdh->public_key_der = NULL;
    }
    if (ecdh->private_key_der) {
      js_free(ecdh->ctx, ecdh->private_key_der);
      ecdh->private_key_der = NULL;
    }
    js_free(ecdh->ctx, ecdh);
  }
}

// ECDH class definition
static JSClassDef js_node_ecdh_class = {
    "ECDH",
    .finalizer = js_node_ecdh_finalizer,
};

// Initialize ECDH class
void js_node_ecdh_init_class(JSRuntime* rt) {
  JS_NewClassID(&js_node_ecdh_class_id);
  JS_NewClass(rt, js_node_ecdh_class_id, &js_node_ecdh_class);
}

// Helper: Decode input data with encoding
static uint8_t* decode_input(JSContext* ctx, JSValue input, const char* encoding, size_t* out_len) {
  uint8_t* data = NULL;
  size_t data_len = 0;

  // Handle Buffer/Uint8Array
  size_t size;
  uint8_t* buf = JS_GetArrayBuffer(ctx, &size, input);
  if (buf) {
    data = js_malloc(ctx, size);
    if (data) {
      memcpy(data, buf, size);
      data_len = size;
    }
  } else {
    // Handle string with encoding
    const char* str = JS_ToCString(ctx, input);
    if (!str) {
      return NULL;
    }

    if (!encoding || strcmp(encoding, "buffer") == 0) {
      // Treat as latin1/binary string
      data_len = strlen(str);
      data = js_malloc(ctx, data_len);
      if (data) {
        memcpy(data, str, data_len);
      }
    } else if (strcmp(encoding, "hex") == 0) {
      // Decode hex string
      data_len = strlen(str) / 2;
      data = js_malloc(ctx, data_len);
      if (data) {
        for (size_t i = 0; i < data_len; i++) {
          int hi = (str[i * 2] >= 'a') ? (str[i * 2] - 'a' + 10) : (str[i * 2] - '0');
          int lo = (str[i * 2 + 1] >= 'a') ? (str[i * 2 + 1] - 'a' + 10) : (str[i * 2 + 1] - '0');
          data[i] = (hi << 4) | lo;
        }
      }
    } else if (strcmp(encoding, "base64") == 0) {
      // Simplified base64 decode (basic implementation)
      // For production, use a proper base64 library
      JS_FreeCString(ctx, str);
      return NULL;  // Not implemented for brevity
    }

    JS_FreeCString(ctx, str);
  }

  *out_len = data_len;
  return data;
}

// Helper: Encode output data with encoding
static JSValue encode_output(JSContext* ctx, const uint8_t* data, size_t data_len, const char* encoding) {
  if (!encoding || strcmp(encoding, "buffer") == 0) {
    return JS_NewArrayBufferCopy(ctx, data, data_len);
  } else if (strcmp(encoding, "hex") == 0) {
    char* hex = js_malloc(ctx, data_len * 2 + 1);
    if (!hex) {
      return JS_EXCEPTION;
    }
    for (size_t i = 0; i < data_len; i++) {
      sprintf(hex + i * 2, "%02x", data[i]);
    }
    JSValue result = JS_NewString(ctx, hex);
    js_free(ctx, hex);
    return result;
  } else if (strcmp(encoding, "base64") == 0) {
    char* b64 = node_crypto_base64_encode(data, data_len);
    if (!b64) {
      return JS_EXCEPTION;
    }
    JSValue result = JS_NewString(ctx, b64);
    free(b64);
    return result;
  }
  return JS_UNDEFINED;
}

//==============================================================================
// ECDH Instance Methods
//==============================================================================

// ecdh.generateKeys()
static JSValue js_ecdh_generate_keys(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeECDH* ecdh = JS_GetOpaque(this_val, js_node_ecdh_class_id);
  if (!ecdh) {
    return JS_ThrowTypeError(ctx, "Not an ECDH instance");
  }

  // Generate EC key pair
  jsrt_ec_keygen_params_t params = {
      .algorithm = JSRT_ECDH, .curve = ecdh->curve, .hash = NULL  // ECDH doesn't need hash
  };

  JSValue keypair = jsrt_ec_generate_key(ctx, &params);
  if (JS_IsException(keypair)) {
    return keypair;
  }

  // Extract private and public keys
  JSValue private_key_obj = JS_GetPropertyStr(ctx, keypair, "privateKey");
  JSValue public_key_obj = JS_GetPropertyStr(ctx, keypair, "publicKey");

  // Get key data
  JSValue priv_data = JS_GetPropertyStr(ctx, private_key_obj, "__keyData");
  JSValue pub_data = JS_GetPropertyStr(ctx, public_key_obj, "__keyData");

  // Load keys from DER
  size_t priv_len, pub_len;
  uint8_t* priv_buf = JS_GetArrayBuffer(ctx, &priv_len, priv_data);
  uint8_t* pub_buf = JS_GetArrayBuffer(ctx, &pub_len, pub_data);

  if (priv_buf && pub_buf) {
    // Store DER-encoded keys
    ecdh->public_key_der = js_malloc(ctx, pub_len);
    ecdh->private_key_der = js_malloc(ctx, priv_len);
    if (ecdh->public_key_der && ecdh->private_key_der) {
      memcpy(ecdh->public_key_der, pub_buf, pub_len);
      ecdh->public_key_der_len = pub_len;
      memcpy(ecdh->private_key_der, priv_buf, priv_len);
      ecdh->private_key_der_len = priv_len;

      // Load EVP_PKEY structures
      ecdh->private_key = jsrt_ec_create_private_key_from_der(priv_buf, priv_len);
      ecdh->public_key = jsrt_ec_create_public_key_from_der(pub_buf, pub_len);
      ecdh->keys_generated = true;
    }
  }

  JS_FreeValue(ctx, priv_data);
  JS_FreeValue(ctx, pub_data);
  JS_FreeValue(ctx, private_key_obj);
  JS_FreeValue(ctx, public_key_obj);
  JS_FreeValue(ctx, keypair);

  if (!ecdh->keys_generated) {
    return JS_ThrowInternalError(ctx, "Failed to generate ECDH keys");
  }

  // Return public key (for Node.js compatibility, generateKeys returns the public key)
  return js_ecdh_get_public_key(ctx, this_val, 0, NULL);
}

// ecdh.computeSecret(otherPublicKey, inputEncoding, outputEncoding)
static JSValue js_ecdh_compute_secret(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeECDH* ecdh = JS_GetOpaque(this_val, js_node_ecdh_class_id);
  if (!ecdh) {
    return JS_ThrowTypeError(ctx, "Not an ECDH instance");
  }

  if (!ecdh->keys_generated || !ecdh->private_key) {
    return JS_ThrowTypeError(ctx, "Keys not generated");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing other public key");
  }

  // Get input and output encodings
  const char* input_encoding = (argc > 1) ? JS_ToCString(ctx, argv[1]) : NULL;
  const char* output_encoding = (argc > 2) ? JS_ToCString(ctx, argv[2]) : NULL;

  // Decode other public key
  size_t pub_key_len;
  uint8_t* pub_key_data = decode_input(ctx, argv[0], input_encoding, &pub_key_len);

  if (input_encoding)
    JS_FreeCString(ctx, input_encoding);

  if (!pub_key_data) {
    if (output_encoding)
      JS_FreeCString(ctx, output_encoding);
    return JS_ThrowTypeError(ctx, "Failed to decode public key");
  }

  // Load other public key from DER
  void* other_public_key = jsrt_ec_create_public_key_from_der(pub_key_data, pub_key_len);
  js_free(ctx, pub_key_data);

  if (!other_public_key) {
    if (output_encoding)
      JS_FreeCString(ctx, output_encoding);
    return JS_ThrowTypeError(ctx, "Invalid public key");
  }

  // Perform ECDH derivation
  jsrt_ecdh_derive_params_t derive_params = {.public_key = other_public_key, .public_key_len = 0};

  JSValue shared_secret = jsrt_ec_derive_bits(ctx, ecdh->private_key, &derive_params);
  jsrt_evp_pkey_free_wrapper(other_public_key);

  if (JS_IsException(shared_secret)) {
    if (output_encoding)
      JS_FreeCString(ctx, output_encoding);
    return shared_secret;
  }

  // Encode output
  if (output_encoding && strcmp(output_encoding, "buffer") != 0) {
    size_t secret_len;
    uint8_t* secret_buf = JS_GetArrayBuffer(ctx, &secret_len, shared_secret);
    JSValue encoded = encode_output(ctx, secret_buf, secret_len, output_encoding);
    JS_FreeValue(ctx, shared_secret);
    JS_FreeCString(ctx, output_encoding);
    return encoded;
  }

  if (output_encoding)
    JS_FreeCString(ctx, output_encoding);
  return shared_secret;
}

// ecdh.getPublicKey(encoding, format)
static JSValue js_ecdh_get_public_key(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeECDH* ecdh = JS_GetOpaque(this_val, js_node_ecdh_class_id);
  if (!ecdh) {
    return JS_ThrowTypeError(ctx, "Not an ECDH instance");
  }

  if (!ecdh->keys_generated || !ecdh->public_key) {
    return JS_ThrowTypeError(ctx, "Keys not generated");
  }

  const char* encoding = (argc > 0) ? JS_ToCString(ctx, argv[0]) : NULL;
  const char* format = (argc > 1) ? JS_ToCString(ctx, argv[1]) : NULL;

  // Return the DER-encoded public key
  // Note: Node.js supports 'compressed', 'uncompressed', 'hybrid' formats
  // This implementation returns DER format which contains the uncompressed point

  JSValue result = encode_output(ctx, ecdh->public_key_der, ecdh->public_key_der_len, encoding);

  if (encoding)
    JS_FreeCString(ctx, encoding);
  if (format)
    JS_FreeCString(ctx, format);

  return result;
}

// ecdh.getPrivateKey(encoding)
static JSValue js_ecdh_get_private_key(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeECDH* ecdh = JS_GetOpaque(this_val, js_node_ecdh_class_id);
  if (!ecdh) {
    return JS_ThrowTypeError(ctx, "Not an ECDH instance");
  }

  if (!ecdh->keys_generated || !ecdh->private_key) {
    return JS_ThrowTypeError(ctx, "Keys not generated");
  }

  const char* encoding = (argc > 0) ? JS_ToCString(ctx, argv[0]) : NULL;

  // Return the DER-encoded private key
  JSValue result = encode_output(ctx, ecdh->private_key_der, ecdh->private_key_der_len, encoding);

  if (encoding)
    JS_FreeCString(ctx, encoding);

  return result;
}

// ecdh.setPrivateKey(privateKey, encoding)
static JSValue js_ecdh_set_private_key(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeECDH* ecdh = JS_GetOpaque(this_val, js_node_ecdh_class_id);
  if (!ecdh) {
    return JS_ThrowTypeError(ctx, "Not an ECDH instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing private key");
  }

  const char* encoding = (argc > 1) ? JS_ToCString(ctx, argv[1]) : NULL;

  // Decode private key
  size_t priv_key_len;
  uint8_t* priv_key_data = decode_input(ctx, argv[0], encoding, &priv_key_len);

  if (encoding)
    JS_FreeCString(ctx, encoding);

  if (!priv_key_data) {
    return JS_ThrowTypeError(ctx, "Failed to decode private key");
  }

  // Free existing private key
  if (ecdh->private_key) {
    jsrt_evp_pkey_free_wrapper(ecdh->private_key);
  }

  // Load new private key
  ecdh->private_key = jsrt_ec_create_private_key_from_der(priv_key_data, priv_key_len);
  js_free(ctx, priv_key_data);

  if (!ecdh->private_key) {
    return JS_ThrowTypeError(ctx, "Invalid private key");
  }

  ecdh->keys_generated = true;
  return JS_UNDEFINED;
}

// ecdh.setPublicKey(publicKey, encoding) - deprecated but supported
static JSValue js_ecdh_set_public_key(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNodeECDH* ecdh = JS_GetOpaque(this_val, js_node_ecdh_class_id);
  if (!ecdh) {
    return JS_ThrowTypeError(ctx, "Not an ECDH instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing public key");
  }

  const char* encoding = (argc > 1) ? JS_ToCString(ctx, argv[1]) : NULL;

  // Decode public key
  size_t pub_key_len;
  uint8_t* pub_key_data = decode_input(ctx, argv[0], encoding, &pub_key_len);

  if (encoding)
    JS_FreeCString(ctx, encoding);

  if (!pub_key_data) {
    return JS_ThrowTypeError(ctx, "Failed to decode public key");
  }

  // Free existing public key
  if (ecdh->public_key) {
    jsrt_evp_pkey_free_wrapper(ecdh->public_key);
  }

  // Load new public key
  ecdh->public_key = jsrt_ec_create_public_key_from_der(pub_key_data, pub_key_len);
  js_free(ctx, pub_key_data);

  if (!ecdh->public_key) {
    return JS_ThrowTypeError(ctx, "Invalid public key");
  }

  return JS_UNDEFINED;
}

//==============================================================================
// createECDH Factory Function
//==============================================================================

JSValue js_crypto_create_ecdh(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing curve name");
  }

  const char* curve_name = JS_ToCString(ctx, argv[0]);
  if (!curve_name) {
    return JS_ThrowTypeError(ctx, "Invalid curve name");
  }

  // Map curve name to jsrt_ec_curve_t
  jsrt_ec_curve_t curve;
  if (strcmp(curve_name, "prime256v1") == 0 || strcmp(curve_name, "P-256") == 0 ||
      strcmp(curve_name, "secp256r1") == 0) {
    curve = JSRT_EC_P256;
  } else if (strcmp(curve_name, "secp384r1") == 0 || strcmp(curve_name, "P-384") == 0) {
    curve = JSRT_EC_P384;
  } else if (strcmp(curve_name, "secp521r1") == 0 || strcmp(curve_name, "P-521") == 0) {
    curve = JSRT_EC_P521;
  } else {
    JS_FreeCString(ctx, curve_name);
    return JS_ThrowTypeError(ctx, "Unsupported curve: %s", curve_name);
  }

  JS_FreeCString(ctx, curve_name);

  // Create ECDH instance
  JSNodeECDH* ecdh = js_mallocz(ctx, sizeof(JSNodeECDH));
  if (!ecdh) {
    return JS_EXCEPTION;
  }

  ecdh->ctx = ctx;
  ecdh->curve = curve;
  ecdh->private_key = NULL;
  ecdh->public_key = NULL;
  ecdh->public_key_der = NULL;
  ecdh->public_key_der_len = 0;
  ecdh->private_key_der = NULL;
  ecdh->private_key_der_len = 0;
  ecdh->keys_generated = false;

  // Create JS object
  JSValue obj = JS_NewObjectClass(ctx, js_node_ecdh_class_id);
  if (JS_IsException(obj)) {
    js_free(ctx, ecdh);
    return obj;
  }

  JS_SetOpaque(obj, ecdh);

  // Add methods
  JS_SetPropertyStr(ctx, obj, "generateKeys", JS_NewCFunction(ctx, js_ecdh_generate_keys, "generateKeys", 0));
  JS_SetPropertyStr(ctx, obj, "computeSecret", JS_NewCFunction(ctx, js_ecdh_compute_secret, "computeSecret", 3));
  JS_SetPropertyStr(ctx, obj, "getPublicKey", JS_NewCFunction(ctx, js_ecdh_get_public_key, "getPublicKey", 2));
  JS_SetPropertyStr(ctx, obj, "getPrivateKey", JS_NewCFunction(ctx, js_ecdh_get_private_key, "getPrivateKey", 1));
  JS_SetPropertyStr(ctx, obj, "setPrivateKey", JS_NewCFunction(ctx, js_ecdh_set_private_key, "setPrivateKey", 2));
  JS_SetPropertyStr(ctx, obj, "setPublicKey", JS_NewCFunction(ctx, js_ecdh_set_public_key, "setPublicKey", 2));

  return obj;
}
