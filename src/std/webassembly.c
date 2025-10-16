#include "webassembly.h"
#include "../util/debug.h"
#include "../wasm/runtime.h"

#include <quickjs.h>
#include <stdlib.h>
#include <string.h>
#include <wasm_export.h>

// WebAssembly class IDs
static JSClassID js_webassembly_module_class_id;
static JSClassID js_webassembly_instance_class_id;
static JSClassID js_webassembly_memory_class_id;
static JSClassID js_webassembly_table_class_id;
static JSClassID js_webassembly_global_class_id;
static JSClassID js_webassembly_tag_class_id;

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

// WebAssembly Error constructors (stored for creating error instances)
static JSValue webassembly_compile_error_ctor;
static JSValue webassembly_link_error_ctor;
static JSValue webassembly_runtime_error_ctor;

// Helper function to create WebAssembly error constructors
static JSValue create_webassembly_error_constructor(JSContext* ctx, const char* name, JSValue error_proto) {
  JSValue ctor = JS_NewCFunction2(ctx, NULL, name, 1, JS_CFUNC_constructor, 0);

  // Create prototype that inherits from Error.prototype
  JSValue proto = JS_NewObject(ctx);
  JS_SetPrototype(ctx, proto, error_proto);

  // Set name property on prototype
  JS_DefinePropertyValueStr(ctx, proto, "name", JS_NewString(ctx, name), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  // Set message property on prototype (empty by default)
  JS_DefinePropertyValueStr(ctx, proto, "message", JS_NewString(ctx, ""), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  // Set prototype property on constructor
  JS_DefinePropertyValueStr(ctx, ctor, "prototype", proto, JS_PROP_WRITABLE);

  // Set constructor property on prototype
  JS_DefinePropertyValueStr(ctx, proto, "constructor", JS_DupValue(ctx, ctor), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  return ctor;
}

// Helper functions to throw WebAssembly errors
static JSValue throw_webassembly_compile_error(JSContext* ctx, const char* message) {
  JSValue error = JS_NewError(ctx);
  JSValue proto = JS_GetPropertyStr(ctx, webassembly_compile_error_ctor, "prototype");
  JS_SetPrototype(ctx, error, proto);
  JS_FreeValue(ctx, proto);
  JS_DefinePropertyValueStr(ctx, error, "name", JS_NewString(ctx, "CompileError"),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, message), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  return JS_Throw(ctx, error);
}

static JSValue throw_webassembly_link_error(JSContext* ctx, const char* message) {
  JSValue error = JS_NewError(ctx);
  JSValue proto = JS_GetPropertyStr(ctx, webassembly_link_error_ctor, "prototype");
  JS_SetPrototype(ctx, error, proto);
  JS_FreeValue(ctx, proto);
  JS_DefinePropertyValueStr(ctx, error, "name", JS_NewString(ctx, "LinkError"),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, message), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  return JS_Throw(ctx, error);
}

static JSValue throw_webassembly_runtime_error(JSContext* ctx, const char* message) {
  JSValue error = JS_NewError(ctx);
  JSValue proto = JS_GetPropertyStr(ctx, webassembly_runtime_error_ctor, "prototype");
  JS_SetPrototype(ctx, error, proto);
  JS_FreeValue(ctx, proto);
  JS_DefinePropertyValueStr(ctx, error, "name", JS_NewString(ctx, "RuntimeError"),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, message), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  return JS_Throw(ctx, error);
}

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

  // Copy bytes first - WAMR stores pointers into this buffer
  // The JS ArrayBuffer backing store might be moved/freed, so we need our own copy
  uint8_t* bytes_copy = js_malloc(ctx, size);
  if (!bytes_copy) {
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(bytes_copy, bytes, size);

  // Load WASM module using WAMR from our stable copy
  char error_buf[256];
  wasm_module_t module = wasm_runtime_load(bytes_copy, (uint32_t)size, error_buf, sizeof(error_buf));

  if (!module) {
    js_free(ctx, bytes_copy);
    return throw_webassembly_compile_error(ctx, error_buf);
  }

  // Create JS Module object with proper class
  JSValue module_obj = JS_NewObjectClass(ctx, js_webassembly_module_class_id);
  if (JS_IsException(module_obj)) {
    wasm_runtime_unload(module);
    js_free(ctx, bytes_copy);
    return module_obj;
  }

  // Store module data using js_malloc
  jsrt_wasm_module_data_t* module_data = js_malloc(ctx, sizeof(jsrt_wasm_module_data_t));
  if (!module_data) {
    wasm_runtime_unload(module);
    js_free(ctx, bytes_copy);
    JS_FreeValue(ctx, module_obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Store the copied bytes and module
  module_data->wasm_bytes = bytes_copy;
  module_data->wasm_size = size;
  module_data->module = module;

  // Set internal module data
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

  // Get module data using proper class ID
  jsrt_wasm_module_data_t* module_data = JS_GetOpaque(argv[0], js_webassembly_module_class_id);
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
    return throw_webassembly_link_error(ctx, error_buf);
  }

  // Create JS Instance object with proper class
  JSValue instance_obj = JS_NewObjectClass(ctx, js_webassembly_instance_class_id);
  if (JS_IsException(instance_obj)) {
    wasm_runtime_deinstantiate(instance);
    return instance_obj;
  }

  // Store instance data using js_malloc
  jsrt_wasm_instance_data_t* instance_data = js_malloc(ctx, sizeof(jsrt_wasm_instance_data_t));
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

// Helper to convert WAMR kind enum to JavaScript kind string
static const char* wasm_export_kind_to_string(wasm_import_export_kind_t kind) {
  switch (kind) {
    case WASM_IMPORT_EXPORT_KIND_FUNC:
      return "function";
    case WASM_IMPORT_EXPORT_KIND_TABLE:
      return "table";
    case WASM_IMPORT_EXPORT_KIND_MEMORY:
      return "memory";
    case WASM_IMPORT_EXPORT_KIND_GLOBAL:
      return "global";
    default:
      return "unknown";
  }
}

// WebAssembly.Module.exports(module) static method
static JSValue js_webassembly_module_exports(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Validate argument count
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Module.exports requires 1 argument");
  }

  // Get module data - validate it's a WebAssembly.Module
  jsrt_wasm_module_data_t* module_data = JS_GetOpaque(argv[0], js_webassembly_module_class_id);
  if (!module_data) {
    return JS_ThrowTypeError(ctx, "Argument must be a WebAssembly.Module");
  }

  // Get export count
  int32_t export_count = wasm_runtime_get_export_count(module_data->module);
  if (export_count < 0) {
    return JS_ThrowInternalError(ctx, "Failed to get export count");
  }

  JSRT_Debug("Module has %d exports", export_count);

  // Create result array
  JSValue result = JS_NewArray(ctx);
  if (JS_IsException(result)) {
    return result;
  }

  // Iterate through exports
  for (int32_t i = 0; i < export_count; i++) {
    wasm_export_t export_info;

    // Get export type (void return, fills export_info)
    wasm_runtime_get_export_type(module_data->module, i, &export_info);

    // Check if name is valid
    if (!export_info.name) {
      JS_FreeValue(ctx, result);
      return JS_ThrowInternalError(ctx, "Export name is NULL");
    }

    // Get kind string
    const char* kind_str = wasm_export_kind_to_string(export_info.kind);

    JSRT_Debug("Export %d: name='%s', kind='%s'", i, export_info.name, kind_str);

    // Create descriptor object {name, kind}
    JSValue descriptor = JS_NewObject(ctx);
    if (JS_IsException(descriptor)) {
      JS_FreeValue(ctx, result);
      return descriptor;
    }

    // Set name property - JS_NewString will copy the string
    JSValue name = JS_NewString(ctx, export_info.name);
    if (JS_IsException(name)) {
      JS_FreeValue(ctx, descriptor);
      JS_FreeValue(ctx, result);
      return name;
    }
    JS_SetPropertyStr(ctx, descriptor, "name", name);

    // Set kind property
    JSValue kind = JS_NewString(ctx, kind_str);
    if (JS_IsException(kind)) {
      JS_FreeValue(ctx, descriptor);
      JS_FreeValue(ctx, result);
      return kind;
    }
    JS_SetPropertyStr(ctx, descriptor, "kind", kind);

    // Add descriptor to result array
    JS_SetPropertyUint32(ctx, result, i, descriptor);
  }

  return result;
}

// WebAssembly class finalizers
static void js_webassembly_module_finalizer(JSRuntime* rt, JSValue val) {
  jsrt_wasm_module_data_t* data = JS_GetOpaque(val, js_webassembly_module_class_id);
  if (data) {
    // Cleanup will be implemented in Task 1.3
    if (data->wasm_bytes) {
      js_free_rt(rt, data->wasm_bytes);
    }
    if (data->module) {
      wasm_runtime_unload(data->module);
    }
    js_free_rt(rt, data);
  }
}

static void js_webassembly_instance_finalizer(JSRuntime* rt, JSValue val) {
  jsrt_wasm_instance_data_t* data = JS_GetOpaque(val, js_webassembly_instance_class_id);
  if (data) {
    // Cleanup will be implemented in Task 1.3
    if (data->instance) {
      wasm_runtime_deinstantiate(data->instance);
    }
    js_free_rt(rt, data);
  }
}

// Placeholder finalizers for not-yet-implemented classes
static void js_webassembly_memory_finalizer(JSRuntime* rt, JSValue val) {
  void* data = JS_GetOpaque(val, js_webassembly_memory_class_id);
  if (data) {
    js_free_rt(rt, data);
  }
}

static void js_webassembly_table_finalizer(JSRuntime* rt, JSValue val) {
  void* data = JS_GetOpaque(val, js_webassembly_table_class_id);
  if (data) {
    js_free_rt(rt, data);
  }
}

static void js_webassembly_global_finalizer(JSRuntime* rt, JSValue val) {
  void* data = JS_GetOpaque(val, js_webassembly_global_class_id);
  if (data) {
    js_free_rt(rt, data);
  }
}

static void js_webassembly_tag_finalizer(JSRuntime* rt, JSValue val) {
  void* data = JS_GetOpaque(val, js_webassembly_tag_class_id);
  if (data) {
    js_free_rt(rt, data);
  }
}

// WebAssembly class definitions
static JSClassDef js_webassembly_module_class = {
    "WebAssembly.Module",
    .finalizer = js_webassembly_module_finalizer,
};

static JSClassDef js_webassembly_instance_class = {
    "WebAssembly.Instance",
    .finalizer = js_webassembly_instance_finalizer,
};

static JSClassDef js_webassembly_memory_class = {
    "WebAssembly.Memory",
    .finalizer = js_webassembly_memory_finalizer,
};

static JSClassDef js_webassembly_table_class = {
    "WebAssembly.Table",
    .finalizer = js_webassembly_table_finalizer,
};

static JSClassDef js_webassembly_global_class = {
    "WebAssembly.Global",
    .finalizer = js_webassembly_global_finalizer,
};

static JSClassDef js_webassembly_tag_class = {
    "WebAssembly.Tag",
    .finalizer = js_webassembly_tag_finalizer,
};

void JSRT_RuntimeSetupStdWebAssembly(JSRT_Runtime* rt) {
  JSRT_Debug("Setting up WebAssembly global object");

  // Initialize WASM runtime if not already done
  if (jsrt_wasm_init() != 0) {
    JSRT_Debug("Failed to initialize WASM runtime");
    return;
  }

  // Register all WebAssembly classes
  JS_NewClassID(&js_webassembly_module_class_id);
  JS_NewClass(JS_GetRuntime(rt->ctx), js_webassembly_module_class_id, &js_webassembly_module_class);

  JS_NewClassID(&js_webassembly_instance_class_id);
  JS_NewClass(JS_GetRuntime(rt->ctx), js_webassembly_instance_class_id, &js_webassembly_instance_class);

  JS_NewClassID(&js_webassembly_memory_class_id);
  JS_NewClass(JS_GetRuntime(rt->ctx), js_webassembly_memory_class_id, &js_webassembly_memory_class);

  JS_NewClassID(&js_webassembly_table_class_id);
  JS_NewClass(JS_GetRuntime(rt->ctx), js_webassembly_table_class_id, &js_webassembly_table_class);

  JS_NewClassID(&js_webassembly_global_class_id);
  JS_NewClass(JS_GetRuntime(rt->ctx), js_webassembly_global_class_id, &js_webassembly_global_class);

  JS_NewClassID(&js_webassembly_tag_class_id);
  JS_NewClass(JS_GetRuntime(rt->ctx), js_webassembly_tag_class_id, &js_webassembly_tag_class);

  // Get Error.prototype for error constructors
  JSValue error_proto = JS_GetPropertyStr(rt->ctx, rt->global, "Error");
  JSValue error_prototype = JS_GetPropertyStr(rt->ctx, error_proto, "prototype");
  JS_FreeValue(rt->ctx, error_proto);

  // Create WebAssembly error constructors
  webassembly_compile_error_ctor = create_webassembly_error_constructor(rt->ctx, "CompileError", error_prototype);
  webassembly_link_error_ctor = create_webassembly_error_constructor(rt->ctx, "LinkError", error_prototype);
  webassembly_runtime_error_ctor = create_webassembly_error_constructor(rt->ctx, "RuntimeError", error_prototype);

  JS_FreeValue(rt->ctx, error_prototype);

  // Create WebAssembly global object
  JSValue webassembly = JS_NewObject(rt->ctx);

  // Add error constructors
  JS_SetPropertyStr(rt->ctx, webassembly, "CompileError", JS_DupValue(rt->ctx, webassembly_compile_error_ctor));
  JS_SetPropertyStr(rt->ctx, webassembly, "LinkError", JS_DupValue(rt->ctx, webassembly_link_error_ctor));
  JS_SetPropertyStr(rt->ctx, webassembly, "RuntimeError", JS_DupValue(rt->ctx, webassembly_runtime_error_ctor));

  // Add validate function
  JS_SetPropertyStr(rt->ctx, webassembly, "validate", JS_NewCFunction(rt->ctx, js_webassembly_validate, "validate", 1));

  // Get Symbol.toStringTag for proper class identification
  JSValue global = JS_GetGlobalObject(rt->ctx);
  JSValue symbol_obj = JS_GetPropertyStr(rt->ctx, global, "Symbol");
  JSValue toStringTag_symbol = JS_GetPropertyStr(rt->ctx, symbol_obj, "toStringTag");
  JS_FreeValue(rt->ctx, symbol_obj);
  JS_FreeValue(rt->ctx, global);

  // Create Module constructor and prototype
  JSValue module_ctor =
      JS_NewCFunction2(rt->ctx, js_webassembly_module_constructor, "Module", 1, JS_CFUNC_constructor, 0);
  JSValue module_proto = JS_NewObject(rt->ctx);
  JS_SetPropertyStr(rt->ctx, module_proto, "constructor", JS_DupValue(rt->ctx, module_ctor));
  JS_DefinePropertyValue(rt->ctx, module_proto, JS_ValueToAtom(rt->ctx, toStringTag_symbol),
                         JS_NewString(rt->ctx, "WebAssembly.Module"), JS_PROP_CONFIGURABLE);
  JS_SetConstructor(rt->ctx, module_ctor, module_proto);
  JS_SetClassProto(rt->ctx, js_webassembly_module_class_id, module_proto);

  // Add Module.exports static method
  JS_SetPropertyStr(rt->ctx, module_ctor, "exports",
                    JS_NewCFunction(rt->ctx, js_webassembly_module_exports, "exports", 1));

  JS_SetPropertyStr(rt->ctx, webassembly, "Module", module_ctor);

  // Create Instance constructor and prototype
  JSValue instance_ctor =
      JS_NewCFunction2(rt->ctx, js_webassembly_instance_constructor, "Instance", 2, JS_CFUNC_constructor, 0);
  JSValue instance_proto = JS_NewObject(rt->ctx);
  JS_SetPropertyStr(rt->ctx, instance_proto, "constructor", JS_DupValue(rt->ctx, instance_ctor));
  JS_DefinePropertyValue(rt->ctx, instance_proto, JS_ValueToAtom(rt->ctx, toStringTag_symbol),
                         JS_NewString(rt->ctx, "WebAssembly.Instance"), JS_PROP_CONFIGURABLE);
  JS_SetConstructor(rt->ctx, instance_ctor, instance_proto);
  JS_SetClassProto(rt->ctx, js_webassembly_instance_class_id, instance_proto);
  JS_SetPropertyStr(rt->ctx, webassembly, "Instance", instance_ctor);

  JS_FreeValue(rt->ctx, toStringTag_symbol);

  // Set WebAssembly global
  JS_SetPropertyStr(rt->ctx, rt->global, "WebAssembly", webassembly);

  JSRT_Debug("WebAssembly global object setup completed");
}