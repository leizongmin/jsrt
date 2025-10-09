#include "zlib_internal.h"
#include "../node_modules.h"
#include "../../util/macro.h"
#include <string.h>

// Placeholder functions - to be implemented
static JSValue js_zlib_gzip_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "gzipSync not yet implemented");
}

static JSValue js_zlib_gunzip_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "gunzipSync not yet implemented");
}

static JSValue js_zlib_deflate_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "deflateSync not yet implemented");
}

static JSValue js_zlib_inflate_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "inflateSync not yet implemented");
}

static const JSCFunctionListEntry js_zlib_funcs[] = {
    JS_CFUNC_DEF("gzipSync", 1, js_zlib_gzip_sync),
    JS_CFUNC_DEF("gunzipSync", 1, js_zlib_gunzip_sync),
    JS_CFUNC_DEF("deflateSync", 1, js_zlib_deflate_sync),
    JS_CFUNC_DEF("inflateSync", 1, js_zlib_inflate_sync),
};

static int js_zlib_init_module(JSContext* ctx, JSModuleDef* m) {
  return JS_SetModuleExportList(ctx, m, js_zlib_funcs, countof(js_zlib_funcs));
}

int js_node_zlib_init(JSContext* ctx, JSModuleDef* m) {
  return js_zlib_init_module(ctx, m);
}

JSModuleDef* js_node_zlib_init_module(JSContext* ctx, const char* module_name) {
  JSModuleDef* m = JS_NewCModule(ctx, module_name, js_zlib_init_module);
  if (!m)
    return NULL;
  JS_AddModuleExportList(ctx, m, js_zlib_funcs, countof(js_zlib_funcs));
  return m;
}

JSValue JSRT_InitNodeZlib(JSContext* ctx) {
  JSValue exports = JS_NewObject(ctx);
  if (JS_IsException(exports))
    return JS_EXCEPTION;

  JS_SetPropertyFunctionList(ctx, exports, js_zlib_funcs, countof(js_zlib_funcs));

  return exports;
}
