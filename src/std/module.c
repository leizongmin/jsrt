#include "module.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>  // for _getcwd
#include <io.h>      // for _access
#include <windows.h>
#define F_OK 0
#define access _access
#define getcwd _getcwd
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#define PATH_DELIMITER ';'
// Windows implementation of realpath using _fullpath
static char* jsrt_realpath(const char* path, char* resolved_path) {
  return _fullpath(resolved_path, path, _MAX_PATH);
}
#else
#include <unistd.h>
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#define PATH_DELIMITER ':'
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

// ==== Cross-platform Path Handling Functions ====

// Check if a character is a path separator (/ or \)
static bool is_path_separator(char c) {
  return c == '/' || c == '\\';
}

// Find the last path separator in a string
static char* find_last_separator(const char* path) {
  if (!path)
    return NULL;

  char* last_slash = strrchr(path, '/');
  char* last_backslash = strrchr(path, '\\');

  if (last_slash && last_backslash) {
    return (last_slash > last_backslash) ? last_slash : last_backslash;
  } else if (last_slash) {
    return last_slash;
  } else {
    return last_backslash;
  }
}

// Normalize path separators to be platform-specific
static char* normalize_path(const char* path) {
  if (!path)
    return NULL;

  char* normalized = strdup(path);
  if (!normalized)
    return NULL;

  // Convert path separators to platform-specific ones
  for (char* p = normalized; *p; p++) {
#ifdef _WIN32
    if (*p == '/')
      *p = '\\';
#else
    if (*p == '\\')
      *p = '/';
#endif
  }

  return normalized;
}

// Get the parent directory of a path
static char* get_parent_directory(const char* path) {
  if (!path)
    return NULL;

  char* normalized = normalize_path(path);
  if (!normalized)
    return NULL;

  char* last_sep = find_last_separator(normalized);
  if (last_sep && last_sep != normalized) {
    *last_sep = '\0';
    return normalized;
  } else if (last_sep == normalized) {
    // Path is at root
    normalized[1] = '\0';
    return normalized;
  } else {
    // No separator found, return current directory
    free(normalized);
    return strdup(".");
  }
}

// Join two path components with appropriate separator
static char* path_join(const char* dir, const char* file) {
  if (!dir || !file)
    return NULL;

  size_t dir_len = strlen(dir);
  size_t file_len = strlen(file);

  // Check if dir already ends with separator
  bool has_trailing_sep = (dir_len > 0 && is_path_separator(dir[dir_len - 1]));

  size_t total_len = dir_len + file_len + (has_trailing_sep ? 0 : 1) + 1;
  char* result = malloc(total_len);
  if (!result)
    return NULL;

  strcpy(result, dir);
  if (!has_trailing_sep) {
    strcat(result, PATH_SEPARATOR_STR);
  }
  strcat(result, file);

  char* normalized = normalize_path(result);
  free(result);
  return normalized;
}

// Check if a path is absolute
static bool is_absolute_path(const char* path) {
  if (!path)
    return false;

#ifdef _WIN32
  // Windows: starts with drive letter (C:) or UNC path (\\) or single slash/backslash
  return (strlen(path) >= 3 && path[1] == ':' && is_path_separator(path[2])) ||
         (strlen(path) >= 2 && path[0] == '\\' && path[1] == '\\') || is_path_separator(path[0]);
#else
  // Unix: starts with /
  return path[0] == '/';
#endif
}

// Check if a path is relative (starts with ./ or ../)
static bool is_relative_path(const char* path) {
  if (!path)
    return false;

  return (path[0] == '.' && (is_path_separator(path[1]) || (path[1] == '.' && is_path_separator(path[2]))));
}

