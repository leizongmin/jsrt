/**
 * WASI Lifecycle Methods
 *
 * Implementation of start() and initialize() methods for WASI instances.
 */

#include "../../std/webassembly.h"
#include "../../util/debug.h"
#include "wasi.h"

#include <string.h>
#include <wasm_export.h>

static void jsrt_wasi_detach_instance(JSContext* ctx, jsrt_wasi_t* wasi) {
  if (!wasi) {
    return;
  }

  if (!JS_IsUndefined(wasi->wasm_instance)) {
    JS_FreeValue(ctx, wasi->wasm_instance);
    wasi->wasm_instance = JS_UNDEFINED;
  }

  if (wasi->exec_env) {
    wasm_runtime_destroy_exec_env(wasi->exec_env);
    wasi->exec_env = NULL;
  }

  wasi->wamr_instance = NULL;
  wasi->memory_validated = false;
  wasi->instance_failed = true;
}

static int jsrt_wasi_attach_instance(JSContext* ctx, jsrt_wasi_t* wasi, JSValueConst instance, JSValue* out_exports) {
  JSRT_Debug("jsrt_wasi_attach_instance: entry (failed=%d, started=%d, initialized=%d)", wasi->instance_failed,
             wasi->started, wasi->initialized);
  if (!JS_IsObject(instance)) {
    jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_ARGUMENT, "Expected WebAssembly.Instance");
    return -1;
  }

  if (wasi->instance_failed) {
    JSRT_Debug("jsrt_wasi_attach_instance: rejecting due to prior failure");
    jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_INSTANCE, "WASI instance cannot be reused after failure");
    return -1;
  }

  if (!JS_IsUndefined(wasi->wasm_instance)) {
    jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_INSTANCE, "WebAssembly.Instance already attached");
    return -1;
  }

  JSValue exports = JS_GetPropertyStr(ctx, instance, "exports");
  if (JS_IsException(exports)) {
    return -1;
  }

  if (!JS_IsObject(exports)) {
    JS_FreeValue(ctx, exports);
    jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_ARGUMENT, "Instance has no exports");
    return -1;
  }

  wasm_module_inst_t module_inst = jsrt_webassembly_get_instance(ctx, instance);
  if (!module_inst) {
    JS_FreeValue(ctx, exports);
    jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INTERNAL, "Failed to extract WAMR instance from WebAssembly.Instance");
    return -1;
  }

  wasm_memory_inst_t memory = wasm_runtime_get_default_memory(module_inst);
  if (!memory) {
    JS_FreeValue(ctx, exports);
    jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_MISSING_MEMORY_EXPORT, NULL);
    return -1;
  }

  if (wasi->exec_env) {
    wasm_runtime_destroy_exec_env(wasi->exec_env);
    wasi->exec_env = NULL;
  }

  wasi->wasm_instance = JS_DupValue(ctx, instance);
  wasi->wamr_instance = module_inst;
  wasi->memory_validated = true;

  *out_exports = exports;
  JSRT_Debug("jsrt_wasi_attach_instance: success");
  return 0;
}

static int jsrt_wasi_require_export_function(JSContext* ctx, JSValue exports, const char* name,
                                             jsrt_wasi_error_t error_code, const char* detail, JSValue* out_fn) {
  JSValue fn = JS_GetPropertyStr(ctx, exports, name);
  if (JS_IsException(fn)) {
    return -1;
  }

  if (!JS_IsFunction(ctx, fn)) {
    JS_FreeValue(ctx, fn);
    jsrt_wasi_throw_error(ctx, error_code, detail);
    return -1;
  }

  *out_fn = fn;
  return 0;
}

