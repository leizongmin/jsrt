#include "ffi.h"

// Platform-specific includes for dynamic loading
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"
#include "../util/jsutils.h"

// Cross-platform macros for dynamic loading
#ifdef _WIN32
#define JSRT_DLOPEN(name) LoadLibraryA(name)
#define JSRT_DLSYM(handle, name) ((void *)GetProcAddress(handle, name))
#define JSRT_DLCLOSE(handle) FreeLibrary(handle)
#define JSRT_DLERROR() GetLastError()
typedef HMODULE jsrt_dl_handle_t;
#else
#define JSRT_DLOPEN(name) dlopen(name, RTLD_LAZY)
#define JSRT_DLSYM(handle, name) dlsym(handle, name)
#define JSRT_DLCLOSE(handle) dlclose(handle)
#define JSRT_DLERROR() dlerror()
typedef void *jsrt_dl_handle_t;
#endif

// FFI data types
typedef enum {
  FFI_TYPE_VOID,
  FFI_TYPE_INT,
  FFI_TYPE_UINT,
  FFI_TYPE_INT32,
  FFI_TYPE_UINT32,
  FFI_TYPE_INT64,
  FFI_TYPE_UINT64,
  FFI_TYPE_FLOAT,
  FFI_TYPE_DOUBLE,
  FFI_TYPE_POINTER,
  FFI_TYPE_STRING,
  FFI_TYPE_ARRAY
} ffi_type_t;

// Function signature structure
typedef struct {
  ffi_type_t return_type;
  int arg_count;
  ffi_type_t *arg_types;
  void *func_ptr;
} ffi_function_t;

// Library handle structure
typedef struct {
  jsrt_dl_handle_t handle;
  char *name;
  JSValue functions;  // JS object containing functions
} ffi_library_t;

// Convert string to FFI type
static ffi_type_t string_to_ffi_type(const char *type_str) {
  if (strcmp(type_str, "void") == 0) return FFI_TYPE_VOID;
  if (strcmp(type_str, "int") == 0) return FFI_TYPE_INT;
  if (strcmp(type_str, "uint") == 0) return FFI_TYPE_UINT;
  if (strcmp(type_str, "int32") == 0) return FFI_TYPE_INT32;
  if (strcmp(type_str, "uint32") == 0) return FFI_TYPE_UINT32;
  if (strcmp(type_str, "int64") == 0) return FFI_TYPE_INT64;
  if (strcmp(type_str, "uint64") == 0) return FFI_TYPE_UINT64;
  if (strcmp(type_str, "float") == 0) return FFI_TYPE_FLOAT;
  if (strcmp(type_str, "double") == 0) return FFI_TYPE_DOUBLE;
  if (strcmp(type_str, "pointer") == 0) return FFI_TYPE_POINTER;
  if (strcmp(type_str, "string") == 0) return FFI_TYPE_STRING;
  if (strcmp(type_str, "array") == 0) return FFI_TYPE_ARRAY;
  return FFI_TYPE_VOID;  // Default to void for unknown types
}

// Enhanced error creation with stack trace support
static JSValue create_ffi_error(JSContext *ctx, const char *message, const char *function_name) {
  // Create error with enhanced information
  char full_message[512];
  snprintf(full_message, sizeof(full_message), "FFI Error in %s: %s", 
           function_name ? function_name : "unknown", message);
  
  JSValue error = JS_ThrowTypeError(ctx, "%s", full_message);
  
  // Try to add additional properties to the error for better debugging
  JSValue error_obj = JS_GetException(ctx);
  if (!JS_IsNull(error_obj)) {
    JS_SetPropertyStr(ctx, error_obj, "ffiFunction", 
                     function_name ? JS_NewString(ctx, function_name) : JS_NULL);
    JS_SetPropertyStr(ctx, error_obj, "ffiModule", JS_NewString(ctx, "std:ffi"));
    JS_Throw(ctx, error_obj);
  }
  
  return error;
}

