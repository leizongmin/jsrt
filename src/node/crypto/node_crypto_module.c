#include "../node_modules.h"
#include "node_crypto_internal.h"

// Include all implementation files
#include "node_crypto_cipher.c"
#include "node_crypto_dh.c"
#include "node_crypto_hash.c"
#include "node_crypto_hmac.c"
#include "node_crypto_kdf.c"
#include "node_crypto_keyobject.c"
#include "node_crypto_random.c"
#include "node_crypto_sign.c"
#include "node_crypto_util.c"

//==============================================================================
// Module Registration and Exports
//==============================================================================

// CommonJS module export
JSValue JSRT_InitNodeCrypto(JSContext* ctx) {
  // Register Hash, Hmac, Cipher, Sign, Verify, ECDH, DH and KeyObject classes
  JSRuntime* rt = JS_GetRuntime(ctx);
  js_node_hash_init_class(rt);
  js_node_hmac_init_class(rt);
  js_node_cipher_init_class(rt);
  js_node_sign_init_class(rt);
  js_node_verify_init_class(rt);
  js_node_ecdh_init_class(rt);
  js_node_dh_init_class(rt);
  js_node_keyobject_init_class(rt);

  JSValue crypto_obj = JS_NewObject(ctx);

  // Add crypto functions
  JS_SetPropertyStr(ctx, crypto_obj, "createHash", JS_NewCFunction(ctx, js_crypto_create_hash, "createHash", 2));
  JS_SetPropertyStr(ctx, crypto_obj, "createHmac", JS_NewCFunction(ctx, js_crypto_create_hmac, "createHmac", 3));
  JS_SetPropertyStr(ctx, crypto_obj, "createCipheriv",
                    JS_NewCFunction(ctx, js_crypto_create_cipheriv, "createCipheriv", 4));
  JS_SetPropertyStr(ctx, crypto_obj, "createDecipheriv",
                    JS_NewCFunction(ctx, js_crypto_create_decipheriv, "createDecipheriv", 4));
  JS_SetPropertyStr(ctx, crypto_obj, "createSign", JS_NewCFunction(ctx, js_crypto_create_sign, "createSign", 2));
  JS_SetPropertyStr(ctx, crypto_obj, "createVerify", JS_NewCFunction(ctx, js_crypto_create_verify, "createVerify", 2));
  JS_SetPropertyStr(ctx, crypto_obj, "randomBytes", JS_NewCFunction(ctx, js_crypto_random_bytes, "randomBytes", 2));
  JS_SetPropertyStr(ctx, crypto_obj, "randomUUID", JS_NewCFunction(ctx, js_crypto_random_uuid, "randomUUID", 0));

  // Add KDF functions
  JS_SetPropertyStr(ctx, crypto_obj, "pbkdf2", JS_NewCFunction(ctx, js_crypto_pbkdf2, "pbkdf2", 6));
  JS_SetPropertyStr(ctx, crypto_obj, "pbkdf2Sync", JS_NewCFunction(ctx, js_crypto_pbkdf2_sync, "pbkdf2Sync", 5));
  JS_SetPropertyStr(ctx, crypto_obj, "hkdf", JS_NewCFunction(ctx, js_crypto_hkdf, "hkdf", 6));
  JS_SetPropertyStr(ctx, crypto_obj, "hkdfSync", JS_NewCFunction(ctx, js_crypto_hkdf_sync, "hkdfSync", 5));
  JS_SetPropertyStr(ctx, crypto_obj, "scrypt", JS_NewCFunction(ctx, js_crypto_scrypt, "scrypt", 5));
  JS_SetPropertyStr(ctx, crypto_obj, "scryptSync", JS_NewCFunction(ctx, js_crypto_scrypt_sync, "scryptSync", 4));

  // Add ECDH and DH functions
  JS_SetPropertyStr(ctx, crypto_obj, "createECDH", JS_NewCFunction(ctx, js_crypto_create_ecdh, "createECDH", 1));
  JS_SetPropertyStr(ctx, crypto_obj, "createDiffieHellman",
                    JS_NewCFunction(ctx, js_crypto_create_diffie_hellman, "createDiffieHellman", 2));

  // Add KeyObject factory functions
  JS_SetPropertyStr(ctx, crypto_obj, "createSecretKey",
                    JS_NewCFunction(ctx, js_crypto_create_secret_key, "createSecretKey", 2));
  JS_SetPropertyStr(ctx, crypto_obj, "createPublicKey",
                    JS_NewCFunction(ctx, js_crypto_create_public_key, "createPublicKey", 1));
  JS_SetPropertyStr(ctx, crypto_obj, "createPrivateKey",
                    JS_NewCFunction(ctx, js_crypto_create_private_key, "createPrivateKey", 1));

  // Add Key Generation functions
  JS_SetPropertyStr(ctx, crypto_obj, "generateKeyPairSync",
                    JS_NewCFunction(ctx, js_crypto_generate_keypair_sync, "generateKeyPairSync", 2));
  JS_SetPropertyStr(ctx, crypto_obj, "generateKeySync",
                    JS_NewCFunction(ctx, js_crypto_generate_key_sync, "generateKeySync", 2));

  // Add internal helper for KeyObject creation (used by factory functions)
  JSValue global_this = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(
      ctx, global_this, "__createKeyObjectFromCryptoKey",
      JS_NewCFunction(ctx, js_node_keyobject_from_cryptokey_wrapper, "__createKeyObjectFromCryptoKey", 1));
  JS_FreeValue(ctx, global_this);

  // Add constants
  JS_SetPropertyStr(ctx, crypto_obj, "constants", create_crypto_constants(ctx));

  // Add webcrypto and subtle aliases (reference globalThis.crypto)
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue global_crypto = JS_GetPropertyStr(ctx, global_obj, "crypto");
  if (!JS_IsUndefined(global_crypto)) {
    JS_SetPropertyStr(ctx, crypto_obj, "webcrypto", JS_DupValue(ctx, global_crypto));
    JSValue subtle = JS_GetPropertyStr(ctx, global_crypto, "subtle");
    if (!JS_IsUndefined(subtle)) {
      JS_SetPropertyStr(ctx, crypto_obj, "subtle", subtle);
    }
  }
  JS_FreeValue(ctx, global_crypto);
  JS_FreeValue(ctx, global_obj);

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
  JS_SetModuleExport(ctx, m, "createCipheriv", JS_GetPropertyStr(ctx, crypto_module, "createCipheriv"));
  JS_SetModuleExport(ctx, m, "createDecipheriv", JS_GetPropertyStr(ctx, crypto_module, "createDecipheriv"));
  JS_SetModuleExport(ctx, m, "createSign", JS_GetPropertyStr(ctx, crypto_module, "createSign"));
  JS_SetModuleExport(ctx, m, "createVerify", JS_GetPropertyStr(ctx, crypto_module, "createVerify"));
  JS_SetModuleExport(ctx, m, "randomBytes", JS_GetPropertyStr(ctx, crypto_module, "randomBytes"));
  JS_SetModuleExport(ctx, m, "randomUUID", JS_GetPropertyStr(ctx, crypto_module, "randomUUID"));
  JS_SetModuleExport(ctx, m, "pbkdf2", JS_GetPropertyStr(ctx, crypto_module, "pbkdf2"));
  JS_SetModuleExport(ctx, m, "pbkdf2Sync", JS_GetPropertyStr(ctx, crypto_module, "pbkdf2Sync"));
  JS_SetModuleExport(ctx, m, "hkdf", JS_GetPropertyStr(ctx, crypto_module, "hkdf"));
  JS_SetModuleExport(ctx, m, "hkdfSync", JS_GetPropertyStr(ctx, crypto_module, "hkdfSync"));
  JS_SetModuleExport(ctx, m, "scrypt", JS_GetPropertyStr(ctx, crypto_module, "scrypt"));
  JS_SetModuleExport(ctx, m, "scryptSync", JS_GetPropertyStr(ctx, crypto_module, "scryptSync"));
  JS_SetModuleExport(ctx, m, "createECDH", JS_GetPropertyStr(ctx, crypto_module, "createECDH"));
  JS_SetModuleExport(ctx, m, "createDiffieHellman", JS_GetPropertyStr(ctx, crypto_module, "createDiffieHellman"));
  JS_SetModuleExport(ctx, m, "createSecretKey", JS_GetPropertyStr(ctx, crypto_module, "createSecretKey"));
  JS_SetModuleExport(ctx, m, "createPublicKey", JS_GetPropertyStr(ctx, crypto_module, "createPublicKey"));
  JS_SetModuleExport(ctx, m, "createPrivateKey", JS_GetPropertyStr(ctx, crypto_module, "createPrivateKey"));
  JS_SetModuleExport(ctx, m, "generateKeyPairSync", JS_GetPropertyStr(ctx, crypto_module, "generateKeyPairSync"));
  JS_SetModuleExport(ctx, m, "generateKeySync", JS_GetPropertyStr(ctx, crypto_module, "generateKeySync"));
  JS_SetModuleExport(ctx, m, "constants", JS_GetPropertyStr(ctx, crypto_module, "constants"));
  JS_SetModuleExport(ctx, m, "webcrypto", JS_GetPropertyStr(ctx, crypto_module, "webcrypto"));
  JS_SetModuleExport(ctx, m, "subtle", JS_GetPropertyStr(ctx, crypto_module, "subtle"));

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", crypto_module);

  return 0;
}
