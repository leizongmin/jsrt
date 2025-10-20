/**
 * WASI Module Registration
 *
 * JavaScript bindings and module registration for WASI class.
 */

#include "../../util/debug.h"
#include "wasi.h"

#include <string.h>

// QuickJS class ID for WASI instances
static JSClassID jsrt_wasi_class_id = 0;

static const char* jsrt_wasi_error_labels[] = {
    [JSRT_WASI_ERROR_INVALID_ARGUMENT] = "ERR_WASI_INVALID_ARGUMENT",
    [JSRT_WASI_ERROR_INVALID_INSTANCE] = "ERR_WASI_INVALID_INSTANCE",
    [JSRT_WASI_ERROR_MISSING_MEMORY_EXPORT] = "ERR_WASI_MISSING_MEMORY_EXPORT",
    [JSRT_WASI_ERROR_MISSING_START_EXPORT] = "ERR_WASI_MISSING_ENTRY_EXPORT",
    [JSRT_WASI_ERROR_ALREADY_STARTED] = "ERR_WASI_ALREADY_STARTED",
    [JSRT_WASI_ERROR_ALREADY_INITIALIZED] = "ERR_WASI_ALREADY_INITIALIZED",
    [JSRT_WASI_ERROR_INTERNAL] = "ERR_WASI_INTERNAL",
};

static const char* jsrt_wasi_error_messages[] = {
    [JSRT_WASI_ERROR_INVALID_ARGUMENT] = "Invalid WASI argument",
    [JSRT_WASI_ERROR_INVALID_INSTANCE] = "Invalid WASI instance",
    [JSRT_WASI_ERROR_MISSING_MEMORY_EXPORT] = "Missing WebAssembly memory export required by WASI",
    [JSRT_WASI_ERROR_MISSING_START_EXPORT] = "Missing required WASI entry export",
    [JSRT_WASI_ERROR_ALREADY_STARTED] = "WASI instance has already started",
    [JSRT_WASI_ERROR_ALREADY_INITIALIZED] = "WASI instance already initialized",
    [JSRT_WASI_ERROR_INTERNAL] = "WASI internal error",
};

const char* jsrt_wasi_error_to_string(jsrt_wasi_error_t code) {
  size_t count = sizeof(jsrt_wasi_error_labels) / sizeof(jsrt_wasi_error_labels[0]);
  if (code < 0 || (size_t)code >= count || jsrt_wasi_error_labels[code] == NULL) {
    return "ERR_WASI_UNKNOWN";
  }
  return jsrt_wasi_error_labels[code];
}

static const char* jsrt_wasi_error_default_message(jsrt_wasi_error_t code) {
  size_t count = sizeof(jsrt_wasi_error_messages) / sizeof(jsrt_wasi_error_messages[0]);
  if (code < 0 || (size_t)code >= count || jsrt_wasi_error_messages[code] == NULL) {
    return "Unknown WASI error";
  }
  return jsrt_wasi_error_messages[code];
}

JSValue jsrt_wasi_throw_error(JSContext* ctx, jsrt_wasi_error_t code, const char* detail) {
  const char* base = jsrt_wasi_error_default_message(code);
  if (code == JSRT_WASI_ERROR_INTERNAL) {
    if (detail) {
      return JS_ThrowInternalError(ctx, "%s: %s", base, detail);
    }
    return JS_ThrowInternalError(ctx, "%s", base);
  }

  if (detail) {
    return JS_ThrowTypeError(ctx, "%s: %s", base, detail);
  }
  return JS_ThrowTypeError(ctx, "%s", base);
}

/**
 * Finalizer for WASI instances (called by garbage collector)
 */
static void jsrt_wasi_finalizer(JSRuntime* rt, JSValue val) {
  jsrt_wasi_t* wasi = JS_GetOpaque(val, jsrt_wasi_class_id);
  if (wasi) {
    JSRT_Debug("WASI finalizer called");
    jsrt_wasi_free(wasi);
  }
}

/**
 * WASI class definition
 */
static JSClassDef jsrt_wasi_class = {
    "WASI",
    .finalizer = jsrt_wasi_finalizer,
};

/**
 * WASI.prototype.getImportObject()
 */
static JSValue js_wasi_get_import_object(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  jsrt_wasi_t* wasi = JS_GetOpaque(this_val, jsrt_wasi_class_id);
  if (!wasi) {
    return JS_ThrowTypeError(ctx, "not a WASI instance");
  }

  return jsrt_wasi_get_import_object(ctx, wasi);
}

/**
 * WASI.prototype.start(instance)
 */
