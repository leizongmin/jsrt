/**
 * ES Module Loader Implementation
 */

#include "esm_loader.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../runtime.h"  // For JSRT_Runtime
#include "../core/module_cache.h"
#include "../protocols/protocol_dispatcher.h"
#include "../resolver/path_resolver.h"
#include "../util/module_debug.h"
#include "../util/module_errors.h"

#ifdef _WIN32
#include <windows.h>
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

/**
 * Convert file path to file:// URL
 */
char* jsrt_path_to_file_url(const char* path) {
  if (!path) {
    return NULL;
  }

  // Check if already a URL
  if (strstr(path, "://")) {
    return strdup(path);
  }

  size_t path_len = strlen(path);
  size_t url_len = path_len + 16;  // "file://" + path + extra for encoding
  char* url = malloc(url_len);
  if (!url) {
    return NULL;
  }

#ifdef _WIN32
  // Windows: Convert C:\path\to\file to file:///C:/path/to/file
  if (path_len >= 2 && isalpha(path[0]) && path[1] == ':') {
    // Drive letter path
    snprintf(url, url_len, "file:///%c:", toupper(path[0]));
    size_t offset = strlen(url);

    // Convert backslashes to forward slashes
    for (size_t i = 2; i < path_len && offset < url_len - 1; i++) {
      url[offset++] = (path[i] == '\\') ? '/' : path[i];
    }
    url[offset] = '\0';
  } else {
    // UNC path or relative path
    snprintf(url, url_len, "file://%s", path);
  }
#else
  // Unix: /path/to/file -> file:///path/to/file
  if (path[0] == '/') {
    snprintf(url, url_len, "file://%s", path);
  } else {
    snprintf(url, url_len, "file:///%s", path);
  }
#endif

  return url;
}

/**
 * import.meta.resolve() implementation
 */
static JSValue js_import_meta_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                      JSValue* func_data) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "import.meta.resolve() expects at least 1 argument");
  }

  const char* specifier = JS_ToCString(ctx, argv[0]);
  if (!specifier) {
    return JS_EXCEPTION;
  }

  // Get module path and loader from func_data
  if (!func_data || JS_IsUndefined(func_data[0]) || JS_IsUndefined(func_data[1])) {
    JS_FreeCString(ctx, specifier);
    return JS_ThrowInternalError(ctx, "Invalid import.meta.resolve() function data");
  }

  const char* module_path = JS_ToCString(ctx, func_data[0]);
  if (!module_path) {
    JS_FreeCString(ctx, specifier);
    return JS_EXCEPTION;
  }

  // Get loader pointer
  void* loader_ptr;
  int64_t loader_int64;
  if (JS_ToBigInt64(ctx, &loader_int64, func_data[1]) != 0) {
    JS_FreeCString(ctx, specifier);
    JS_FreeCString(ctx, module_path);
    return JS_ThrowInternalError(ctx, "Invalid loader pointer");
  }
  loader_ptr = (void*)(intptr_t)loader_int64;

  JSRT_ModuleLoader* loader = (JSRT_ModuleLoader*)loader_ptr;
  (void)loader;  // Use loader if needed

  MODULE_DEBUG_LOADER("import.meta.resolve('%s') from: %s", specifier, module_path);

  // Resolve the specifier using path resolver
  JSRT_ResolvedPath* resolved = jsrt_resolve_path(ctx, specifier, module_path, true);

  JS_FreeCString(ctx, specifier);
  JS_FreeCString(ctx, module_path);

  if (!resolved) {
    return JS_ThrowReferenceError(ctx, "Cannot resolve specifier");
  }

  // Convert to URL
  char* url = jsrt_path_to_file_url(resolved->resolved_path);
  jsrt_resolved_path_free(resolved);

  if (!url) {
    return JS_ThrowInternalError(ctx, "Failed to convert path to URL");
  }

  JSValue result = JS_NewString(ctx, url);
  free(url);

  return result;
}

/**
 * Create import.meta.resolve() function
 */
JSValue jsrt_create_import_meta_resolve(JSContext* ctx, JSRT_ModuleLoader* loader, const char* module_path) {
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
  JSValue resolve_func = JS_NewCFunctionData(ctx, js_import_meta_resolve, 1, 0, 2, func_data);

  return resolve_func;
}

/**
 * Set up import.meta for module
 */
