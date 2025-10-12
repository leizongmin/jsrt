/**
 * CommonJS Module Loader Implementation
 */

#include "commonjs_loader.h"
#include <stdlib.h>
#include <string.h>
#include "../core/module_cache.h"
#include "../core/module_loader.h"
#include "../protocols/protocol_dispatcher.h"
#include "../resolver/path_util.h"
#include "../util/module_debug.h"
#include "../util/module_errors.h"

// Loading stack for circular dependency detection
#define MAX_LOADING_DEPTH 100

typedef struct {
  char* paths[MAX_LOADING_DEPTH];
  int count;
} LoadingStack;

static LoadingStack loading_stack = {{NULL}, 0};

/**
 * Check if module is currently being loaded
 */
bool jsrt_is_loading_commonjs(JSRT_ModuleLoader* loader, const char* resolved_path) {
  (void)loader;  // Currently using global stack, loader param for future use

  for (int i = 0; i < loading_stack.count; i++) {
    if (loading_stack.paths[i] && strcmp(loading_stack.paths[i], resolved_path) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * Push module to loading stack
 */
int jsrt_push_loading_commonjs(JSRT_ModuleLoader* loader, const char* resolved_path) {
  (void)loader;  // Currently using global stack

  if (loading_stack.count >= MAX_LOADING_DEPTH) {
    MODULE_Debug_Error("Module loading stack overflow (depth > %d)", MAX_LOADING_DEPTH);
    return -1;
  }

  loading_stack.paths[loading_stack.count] = strdup(resolved_path);
  if (!loading_stack.paths[loading_stack.count]) {
    return -1;
  }

  loading_stack.count++;
  MODULE_Debug_Loader("Pushed to loading stack: %s (depth: %d)", resolved_path, loading_stack.count);
  return 0;
}

/**
 * Pop module from loading stack
 */
int jsrt_pop_loading_commonjs(JSRT_ModuleLoader* loader) {
  (void)loader;  // Currently using global stack

  if (loading_stack.count <= 0) {
    MODULE_Debug_Error("Module loading stack underflow");
    return -1;
  }

  loading_stack.count--;
  if (loading_stack.paths[loading_stack.count]) {
    MODULE_Debug_Loader("Popped from loading stack: %s (depth: %d)", loading_stack.paths[loading_stack.count],
                        loading_stack.count);
    free(loading_stack.paths[loading_stack.count]);
    loading_stack.paths[loading_stack.count] = NULL;
  }

  return 0;
}

/**
 * Get directory name from path (cross-platform)
 */
static char* get_dirname(const char* path) {
  if (!path) {
    return strdup(".");
  }

  // Find last path separator (/ or \)
  const char* last_sep = NULL;
  for (const char* p = path; *p; p++) {
    if (*p == '/' || *p == '\\') {
      last_sep = p;
    }
  }

  if (!last_sep) {
    return strdup(".");
  }

  size_t len = last_sep - path;
  if (len == 0) {
    return strdup("/");  // Root directory
  }

  char* dirname = malloc(len + 1);
  if (!dirname) {
    return NULL;
  }

  memcpy(dirname, path, len);
  dirname[len] = '\0';
  return dirname;
}

/**
 * Create CommonJS wrapper code
 */
static char* create_wrapper_code(const char* content, const char* resolved_path) {
  (void)resolved_path;  // Not used in wrapper itself, only passed as argument

  // CommonJS wrapper format:
  // (function(exports, require, module, __filename, __dirname) {
  //   <module code>
  // })

  size_t content_len = strlen(content);
  size_t wrapper_size = content_len + 256;  // Extra space for wrapper

  char* wrapper = malloc(wrapper_size);
  if (!wrapper) {
    return NULL;
  }

  snprintf(wrapper, wrapper_size,
           "(function(exports, require, module, __filename, __dirname) {\n"
           "%s\n"
           "})",
           content);

  return wrapper;
}

/**
 * Load CommonJS module
 */
JSValue jsrt_load_commonjs_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* resolved_path,
                                  const char* specifier) {
  if (!ctx || !loader || !resolved_path) {
    MODULE_Debug_Error("Invalid arguments to jsrt_load_commonjs_module");
    return JS_EXCEPTION;
  }

  MODULE_Debug_Loader("=== Loading CommonJS module: %s ===", resolved_path);

  // Check cache first
  if (loader->enable_cache && loader->cache) {
    JSValue cached = jsrt_module_cache_get(loader->cache, resolved_path);
    if (!JS_IsUndefined(cached)) {
      MODULE_Debug_Loader("Cache HIT for CommonJS module: %s", resolved_path);
      return cached;
    }
  }

  // Check for circular dependency
  if (jsrt_is_loading_commonjs(loader, resolved_path)) {
    MODULE_Debug_Error("Circular dependency detected: %s", resolved_path);
    return jsrt_module_throw_error(ctx, JSRT_MODULE_CIRCULAR_DEPENDENCY, "Circular dependency detected for module: %s",
                                   resolved_path);
  }

  // Mark module as loading
  if (jsrt_push_loading_commonjs(loader, resolved_path) != 0) {
    return jsrt_module_throw_error(ctx, JSRT_MODULE_INTERNAL_ERROR, "Failed to track module loading state");
  }

  // Load module content via protocol dispatcher
  JSRT_ReadFileResult file_result = jsrt_load_content_by_protocol(resolved_path);
  if (file_result.error != JSRT_READ_FILE_OK) {
    jsrt_pop_loading_commonjs(loader);
    return jsrt_module_throw_error(ctx, JSRT_MODULE_LOAD_FAILED, "Failed to load module '%s': %s",
                                   specifier ? specifier : resolved_path,
                                   JSRT_ReadFileErrorToString(file_result.error));
  }

  MODULE_Debug_Loader("Loaded content for %s (%zu bytes)", resolved_path, file_result.size);

  // Create module and exports objects
  JSValue module = JS_NewObject(ctx);
  if (JS_IsException(module)) {
    JSRT_ReadFileResultFree(&file_result);
    jsrt_pop_loading_commonjs(loader);
    return JS_EXCEPTION;
  }

  JSValue exports = JS_NewObject(ctx);
  if (JS_IsException(exports)) {
    JS_FreeValue(ctx, module);
    JSRT_ReadFileResultFree(&file_result);
    jsrt_pop_loading_commonjs(loader);
    return JS_EXCEPTION;
  }

  // Set module properties
  JS_SetPropertyStr(ctx, module, "exports", JS_DupValue(ctx, exports));
  JS_SetPropertyStr(ctx, module, "id", JS_NewString(ctx, resolved_path));
  JS_SetPropertyStr(ctx, module, "filename", JS_NewString(ctx, resolved_path));
  JS_SetPropertyStr(ctx, module, "loaded", JS_NewBool(ctx, false));

  // Create wrapper code
  char* wrapper_code = create_wrapper_code(file_result.data, resolved_path);
  JSRT_ReadFileResultFree(&file_result);

  if (!wrapper_code) {
    JS_FreeValue(ctx, module);
    JS_FreeValue(ctx, exports);
    jsrt_pop_loading_commonjs(loader);
    return jsrt_module_throw_error(ctx, JSRT_MODULE_INTERNAL_ERROR, "Failed to create wrapper code");
  }

  // Compile wrapper function
  JSValue func = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), resolved_path, JS_EVAL_TYPE_GLOBAL);
  free(wrapper_code);

  if (JS_IsException(func)) {
    JS_FreeValue(ctx, module);
    JS_FreeValue(ctx, exports);
    jsrt_pop_loading_commonjs(loader);
    return JS_EXCEPTION;
  }

  // Create require function for this module
  JSValue require_func = jsrt_create_require_function(ctx, loader, resolved_path);
  if (JS_IsException(require_func)) {
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, module);
    JS_FreeValue(ctx, exports);
    jsrt_pop_loading_commonjs(loader);
    return JS_EXCEPTION;
  }

  // Calculate __dirname
  char* dirname = get_dirname(resolved_path);
  if (!dirname) {
    dirname = strdup(".");
  }

  // Call wrapper function with CommonJS environment
  JSValue args[5] = {
      JS_DupValue(ctx, exports),         // exports
      require_func,                      // require
      JS_DupValue(ctx, module),          // module
      JS_NewString(ctx, resolved_path),  // __filename
      JS_NewString(ctx, dirname)         // __dirname
  };

  free(dirname);

  JSValue global = JS_GetGlobalObject(ctx);
  JSValue result = JS_Call(ctx, func, global, 5, args);
  JS_FreeValue(ctx, global);

  // Clean up arguments and function
  JS_FreeValue(ctx, func);
  for (int i = 0; i < 5; i++) {
    JS_FreeValue(ctx, args[i]);
  }

  // Mark module as no longer loading
  jsrt_pop_loading_commonjs(loader);

  if (JS_IsException(result)) {
    JS_FreeValue(ctx, module);
    JS_FreeValue(ctx, exports);
    return JS_EXCEPTION;
  }

  JS_FreeValue(ctx, result);

  // Get final module.exports (may have been reassigned)
  JSValue module_exports = JS_GetPropertyStr(ctx, module, "exports");

  // Mark module as loaded
  JS_SetPropertyStr(ctx, module, "loaded", JS_NewBool(ctx, true));

  // Free temporary objects
  JS_FreeValue(ctx, module);
  JS_FreeValue(ctx, exports);

  // Cache the result
  if (loader->enable_cache && loader->cache) {
    jsrt_module_cache_put(loader->cache, resolved_path, module_exports);
  }

  MODULE_Debug_Loader("Successfully loaded CommonJS module: %s", resolved_path);
  return module_exports;
}