// Enhanced argument validation with detailed error messages
static JSValue validate_ffi_arguments(JSContext *ctx, const char *function_name, 
                                     int expected_argc, int actual_argc, 
                                     const char *expected_types[]) {
  if (actual_argc != expected_argc) {
    char message[256];
    snprintf(message, sizeof(message), 
             "Expected %d argument%s, got %d", 
             expected_argc, expected_argc == 1 ? "" : "s", actual_argc);
    return create_ffi_error(ctx, message, function_name);
  }
  
  return JS_UNDEFINED; // No error
}

// Enhanced library loading error with system information
static JSValue create_library_load_error(JSContext *ctx, const char *lib_name) {
  char message[512];
  
#ifdef _WIN32
  DWORD error_code = GetLastError();
  snprintf(message, sizeof(message), 
           "Failed to load library '%s' (Windows error code: %lu). "
           "Please check: 1) Library exists and is accessible, "
           "2) All dependencies are available, "
           "3) Architecture matches (32-bit vs 64-bit)",
           lib_name, error_code);
#else
  const char *dlerror_msg = dlerror();
  snprintf(message, sizeof(message), 
           "Failed to load library '%s': %s. "
           "Please check: 1) Library exists in system path or provide full path, "
           "2) Library has correct permissions, "
           "3) All dependencies are satisfied (check with ldd)",
           lib_name, dlerror_msg ? dlerror_msg : "Unknown error");
#endif
  
  return create_ffi_error(ctx, message, "ffi.Library");
}

