/**
 * CommonJS Module Loader Implementation
 */

#include "commonjs_loader.h"
#include <stdlib.h>
#include <string.h>
#include "../../node/module/compile_cache.h"
#include "../../node/node_modules.h"
#include "../../runtime.h"  // For JSRT_Runtime
#include "babel_loader.h"

// Include hooks for load hook execution
#ifdef JSRT_NODE_COMPAT
#include "../../node/module/hooks.h"
#include "../../node/module/module_api.h"
#include "../../util/file.h"  // For JSRT_ReadFileResult

// Forward declaration for hook result conversion
JSRT_ReadFileResult jsrt_hook_result_to_file_result(JSRTHookLoadResult* hook_result);
#endif

#include "../core/module_cache.h"
#include "../core/module_loader.h"
#include "../protocols/protocol_dispatcher.h"
#include "../resolver/path_resolver.h"
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
    MODULE_DEBUG_ERROR("Module loading stack overflow (depth > %d)", MAX_LOADING_DEPTH);
    return -1;
  }

  loading_stack.paths[loading_stack.count] = strdup(resolved_path);
  if (!loading_stack.paths[loading_stack.count]) {
    return -1;
  }

  loading_stack.count++;
  MODULE_DEBUG_LOADER("Pushed to loading stack: %s (depth: %d)", resolved_path, loading_stack.count);
  return 0;
}

/**
 * Pop module from loading stack
 */