/**
 * CommonJS require() implementation
 */
static JSValue js_commonjs_require(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                   JSValue* func_data) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "require() expects at least 1 argument");
  }

  const char* specifier = JS_ToCString(ctx, argv[0]);
  if (!specifier) {
    return JS_EXCEPTION;
  }

  // Get module path and loader from func_data
  if (!func_data || JS_IsUndefined(func_data[0]) || JS_IsUndefined(func_data[1])) {
    JS_FreeCString(ctx, specifier);
    return JS_ThrowInternalError(ctx, "Invalid require() function data");
  }

  const char* module_path = JS_ToCString(ctx, func_data[0]);
  if (!module_path) {
    JS_FreeCString(ctx, specifier);
    return JS_EXCEPTION;
  }

  // Get loader pointer
  void* loader_ptr;
  if (JS_ToInt64(ctx, (int64_t*)&loader_ptr, func_data[1]) != 0) {
    JS_FreeCString(ctx, specifier);
    JS_FreeCString(ctx, module_path);
    return JS_ThrowInternalError(ctx, "Invalid loader pointer");
  }

  JSRT_ModuleLoader* loader = (JSRT_ModuleLoader*)loader_ptr;

  MODULE_Debug_Loader("require('%s') from module: %s", specifier, module_path);

  // Load the module (this will handle resolution, detection, and loading)
  JSValue result = jsrt_load_module(loader, specifier, module_path);

  JS_FreeCString(ctx, specifier);
  JS_FreeCString(ctx, module_path);

  return result;
}

/**
 * Create require() function bound to module path
 */
JSValue jsrt_create_require_function(JSContext* ctx, JSRT_ModuleLoader* loader, const char* module_path) {
  if (!ctx || !loader || !module_path) {
    return JS_EXCEPTION;
  }

  // Create function data array [module_path, loader_ptr]
  JSValue func_data[2];
  func_data[0] = JS_NewString(ctx, module_path);
  func_data[1] = JS_NewBigInt64(ctx, (int64_t)(intptr_t)loader);

  if (JS_IsException(func_data[0]) || JS_IsException(func_data[1])) {
    JS_FreeValue(ctx, func_data[0]);
    JS_FreeValue(ctx, func_data[1]);
    return JS_EXCEPTION;
  }

  // Create function with data
  JSValue require_func = JS_NewCFunctionData(ctx, js_commonjs_require, 1, 0, 2, func_data);

  // func_data values are now owned by the function
  // Don't free them here

  return require_func;
}
