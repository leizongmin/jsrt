#include "ffi.h"

// Platform-specific includes for dynamic loading
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <pthread.h>
#endif

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include libffi if available for robust calling conventions
#ifdef HAVE_LIBFFI
#include <ffi.h>
#define FFI_ENABLED 1
#else
#define FFI_ENABLED 0
#endif

#include "../util/debug.h"
#include "../util/jsutils.h"

// Cross-platform macros for dynamic loading
#ifdef _WIN32
#define JSRT_DLOPEN(name) LoadLibraryA(name)
#define JSRT_DLSYM(handle, name) ((void*)GetProcAddress(handle, name))
#define JSRT_DLCLOSE(handle) FreeLibrary(handle)
#define JSRT_DLERROR() GetLastError()
typedef HMODULE jsrt_dl_handle_t;
#else
#define JSRT_DLOPEN(name) dlopen(name, RTLD_LAZY)
#define JSRT_DLSYM(handle, name) dlsym(handle, name)
#define JSRT_DLCLOSE(handle) dlclose(handle)
#define JSRT_DLERROR() dlerror()
typedef void* jsrt_dl_handle_t;
#endif

// FFI data types - prefixed to avoid conflicts with libffi
typedef enum {
  JSRT_FFI_TYPE_VOID,
  JSRT_FFI_TYPE_INT,
  JSRT_FFI_TYPE_UINT,
  JSRT_FFI_TYPE_INT32,
  JSRT_FFI_TYPE_UINT32,
  JSRT_FFI_TYPE_INT64,
  JSRT_FFI_TYPE_UINT64,
  JSRT_FFI_TYPE_FLOAT,
  JSRT_FFI_TYPE_DOUBLE,
  JSRT_FFI_TYPE_POINTER,
  JSRT_FFI_TYPE_STRING,
  JSRT_FFI_TYPE_ARRAY,
  JSRT_FFI_TYPE_CALLBACK,
  JSRT_FFI_TYPE_STRUCT
} jsrt_ffi_type_t;

// Function signature structure with libffi support
typedef struct {
  jsrt_ffi_type_t return_type;
  int arg_count;
  jsrt_ffi_type_t* arg_types;
  void* func_ptr;
#ifdef HAVE_LIBFFI
  ffi_cif cif;                // libffi call interface
  ffi_type** ffi_arg_types;   // libffi argument types
  ffi_type* ffi_return_type;  // libffi return type
  bool cif_prepared;          // whether CIF has been prepared
#endif
} jsrt_ffi_function_t;

// Global storage for FFI function metadata
static JSValue ffi_functions_map;
static int next_function_id = 1;
static bool ffi_initialized = false;

// Function wrapper data structure
typedef struct {
  int function_id;
} ffi_function_wrapper_t;

// Library handle structure
typedef struct {
  jsrt_dl_handle_t handle;
  char* name;
  JSValue functions;  // JS object containing functions
} ffi_library_t;

// Callback structure for JavaScript functions called from C
typedef struct {
  JSContext* ctx;
  JSValue js_function;
  jsrt_ffi_type_t return_type;
  int arg_count;
  jsrt_ffi_type_t* arg_types;
  void* callback_ptr;  // Generated trampoline function pointer
} ffi_callback_t;

// Enhanced struct definition for complex nested types
typedef struct ffi_struct_def ffi_struct_def_t;

typedef enum {
  JSRT_FIELD_TYPE_PRIMITIVE,  // Basic types (int, float, etc.)
  JSRT_FIELD_TYPE_STRUCT,     // Nested struct
  JSRT_FIELD_TYPE_ARRAY,      // Array of primitives or structs
  JSRT_FIELD_TYPE_POINTER     // Pointer to any type
} jsrt_field_type_category_t;

// Field descriptor for complex types
typedef struct {
  char* name;
  jsrt_ffi_type_t base_type;            // Basic FFI type
  jsrt_field_type_category_t category;  // Field category
  size_t offset;                        // Offset in parent struct

  // For nested structs
  ffi_struct_def_t* nested_struct;  // Reference to nested struct definition

  // For arrays
  int array_length;                  // Array size (0 for dynamic)
  jsrt_ffi_type_t element_type;      // Element type for arrays
  ffi_struct_def_t* element_struct;  // Element struct for struct arrays

  // For pointers
  int pointer_levels;                // Number of pointer levels (*, **, etc.)
  jsrt_ffi_type_t pointed_type;      // Type being pointed to
  ffi_struct_def_t* pointed_struct;  // Struct being pointed to
} jsrt_field_descriptor_t;

// Enhanced struct definition for complex nested types
struct ffi_struct_def {
  char* name;
  int field_count;
  jsrt_field_descriptor_t* fields;  // Enhanced field descriptors
  size_t total_size;
  size_t alignment;  // Struct alignment requirement

  // Dependency tracking for complex types
  int dependency_count;
  ffi_struct_def_t** dependencies;  // Other structs this depends on
};

// Store function metadata for lookup during calls
static int store_function_metadata(JSContext* ctx, jsrt_ffi_function_t* func) {
  if (!ffi_initialized) {
    ffi_functions_map = JS_NewObject(ctx);
    ffi_initialized = true;
  }

  int function_id = next_function_id++;
  char id_str[32];
  snprintf(id_str, sizeof(id_str), "%d", function_id);

  // Create metadata object
  JSValue metadata = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, metadata, "return_type", JS_NewInt32(ctx, func->return_type));
  JS_SetPropertyStr(ctx, metadata, "arg_count", JS_NewInt32(ctx, func->arg_count));
  JS_SetPropertyStr(ctx, metadata, "func_ptr", JS_NewInt64(ctx, (intptr_t)func->func_ptr));

  JS_SetPropertyStr(ctx, ffi_functions_map, id_str, metadata);
  return function_id;
}

// Retrieve function metadata during calls
static bool get_function_metadata_by_id(JSContext* ctx, int function_id, jsrt_ffi_function_t* func) {
  if (!ffi_initialized) {
    return false;
  }

  char id_str[32];
  snprintf(id_str, sizeof(id_str), "%d", function_id);

  JSValue metadata = JS_GetPropertyStr(ctx, ffi_functions_map, id_str);
  if (JS_IsUndefined(metadata)) {
    return false;
  }

  JSValue return_type_val = JS_GetPropertyStr(ctx, metadata, "return_type");
  JSValue arg_count_val = JS_GetPropertyStr(ctx, metadata, "arg_count");
  JSValue func_ptr_val = JS_GetPropertyStr(ctx, metadata, "func_ptr");

  int32_t return_type, arg_count;
  int64_t stored_ptr;

  bool success = (JS_ToInt32(ctx, &return_type, return_type_val) == 0) &&
                 (JS_ToInt32(ctx, &arg_count, arg_count_val) == 0) && (JS_ToInt64(ctx, &stored_ptr, func_ptr_val) == 0);

  if (success) {
    func->return_type = (jsrt_ffi_type_t)return_type;
    func->arg_count = arg_count;
    func->arg_types = NULL;
    func->func_ptr = (void*)(intptr_t)stored_ptr;
  }

  JS_FreeValue(ctx, return_type_val);
  JS_FreeValue(ctx, arg_count_val);
  JS_FreeValue(ctx, func_ptr_val);
  JS_FreeValue(ctx, metadata);

  return success;
}

// Convert string to FFI type
static jsrt_ffi_type_t string_to_ffi_type(const char* type_str) {
  if (strcmp(type_str, "void") == 0)
    return JSRT_FFI_TYPE_VOID;
  if (strcmp(type_str, "int") == 0)
    return JSRT_FFI_TYPE_INT;
  if (strcmp(type_str, "uint") == 0)
    return JSRT_FFI_TYPE_UINT;
  if (strcmp(type_str, "int32") == 0)
    return JSRT_FFI_TYPE_INT32;
  if (strcmp(type_str, "uint32") == 0)
    return JSRT_FFI_TYPE_UINT32;
  if (strcmp(type_str, "int64") == 0)
    return JSRT_FFI_TYPE_INT64;
  if (strcmp(type_str, "uint64") == 0)
    return JSRT_FFI_TYPE_UINT64;
  if (strcmp(type_str, "float") == 0)
    return JSRT_FFI_TYPE_FLOAT;
  if (strcmp(type_str, "double") == 0)
    return JSRT_FFI_TYPE_DOUBLE;
  if (strcmp(type_str, "pointer") == 0)
    return JSRT_FFI_TYPE_POINTER;
  if (strcmp(type_str, "string") == 0)
    return JSRT_FFI_TYPE_STRING;
  if (strcmp(type_str, "array") == 0)
    return JSRT_FFI_TYPE_ARRAY;
  if (strcmp(type_str, "callback") == 0)
    return JSRT_FFI_TYPE_CALLBACK;
  if (strcmp(type_str, "struct") == 0)
    return JSRT_FFI_TYPE_STRUCT;
  return JSRT_FFI_TYPE_VOID;  // Default to void for unknown types
}

#ifdef HAVE_LIBFFI
// Convert our FFI types to libffi types
static ffi_type* jsrt_ffi_type_to_libffi(jsrt_ffi_type_t type) {
  switch (type) {
    case JSRT_FFI_TYPE_VOID:
      return &ffi_type_void;
    case JSRT_FFI_TYPE_INT:
      return &ffi_type_sint;
    case JSRT_FFI_TYPE_UINT:
      return &ffi_type_uint;
    case JSRT_FFI_TYPE_INT32:
      return &ffi_type_sint32;
    case JSRT_FFI_TYPE_UINT32:
      return &ffi_type_uint32;
    case JSRT_FFI_TYPE_INT64:
      return &ffi_type_sint64;
    case JSRT_FFI_TYPE_UINT64:
      return &ffi_type_uint64;
    case JSRT_FFI_TYPE_FLOAT:
      return &ffi_type_float;
    case JSRT_FFI_TYPE_DOUBLE:
      return &ffi_type_double;
    case JSRT_FFI_TYPE_POINTER:
    case JSRT_FFI_TYPE_STRING:
    case JSRT_FFI_TYPE_ARRAY:
    case JSRT_FFI_TYPE_CALLBACK:
    case JSRT_FFI_TYPE_STRUCT:
      return &ffi_type_pointer;  // All these are essentially pointers
    default:
      return &ffi_type_void;
  }
}

// Prepare libffi CIF (Call InterFace) for a function
static bool prepare_libffi_cif(jsrt_ffi_function_t* func) {
  if (func->cif_prepared) {
    return true;  // Already prepared
  }

  // Allocate libffi argument types array
  func->ffi_arg_types = malloc(func->arg_count * sizeof(ffi_type*));
  if (!func->ffi_arg_types && func->arg_count > 0) {
    return false;
  }

  // Convert argument types
  for (int i = 0; i < func->arg_count; i++) {
    func->ffi_arg_types[i] = jsrt_ffi_type_to_libffi(func->arg_types[i]);
  }

  // Convert return type
  func->ffi_return_type = jsrt_ffi_type_to_libffi(func->return_type);

  // Prepare the call interface
  ffi_status status =
      ffi_prep_cif(&func->cif, FFI_DEFAULT_ABI, func->arg_count, func->ffi_return_type, func->ffi_arg_types);

  if (status == FFI_OK) {
    func->cif_prepared = true;
    return true;
  }

  // Cleanup on failure
  free(func->ffi_arg_types);
  func->ffi_arg_types = NULL;
  return false;
}

// Cleanup libffi resources
static void cleanup_libffi_cif(jsrt_ffi_function_t* func) {
#ifdef HAVE_LIBFFI
  if (func->ffi_arg_types) {
    free(func->ffi_arg_types);
    func->ffi_arg_types = NULL;
  }
  func->cif_prepared = false;
#endif
}