int jsrt_pop_loading_commonjs(JSRT_ModuleLoader* loader) {
  (void)loader;  // Currently using global stack

  if (loading_stack.count <= 0) {
    MODULE_DEBUG_ERROR("Module loading stack underflow");
    return -1;
  }

  loading_stack.count--;
  if (loading_stack.paths[loading_stack.count]) {
    MODULE_DEBUG_LOADER("Popped from loading stack: %s (depth: %d)", loading_stack.paths[loading_stack.count],
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
char* create_wrapper_code(const char* content, const char* resolved_path) {
  (void)resolved_path;  // Not used in wrapper itself, only passed as argument

  // CommonJS wrapper format:
  // (function(exports, require, module, __filename, __dirname) {
  //   <module code>
  // })

  if (!content) {
    MODULE_DEBUG_LOADER("Error: content is NULL in create_wrapper_code");
    return NULL;
  }

  size_t content_len = strlen(content);
  size_t wrapper_size = content_len + 256;  // Extra space for wrapper

  char* wrapper = malloc(wrapper_size);
  if (!wrapper) {
    return NULL;
  }

  // CommonJS wrapper with line number fix registration
  // Register module path for Error.stack line number adjustment (handled by error_stack_fix_cjs.js)
  // The registration line ensures this file's path is tracked for line number adjustment
  int written = snprintf(wrapper, wrapper_size,
                         "(function(exports, require, module, __filename, __dirname) {\n"
                         "globalThis.__jsrt_cjs_modules&&globalThis.__jsrt_cjs_modules.add(__filename);\n"
                         "%s\n})",
                         content);

  // Check if snprintf was truncated
  if (written < 0 || (size_t)written >= wrapper_size) {
    MODULE_DEBUG_LOADER("Wrapper code truncated (written=%d, size=%zu), reallocating", written, wrapper_size);
    free(wrapper);
    wrapper_size = written + 1;  // Include null terminator
    if (wrapper_size <= 0) {     // Handle snprintf error
      return NULL;
    }
    wrapper = malloc(wrapper_size);
    if (!wrapper) {
      return NULL;
    }
    written = snprintf(wrapper, wrapper_size,
                       "(function(exports, require, module, __filename, __dirname) {\n"
                       "globalThis.__jsrt_cjs_modules&&globalThis.__jsrt_cjs_modules.add(__filename);\n"
                       "%s\n})",
                       content);
    if (written < 0 || (size_t)written >= wrapper_size) {
      free(wrapper);
      return NULL;
    }
  }

  return wrapper;
}

/**
 * Load CommonJS module
 */
JSValue jsrt_load_commonjs_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* resolved_path,
                                  const char* specifier) {
  if (!ctx || !loader || !resolved_path) {
    MODULE_DEBUG_ERROR("Invalid arguments to jsrt_load_commonjs_module");
    return JS_EXCEPTION;
  }

  MODULE_DEBUG_LOADER("=== Loading CommonJS module: %s ===", resolved_path);

  // Check cache first
  if (loader->enable_cache && loader->cache) {
    JSValue cached = jsrt_module_cache_get(loader->cache, resolved_path);
    if (!JS_IsUndefined(cached)) {
      MODULE_DEBUG_LOADER("Cache HIT for CommonJS module: %s", resolved_path);
      return cached;
    }
  }

  // Check for circular dependency - return partially initialized module (Node.js behavior)
  if (jsrt_is_loading_commonjs(loader, resolved_path)) {
    MODULE_DEBUG_LOADER("Circular dependency detected, returning partial module: %s", resolved_path);

    // Return a partial module object that will be completed when the circular dependency resolves
    JSValue partial_module = JS_NewObject(ctx);
    if (JS_IsException(partial_module)) {
      return partial_module;
    }

    // Create a basic exports object that can be populated later
    JSValue exports_obj = JS_NewObject(ctx);
    if (JS_IsException(exports_obj)) {
      JS_FreeValue(ctx, partial_module);
      return exports_obj;
    }

    JS_SetPropertyStr(ctx, partial_module, "exports", exports_obj);

    // Cache the partial module so subsequent gets return the same object
    if (loader->enable_cache && loader->cache) {
      jsrt_module_cache_put(loader->cache, resolved_path, partial_module);
    }

    return partial_module;
  }

  // Mark module as loading
  if (jsrt_push_loading_commonjs(loader, resolved_path) != 0) {
    return jsrt_module_throw_error(ctx, JSRT_MODULE_INTERNAL_ERROR, "Failed to track module loading state");
  }

  JSRT_Runtime* runtime = (JSRT_Runtime*)JS_GetContextOpaque(ctx);
  JSRT_CompileCacheConfig* compile_cache = runtime ? runtime->compile_cache : NULL;
  bool compile_cache_enabled = compile_cache && jsrt_compile_cache_is_enabled(compile_cache);
  JSValue compiled_bytecode = JS_UNDEFINED;
  bool bytecode_cache_hit = false;

  if (compile_cache_enabled) {
    compiled_bytecode = jsrt_compile_cache_lookup(ctx, compile_cache, resolved_path);
    if (!JS_IsUndefined(compiled_bytecode)) {
      bytecode_cache_hit = true;
      MODULE_DEBUG_LOADER("Compile cache HIT for CommonJS bytecode: %s", resolved_path);
    }
  }

  // Load module content via protocol dispatcher (with load hook support)
  JSRT_ReadFileResult file_result = {0};
  char* wrapper_code = NULL;

  if (!bytecode_cache_hit) {
#ifdef JSRT_NODE_COMPAT
    // Try load hooks first if they're registered
    JSRT_Runtime* runtime = (JSRT_Runtime*)JS_GetContextOpaque(ctx);
    JSRTHookRegistry* hook_registry = runtime ? runtime->hook_registry : NULL;

    if (hook_registry && jsrt_hook_get_count(hook_registry) > 0) {
      // Create hook context
      JSRTHookContext hook_context = {0};
      hook_context.specifier = specifier ? specifier : resolved_path;
      hook_context.base_path = "";  // TODO: Determine base path from resolved_path
      hook_context.resolved_url = resolved_path;
      hook_context.is_main_module = false;  // TODO: Determine if main module
      hook_context.condition_count = 1;
      hook_context.conditions[0] = "require";
      hook_context.conditions[1] = "node";

      // Execute enhanced load hooks
      char* conditions[] = {"require", "node", NULL};
      JSRTHookLoadResult* hook_result =
          jsrt_hook_execute_load_enhanced(hook_registry, resolved_path, &hook_context, "commonjs", conditions);

      if (hook_result) {
        file_result = jsrt_hook_result_to_file_result(hook_result);

        if (file_result.error == JSRT_READ_FILE_OK) {
          // Free hook result structure but not the data (now owned by file_result)
          if (hook_result->format)
            free(hook_result->format);
          free(hook_result);
        } else {
          jsrt_hook_load_result_free(hook_result);
          // Fall through to normal loading
          file_result = jsrt_load_content_by_protocol(resolved_path);
        }
      } else {
        // No hook result, use normal loading
        file_result = jsrt_load_content_by_protocol(resolved_path);
      }
    } else {
      // No hooks registered, use normal loading
      file_result = jsrt_load_content_by_protocol(resolved_path);
    }
#else
    // Normal loading path when Node.js compatibility is disabled
    file_result = jsrt_load_content_by_protocol(resolved_path);
#endif

    if (file_result.error != JSRT_READ_FILE_OK) {
      jsrt_pop_loading_commonjs(loader);
      return jsrt_module_throw_error(ctx, JSRT_MODULE_LOAD_FAILED, "Failed to load module '%s': %s",
                                     specifier ? specifier : resolved_path,
                                     JSRT_ReadFileErrorToString(file_result.error));
    }

    MODULE_DEBUG_LOADER("Loaded content for %s (%zu bytes)", resolved_path, file_result.size);

    wrapper_code = jsrt_create_enhanced_wrapper_code(file_result.data, resolved_path);
    JSRT_ReadFileResultFree(&file_result);

    if (!wrapper_code) {
      jsrt_pop_loading_commonjs(loader);
      return jsrt_module_throw_error(ctx, JSRT_MODULE_INTERNAL_ERROR, "Failed to create wrapper code");
    }

    int eval_flags = JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY;
    compiled_bytecode = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), resolved_path, eval_flags);
    free(wrapper_code);
    wrapper_code = NULL;

    if (JS_IsException(compiled_bytecode)) {
      jsrt_pop_loading_commonjs(loader);
      return JS_EXCEPTION;
    }

    if (compile_cache_enabled) {
      if (!jsrt_compile_cache_store(ctx, compile_cache, resolved_path, compiled_bytecode)) {
        MODULE_DEBUG_LOADER("Compile cache store failed for %s", resolved_path);
      }
    }
  }

  JSValue func = JS_EvalFunction(ctx, compiled_bytecode);
  compiled_bytecode = JS_UNDEFINED;

  if (JS_IsException(func)) {
    jsrt_pop_loading_commonjs(loader);
    return JS_EXCEPTION;
  }

  // Create module and exports objects
  JSValue module = JS_NewObject(ctx);
  if (JS_IsException(module)) {
    JS_FreeValue(ctx, func);
    jsrt_pop_loading_commonjs(loader);
    return JS_EXCEPTION;
  }

  JSValue exports = JS_NewObject(ctx);
  if (JS_IsException(exports)) {
    JS_FreeValue(ctx, func);
    JS_FreeValue(ctx, module);
    jsrt_pop_loading_commonjs(loader);
    return JS_EXCEPTION;
  }

  // Set module properties (check for exceptions)
  if (JS_SetPropertyStr(ctx, module, "exports", JS_DupValue(ctx, exports)) < 0 ||
      JS_SetPropertyStr(ctx, module, "id", JS_NewString(ctx, resolved_path)) < 0 ||
      JS_SetPropertyStr(ctx, module, "filename", JS_NewString(ctx, resolved_path)) < 0 ||
      JS_SetPropertyStr(ctx, module, "loaded", JS_NewBool(ctx, false)) < 0) {
    free(wrapper_code);
    JS_FreeValue(ctx, module);
    JS_FreeValue(ctx, exports);
    jsrt_pop_loading_commonjs(loader);
    return JS_EXCEPTION;
  }

  // Compile wrapper function
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

  MODULE_DEBUG_LOADER("Successfully loaded CommonJS module: %s", resolved_path);
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

  MODULE_DEBUG_LOADER("js_commonjs_require called with specifier='%s'", specifier);

  // Get module path and loader from func_data
  if (!func_data) {
    MODULE_DEBUG_ERROR("func_data is NULL");
    JS_FreeCString(ctx, specifier);
    return JS_ThrowInternalError(ctx, "Invalid require() function data (NULL)");
  }

  if (JS_IsUndefined(func_data[0])) {
    MODULE_DEBUG_ERROR("func_data[0] is undefined");
    JS_FreeCString(ctx, specifier);
    return JS_ThrowInternalError(ctx, "Invalid require() function data (path undefined)");
  }

  if (JS_IsUndefined(func_data[1])) {
    MODULE_DEBUG_ERROR("func_data[1] is undefined");
    JS_FreeCString(ctx, specifier);
    return JS_ThrowInternalError(ctx, "Invalid require() function data (loader undefined)");
  }

  const char* module_path = JS_ToCString(ctx, func_data[0]);
  if (!module_path) {
    JS_FreeCString(ctx, specifier);
    return JS_EXCEPTION;
  }

  MODULE_DEBUG_LOADER("module_path='%s'", module_path);

  // Get loader pointer
  void* loader_ptr;
  int64_t loader_int64;
  if (JS_ToBigInt64(ctx, &loader_int64, func_data[1]) != 0) {
    MODULE_DEBUG_ERROR("Failed to convert func_data[1] to BigInt64");
    JS_FreeCString(ctx, specifier);
    JS_FreeCString(ctx, module_path);
    return JS_ThrowInternalError(ctx, "Invalid loader pointer (conversion failed)");
  }
  loader_ptr = (void*)(intptr_t)loader_int64;
  MODULE_DEBUG_LOADER("loader_ptr=%p", loader_ptr);

  JSRT_ModuleLoader* loader = (JSRT_ModuleLoader*)loader_ptr;

  MODULE_DEBUG_LOADER("require('%s') from module: %s", specifier, module_path);

#ifdef JSRT_NODE_COMPAT
  // Compact Node mode: check bare name as node module for CommonJS
  // Import path utility functions
  extern bool is_absolute_path(const char* path);
  extern bool is_relative_path(const char* path);
  extern bool JSRT_IsNodeModule(const char* name);

  // Get runtime to check compact_node_mode
  JSRT_Runtime* rt = (JSRT_Runtime*)JS_GetContextOpaque(ctx);
  if (rt && rt->compact_node_mode && !is_absolute_path(specifier) && !is_relative_path(specifier) &&
      JSRT_IsNodeModule(specifier)) {
    MODULE_DEBUG_LOADER("Compact Node mode (CJS): resolving '%s' as 'node:%s'", specifier, specifier);

    // Load as node: module
    char* node_specifier = malloc(strlen(specifier) + 6);
    if (node_specifier) {
      sprintf(node_specifier, "node:%s", specifier);

      JSRT_ModuleRequestType previous_request_type = loader->current_request_type;
      loader->current_request_type = JSRT_MODULE_REQUEST_COMMONJS;
      JSValue result = jsrt_load_module(loader, node_specifier, module_path);
      loader->current_request_type = previous_request_type;

      free(node_specifier);
      JS_FreeCString(ctx, specifier);
      JS_FreeCString(ctx, module_path);
      return result;
    }
  }
#endif

  JSRT_ModuleRequestType previous_request_type = loader->current_request_type;
  loader->current_request_type = JSRT_MODULE_REQUEST_COMMONJS;

  // Load the module (this will handle resolution, detection, and loading)
  JSValue result = jsrt_load_module(loader, specifier, module_path);

  loader->current_request_type = previous_request_type;

  JS_FreeCString(ctx, specifier);
  JS_FreeCString(ctx, module_path);

  return result;
}