int jsrt_setup_import_meta(JSContext* ctx, JSModuleDef* module, JSRT_ModuleLoader* loader, const char* resolved_path) {
  if (!ctx || !module || !loader || !resolved_path) {
    return -1;
  }

  MODULE_DEBUG_LOADER("Setting up import.meta for: %s", resolved_path);

  // Get import.meta object
  JSValue meta_obj = JS_GetImportMeta(ctx, module);
  if (JS_IsUndefined(meta_obj)) {
    MODULE_DEBUG_ERROR("Failed to get import.meta object");
    return -1;
  }

  // Set import.meta.url
  char* url = jsrt_path_to_file_url(resolved_path);
  if (!url) {
    JS_FreeValue(ctx, meta_obj);
    return -1;
  }

  JS_SetPropertyStr(ctx, meta_obj, "url", JS_NewString(ctx, url));
  free(url);

  // TODO: Set import.meta.resolve function
  // Temporarily skip this to avoid potential issues with function data conversion
  // JSValue resolve_func = jsrt_create_import_meta_resolve(ctx, loader, resolved_path);
  // if (JS_IsException(resolve_func)) {
  //   JS_FreeValue(ctx, meta_obj);
  //   return -1;
  // }
  // JS_SetPropertyStr(ctx, meta_obj, "resolve", resolve_func);

  JS_FreeValue(ctx, meta_obj);

  MODULE_DEBUG_LOADER("import.meta setup complete for: %s", resolved_path);
  return 0;
}

/**
 * Load ES module
 */
