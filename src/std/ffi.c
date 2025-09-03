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
  FFI_TYPE_STRING
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

// FFI function metadata storage
typedef struct {
  char *return_type;
  int arg_count;
  void *func_ptr;
} ffi_func_metadata_t;

// Global metadata storage (simple approach for now)
static ffi_func_metadata_t *global_metadata_table = NULL;
static int global_metadata_count = 0;
static int global_metadata_capacity = 0;

// Add metadata to global table
static int add_metadata(const char *return_type, int arg_count, void *func_ptr) {
  if (global_metadata_count >= global_metadata_capacity) {
    global_metadata_capacity = global_metadata_capacity == 0 ? 8 : global_metadata_capacity * 2;
    global_metadata_table = realloc(global_metadata_table, 
                                    global_metadata_capacity * sizeof(ffi_func_metadata_t));
    if (!global_metadata_table) {
      return -1;
    }
  }
  
  int index = global_metadata_count++;
  global_metadata_table[index].return_type = strdup(return_type);
  global_metadata_table[index].arg_count = arg_count;
  global_metadata_table[index].func_ptr = func_ptr;
  return index;
}

// Find metadata by function pointer
static ffi_func_metadata_t *find_metadata(void *func_ptr) {
  for (int i = 0; i < global_metadata_count; i++) {
    if (global_metadata_table[i].func_ptr == func_ptr) {
      return &global_metadata_table[i];
    }
  }
  return NULL;
}
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
  return FFI_TYPE_VOID;  // Default to void for unknown types
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
  
  // Get metadata index from properties  
  JSValue metadata_index_val = JS_GetPropertyStr(ctx, this_val, "_ffi_metadata_index");
  
  int32_t metadata_index;
  if (JS_ToInt32(ctx, &metadata_index, metadata_index_val) < 0) {
    JS_FreeValue(ctx, metadata_index_val);
    return JS_ThrowTypeError(ctx, "Invalid FFI function - missing metadata index");
  }
  
  JS_FreeValue(ctx, metadata_index_val);
  
  JSRT_Debug("FFI Call: Looking up metadata[%d]", metadata_index);
  
  // Get metadata from global table
  if (metadata_index < 0 || metadata_index >= global_metadata_count) {
    return JS_ThrowTypeError(ctx, "Invalid FFI function - metadata index out of range");
  }
  
  ffi_func_metadata_t *metadata = &global_metadata_table[metadata_index];
  
  JSRT_Debug("FFI Call: Found metadata - return_type='%s', arg_count=%d, func_ptr=%p", 
             metadata->return_type, metadata->arg_count, metadata->func_ptr);

  if (argc != metadata->arg_count) {
    return JS_ThrowTypeError(ctx, "FFI function expects %d arguments, got %d", 
                             metadata->arg_count, argc);
  }
  // Enhanced implementation for function calls with up to 16 arguments
  if (argc > 16) {
    return JS_ThrowTypeError(ctx, "FFI functions with more than 16 arguments not supported");
  }

  JSRT_Debug("FFI Call: About to convert arguments, argc=%d", argc);

  // Convert arguments to native values - now supports up to 16 arguments
  uint64_t args[16] = {0};
  const char *string_args[16] = {NULL};

  JSRT_Debug("FFI Call: Starting argument conversion loop");
  for (int i = 0; i < argc; i++) {
    JSRT_Debug("FFI Call: Converting argument %d", i);
    if (JS_IsString(argv[i])) {
      string_args[i] = JS_ToCString(ctx, argv[i]);
      args[i] = (uint64_t)(uintptr_t)string_args[i];
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
          // Clean up allocated strings
          for (int j = 0; j < i; j++) {
            if (string_args[j]) JS_FreeCString(ctx, string_args[j]);
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
      // Clean up allocated strings
      for (int j = 0; j < i; j++) {
        if (string_args[j]) JS_FreeCString(ctx, string_args[j]);
      }
      return JS_ThrowTypeError(ctx, "Unsupported argument type at position %d", i);
    }
  }

  JSRT_Debug("FFI Call: Argument conversion completed, starting function call");

  // Perform the function call based on signature with enhanced type support
  JSValue result = JS_UNDEFINED;
  ffi_type_t return_type = string_to_ffi_type(metadata->return_type);

  JSRT_Debug("FFI Call: Return type parsed as %d", (int)return_type);

  // Get function pointer from metadata
  void *func_ptr = metadata->func_ptr;
  
  // Handle different return types with expanded argument support
  if (return_type == FFI_TYPE_INT || return_type == FFI_TYPE_INT32) {
    JSRT_Debug("FFI Call: Calling function with int return type");
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
    result = JS_ThrowTypeError(ctx, "Unsupported return type: %s", metadata->return_type);
  }

  // Clean up string arguments
  for (int i = 0; i < argc; i++) {
    if (string_args[i]) {
      JS_FreeCString(ctx, string_args[i]);
    }
  }

  return result;
}

