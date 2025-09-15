#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "process.h"

// External function declarations
extern const char* JSRT_GetOpenSSLVersion(void);

// Helper function to get jsrt version from compile-time macro
static char* get_jsrt_version(void) {
  static char* cached_version = NULL;
  static int version_loaded = 0;

  if (version_loaded) {
    return cached_version;
  }

  version_loaded = 1;

#ifdef JSRT_VERSION
  cached_version = strdup(JSRT_VERSION);
#else
  // Try to read from VERSION file or use fallback
  cached_version = strdup("1.0.0");
#endif

  return cached_version;
}

// Process version getter (with 'v' prefix)
JSValue js_process_get_version(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  char* version = get_jsrt_version();
  if (version != NULL) {
    // Add 'v' prefix to version for Node.js compatibility
    char* prefixed_version = malloc(strlen(version) + 2);
    if (prefixed_version) {
      sprintf(prefixed_version, "v%s", version);
      JSValue result = JS_NewString(ctx, prefixed_version);
      free(prefixed_version);
      return result;
    }
  }
  return JS_NewString(ctx, "v1.0.0");  // Fallback
}

// Process versions object getter
JSValue js_process_get_versions(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue versions_obj = JS_NewObject(ctx);

  if (JS_IsException(versions_obj)) {
    return JS_EXCEPTION;
  }

  // Add jsrt version
  char* version = get_jsrt_version();
  if (version != NULL) {
    JS_SetPropertyStr(ctx, versions_obj, "jsrt", JS_NewString(ctx, version));
  } else {
    JS_SetPropertyStr(ctx, versions_obj, "jsrt", JS_NewString(ctx, "1.0.0"));
  }

  // Add libuv version
  const char* uv_version = uv_version_string();
  if (uv_version != NULL) {
    JS_SetPropertyStr(ctx, versions_obj, "uv", JS_NewString(ctx, uv_version));
  }

  // Add OpenSSL version if available
  const char* openssl_version = JSRT_GetOpenSSLVersion();
  if (openssl_version != NULL) {
    JS_SetPropertyStr(ctx, versions_obj, "openssl", JS_NewString(ctx, openssl_version));
  }

  // Add QuickJS version from compile-time macro
#ifdef QUICKJS_VERSION
  JS_SetPropertyStr(ctx, versions_obj, "quickjs", JS_NewString(ctx, QUICKJS_VERSION));
#else
  JS_SetPropertyStr(ctx, versions_obj, "quickjs", JS_NewString(ctx, "unknown"));
#endif

  // Add Node.js compatibility version
  JS_SetPropertyStr(ctx, versions_obj, "node", JS_NewString(ctx, "18.0.0"));

  // Add V8 compatibility version (for compatibility)
  JS_SetPropertyStr(ctx, versions_obj, "v8", JS_NewString(ctx, "10.2.154.26"));

  return versions_obj;
}

void jsrt_process_init_versions(void) {
  // Version initialization if needed
  // Currently no initialization required
}