JSModuleDef* jsrt_load_esm_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* resolved_path,
                                  const char* specifier) {
  if (!ctx || !loader || !resolved_path) {
    MODULE_DEBUG_ERROR("Invalid arguments to jsrt_load_esm_module");
    return NULL;
  }

  MODULE_DEBUG_LOADER("=== Loading ES module: %s ===", resolved_path);

  // Note: ES modules are cached by QuickJS itself via JSModuleDef*
  // We don't need to duplicate that caching here

  // Load module content via protocol dispatcher
  JSRT_ReadFileResult file_result = jsrt_load_content_by_protocol(resolved_path);
  if (file_result.error != JSRT_READ_FILE_OK) {
    jsrt_module_throw_error(ctx, JSRT_MODULE_LOAD_FAILED, "Failed to load module '%s': %s",
                            specifier ? specifier : resolved_path, JSRT_ReadFileErrorToString(file_result.error));
    return NULL;
  }

  MODULE_DEBUG_LOADER("Loaded content for %s (%zu bytes)", resolved_path, file_result.size);

  // Compile as ES module
  JSValue func_val =
      JS_Eval(ctx, file_result.data, file_result.size, resolved_path, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

  JSRT_ReadFileResultFree(&file_result);

  if (JS_IsException(func_val)) {
    MODULE_DEBUG_ERROR("Failed to compile ES module: %s", resolved_path);
    return NULL;
  }

  // Get the module definition from the compiled function
  // When JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY is used,
  // QuickJS returns a special value containing the JSModuleDef pointer
  JSModuleDef* module = (JSModuleDef*)JS_VALUE_GET_PTR(func_val);
  if (!module) {
    MODULE_DEBUG_ERROR("Failed to extract module definition: %s", resolved_path);
    JS_FreeValue(ctx, func_val);
    return NULL;
  }

  // Free the function value after extracting the module pointer
  // The module is now owned by QuickJS's module system
  JS_FreeValue(ctx, func_val);

  // Set up import.meta
  if (jsrt_setup_import_meta(ctx, module, loader, resolved_path) != 0) {
    MODULE_DEBUG_ERROR("Failed to setup import.meta for: %s", resolved_path);
    return NULL;
  }

  MODULE_DEBUG_LOADER("Successfully loaded ES module: %s", resolved_path);
  return module;
}

/**
 * Get exports from ES module
 */
JSValue jsrt_get_esm_exports(JSContext* ctx, JSModuleDef* module) {
  if (!ctx || !module) {
    return JS_EXCEPTION;
  }

  MODULE_DEBUG_LOADER("Getting exports from ES module");

  // Get the module namespace object which contains all exports
  // This is the standard way to access ES module exports in QuickJS
  JSValue ns = JS_GetModuleNamespace(ctx, module);

  if (JS_IsException(ns)) {
    MODULE_DEBUG_ERROR("Failed to get module namespace");
    return JS_EXCEPTION;
  }

  MODULE_DEBUG_LOADER("Successfully retrieved ES module exports");
  return ns;
}

/**
 * QuickJS module normalize callback (bridge to new system)
 *
 * This function is called by QuickJS to normalize (resolve) ES module specifiers.
 * It bridges the old JSRT_StdModuleNormalize API to the new path resolver.
 */
// Forward declaration for node module check
#ifdef JSRT_NODE_COMPAT
extern bool JSRT_IsNodeModule(const char* name);
extern bool is_absolute_path(const char* path);
extern bool is_relative_path(const char* path);
#endif

char* jsrt_esm_normalize_callback(JSContext* ctx, const char* module_base_name, const char* module_name, void* opaque) {
  MODULE_DEBUG_RESOLVER("=== ESM Normalize: '%s' from base '%s' ===", module_name,
                        module_base_name ? module_base_name : "null");

  if (!module_name) {
    MODULE_DEBUG_ERROR("Module name is NULL");
    return NULL;
  }

#ifdef JSRT_NODE_COMPAT
  // Compact Node mode: check bare name as node module for ES modules
  JSRT_Runtime* rt = (JSRT_Runtime*)opaque;
  if (rt && rt->compact_node_mode && !is_absolute_path(module_name) && !is_relative_path(module_name) &&
      JSRT_IsNodeModule(module_name)) {
    MODULE_DEBUG_RESOLVER("Compact Node mode (ESM): resolving '%s' as 'node:%s'", module_name, module_name);
    char* prefixed = malloc(strlen(module_name) + 6);
    if (prefixed) {
      sprintf(prefixed, "node:%s", module_name);
      return prefixed;
    }
  }
#endif

  // Use the new path resolver
  JSRT_ResolvedPath* resolved = jsrt_resolve_path(ctx, module_name, module_base_name, true);

  if (!resolved || !resolved->resolved_path) {
    MODULE_DEBUG_ERROR("Failed to resolve ES module: %s", module_name);
    if (resolved) {
      jsrt_resolved_path_free(resolved);
    }
    return NULL;
  }

  // Extract the resolved path
  char* result = strdup(resolved->resolved_path);
  jsrt_resolved_path_free(resolved);

  MODULE_DEBUG_RESOLVER("Resolved '%s' to '%s'", module_name, result);
  return result;
}

// Include standard module header for builtin module init functions
#include "../module.h"

// Forward declarations for HTTP and Node modules
#ifdef JSRT_NODE_COMPAT
extern JSModuleDef* JSRT_LoadNodeModule(JSContext* ctx, const char* module_name);
#endif
extern JSModuleDef* jsrt_load_http_module(JSContext* ctx, const char* module_name);
extern bool jsrt_is_http_url(const char* url);

/**
 * QuickJS module loader callback (bridge to new system)
 *
 * This function is called by QuickJS to load ES modules.
 * It bridges the old JSRT_StdModuleLoader API to the new ES module loader.
 */
JSModuleDef* jsrt_esm_loader_callback(JSContext* ctx, const char* module_name, void* opaque) {
  MODULE_DEBUG_LOADER("=== ESM Loader: '%s' ===", module_name);

  if (!module_name) {
    MODULE_DEBUG_ERROR("Module name is NULL");
    return NULL;
  }

  // Handle jsrt: builtin modules
  if (strncmp(module_name, "jsrt:", 5) == 0) {
    const char* std_module = module_name + 5;  // Skip "jsrt:" prefix

    if (strcmp(std_module, "assert") == 0) {
      JSModuleDef* m = JS_NewCModule(ctx, module_name, js_std_assert_init);
      if (m) {
        JS_AddModuleExport(ctx, m, "default");
      }
      return m;
    }

    if (strcmp(std_module, "process") == 0) {
      JSModuleDef* m = JS_NewCModule(ctx, module_name, js_std_process_module_init);
      if (m) {
        JS_AddModuleExport(ctx, m, "default");
      }
      return m;
    }

    if (strcmp(std_module, "ffi") == 0) {
      JSModuleDef* m = JS_NewCModule(ctx, module_name, js_std_ffi_init);
      if (m) {
        JS_AddModuleExport(ctx, m, "default");
      }
      return m;
    }

    JS_ThrowReferenceError(ctx, "Unknown std module '%s'", std_module);
    return NULL;
  }

#ifdef JSRT_NODE_COMPAT
  // Handle node: modules
  if (strncmp(module_name, "node:", 5) == 0) {
    const char* node_module = module_name + 5;  // Skip "node:" prefix
    return JSRT_LoadNodeModule(ctx, node_module);
  }
#endif

  // Handle HTTP/HTTPS modules
  if (jsrt_is_http_url(module_name)) {
    return jsrt_load_http_module(ctx, module_name);
  }

  // Get the module loader from runtime context
  // opaque should be JSRT_Runtime*
  JSRT_Runtime* rt = (JSRT_Runtime*)opaque;
  if (!rt || !rt->module_loader) {
    MODULE_DEBUG_ERROR("Module loader not available");
    return NULL;
  }

  JSRT_ModuleLoader* loader = rt->module_loader;

  // Load the module using new ES module loader
  JSModuleDef* module = jsrt_load_esm_module(ctx, loader, module_name, module_name);

  if (!module) {
    MODULE_DEBUG_ERROR("Failed to load ES module: %s", module_name);
    return NULL;
  }

  MODULE_DEBUG_LOADER("Successfully loaded ES module: %s", module_name);
  return module;
}
