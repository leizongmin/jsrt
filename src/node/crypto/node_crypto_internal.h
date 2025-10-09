#ifndef __JSRT_NODE_CRYPTO_INTERNAL_H__
#define __JSRT_NODE_CRYPTO_INTERNAL_H__

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../../crypto/crypto_digest.h"
#include "../../crypto/crypto_hmac.h"
#include "../../crypto/crypto_subtle.h"
#include "../../crypto/crypto_symmetric.h"
#include "../../util/debug.h"

// Forward declarations for class IDs
extern JSClassID js_node_hash_class_id;
extern JSClassID js_node_hmac_class_id;

// Hash class structure
typedef struct {
  JSContext* ctx;
  jsrt_crypto_algorithm_t algorithm;
  uint8_t* buffer;
  size_t buffer_len;
  size_t buffer_capacity;
  bool finalized;
} JSNodeHash;

// HMAC class structure
typedef struct {
  JSContext* ctx;
  jsrt_hmac_algorithm_t algorithm;
  uint8_t* key_data;
  size_t key_length;
  uint8_t* buffer;
  size_t buffer_len;
  size_t buffer_capacity;
  bool finalized;
} JSNodeHmac;

// Hash API functions
JSValue js_crypto_create_hash(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_node_hash_init_class(JSRuntime* rt);

// HMAC API functions
JSValue js_crypto_create_hmac(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_node_hmac_init_class(JSRuntime* rt);

// Random API functions
JSValue js_crypto_random_bytes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_crypto_random_uuid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Constants
JSValue create_crypto_constants(JSContext* ctx);

#endif  // __JSRT_NODE_CRYPTO_INTERNAL_H__
