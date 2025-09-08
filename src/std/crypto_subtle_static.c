// Static OpenSSL build - minimal SubtleCrypto implementation
#include <quickjs.h>
#include "crypto_subtle.h"

// Minimal implementation for static builds
JSValue JSRT_CreateSubtleCrypto(JSContext* ctx) {
  // Return empty object for now
  return JS_NewObject(ctx);
}

void JSRT_SetupSubtleCrypto(JSRT_Runtime* rt) {
  // Do nothing for now
  (void)rt;
}