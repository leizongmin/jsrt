#include "module.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>  // for _access
#include <windows.h>
#define F_OK 0
#define access _access
// Windows implementation of realpath using _fullpath
static char* jsrt_realpath(const char* path, char* resolved_path) {
  return _fullpath(resolved_path, path, _MAX_PATH);
}
#else
#include <unistd.h>
// Unix implementation uses system realpath
#define jsrt_realpath(path, resolved) realpath(path, resolved)
#endif

#include "../util/debug.h"
#include "../util/file.h"
#include "../util/json.h"
#include "../util/path.h"
#include "assert.h"
#include "ffi.h"
#include "process.h"

// Module init function for jsrt:assert ES module
static int js_std_assert_init(JSContext* ctx, JSModuleDef* m) {
  JSValue assert_module = JSRT_CreateAssertModule(ctx);
  if (JS_IsException(assert_module)) {
    return -1;
  }
  JS_SetModuleExport(ctx, m, "default", assert_module);
  return 0;
}

// Module init function for jsrt:process ES module
static int js_std_process_init(JSContext* ctx, JSModuleDef* m) {
  JSValue process_module = JSRT_CreateProcessModule(ctx);
  if (JS_IsException(process_module)) {
    return -1;
  }
  JS_SetModuleExport(ctx, m, "default", process_module);
  return 0;
}

// Module init function for jsrt:ffi ES module
static int js_std_ffi_init(JSContext* ctx, JSModuleDef* m) {
  JSValue ffi_module = JSRT_CreateFFIModule(ctx);
  if (JS_IsException(ffi_module)) {
    return -1;
  }
  JS_SetModuleExport(ctx, m, "default", ffi_module);
  return 0;
}

// Node.js module resolution functions
static char* find_node_modules_path(const char* start_dir, const char* package_name) {
  JSRT_Debug("find_node_modules_path: start_dir='%s', package_name='%s'", start_dir, package_name);

  // Start from the given directory and walk up the tree
  char* current_dir = strdup(start_dir ? start_dir : ".");
  char* normalized_dir = jsrt_realpath(current_dir, NULL);
  free(current_dir);

  if (!normalized_dir) {
    return NULL;
  }

  // Walk up the directory tree looking for node_modules
  char* search_dir = normalized_dir;
  while (search_dir && strlen(search_dir) > 1) {
    // Build node_modules/package_name path
    size_t path_len = strlen(search_dir) + strlen(package_name) + 20;
    char* package_path = malloc(path_len);
    snprintf(package_path, path_len, "%s/node_modules/%s", search_dir, package_name);

    // Check if the package directory exists using access()
    if (access(package_path, F_OK) == 0) {
      JSRT_Debug("find_node_modules_path: found package at '%s'", package_path);
      free(normalized_dir);
      return package_path;
    }
    free(package_path);

    // Move up one directory
    char* parent = strrchr(search_dir, '/');
    if (parent && parent != search_dir) {
      *parent = '\0';
    } else {
      break;
    }
  }

  free(normalized_dir);
  JSRT_Debug("find_node_modules_path: package '%s' not found", package_name);
  return NULL;
}

static char* resolve_package_main(const char* package_dir, bool is_esm) {
  JSRT_Debug("resolve_package_main: package_dir='%s', is_esm=%d", package_dir, is_esm);

  // Try to read package.json
  size_t package_json_path_len = strlen(package_dir) + 15;
  char* package_json_path = malloc(package_json_path_len);
  snprintf(package_json_path, package_json_path_len, "%s/package.json", package_dir);

  JSRT_ReadFileResult json_result = JSRT_ReadFile(package_json_path);
  free(package_json_path);

  if (json_result.error == JSRT_READ_FILE_OK) {
    // Parse package.json
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);

    JSValue package_json = JSRT_ParseJSON(ctx, json_result.data);
    JSRT_ReadFileResultFree(&json_result);

    if (!JS_IsNull(package_json) && !JS_IsException(package_json)) {
      const char* entry_point = NULL;

      if (is_esm) {
        // For ES modules, prefer "module" field over "main"
        entry_point = JSRT_GetPackageModule(ctx, package_json);
        if (!entry_point) {
          entry_point = JSRT_GetPackageMain(ctx, package_json);
        }
      } else {
        // For CommonJS, use "main" field
        entry_point = JSRT_GetPackageMain(ctx, package_json);
      }

      if (entry_point) {
        size_t full_path_len = strlen(package_dir) + strlen(entry_point) + 2;
        char* full_path = malloc(full_path_len);
        snprintf(full_path, full_path_len, "%s/%s", package_dir, entry_point);

        // Free the JSON parsing resources
        if (entry_point != JSRT_GetPackageMain(ctx, package_json)) {
          JS_FreeCString(ctx, entry_point);
        }
        JS_FreeCString(ctx, JSRT_GetPackageMain(ctx, package_json));
        JS_FreeValue(ctx, package_json);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);

        JSRT_Debug("resolve_package_main: resolved to '%s'", full_path);
        return full_path;
      }

      JS_FreeValue(ctx, package_json);
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
  } else {
    JSRT_ReadFileResultFree(&json_result);
  }

  // Fallback to index.js or index.mjs
  const char* default_file = is_esm ? "index.mjs" : "index.js";
  size_t default_path_len = strlen(package_dir) + strlen(default_file) + 2;
  char* default_path = malloc(default_path_len);
  snprintf(default_path, default_path_len, "%s/%s", package_dir, default_file);

  JSRT_Debug("resolve_package_main: falling back to '%s'", default_path);
  return default_path;
}

