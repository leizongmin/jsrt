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

/**
 * Start WASI instance (call _start export)
 */
static JSValue jsrt_wasi_missing_memory(JSContext* ctx, jsrt_wasi_t* wasi) {
  JS_FreeValue(ctx, wasi->wasm_instance);
  wasi->wasm_instance = JS_UNDEFINED;
  wasi->wamr_instance = NULL;
  return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_MISSING_MEMORY_EXPORT, NULL);
}

JSValue jsrt_wasi_start(JSContext* ctx, jsrt_wasi_t* wasi, JSValue instance) {
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

  // Validate instance is a WebAssembly.Instance
  if (!JS_IsObject(instance)) {
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_ARGUMENT, "Expected WebAssembly.Instance");
  }

  // Get exports object
  JSValue exports = JS_GetPropertyStr(ctx, instance, "exports");
  if (JS_IsException(exports)) {
    return JS_EXCEPTION;
  }

  if (!JS_IsObject(exports)) {
    JS_FreeValue(ctx, exports);
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_ARGUMENT, "Instance has no exports");
  }

  // Get _start function
  JSValue start_fn = JS_GetPropertyStr(ctx, exports, "_start");
  JS_FreeValue(ctx, exports);

  if (JS_IsException(start_fn)) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, start_fn)) {
    JS_FreeValue(ctx, start_fn);
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_MISSING_START_EXPORT, "_start export not found");
  }

  // Store instance reference (keep it alive)
  wasi->wasm_instance = JS_DupValue(ctx, instance);

  // Extract WAMR instance from WebAssembly.Instance
  wasi->wamr_instance = jsrt_webassembly_get_instance(ctx, instance);
  if (!wasi->wamr_instance) {
    JS_FreeValue(ctx, wasi->wasm_instance);
    wasi->wasm_instance = JS_UNDEFINED;
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INTERNAL,
                                 "Failed to extract WAMR instance from WebAssembly.Instance");
  }

  if (!wasi->memory_validated) {
    wasm_memory_inst_t memory = wasm_runtime_get_default_memory(wasi->wamr_instance);
    if (!memory) {
      JS_FreeValue(ctx, start_fn);
      return jsrt_wasi_missing_memory(ctx, wasi);
    }
    wasi->memory_validated = true;
  }

  // Create WAMR execution environment
  // Stack size: 64KB (typical for WASI applications)
  wasi->exec_env = wasm_runtime_create_exec_env(wasi->wamr_instance, 65536);
  if (!wasi->exec_env) {
    JS_FreeValue(ctx, wasi->wasm_instance);
    wasi->wasm_instance = JS_UNDEFINED;
    wasi->wamr_instance = NULL;
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

  // Validate instance is a WebAssembly.Instance
  if (!JS_IsObject(instance)) {
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_ARGUMENT, "Expected WebAssembly.Instance");
  }

  // Get exports object
  JSValue exports = JS_GetPropertyStr(ctx, instance, "exports");
  if (JS_IsException(exports)) {
    return JS_EXCEPTION;
  }

  if (!JS_IsObject(exports)) {
    JS_FreeValue(ctx, exports);
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INVALID_ARGUMENT, "Instance has no exports");
  }

  // Get _initialize function
  JSValue init_fn = JS_GetPropertyStr(ctx, exports, "_initialize");
  JS_FreeValue(ctx, exports);

  if (JS_IsException(init_fn)) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, init_fn)) {
    JS_FreeValue(ctx, init_fn);
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_MISSING_START_EXPORT, "_initialize export not found");
  }

  // Store instance reference (keep it alive)
  wasi->wasm_instance = JS_DupValue(ctx, instance);

  // Extract WAMR instance from WebAssembly.Instance
  wasi->wamr_instance = jsrt_webassembly_get_instance(ctx, instance);
  if (!wasi->wamr_instance) {
    JS_FreeValue(ctx, wasi->wasm_instance);
    wasi->wasm_instance = JS_UNDEFINED;
    return jsrt_wasi_throw_error(ctx, JSRT_WASI_ERROR_INTERNAL,
                                 "Failed to extract WAMR instance from WebAssembly.Instance");
  }

  if (!wasi->memory_validated) {
    wasm_memory_inst_t memory = wasm_runtime_get_default_memory(wasi->wamr_instance);
    if (!memory) {
      JS_FreeValue(ctx, init_fn);
      return jsrt_wasi_missing_memory(ctx, wasi);
    }
    wasi->memory_validated = true;
  }

  // Create WAMR execution environment
  // Stack size: 64KB (typical for WASI applications)
  wasi->exec_env = wasm_runtime_create_exec_env(wasi->wamr_instance, 65536);
  if (!wasi->exec_env) {
    JS_FreeValue(ctx, wasi->wasm_instance);
    wasi->wasm_instance = JS_UNDEFINED;
    wasi->wamr_instance = NULL;
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
