#include "webassembly.h"
#include "../util/debug.h"
#include "../wasm/runtime.h"

#include <quickjs.h>
#include <stdlib.h>
#include <string.h>
#include <wasm_c_api.h>
#include <wasm_export.h>

// WebAssembly class IDs
static JSClassID js_webassembly_module_class_id;
static JSClassID js_webassembly_instance_class_id;
static JSClassID js_webassembly_memory_class_id;
static JSClassID js_webassembly_table_class_id;
static JSClassID js_webassembly_global_class_id;
static JSClassID js_webassembly_tag_class_id;
static JSClassID js_webassembly_exported_function_class_id;

// Forward declare import resolver type
typedef struct jsrt_wasm_import_resolver jsrt_wasm_import_resolver_t;

// Forward declarations for internal module/instance structure
typedef struct {
  wasm_module_t module;
  uint8_t* wasm_bytes;
  size_t wasm_size;
} jsrt_wasm_module_data_t;

typedef struct {
  wasm_module_inst_t instance;
  jsrt_wasm_module_data_t* module_data;
  JSValue exports_object;                        // Cached exports object
  jsrt_wasm_import_resolver_t* import_resolver;  // Import resolver (keeps imports alive)
} jsrt_wasm_instance_data_t;

// Data structure for exported WASM function wrapper
typedef struct {
  wasm_module_inst_t instance;
  wasm_function_inst_t func;
  char* name;            // Function name (for debugging)
  JSValue instance_obj;  // Keep Instance alive while function exists
  JSContext* ctx;        // Context for freeing instance_obj in finalizer
} jsrt_wasm_export_func_data_t;

// Data structure for WebAssembly.Memory
typedef struct {
  wasm_memory_t* memory;  // WASM C API memory object
  JSContext* ctx;         // Context for buffer management
  JSValue buffer;         // Current ArrayBuffer (detached on grow)
} jsrt_wasm_memory_data_t;

// Function import wrapper for JS function imports
typedef struct {
  char* module_name;    // Module namespace (e.g., "env")
  char* field_name;     // Field name (e.g., "log")
  JSValue js_function;  // JS function reference (keep alive)
  JSContext* ctx;       // Context for calls
} jsrt_wasm_function_import_t;

// Import resolver context - stores parsed imports
struct jsrt_wasm_import_resolver {
  JSContext* ctx;
  wasm_module_t module;

  // Parsed function imports
  uint32_t function_import_count;
  jsrt_wasm_function_import_t* function_imports;  // Array

  // Native symbols registered with WAMR (must not be freed while module is alive)
  NativeSymbol* native_symbols;
  uint32_t native_symbol_count;
  char* module_name_for_natives;  // Module name used for registration

  // Keep JS import object alive
  JSValue import_object_ref;
};

// WebAssembly Error constructors (stored for creating error instances)
static JSValue webassembly_compile_error_ctor;
static JSValue webassembly_link_error_ctor;
static JSValue webassembly_runtime_error_ctor;

// Forward declarations
static JSValue js_webassembly_instance_exports_getter(JSContext* ctx, JSValueConst this_val, int argc,
                                                      JSValueConst* argv);

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