// Resolve a relative path against a base path
static char* resolve_relative_path(const char* base_path, const char* relative_path) {
  if (!base_path || !relative_path)
    return NULL;

  char* base_dir = get_parent_directory(base_path);
  if (!base_dir)
    return NULL;

  // Handle ./ and ../ prefixes
  const char* clean_relative = relative_path;
  if (relative_path[0] == '.' && is_path_separator(relative_path[1])) {
    // Skip "./" prefix
    clean_relative = relative_path + 2;
  } else if (relative_path[0] == '.' && relative_path[1] == '.' && is_path_separator(relative_path[2])) {
    // Handle "../" - need to go up one more level
    char* parent_dir = get_parent_directory(base_dir);
    free(base_dir);
    if (!parent_dir)
      return NULL;
    base_dir = parent_dir;
    clean_relative = relative_path + 3;

    // Handle multiple "../" sequences
    while (clean_relative[0] == '.' && clean_relative[1] == '.' && is_path_separator(clean_relative[2])) {
      parent_dir = get_parent_directory(base_dir);
      free(base_dir);
      if (!parent_dir)
        return NULL;
      base_dir = parent_dir;
      clean_relative += 3;
    }
  }

  char* result = path_join(base_dir, clean_relative);
  free(base_dir);

  return result;
}

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
  char* normalized_current = normalize_path(current_dir);
  free(current_dir);

  char* normalized_dir = jsrt_realpath(normalized_current, NULL);
  free(normalized_current);

  if (!normalized_dir) {
    return NULL;
  }

  // Normalize the resolved path as well
  char* search_dir = normalize_path(normalized_dir);
  free(normalized_dir);

  if (!search_dir) {
    return NULL;
  }

  // Walk up the directory tree looking for node_modules
  char* current_search = search_dir;
  while (current_search && strlen(current_search) > 1) {
    // Build node_modules path
    char* node_modules_path = path_join(current_search, "node_modules");
    if (!node_modules_path)
      break;

    // Build package path
    char* package_path = path_join(node_modules_path, package_name);
    free(node_modules_path);

    if (!package_path)
      break;

    // Check if the package directory exists using access()
    if (access(package_path, F_OK) == 0) {
      JSRT_Debug("find_node_modules_path: found package at '%s'", package_path);
      free(search_dir);
      return package_path;
    }
    free(package_path);

    // Move up one directory using our path helper
    char* parent = get_parent_directory(current_search);
    if (!parent || strcmp(parent, current_search) == 0) {
      free(parent);
      break;
    }

    // Update current_search to parent (reuse the same buffer)
    strcpy(current_search, parent);
    free(parent);
  }

  free(search_dir);
  JSRT_Debug("find_node_modules_path: package '%s' not found", package_name);
  return NULL;
}

static char* resolve_package_main(const char* package_dir, bool is_esm) {
  JSRT_Debug("resolve_package_main: package_dir='%s', is_esm=%d", package_dir, is_esm);

  // Try to read package.json
  char* package_json_path = path_join(package_dir, "package.json");
  if (!package_json_path)
    return NULL;

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
        char* full_path = path_join(package_dir, entry_point);

        // Free the JSON parsing resources
        if (entry_point != JSRT_GetPackageMain(ctx, package_json)) {
          JS_FreeCString(ctx, entry_point);
        }
        JS_FreeCString(ctx, JSRT_GetPackageMain(ctx, package_json));
        JS_FreeValue(ctx, package_json);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);

        JSRT_Debug("resolve_package_main: resolved to '%s'", full_path ? full_path : "NULL");
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
  char* default_path = path_join(package_dir, default_file);

  JSRT_Debug("resolve_package_main: falling back to '%s'", default_path ? default_path : "NULL");
  return default_path;
}