JSValue jsrt_wasi_start(JSContext* ctx, jsrt_wasi_t* wasi, JSValue instance) {
  JSRT_Debug("jsrt_wasi_start: entry");
  if (!wasi) {
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_INSTANCE, NULL);
  }

  // Check if already started or initialized
  if (wasi->started) {
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_ALREADY_STARTED, NULL);
  }
  if (wasi->initialized) {
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_ALREADY_INITIALIZED, NULL);
  }

  JSValue exports;
  if (jsrt_wasi_attach_instance(ctx, wasi, instance, &exports) < 0) {
    JSRT_Debug("jsrt_wasi_start: attach failed");
    return JS_EXCEPTION;
  }

  JSValue start_fn;
  if (jsrt_wasi_require_export_function(ctx, exports, "_start", JSRT_WASI_ERROR_MISSING_START_EXPORT,
                                        "_start export not found", &start_fn) < 0) {
    JS_FreeValue(ctx, exports);
    jsrt_wasi_detach_instance(ctx, wasi);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, exports);

  wasi->exec_env = wasm_runtime_create_exec_env(wasi->wamr_instance, 65536);
  if (!wasi->exec_env) {
    JS_FreeValue(ctx, start_fn);
    jsrt_wasi_detach_instance(ctx, wasi);
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INTERNAL, "Failed to create WASM execution environment");
  }

  JSRT_Debug("WAMR instance extracted and execution environment created");

  // Call _start()
  JSRT_Debug("Calling WASI _start()");
  wasi->exit_requested = false;
  JSValue result = JS_Call(ctx, start_fn, JS_UNDEFINED, 0, NULL);
  JS_FreeValue(ctx, start_fn);

  if (JS_IsException(result)) {
    JSValue exception = JS_GetException(ctx);
    if (wasi->exit_requested && wasi->options.return_on_exit) {
      JS_FreeValue(ctx, exception);
      wasi->exit_requested = false;
      wasi->started = true;
      return JS_NewInt32(ctx, wasi->exit_code);
    }
    jsrt_wasi_detach_instance(ctx, wasi);
    JS_Throw(ctx, exception);
    return JS_EXCEPTION;
  }

  // Mark as started
  wasi->started = true;
  wasi->exit_requested = false;

  // Get exit code (if _start returned one)
  // In WASI, _start typically doesn't return a value, but we handle it anyway
  int32_t exit_code = 0;
  if (JS_IsNumber(result)) {
    JS_ToInt32(ctx, &exit_code, result);
  }
  JS_FreeValue(ctx, result);

  wasi->exit_code = exit_code;

  JSRT_Debug("WASI _start() completed with exit code: %d", exit_code);

  // Handle exit behavior
  if (!wasi->options.return_on_exit) {
    return JS_UNDEFINED;
  }

  // Return exit code
  return JS_NewInt32(ctx, exit_code);
}

/**
 * Initialize WASI instance (call _initialize export)
 */
JSValue jsrt_wasi_initialize(JSContext* ctx, jsrt_wasi_t* wasi, JSValue instance) {
  JSRT_Debug("jsrt_wasi_initialize: entry");
  if (!wasi) {
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_INSTANCE, NULL);
  }

  // Check if already started or initialized
  if (wasi->started) {
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_ALREADY_STARTED, NULL);
  }
  if (wasi->initialized) {
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_ALREADY_INITIALIZED, NULL);
  }

  JSValue exports;
  if (jsrt_wasi_attach_instance(ctx, wasi, instance, &exports) < 0) {
    JSRT_Debug("jsrt_wasi_initialize: attach failed");
    return JS_EXCEPTION;
  }

  JSValue init_fn;
  if (jsrt_wasi_require_export_function(ctx, exports, "_initialize", JSRT_WASI_ERROR_MISSING_START_EXPORT,
                                        "_initialize export not found", &init_fn) < 0) {
    JS_FreeValue(ctx, exports);
    jsrt_wasi_detach_instance(ctx, wasi);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, exports);

  wasi->exec_env = wasm_runtime_create_exec_env(wasi->wamr_instance, 65536);
  if (!wasi->exec_env) {
    JS_FreeValue(ctx, init_fn);
    jsrt_wasi_detach_instance(ctx, wasi);
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INTERNAL, "Failed to create WASM execution environment");
  }

  JSRT_Debug("WAMR instance extracted and execution environment created");

  // Call _initialize()
  JSRT_Debug("Calling WASI _initialize()");
  wasi->exit_requested = false;
  JSValue result = JS_Call(ctx, init_fn, JS_UNDEFINED, 0, NULL);
  JS_FreeValue(ctx, init_fn);

  if (JS_IsException(result)) {
    JSValue exception = JS_GetException(ctx);
    if (wasi->exit_requested && wasi->options.return_on_exit) {
      JS_FreeValue(ctx, exception);
      wasi->exit_requested = false;
      wasi->initialized = true;
      return JS_NewInt32(ctx, wasi->exit_code);
    }
    jsrt_wasi_detach_instance(ctx, wasi);
    JS_Throw(ctx, exception);
    return JS_EXCEPTION;
  }

  JS_FreeValue(ctx, result);

  // Mark as initialized
  wasi->initialized = true;
  wasi->exit_requested = false;

  JSRT_Debug("WASI _initialize() completed");

  return JS_UNDEFINED;
}
