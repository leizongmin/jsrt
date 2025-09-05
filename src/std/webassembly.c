#include "webassembly.h"
#include "../util/debug.h"
#include "../wasm/runtime.h"

#include <quickjs.h>
#include <stdlib.h>
#include <string.h>
#include <wasm_export.h>

// Forward declarations for internal module/instance structure
typedef struct {
  wasm_module_t module;
  uint8_t* wasm_bytes;
  size_t wasm_size;
} jsrt_wasm_module_data_t;

typedef struct {
  wasm_module_inst_t instance;
  jsrt_wasm_module_data_t* module_data;
} jsrt_wasm_instance_data_t;

// WebAssembly.validate(bytes) -> boolean
static JSValue js_webassembly_validate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "WebAssembly.validate requires 1 argument");
  }

  // Get the bytes argument - can be ArrayBuffer, Uint8Array, etc.
  size_t size;
  uint8_t* bytes = JS_GetArrayBuffer(ctx, &size, argv[0]);

  if (!bytes) {
    // Try to get TypedArray buffer
    JSValue buffer = JS_GetTypedArrayBuffer(ctx, argv[0], NULL, NULL, NULL);
    if (JS_IsException(buffer)) {
      JS_FreeValue(ctx, buffer);
      return JS_ThrowTypeError(ctx, "First argument must be ArrayBuffer or TypedArray");
    }
    bytes = JS_GetArrayBuffer(ctx, &size, buffer);
    JS_FreeValue(ctx, buffer);

    if (!bytes) {
      return JS_ThrowTypeError(ctx, "Unable to get bytes from first argument");
    }
  }

  // Validate WASM bytes using WAMR
  char error_buf[256];
  wasm_module_t module = wasm_runtime_load(bytes, (uint32_t)size, error_buf, sizeof(error_buf));

  if (module) {
    wasm_runtime_unload(module);
    return JS_TRUE;
  } else {
    JSRT_Debug("WASM validation failed: %s", error_buf);
    return JS_FALSE;
  }
}

// WebAssembly.Module(bytes) constructor
static JSValue js_webassembly_module_constructor(JSContext* ctx, JSValueConst new_target, int argc,
                                                 JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "WebAssembly.Module constructor requires 1 argument");
  }

  // Get the bytes argument
  size_t size;
  uint8_t* bytes = JS_GetArrayBuffer(ctx, &size, argv[0]);

  if (!bytes) {
    // Try to get TypedArray buffer
    JSValue buffer = JS_GetTypedArrayBuffer(ctx, argv[0], NULL, NULL, NULL);
    if (JS_IsException(buffer)) {
      JS_FreeValue(ctx, buffer);
      return JS_ThrowTypeError(ctx, "First argument must be ArrayBuffer or TypedArray");
    }
    bytes = JS_GetArrayBuffer(ctx, &size, buffer);
    JS_FreeValue(ctx, buffer);

    if (!bytes) {
      return JS_ThrowTypeError(ctx, "Unable to get bytes from first argument");
    }
  }

  // Load WASM module using WAMR
  char error_buf[256];
  wasm_module_t module = wasm_runtime_load(bytes, (uint32_t)size, error_buf, sizeof(error_buf));

  if (!module) {
    return JS_ThrowTypeError(ctx, "WebAssembly.Module: %s", error_buf);
  }

  // Create JS Module object
  JSValue module_obj = JS_NewObjectClass(ctx, 0);  // We'll define a proper class later
  if (JS_IsException(module_obj)) {
    wasm_runtime_unload(module);
    return module_obj;
  }

  // Store module data
  jsrt_wasm_module_data_t* module_data = malloc(sizeof(jsrt_wasm_module_data_t));
  if (!module_data) {
    wasm_runtime_unload(module);
    JS_FreeValue(ctx, module_obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Copy bytes for later use
  module_data->wasm_bytes = malloc(size);
  if (!module_data->wasm_bytes) {
    free(module_data);
    wasm_runtime_unload(module);
    JS_FreeValue(ctx, module_obj);
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(module_data->wasm_bytes, bytes, size);
  module_data->wasm_size = size;
  module_data->module = module;

  // Set internal module data (we'll use opaque pointer for now)
  JS_SetOpaque(module_obj, module_data);

  JSRT_Debug("WebAssembly.Module created successfully");
  return module_obj;
}

// WebAssembly.Instance(module, importObject) constructor
static JSValue js_webassembly_instance_constructor(JSContext* ctx, JSValueConst new_target, int argc,
                                                   JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "WebAssembly.Instance constructor requires 1 argument");
  }

  // Get module data
  jsrt_wasm_module_data_t* module_data = JS_GetOpaque(argv[0], 0);
  if (!module_data) {
    return JS_ThrowTypeError(ctx, "First argument must be a WebAssembly.Module");
  }

  // For now, ignore importObject (Phase 3 feature)
  // TODO: Process import object in Phase 3

  // Instantiate module
  char error_buf[256];
  uint32_t stack_size = 16384;  // 16KB stack
  uint32_t heap_size = 65536;   // 64KB heap

  wasm_module_inst_t instance =
      wasm_runtime_instantiate(module_data->module, stack_size, heap_size, error_buf, sizeof(error_buf));

  if (!instance) {
    return JS_ThrowTypeError(ctx, "WebAssembly.Instance: %s", error_buf);
  }

  // Create JS Instance object
  JSValue instance_obj = JS_NewObjectClass(ctx, 0);  // We'll define a proper class later
  if (JS_IsException(instance_obj)) {
    wasm_runtime_deinstantiate(instance);
    return instance_obj;
  }

  // Store instance data
  jsrt_wasm_instance_data_t* instance_data = malloc(sizeof(jsrt_wasm_instance_data_t));
  if (!instance_data) {
    wasm_runtime_deinstantiate(instance);
    JS_FreeValue(ctx, instance_obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  instance_data->instance = instance;
  instance_data->module_data = module_data;

  JS_SetOpaque(instance_obj, instance_data);

  JSRT_Debug("WebAssembly.Instance created successfully");
  return instance_obj;
}

void JSRT_RuntimeSetupStdWebAssembly(JSRT_Runtime* rt) {
  JSRT_Debug("Setting up WebAssembly global object");

  // Initialize WASM runtime if not already done
  if (jsrt_wasm_init() != 0) {
    JSRT_Debug("Failed to initialize WASM runtime");
    return;
  }

  // Create WebAssembly global object
  JSValue webassembly = JS_NewObject(rt->ctx);

  // Add validate function
  JS_SetPropertyStr(rt->ctx, webassembly, "validate", JS_NewCFunction(rt->ctx, js_webassembly_validate, "validate", 1));

  // Add Module constructor
  JS_SetPropertyStr(rt->ctx, webassembly, "Module",
                    JS_NewCFunction2(rt->ctx, js_webassembly_module_constructor, "Module", 1, JS_CFUNC_constructor, 0));

  // Add Instance constructor
  JS_SetPropertyStr(
      rt->ctx, webassembly, "Instance",
      JS_NewCFunction2(rt->ctx, js_webassembly_instance_constructor, "Instance", 2, JS_CFUNC_constructor, 0));

  // Set WebAssembly global
  JS_SetPropertyStr(rt->ctx, rt->global, "WebAssembly", webassembly);

  JSRT_Debug("WebAssembly global object setup completed");
}