static char* resolve_npm_module(const char* module_name, const char* base_path, bool is_esm) {
  JSRT_Debug("resolve_npm_module: module_name='%s', base_path='%s', is_esm=%d", module_name,
             base_path ? base_path : "null", is_esm);

  // Determine the starting directory for resolution
  char* start_dir = NULL;
  if (base_path) {
    start_dir = get_parent_directory(base_path);
    if (!start_dir) {
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
  if (is_absolute_path(module_name)) {
    return normalize_path(module_name);
  }

  // Handle relative paths
  if (is_relative_path(module_name)) {
    if (!base_path) {
      return normalize_path(module_name);
    }
    return resolve_relative_path(base_path, module_name);
  }

  // Handle bare module names (npm packages) - try npm resolution first
  char* npm_path = resolve_npm_module(module_name, base_path, false);
  if (npm_path) {
    return npm_path;
  }

  // Fallback: treat as relative to current directory (original behavior)
  return normalize_path(module_name);
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

  // Check if this is a bare module name (not absolute or relative)
  if (!is_absolute_path(module_name) && !is_relative_path(module_name)) {
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

  // Ensure final normalization
  char* normalized_final = normalize_path(final_path);
  free(final_path);

  JSRT_Debug("JSRT_ModuleNormalize: resolved to '%s'", normalized_final);
  return normalized_final;
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

  // Handle node: modules (Node.js compatibility)
  if (strncmp(module_name, "node:", 5) == 0) {
    const char* node_module = module_name + 5;  // Skip "node:" prefix

    if (strcmp(node_module, "path") == 0) {
      // Create node:path module with init function
      JSModuleDef* m = JS_NewCModule(ctx, module_name, js_node_path_init);
      if (m) {
        JS_AddModuleExport(ctx, m, "default");
        JS_AddModuleExport(ctx, m, "join");
        JS_AddModuleExport(ctx, m, "resolve");
        JS_AddModuleExport(ctx, m, "normalize");
        JS_AddModuleExport(ctx, m, "dirname");
        JS_AddModuleExport(ctx, m, "basename");
        JS_AddModuleExport(ctx, m, "extname");
        JS_AddModuleExport(ctx, m, "relative");
        JS_AddModuleExport(ctx, m, "isAbsolute");
        JS_AddModuleExport(ctx, m, "sep");
        JS_AddModuleExport(ctx, m, "delimiter");
      }
      return m;
    }

    JS_ThrowReferenceError(ctx, "Unknown node module '%s'", node_module);
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

  // Check if this looks like a CommonJS module
  bool is_commonjs = false;
  if (strstr(file_result.data, "module.exports") != NULL || strstr(file_result.data, "exports.") != NULL ||
      strstr(file_result.data, "exports[") != NULL) {
    is_commonjs = true;
  }

  JSModuleDef* m;
  if (is_commonjs) {
    JSRT_Debug("JSRT_ModuleLoader: detected CommonJS module, wrapping as ES module for '%s'", module_name);

    // Create a synthetic ES module
    char* wrapper_code;
    size_t wrapper_size = strlen(file_result.data) + 1024;  // Increased buffer
    wrapper_code = malloc(wrapper_size);

    // Create a simple wrapper that uses the global require with proper context
    // First, escape backslashes in the module_name for JavaScript string literal
    size_t escaped_name_len = strlen(module_name) * 2 + 1;  // Worst case: every char needs escaping
    char* escaped_module_name = malloc(escaped_name_len);
    char* dst = escaped_module_name;
    for (const char* src = module_name; *src; src++) {
      if (*src == '\\') {
        *dst++ = '\\';
        *dst++ = '\\';
      } else if (*src == '"') {
        *dst++ = '\\';
        *dst++ = '"';
      } else {
        *dst++ = *src;
      }
    }
    *dst = '\0';

    snprintf(wrapper_code, wrapper_size,
             "// Auto-generated ES module wrapper for CommonJS module\n"
             "const __cjs_module = { exports: {} };\n"
             "const __cjs_exports = __cjs_module.exports;\n"
             "const __cjs_filename = \"%s\";\n"
             "const __cjs_dirname = (() => {\n"
             "  const lastSlash = __cjs_filename.lastIndexOf('/');\n"
             "  const lastBackslash = __cjs_filename.lastIndexOf('\\\\');\n"
             "  const lastSep = Math.max(lastSlash, lastBackslash);\n"
             "  return lastSep > 0 ? __cjs_filename.substring(0, lastSep) : '.';\n"
             "})();\n"
             "\n"
             "// Set context for require resolution\n"
             "globalThis.__esm_module_context = __cjs_filename;\n"
             "\n"
             "// CommonJS module code starts here\n"
             "(function(exports, require, module, __filename, __dirname) {\n"
             "%s\n"
             "})(__cjs_exports, globalThis.require, __cjs_module, __cjs_filename, __cjs_dirname);\n"
             "\n"
             "// Clear context\n"
             "globalThis.__esm_module_context = undefined;\n"
             "\n"
             "// ES module exports\n"
             "export default __cjs_module.exports;\n",
             escaped_module_name, file_result.data);

    free(escaped_module_name);

    JSValue func_val =
        JS_Eval(ctx, wrapper_code, strlen(wrapper_code), module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    free(wrapper_code);

    if (JS_IsException(func_val)) {
      JSRT_Debug("JSRT_ModuleLoader: failed to compile CommonJS wrapper for '%s'", module_name);
      JSRT_ReadFileResultFree(&file_result);
      return NULL;
    }

    m = JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);

  } else {
    // Compile as ES module - the module loader is only for ES modules
    JSValue func_val =
        JS_Eval(ctx, file_result.data, file_result.size, module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

    if (JS_IsException(func_val)) {
      JSRT_Debug("JSRT_ModuleLoader: failed to compile ES module '%s'", module_name);
      JSRT_ReadFileResultFree(&file_result);
      return NULL;
    }

    // Get the module definition from the compiled function
    m = JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);
  }

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

// Module cache for require()
typedef struct {
  char* name;
  JSValue exports;
} RequireModuleCache;

static RequireModuleCache* module_cache = NULL;
static size_t module_cache_size = 0;
static size_t module_cache_capacity = 0;

// Current module path context for relative require resolution
static char* current_module_path = NULL;

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

  // Check for ES module context
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue esm_context_prop = JS_GetPropertyStr(ctx, global, "__esm_module_context");
  char* esm_context_path = NULL;
  if (!JS_IsUndefined(esm_context_prop) && !JS_IsNull(esm_context_prop)) {
    const char* path_str = JS_ToCString(ctx, esm_context_prop);
    if (path_str) {
      esm_context_path = strdup(path_str);
      JS_FreeCString(ctx, path_str);
    }
  }
  JS_FreeValue(ctx, esm_context_prop);
  JS_FreeValue(ctx, global);

  // Use ES module context if available, otherwise current_module_path
  const char* effective_module_path = esm_context_path ? esm_context_path : current_module_path;

  // Handle jsrt: modules
  if (strncmp(module_name, "jsrt:", 5) == 0) {
    const char* std_module = module_name + 5;  // Skip "jsrt:" prefix

    if (strcmp(std_module, "assert") == 0) {
      JSValue result = JSRT_CreateAssertModule(ctx);
      JS_FreeCString(ctx, module_name);
      free(esm_context_path);
      return result;
    }

    if (strcmp(std_module, "process") == 0) {
      JSValue result = JSRT_CreateProcessModule(ctx);
      JS_FreeCString(ctx, module_name);
      free(esm_context_path);
      return result;
    }

    if (strcmp(std_module, "ffi") == 0) {
      JSValue result = JSRT_CreateFFIModule(ctx);
      JS_FreeCString(ctx, module_name);
      free(esm_context_path);
      return result;
    }

    JS_FreeCString(ctx, module_name);
    free(esm_context_path);
    return JS_ThrowReferenceError(ctx, "Unknown jsrt module '%s'", std_module);
  }

  // Handle node: modules (Node.js compatibility)
  if (strncmp(module_name, "node:", 5) == 0) {
    const char* node_module = module_name + 5;  // Skip "node:" prefix

    if (strcmp(node_module, "path") == 0) {
      JSValue result = JSRT_CreateNodePathModule(ctx);
      JS_FreeCString(ctx, module_name);
      free(esm_context_path);
      return result;
    }

    JS_FreeCString(ctx, module_name);
    free(esm_context_path);
    return JS_ThrowReferenceError(ctx, "Unknown node module '%s'", node_module);
  }

  // Check if this is a bare module name (npm package)
  char* resolved_path;

  if (!is_relative_path(module_name) && !is_absolute_path(module_name)) {
    // Try npm module resolution first
    JSRT_Debug("js_require: trying npm module resolution");
    resolved_path = resolve_npm_module(module_name, effective_module_path, false);
    if (!resolved_path) {
      // Fallback to treating as relative path
      JSRT_Debug("js_require: npm resolution failed, falling back to module path resolution");
      resolved_path = resolve_module_path(module_name, effective_module_path);
    }
  } else {
    // Resolve relative/absolute paths normally
    JSRT_Debug("js_require: resolving relative/absolute path");
    resolved_path = resolve_module_path(module_name, effective_module_path);
  }
  char* final_path = try_extensions(resolved_path);

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
    free(esm_context_path);
    return cached;
  }

  // Load and evaluate the file
  JSRT_ReadFileResult file_result = JSRT_ReadFile(final_path);
  if (file_result.error != JSRT_READ_FILE_OK) {
    JS_FreeCString(ctx, module_name);
    free(final_path);
    free(esm_context_path);
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
    free(esm_context_path);
    JS_FreeValue(ctx, module);
    JS_FreeValue(ctx, exports);
    return func;
  }

  // Call the wrapper function with CommonJS arguments
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue require_func = JS_GetPropertyStr(ctx, global_obj, "require");

  // Calculate __dirname as the directory containing the module file
  char* dirname_str = get_parent_directory(final_path);
  if (!dirname_str) {
    dirname_str = strdup(".");
  }

  JSValue args[5] = {JS_DupValue(ctx, exports), require_func, JS_DupValue(ctx, module), JS_NewString(ctx, final_path),
                     JS_NewString(ctx, dirname_str)};

  free(dirname_str);

  // Set current module path context for relative require resolution
  char* old_module_path = current_module_path;
  current_module_path = strdup(final_path);

  JSValue result = JS_Call(ctx, func, global_obj, 5, args);

  // Restore previous module path context
  free(current_module_path);
  current_module_path = old_module_path;

  // Clean up
  JSRT_ReadFileResultFree(&file_result);
  JS_FreeValue(ctx, func);
  JS_FreeValue(ctx, global_obj);
  for (int i = 0; i < 5; i++) {
    JS_FreeValue(ctx, args[i]);
  }

  if (JS_IsException(result)) {
    JS_FreeCString(ctx, module_name);
    free(final_path);
    free(esm_context_path);
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
  free(esm_context_path);

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

// ==== Node.js Path Module Implementation ====

// Helper function to get the current working directory
static char* get_current_directory() {
  char* cwd = getcwd(NULL, 0);  // Let getcwd allocate the buffer
  if (!cwd) {
    return strdup(".");  // Fallback to current directory
  }
  return cwd;
}

// node:path.join - Join path segments using platform-specific separator
static JSValue js_node_path_join(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc == 0) {
    return JS_NewString(ctx, ".");
  }

  size_t total_len = 0;
  char** segments = malloc(argc * sizeof(char*));

  // Get all string arguments and calculate total length
  for (int i = 0; i < argc; i++) {
    segments[i] = (char*)JS_ToCString(ctx, argv[i]);
    if (!segments[i]) {
      // Cleanup on error
      for (int j = 0; j < i; j++) {
        JS_FreeCString(ctx, segments[j]);
      }
      free(segments);
      return JS_EXCEPTION;
    }
    total_len += strlen(segments[i]) + 1;  // +1 for separator
  }

  // Allocate result buffer
  char* result = malloc(total_len + 1);
  if (!result) {
    for (int i = 0; i < argc; i++) {
      JS_FreeCString(ctx, segments[i]);
    }
    free(segments);
    return JS_ThrowOutOfMemory(ctx);
  }

  result[0] = '\0';
  bool first = true;

  // Join all segments
  for (int i = 0; i < argc; i++) {
    if (strlen(segments[i]) > 0) {  // Skip empty segments
      if (!first && !is_path_separator(result[strlen(result) - 1])) {
        strcat(result, PATH_SEPARATOR_STR);
      }
      strcat(result, segments[i]);
      first = false;
    }
    JS_FreeCString(ctx, segments[i]);
  }
  free(segments);

  if (result[0] == '\0') {
    strcpy(result, ".");
  }

  // Normalize the result
  char* normalized = normalize_path(result);
  free(result);

  JSValue ret = JS_NewString(ctx, normalized);
  free(normalized);
  return ret;
}

// node:path.resolve - Resolve path segments to an absolute path
static JSValue js_node_path_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  char* current = get_current_directory();

  for (int i = 0; i < argc; i++) {
    const char* segment = JS_ToCString(ctx, argv[i]);
    if (!segment) {
      free(current);
      return JS_EXCEPTION;
    }

    if (strlen(segment) > 0) {
      if (is_absolute_path(segment)) {
        // Replace current with absolute segment
        free(current);
        current = strdup(segment);
      } else {
        // Join relative segment to current
        char* temp = path_join(current, segment);
        free(current);
        current = temp;
      }
    }

    JS_FreeCString(ctx, segment);
  }

  if (!current) {
    current = get_current_directory();
  }

  // Normalize the final result
  char* normalized = normalize_path(current);
  free(current);

  JSValue ret = JS_NewString(ctx, normalized);
  free(normalized);
  return ret;
}

// node:path.normalize - Normalize a path, removing duplicate separators and resolving . and ..
static JSValue js_node_path_normalize(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path.normalize requires a path argument");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // First normalize separators
  char* normalized = normalize_path(path);
  JS_FreeCString(ctx, path);

  if (!normalized) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Handle . and .. resolution
  char* segments[256];  // Support up to 256 path segments
  int segment_count = 0;

  // Split path into segments
  char* path_copy = strdup(normalized);
  char* token = strtok(path_copy, PATH_SEPARATOR_STR);

  // Track whether path was absolute
  bool is_absolute = is_absolute_path(normalized);

  while (token && segment_count < 256) {
    if (strcmp(token, ".") == 0) {
      // Skip current directory references
      continue;
    } else if (strcmp(token, "..") == 0) {
      // Go up one directory
      if (segment_count > 0 && strcmp(segments[segment_count - 1], "..") != 0) {
        // Remove the last segment
        free(segments[segment_count - 1]);
        segment_count--;
      } else if (!is_absolute) {
        // For relative paths, keep .. if we can't go up further
        segments[segment_count] = strdup("..");
        segment_count++;
      }
      // For absolute paths, .. at root is ignored
    } else {
      // Regular segment
      segments[segment_count] = strdup(token);
      segment_count++;
    }
    token = strtok(NULL, PATH_SEPARATOR_STR);
  }

  free(path_copy);

  // Rebuild the path
  size_t total_len = 0;
  for (int i = 0; i < segment_count; i++) {
    total_len += strlen(segments[i]) + 1;  // +1 for separator
  }
  total_len += is_absolute ? 1 : 0;  // +1 for leading separator if absolute

  char* result = malloc(total_len + 1);
  if (!result) {
    // Cleanup segments
    for (int i = 0; i < segment_count; i++) {
      free(segments[i]);
    }
    free(normalized);
    return JS_ThrowOutOfMemory(ctx);
  }

  result[0] = '\0';

  if (is_absolute) {
    strcat(result, PATH_SEPARATOR_STR);
  }

  for (int i = 0; i < segment_count; i++) {
    if (i > 0 || is_absolute) {
      strcat(result, PATH_SEPARATOR_STR);
    }
    strcat(result, segments[i]);
    free(segments[i]);
  }

  // Handle empty result
  if (result[0] == '\0') {
    strcpy(result, ".");
  }

  free(normalized);

  JSValue ret = JS_NewString(ctx, result);
  free(result);
  return ret;
}

// node:path.dirname - Get directory name of a path
static JSValue js_node_path_dirname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path.dirname requires a path argument");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  char* dirname_result = get_parent_directory(path);
  JS_FreeCString(ctx, path);

  if (!dirname_result) {
    return JS_NewString(ctx, ".");
  }

  JSValue ret = JS_NewString(ctx, dirname_result);
  free(dirname_result);
  return ret;
}

// node:path.basename - Get base name of a path
static JSValue js_node_path_basename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path.basename requires a path argument");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  const char* ext = NULL;
  if (argc >= 2) {
    ext = JS_ToCString(ctx, argv[1]);
    if (!ext) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
  }

  // Find the basename
  const char* last_sep = find_last_separator(path);
  const char* basename_start = last_sep ? last_sep + 1 : path;

  // Remove extension if provided
  char* basename = strdup(basename_start);
  if (ext && strlen(ext) > 0) {
    size_t basename_len = strlen(basename);
    size_t ext_len = strlen(ext);
    if (basename_len >= ext_len && strcmp(basename + basename_len - ext_len, ext) == 0) {
      basename[basename_len - ext_len] = '\0';
    }
  }

  JS_FreeCString(ctx, path);
  if (ext)
    JS_FreeCString(ctx, ext);

  JSValue ret = JS_NewString(ctx, basename);
  free(basename);
  return ret;
}

// node:path.extname - Get extension of a path
static JSValue js_node_path_extname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path.extname requires a path argument");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Find last dot after last separator
  const char* last_sep = find_last_separator(path);
  const char* search_start = last_sep ? last_sep + 1 : path;
  const char* last_dot = strrchr(search_start, '.');

  JSValue ret;
  if (last_dot && last_dot != search_start) {
    ret = JS_NewString(ctx, last_dot);
  } else {
    ret = JS_NewString(ctx, "");
  }

  JS_FreeCString(ctx, path);
  return ret;
}

