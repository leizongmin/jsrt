/**
 * WASI Lifecycle Methods
 *
 * Implementation of start() and initialize() methods for WASI instances.
 */

#include "../../util/debug.h"
#include "wasi.h"

#include <string.h>

/**
 * Start WASI instance (call _start export)
 */
JSValue jsrt_wasi_start(JSContext* ctx, jsrt_wasi_t* wasi, JSValue instance) {
  if (!wasi) {
    return JS_ThrowTypeError(ctx, "Invalid WASI instance");
  }

  // Check if already started or initialized
  if (wasi->started) {
    return JS_ThrowTypeError(ctx, "WASI instance already started");
  }
  if (wasi->initialized) {
    return JS_ThrowTypeError(ctx, "WASI instance already initialized");
  }

  // Validate instance is a WebAssembly.Instance
  if (!JS_IsObject(instance)) {
    return JS_ThrowTypeError(ctx, "Argument must be a WebAssembly.Instance");
  }

  // Get exports object
  JSValue exports = JS_GetPropertyStr(ctx, instance, "exports");
  if (JS_IsException(exports)) {
    return JS_EXCEPTION;
  }

  if (!JS_IsObject(exports)) {
    JS_FreeValue(ctx, exports);
    return JS_ThrowTypeError(ctx, "Instance has no exports");
  }

  // Get _start function
  JSValue start_fn = JS_GetPropertyStr(ctx, exports, "_start");
  JS_FreeValue(ctx, exports);

  if (JS_IsException(start_fn)) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, start_fn)) {
    JS_FreeValue(ctx, start_fn);
    return JS_ThrowTypeError(ctx, "WebAssembly.Instance missing _start export");
  }

  // Store instance reference
  wasi->wasm_instance = JS_DupValue(ctx, instance);

  // Call _start()
  JSRT_Debug("Calling WASI _start()");
  JSValue result = JS_Call(ctx, start_fn, JS_UNDEFINED, 0, NULL);
  JS_FreeValue(ctx, start_fn);

  if (JS_IsException(result)) {
    // _start() threw an exception
    return JS_EXCEPTION;
  }

  // Mark as started
  wasi->started = true;

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
    // Call process.exit(exit_code)
    // For now, we just return undefined
    // TODO: Implement actual process.exit() call in Phase 5
    JSRT_Debug("returnOnExit=false: would call process.exit(%d)", exit_code);
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
    return JS_ThrowTypeError(ctx, "Invalid WASI instance");
  }

  // Check if already started or initialized
  if (wasi->started) {
    return JS_ThrowTypeError(ctx, "WASI instance already started");
  }
  if (wasi->initialized) {
    return JS_ThrowTypeError(ctx, "WASI instance already initialized");
  }

  // Validate instance is a WebAssembly.Instance
  if (!JS_IsObject(instance)) {
    return JS_ThrowTypeError(ctx, "Argument must be a WebAssembly.Instance");
  }

  // Get exports object
  JSValue exports = JS_GetPropertyStr(ctx, instance, "exports");
  if (JS_IsException(exports)) {
    return JS_EXCEPTION;
  }

  if (!JS_IsObject(exports)) {
    JS_FreeValue(ctx, exports);
    return JS_ThrowTypeError(ctx, "Instance has no exports");
  }

  // Get _initialize function
  JSValue init_fn = JS_GetPropertyStr(ctx, exports, "_initialize");
  JS_FreeValue(ctx, exports);

  if (JS_IsException(init_fn)) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, init_fn)) {
    JS_FreeValue(ctx, init_fn);
    return JS_ThrowTypeError(ctx, "WebAssembly.Instance missing _initialize export");
  }

  // Store instance reference
  wasi->wasm_instance = JS_DupValue(ctx, instance);

  // Call _initialize()
  JSRT_Debug("Calling WASI _initialize()");
  JSValue result = JS_Call(ctx, init_fn, JS_UNDEFINED, 0, NULL);
  JS_FreeValue(ctx, init_fn);

  if (JS_IsException(result)) {
    // _initialize() threw an exception
    return JS_EXCEPTION;
  }

  JS_FreeValue(ctx, result);

  // Mark as initialized
  wasi->initialized = true;

  JSRT_Debug("WASI _initialize() completed");

  return JS_UNDEFINED;
}