/**
 * require.resolve(request, [options]) - Resolve module path without loading it
 * Compatible with Node.js require.resolve behavior
 */
static JSValue js_commonjs_require_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "require.resolve() requires at least one argument");
  }

  const char* request = JS_ToCString(ctx, argv[0]);
  if (!request) {
    return JS_EXCEPTION;
  }

  // Check if it's a builtin module (return as-is, like Module._resolveFilename)
  if (JSRT_IsNodeModule(request)) {
    JSValue result = JS_NewString(ctx, request);
    JS_FreeCString(ctx, request);
    return result;
  }

  // Handle node: prefixed builtins
  if (strncmp(request, "node:", 5) == 0) {
    const char* name = request + 5;
    if (JSRT_IsNodeModule(name)) {
      JSValue result = JS_NewString(ctx, request);
      JS_FreeCString(ctx, request);
      return result;
    }
  }

  // Get runtime and module loader from context
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->module_loader) {
    JS_FreeCString(ctx, request);
    return JS_ThrowInternalError(ctx, "require.resolve: module loader not available");
  }

  // Use path resolver to resolve the module path from current working directory
  JSRT_ResolvedPath* resolved = jsrt_resolve_path(ctx, request, NULL, false);

  JS_FreeCString(ctx, request);

  if (!resolved) {
    // Throw MODULE_NOT_FOUND error
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "MODULE_NOT_FOUND"));
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Cannot find module"));
    return JS_Throw(ctx, error);
  }

  // Return resolved path
  JSValue result = JS_NewString(ctx, resolved->resolved_path);
  jsrt_resolved_path_free(resolved);

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

  if (JS_IsException(require_func)) {
    JS_FreeValue(ctx, func_data[0]);
    JS_FreeValue(ctx, func_data[1]);
    return JS_EXCEPTION;
  }

  // Add require.resolve, require.cache, require.extensions, require.main properties
  // These are expected to be present on require functions in Node.js

  // Create a resolve function that calls Module._resolveFilename when available
  // We use a bound function to ensure we can access the Module constructor when needed
  JSValue resolve_func = JS_NewCFunction(ctx, js_commonjs_require_resolve, "resolve", 2);
  if (!JS_IsException(resolve_func)) {
    JS_SetPropertyStr(ctx, require_func, "resolve", resolve_func);
  } else {
    JS_FreeValue(ctx, resolve_func);
  }

  // Create empty cache and extensions objects for compatibility
  JSValue cache_obj = JS_NewObject(ctx);
  if (!JS_IsException(cache_obj)) {
    JS_SetPropertyStr(ctx, require_func, "cache", cache_obj);
  } else {
    JS_FreeValue(ctx, cache_obj);
  }

  JSValue extensions_obj = JS_NewObject(ctx);
  if (!JS_IsException(extensions_obj)) {
    JS_SetPropertyStr(ctx, require_func, "extensions", extensions_obj);
  } else {
    JS_FreeValue(ctx, extensions_obj);
  }

  // require.main - undefined for now (set when main module runs)
  JS_SetPropertyStr(ctx, require_func, "main", JS_UNDEFINED);

  // func_data values are now owned by the function
  // Don't free them here

  return require_func;
}