// node:path.isAbsolute - Check if a path is absolute
static JSValue js_node_path_is_absolute(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  bool absolute = is_absolute_path(path);
  JS_FreeCString(ctx, path);

  return JS_NewBool(ctx, absolute);
}

// node:path.relative - Get relative path from one path to another
static JSValue js_node_path_relative(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "path.relative requires from and to arguments");
  }

  // For now, return empty string - TODO: implement proper relative path calculation
  return JS_NewString(ctx, "");
}

// Create the Node.js path module object
JSValue JSRT_CreateNodePathModule(JSContext* ctx) {
  JSValue path_obj = JS_NewObject(ctx);

  // Add methods
  JS_SetPropertyStr(ctx, path_obj, "join", JS_NewCFunction(ctx, js_node_path_join, "join", 0));
  JS_SetPropertyStr(ctx, path_obj, "resolve", JS_NewCFunction(ctx, js_node_path_resolve, "resolve", 0));
  JS_SetPropertyStr(ctx, path_obj, "normalize", JS_NewCFunction(ctx, js_node_path_normalize, "normalize", 1));
  JS_SetPropertyStr(ctx, path_obj, "dirname", JS_NewCFunction(ctx, js_node_path_dirname, "dirname", 1));
  JS_SetPropertyStr(ctx, path_obj, "basename", JS_NewCFunction(ctx, js_node_path_basename, "basename", 2));
  JS_SetPropertyStr(ctx, path_obj, "extname", JS_NewCFunction(ctx, js_node_path_extname, "extname", 1));
  JS_SetPropertyStr(ctx, path_obj, "relative", JS_NewCFunction(ctx, js_node_path_relative, "relative", 2));
  JS_SetPropertyStr(ctx, path_obj, "isAbsolute", JS_NewCFunction(ctx, js_node_path_is_absolute, "isAbsolute", 1));

  // Add properties
  JS_SetPropertyStr(ctx, path_obj, "sep", JS_NewString(ctx, PATH_SEPARATOR_STR));

  char delimiter[2] = {PATH_DELIMITER, '\0'};
  JS_SetPropertyStr(ctx, path_obj, "delimiter", JS_NewString(ctx, delimiter));

  return path_obj;
}

