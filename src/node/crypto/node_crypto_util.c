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