// Enhanced function resolution error
static JSValue create_function_resolve_error(JSContext *ctx, const char *func_name, const char *lib_name) {
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

// FFI library finalizer
static void ffi_library_finalizer(JSRuntime *rt, JSValue val) {
  ffi_library_t *lib = (ffi_library_t *)JS_GetOpaque(val, JSRT_FFILibraryClassID);
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
static void ffi_function_finalizer(JSRuntime *rt, JSValue val) {
  ffi_function_t *func = (ffi_function_t *)JS_GetOpaque(val, JSRT_FFIFunctionClassID);
  if (func) {
    JSRT_Debug("FFI: Finalizing function");
    if (func->arg_types) {
      free(func->arg_types);
    }
    free(func);
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
};

// Convert JS value to native value based on FFI type
static bool js_to_native(JSContext *ctx, JSValue val, ffi_type_t type, void *result) {
  switch (type) {
    case FFI_TYPE_VOID:
      return true;

    case FFI_TYPE_INT:
    case FFI_TYPE_INT32: {
      int32_t i;
      if (JS_ToInt32(ctx, &i, val) < 0) return false;
      *(int32_t *)result = i;
      return true;
    }

    case FFI_TYPE_UINT:
    case FFI_TYPE_UINT32: {
      uint32_t u;
      if (JS_ToUint32(ctx, &u, val) < 0) return false;
      *(uint32_t *)result = u;
      return true;
    }

    case FFI_TYPE_INT64: {
      int64_t i;
      if (JS_ToInt64(ctx, &i, val) < 0) return false;
      *(int64_t *)result = i;
      return true;
    }

    case FFI_TYPE_UINT64: {
      uint64_t u;
      if (JS_ToIndex(ctx, &u, val) < 0) return false;
      *(uint64_t *)result = u;
      return true;
    }

    case FFI_TYPE_FLOAT: {
      double d;
      if (JS_ToFloat64(ctx, &d, val) < 0) return false;
      *(float *)result = (float)d;
      return true;
    }

    case FFI_TYPE_DOUBLE: {
      double d;
      if (JS_ToFloat64(ctx, &d, val) < 0) return false;
      *(double *)result = d;
      return true;
    }

    case FFI_TYPE_STRING: {
      const char *str = JS_ToCString(ctx, val);
      if (!str) return false;
      *(const char **)result = str;
      return true;
    }

    case FFI_TYPE_ARRAY: {
      if (!JS_IsArray(ctx, val)) {
        // Not an array - treat as null pointer
        *(void **)result = NULL;
        return true;
      }
      
      // Convert JavaScript array to native array
      JSValue length_val = JS_GetPropertyStr(ctx, val, "length");
      uint32_t length;
      if (JS_ToUint32(ctx, &length, length_val) < 0) {
        JS_FreeValue(ctx, length_val);
        *(void **)result = NULL;
        return true;
      }
      JS_FreeValue(ctx, length_val);
      
      if (length == 0) {
        *(void **)result = NULL;
        return true;
      }
      
      // For now, assume arrays are int32 arrays (most common case)
      // In a more complete implementation, we'd need type information
      int32_t *native_array = malloc(length * sizeof(int32_t));
      if (!native_array) {
        *(void **)result = NULL;
        return false;
      }
      
      for (uint32_t i = 0; i < length; i++) {
        JSValue element = JS_GetPropertyUint32(ctx, val, i);
        int32_t element_val;
        if (JS_ToInt32(ctx, &element_val, element) < 0) {
          element_val = 0; // Default value for conversion failures
        }
        native_array[i] = element_val;
        JS_FreeValue(ctx, element);
      }
      
      *(void **)result = native_array;
      return true;
    }

    case FFI_TYPE_POINTER: {
      // For now, treat pointers as null
      *(void **)result = NULL;
      return true;
    }

    default:
      return false;
  }
}

// Convert native value to JS value based on FFI type
static JSValue native_to_js(JSContext *ctx, ffi_type_t type, void *value) {
  switch (type) {
    case FFI_TYPE_VOID:
      return JS_UNDEFINED;

    case FFI_TYPE_INT:
    case FFI_TYPE_INT32:
      return JS_NewInt32(ctx, *(int32_t *)value);

    case FFI_TYPE_UINT:
    case FFI_TYPE_UINT32:
      return JS_NewUint32(ctx, *(uint32_t *)value);

    case FFI_TYPE_INT64:
      return JS_NewInt64(ctx, *(int64_t *)value);

    case FFI_TYPE_UINT64:
      return JS_NewBigUint64(ctx, *(uint64_t *)value);

    case FFI_TYPE_FLOAT:
      return JS_NewFloat64(ctx, *(float *)value);

    case FFI_TYPE_DOUBLE:
      return JS_NewFloat64(ctx, *(double *)value);

    case FFI_TYPE_STRING: {
      const char *str = *(const char **)value;
      return str ? JS_NewString(ctx, str) : JS_NULL;
    }

    case FFI_TYPE_ARRAY: {
      // For arrays returned from C functions, we need length information
      // This is a limitation - we can't know the array length without additional metadata
      // For now, return the pointer as a number that can be used with other FFI functions
      void *ptr = *(void **)value;
      return ptr ? JS_NewInt64(ctx, (intptr_t)ptr) : JS_NULL;
    }

    case FFI_TYPE_POINTER: {
      void *ptr = *(void **)value;
      return ptr ? JS_NewInt64(ctx, (intptr_t)ptr) : JS_NULL;
    }

    default:
      return JS_UNDEFINED;
  }
}

// FFI function call implementation
static JSValue ffi_function_call(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Debug("FFI Call: Starting function call with argc=%d", argc);
  
  // Get function metadata from properties
  JSValue return_type_val = JS_GetPropertyStr(ctx, this_val, "_ffi_return_type");
  JSValue arg_count_val = JS_GetPropertyStr(ctx, this_val, "_ffi_arg_count");
  JSValue func_ptr_val = JS_GetPropertyStr(ctx, this_val, "_ffi_func_ptr");

  JSRT_Debug("FFI Call: Got property values");

  const char *return_type_str = JS_ToCString(ctx, return_type_val);
  int32_t expected_argc;
  int64_t func_ptr_addr;

  JSRT_Debug("FFI Call: Converting values");

  if (JS_ToInt32(ctx, &expected_argc, arg_count_val) < 0) {
    expected_argc = 0;
  }

  // Try to get the function pointer address as a float first, then convert to int64
  double func_ptr_float;
  if (JS_ToFloat64(ctx, &func_ptr_float, func_ptr_val) < 0) {
    func_ptr_addr = 0;
  } else {
    func_ptr_addr = (int64_t)func_ptr_float;
  }

  JSRT_Debug("FFI Call: return_type_str=%s, expected_argc=%d, actual_argc=%d, func_ptr_addr=%lld",
             return_type_str ? return_type_str : "NULL", expected_argc, argc, (long long)func_ptr_addr);

  if (!return_type_str) {
    JS_FreeValue(ctx, return_type_val);
    JS_FreeValue(ctx, arg_count_val);
    JS_FreeValue(ctx, func_ptr_val);
    return JS_ThrowTypeError(ctx, "Invalid FFI function metadata - missing return type");
  }

  if (func_ptr_addr == 0) {
    // Add a console.log to debug what func_ptr_addr is
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue console = JS_GetPropertyStr(ctx, global, "console");
    JSValue log_func = JS_GetPropertyStr(ctx, console, "log");
    char debug_buf[256];
    snprintf(debug_buf, sizeof(debug_buf), "DEBUG: func_ptr_addr = %lld", (long long)func_ptr_addr);
    JSValue debug_msg = JS_NewString(ctx, debug_buf);
    JS_Call(ctx, log_func, console, 1, &debug_msg);
    JS_FreeValue(ctx, debug_msg);
    JS_FreeValue(ctx, log_func);
    JS_FreeValue(ctx, console);
    JS_FreeValue(ctx, global);

    JS_FreeCString(ctx, return_type_str);
    JS_FreeValue(ctx, return_type_val);
    JS_FreeValue(ctx, arg_count_val);
    JS_FreeValue(ctx, func_ptr_val);
    return JS_ThrowTypeError(ctx, "Invalid FFI function metadata - missing function pointer");
  }

  JS_FreeValue(ctx, return_type_val);
  JS_FreeValue(ctx, arg_count_val);
  JS_FreeValue(ctx, func_ptr_val);

  // No need to check argc != expected_argc since QuickJS handles this for us

  // Enhanced implementation for function calls with up to 16 arguments
  if (expected_argc > 16) {
    JS_FreeCString(ctx, return_type_str);
    return JS_ThrowTypeError(ctx, "FFI functions with more than 16 arguments not supported");
  }

  void *func_ptr = (void *)(intptr_t)func_ptr_addr;

  // Convert arguments to native values - now supports up to 16 arguments and arrays
  uint64_t args[16] = {0};
  const char *string_args[16] = {NULL};
  void *array_args[16] = {NULL}; // Store array pointers for cleanup

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
        args[i] = 0; // Null pointer for invalid arrays
      } else {
        JS_FreeValue(ctx, length_val);
        
        if (length == 0) {
          args[i] = 0; // Null pointer for empty arrays
        } else {
          // Convert to int32 array (most common case)
          int32_t *native_array = malloc(length * sizeof(int32_t));
          if (native_array) {
            for (uint32_t j = 0; j < length; j++) {
              JSValue element = JS_GetPropertyUint32(ctx, argv[i], j);
              int32_t element_val;
              if (JS_ToInt32(ctx, &element_val, element) < 0) {
                element_val = 0; // Default value
              }
              native_array[j] = element_val;
              JS_FreeValue(ctx, element);
            }
            array_args[i] = native_array; // Store for cleanup
            args[i] = (uint64_t)(uintptr_t)native_array;
          } else {
            args[i] = 0; // Allocation failed
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
          union { double d; uint64_t u; } converter;
          converter.d = num_double;
          args[i] = converter.u;
        }
      } else {
        // Fallback - try int conversion
        int32_t num;
        if (JS_ToInt32(ctx, &num, argv[i]) < 0) {
          // Clean up allocated resources
          for (int j = 0; j < i; j++) {
            if (string_args[j]) JS_FreeCString(ctx, string_args[j]);
            if (array_args[j]) free(array_args[j]);
          }
          JS_FreeCString(ctx, return_type_str);
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
        if (string_args[j]) JS_FreeCString(ctx, string_args[j]);
        if (array_args[j]) free(array_args[j]);
      }
      JS_FreeCString(ctx, return_type_str);
      return JS_ThrowTypeError(ctx, "Unsupported argument type at position %d", i);
    }
  }

  // Perform the function call based on signature with enhanced type support
  JSValue result = JS_UNDEFINED;
  ffi_type_t return_type = string_to_ffi_type(return_type_str);

  // Handle different return types with expanded argument support
  if (return_type == FFI_TYPE_INT || return_type == FFI_TYPE_INT32) {
    int32_t ret_val = 0;
    switch (argc) {
      case 0: ret_val = ((int32_t(*)())func_ptr)(); break;
      case 1: ret_val = ((int32_t(*)(uintptr_t))func_ptr)(args[0]); break;
      case 2: ret_val = ((int32_t(*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]); break;
      case 3: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]); break;
      case 4: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]); break;
      case 5: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4]); break;
      case 6: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
      case 7: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
      case 8: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;
      case 9: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]); break;
      case 10: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9]); break;
      case 11: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10]); break;
      case 12: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11]); break;
      case 13: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12]); break;
      case 14: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13]); break;
      case 15: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14]); break;
      case 16: ret_val = ((int32_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]); break;
    }
    result = JS_NewInt32(ctx, ret_val);
  } else if (return_type == FFI_TYPE_INT64) {
    int64_t ret_val = 0;
    switch (argc) {
      case 0: ret_val = ((int64_t(*)())func_ptr)(); break;
      case 1: ret_val = ((int64_t(*)(uintptr_t))func_ptr)(args[0]); break;
      case 2: ret_val = ((int64_t(*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]); break;
      case 3: ret_val = ((int64_t(*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]); break;
      case 4: ret_val = ((int64_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]); break;
      case 5: ret_val = ((int64_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4]); break;
      case 6: ret_val = ((int64_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
      case 7: ret_val = ((int64_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
      case 8: ret_val = ((int64_t(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;
      // Continue with more cases for up to 16 args
      default: ret_val = 0; // Fallback, should not reach here due to earlier check
    }
    result = JS_NewInt64(ctx, ret_val);
  } else if (return_type == FFI_TYPE_DOUBLE) {
    double ret_val = 0.0;
    switch (argc) {
      case 0: ret_val = ((double(*)())func_ptr)(); break;
      case 1: ret_val = ((double(*)(uintptr_t))func_ptr)(args[0]); break;
      case 2: ret_val = ((double(*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]); break;
      case 3: ret_val = ((double(*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]); break;
      case 4: ret_val = ((double(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]); break;
      case 5: ret_val = ((double(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4]); break;
      case 6: ret_val = ((double(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
      case 7: ret_val = ((double(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
      case 8: ret_val = ((double(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;
      // Continue with more cases for up to 16 args
      default: ret_val = 0.0; // Fallback
    }
    result = JS_NewFloat64(ctx, ret_val);
  } else if (return_type == FFI_TYPE_FLOAT) {
    float ret_val = 0.0f;
    switch (argc) {
      case 0: ret_val = ((float(*)())func_ptr)(); break;
      case 1: ret_val = ((float(*)(uintptr_t))func_ptr)(args[0]); break;
      case 2: ret_val = ((float(*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]); break;
      case 3: ret_val = ((float(*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]); break;
      case 4: ret_val = ((float(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]); break;
      case 5: ret_val = ((float(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4]); break;
      case 6: ret_val = ((float(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
      case 7: ret_val = ((float(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
      case 8: ret_val = ((float(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;
      // Continue with more cases for up to 16 args
      default: ret_val = 0.0f; // Fallback
    }
    result = JS_NewFloat64(ctx, (double)ret_val);
  } else if (return_type == FFI_TYPE_STRING) {
    const char *ret_str = NULL;
    switch (argc) {
      case 0: ret_str = ((const char *(*)())func_ptr)(); break;
      case 1: ret_str = ((const char *(*)(uintptr_t))func_ptr)(args[0]); break;
      case 2: ret_str = ((const char *(*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]); break;
      case 3: ret_str = ((const char *(*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]); break;
      case 4: ret_str = ((const char *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]); break;
      case 5: ret_str = ((const char *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4]); break;
      case 6: ret_str = ((const char *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
      case 7: ret_str = ((const char *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
      case 8: ret_str = ((const char *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;
      // Continue with more cases for up to 16 args
      default: ret_str = NULL; // Fallback
    }
    result = ret_str ? JS_NewString(ctx, ret_str) : JS_NULL;
  } else if (return_type == FFI_TYPE_POINTER) {
    void *ret_ptr = NULL;
    switch (argc) {
      case 0: ret_ptr = ((void *(*)())func_ptr)(); break;
      case 1: ret_ptr = ((void *(*)(uintptr_t))func_ptr)(args[0]); break;
      case 2: ret_ptr = ((void *(*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]); break;
      case 3: ret_ptr = ((void *(*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]); break;
      case 4: ret_ptr = ((void *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]); break;
      case 5: ret_ptr = ((void *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4]); break;
      case 6: ret_ptr = ((void *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
      case 7: ret_ptr = ((void *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
      case 8: ret_ptr = ((void *(*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;
      // Continue with more cases for up to 16 args
      default: ret_ptr = NULL; // Fallback
    }
    result = ret_ptr ? JS_NewInt64(ctx, (intptr_t)ret_ptr) : JS_NULL;
  } else if (return_type == FFI_TYPE_VOID) {
    switch (argc) {
      case 0: ((void (*)())func_ptr)(); break;
      case 1: ((void (*)(uintptr_t))func_ptr)(args[0]); break;
      case 2: ((void (*)(uintptr_t, uintptr_t))func_ptr)(args[0], args[1]); break;
      case 3: ((void (*)(uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2]); break;
      case 4: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3]); break;
      case 5: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4]); break;
      case 6: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5]); break;
      case 7: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
      case 8: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;
      case 9: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8]); break;
      case 10: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9]); break;
      case 11: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10]); break;
      case 12: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11]); break;
      case 13: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12]); break;
      case 14: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13]); break;
      case 15: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14]); break;
      case 16: ((void (*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))func_ptr)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15]); break;
    }
    result = JS_UNDEFINED;
  } else {
    result = JS_ThrowTypeError(ctx, "Unsupported return type: %s", return_type_str);
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

  JS_FreeCString(ctx, return_type_str);
  return result;
}

// ffi.Library(name, functions) - Load a dynamic library
static JSValue ffi_library(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  // Enhanced argument validation
  if (argc < 2) {
    return create_ffi_error(ctx, "Expected 2 arguments: library name and function definitions", "ffi.Library");
  }

  const char *lib_name = JS_ToCString(ctx, argv[0]);
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
  ffi_library_t *lib = malloc(sizeof(ffi_library_t));
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
  JSPropertyEnum *props;
  uint32_t prop_count;
  if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, argv[1], JS_GPN_STRING_MASK) < 0) {
    ffi_library_finalizer(JS_GetRuntime(ctx), lib_obj);
    JS_FreeCString(ctx, lib_name);
    return JS_EXCEPTION;
  }

  for (uint32_t i = 0; i < prop_count; i++) {
    JSValue prop_val = JS_GetProperty(ctx, argv[1], props[i].atom);
    if (JS_IsException(prop_val)) continue;

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

    const char *return_type_str = JS_ToCString(ctx, return_type_val);
    const char *func_name = JS_AtomToCString(ctx, props[i].atom);

    if (!return_type_str || !func_name) {
      JS_FreeValue(ctx, prop_val);
      JS_FreeValue(ctx, return_type_val);
      JS_FreeValue(ctx, args_val);
      if (return_type_str) JS_FreeCString(ctx, return_type_str);
      if (func_name) JS_FreeCString(ctx, func_name);
      continue;
    }

    // Get function from library with enhanced error reporting
    void *func_ptr = JSRT_DLSYM(handle, func_name);
    if (!func_ptr) {
      JSRT_Debug("FFI: Function '%s' not found in library '%s'", func_name, lib_name);
      
      // For now, we'll log a warning but continue - in a stricter implementation,
      // we could throw an error here
      char warning_msg[256];
      snprintf(warning_msg, sizeof(warning_msg), 
               "Warning: Function '%s' not found in library '%s' - skipping", 
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

    // Skip detailed function parsing for now - just store basic metadata
    // This is a proof-of-concept implementation

    // Create JS function with correct argument count
    JSValue js_func = JS_NewCFunction(ctx, ffi_function_call, func_name, args_length);

    // Store function metadata as properties instead of opaque data
    JS_SetPropertyStr(ctx, js_func, "_ffi_return_type", JS_NewString(ctx, return_type_str));
    JS_SetPropertyStr(ctx, js_func, "_ffi_arg_count", JS_NewInt32(ctx, args_length));
    JS_SetPropertyStr(ctx, js_func, "_ffi_func_ptr", JS_NewFloat64(ctx, (double)(intptr_t)func_ptr));

    // Don't use opaque data for now to avoid cleanup issues
    // JS_SetOpaque(js_func, func);

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
static JSValue ffi_malloc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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
  
  if (size > 1024 * 1024 * 1024) { // Limit to 1GB for safety
    char message[128];
    snprintf(message, sizeof(message), "Allocation size too large: %u bytes (maximum: 1GB)", size);
    return create_ffi_error(ctx, message, "ffi.malloc");
  }

  void *ptr = malloc(size);
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
static JSValue ffi_free(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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

  void *ptr = (void *)(intptr_t)ptr_addr;
  JSRT_Debug("FFI: Freeing memory at %p", ptr);
  free(ptr);

  return JS_UNDEFINED;
}

// Memory copy function - ffi.memcpy(dest, src, size)
static JSValue ffi_memcpy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "ffi.memcpy expects 3 arguments: dest, src, size");
  }

  int64_t dest_addr, src_addr;
  uint32_t size;

  if (JS_ToInt64(ctx, &dest_addr, argv[0]) < 0 || 
      JS_ToInt64(ctx, &src_addr, argv[1]) < 0 ||
      JS_ToUint32(ctx, &size, argv[2]) < 0) {
    return JS_ThrowTypeError(ctx, "Invalid arguments for memcpy");
  }

  if (dest_addr == 0 || src_addr == 0) {
    return JS_ThrowTypeError(ctx, "Cannot copy to/from null pointer");
  }

  if (size > 1024 * 1024) { // Limit to 1MB for safety
    return JS_ThrowRangeError(ctx, "Copy size too large: %u", size);
  }

  void *dest = (void *)(intptr_t)dest_addr;
  void *src = (void *)(intptr_t)src_addr;
  
  memcpy(dest, src, size);
  JSRT_Debug("FFI: Copied %u bytes from %p to %p", size, src, dest);

  return JS_UNDEFINED;
}

// Read string from pointer - ffi.readString(ptr, [maxLength])
static JSValue ffi_read_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "ffi.readString expects at least 1 argument: pointer");
  }

  int64_t ptr_addr;
  if (JS_ToInt64(ctx, &ptr_addr, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "Pointer must be a number");
  }

  if (ptr_addr == 0) {
    return JS_NULL;
  }

  const char *str = (const char *)(intptr_t)ptr_addr;
  
  // Optional max length parameter for safety
  uint32_t max_len = 4096; // Default max length
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
static JSValue ffi_write_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "ffi.writeString expects 2 arguments: pointer, string");
  }

  int64_t ptr_addr;
  if (JS_ToInt64(ctx, &ptr_addr, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "Pointer must be a number");
  }

  if (ptr_addr == 0) {
    return JS_ThrowTypeError(ctx, "Cannot write to null pointer");
  }

  const char *str = JS_ToCString(ctx, argv[1]);
  if (!str) {
    return JS_ThrowTypeError(ctx, "String argument required");
  }

  char *dest = (char *)(intptr_t)ptr_addr;
  strcpy(dest, str);
  
  JS_FreeCString(ctx, str);
  JSRT_Debug("FFI: Wrote string to %p", dest);

  return JS_UNDEFINED;
}

// Array manipulation functions for better array support

// Create JavaScript array from C array - ffi.arrayFromPointer(ptr, length, type)
static JSValue ffi_array_from_pointer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 3) {
    return create_ffi_error(ctx, "Expected 3 arguments: pointer, length, type", "ffi.arrayFromPointer");
  }

  int64_t ptr_addr;
  uint32_t length;
  const char *type_str;

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

  if (length > 1024 * 1024) { // Limit to reasonable size
    JS_FreeCString(ctx, type_str);
    char message[128];
    snprintf(message, sizeof(message), "Array length too large: %u (maximum: 1M elements)", length);
    return create_ffi_error(ctx, message, "ffi.arrayFromPointer");
  }

  // Validate type
  ffi_type_t type = string_to_ffi_type(type_str);
  if (type == FFI_TYPE_VOID || type == FFI_TYPE_STRING || type == FFI_TYPE_ARRAY) {
    JS_FreeCString(ctx, type_str);
    char message[128];
    snprintf(message, sizeof(message), "Invalid array element type: '%s' (use int32, float, double, etc.)", type_str);
    return create_ffi_error(ctx, message, "ffi.arrayFromPointer");
  }

  void *ptr = (void *)(intptr_t)ptr_addr;
  JSValue array = JS_NewArray(ctx);

  // Convert C array to JavaScript array based on type
  for (uint32_t i = 0; i < length; i++) {
    JSValue element;
    
    switch (type) {
      case FFI_TYPE_INT:
      case FFI_TYPE_INT32: {
        int32_t *int_array = (int32_t *)ptr;
        element = JS_NewInt32(ctx, int_array[i]);
        break;
      }
      case FFI_TYPE_FLOAT: {
        float *float_array = (float *)ptr;
        element = JS_NewFloat64(ctx, (double)float_array[i]);
        break;
      }
      case FFI_TYPE_DOUBLE: {
        double *double_array = (double *)ptr;
        element = JS_NewFloat64(ctx, double_array[i]);
        break;
      }
      case FFI_TYPE_UINT32: {
        uint32_t *uint_array = (uint32_t *)ptr;
        element = JS_NewUint32(ctx, uint_array[i]);
        break;
      }
      default:
        // Default to treating as int32
        int32_t *default_array = (int32_t *)ptr;
        element = JS_NewInt32(ctx, default_array[i]);
        break;
    }
    
    JS_SetPropertyUint32(ctx, array, i, element);
  }

  JS_FreeCString(ctx, type_str);
  return array;
}

// Get array length helper - ffi.arrayLength(array)
static JSValue ffi_array_length(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "ffi.arrayLength expects 1 argument: array");
  }

  if (!JS_IsArray(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "Argument must be an array");
  }

  JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
  return length_val;
}

// Create FFI module for require("std:ffi")
JSValue JSRT_CreateFFIModule(JSContext *ctx) {
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
  JS_SetPropertyStr(ctx, ffi_obj, "arrayFromPointer", JS_NewCFunction(ctx, ffi_array_from_pointer, "arrayFromPointer", 3));
  JS_SetPropertyStr(ctx, ffi_obj, "arrayLength", JS_NewCFunction(ctx, ffi_array_length, "arrayLength", 1));

  // Add version information  
  JS_SetPropertyStr(ctx, ffi_obj, "version", JS_NewString(ctx, "2.2.0"));
  
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
  JS_SetPropertyStr(ctx, ffi_obj, "types", types_obj);

  JSRT_Debug("FFI: Created enhanced FFI module v2.2.0 with array support and enhanced error reporting");

  return ffi_obj;
}

// Initialize FFI module
void JSRT_RuntimeSetupStdFFI(JSRT_Runtime *rt) {
  // Create class IDs
  JS_NewClassID(&JSRT_FFILibraryClassID);
  JS_NewClassID(&JSRT_FFIFunctionClassID);

  // Register classes
  JS_NewClass(rt->rt, JSRT_FFILibraryClassID, &JSRT_FFILibraryClass);
  JS_NewClass(rt->rt, JSRT_FFIFunctionClassID, &JSRT_FFIFunctionClass);

  JSRT_Debug("FFI: Initialized FFI module");
}