// Helper function to get ArrayBuffer bytes with detachment check
static uint8_t* get_arraybuffer_bytes_safe(JSContext* ctx, JSValueConst val, size_t* size) {
  // Try to get ArrayBuffer directly
  uint8_t* bytes = JS_GetArrayBuffer(ctx, size, val);
  if (bytes) {
    return bytes;
  }

  // Try to get TypedArray buffer
  JSValue buffer = JS_GetTypedArrayBuffer(ctx, val, NULL, NULL, NULL);
  if (JS_IsException(buffer)) {
    return NULL;  // Not an ArrayBuffer or TypedArray
  }

  bytes = JS_GetArrayBuffer(ctx, size, buffer);
  JS_FreeValue(ctx, buffer);
  return bytes;
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
  uint8_t* bytes = get_arraybuffer_bytes_safe(ctx, argv[0], &size);

  if (!bytes) {
    return JS_ThrowTypeError(ctx, "First argument must be a non-detached ArrayBuffer or TypedArray");
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
  uint8_t* bytes = get_arraybuffer_bytes_safe(ctx, argv[0], &size);

  if (!bytes) {
    return JS_ThrowTypeError(ctx, "First argument must be a non-detached ArrayBuffer or TypedArray");
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

// Exported WASM function wrapper - callable from JavaScript
static JSValue js_wasm_exported_function_call(JSContext* ctx, JSValueConst func_obj, JSValueConst this_val, int argc,
                                              JSValueConst* argv, int flags) {
  // Get wrapper data
  jsrt_wasm_export_func_data_t* func_data = JS_GetOpaque(func_obj, js_webassembly_exported_function_class_id);
  if (!func_data) {
    return JS_ThrowTypeError(ctx, "not an exported WebAssembly function");
  }

  // Get function signature info
  uint32_t param_count = wasm_func_get_param_count(func_data->func, func_data->instance);
  uint32_t result_count = wasm_func_get_result_count(func_data->func, func_data->instance);

  JSRT_Debug("Calling WASM function '%s': params=%u, results=%u", func_data->name ? func_data->name : "<unknown>",
             param_count, result_count);

  // Check argument count
  if ((uint32_t)argc < param_count) {
    return JS_ThrowTypeError(ctx, "insufficient arguments for WebAssembly function");
  }

  // Allocate argument array for WAMR (uses uint32 cells)
  // The array needs to be large enough for both arguments and return values
  // Even with 0 params, we need space for potential return values
  uint32_t total_cells = (param_count > result_count) ? param_count : result_count;
  if (total_cells == 0) {
    total_cells = 1;  // Minimum 1 cell
  }

  // Sanity check: prevent integer overflow and excessive allocations
  if (total_cells > 1024) {
    return JS_ThrowRangeError(ctx, "WebAssembly function has too many parameters or return values");
  }

  uint32_t* wasm_argv = js_malloc(ctx, sizeof(uint32_t) * total_cells);
  if (!wasm_argv) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize to zero
  memset(wasm_argv, 0, sizeof(uint32_t) * total_cells);

  // Convert JS arguments to WASM i32 values
  for (uint32_t i = 0; i < param_count; i++) {
    int32_t val;
    if (JS_ToInt32(ctx, &val, argv[i])) {
      js_free(ctx, wasm_argv);
      return JS_EXCEPTION;
    }
    wasm_argv[i] = (uint32_t)val;
    JSRT_Debug("  arg[%u] = %d (0x%x)", i, val, wasm_argv[i]);
  }

  // Create execution environment
  wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(func_data->instance, 16384);  // 16KB stack
  if (!exec_env) {
    js_free(ctx, wasm_argv);
    return throw_webassembly_runtime_error(ctx, "failed to create execution environment");
  }

  // Call the WASM function
  // Note: wasm_runtime_call_wasm expects argc (argument count) as 3rd parameter
  bool call_result = wasm_runtime_call_wasm(exec_env, func_data->func, param_count, wasm_argv);

  // Check for exceptions/traps
  if (!call_result) {
    const char* exception = wasm_runtime_get_exception(func_data->instance);
    wasm_runtime_destroy_exec_env(exec_env);
    js_free(ctx, wasm_argv);
    return throw_webassembly_runtime_error(ctx, exception ? exception : "WASM function call failed");
  }

  // Destroy execution environment
  wasm_runtime_destroy_exec_env(exec_env);

  // Convert result back to JavaScript
  JSValue result = JS_UNDEFINED;
  if (result_count > 0) {
    // For now, only support single i32 return value
    int32_t ret_val = (int32_t)wasm_argv[0];
    JSRT_Debug("  result = %d (0x%x)", ret_val, wasm_argv[0]);
    result = JS_NewInt32(ctx, ret_val);
  }

  js_free(ctx, wasm_argv);
  return result;
}

// Finalizer for exported function wrapper
static void js_wasm_exported_function_finalizer(JSRuntime* rt, JSValue val) {
  jsrt_wasm_export_func_data_t* data = JS_GetOpaque(val, js_webassembly_exported_function_class_id);
  if (data) {
    // Release the Instance reference (keeps Instance alive while function exists)
    if (!JS_IsUndefined(data->instance_obj) && data->ctx) {
      JS_FreeValue(data->ctx, data->instance_obj);
    }
    if (data->name) {
      js_free_rt(rt, data->name);
    }
    js_free_rt(rt, data);
  }
}

// Import resolver functions

// Create import resolver
static jsrt_wasm_import_resolver_t* jsrt_wasm_import_resolver_create(JSContext* ctx, wasm_module_t module,
                                                                     JSValue import_obj) {
  jsrt_wasm_import_resolver_t* resolver = js_malloc(ctx, sizeof(jsrt_wasm_import_resolver_t));
  if (!resolver) {
    return NULL;
  }

  memset(resolver, 0, sizeof(jsrt_wasm_import_resolver_t));
  resolver->ctx = ctx;
  resolver->module = module;
  resolver->import_object_ref = JS_DupValue(ctx, import_obj);

  JSRT_Debug("Created import resolver");
  return resolver;
}

// Destroy import resolver (context version - for use during normal execution)
static void jsrt_wasm_import_resolver_destroy(jsrt_wasm_import_resolver_t* resolver) {
  if (!resolver) {
    return;
  }

  JSContext* ctx = resolver->ctx;

  // Unregister native symbols from WAMR
  if (resolver->native_symbols && resolver->module_name_for_natives) {
    JSRT_Debug("Unregistering native symbols from WAMR");
    wasm_runtime_unregister_natives(resolver->module_name_for_natives, resolver->native_symbols);
    js_free(ctx, resolver->native_symbols);
    js_free(ctx, resolver->module_name_for_natives);
  }

  // Free function imports
  for (uint32_t i = 0; i < resolver->function_import_count; i++) {
    if (resolver->function_imports[i].module_name) {
      js_free(ctx, resolver->function_imports[i].module_name);
    }
    if (resolver->function_imports[i].field_name) {
      js_free(ctx, resolver->function_imports[i].field_name);
    }
    if (!JS_IsUndefined(resolver->function_imports[i].js_function)) {
      JS_FreeValue(ctx, resolver->function_imports[i].js_function);
    }
  }
  if (resolver->function_imports) {
    js_free(ctx, resolver->function_imports);
  }

  // Free import object reference
  if (!JS_IsUndefined(resolver->import_object_ref)) {
    JS_FreeValue(ctx, resolver->import_object_ref);
  }

  js_free(ctx, resolver);
  JSRT_Debug("Destroyed import resolver");
}

// Destroy import resolver (runtime version - for use in finalizers)
static void jsrt_wasm_import_resolver_destroy_rt(JSRuntime* rt, jsrt_wasm_import_resolver_t* resolver) {
  if (!resolver) {
    return;
  }

  // Unregister native symbols from WAMR
  if (resolver->native_symbols && resolver->module_name_for_natives) {
    JSRT_Debug("Unregistering native symbols from WAMR (finalizer)");
    wasm_runtime_unregister_natives(resolver->module_name_for_natives, resolver->native_symbols);
    js_free_rt(rt, resolver->native_symbols);
    js_free_rt(rt, resolver->module_name_for_natives);
  }

  // Free function imports
  for (uint32_t i = 0; i < resolver->function_import_count; i++) {
    if (resolver->function_imports[i].module_name) {
      js_free_rt(rt, resolver->function_imports[i].module_name);
    }
    if (resolver->function_imports[i].field_name) {
      js_free_rt(rt, resolver->function_imports[i].field_name);
    }
    if (!JS_IsUndefined(resolver->function_imports[i].js_function)) {
      JS_FreeValueRT(rt, resolver->function_imports[i].js_function);
    }
  }
  if (resolver->function_imports) {
    js_free_rt(rt, resolver->function_imports);
  }

  // Free import object reference
  if (!JS_IsUndefined(resolver->import_object_ref)) {
    JS_FreeValueRT(rt, resolver->import_object_ref);
  }

  js_free_rt(rt, resolver);
  JSRT_Debug("Destroyed import resolver (finalizer)");
}

// Parse function import
static int parse_function_import(jsrt_wasm_import_resolver_t* resolver, wasm_import_t* import_info, JSValue js_func) {
  JSContext* ctx = resolver->ctx;

  // Validate it's a function
  if (!JS_IsFunction(ctx, js_func)) {
    JSRT_Debug("Import '%s.%s' is not a function", import_info->module_name, import_info->name);
    return -1;
  }

  // Allocate function import array if needed (initial capacity: 16)
  if (!resolver->function_imports) {
    resolver->function_imports = js_malloc(ctx, sizeof(jsrt_wasm_function_import_t) * 16);
    if (!resolver->function_imports) {
      JSRT_Debug("Failed to allocate function imports array");
      return -1;
    }
    memset(resolver->function_imports, 0, sizeof(jsrt_wasm_function_import_t) * 16);
  }

  // Check capacity (simple fixed size for now)
  if (resolver->function_import_count >= 16) {
    JSRT_Debug("Too many function imports (max 16)");
    return -1;
  }

  uint32_t idx = resolver->function_import_count++;
  jsrt_wasm_function_import_t* func_import = &resolver->function_imports[idx];

  // Copy module name
  size_t module_len = strlen(import_info->module_name);
  func_import->module_name = js_malloc(ctx, module_len + 1);
  if (!func_import->module_name) {
    JSRT_Debug("Failed to allocate module name");
    resolver->function_import_count--;  // Roll back
    return -1;
  }
  memcpy(func_import->module_name, import_info->module_name, module_len + 1);

  // Copy field name
  size_t field_len = strlen(import_info->name);
  func_import->field_name = js_malloc(ctx, field_len + 1);
  if (!func_import->field_name) {
    JSRT_Debug("Failed to allocate field name");
    js_free(ctx, func_import->module_name);
    func_import->module_name = NULL;
    resolver->function_import_count--;  // Roll back
    return -1;
  }
  memcpy(func_import->field_name, import_info->name, field_len + 1);

  // Keep JS function reference alive
  func_import->js_function = JS_DupValue(ctx, js_func);
  func_import->ctx = ctx;

  JSRT_Debug("Registered function import '%s.%s'", func_import->module_name, func_import->field_name);

  return 0;
}

// Parse import object
static int parse_import_object(jsrt_wasm_import_resolver_t* resolver, JSValue import_obj) {
  JSContext* ctx = resolver->ctx;
  wasm_module_t module = resolver->module;

  // Check if import_obj is an object
  if (!JS_IsObject(import_obj)) {
    JSRT_Debug("Import object is not an object");
    return -1;
  }

  // Get all imports from module
  int32_t import_count = wasm_runtime_get_import_count(module);
  if (import_count < 0) {
    JSRT_Debug("Failed to get import count");
    return -1;
  }

  JSRT_Debug("Module requires %d imports", import_count);

  // If module has no imports, success
  if (import_count == 0) {
    return 0;
  }

  // Iterate through required imports
  for (int32_t i = 0; i < import_count; i++) {
    wasm_import_t import_info;
    wasm_runtime_get_import_type(module, i, &import_info);

    if (!import_info.module_name || !import_info.name) {
      JSRT_Debug("Import %d has NULL name", i);
      return -1;
    }

    JSRT_Debug("Import %d: module='%s', name='%s', kind=%d", i, import_info.module_name, import_info.name,
               import_info.kind);

    // Get module namespace from import object
    JSValue module_ns = JS_GetPropertyStr(ctx, import_obj, import_info.module_name);
    if (JS_IsException(module_ns)) {
      JSRT_Debug("Failed to get module namespace '%s'", import_info.module_name);
      return -1;
    }

    if (JS_IsUndefined(module_ns) || JS_IsNull(module_ns)) {
      JS_FreeValue(ctx, module_ns);
      JSRT_Debug("Missing module namespace '%s'", import_info.module_name);
      return -1;
    }

    // Get field from namespace
    JSValue field_value = JS_GetPropertyStr(ctx, module_ns, import_info.name);
    JS_FreeValue(ctx, module_ns);

    if (JS_IsException(field_value)) {
      JSRT_Debug("Failed to get field '%s' in module '%s'", import_info.name, import_info.module_name);
      return -1;
    }

    if (JS_IsUndefined(field_value) || JS_IsNull(field_value)) {
      JS_FreeValue(ctx, field_value);
      JSRT_Debug("Missing field '%s' in module '%s'", import_info.name, import_info.module_name);
      return -1;
    }

    // Process by kind
    int result = 0;
    switch (import_info.kind) {
      case WASM_IMPORT_EXPORT_KIND_FUNC:
        result = parse_function_import(resolver, &import_info, field_value);
        break;

      case WASM_IMPORT_EXPORT_KIND_MEMORY:
        // Memory imports not yet supported (Task 2.4-2.6 blocked)
        JSRT_Debug("Memory imports not yet supported");
        result = -1;
        break;

      case WASM_IMPORT_EXPORT_KIND_TABLE:
        // Table imports - Phase 4
        JSRT_Debug("Table imports not yet supported");
        result = -1;
        break;

      case WASM_IMPORT_EXPORT_KIND_GLOBAL:
        // Global imports - Phase 4
        JSRT_Debug("Global imports not yet supported");
        result = -1;
        break;

      default:
        JSRT_Debug("Unknown import kind %d", import_info.kind);
        result = -1;
        break;
    }

    JS_FreeValue(ctx, field_value);

    if (result < 0) {
      return -1;
    }
  }

  JSRT_Debug("Successfully parsed import object: %u function imports", resolver->function_import_count);
  return 0;
}

// Native function trampoline for function imports (Phase 3.2A - i32 only)
//
// This is the bridge between WASM and JavaScript:
// WASM calls this native function -> we convert args -> call JS function -> convert result -> return to WASM
//
// For now, we support only i32 parameters and return values (Phase 3.2A)
// Full type support (i64, f32, f64, BigInt) will be added in Phase 3.2B

static void jsrt_wasm_import_func_trampoline(wasm_exec_env_t exec_env, uint32_t* args, uint32_t argc) {
  // Get the attachment (our jsrt_wasm_function_import_t*)
  jsrt_wasm_function_import_t* func_import =
      (jsrt_wasm_function_import_t*)wasm_runtime_get_function_attachment(exec_env);

  if (!func_import) {
    JSRT_Debug("ERROR: No function import attachment");
    wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env), "Internal error: missing function attachment");
    return;
  }

  JSContext* ctx = func_import->ctx;
  JSValue js_func = func_import->js_function;

  JSRT_Debug("Calling JS import '%s.%s' with %u args", func_import->module_name, func_import->field_name, argc);

  // Convert WASM arguments (i32 only) to JS values
  JSValue* js_args = NULL;
  if (argc > 0) {
    js_args = js_malloc(ctx, sizeof(JSValue) * argc);
    if (!js_args) {
      wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env), "Out of memory");
      return;
    }

    for (uint32_t i = 0; i < argc; i++) {
      // Phase 3.2A: All parameters are i32
      int32_t val = (int32_t)args[i];
      js_args[i] = JS_NewInt32(ctx, val);
      JSRT_Debug("  arg[%u] = %d (0x%x)", i, val, args[i]);
    }
  }

  // Call the JavaScript function
  JSValue result = JS_Call(ctx, js_func, JS_UNDEFINED, argc, js_args);

  // Free JS argument values
  if (js_args) {
    for (uint32_t i = 0; i < argc; i++) {
      JS_FreeValue(ctx, js_args[i]);
    }
    js_free(ctx, js_args);
  }

  // Check for JS exceptions
  if (JS_IsException(result)) {
    // Propagate JS exception as WASM trap
    JSValue exception = JS_GetException(ctx);
    const char* error_msg = JS_ToCString(ctx, exception);
    JSRT_Debug("JS exception in import: %s", error_msg ? error_msg : "(unknown)");

    // Set WASM exception
    char exception_buf[256];
    snprintf(exception_buf, sizeof(exception_buf), "JavaScript exception: %s", error_msg ? error_msg : "(unknown)");
    wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env), exception_buf);

    if (error_msg)
      JS_FreeCString(ctx, error_msg);
    JS_FreeValue(ctx, exception);
    return;
  }

  // Convert return value from JS to WASM (i32 only in Phase 3.2A)
  // WAMR expects return value in args[0]
  if (!JS_IsUndefined(result) && !JS_IsNull(result)) {
    int32_t ret_val;
    if (JS_ToInt32(ctx, &ret_val, result)) {
      // Conversion failed
      JS_FreeValue(ctx, result);
      wasm_runtime_set_exception(wasm_runtime_get_module_inst(exec_env), "Failed to convert JS return value to i32");
      return;
    }
    args[0] = (uint32_t)ret_val;
    JSRT_Debug("  result = %d (0x%x)", ret_val, args[0]);
  } else {
    // undefined/null -> 0
    args[0] = 0;
    JSRT_Debug("  result = undefined/null -> 0");
  }

  JS_FreeValue(ctx, result);
}