#ifdef HAVE_LIBFFI
// Enhanced function calling using libffi for robust calling conventions
static JSValue call_function_with_libffi(JSContext* ctx, jsrt_ffi_function_t* func, uint64_t* args, void** array_args,
                                         int argc) {
  // Prepare the CIF if not already done
  if (!prepare_libffi_cif(func)) {
    return JS_ThrowTypeError(ctx, "Failed to prepare libffi call interface");
  }

  // Prepare argument values for libffi
  void** ffi_args = malloc(argc * sizeof(void*));
  if (!ffi_args && argc > 0) {
    return JS_ThrowTypeError(ctx, "Failed to allocate libffi arguments");
  }

  // Convert arguments to libffi format
  for (int i = 0; i < argc; i++) {
    ffi_args[i] = &args[i];  // Use address of the argument value
  }

  // Prepare result storage
  union {
    int32_t i32;
    int64_t i64;
    uint32_t u32;
    uint64_t u64;
    float f;
    double d;
    void* ptr;
  } result_storage;

  // Make the call using libffi
  ffi_call(&func->cif, func->func_ptr, &result_storage, ffi_args);

  free(ffi_args);

  // Convert result back to JavaScript value
  JSValue result = JS_UNDEFINED;
  switch (func->return_type) {
    case JSRT_FFI_TYPE_VOID:
      result = JS_UNDEFINED;
      break;
    case JSRT_FFI_TYPE_INT:
    case JSRT_FFI_TYPE_INT32:
      result = JS_NewInt32(ctx, result_storage.i32);
      break;
    case JSRT_FFI_TYPE_UINT:
    case JSRT_FFI_TYPE_UINT32:
      result = JS_NewUint32(ctx, result_storage.u32);
      break;
    case JSRT_FFI_TYPE_INT64:
      result = JS_NewInt64(ctx, result_storage.i64);
      break;
    case JSRT_FFI_TYPE_UINT64:
      result = JS_NewBigUint64(ctx, result_storage.u64);
      break;
    case JSRT_FFI_TYPE_FLOAT:
      result = JS_NewFloat64(ctx, (double)result_storage.f);
      break;
    case JSRT_FFI_TYPE_DOUBLE:
      result = JS_NewFloat64(ctx, result_storage.d);
      break;
    case JSRT_FFI_TYPE_POINTER:
      result = JS_NewInt64(ctx, (intptr_t)result_storage.ptr);
      break;
    case JSRT_FFI_TYPE_STRING:
      if (result_storage.ptr) {
        result = JS_NewString(ctx, (const char*)result_storage.ptr);
      } else {
        result = JS_NULL;
      }
      break;
    case JSRT_FFI_TYPE_ARRAY:
    case JSRT_FFI_TYPE_CALLBACK:
    case JSRT_FFI_TYPE_STRUCT:
      // These return pointers
      result = JS_NewInt64(ctx, (intptr_t)result_storage.ptr);
      break;
    default:
      result = JS_UNDEFINED;
      break;
  }

  return result;
}
#endif
#endif

// Enhanced error creation with stack trace support
static JSValue create_ffi_error_with_stack(JSContext* ctx, const char* message, const char* function_name) {
  // Create error with enhanced information
  char full_message[512];
  snprintf(full_message, sizeof(full_message), "FFI Error in %s: %s", function_name ? function_name : "unknown",
           message);

  // Create a proper Error object instead of just throwing
  JSValue error_ctor = JS_GetGlobalObject(ctx);
  JSValue error_obj = JS_GetPropertyStr(ctx, error_ctor, "Error");
  JS_FreeValue(ctx, error_ctor);

  if (JS_IsConstructor(ctx, error_obj)) {
    JSValue message_val = JS_NewString(ctx, full_message);
    JSValue error_instance = JS_CallConstructor(ctx, error_obj, 1, &message_val);
    JS_FreeValue(ctx, message_val);
    JS_FreeValue(ctx, error_obj);

    if (!JS_IsException(error_instance)) {
      // Add additional properties to the error for better debugging
      JS_SetPropertyStr(ctx, error_instance, "ffiFunction", function_name ? JS_NewString(ctx, function_name) : JS_NULL);
      JS_SetPropertyStr(ctx, error_instance, "ffiModule", JS_NewString(ctx, "jsrt:ffi"));

      // Try to add stack trace by accessing the Error.stack property
      JSValue stack = JS_GetPropertyStr(ctx, error_instance, "stack");
      if (!JS_IsUndefined(stack) && !JS_IsNull(stack)) {
        // Stack trace is automatically populated by JavaScript engine
        JS_FreeValue(ctx, stack);
      }

      return JS_Throw(ctx, error_instance);
    }
    JS_FreeValue(ctx, error_instance);
  }
  JS_FreeValue(ctx, error_obj);

  // Fallback to the original method if Error constructor fails
  JSValue error = JS_ThrowTypeError(ctx, "%s", full_message);

  // Try to add additional properties to the error for better debugging
  JSValue error_obj_fallback = JS_GetException(ctx);
  if (!JS_IsNull(error_obj_fallback)) {
    JS_SetPropertyStr(ctx, error_obj_fallback, "ffiFunction",
                      function_name ? JS_NewString(ctx, function_name) : JS_NULL);
    JS_SetPropertyStr(ctx, error_obj_fallback, "ffiModule", JS_NewString(ctx, "jsrt:ffi"));
    JS_Throw(ctx, error_obj_fallback);
  }

  return error;
}

// Enhanced error creation with stack trace support
static JSValue create_ffi_error(JSContext* ctx, const char* message, const char* function_name) {
  return create_ffi_error_with_stack(ctx, message, function_name);
}

// Enhanced argument validation with detailed error messages
static JSValue validate_ffi_arguments(JSContext* ctx, const char* function_name, int expected_argc, int actual_argc,
                                      const char* expected_types[]) {
  if (actual_argc != expected_argc) {
    char message[256];
    snprintf(message, sizeof(message), "Expected %d argument%s, got %d", expected_argc, expected_argc == 1 ? "" : "s",
             actual_argc);
    return create_ffi_error(ctx, message, function_name);
  }

  return JS_UNDEFINED;  // No error
}

// Enhanced library loading error with ReferenceError type
static JSValue create_library_load_error(JSContext* ctx, const char* lib_name) {
  char message[512];
  char full_message[600];

#ifdef _WIN32
  DWORD error_code = GetLastError();
  snprintf(message, sizeof(message),
           "Failed to load library '%s' (Windows error code: %lu). "
           "Please check: 1) Library exists and is accessible, "
           "2) All dependencies are available, "
           "3) Architecture matches (32-bit vs 64-bit)",
           lib_name, error_code);
#else
  const char* dlerror_msg = dlerror();
  snprintf(message, sizeof(message),
           "Failed to load library '%s': %s. "
           "Please check: 1) Library exists in system path or provide full path, "
           "2) Library has correct permissions, "
           "3) All dependencies are satisfied (check with ldd)",
           lib_name, dlerror_msg ? dlerror_msg : "Unknown error");
#endif

  // Create ReferenceError instead of generic Error for library loading failures
  snprintf(full_message, sizeof(full_message), "FFI Error in ffi.Library: %s", message);

  JSValue error_ctor = JS_GetGlobalObject(ctx);
  JSValue ref_error_obj = JS_GetPropertyStr(ctx, error_ctor, "ReferenceError");
  JS_FreeValue(ctx, error_ctor);

  if (JS_IsConstructor(ctx, ref_error_obj)) {
    JSValue message_val = JS_NewString(ctx, full_message);
    JSValue error_instance = JS_CallConstructor(ctx, ref_error_obj, 1, &message_val);
    JS_FreeValue(ctx, message_val);
    JS_FreeValue(ctx, ref_error_obj);

    if (!JS_IsException(error_instance)) {
      // Add additional properties for debugging
      JS_SetPropertyStr(ctx, error_instance, "ffiFunction", JS_NewString(ctx, "ffi.Library"));
      JS_SetPropertyStr(ctx, error_instance, "ffiModule", JS_NewString(ctx, "jsrt:ffi"));
      return JS_Throw(ctx, error_instance);
    }
    JS_FreeValue(ctx, error_instance);
  }
  JS_FreeValue(ctx, ref_error_obj);

  // Fallback to original method
  return create_ffi_error(ctx, message, "ffi.Library");
}

// Enhanced function resolution error
static JSValue create_function_resolve_error(JSContext* ctx, const char* func_name, const char* lib_name) {
  char message[512];
  snprintf(message, sizeof(message),
           "Function '%s' not found in library '%s'. "
           "Please check: 1) Function name is correct and exported, "
           "2) Function is not mangled (use extern \"C\" in C++), "
           "3) Library was compiled with function visibility",
           func_name, lib_name);
  return create_ffi_error(ctx, message, "ffi.Library");
}

// Forward declare class IDs
static JSClassID JSRT_FFILibraryClassID;
static JSClassID JSRT_FFIFunctionClassID;

// Forward declare function call handler
static JSValue ffi_function_call(JSContext* ctx, JSValueConst func_obj, JSValueConst this_val, int argc,
                                 JSValueConst* argv, int flags);

// FFI library finalizer
static void ffi_library_finalizer(JSRuntime* rt, JSValue val) {
  ffi_library_t* lib = (ffi_library_t*)JS_GetOpaque(val, JSRT_FFILibraryClassID);
  if (lib) {
    JSRT_Debug("FFI: Finalizing library '%s'", lib->name ? lib->name : "unknown");
    if (lib->handle) {
      JSRT_DLCLOSE(lib->handle);
    }
    if (lib->name) {
      free(lib->name);
    }
    JS_FreeValueRT(rt, lib->functions);
    free(lib);
  }
}

// FFI function finalizer
static void ffi_function_finalizer(JSRuntime* rt, JSValue val) {
  ffi_function_wrapper_t* wrapper = (ffi_function_wrapper_t*)JS_GetOpaque(val, JSRT_FFIFunctionClassID);
  if (wrapper) {
    JSRT_Debug("FFI: Finalizing function wrapper with ID %d", wrapper->function_id);
    free(wrapper);
  }
}

// Class definitions
static JSClassDef JSRT_FFILibraryClass = {
    .class_name = "FFILibrary",
    .finalizer = ffi_library_finalizer,
};

static JSClassDef JSRT_FFIFunctionClass = {
    .class_name = "FFIFunction",
    .finalizer = ffi_function_finalizer,
    .call = ffi_function_call,
};