static JSValue js_wasi_start(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  jsrt_wasi_t* wasi = JS_GetOpaque(this_val, jsrt_wasi_class_id);
  if (!wasi) {
    return JS_ThrowTypeError(ctx, "not a WASI instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "start() requires WebAssembly.Instance argument");
  }

  return jsrt_wasi_start(ctx, wasi, argv[0]);
}

/**
 * WASI.prototype.initialize(instance)
 */
static JSValue js_wasi_initialize(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  jsrt_wasi_t* wasi = JS_GetOpaque(this_val, jsrt_wasi_class_id);
  if (!wasi) {
    return JS_ThrowTypeError(ctx, "not a WASI instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "initialize() requires WebAssembly.Instance argument");
  }

  return jsrt_wasi_initialize(ctx, wasi, argv[0]);
}

/**
 * WASI.prototype.wasiImport getter
 */
static JSValue js_wasi_get_wasi_import(JSContext* ctx, JSValueConst this_val) {
  jsrt_wasi_t* wasi = JS_GetOpaque(this_val, jsrt_wasi_class_id);
  if (!wasi) {
    return JS_ThrowTypeError(ctx, "not a WASI instance");
  }

  // wasiImport is same as getImportObject().wasi_snapshot_preview1
  JSValue import_obj = jsrt_wasi_get_import_object(ctx, wasi);
  if (JS_IsException(import_obj)) {
    return JS_EXCEPTION;
  }

  // Get wasi_snapshot_preview1 namespace
  JSValue wasi_import = JS_GetPropertyStr(ctx, import_obj, "wasi_snapshot_preview1");
  JS_FreeValue(ctx, import_obj);

  return wasi_import;
}

/**
 * WASI constructor
 * new WASI(options)
 */
static JSValue js_wasi_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue proto;
  JSValue obj = JS_UNDEFINED;
  jsrt_wasi_options_t options;
  jsrt_wasi_t* wasi = NULL;

  // Parse options
  JSValue options_obj = argc > 0 ? argv[0] : JS_UNDEFINED;
  if (jsrt_wasi_parse_options(ctx, options_obj, &options) < 0) {
    return JS_EXCEPTION;
  }

  // Create C WASI instance
  wasi = jsrt_wasi_new(ctx, &options);
  jsrt_wasi_free_options(&options);  // jsrt_wasi_new() makes a copy

  if (!wasi) {
    return JS_EXCEPTION;
  }

  // Create JS object
  proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if (JS_IsException(proto)) {
    jsrt_wasi_free(wasi);
    return JS_EXCEPTION;
  }

  obj = JS_NewObjectProtoClass(ctx, proto, jsrt_wasi_class_id);
  JS_FreeValue(ctx, proto);

  if (JS_IsException(obj)) {
    jsrt_wasi_free(wasi);
    return JS_EXCEPTION;
  }

  // Attach C instance to JS object
  JS_SetOpaque(obj, wasi);

  JSRT_Debug("WASI constructor: created instance");
  return obj;
}

/**
 * WASI class method definitions
 */
static const JSCFunctionListEntry js_wasi_proto_funcs[] = {
    JS_CFUNC_DEF("getImportObject", 0, js_wasi_get_import_object),
    JS_CFUNC_DEF("start", 1, js_wasi_start),
    JS_CFUNC_DEF("initialize", 1, js_wasi_initialize),
    JS_CGETSET_DEF("wasiImport", js_wasi_get_wasi_import, NULL),
};

/**
 * Initialize WASI module (CommonJS)
 */
JSValue JSRT_InitNodeWASI(JSContext* ctx) {
  JSValue wasi_obj = JS_NewObject(ctx);

  // Register WASI class if not already registered
  if (!jsrt_wasi_class_id) {
    JS_NewClassID(&jsrt_wasi_class_id);
    JS_NewClass(JS_GetRuntime(ctx), jsrt_wasi_class_id, &jsrt_wasi_class);
  }

  // Create WASI constructor
  JSValue wasi_ctor = JS_NewCFunction2(ctx, js_wasi_constructor, "WASI", 1, JS_CFUNC_constructor, 0);

  // Set prototype methods
  JSValue proto = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, proto, js_wasi_proto_funcs,
                             sizeof(js_wasi_proto_funcs) / sizeof(js_wasi_proto_funcs[0]));
  JS_SetConstructor(ctx, wasi_ctor, proto);
  JS_FreeValue(ctx, proto);

  // Export WASI constructor
  JS_SetPropertyStr(ctx, wasi_obj, "WASI", wasi_ctor);

  JSRT_Debug("WASI module initialized (CommonJS)");
  return wasi_obj;
}

/**
 * Initialize WASI module (ESM)
 */
int js_node_wasi_init(JSContext* ctx, JSModuleDef* m) {
  JSValue wasi_obj = JSRT_InitNodeWASI(ctx);

  // Get WASI constructor
  JSValue wasi_ctor = JS_GetPropertyStr(ctx, wasi_obj, "WASI");

  // Export named export: WASI
  JS_SetModuleExport(ctx, m, "WASI", JS_DupValue(ctx, wasi_ctor));

  // Export default: { WASI }
  JS_SetModuleExport(ctx, m, "default", wasi_obj);

  JS_FreeValue(ctx, wasi_ctor);

  JSRT_Debug("WASI module initialized (ESM)");
  return 0;
}

/**
 * Check if module is WASI
 */
bool JSRT_IsWASIModule(const char* name) {
  return name && strcmp(name, "wasi") == 0;
}