// Register function imports with WAMR
// This creates the NativeSymbol array and calls wasm_runtime_register_natives()
static int jsrt_wasm_register_function_imports(jsrt_wasm_import_resolver_t* resolver) {
  if (!resolver || resolver->function_import_count == 0) {
    return 0;  // No function imports to register
  }

  JSContext* ctx = resolver->ctx;
  uint32_t count = resolver->function_import_count;

  JSRT_Debug("Registering %u function imports with WAMR", count);

  // For each unique module name, we need to register a group of native symbols
  // For simplicity in Phase 3.2A, we'll assume all imports are in one module (typically "env")
  // TODO Phase 3.2B: Handle multiple module namespaces

  // Find the first module name (assume all imports use same module for now)
  const char* module_name = resolver->function_imports[0].module_name;

  // Create NativeSymbol array
  NativeSymbol* native_symbols = js_malloc(ctx, sizeof(NativeSymbol) * count);
  if (!native_symbols) {
    JSRT_Debug("Failed to allocate NativeSymbol array");
    return -1;
  }

  memset(native_symbols, 0, sizeof(NativeSymbol) * count);

  // Populate NativeSymbol array
  for (uint32_t i = 0; i < count; i++) {
    jsrt_wasm_function_import_t* func_import = &resolver->function_imports[i];

    // Verify module name matches (for Phase 3.2A single-module assumption)
    if (strcmp(func_import->module_name, module_name) != 0) {
      JSRT_Debug("ERROR: Multiple module namespaces not yet supported in Phase 3.2A");
      js_free(ctx, native_symbols);
      return -1;
    }

    native_symbols[i].symbol = func_import->field_name;  // Function name
    native_symbols[i].func_ptr = (void*)jsrt_wasm_import_func_trampoline;
    native_symbols[i].signature = "(ii)i";       // Phase 3.2A: Default to (i32, i32) -> i32 signature
                                                 // TODO Phase 3.2B: Get actual signature from module
    native_symbols[i].attachment = func_import;  // Pass our data to the trampoline

    JSRT_Debug("  [%u] symbol='%s' signature='%s'", i, native_symbols[i].symbol, native_symbols[i].signature);
  }

  // Register with WAMR
  // Note: WAMR does NOT copy this array - it must remain valid for the module's lifetime
  if (!wasm_runtime_register_natives(module_name, native_symbols, count)) {
    JSRT_Debug("wasm_runtime_register_natives failed");
    js_free(ctx, native_symbols);
    return -1;
  }

  // Store native_symbols in resolver so we can properly cleanup later
  resolver->native_symbols = native_symbols;
  resolver->native_symbol_count = count;

  // Copy module name for unregistration later
  size_t name_len = strlen(module_name);
  resolver->module_name_for_natives = js_malloc(ctx, name_len + 1);
  if (!resolver->module_name_for_natives) {
    JSRT_Debug("Failed to allocate module name copy");
    // Still continue - worst case we leak the registration
  } else {
    memcpy(resolver->module_name_for_natives, module_name, name_len + 1);
  }

  JSRT_Debug("Successfully registered %u function imports", count);
  return 0;
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

  // Process import object if provided (Phase 3.2A)
  jsrt_wasm_import_resolver_t* resolver = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    // Create import resolver
    resolver = jsrt_wasm_import_resolver_create(ctx, module_data->module, argv[1]);
    if (!resolver) {
      return JS_ThrowOutOfMemory(ctx);
    }

    // Parse import object
    if (parse_import_object(resolver, argv[1]) < 0) {
      jsrt_wasm_import_resolver_destroy(resolver);
      return throw_webassembly_link_error(ctx, "Failed to parse import object");
    }

    // Register function imports with WAMR (before instantiation)
    if (jsrt_wasm_register_function_imports(resolver) < 0) {
      jsrt_wasm_import_resolver_destroy(resolver);
      return throw_webassembly_link_error(ctx, "Failed to register function imports");
    }
  }

  // Instantiate module
  char error_buf[256];
  uint32_t stack_size = 16384;  // 16KB stack
  uint32_t heap_size = 65536;   // 64KB heap

  wasm_module_inst_t instance =
      wasm_runtime_instantiate(module_data->module, stack_size, heap_size, error_buf, sizeof(error_buf));

  if (!instance) {
    if (resolver) {
      jsrt_wasm_import_resolver_destroy(resolver);
    }
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
  instance_data->exports_object = JS_UNDEFINED;  // Will be created below
  instance_data->import_resolver = resolver;     // Keep resolver alive (NULL if no imports)

  JS_SetOpaque(instance_obj, instance_data);

  // Create exports object and set it as a property
  JSValue exports = js_webassembly_instance_exports_getter(ctx, instance_obj, 0, NULL);
  if (JS_IsException(exports)) {
    JS_FreeValue(ctx, instance_obj);
    return exports;
  }

  // Set exports as a readonly, non-configurable property on the instance (per WebAssembly spec)
  JS_DefinePropertyValueStr(ctx, instance_obj, "exports", exports, JS_PROP_ENUMERABLE);

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

// Instance.exports property getter
static JSValue js_webassembly_instance_exports_getter(JSContext* ctx, JSValueConst this_val, int argc,
                                                      JSValueConst* argv) {
  jsrt_wasm_instance_data_t* instance_data = JS_GetOpaque(this_val, js_webassembly_instance_class_id);
  if (!instance_data) {
    return JS_ThrowTypeError(ctx, "not an Instance");
  }

  // Return cached exports object if already created
  if (!JS_IsUndefined(instance_data->exports_object)) {
    return JS_DupValue(ctx, instance_data->exports_object);
  }

  // Create exports object
  JSValue exports = JS_NewObject(ctx);
  if (JS_IsException(exports)) {
    return exports;
  }

  // Get module from instance
  wasm_module_t module = instance_data->module_data->module;

  // Enumerate exports
  int32_t export_count = wasm_runtime_get_export_count(module);
  if (export_count < 0) {
    JS_FreeValue(ctx, exports);
    return JS_ThrowInternalError(ctx, "Failed to get export count");
  }

  JSRT_Debug("Instance has %d exports", export_count);

  // Add each export to the exports object
  for (int32_t i = 0; i < export_count; i++) {
    wasm_export_t export_info;
    wasm_runtime_get_export_type(module, i, &export_info);

    if (!export_info.name) {
      continue;  // Skip invalid exports
    }

    JSRT_Debug("Processing export '%s' kind=%d", export_info.name, export_info.kind);

    JSValue export_value = JS_UNDEFINED;

    if (export_info.kind == WASM_IMPORT_EXPORT_KIND_FUNC) {
      // Look up the function in the instance
      wasm_function_inst_t func = wasm_runtime_lookup_function(instance_data->instance, export_info.name);
      if (!func) {
        JSRT_Debug("Warning: function '%s' not found in instance", export_info.name);
        continue;
      }

      // Create wrapper function data
      jsrt_wasm_export_func_data_t* func_data = js_malloc(ctx, sizeof(jsrt_wasm_export_func_data_t));
      if (!func_data) {
        JS_FreeValue(ctx, exports);
        return JS_ThrowOutOfMemory(ctx);
      }

      func_data->instance = instance_data->instance;
      func_data->func = func;
      func_data->ctx = ctx;
      // Keep Instance alive while exported function exists (prevents use-after-free)
      func_data->instance_obj = JS_DupValue(ctx, this_val);
      // Copy function name for debugging
      size_t name_len = strlen(export_info.name);
      func_data->name = js_malloc(ctx, name_len + 1);
      if (func_data->name) {
        memcpy(func_data->name, export_info.name, name_len + 1);
      }

      // Create JS function object with call handler
      // The class already has .call set, so we just need to create the object
      export_value = JS_NewObjectClass(ctx, js_webassembly_exported_function_class_id);
      if (JS_IsException(export_value)) {
        if (func_data->name)
          js_free(ctx, func_data->name);
        js_free(ctx, func_data);
        JS_FreeValue(ctx, exports);
        return export_value;
      }

      JS_SetOpaque(export_value, func_data);
    } else {
      // Memory, Table, Global not yet implemented - skip for now
      JSRT_Debug("Skipping non-function export '%s'", export_info.name);
      continue;
    }

    // Add to exports object
    if (!JS_IsUndefined(export_value)) {
      JS_SetPropertyStr(ctx, exports, export_info.name, export_value);
    }
  }

  // Freeze the exports object (per WebAssembly spec)
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue freeze_func = JS_GetPropertyStr(ctx, global_obj, "Object");
  if (!JS_IsException(freeze_func)) {
    JSValue freeze_method = JS_GetPropertyStr(ctx, freeze_func, "freeze");
    if (!JS_IsException(freeze_method)) {
      JSValue args[] = {exports};
      JSValue frozen = JS_Call(ctx, freeze_method, freeze_func, 1, args);
      if (JS_IsException(frozen)) {
        // Failed to freeze - propagate error
        JS_FreeValue(ctx, freeze_method);
        JS_FreeValue(ctx, freeze_func);
        JS_FreeValue(ctx, global_obj);
        JS_FreeValue(ctx, exports);
        return frozen;
      }
      JS_FreeValue(ctx, frozen);
      JS_FreeValue(ctx, freeze_method);
    }
    JS_FreeValue(ctx, freeze_func);
  }
  JS_FreeValue(ctx, global_obj);

  // Cache the exports object
  instance_data->exports_object = JS_DupValue(ctx, exports);

  return exports;
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

// WebAssembly.Module.imports(module) static method
static JSValue js_webassembly_module_imports(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Validate argument count
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Module.imports requires 1 argument");
  }

  // Get module data - validate it's a WebAssembly.Module
  jsrt_wasm_module_data_t* module_data = JS_GetOpaque(argv[0], js_webassembly_module_class_id);
  if (!module_data) {
    return JS_ThrowTypeError(ctx, "Argument must be a WebAssembly.Module");
  }

  // Get import count
  int32_t import_count = wasm_runtime_get_import_count(module_data->module);
  if (import_count < 0) {
    return JS_ThrowInternalError(ctx, "Failed to get import count");
  }

  JSRT_Debug("Module has %d imports", import_count);

  // Create result array
  JSValue result = JS_NewArray(ctx);
  if (JS_IsException(result)) {
    return result;
  }

  // Iterate through imports
  for (int32_t i = 0; i < import_count; i++) {
    wasm_import_t import_info;

    // Get import type (void return, fills import_info)
    wasm_runtime_get_import_type(module_data->module, i, &import_info);

    // Check if names are valid
    if (!import_info.module_name || !import_info.name) {
      JS_FreeValue(ctx, result);
      return JS_ThrowInternalError(ctx, "Import name is NULL");
    }

    // Get kind string
    const char* kind_str = wasm_export_kind_to_string(import_info.kind);

    JSRT_Debug("Import %d: module='%s', name='%s', kind='%s'", i, import_info.module_name, import_info.name, kind_str);

    // Create descriptor object {module, name, kind}
    JSValue descriptor = JS_NewObject(ctx);
    if (JS_IsException(descriptor)) {
      JS_FreeValue(ctx, result);
      return descriptor;
    }

    // Set module property
    JSValue module = JS_NewString(ctx, import_info.module_name);
    if (JS_IsException(module)) {
      JS_FreeValue(ctx, descriptor);
      JS_FreeValue(ctx, result);
      return module;
    }
    JS_SetPropertyStr(ctx, descriptor, "module", module);

    // Set name property
    JSValue name = JS_NewString(ctx, import_info.name);
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

// Helper function to read LEB128 unsigned integer from WASM binary
// Returns number of bytes read, or 0 on error
static size_t read_leb128_u32(const uint8_t* bytes, size_t max_len, uint32_t* out_value) {
  uint32_t result = 0;
  uint32_t shift = 0;
  size_t count = 0;

  while (count < max_len && count < 5) {  // Max 5 bytes for uint32
    uint8_t byte = bytes[count++];
    result |= (uint32_t)(byte & 0x7F) << shift;

    if ((byte & 0x80) == 0) {
      *out_value = result;
      return count;
    }
    shift += 7;
  }

  return 0;  // Error: incomplete LEB128 or overflow
}

// Helper function to read a string (length-prefixed) from WASM binary
// Returns number of bytes read (including length prefix), or 0 on error
// The string pointer will point into the bytes array (not copied)
static size_t read_wasm_string(const uint8_t* bytes, size_t max_len, const char** out_str, uint32_t* out_len) {
  uint32_t str_len;
  size_t len_bytes = read_leb128_u32(bytes, max_len, &str_len);
  if (len_bytes == 0 || len_bytes + str_len > max_len) {
    return 0;
  }

  *out_str = (const char*)(bytes + len_bytes);
  *out_len = str_len;
  return len_bytes + str_len;
}

// WebAssembly.Module.customSections(module, sectionName) static method
// Returns an array of ArrayBuffer, one for each custom section with the given name
static JSValue js_webassembly_module_custom_sections(JSContext* ctx, JSValueConst this_val, int argc,
                                                     JSValueConst* argv) {
  // Validate argument count
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Module.customSections requires 2 arguments");
  }

  // Get module data - validate it's a WebAssembly.Module
  jsrt_wasm_module_data_t* module_data = JS_GetOpaque(argv[0], js_webassembly_module_class_id);
  if (!module_data) {
    return JS_ThrowTypeError(ctx, "First argument must be a WebAssembly.Module");
  }

  // Get section name string
  const char* section_name = JS_ToCString(ctx, argv[1]);
  if (!section_name) {
    return JS_ThrowTypeError(ctx, "Second argument must be a string");
  }

  size_t section_name_len = strlen(section_name);
  JSRT_Debug("Searching for custom sections named '%s'", section_name);

  // Create result array
  JSValue result = JS_NewArray(ctx);
  if (JS_IsException(result)) {
    JS_FreeCString(ctx, section_name);
    return result;
  }

  // Parse WASM binary format to find custom sections
  // WASM format: magic (4 bytes) + version (4 bytes) + sections
  const uint8_t* bytes = module_data->wasm_bytes;
  size_t size = module_data->wasm_size;

  // Check magic number and version (8 bytes total)
  if (size < 8) {
    JS_FreeCString(ctx, section_name);
    return result;  // Return empty array for invalid module
  }

  size_t pos = 8;  // Skip magic + version
  uint32_t result_count = 0;

  // Parse sections
  while (pos < size) {
    // Read section ID (1 byte)
    if (pos >= size)
      break;
    uint8_t section_id = bytes[pos++];

    // Read section size (LEB128)
    uint32_t section_size;
    size_t size_bytes = read_leb128_u32(bytes + pos, size - pos, &section_size);
    if (size_bytes == 0) {
      JSRT_Debug("Failed to read section size at offset %zu", pos);
      break;
    }
    pos += size_bytes;

    // Check if we have enough bytes for the section payload
    if (pos + section_size > size) {
      JSRT_Debug("Section size exceeds module size at offset %zu", pos);
      break;
    }

    // Section ID 0 is custom section
    if (section_id == 0) {
      // Custom section format: name (length + string) + payload
      const char* name_ptr;
      uint32_t name_len;
      size_t name_bytes = read_wasm_string(bytes + pos, section_size, &name_ptr, &name_len);

      if (name_bytes == 0) {
        JSRT_Debug("Failed to read custom section name at offset %zu", pos);
        pos += section_size;  // Skip this section
        continue;
      }

      // Check if name matches
      if (name_len == section_name_len && memcmp(name_ptr, section_name, name_len) == 0) {
        // Found matching custom section!
        // Content starts after the name
        const uint8_t* content_start = bytes + pos + name_bytes;
        size_t content_size = section_size - name_bytes;

        JSRT_Debug("Found custom section '%s' at offset %zu, size %zu", section_name, pos, content_size);

        // Create ArrayBuffer with section content (excluding name)
        JSValue array_buffer = JS_NewArrayBufferCopy(ctx, content_start, content_size);
        if (JS_IsException(array_buffer)) {
          JS_FreeValue(ctx, result);
          JS_FreeCString(ctx, section_name);
          return array_buffer;
        }

        // Add to result array
        JS_SetPropertyUint32(ctx, result, result_count++, array_buffer);
      }
    }

    // Move to next section
    pos += section_size;
  }

  JS_FreeCString(ctx, section_name);
  JSRT_Debug("Found %u custom sections named '%s'", result_count, section_name);

  return result;
}

// WebAssembly.Memory(descriptor) constructor
static JSValue js_webassembly_memory_constructor(JSContext* ctx, JSValueConst new_target, int argc,
                                                 JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "WebAssembly.Memory constructor requires 1 argument");
  }

  // Get descriptor object
  JSValueConst descriptor = argv[0];
  if (!JS_IsObject(descriptor)) {
    return JS_ThrowTypeError(ctx, "First argument must be an object");
  }

  // Get initial property (required)
  JSValue initial_val = JS_GetPropertyStr(ctx, descriptor, "initial");
  if (JS_IsException(initial_val)) {
    return initial_val;
  }

  uint32_t initial;
  if (JS_ToUint32(ctx, &initial, initial_val)) {
    JS_FreeValue(ctx, initial_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, initial_val);

  // Get maximum property (optional)
  JSValue maximum_val = JS_GetPropertyStr(ctx, descriptor, "maximum");
  uint32_t maximum = 0xffffffff;  // wasm_limits_max_default
  bool has_maximum = false;

  if (!JS_IsException(maximum_val) && !JS_IsUndefined(maximum_val)) {
    has_maximum = true;
    if (JS_ToUint32(ctx, &maximum, maximum_val)) {
      JS_FreeValue(ctx, maximum_val);
      return JS_EXCEPTION;
    }
  }
  JS_FreeValue(ctx, maximum_val);

  // Validate: initial must not exceed maximum
  if (has_maximum && initial > maximum) {
    return JS_ThrowRangeError(ctx, "Initial memory size exceeds maximum");
  }

  JSRT_Debug("Creating Memory: initial=%u, maximum=%u (has_max=%d)", initial, maximum, has_maximum);

  // Create memory type using C API
  wasm_limits_t limits;
  limits.min = initial;
  limits.max = has_maximum ? maximum : wasm_limits_max_default;

  wasm_memorytype_t* memtype = wasm_memorytype_new(&limits);
  if (!memtype) {
    return JS_ThrowInternalError(ctx, "Failed to create memory type");
  }

  // Create memory using C API
  wasm_store_t* store = jsrt_wasm_get_store();
  if (!store) {
    wasm_memorytype_delete(memtype);
    return JS_ThrowInternalError(ctx, "WASM store not initialized");
  }

  wasm_memory_t* memory = wasm_memory_new(store, memtype);
  wasm_memorytype_delete(memtype);  // No longer needed

  if (!memory) {
    return JS_ThrowRangeError(ctx, "Failed to create WebAssembly memory");
  }

  // Create JS Memory object
  JSValue memory_obj = JS_NewObjectClass(ctx, js_webassembly_memory_class_id);
  if (JS_IsException(memory_obj)) {
    wasm_memory_delete(memory);
    return memory_obj;
  }

  // Create memory data
  jsrt_wasm_memory_data_t* memory_data = js_malloc(ctx, sizeof(jsrt_wasm_memory_data_t));
  if (!memory_data) {
    wasm_memory_delete(memory);
    JS_FreeValue(ctx, memory_obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  memory_data->memory = memory;
  memory_data->ctx = ctx;
  memory_data->buffer = JS_UNDEFINED;  // Will be created on first access

  JS_SetOpaque(memory_obj, memory_data);

  JSRT_Debug("WebAssembly.Memory created successfully");
  return memory_obj;
}

// Memory.prototype.buffer getter
static JSValue js_webassembly_memory_buffer_getter(JSContext* ctx, JSValueConst this_val, int argc,
                                                   JSValueConst* argv) {
  jsrt_wasm_memory_data_t* memory_data = JS_GetOpaque(this_val, js_webassembly_memory_class_id);
  if (!memory_data) {
    return JS_ThrowTypeError(ctx, "not a Memory object");
  }

  // Return cached buffer if it exists and hasn't been detached
  if (!JS_IsUndefined(memory_data->buffer)) {
    return JS_DupValue(ctx, memory_data->buffer);
  }

  // Create new ArrayBuffer from memory data
  byte_t* data = wasm_memory_data(memory_data->memory);
  size_t size = wasm_memory_data_size(memory_data->memory);

  JSRT_Debug("Creating ArrayBuffer view: data=%p, size=%zu", (void*)data, size);

  // Create ArrayBuffer that references the WASM memory (not a copy)
  JSValue buffer = JS_NewArrayBuffer(ctx, data, size, NULL, NULL, false);
  if (JS_IsException(buffer)) {
    return buffer;
  }

  // Cache the buffer
  memory_data->buffer = JS_DupValue(ctx, buffer);

  return buffer;
}

// Memory.prototype.grow(delta) method
static JSValue js_webassembly_memory_grow(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Memory.grow requires 1 argument");
  }

  jsrt_wasm_memory_data_t* memory_data = JS_GetOpaque(this_val, js_webassembly_memory_class_id);
  if (!memory_data) {
    return JS_ThrowTypeError(ctx, "not a Memory object");
  }

  // Get delta parameter
  uint32_t delta;
  if (JS_ToUint32(ctx, &delta, argv[0])) {
    return JS_EXCEPTION;
  }

  // Get current size (in pages)
  wasm_memory_pages_t old_size = wasm_memory_size(memory_data->memory);

  JSRT_Debug("Memory.grow: old_size=%u pages, delta=%u pages", old_size, delta);

  // Detach old buffer (per WebAssembly spec)
  if (!JS_IsUndefined(memory_data->buffer)) {
    JSRT_Debug("Detaching old ArrayBuffer");
    JS_DetachArrayBuffer(ctx, memory_data->buffer);
    JS_FreeValue(ctx, memory_data->buffer);
    memory_data->buffer = JS_UNDEFINED;
  }

  // Grow the memory
  if (!wasm_memory_grow(memory_data->memory, delta)) {
    return JS_ThrowRangeError(ctx, "Failed to grow memory (maximum exceeded or out of memory)");
  }

  JSRT_Debug("Memory grown successfully: new_size=%u pages", wasm_memory_size(memory_data->memory));

  // Return old size
  return JS_NewUint32(ctx, old_size);
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
    // Free cached exports object if it exists
    if (!JS_IsUndefined(data->exports_object)) {
      JS_FreeValueRT(rt, data->exports_object);
    }
    // Cleanup import resolver (unregisters natives, frees JS references)
    if (data->import_resolver) {
      jsrt_wasm_import_resolver_destroy_rt(rt, data->import_resolver);
    }
    // Cleanup instance
    if (data->instance) {
      wasm_runtime_deinstantiate(data->instance);
    }
    js_free_rt(rt, data);
  }
}

// Memory finalizer
static void js_webassembly_memory_finalizer(JSRuntime* rt, JSValue val) {
  jsrt_wasm_memory_data_t* data = JS_GetOpaque(val, js_webassembly_memory_class_id);
  if (data) {
    // Free cached buffer if it exists
    if (!JS_IsUndefined(data->buffer)) {
      JS_FreeValueRT(rt, data->buffer);
    }
    // Delete WASM memory
    if (data->memory) {
      wasm_memory_delete(data->memory);
    }
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

static JSClassDef js_webassembly_exported_function_class = {
    "WebAssembly.ExportedFunction",
    .finalizer = js_wasm_exported_function_finalizer,
    .call = js_wasm_exported_function_call,
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

  JS_NewClassID(&js_webassembly_exported_function_class_id);
  JS_NewClass(JS_GetRuntime(rt->ctx), js_webassembly_exported_function_class_id,
              &js_webassembly_exported_function_class);

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

  // Add Module.imports static method
  JS_SetPropertyStr(rt->ctx, module_ctor, "imports",
                    JS_NewCFunction(rt->ctx, js_webassembly_module_imports, "imports", 1));

  // Add Module.customSections static method
  JS_SetPropertyStr(rt->ctx, module_ctor, "customSections",
                    JS_NewCFunction(rt->ctx, js_webassembly_module_custom_sections, "customSections", 2));

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

  // Create Memory constructor and prototype
  JSValue memory_ctor =
      JS_NewCFunction2(rt->ctx, js_webassembly_memory_constructor, "Memory", 1, JS_CFUNC_constructor, 0);
  JSValue memory_proto = JS_NewObject(rt->ctx);
  JS_SetPropertyStr(rt->ctx, memory_proto, "constructor", JS_DupValue(rt->ctx, memory_ctor));
  JS_DefinePropertyValue(rt->ctx, memory_proto, JS_ValueToAtom(rt->ctx, toStringTag_symbol),
                         JS_NewString(rt->ctx, "WebAssembly.Memory"), JS_PROP_CONFIGURABLE);

  // Add Memory.prototype.buffer getter
  JS_DefinePropertyGetSet(rt->ctx, memory_proto, JS_NewAtom(rt->ctx, "buffer"),
                          JS_NewCFunction(rt->ctx, js_webassembly_memory_buffer_getter, "get buffer", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);

  // Add Memory.prototype.grow() method
  JS_SetPropertyStr(rt->ctx, memory_proto, "grow", JS_NewCFunction(rt->ctx, js_webassembly_memory_grow, "grow", 1));

  JS_SetConstructor(rt->ctx, memory_ctor, memory_proto);
  JS_SetClassProto(rt->ctx, js_webassembly_memory_class_id, memory_proto);
  JS_SetPropertyStr(rt->ctx, webassembly, "Memory", memory_ctor);

  JS_FreeValue(rt->ctx, toStringTag_symbol);

  // Set WebAssembly global
  JS_SetPropertyStr(rt->ctx, rt->global, "WebAssembly", webassembly);

  JSRT_Debug("WebAssembly global object setup completed");
}