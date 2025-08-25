#include "module.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/debug.h"
#include "../util/file.h"
#include "../util/path.h"

static char *resolve_module_path(const char *module_name, const char *base_path) {
  JSRT_Debug("resolve_module_path: module_name='%s', base_path='%s'", module_name, base_path ? base_path : "null");

  // Handle absolute paths
  if (module_name[0] == '/') {
    return strdup(module_name);
  }

  // Handle relative paths
  if (module_name[0] == '.' && (module_name[1] == '/' || (module_name[1] == '.' && module_name[2] == '/'))) {
    if (!base_path) {
      return strdup(module_name);
    }

    // Find the directory of base_path
    char *base_dir = strdup(base_path);
    char *last_slash = strrchr(base_dir, '/');
    if (last_slash) {
      *last_slash = '\0';
    } else {
      strcpy(base_dir, ".");
    }

    // Build the resolved path
    size_t path_len = strlen(base_dir) + strlen(module_name) + 2;
    char *resolved_path = malloc(path_len);
    snprintf(resolved_path, path_len, "%s/%s", base_dir, module_name);

    free(base_dir);
    return resolved_path;
  }

  // Handle bare module names (for now, just treat as relative to current directory)
  return strdup(module_name);
}

static char *try_extensions(const char *base_path) {
  const char *extensions[] = {".js", ".mjs", ""};
  size_t base_len = strlen(base_path);

  for (int i = 0; extensions[i][0] != '\0' || i == 2; i++) {
    const char *ext = extensions[i];
    size_t ext_len = strlen(ext);
    char *full_path = malloc(base_len + ext_len + 1);
    strcpy(full_path, base_path);
    strcat(full_path, ext);

    // Check if file exists
    JSRT_ReadFileResult result = JSRT_ReadFile(full_path);
    if (result.error == JSRT_READ_FILE_OK) {
      JSRT_ReadFileResultFree(&result);
      return full_path;
    }
    JSRT_ReadFileResultFree(&result);
    free(full_path);
  }

  return NULL;
}

char *JSRT_ModuleNormalize(JSContext *ctx, const char *module_base_name, const char *module_name, void *opaque) {
  JSRT_Debug("JSRT_ModuleNormalize: module_name='%s', module_base_name='%s'", module_name, module_base_name ? module_base_name : "null");

  // module_name is what we want to import, module_base_name is the importing module
  char *resolved_path = resolve_module_path(module_name, module_base_name);
  char *final_path = try_extensions(resolved_path);

  if (!final_path) {
    final_path = resolved_path;
  } else {
    free(resolved_path);
  }

  JSRT_Debug("JSRT_ModuleNormalize: resolved to '%s'", final_path);
  return final_path;
}

