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
  ffi_function_t *func = (ffi_function_t *)JS_GetOpaque(this_val, JSRT_FFIFunctionClassID);
  if (!func) {
    return JS_ThrowTypeError(ctx, "Invalid FFI function");
  }

  if (argc != func->arg_count) {
    return JS_ThrowTypeError(ctx, "FFI function expects %d arguments, got %d", func->arg_count, argc);
  }

  // For simplicity, we'll only support functions with up to 8 arguments
  // This is a basic implementation - a full FFI would use libffi
  if (func->arg_count > 8) {
    return JS_ThrowTypeError(ctx, "FFI functions with more than 8 arguments are not supported");
  }

  // Convert JS arguments to native values
  uint64_t args[8] = {0};  // Use uint64_t to handle all basic types
  const char *string_args[8] = {NULL};

  for (int i = 0; i < func->arg_count; i++) {
    if (!js_to_native(ctx, argv[i], func->arg_types[i], &args[i])) {
      // Free any allocated strings before returning error
      for (int j = 0; j < i; j++) {
        if (func->arg_types[j] == FFI_TYPE_STRING && string_args[j]) {
          JS_FreeCString(ctx, string_args[j]);
        }
      }
      return JS_ThrowTypeError(ctx, "Failed to convert argument %d", i);
    }

    // Keep track of string arguments for later cleanup
    if (func->arg_types[i] == FFI_TYPE_STRING) {
      string_args[i] = (const char *)args[i];
    }
  }

  // Call the function - this is a simplified implementation
  // A real FFI would use libffi for proper calling convention handling
  uint64_t result = 0;

  // Cast function pointer and call based on signature
  // This is highly simplified and only works for basic cases
  if (func->return_type == FFI_TYPE_VOID) {
    switch (func->arg_count) {
      case 0:
        ((void (*)())func->func_ptr)();
        break;
      case 1:
        ((void (*)(uint64_t))func->func_ptr)(args[0]);
        break;
      case 2:
        ((void (*)(uint64_t, uint64_t))func->func_ptr)(args[0], args[1]);
        break;
      // Add more cases as needed
      default:
        // Free string arguments
        for (int i = 0; i < func->arg_count; i++) {
          if (func->arg_types[i] == FFI_TYPE_STRING && string_args[i]) {
            JS_FreeCString(ctx, string_args[i]);
          }
        }
        return JS_ThrowTypeError(ctx, "Unsupported function signature");
    }
  } else {
    switch (func->arg_count) {
      case 0:
        result = ((uint64_t(*)())func->func_ptr)();
        break;
      case 1:
        result = ((uint64_t(*)(uint64_t))func->func_ptr)(args[0]);
        break;
      case 2:
        result = ((uint64_t(*)(uint64_t, uint64_t))func->func_ptr)(args[0], args[1]);
        break;
      // Add more cases as needed
      default:
        // Free string arguments
        for (int i = 0; i < func->arg_count; i++) {
          if (func->arg_types[i] == FFI_TYPE_STRING && string_args[i]) {
            JS_FreeCString(ctx, string_args[i]);
          }
        }
        return JS_ThrowTypeError(ctx, "Unsupported function signature");
    }
  }

  // Free string arguments
  for (int i = 0; i < func->arg_count; i++) {
    if (func->arg_types[i] == FFI_TYPE_STRING && string_args[i]) {
      JS_FreeCString(ctx, string_args[i]);
    }
  }

  // Convert result back to JS value
  return native_to_js(ctx, func->return_type, &result);
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

    ffi_function_t *func = malloc(sizeof(ffi_function_t));
    if (!func) {
      JS_FreeValue(ctx, prop_val);
      JS_FreeValue(ctx, return_type_val);
      JS_FreeValue(ctx, args_val);
      JS_FreeCString(ctx, return_type_str);
      JS_FreeCString(ctx, func_name);
      continue;
    }

    func->return_type = string_to_ffi_type(return_type_str);
    func->arg_count = args_length;
    func->func_ptr = func_ptr;
    func->arg_types = malloc(args_length * sizeof(ffi_type_t));

    if (!func->arg_types && args_length > 0) {
      free(func);
      JS_FreeValue(ctx, prop_val);
      JS_FreeValue(ctx, return_type_val);
      JS_FreeValue(ctx, args_val);
      JS_FreeCString(ctx, return_type_str);
      JS_FreeCString(ctx, func_name);
      continue;
    }

    // Parse each argument type
    for (uint32_t j = 0; j < args_length; j++) {
      JSValue arg_type_val = JS_GetPropertyUint32(ctx, args_val, j);
      if (JS_IsString(arg_type_val)) {
        const char *arg_type_str = JS_ToCString(ctx, arg_type_val);
        if (arg_type_str) {
          func->arg_types[j] = string_to_ffi_type(arg_type_str);
          JS_FreeCString(ctx, arg_type_str);
        } else {
          func->arg_types[j] = FFI_TYPE_VOID;
        }
      } else {
        func->arg_types[j] = FFI_TYPE_VOID;
      }
      JS_FreeValue(ctx, arg_type_val);
    }

    // Create JS function
    JSValue js_func = JS_NewCFunction(ctx, ffi_function_call, func_name, args_length);
    JS_SetOpaque(js_func, func);
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

// Create FFI module for require("std:ffi")
JSValue JSRT_CreateFFIModule(JSContext *ctx) {
  JSValue ffi_obj = JS_NewObject(ctx);

  // ffi.Library function
  JS_SetPropertyStr(ctx, ffi_obj, "Library", JS_NewCFunction(ctx, ffi_library, "Library", 2));

  // Add version information
  JS_SetPropertyStr(ctx, ffi_obj, "version", JS_NewString(ctx, "1.0.0"));

  JSRT_Debug("FFI: Created FFI module");

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