#include "../node_modules.h"
#include "node_crypto_internal.h"

// Include all implementation files
#include "node_crypto_hash.c"
#include "node_crypto_hmac.c"
#include "node_crypto_random.c"
#include "node_crypto_util.c"

//==============================================================================
// Module Registration and Exports
//==============================================================================

// CommonJS module export
JSValue JSRT_InitNodeCrypto(JSContext* ctx) {
  // Register Hash and Hmac classes
  JSRuntime* rt = JS_GetRuntime(ctx);
  js_node_hash_init_class(rt);
  js_node_hmac_init_class(rt);

  JSValue crypto_obj = JS_NewObject(ctx);

  // Add crypto functions
  JS_SetPropertyStr(ctx, crypto_obj, "createHash", JS_NewCFunction(ctx, js_crypto_create_hash, "createHash", 2));
  JS_SetPropertyStr(ctx, crypto_obj, "createHmac", JS_NewCFunction(ctx, js_crypto_create_hmac, "createHmac", 3));
  JS_SetPropertyStr(ctx, crypto_obj, "randomBytes", JS_NewCFunction(ctx, js_crypto_random_bytes, "randomBytes", 2));
  JS_SetPropertyStr(ctx, crypto_obj, "randomUUID", JS_NewCFunction(ctx, js_crypto_random_uuid, "randomUUID", 0));

  // Add constants
  JS_SetPropertyStr(ctx, crypto_obj, "constants", create_crypto_constants(ctx));

  // Export as default export for CommonJS
  JS_SetPropertyStr(ctx, crypto_obj, "default", JS_DupValue(ctx, crypto_obj));

  return crypto_obj;
}

// ES Module initialization
int js_node_crypto_init(JSContext* ctx, JSModuleDef* m) {
  JSValue crypto_module = JSRT_InitNodeCrypto(ctx);

  // Export individual functions
  JS_SetModuleExport(ctx, m, "createHash", JS_GetPropertyStr(ctx, crypto_module, "createHash"));
  JS_SetModuleExport(ctx, m, "createHmac", JS_GetPropertyStr(ctx, crypto_module, "createHmac"));
  JS_SetModuleExport(ctx, m, "randomBytes", JS_GetPropertyStr(ctx, crypto_module, "randomBytes"));
  JS_SetModuleExport(ctx, m, "randomUUID", JS_GetPropertyStr(ctx, crypto_module, "randomUUID"));
  JS_SetModuleExport(ctx, m, "constants", JS_GetPropertyStr(ctx, crypto_module, "constants"));

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", crypto_module);

  return 0;
}