static char* resolve_npm_module(const char* module_name, const char* base_path, bool is_esm) {
  JSRT_Debug("resolve_npm_module: module_name='%s', base_path='%s', is_esm=%d", module_name,
             base_path ? base_path : "null", is_esm);

  // Determine the starting directory for resolution
  char* start_dir = NULL;
  if (base_path) {
    char* base_copy = strdup(base_path);
    char* last_slash = strrchr(base_copy, '/');
    if (last_slash) {
      *last_slash = '\0';
      start_dir = base_copy;
    } else {
      free(base_copy);
      start_dir = strdup(".");
    }
  } else {
    start_dir = strdup(".");
  }

  // Find the package directory
  char* package_dir = find_node_modules_path(start_dir, module_name);
  free(start_dir);

  if (!package_dir) {
    return NULL;
  }

  // Resolve the main entry point
  char* main_path = resolve_package_main(package_dir, is_esm);
  free(package_dir);

  return main_path;
}

static char* resolve_module_path(const char* module_name, const char* base_path) {
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
    char* base_dir = strdup(base_path);
    char* last_slash = strrchr(base_dir, '/');
    if (last_slash) {
      *last_slash = '\0';
    } else {
      strcpy(base_dir, ".");
    }

    // Build the resolved path
    size_t path_len = strlen(base_dir) + strlen(module_name) + 2;
    char* resolved_path = malloc(path_len);
    snprintf(resolved_path, path_len, "%s/%s", base_dir, module_name);

    free(base_dir);
    return resolved_path;
  }

  // Handle bare module names (npm packages) - try npm resolution first
  char* npm_path = resolve_npm_module(module_name, base_path, false);
  if (npm_path) {
    return npm_path;
  }

  // Fallback: treat as relative to current directory (original behavior)
  return strdup(module_name);
}