JSModuleDef *JSRT_ModuleLoader(JSContext *ctx, const char *module_name, void *opaque) {
  JSRT_Debug("JSRT_ModuleLoader: loading ES module '%s'", module_name);

  // Load the file
  JSRT_ReadFileResult file_result = JSRT_ReadFile(module_name);
  if (file_result.error != JSRT_READ_FILE_OK) {
    JS_ThrowReferenceError(ctx, "could not load module filename '%s': %s", module_name, JSRT_ReadFileErrorToString(file_result.error));
    JSRT_ReadFileResultFree(&file_result);
    return NULL;
  }

  // Always compile as ES module - the module loader is only for ES modules
  JSValue func_val = JS_Eval(ctx, file_result.data, file_result.size, module_name, 
                             JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
  
  JSRT_ReadFileResultFree(&file_result);

  if (JS_IsException(func_val)) {
    JSRT_Debug("JSRT_ModuleLoader: failed to compile module '%s'", module_name);
    return NULL;
  }

  // Get the module definition from the compiled function
  JSModuleDef *m = JS_VALUE_GET_PTR(func_val);
  JS_FreeValue(ctx, func_val);

  JSRT_Debug("JSRT_ModuleLoader: successfully loaded ES module '%s'", module_name);
  return m;
}

// Simple module cache for require()
typedef struct {
  char *name;
  JSValue exports;
} RequireModuleCache;

static RequireModuleCache *module_cache = NULL;
static size_t module_cache_size = 0;
static size_t module_cache_capacity = 0;

static JSValue get_cached_module(JSContext *ctx, const char *name) {
  for (size_t i = 0; i < module_cache_size; i++) {
    if (strcmp(module_cache[i].name, name) == 0) {
      return JS_DupValue(ctx, module_cache[i].exports);
    }
  }
  return JS_UNDEFINED;
}

static void cache_module(JSContext *ctx, const char *name, JSValue exports) {
  if (module_cache_size >= module_cache_capacity) {
    module_cache_capacity = module_cache_capacity == 0 ? 16 : module_cache_capacity * 2;
    module_cache = realloc(module_cache, module_cache_capacity * sizeof(RequireModuleCache));
  }
  
  module_cache[module_cache_size].name = strdup(name);
  module_cache[module_cache_size].exports = JS_DupValue(ctx, exports);
  module_cache_size++;
}

static JSValue js_require(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "require expects at least 1 argument");
  }

  const char *module_name = JS_ToCString(ctx, argv[0]);
  if (!module_name) {
    return JS_EXCEPTION;
  }

  JSRT_Debug("js_require: loading CommonJS module '%s'", module_name);

  // Resolve the module path
  char *resolved_path = resolve_module_path(module_name, NULL);
  char *final_path = try_extensions(resolved_path);
  
  if (!final_path) {
    final_path = resolved_path;
  } else {
    free(resolved_path);
  }

  // Check cache first
  JSValue cached = get_cached_module(ctx, final_path);
  if (!JS_IsUndefined(cached)) {
    JS_FreeCString(ctx, module_name);
    free(final_path);
    return cached;
  }

  // Load and evaluate the file
  JSRT_ReadFileResult file_result = JSRT_ReadFile(final_path);
  if (file_result.error != JSRT_READ_FILE_OK) {
    JS_FreeCString(ctx, module_name);
    free(final_path);
    JSRT_ReadFileResultFree(&file_result);
    return JS_ThrowReferenceError(ctx, "Cannot find module '%s'", module_name);
  }

  // Create isolated module context
  JSValue module = JS_NewObject(ctx);
  JSValue exports = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, module, "exports", JS_DupValue(ctx, exports));

  // Create a wrapper function to provide CommonJS environment
  size_t wrapper_size = file_result.size + 512;
  char *wrapper_code = malloc(wrapper_size);
  snprintf(wrapper_code, wrapper_size,
           "(function(exports, require, module, __filename, __dirname) {\n%s\n})",
           file_result.data);

  // Evaluate the wrapper function
  JSValue func = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), final_path, JS_EVAL_TYPE_GLOBAL);
  free(wrapper_code);
  
  if (JS_IsException(func)) {
    JSRT_ReadFileResultFree(&file_result);
    JS_FreeCString(ctx, module_name);
    free(final_path);
    JS_FreeValue(ctx, module);
    JS_FreeValue(ctx, exports);
    return func;
  }

  // Call the wrapper function with CommonJS arguments
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue require_func = JS_GetPropertyStr(ctx, global, "require");
  JSValue args[5] = {
    JS_DupValue(ctx, exports),
    require_func,
    JS_DupValue(ctx, module),
    JS_NewString(ctx, final_path),
    JS_NewString(ctx, ".")  // Simple __dirname
  };

  JSValue result = JS_Call(ctx, func, global, 5, args);

  // Clean up
  JSRT_ReadFileResultFree(&file_result);
  JS_FreeValue(ctx, func);
  JS_FreeValue(ctx, global);
  for (int i = 0; i < 5; i++) {
    JS_FreeValue(ctx, args[i]);
  }

  if (JS_IsException(result)) {
    JS_FreeCString(ctx, module_name);
    free(final_path);
    JS_FreeValue(ctx, module);
    JS_FreeValue(ctx, exports);
    return result;
  }

  JS_FreeValue(ctx, result);

  // Get the final module.exports value
  JSValue module_exports = JS_GetPropertyStr(ctx, module, "exports");
  JS_FreeValue(ctx, module);
  JS_FreeValue(ctx, exports);

  // Cache the module
  cache_module(ctx, final_path, module_exports);

  JS_FreeCString(ctx, module_name);
  free(final_path);

  return module_exports;
}

void JSRT_StdModuleInit(JSRT_Runtime *rt) {
  JSRT_Debug("JSRT_StdModuleInit: initializing ES module loader");
  JS_SetModuleLoaderFunc(rt->rt, JSRT_ModuleNormalize, JSRT_ModuleLoader, rt);
}

void JSRT_StdCommonJSInit(JSRT_Runtime *rt) {
  JSRT_Debug("JSRT_StdCommonJSInit: initializing CommonJS support");
  
  JSValue global = JS_GetGlobalObject(rt->ctx);
  JS_SetPropertyStr(rt->ctx, global, "require", JS_NewCFunction(rt->ctx, js_require, "require", 1));
  JS_FreeValue(rt->ctx, global);
}