// Convert JS value to native value based on FFI type
static bool js_to_native(JSContext* ctx, JSValue val, jsrt_ffi_type_t type, void* result) {
  switch (type) {
    case JSRT_FFI_TYPE_VOID:
      return true;

    case JSRT_FFI_TYPE_INT:
    case JSRT_FFI_TYPE_INT32: {
      int32_t i;
      if (JS_ToInt32(ctx, &i, val) < 0)
        return false;
      *(int32_t*)result = i;
      return true;
    }

    case JSRT_FFI_TYPE_UINT:
    case JSRT_FFI_TYPE_UINT32: {
      uint32_t u;
      if (JS_ToUint32(ctx, &u, val) < 0)
        return false;
      *(uint32_t*)result = u;
      return true;
    }

    case JSRT_FFI_TYPE_INT64: {
      int64_t i;
      if (JS_ToInt64(ctx, &i, val) < 0)
        return false;
      *(int64_t*)result = i;
      return true;
    }

    case JSRT_FFI_TYPE_UINT64: {
      uint64_t u;
      if (JS_ToIndex(ctx, &u, val) < 0)
        return false;
      *(uint64_t*)result = u;
      return true;
    }

    case JSRT_FFI_TYPE_FLOAT: {
      double d;
      if (JS_ToFloat64(ctx, &d, val) < 0)
        return false;
      *(float*)result = (float)d;
      return true;
    }

    case JSRT_FFI_TYPE_DOUBLE: {
      double d;
      if (JS_ToFloat64(ctx, &d, val) < 0)
        return false;
      *(double*)result = d;
      return true;
    }

    case JSRT_FFI_TYPE_STRING: {
      const char* str = JS_ToCString(ctx, val);
      if (!str)
        return false;
      *(const char**)result = str;
      return true;
    }

    case JSRT_FFI_TYPE_ARRAY: {
      if (!JS_IsArray(ctx, val)) {
        // Not an array - treat as null pointer
        *(void**)result = NULL;
        return true;
      }

      // Convert JavaScript array to native array
      JSValue length_val = JS_GetPropertyStr(ctx, val, "length");
      uint32_t length;
      if (JS_ToUint32(ctx, &length, length_val) < 0) {
        JS_FreeValue(ctx, length_val);
        *(void**)result = NULL;
        return true;
      }
      JS_FreeValue(ctx, length_val);

      if (length == 0) {
        *(void**)result = NULL;
        return true;
      }

      // For now, assume arrays are int32 arrays (most common case)
      // In a more complete implementation, we'd need type information
      int32_t* native_array = malloc(length * sizeof(int32_t));
      if (!native_array) {
        *(void**)result = NULL;
        return false;
      }

      for (uint32_t i = 0; i < length; i++) {
        JSValue element = JS_GetPropertyUint32(ctx, val, i);
        int32_t element_val;
        if (JS_ToInt32(ctx, &element_val, element) < 0) {
          element_val = 0;  // Default value for conversion failures
        }
        native_array[i] = element_val;
        JS_FreeValue(ctx, element);
      }

      *(void**)result = native_array;
      return true;
    }

    case JSRT_FFI_TYPE_POINTER: {
      // For now, treat pointers as null
      *(void**)result = NULL;
      return true;
    }

    default:
      return false;
  }
}

// Convert native value to JS value based on FFI type
static JSValue native_to_js(JSContext* ctx, jsrt_ffi_type_t type, void* value) {
  switch (type) {
    case JSRT_FFI_TYPE_VOID:
      return JS_UNDEFINED;

    case JSRT_FFI_TYPE_INT:
    case JSRT_FFI_TYPE_INT32:
      return JS_NewInt32(ctx, *(int32_t*)value);

    case JSRT_FFI_TYPE_UINT:
    case JSRT_FFI_TYPE_UINT32:
      return JS_NewUint32(ctx, *(uint32_t*)value);

    case JSRT_FFI_TYPE_INT64:
      return JS_NewInt64(ctx, *(int64_t*)value);

    case JSRT_FFI_TYPE_UINT64:
      return JS_NewBigUint64(ctx, *(uint64_t*)value);

    case JSRT_FFI_TYPE_FLOAT:
      return JS_NewFloat64(ctx, *(float*)value);

    case JSRT_FFI_TYPE_DOUBLE:
      return JS_NewFloat64(ctx, *(double*)value);

    case JSRT_FFI_TYPE_STRING: {
      const char* str = *(const char**)value;
      return str ? JS_NewString(ctx, str) : JS_NULL;
    }

    case JSRT_FFI_TYPE_ARRAY: {
      // For arrays returned from C functions, we need length information
      // This is a limitation - we can't know the array length without additional metadata
      // For now, return the pointer as a number that can be used with other FFI functions
      void* ptr = *(void**)value;
      return ptr ? JS_NewInt64(ctx, (intptr_t)ptr) : JS_NULL;
    }

    case JSRT_FFI_TYPE_POINTER: {
      void* ptr = *(void**)value;
      return ptr ? JS_NewInt64(ctx, (intptr_t)ptr) : JS_NULL;
    }

    default:
      return JS_UNDEFINED;
  }
}