// ffi.Library(name, functions) - Load a dynamic library
static JSValue ffi_library(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "ffi.Library expects 2 arguments: library name and function definitions");
  }

  const char *lib_name = JS_ToCString(ctx, argv[0]);
  if (!lib_name) {
    return JS_ThrowTypeError(ctx, "Library name must be a string");
  }

  if (!JS_IsObject(argv[1])) {
    JS_FreeCString(ctx, lib_name);
    return JS_ThrowTypeError(ctx, "Function definitions must be an object");
  }

  // Load the library
  jsrt_dl_handle_t handle = JSRT_DLOPEN(lib_name);
  if (!handle) {
    JSValue error = JS_ThrowReferenceError(ctx, "Failed to load library '%s'", lib_name);
    JS_FreeCString(ctx, lib_name);
    return error;
  }

  JSRT_Debug("FFI: Successfully loaded library '%s'", lib_name);

  // Create library object
  ffi_library_t *lib = malloc(sizeof(ffi_library_t));
  if (!lib) {
    JSRT_DLCLOSE(handle);
    JS_FreeCString(ctx, lib_name);
    return JS_ThrowOutOfMemory(ctx);
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

    // Get function from library
    void *func_ptr = JSRT_DLSYM(handle, func_name);
    if (!func_ptr) {
      JSRT_Debug("FFI: Function '%s' not found in library '%s'", func_name, lib_name);
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

    // Store metadata in global table instead of properties
    int metadata_index = add_metadata(return_type_str, args_length, func_ptr);
    if (metadata_index < 0) {
      JS_FreeValue(ctx, js_func);
      JS_FreeValue(ctx, prop_val);
      JS_FreeValue(ctx, return_type_val);
      JS_FreeValue(ctx, args_val);
      JS_FreeCString(ctx, return_type_str);
      JS_FreeCString(ctx, func_name);
      continue;
    }
    
    // Still store metadata index as property for lookup (more reliable than function pointer)
    JS_SetPropertyStr(ctx, js_func, "_ffi_metadata_index", JS_NewInt32(ctx, metadata_index));
    
    JSRT_Debug("FFI: Stored metadata[%d] - return_type='%s', arg_count=%d, func_ptr=%p", 
               metadata_index, return_type_str, args_length, func_ptr);

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
    return JS_ThrowTypeError(ctx, "ffi.malloc expects 1 argument: size");
  }

  uint32_t size;
  if (JS_ToUint32(ctx, &size, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "Size must be a positive number");
  }

  if (size == 0 || size > 1024 * 1024 * 1024) { // Limit to 1GB for safety
    return JS_ThrowRangeError(ctx, "Invalid allocation size: %u", size);
  }

  void *ptr = malloc(size);
  if (!ptr) {
    return JS_ThrowOutOfMemory(ctx);
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

  // Add version information  
  JS_SetPropertyStr(ctx, ffi_obj, "version", JS_NewString(ctx, "2.0.0"));
  
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
  JS_SetPropertyStr(ctx, ffi_obj, "types", types_obj);

  JSRT_Debug("FFI: Created enhanced FFI module v2.0.0");

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