static char* try_extensions(const char* base_path) {
  const char* extensions[] = {".js", ".mjs", ""};
  size_t base_len = strlen(base_path);

  for (int i = 0; i < 3; i++) {
    const char* ext = extensions[i];
    size_t ext_len = strlen(ext);
    char* full_path = malloc(base_len + ext_len + 1);
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

char* JSRT_ModuleNormalize(JSContext* ctx, const char* module_base_name, const char* module_name, void* opaque) {
  JSRT_Debug("JSRT_ModuleNormalize: module_name='%s', module_base_name='%s'", module_name,
             module_base_name ? module_base_name : "null");

  // Handle jsrt: modules specially - don't try to resolve them as files
  if (strncmp(module_name, "jsrt:", 5) == 0) {
    return strdup(module_name);
  }

  // Check if this is a bare module name (not starting with . or /)
  if (module_name[0] != '.' && module_name[0] != '/') {
    // Try npm module resolution for ES modules
    char* npm_path = resolve_npm_module(module_name, module_base_name, true);
    if (npm_path) {
      JSRT_Debug("JSRT_ModuleNormalize: resolved npm module to '%s'", npm_path);
      return npm_path;
    }
  }

  // module_name is what we want to import, module_base_name is the importing module
  char* resolved_path = resolve_module_path(module_name, module_base_name);
  char* final_path = try_extensions(resolved_path);

  if (!final_path) {
    final_path = resolved_path;
  } else {
    free(resolved_path);
  }

  JSRT_Debug("JSRT_ModuleNormalize: resolved to '%s'", final_path);
  return final_path;
}

JSModuleDef* JSRT_ModuleLoader(JSContext* ctx, const char* module_name, void* opaque) {
  JSRT_Debug("JSRT_ModuleLoader: loading ES module '%s'", module_name);

  // Handle jsrt: modules
  if (strncmp(module_name, "jsrt:", 5) == 0) {
    const char* std_module = module_name + 5;  // Skip "jsrt:" prefix

    if (strcmp(std_module, "assert") == 0) {
      // Create jsrt:assert module with init function
      JSModuleDef* m = JS_NewCModule(ctx, module_name, js_std_assert_init);
      if (m) {
        JS_AddModuleExport(ctx, m, "default");
      }
      return m;
    }

    if (strcmp(std_module, "process") == 0) {
      // Create jsrt:process module with init function
      JSModuleDef* m = JS_NewCModule(ctx, module_name, js_std_process_init);
      if (m) {
        JS_AddModuleExport(ctx, m, "default");
      }
      return m;
    }

    if (strcmp(std_module, "ffi") == 0) {
      // Create jsrt:ffi module with init function
      JSModuleDef* m = JS_NewCModule(ctx, module_name, js_std_ffi_init);
      if (m) {
        JS_AddModuleExport(ctx, m, "default");
      }
      return m;
    }

    JS_ThrowReferenceError(ctx, "Unknown std module '%s'", std_module);
    return NULL;
  }

  // Load the file
  JSRT_ReadFileResult file_result = JSRT_ReadFile(module_name);
  if (file_result.error != JSRT_READ_FILE_OK) {
    JS_ThrowReferenceError(ctx, "could not load module filename '%s': %s", module_name,
                           JSRT_ReadFileErrorToString(file_result.error));
    JSRT_ReadFileResultFree(&file_result);
    return NULL;
  }

  // Always compile as ES module - the module loader is only for ES modules
  JSValue func_val =
      JS_Eval(ctx, file_result.data, file_result.size, module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

  JSRT_ReadFileResultFree(&file_result);

  if (JS_IsException(func_val)) {
    JSRT_Debug("JSRT_ModuleLoader: failed to compile module '%s'", module_name);
    return NULL;
  }

  // Get the module definition from the compiled function
  JSModuleDef* m = JS_VALUE_GET_PTR(func_val);
  JS_FreeValue(ctx, func_val);

  // Set up import.meta for the module
  JSValue meta_obj = JS_GetImportMeta(ctx, m);
  if (!JS_IsUndefined(meta_obj)) {
    // Set import.meta.url
    char* url = malloc(strlen(module_name) + 8);  // "file://" + module_name + null
    if (module_name[0] == '/') {
      // Absolute path
      snprintf(url, strlen(module_name) + 8, "file://%s", module_name);
    } else if (strstr(module_name, "://")) {
      // Already a URL
      strcpy(url, module_name);
    } else {
      // Relative path - make it absolute
      char cwd[1024];
      if (getcwd(cwd, sizeof(cwd))) {
        free(url);
        url = malloc(strlen(cwd) + strlen(module_name) + 9);  // "file://" + cwd + "/" + module_name + null
        snprintf(url, strlen(cwd) + strlen(module_name) + 9, "file://%s/%s", cwd, module_name);
      } else {
        snprintf(url, strlen(module_name) + 8, "file://%s", module_name);
      }
    }

    JS_SetPropertyStr(ctx, meta_obj, "url", JS_NewString(ctx, url));
    free(url);
    JS_FreeValue(ctx, meta_obj);
  }

  JSRT_Debug("JSRT_ModuleLoader: successfully loaded ES module '%s'", module_name);
  return m;
}

// Simple module cache for require()
typedef struct {
  char* name;
  JSValue exports;
} RequireModuleCache;

static RequireModuleCache* module_cache = NULL;
static size_t module_cache_size = 0;
static size_t module_cache_capacity = 0;

static JSValue get_cached_module(JSContext* ctx, const char* name) {
  for (size_t i = 0; i < module_cache_size; i++) {
    if (strcmp(module_cache[i].name, name) == 0) {
      return JS_DupValue(ctx, module_cache[i].exports);
    }
  }
  return JS_UNDEFINED;
}

static void cache_module(JSContext* ctx, const char* name, JSValue exports) {
  if (module_cache_size >= module_cache_capacity) {
    module_cache_capacity = module_cache_capacity == 0 ? 16 : module_cache_capacity * 2;
    module_cache = realloc(module_cache, module_cache_capacity * sizeof(RequireModuleCache));
  }

  module_cache[module_cache_size].name = strdup(name);
  module_cache[module_cache_size].exports = JS_DupValue(ctx, exports);
  module_cache_size++;
}

static JSValue js_require(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "require expects at least 1 argument");
  }

  const char* module_name = JS_ToCString(ctx, argv[0]);
  if (!module_name) {
    return JS_EXCEPTION;
  }

  JSRT_Debug("js_require: loading CommonJS module '%s'", module_name);

  // Handle jsrt: modules
  if (strncmp(module_name, "jsrt:", 5) == 0) {
    const char* std_module = module_name + 5;  // Skip "jsrt:" prefix

    if (strcmp(std_module, "assert") == 0) {
      JSValue result = JSRT_CreateAssertModule(ctx);
      JS_FreeCString(ctx, module_name);
      return result;
    }

    if (strcmp(std_module, "process") == 0) {
      JSValue result = JSRT_CreateProcessModule(ctx);
      JS_FreeCString(ctx, module_name);
      return result;
    }

    if (strcmp(std_module, "ffi") == 0) {
      JSValue result = JSRT_CreateFFIModule(ctx);
      JS_FreeCString(ctx, module_name);
      return result;
    }

    JS_FreeCString(ctx, module_name);
    return JS_ThrowReferenceError(ctx, "Unknown jsrt module '%s'", std_module);
  }

  // Check if this is a bare module name (npm package)
  char* resolved_path;
  if (module_name[0] != '.' && module_name[0] != '/') {
    // Try npm module resolution first
    resolved_path = resolve_npm_module(module_name, NULL, false);
    if (!resolved_path) {
      // Fallback to treating as relative path
      resolved_path = resolve_module_path(module_name, NULL);
    }
  } else {
    // Resolve relative/absolute paths normally
    resolved_path = resolve_module_path(module_name, NULL);
  }
  JSRT_Debug("js_require: resolved_path='%s'", resolved_path);
  char* final_path = try_extensions(resolved_path);
  JSRT_Debug("js_require: after try_extensions, final_path='%s'", final_path ? final_path : "NULL");

  if (!final_path) {
    final_path = resolved_path;
    JSRT_Debug("js_require: using resolved_path as final_path='%s'", final_path);
  } else {
    free(resolved_path);
    JSRT_Debug("js_require: freed resolved_path, using final_path='%s'", final_path);
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
  char* wrapper_code = malloc(wrapper_size);
  snprintf(wrapper_code, wrapper_size, "(function(exports, require, module, __filename, __dirname) {\n%s\n})",
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
      JS_DupValue(ctx, exports), require_func, JS_DupValue(ctx, module), JS_NewString(ctx, final_path),
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

  // Cache the module for future use
  cache_module(ctx, final_path, module_exports);

  JS_FreeCString(ctx, module_name);
  free(final_path);

  return module_exports;
}

void JSRT_StdModuleInit(JSRT_Runtime* rt) {
  JSRT_Debug("JSRT_StdModuleInit: initializing ES module loader");
  JS_SetModuleLoaderFunc(rt->rt, JSRT_ModuleNormalize, JSRT_ModuleLoader, rt);
}

void JSRT_StdCommonJSInit(JSRT_Runtime* rt) {
  JSRT_Debug("JSRT_StdCommonJSInit: initializing CommonJS support");

  JSValue global = JS_GetGlobalObject(rt->ctx);
  JS_SetPropertyStr(rt->ctx, global, "require", JS_NewCFunction(rt->ctx, js_require, "require", 1));
  JS_FreeValue(rt->ctx, global);
}

// Module cache cleanup function
void JSRT_StdModuleCleanup(JSContext* ctx) {
  if (module_cache) {
    for (size_t i = 0; i < module_cache_size; i++) {
      free(module_cache[i].name);
      if (ctx) {
        JS_FreeValue(ctx, module_cache[i].exports);
      }
    }
    free(module_cache);
    module_cache = NULL;
    module_cache_size = 0;
    module_cache_capacity = 0;
  }
}