// FFI function call implementation
static JSValue ffi_function_call(JSContext* ctx, JSValueConst func_obj, JSValueConst this_val, int argc,
                                 JSValueConst* argv, int flags) {
  JSRT_Debug("FFI Call: Starting function call with argc=%d", argc);

  // Get function ID from opaque data - use func_obj instead of this_val
  ffi_function_wrapper_t* wrapper = (ffi_function_wrapper_t*)JS_GetOpaque(func_obj, JSRT_FFIFunctionClassID);
  if (!wrapper) {
    return JS_ThrowTypeError(ctx, "Invalid FFI function - no wrapper data found");
  }

  // Get metadata from global storage
  jsrt_ffi_function_t func_metadata;
  if (!get_function_metadata_by_id(ctx, wrapper->function_id, &func_metadata)) {
    return JS_ThrowTypeError(ctx, "FFI function metadata not found");
  }

  JSRT_Debug("FFI Call: Got function metadata: return_type=%d, arg_count=%d, func_ptr=%p", func_metadata.return_type,
             func_metadata.arg_count, func_metadata.func_ptr);

  if (!func_metadata.func_ptr) {
    return JS_ThrowTypeError(ctx, "Invalid FFI function metadata - missing function pointer");
  }

  // Enhanced implementation for function calls with up to 16 arguments
  if (func_metadata.arg_count > 16) {
    return JS_ThrowTypeError(ctx, "FFI functions with more than 16 arguments not supported");
  }

  // Convert arguments to native values - now supports up to 16 arguments and arrays
  uint64_t args[16] = {0};
  const char* string_args[16] = {NULL};
  void* array_args[16] = {NULL};  // Store array pointers for cleanup

  for (int i = 0; i < argc; i++) {
    if (JS_IsString(argv[i])) {
      string_args[i] = JS_ToCString(ctx, argv[i]);
      args[i] = (uint64_t)(uintptr_t)string_args[i];
    } else if (JS_IsArray(ctx, argv[i])) {
      // Handle JavaScript arrays
      JSValue length_val = JS_GetPropertyStr(ctx, argv[i], "length");
      uint32_t length;
      if (JS_ToUint32(ctx, &length, length_val) < 0) {
        JS_FreeValue(ctx, length_val);
        args[i] = 0;  // Null pointer for invalid arrays
      } else {
        JS_FreeValue(ctx, length_val);

        if (length == 0) {
          args[i] = 0;  // Null pointer for empty arrays
        } else {
          // Convert to int32 array (most common case)
          int32_t* native_array = malloc(length * sizeof(int32_t));
          if (native_array) {
            for (uint32_t j = 0; j < length; j++) {
              JSValue element = JS_GetPropertyUint32(ctx, argv[i], j);
              int32_t element_val;
              if (JS_ToInt32(ctx, &element_val, element) < 0) {
                element_val = 0;  // Default value
              }
              native_array[j] = element_val;
              JS_FreeValue(ctx, element);
            }
            array_args[i] = native_array;  // Store for cleanup
            args[i] = (uint64_t)(uintptr_t)native_array;
          } else {
            args[i] = 0;  // Allocation failed
          }
        }
      }
    } else if (JS_IsNumber(argv[i])) {
      // Handle different number types
      double num_double;
      if (JS_ToFloat64(ctx, &num_double, argv[i]) == 0) {
        // Check if it's an integer or floating point
        if (num_double == (double)(int64_t)num_double) {
          // It's an integer
          int64_t num_int = (int64_t)num_double;
          args[i] = (uint64_t)num_int;
        } else {
          // It's a floating point - store as double in union
          union {
            double d;
            uint64_t u;
          } converter;
          converter.d = num_double;
          args[i] = converter.u;
        }
      } else {
        // Fallback - try int conversion
        int32_t num;
        if (JS_ToInt32(ctx, &num, argv[i]) < 0) {
          // Clean up allocated resources
          for (int j = 0; j < i; j++) {
            if (string_args[j])
              JS_FreeCString(ctx, string_args[j]);
            if (array_args[j])
              free(array_args[j]);
          }
          return JS_ThrowTypeError(ctx, "Failed to convert argument %d to number", i);
        }
        args[i] = (uint64_t)(int64_t)num;
      }
    } else if (JS_IsBool(argv[i])) {
      // Handle boolean as integer
      args[i] = JS_ToBool(ctx, argv[i]) ? 1 : 0;
    } else if (JS_IsNull(argv[i]) || JS_IsUndefined(argv[i])) {
      // Handle null/undefined as null pointer
      args[i] = 0;
    } else {
      // Clean up allocated resources
      for (int j = 0; j < i; j++) {
        if (string_args[j])
          JS_FreeCString(ctx, string_args[j]);
        if (array_args[j])
          free(array_args[j]);
      }
      return JS_ThrowTypeError(ctx, "Unsupported argument type at position %d", i);
    }
  }

  // Perform the function call based on signature with enhanced type support
  JSValue result = JS_UNDEFINED;
  jsrt_ffi_type_t return_type = func_metadata.return_type;
  void* func_ptr = func_metadata.func_ptr;

  // Handle different return types with expanded argument support
  if (return_type == JSRT_FFI_TYPE_INT || return_type == JSRT_FFI_TYPE_INT32) {
    int32_t ret_val = 0;
    switch (argc) {
      case 0:
        ret_val = ((int32_t (*)())func_ptr)();
        break;
      case 1:
        ret_val = ((int32_t (*)(uintptr_t))func_ptr)(args[0]);
        break;
      case 2:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]);
        break;
      case 3:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]);
        break;
      case 4:
        ret_val =
            ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]);
        break;
      case 5:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4]);
        break;
      case 6:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
      case 7:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        break;
      case 8:
        ret_val =
            ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                          uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        break;
      case 9:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6],
                                                     args[7], args[8]);
        break;
      case 10:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5],
                                                                args[6], args[7], args[8], args[9]);
        break;
      case 11:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10]);
        break;
      case 12:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10],
            args[11]);
        break;
      case 13:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10],
            args[11], args[12]);
        break;
      case 14:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10],
            args[11], args[12], args[13]);
        break;
      case 15:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10],
            args[11], args[12], args[13], args[14]);
        break;
      case 16:
        ret_val = ((int32_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6],
                                                     args[7], args[8], args[9], args[10], args[11], args[12], args[13],
                                                     args[14], args[15]);
        break;
    }
    result = JS_NewInt32(ctx, ret_val);
  } else if (return_type == JSRT_FFI_TYPE_INT64) {
    int64_t ret_val = 0;
    switch (argc) {
      case 0:
        ret_val = ((int64_t (*)())func_ptr)();
        break;
      case 1:
        ret_val = ((int64_t (*)(uintptr_t))func_ptr)(args[0]);
        break;
      case 2:
        ret_val = ((int64_t (*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]);
        break;
      case 3:
        ret_val = ((int64_t (*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]);
        break;
      case 4:
        ret_val =
            ((int64_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]);
        break;
      case 5:
        ret_val = ((int64_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4]);
        break;
      case 6:
        ret_val = ((int64_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
      case 7:
        ret_val = ((int64_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        break;
      case 8:
        ret_val =
            ((int64_t (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                          uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        break;
      // Continue with more cases for up to 16 args
      default:
        ret_val = 0;  // Fallback, should not reach here due to earlier check
    }
    result = JS_NewInt64(ctx, ret_val);
  } else if (return_type == JSRT_FFI_TYPE_DOUBLE) {
    double ret_val = 0.0;
    switch (argc) {
      case 0:
        ret_val = ((double (*)())func_ptr)();
        break;
      case 1:
        ret_val = ((double (*)(uintptr_t))func_ptr)(args[0]);
        break;
      case 2:
        ret_val = ((double (*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]);
        break;
      case 3:
        ret_val = ((double (*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]);
        break;
      case 4:
        ret_val =
            ((double (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]);
        break;
      case 5:
        ret_val = ((double (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4]);
        break;
      case 6:
        ret_val = ((double (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
      case 7:
        ret_val = ((double (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        break;
      case 8:
        ret_val =
            ((double (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                         uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        break;
      // Continue with more cases for up to 16 args
      default:
        ret_val = 0.0;  // Fallback
    }
    result = JS_NewFloat64(ctx, ret_val);
  } else if (return_type == JSRT_FFI_TYPE_FLOAT) {
    float ret_val = 0.0f;
    switch (argc) {
      case 0:
        ret_val = ((float (*)())func_ptr)();
        break;
      case 1:
        ret_val = ((float (*)(uintptr_t))func_ptr)(args[0]);
        break;
      case 2:
        ret_val = ((float (*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]);
        break;
      case 3:
        ret_val = ((float (*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]);
        break;
      case 4:
        ret_val = ((float (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]);
        break;
      case 5:
        ret_val = ((float (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4]);
        break;
      case 6:
        ret_val = ((float (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
      case 7:
        ret_val = ((float (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        break;
      case 8:
        ret_val =
            ((float (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                        uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        break;
      // Continue with more cases for up to 16 args
      default:
        ret_val = 0.0f;  // Fallback
    }
    result = JS_NewFloat64(ctx, (double)ret_val);
  } else if (return_type == JSRT_FFI_TYPE_STRING) {
    const char* ret_str = NULL;
    switch (argc) {
      case 0:
        ret_str = ((const char* (*)())func_ptr)();
        break;
      case 1:
        ret_str = ((const char* (*)(uintptr_t))func_ptr)(args[0]);
        break;
      case 2:
        ret_str = ((const char* (*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]);
        break;
      case 3:
        ret_str = ((const char* (*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]);
        break;
      case 4:
        ret_str =
            ((const char* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]);
        break;
      case 5:
        ret_str = ((const char* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4]);
        break;
      case 6:
        ret_str = ((const char* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
      case 7:
        ret_str = ((const char* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                    uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        break;
      case 8:
        ret_str = ((const char* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                    uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6],
                                                         args[7]);
        break;
      // Continue with more cases for up to 16 args
      default:
        ret_str = NULL;  // Fallback
    }
    result = ret_str ? JS_NewString(ctx, ret_str) : JS_NULL;
  } else if (return_type == JSRT_FFI_TYPE_POINTER) {
    void* ret_ptr = NULL;
    switch (argc) {
      case 0:
        ret_ptr = ((void* (*)())func_ptr)();
        break;
      case 1:
        ret_ptr = ((void* (*)(uintptr_t))func_ptr)(args[0]);
        break;
      case 2:
        ret_ptr = ((void* (*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]);
        break;
      case 3:
        ret_ptr = ((void* (*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]);
        break;
      case 4:
        ret_ptr = ((void* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]);
        break;
      case 5:
        ret_ptr = ((void* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4]);
        break;
      case 6:
        ret_ptr = ((void* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
      case 7:
        ret_ptr = ((void* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        break;
      case 8:
        ret_ptr =
            ((void* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                        uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        break;
      // Continue with more cases for up to 16 args
      default:
        ret_ptr = NULL;  // Fallback
    }
    result = ret_ptr ? JS_NewInt64(ctx, (intptr_t)ret_ptr) : JS_NULL;
  } else if (return_type == JSRT_FFI_TYPE_VOID) {
    switch (argc) {
      case 0:
        ((void (*)())func_ptr)();
        break;
      case 1:
        ((void (*)(uintptr_t))func_ptr)(args[0]);
        break;
      case 2:
        ((void (*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]);
        break;
      case 3:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]);
        break;
      case 4:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]);
        break;
      case 5:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3],
                                                                                    args[4]);
        break;
      case 6:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
      case 7:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        break;
      case 8:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        break;
      case 9:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                   uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7],
                                        args[8]);
        break;
      case 10:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                   uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8],
                                        args[9]);
        break;
      case 11:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                   uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6],
                                                   args[7], args[8], args[9], args[10]);
        break;
      case 12:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                   uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5],
                                                              args[6], args[7], args[8], args[9], args[10], args[11]);
        break;
      case 13:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                   uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4],
                                                                         args[5], args[6], args[7], args[8], args[9],
                                                                         args[10], args[11], args[12]);
        break;
      case 14:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                   uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10],
            args[11], args[12], args[13]);
        break;
      case 15:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                   uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10],
            args[11], args[12], args[13], args[14]);
        break;
      case 16:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                   uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(
            args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10],
            args[11], args[12], args[13], args[14], args[15]);
        break;
    }
    result = JS_UNDEFINED;
  } else {
    char type_name[32];
    snprintf(type_name, sizeof(type_name), "type_%d", (int)return_type);
    result = JS_ThrowTypeError(ctx, "Unsupported return type: %s", type_name);
  }

  // Clean up allocated resources
  for (int i = 0; i < argc; i++) {
    if (string_args[i]) {
      JS_FreeCString(ctx, string_args[i]);
    }
    if (array_args[i]) {
      free(array_args[i]);
    }
  }

  return result;
}

// ffi.Library(name, functions) - Load a dynamic library
static JSValue ffi_library(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Enhanced argument validation
  if (argc < 2) {
    return create_ffi_error(ctx, "Expected 2 arguments: library name and function definitions", "ffi.Library");
  }

  const char* lib_name = JS_ToCString(ctx, argv[0]);
  if (!lib_name) {
    return create_ffi_error(ctx, "Library name must be a string", "ffi.Library");
  }

  if (!JS_IsObject(argv[1])) {
    JS_FreeCString(ctx, lib_name);
    return create_ffi_error(ctx, "Function definitions must be an object", "ffi.Library");
  }

  // Load the library with enhanced error reporting
  jsrt_dl_handle_t handle = JSRT_DLOPEN(lib_name);
  if (!handle) {
    JSValue error = create_library_load_error(ctx, lib_name);
    JS_FreeCString(ctx, lib_name);
    return error;
  }

  JSRT_Debug("FFI: Successfully loaded library '%s'", lib_name);

  // Create library object
  ffi_library_t* lib = malloc(sizeof(ffi_library_t));
  if (!lib) {
    JSRT_DLCLOSE(handle);
    JS_FreeCString(ctx, lib_name);
    return create_ffi_error(ctx, "Failed to allocate memory for library object", "ffi.Library");
  }

  lib->handle = handle;
  lib->name = strdup(lib_name);
  lib->functions = JS_NewObject(ctx);

  // Create JS object for the library
  JSValue lib_obj = JS_NewObjectClass(ctx, JSRT_FFILibraryClassID);
  JS_SetOpaque(lib_obj, lib);
  JS_SetPropertyStr(ctx, lib_obj, "_handle", JS_NewInt64(ctx, (intptr_t)handle));
  JS_SetPropertyStr(ctx, lib_obj, "_name", JS_NewString(ctx, lib_name));

  // Iterate through function definitions
  JSPropertyEnum* props;
  uint32_t prop_count;
  if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, argv[1], JS_GPN_STRING_MASK) < 0) {
    ffi_library_finalizer(JS_GetRuntime(ctx), lib_obj);
    JS_FreeCString(ctx, lib_name);
    return JS_EXCEPTION;
  }

  for (uint32_t i = 0; i < prop_count; i++) {
    JSValue prop_val = JS_GetProperty(ctx, argv[1], props[i].atom);
    if (JS_IsException(prop_val))
      continue;

    if (!JS_IsArray(ctx, prop_val)) {
      JS_FreeValue(ctx, prop_val);
      continue;
    }

    // Parse function signature: [return_type, [arg_types...]]
    JSValue return_type_val = JS_GetPropertyUint32(ctx, prop_val, 0);
    JSValue args_val = JS_GetPropertyUint32(ctx, prop_val, 1);

    if (!JS_IsString(return_type_val) || !JS_IsArray(ctx, args_val)) {
      JS_FreeValue(ctx, prop_val);
      JS_FreeValue(ctx, return_type_val);
      JS_FreeValue(ctx, args_val);
      continue;
    }

    const char* return_type_str = JS_ToCString(ctx, return_type_val);
    const char* func_name = JS_AtomToCString(ctx, props[i].atom);

    if (!return_type_str || !func_name) {
      JS_FreeValue(ctx, prop_val);
      JS_FreeValue(ctx, return_type_val);
      JS_FreeValue(ctx, args_val);
      if (return_type_str)
        JS_FreeCString(ctx, return_type_str);
      if (func_name)
        JS_FreeCString(ctx, func_name);
      continue;
    }

    // Get function from library with enhanced error reporting
    void* func_ptr = JSRT_DLSYM(handle, func_name);
    if (!func_ptr) {
      JSRT_Debug("FFI: Function '%s' not found in library '%s'", func_name, lib_name);

      // For now, we'll log a warning but continue - in a stricter implementation,
      // we could throw an error here
      char warning_msg[256];
      snprintf(warning_msg, sizeof(warning_msg), "Warning: Function '%s' not found in library '%s' - skipping",
               func_name, lib_name);

      // Log to console if available
      JSValue global = JS_GetGlobalObject(ctx);
      JSValue console = JS_GetPropertyStr(ctx, global, "console");
      if (!JS_IsUndefined(console)) {
        JSValue warn_func = JS_GetPropertyStr(ctx, console, "warn");
        if (!JS_IsUndefined(warn_func)) {
          JSValue warn_msg = JS_NewString(ctx, warning_msg);
          JS_Call(ctx, warn_func, console, 1, &warn_msg);
          JS_FreeValue(ctx, warn_msg);
        }
        JS_FreeValue(ctx, warn_func);
      }
      JS_FreeValue(ctx, console);
      JS_FreeValue(ctx, global);

      JS_FreeValue(ctx, prop_val);
      JS_FreeValue(ctx, return_type_val);
      JS_FreeValue(ctx, args_val);
      JS_FreeCString(ctx, return_type_str);
      JS_FreeCString(ctx, func_name);
      continue;
    }

    // Parse argument types
    JSValue args_length_val = JS_GetPropertyStr(ctx, args_val, "length");
    uint32_t args_length;
    if (JS_ToUint32(ctx, &args_length, args_length_val) < 0) {
      args_length = 0;
    }
    JS_FreeValue(ctx, args_length_val);

    // Create function metadata structure and store it globally
    jsrt_ffi_function_t func_metadata;
    func_metadata.return_type = string_to_ffi_type(return_type_str);
    func_metadata.arg_count = args_length;
    func_metadata.arg_types = NULL;  // TODO: parse argument types if needed
    func_metadata.func_ptr = func_ptr;

    // Store metadata in global map and get function ID
    int function_id = store_function_metadata(ctx, &func_metadata);

    // Create wrapper structure
    ffi_function_wrapper_t* wrapper = malloc(sizeof(ffi_function_wrapper_t));
    if (!wrapper) {
      JS_FreeValue(ctx, prop_val);
      JS_FreeValue(ctx, return_type_val);
      JS_FreeValue(ctx, args_val);
      JS_FreeCString(ctx, return_type_str);
      JS_FreeCString(ctx, func_name);
      continue;  // Skip this function
    }
    wrapper->function_id = function_id;

    // Create callable function object
    JSValue js_func = JS_NewObjectClass(ctx, JSRT_FFIFunctionClassID);
    JS_SetOpaque(js_func, wrapper);

    JS_SetPropertyStr(ctx, lib_obj, func_name, js_func);

    JSRT_Debug("FFI: Added function '%s' to library '%s'", func_name, lib_name);

    JS_FreeValue(ctx, prop_val);
    JS_FreeValue(ctx, return_type_val);
    JS_FreeValue(ctx, args_val);
    JS_FreeCString(ctx, return_type_str);
    JS_FreeCString(ctx, func_name);
  }

  js_free(ctx, props);
  JS_FreeCString(ctx, lib_name);

  return lib_obj;
}

// Memory allocation function - ffi.malloc(size)
static JSValue ffi_malloc(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return create_ffi_error(ctx, "Expected 1 argument: size", "ffi.malloc");
  }

  uint32_t size;
  if (JS_ToUint32(ctx, &size, argv[0]) < 0) {
    return create_ffi_error(ctx, "Size must be a positive number", "ffi.malloc");
  }

  if (size == 0) {
    return create_ffi_error(ctx, "Cannot allocate zero bytes (use a positive size)", "ffi.malloc");
  }

  if (size > 1024 * 1024 * 1024) {  // Limit to 1GB for safety
    char message[128];
    snprintf(message, sizeof(message), "Allocation size too large: %u bytes (maximum: 1GB)", size);
    return create_ffi_error(ctx, message, "ffi.malloc");
  }

  void* ptr = malloc(size);
  if (!ptr) {
    char message[128];
    snprintf(message, sizeof(message), "Failed to allocate %u bytes (out of memory)", size);
    return create_ffi_error(ctx, message, "ffi.malloc");
  }

  JSRT_Debug("FFI: Allocated %u bytes at %p", size, ptr);

  // Return pointer as integer (can be passed to native functions)
  return JS_NewInt64(ctx, (intptr_t)ptr);
}

// Memory deallocation function - ffi.free(ptr)
static JSValue ffi_free(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "ffi.free expects 1 argument: pointer");
  }

  int64_t ptr_addr;
  if (JS_ToInt64(ctx, &ptr_addr, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "Pointer must be a number");
  }

  if (ptr_addr == 0) {
    return JS_ThrowTypeError(ctx, "Cannot free null pointer");
  }

  void* ptr = (void*)(intptr_t)ptr_addr;
  JSRT_Debug("FFI: Freeing memory at %p", ptr);
  free(ptr);

  return JS_UNDEFINED;
}

// Memory copy function - ffi.memcpy(dest, src, size)
static JSValue ffi_memcpy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "ffi.memcpy expects 3 arguments: dest, src, size");
  }

  int64_t dest_addr, src_addr;
  uint32_t size;

  if (JS_ToInt64(ctx, &dest_addr, argv[0]) < 0 || JS_ToInt64(ctx, &src_addr, argv[1]) < 0 ||
      JS_ToUint32(ctx, &size, argv[2]) < 0) {
    return JS_ThrowTypeError(ctx, "Invalid arguments for memcpy");
  }

  if (dest_addr == 0 || src_addr == 0) {
    return JS_ThrowTypeError(ctx, "Cannot copy to/from null pointer");
  }

  if (size > 1024 * 1024) {  // Limit to 1MB for safety
    return JS_ThrowRangeError(ctx, "Copy size too large: %u", size);
  }

  void* dest = (void*)(intptr_t)dest_addr;
  void* src = (void*)(intptr_t)src_addr;

  memcpy(dest, src, size);
  JSRT_Debug("FFI: Copied %u bytes from %p to %p", size, src, dest);

  return JS_UNDEFINED;
}

// Read string from pointer - ffi.readString(ptr, [maxLength])
static JSValue ffi_read_string(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return create_ffi_error(ctx, "Expected at least 1 argument: pointer", "ffi.readString");
  }

  int64_t ptr_addr;
  if (JS_ToInt64(ctx, &ptr_addr, argv[0]) < 0) {
    return create_ffi_error(ctx, "Pointer must be a number", "ffi.readString");
  }

  if (ptr_addr == 0) {
    return JS_NULL;
  }

  const char* str = (const char*)(intptr_t)ptr_addr;

  // Optional max length parameter for safety
  uint32_t max_len = 4096;  // Default max length
  if (argc > 1) {
    if (JS_ToUint32(ctx, &max_len, argv[1]) < 0) {
      max_len = 4096;
    }
  }

  // Use strnlen to safely get string length
  size_t len = strnlen(str, max_len);
  return JS_NewStringLen(ctx, str, len);
}

// Write string to pointer - ffi.writeString(ptr, str)
static JSValue ffi_write_string(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return create_ffi_error(ctx, "Expected 2 arguments: pointer and string", "ffi.writeString");
  }

  int64_t ptr_addr;
  if (JS_ToInt64(ctx, &ptr_addr, argv[0]) < 0) {
    return create_ffi_error(ctx, "Pointer must be a number", "ffi.writeString");
  }

  if (ptr_addr == 0) {
    return create_ffi_error(ctx, "Cannot write to null pointer", "ffi.writeString");
  }

  const char* str = JS_ToCString(ctx, argv[1]);
  if (!str) {
    return create_ffi_error(ctx, "String argument required", "ffi.writeString");
  }

  char* dest = (char*)(intptr_t)ptr_addr;
  strcpy(dest, str);

  JS_FreeCString(ctx, str);
  JSRT_Debug("FFI: Wrote string to %p", dest);

  return JS_UNDEFINED;
}

// Array manipulation functions for better array support

// Create JavaScript array from C array - ffi.arrayFromPointer(ptr, length, type)
static JSValue ffi_array_from_pointer(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return create_ffi_error(ctx, "Expected 3 arguments: pointer, length, type", "ffi.arrayFromPointer");
  }

  int64_t ptr_addr;
  uint32_t length;
  const char* type_str;

  if (JS_ToInt64(ctx, &ptr_addr, argv[0]) < 0) {
    return create_ffi_error(ctx, "Pointer must be a number", "ffi.arrayFromPointer");
  }

  if (JS_ToUint32(ctx, &length, argv[1]) < 0) {
    return create_ffi_error(ctx, "Length must be a positive number", "ffi.arrayFromPointer");
  }

  type_str = JS_ToCString(ctx, argv[2]);
  if (!type_str) {
    return create_ffi_error(ctx, "Type must be a string", "ffi.arrayFromPointer");
  }

  if (ptr_addr == 0) {
    JS_FreeCString(ctx, type_str);
    return JS_NULL;
  }

  if (length > 1024 * 1024) {  // Limit to reasonable size
    JS_FreeCString(ctx, type_str);
    char message[128];
    snprintf(message, sizeof(message), "Array length too large: %u (maximum: 1M elements)", length);
    return create_ffi_error(ctx, message, "ffi.arrayFromPointer");
  }

  // Validate type
  jsrt_ffi_type_t type = string_to_ffi_type(type_str);
  if (type == JSRT_FFI_TYPE_VOID || type == JSRT_FFI_TYPE_STRING || type == JSRT_FFI_TYPE_ARRAY) {
    JS_FreeCString(ctx, type_str);
    char message[128];
    snprintf(message, sizeof(message), "Invalid array element type: '%s' (use int32, float, double, etc.)", type_str);
    return create_ffi_error(ctx, message, "ffi.arrayFromPointer");
  }

  void* ptr = (void*)(intptr_t)ptr_addr;
  JSValue array = JS_NewArray(ctx);

  // Convert C array to JavaScript array based on type
  for (uint32_t i = 0; i < length; i++) {
    JSValue element;

    switch (type) {
      case JSRT_FFI_TYPE_INT:
      case JSRT_FFI_TYPE_INT32: {
        int32_t* int_array = (int32_t*)ptr;
        element = JS_NewInt32(ctx, int_array[i]);
        break;
      }
      case JSRT_FFI_TYPE_FLOAT: {
        float* float_array = (float*)ptr;
        element = JS_NewFloat64(ctx, (double)float_array[i]);
        break;
      }
      case JSRT_FFI_TYPE_DOUBLE: {
        double* double_array = (double*)ptr;
        element = JS_NewFloat64(ctx, double_array[i]);
        break;
      }
      case JSRT_FFI_TYPE_UINT32: {
        uint32_t* uint_array = (uint32_t*)ptr;
        element = JS_NewUint32(ctx, uint_array[i]);
        break;
      }
      default:
        // Default to treating as int32
        int32_t* default_array = (int32_t*)ptr;
        element = JS_NewInt32(ctx, default_array[i]);
        break;
    }

    JS_SetPropertyUint32(ctx, array, i, element);
  }

  JS_FreeCString(ctx, type_str);
  return array;
}

// Get array length helper - ffi.arrayLength(array)
static JSValue ffi_array_length(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "ffi.arrayLength expects 1 argument: array");
  }

  if (!JS_IsArray(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "Argument must be an array");
  }

  JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
  return length_val;
}

// Global callback registry to track active callbacks
static ffi_callback_t** g_callback_registry = NULL;
static int g_callback_count = 0;
static int g_callback_capacity = 0;

// Register a callback in the global registry
static int register_callback(ffi_callback_t* callback) {
  if (g_callback_count >= g_callback_capacity) {
    int new_capacity = g_callback_capacity == 0 ? 16 : g_callback_capacity * 2;
    ffi_callback_t** new_registry = realloc(g_callback_registry, new_capacity * sizeof(ffi_callback_t*));
    if (!new_registry) {
      return -1;
    }
    g_callback_registry = new_registry;
    g_callback_capacity = new_capacity;
  }

  g_callback_registry[g_callback_count] = callback;
  return g_callback_count++;
}

// Forward declarations for callback trampolines
static uintptr_t callback_trampoline_0();
static uintptr_t callback_trampoline_1(uintptr_t arg1);
static uintptr_t callback_trampoline_2(uintptr_t arg1, uintptr_t arg2);
static uintptr_t callback_trampoline_3(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);
static uintptr_t callback_trampoline_4(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);

// Generic callback handler that calls JavaScript from C
static uintptr_t handle_callback(int callback_id, int argc, uintptr_t* args) {
  if (callback_id < 0 || callback_id >= g_callback_count) {
    JSRT_Debug("FFI: Invalid callback ID: %d", callback_id);
    return 0;
  }

  ffi_callback_t* callback = g_callback_registry[callback_id];
  if (!callback || !callback->ctx) {
    JSRT_Debug("FFI: Invalid callback or context");
    return 0;
  }

  // Prepare JavaScript arguments
  JSValue js_args[16];
  for (int i = 0; i < argc && i < callback->arg_count; i++) {
    if (i < callback->arg_count) {
      switch (callback->arg_types[i]) {
        case JSRT_FFI_TYPE_INT:
        case JSRT_FFI_TYPE_INT32:
          js_args[i] = JS_NewInt32(callback->ctx, (int32_t)args[i]);
          break;
        case JSRT_FFI_TYPE_UINT32:
          js_args[i] = JS_NewUint32(callback->ctx, (uint32_t)args[i]);
          break;
        case JSRT_FFI_TYPE_INT64:
          js_args[i] = JS_NewInt64(callback->ctx, (int64_t)args[i]);
          break;
        case JSRT_FFI_TYPE_FLOAT:
          js_args[i] = JS_NewFloat64(callback->ctx, *(float*)&args[i]);
          break;
        case JSRT_FFI_TYPE_DOUBLE:
          js_args[i] = JS_NewFloat64(callback->ctx, *(double*)&args[i]);
          break;
        case JSRT_FFI_TYPE_STRING:
          js_args[i] = args[i] ? JS_NewString(callback->ctx, (const char*)args[i]) : JS_NULL;
          break;
        case JSRT_FFI_TYPE_POINTER:
          js_args[i] = JS_NewInt64(callback->ctx, (intptr_t)args[i]);
          break;
        default:
          js_args[i] = JS_NewInt64(callback->ctx, (int64_t)args[i]);
          break;
      }
    }
  }

  // Call JavaScript function
  JSValue result = JS_Call(callback->ctx, callback->js_function, JS_UNDEFINED, argc, js_args);

  // Clean up arguments
  for (int i = 0; i < argc && i < callback->arg_count; i++) {
    JS_FreeValue(callback->ctx, js_args[i]);
  }

  // Convert return value
  uintptr_t ret_val = 0;
  if (!JS_IsException(result)) {
    switch (callback->return_type) {
      case JSRT_FFI_TYPE_INT:
      case JSRT_FFI_TYPE_INT32: {
        int32_t int_val;
        JS_ToInt32(callback->ctx, &int_val, result);
        ret_val = (uintptr_t)int_val;
        break;
      }
      case JSRT_FFI_TYPE_UINT32: {
        uint32_t uint_val;
        JS_ToUint32(callback->ctx, &uint_val, result);
        ret_val = (uintptr_t)uint_val;
        break;
      }
      case JSRT_FFI_TYPE_INT64: {
        int64_t int64_val;
        JS_ToInt64(callback->ctx, &int64_val, result);
        ret_val = (uintptr_t)int64_val;
        break;
      }
      case JSRT_FFI_TYPE_FLOAT: {
        double double_val;
        JS_ToFloat64(callback->ctx, &double_val, result);
        float float_val = (float)double_val;
        ret_val = *(uintptr_t*)&float_val;
        break;
      }
      case JSRT_FFI_TYPE_DOUBLE: {
        double double_val;
        JS_ToFloat64(callback->ctx, &double_val, result);
        ret_val = *(uintptr_t*)&double_val;
        break;
      }
      case JSRT_FFI_TYPE_POINTER: {
        int64_t ptr_val;
        JS_ToInt64(callback->ctx, &ptr_val, result);
        ret_val = (uintptr_t)ptr_val;
        break;
      }
      case JSRT_FFI_TYPE_VOID:
      default:
        ret_val = 0;
        break;
    }
  } else {
    // Handle JavaScript exception
    JSValue exception = JS_GetException(callback->ctx);
    JSRT_Debug("FFI: JavaScript callback threw exception");
    JS_FreeValue(callback->ctx, exception);
    ret_val = 0;
  }

  JS_FreeValue(callback->ctx, result);
  return ret_val;
}

// Callback trampolines for different argument counts
static uintptr_t callback_trampoline_0() {
  return handle_callback(0, 0, NULL);
}

static uintptr_t callback_trampoline_1(uintptr_t arg1) {
  uintptr_t args[] = {arg1};
  return handle_callback(1, 1, args);
}

static uintptr_t callback_trampoline_2(uintptr_t arg1, uintptr_t arg2) {
  uintptr_t args[] = {arg1, arg2};
  return handle_callback(2, 2, args);
}

static uintptr_t callback_trampoline_3(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
  uintptr_t args[] = {arg1, arg2, arg3};
  return handle_callback(3, 3, args);
}

static uintptr_t callback_trampoline_4(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4) {
  uintptr_t args[] = {arg1, arg2, arg3, arg4};
  return handle_callback(4, 4, args);
}

// Create callback function - ffi.Callback(jsFunction, returnType, argTypes)
static JSValue ffi_callback(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return create_ffi_error(ctx, "Expected 3 arguments: function, return type, argument types", "ffi.Callback");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return create_ffi_error(ctx, "First argument must be a JavaScript function", "ffi.Callback");
  }

  const char* return_type_str = JS_ToCString(ctx, argv[1]);
  if (!return_type_str) {
    return create_ffi_error(ctx, "Return type must be a string", "ffi.Callback");
  }

  if (!JS_IsArray(ctx, argv[2])) {
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Argument types must be an array", "ffi.Callback");
  }

  // Get argument count
  JSValue length_val = JS_GetPropertyStr(ctx, argv[2], "length");
  uint32_t arg_count;
  if (JS_ToUint32(ctx, &arg_count, length_val) < 0) {
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Invalid argument types array", "ffi.Callback");
  }
  JS_FreeValue(ctx, length_val);

  if (arg_count > 4) {
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Maximum 4 arguments supported for callbacks", "ffi.Callback");
  }

  // Create callback structure
  ffi_callback_t* callback = malloc(sizeof(ffi_callback_t));
  if (!callback) {
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Failed to allocate callback structure", "ffi.Callback");
  }

  callback->ctx = ctx;
  callback->js_function = JS_DupValue(ctx, argv[0]);
  callback->return_type = string_to_ffi_type(return_type_str);
  callback->arg_count = arg_count;
  callback->arg_types = arg_count > 0 ? malloc(arg_count * sizeof(jsrt_ffi_type_t)) : NULL;

  // Parse argument types
  for (uint32_t i = 0; i < arg_count; i++) {
    JSValue arg_type_val = JS_GetPropertyUint32(ctx, argv[2], i);
    const char* arg_type_str = JS_ToCString(ctx, arg_type_val);
    if (arg_type_str) {
      callback->arg_types[i] = string_to_ffi_type(arg_type_str);
      JS_FreeCString(ctx, arg_type_str);
    } else {
      callback->arg_types[i] = JSRT_FFI_TYPE_VOID;
    }
    JS_FreeValue(ctx, arg_type_val);
  }

  // Assign trampoline function based on argument count
  switch (arg_count) {
    case 0:
      callback->callback_ptr = (void*)callback_trampoline_0;
      break;
    case 1:
      callback->callback_ptr = (void*)callback_trampoline_1;
      break;
    case 2:
      callback->callback_ptr = (void*)callback_trampoline_2;
      break;
    case 3:
      callback->callback_ptr = (void*)callback_trampoline_3;
      break;
    case 4:
      callback->callback_ptr = (void*)callback_trampoline_4;
      break;
    default:
      free(callback->arg_types);
      JS_FreeValue(ctx, callback->js_function);
      free(callback);
      JS_FreeCString(ctx, return_type_str);
      return create_ffi_error(ctx, "Unsupported argument count for callback", "ffi.Callback");
  }

  // Register callback
  int callback_id = register_callback(callback);
  if (callback_id < 0) {
    free(callback->arg_types);
    JS_FreeValue(ctx, callback->js_function);
    free(callback);
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Failed to register callback", "ffi.Callback");
  }

  JS_FreeCString(ctx, return_type_str);

  // Return pointer to the trampoline function
  return JS_NewInt64(ctx, (intptr_t)callback->callback_ptr);
}

// Enhanced async function call structure for production use
typedef struct {
  JSContext* ctx;
  JSValue promise_resolve;
  JSValue promise_reject;
  void* func_ptr;
  jsrt_ffi_type_t return_type;
  int arg_count;
  uintptr_t* args;
  char** string_args;
  void** array_args;
} ffi_async_call_t;

// Thread pool management for production async calls
#define MAX_ASYNC_THREADS 8
static ffi_async_call_t* active_async_calls[MAX_ASYNC_THREADS];
static int active_call_count = 0;

#ifdef _WIN32
static CRITICAL_SECTION thread_pool_lock;
static bool thread_pool_initialized = false;
#else
static pthread_mutex_t thread_pool_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

// Initialize thread pool management
static void init_thread_pool() {
#ifdef _WIN32
  if (!thread_pool_initialized) {
    InitializeCriticalSection(&thread_pool_lock);
    thread_pool_initialized = true;
  }
#endif
  // pthread mutex is statically initialized
}

// Find available slot for async call
static int find_available_async_slot() {
#ifdef _WIN32
  EnterCriticalSection(&thread_pool_lock);
#else
  pthread_mutex_lock(&thread_pool_lock);
#endif

  int slot = -1;
  for (int i = 0; i < MAX_ASYNC_THREADS; i++) {
    if (active_async_calls[i] == NULL) {
      slot = i;
      break;
    }
  }

#ifdef _WIN32
  LeaveCriticalSection(&thread_pool_lock);
#else
  pthread_mutex_unlock(&thread_pool_lock);
#endif

  return slot;
}

// Register async call in thread pool
static bool register_async_call(ffi_async_call_t* call, int slot) {
#ifdef _WIN32
  EnterCriticalSection(&thread_pool_lock);
#else
  pthread_mutex_lock(&thread_pool_lock);
#endif

  if (slot >= 0 && slot < MAX_ASYNC_THREADS && active_async_calls[slot] == NULL) {
    active_async_calls[slot] = call;
    active_call_count++;

#ifdef _WIN32
    LeaveCriticalSection(&thread_pool_lock);
#else
    pthread_mutex_unlock(&thread_pool_lock);
#endif
    return true;
  }

#ifdef _WIN32
  LeaveCriticalSection(&thread_pool_lock);
#else
  pthread_mutex_unlock(&thread_pool_lock);
#endif
  return false;
}

// Unregister async call from thread pool
static void unregister_async_call(int slot) {
#ifdef _WIN32
  EnterCriticalSection(&thread_pool_lock);
#else
  pthread_mutex_lock(&thread_pool_lock);
#endif

  if (slot >= 0 && slot < MAX_ASYNC_THREADS && active_async_calls[slot] != NULL) {
    active_async_calls[slot] = NULL;
    active_call_count--;
  }

#ifdef _WIN32
  LeaveCriticalSection(&thread_pool_lock);
#else
  pthread_mutex_unlock(&thread_pool_lock);
#endif
}

// Thread function for async FFI calls
#ifdef _WIN32
static DWORD WINAPI async_call_thread(LPVOID lpParam) {
#else
static void* async_call_thread(void* arg) {
#endif
#ifdef _WIN32
  ffi_async_call_t* async_call = (ffi_async_call_t*)lpParam;
#else
  ffi_async_call_t* async_call = (ffi_async_call_t*)arg;
#endif
  if (!async_call) {
#ifdef _WIN32
    return 1;
#else
    return NULL;
#endif
  }

  // Perform the function call in the background thread
  JSValue result = JS_UNDEFINED;
  bool call_success = true;

  if (async_call->return_type == JSRT_FFI_TYPE_INT || async_call->return_type == JSRT_FFI_TYPE_INT32) {
    int ret_val = 0;
    switch (async_call->arg_count) {
      case 0:
        ret_val = ((int (*)())async_call->func_ptr)();
        break;
      case 1:
        ret_val = ((int (*)(uintptr_t))async_call->func_ptr)(async_call->args[0]);
        break;
      case 2:
        ret_val = ((int (*)(uintptr_t, uintptr_t))async_call->func_ptr)(async_call->args[0], async_call->args[1]);
        break;
      case 3:
        ret_val = ((int (*)(uintptr_t, uintptr_t, uintptr_t))async_call->func_ptr)(
            async_call->args[0], async_call->args[1], async_call->args[2]);
        break;
      case 4:
        ret_val = ((int (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))async_call->func_ptr)(
            async_call->args[0], async_call->args[1], async_call->args[2], async_call->args[3]);
        break;
      default:
        ret_val = 0;
        call_success = false;
        break;
    }
    if (call_success) {
      result = JS_NewInt32(async_call->ctx, ret_val);
    }
  } else if (async_call->return_type == JSRT_FFI_TYPE_STRING) {
    const char* ret_str = NULL;
    switch (async_call->arg_count) {
      case 0:
        ret_str = ((const char* (*)())async_call->func_ptr)();
        break;
      case 1:
        ret_str = ((const char* (*)(uintptr_t))async_call->func_ptr)(async_call->args[0]);
        break;
      case 2:
        ret_str =
            ((const char* (*)(uintptr_t, uintptr_t))async_call->func_ptr)(async_call->args[0], async_call->args[1]);
        break;
      case 3:
        ret_str = ((const char* (*)(uintptr_t, uintptr_t, uintptr_t))async_call->func_ptr)(
            async_call->args[0], async_call->args[1], async_call->args[2]);
        break;
      case 4:
        ret_str = ((const char* (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))async_call->func_ptr)(
            async_call->args[0], async_call->args[1], async_call->args[2], async_call->args[3]);
        break;
      default:
        ret_str = NULL;
        call_success = false;
        break;
    }
    if (call_success) {
      result = ret_str ? JS_NewString(async_call->ctx, ret_str) : JS_NULL;
    }
  } else if (async_call->return_type == JSRT_FFI_TYPE_VOID) {
    switch (async_call->arg_count) {
      case 0:
        ((void (*)())async_call->func_ptr)();
        break;
      case 1:
        ((void (*)(uintptr_t))async_call->func_ptr)(async_call->args[0]);
        break;
      case 2:
        ((void (*)(uintptr_t, uintptr_t))async_call->func_ptr)(async_call->args[0], async_call->args[1]);
        break;
      case 3:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t))async_call->func_ptr)(async_call->args[0], async_call->args[1],
                                                                          async_call->args[2]);
        break;
      case 4:
        ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))async_call->func_ptr)(
            async_call->args[0], async_call->args[1], async_call->args[2], async_call->args[3]);
        break;
      default:
        call_success = false;
        break;
    }
    if (call_success) {
      result = JS_UNDEFINED;
    }
  } else {
    call_success = false;
  }

  if (call_success) {
    // Call the promise resolve function
    JSValue resolve_args[] = {result};
    JS_Call(async_call->ctx, async_call->promise_resolve, JS_UNDEFINED, 1, resolve_args);
  } else {
    // Call the promise reject function
    JSValue error = JS_NewString(async_call->ctx, "Native function call failed");
    JSValue reject_args[] = {error};
    JS_Call(async_call->ctx, async_call->promise_reject, JS_UNDEFINED, 1, reject_args);
    JS_FreeValue(async_call->ctx, error);
  }

  // Clean up
  if (async_call->args)
    free(async_call->args);
  if (async_call->string_args) {
    for (int i = 0; i < async_call->arg_count; i++) {
      if (async_call->string_args[i]) {
        JS_FreeCString(async_call->ctx, async_call->string_args[i]);
      }
    }
    free(async_call->string_args);
  }
  if (async_call->array_args) {
    for (int i = 0; i < async_call->arg_count; i++) {
      if (async_call->array_args[i]) {
        free(async_call->array_args[i]);
      }
    }
    free(async_call->array_args);
  }

  JS_FreeValue(async_call->ctx, async_call->promise_resolve);
  JS_FreeValue(async_call->ctx, async_call->promise_reject);
  JS_FreeValue(async_call->ctx, result);
  free(async_call);

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

// Async function call - ffi.callAsync(funcPtr, returnType, argTypes, args)
static JSValue ffi_call_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 4) {
    return create_ffi_error(ctx, "Expected 4 arguments: function pointer, return type, argument types, arguments",
                            "ffi.callAsync");
  }

  // Parse function pointer
  int64_t func_ptr_val;
  if (JS_ToInt64(ctx, &func_ptr_val, argv[0]) < 0) {
    return create_ffi_error(ctx, "Function pointer must be a number", "ffi.callAsync");
  }
  void* func_ptr = (void*)(intptr_t)func_ptr_val;

  // Parse return type
  const char* return_type_str = JS_ToCString(ctx, argv[1]);
  if (!return_type_str) {
    return create_ffi_error(ctx, "Return type must be a string", "ffi.callAsync");
  }
  jsrt_ffi_type_t return_type = string_to_ffi_type(return_type_str);

  // Parse argument types
  if (!JS_IsArray(ctx, argv[2])) {
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Argument types must be an array", "ffi.callAsync");
  }

  // Parse arguments
  if (!JS_IsArray(ctx, argv[3])) {
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Arguments must be an array", "ffi.callAsync");
  }

  // Get argument count
  JSValue length_val = JS_GetPropertyStr(ctx, argv[3], "length");
  uint32_t arg_count;
  if (JS_ToUint32(ctx, &arg_count, length_val) < 0) {
    JS_FreeValue(ctx, length_val);
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Invalid arguments array", "ffi.callAsync");
  }
  JS_FreeValue(ctx, length_val);

  if (arg_count > 4) {
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Maximum 4 arguments supported for async calls", "ffi.callAsync");
  }

  // Create async call structure
  ffi_async_call_t* async_call = malloc(sizeof(ffi_async_call_t));
  if (!async_call) {
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Failed to allocate async call structure", "ffi.callAsync");
  }

  async_call->ctx = ctx;
  async_call->func_ptr = func_ptr;
  async_call->return_type = return_type;
  async_call->arg_count = arg_count;
  async_call->args = arg_count > 0 ? malloc(arg_count * sizeof(uintptr_t)) : NULL;
  async_call->string_args = arg_count > 0 ? malloc(arg_count * sizeof(char*)) : NULL;
  async_call->array_args = arg_count > 0 ? malloc(arg_count * sizeof(void*)) : NULL;

  // Initialize arrays
  if (async_call->string_args) {
    memset(async_call->string_args, 0, arg_count * sizeof(char*));
  }
  if (async_call->array_args) {
    memset(async_call->array_args, 0, arg_count * sizeof(void*));
  }

  // Convert arguments
  for (uint32_t i = 0; i < arg_count; i++) {
    JSValue arg_val = JS_GetPropertyUint32(ctx, argv[3], i);

    if (JS_IsString(arg_val)) {
      const char* str_val = JS_ToCString(ctx, arg_val);
      async_call->string_args[i] = (char*)str_val;  // Cast away const for storage
      async_call->args[i] = (uintptr_t)async_call->string_args[i];
    } else if (JS_IsNumber(arg_val)) {
      int64_t num_val;
      JS_ToInt64(ctx, &num_val, arg_val);
      async_call->args[i] = (uintptr_t)num_val;
    } else {
      async_call->args[i] = 0;
    }

    JS_FreeValue(ctx, arg_val);
  }

  // Create promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  async_call->promise_resolve = resolving_funcs[0];
  async_call->promise_reject = resolving_funcs[1];

  // Start thread for async execution
#ifdef _WIN32
  HANDLE thread_handle = CreateThread(NULL, 0, async_call_thread, async_call, 0, NULL);
  if (!thread_handle) {
    // Cleanup on failure
    free(async_call->args);
    free(async_call->string_args);
    free(async_call->array_args);
    free(async_call);
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Failed to create thread for async call", "ffi.callAsync");
  }
  CloseHandle(thread_handle);
#else
  pthread_t thread;
  if (pthread_create(&thread, NULL, async_call_thread, async_call) != 0) {
    // Cleanup on failure
    free(async_call->args);
    free(async_call->string_args);
    free(async_call->array_args);
    free(async_call);
    JS_FreeCString(ctx, return_type_str);
    return create_ffi_error(ctx, "Failed to create thread for async call", "ffi.callAsync");
  }
  pthread_detach(thread);
#endif

  JS_FreeCString(ctx, return_type_str);
  return promise;
}

// Global struct registry
static ffi_struct_def_t** g_struct_registry = NULL;
static int g_struct_count = 0;
static int g_struct_capacity = 0;

// Register a struct definition
static int register_struct(ffi_struct_def_t* struct_def) {
  if (g_struct_count >= g_struct_capacity) {
    int new_capacity = g_struct_capacity == 0 ? 16 : g_struct_capacity * 2;
    ffi_struct_def_t** new_registry = realloc(g_struct_registry, new_capacity * sizeof(ffi_struct_def_t*));
    if (!new_registry) {
      return -1;
    }
    g_struct_registry = new_registry;
    g_struct_capacity = new_capacity;
  }

  g_struct_registry[g_struct_count] = struct_def;
  return g_struct_count++;
}

// Forward declarations
static ffi_struct_def_t* find_struct(const char* name);
static size_t get_type_size(jsrt_ffi_type_t type);

// Parse complex type descriptor (supports nested types)
// Format examples:
//   "int" - basic type
//   "MyStruct" - nested struct
//   "int[10]" - fixed array
//   "int*" - pointer to int
//   "MyStruct**" - pointer to pointer to MyStruct
//   "MyStruct[5]" - array of structs
static bool parse_complex_type(const char* type_desc, jsrt_field_descriptor_t* field) {
  if (!type_desc || !field)
    return false;

  field->category = JSRT_FIELD_TYPE_PRIMITIVE;
  field->base_type = JSRT_FFI_TYPE_VOID;
  field->nested_struct = NULL;
  field->array_length = 0;
  field->element_type = JSRT_FFI_TYPE_VOID;
  field->element_struct = NULL;
  field->pointer_levels = 0;
  field->pointed_type = JSRT_FFI_TYPE_VOID;
  field->pointed_struct = NULL;

  char* desc_copy = strdup(type_desc);
  char* base_desc = desc_copy;

  // Count pointer levels (*, **, etc.)
  char* ptr_pos = base_desc;
  while ((ptr_pos = strchr(ptr_pos, '*')) != NULL) {
    field->pointer_levels++;
    *ptr_pos = '\0';  // Remove * from string
    ptr_pos++;
  }

  // Check for array notation [N]
  char* bracket_pos = strchr(base_desc, '[');
  if (bracket_pos) {
    *bracket_pos = '\0';
    char* end_bracket = strchr(bracket_pos + 1, ']');
    if (end_bracket) {
      *end_bracket = '\0';
      field->array_length = atoi(bracket_pos + 1);
      field->category = JSRT_FIELD_TYPE_ARRAY;
    }
  }

  // Trim whitespace
  while (*base_desc == ' ')
    base_desc++;
  int len = strlen(base_desc);
  while (len > 0 && base_desc[len - 1] == ' ') {
    base_desc[--len] = '\0';
  }

  // Handle pointers
  if (field->pointer_levels > 0) {
    field->category = JSRT_FIELD_TYPE_POINTER;
  }

  // Check if it's a known struct
  ffi_struct_def_t* found_struct = find_struct(base_desc);
  if (found_struct) {
    if (field->category == JSRT_FIELD_TYPE_ARRAY) {
      field->element_struct = found_struct;
      field->element_type = JSRT_FFI_TYPE_STRUCT;
    } else if (field->category == JSRT_FIELD_TYPE_POINTER) {
      field->pointed_struct = found_struct;
      field->pointed_type = JSRT_FFI_TYPE_STRUCT;
    } else {
      field->category = JSRT_FIELD_TYPE_STRUCT;
      field->nested_struct = found_struct;
      field->base_type = JSRT_FFI_TYPE_STRUCT;
    }
  } else {
    // Parse as primitive type
    jsrt_ffi_type_t prim_type = string_to_ffi_type(base_desc);
    if (field->category == JSRT_FIELD_TYPE_ARRAY) {
      field->element_type = prim_type;
    } else if (field->category == JSRT_FIELD_TYPE_POINTER) {
      field->pointed_type = prim_type;
    } else {
      field->base_type = prim_type;
    }
  }

  free(desc_copy);
  return true;
}

// Calculate size with complex type support
static size_t calculate_complex_field_size(const jsrt_field_descriptor_t* field) {
  switch (field->category) {
    case JSRT_FIELD_TYPE_PRIMITIVE:
      return get_type_size(field->base_type);

    case JSRT_FIELD_TYPE_STRUCT:
      return field->nested_struct ? field->nested_struct->total_size : 0;

    case JSRT_FIELD_TYPE_ARRAY: {
      size_t element_size = 0;
      if (field->element_struct) {
        element_size = field->element_struct->total_size;
      } else {
        element_size = get_type_size(field->element_type);
      }
      return element_size * field->array_length;
    }

    case JSRT_FIELD_TYPE_POINTER:
      return sizeof(void*);  // All pointers have same size

    default:
      return 0;
  }
}

// Find struct definition by name
static ffi_struct_def_t* find_struct(const char* name) {
  for (int i = 0; i < g_struct_count; i++) {
    if (g_struct_registry[i] && g_struct_registry[i]->name && strcmp(g_struct_registry[i]->name, name) == 0) {
      return g_struct_registry[i];
    }
  }
  return NULL;
}

// Calculate size and alignment for basic types
static size_t get_type_size(jsrt_ffi_type_t type) {
  switch (type) {
    case JSRT_FFI_TYPE_INT:
    case JSRT_FFI_TYPE_INT32:
    case JSRT_FFI_TYPE_UINT32:
    case JSRT_FFI_TYPE_FLOAT:
      return 4;
    case JSRT_FFI_TYPE_INT64:
    case JSRT_FFI_TYPE_UINT64:
    case JSRT_FFI_TYPE_DOUBLE:
    case JSRT_FFI_TYPE_POINTER:
    case JSRT_FFI_TYPE_STRING:
      return 8;
    default:
      return 1;
  }
}

// Calculate struct alignment
static size_t calculate_aligned_offset(size_t current_offset, size_t alignment) {
  return (current_offset + alignment - 1) & ~(alignment - 1);
}

// Define struct - ffi.Struct(name, fields)
static JSValue ffi_struct(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return create_ffi_error(ctx, "Expected 2 arguments: struct name and field definitions", "ffi.Struct");
  }

  // Check if first argument is a string
  if (!JS_IsString(argv[0])) {
    return create_ffi_error(ctx, "Struct name must be a string", "ffi.Struct");
  }

  const char* struct_name = JS_ToCString(ctx, argv[0]);
  if (!struct_name) {
    return create_ffi_error(ctx, "Struct name must be a string", "ffi.Struct");
  }

  if (!JS_IsObject(argv[1])) {
    JS_FreeCString(ctx, struct_name);
    return create_ffi_error(ctx, "Field definitions must be an object", "ffi.Struct");
  }

  // Check if struct already exists
  if (find_struct(struct_name)) {
    JS_FreeCString(ctx, struct_name);
    return create_ffi_error(ctx, "Struct with this name already exists", "ffi.Struct");
  }

  // Get field names
  JSPropertyEnum* tab;
  uint32_t tab_len;
  if (JS_GetOwnPropertyNames(ctx, &tab, &tab_len, argv[1], JS_GPN_STRING_MASK) < 0) {
    JS_FreeCString(ctx, struct_name);
    return create_ffi_error(ctx, "Failed to get field names", "ffi.Struct");
  }

  // Create struct definition
  ffi_struct_def_t* struct_def = malloc(sizeof(ffi_struct_def_t));
  if (!struct_def) {
    js_free(ctx, tab);
    JS_FreeCString(ctx, struct_name);
    return create_ffi_error(ctx, "Failed to allocate struct definition", "ffi.Struct");
  }

  struct_def->name = strdup(struct_name);
  struct_def->field_count = tab_len;
  struct_def->fields = malloc(tab_len * sizeof(jsrt_field_descriptor_t));
  struct_def->alignment = 8;  // Default alignment
  struct_def->dependency_count = 0;
  struct_def->dependencies = NULL;

  if (!struct_def->fields) {
    free(struct_def->name);
    free(struct_def);
    js_free(ctx, tab);
    JS_FreeCString(ctx, struct_name);
    return create_ffi_error(ctx, "Failed to allocate struct field descriptors", "ffi.Struct");
  }

  // Calculate field offsets and total size with enhanced type support
  size_t current_offset = 0;
  for (uint32_t i = 0; i < tab_len; i++) {
    const char* field_name = JS_AtomToCString(ctx, tab[i].atom);
    struct_def->fields[i].name = strdup(field_name);

    JSValue field_type_val = JS_GetProperty(ctx, argv[1], tab[i].atom);
    const char* field_type_str = JS_ToCString(ctx, field_type_val);

    if (field_type_str) {
      // Parse complex type descriptor
      if (!parse_complex_type(field_type_str, &struct_def->fields[i])) {
        // Default to void on parse failure
        struct_def->fields[i].base_type = JSRT_FFI_TYPE_VOID;
        struct_def->fields[i].category = JSRT_FIELD_TYPE_PRIMITIVE;
      }

      // Calculate alignment and offset
      size_t field_size = calculate_complex_field_size(&struct_def->fields[i]);
      size_t field_alignment = field_size > 8 ? 8 : field_size;  // Max 8-byte alignment

      current_offset = calculate_aligned_offset(current_offset, field_alignment);
      struct_def->fields[i].offset = current_offset;
      current_offset += field_size;

      JS_FreeCString(ctx, field_type_str);
    } else {
      // Default field setup
      struct_def->fields[i].base_type = JSRT_FFI_TYPE_VOID;
      struct_def->fields[i].category = JSRT_FIELD_TYPE_PRIMITIVE;
      struct_def->fields[i].offset = current_offset;
    }

    JS_FreeCString(ctx, field_name);
    JS_FreeValue(ctx, field_type_val);
  }

  // Align total size to largest field alignment (simplified)
  struct_def->total_size = calculate_aligned_offset(current_offset, 8);

  // Register struct
  int struct_id = register_struct(struct_def);
  if (struct_id < 0) {
    // Cleanup on failure
    for (uint32_t i = 0; i < tab_len; i++) {
      free(struct_def->fields[i].name);
    }
    free(struct_def->fields);
    free(struct_def->name);
    free(struct_def);
    js_free(ctx, tab);
    JS_FreeCString(ctx, struct_name);
    return create_ffi_error(ctx, "Failed to register struct", "ffi.Struct");
  }

  js_free(ctx, tab);
  JS_FreeCString(ctx, struct_name);

  // Return struct information object
  JSValue struct_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, struct_obj, "name", JS_NewString(ctx, struct_def->name));
  JS_SetPropertyStr(ctx, struct_obj, "size", JS_NewUint32(ctx, (uint32_t)struct_def->total_size));
  JS_SetPropertyStr(ctx, struct_obj, "fieldCount", JS_NewUint32(ctx, struct_def->field_count));
  JS_SetPropertyStr(ctx, struct_obj, "id", JS_NewInt32(ctx, struct_id));

  return struct_obj;
}

// Allocate struct - ffi.allocStruct(structName)
static JSValue ffi_alloc_struct(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return create_ffi_error(ctx, "Expected 1 argument: struct name", "ffi.allocStruct");
  }

  // Check if first argument is a string
  if (!JS_IsString(argv[0])) {
    return create_ffi_error(ctx, "Struct name must be a string", "ffi.allocStruct");
  }

  const char* struct_name = JS_ToCString(ctx, argv[0]);
  if (!struct_name) {
    return create_ffi_error(ctx, "Struct name must be a string", "ffi.allocStruct");
  }

  ffi_struct_def_t* struct_def = find_struct(struct_name);
  if (!struct_def) {
    JS_FreeCString(ctx, struct_name);
    return create_ffi_error(ctx, "Struct not found", "ffi.allocStruct");
  }

  // Allocate memory for struct
  void* struct_ptr = malloc(struct_def->total_size);
  if (!struct_ptr) {
    JS_FreeCString(ctx, struct_name);
    return create_ffi_error(ctx, "Failed to allocate struct memory", "ffi.allocStruct");
  }

  // Initialize to zero
  memset(struct_ptr, 0, struct_def->total_size);

  JS_FreeCString(ctx, struct_name);

  // Return pointer to allocated struct
  return JS_NewInt64(ctx, (intptr_t)struct_ptr);
}

// Create FFI module for require("jsrt:ffi")
JSValue JSRT_CreateFFIModule(JSContext* ctx) {
  JSValue ffi_obj = JS_NewObject(ctx);

  // Core FFI function
  JS_SetPropertyStr(ctx, ffi_obj, "Library", JS_NewCFunction(ctx, ffi_library, "Library", 2));

  // Memory management functions
  JS_SetPropertyStr(ctx, ffi_obj, "malloc", JS_NewCFunction(ctx, ffi_malloc, "malloc", 1));
  JS_SetPropertyStr(ctx, ffi_obj, "free", JS_NewCFunction(ctx, ffi_free, "free", 1));
  JS_SetPropertyStr(ctx, ffi_obj, "memcpy", JS_NewCFunction(ctx, ffi_memcpy, "memcpy", 3));
  JS_SetPropertyStr(ctx, ffi_obj, "readString", JS_NewCFunction(ctx, ffi_read_string, "readString", 2));
  JS_SetPropertyStr(ctx, ffi_obj, "writeString", JS_NewCFunction(ctx, ffi_write_string, "writeString", 2));

  // Array manipulation functions
  JS_SetPropertyStr(ctx, ffi_obj, "arrayFromPointer",
                    JS_NewCFunction(ctx, ffi_array_from_pointer, "arrayFromPointer", 3));
  JS_SetPropertyStr(ctx, ffi_obj, "arrayLength", JS_NewCFunction(ctx, ffi_array_length, "arrayLength", 1));

  // Callback support
  JS_SetPropertyStr(ctx, ffi_obj, "Callback", JS_NewCFunction(ctx, ffi_callback, "Callback", 3));

  // Async function support
  JS_SetPropertyStr(ctx, ffi_obj, "callAsync", JS_NewCFunction(ctx, ffi_call_async, "callAsync", 4));

  // Struct support
  JS_SetPropertyStr(ctx, ffi_obj, "Struct", JS_NewCFunction(ctx, ffi_struct, "Struct", 2));
  JS_SetPropertyStr(ctx, ffi_obj, "allocStruct", JS_NewCFunction(ctx, ffi_alloc_struct, "allocStruct", 1));

  // Add version information
  JS_SetPropertyStr(ctx, ffi_obj, "version", JS_NewString(ctx, "3.0.0"));

  // Add type constants for convenience
  JSValue types_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, types_obj, "void", JS_NewString(ctx, "void"));
  JS_SetPropertyStr(ctx, types_obj, "int", JS_NewString(ctx, "int"));
  JS_SetPropertyStr(ctx, types_obj, "int32", JS_NewString(ctx, "int32"));
  JS_SetPropertyStr(ctx, types_obj, "int64", JS_NewString(ctx, "int64"));
  JS_SetPropertyStr(ctx, types_obj, "uint", JS_NewString(ctx, "uint"));
  JS_SetPropertyStr(ctx, types_obj, "uint32", JS_NewString(ctx, "uint32"));
  JS_SetPropertyStr(ctx, types_obj, "uint64", JS_NewString(ctx, "uint64"));
  JS_SetPropertyStr(ctx, types_obj, "float", JS_NewString(ctx, "float"));
  JS_SetPropertyStr(ctx, types_obj, "double", JS_NewString(ctx, "double"));
  JS_SetPropertyStr(ctx, types_obj, "string", JS_NewString(ctx, "string"));
  JS_SetPropertyStr(ctx, types_obj, "pointer", JS_NewString(ctx, "pointer"));
  JS_SetPropertyStr(ctx, types_obj, "array", JS_NewString(ctx, "array"));
  JS_SetPropertyStr(ctx, types_obj, "callback", JS_NewString(ctx, "callback"));
  JS_SetPropertyStr(ctx, types_obj, "struct", JS_NewString(ctx, "struct"));
  JS_SetPropertyStr(ctx, ffi_obj, "types", types_obj);

  JSRT_Debug(
      "FFI: Created enhanced FFI module v3.0.0 with callback support, async functions, struct support, array support "
      "and enhanced error reporting");

  return ffi_obj;
}

// Cleanup FFI module
void JSRT_RuntimeCleanupStdFFI(JSContext* ctx) {
  // Clean up global ffi_functions_map if it was initialized
  if (ffi_initialized) {
    JSRT_Debug("FFI: Cleaning up global ffi_functions_map");
    JS_FreeValue(ctx, ffi_functions_map);
    ffi_initialized = false;
  }

  // Reset function ID counter
  next_function_id = 1;

  JSRT_Debug("FFI: FFI module cleanup completed");
}

// Initialize FFI module
void JSRT_RuntimeSetupStdFFI(JSRT_Runtime* rt) {
  // Create class IDs
  JS_NewClassID(&JSRT_FFILibraryClassID);
  JS_NewClassID(&JSRT_FFIFunctionClassID);

  // Register classes
  JS_NewClass(rt->rt, JSRT_FFILibraryClassID, &JSRT_FFILibraryClass);
  JS_NewClass(rt->rt, JSRT_FFIFunctionClassID, &JSRT_FFIFunctionClass);

  JSRT_Debug("FFI: Initialized FFI module");
}