// ES module initialization for node:path
int js_node_path_init(JSContext* ctx, JSModuleDef* m) {
  JSValue path_module = JSRT_CreateNodePathModule(ctx);

  // Export as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, path_module));

  // Export individual functions
  JSValue join = JS_GetPropertyStr(ctx, path_module, "join");
  JS_SetModuleExport(ctx, m, "join", join);

  JSValue resolve = JS_GetPropertyStr(ctx, path_module, "resolve");
  JS_SetModuleExport(ctx, m, "resolve", resolve);

  JSValue normalize = JS_GetPropertyStr(ctx, path_module, "normalize");
  JS_SetModuleExport(ctx, m, "normalize", normalize);

  JSValue dirname = JS_GetPropertyStr(ctx, path_module, "dirname");
  JS_SetModuleExport(ctx, m, "dirname", dirname);

  JSValue basename = JS_GetPropertyStr(ctx, path_module, "basename");
  JS_SetModuleExport(ctx, m, "basename", basename);

  JSValue extname = JS_GetPropertyStr(ctx, path_module, "extname");
  JS_SetModuleExport(ctx, m, "extname", extname);

  JSValue relative = JS_GetPropertyStr(ctx, path_module, "relative");
  JS_SetModuleExport(ctx, m, "relative", relative);

  JSValue isAbsolute = JS_GetPropertyStr(ctx, path_module, "isAbsolute");
  JS_SetModuleExport(ctx, m, "isAbsolute", isAbsolute);

  JSValue sep = JS_GetPropertyStr(ctx, path_module, "sep");
  JS_SetModuleExport(ctx, m, "sep", sep);

  JSValue delimiter = JS_GetPropertyStr(ctx, path_module, "delimiter");
  JS_SetModuleExport(ctx, m, "delimiter", delimiter);

  JS_FreeValue(ctx, path_module);
  return 0;
}
