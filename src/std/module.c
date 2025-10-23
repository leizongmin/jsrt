#include "module.h"

#ifdef JSRT_NODE_COMPAT
#include "../node/node_modules.h"
#endif

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>  // for _access
#include <windows.h>
#define F_OK 0
#define access _access
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
// Windows implementation of realpath using _fullpath
static char* jsrt_realpath(const char* path, char* resolved_path) {
  return _fullpath(resolved_path, path, _MAX_PATH);
}
#else
#include <unistd.h>
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
// Unix implementation uses system realpath
#define jsrt_realpath(path, resolved) realpath(path, resolved)
#endif

#include "../http/module_loader.h"
#include "../http/security.h"
#include "../module/loaders/esm_loader.h"  // For new ES module loader bridge
#include "../node/process/process.h"
#include "../util/debug.h"
#include "../util/file.h"
#include "../util/json.h"
#include "../util/path.h"
#include "assert.h"
#include "ffi.h"
extern JSValue JSRT_InitNodeWASI(JSContext* ctx);

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
bool is_absolute_path(const char* path) {
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
bool is_relative_path(const char* path) {
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
int js_std_assert_init(JSContext* ctx, JSModuleDef* m) {
  JSValue assert_module = JSRT_CreateAssertModule(ctx);
  if (JS_IsException(assert_module)) {
    return -1;
  }
  JS_SetModuleExport(ctx, m, "default", assert_module);
  return 0;
}

// Module init function for jsrt:process ES module
int js_std_process_module_init(JSContext* ctx, JSModuleDef* m) {
  // Use the unified process module
  return js_unified_process_init(ctx, m);
}

// Module init function for jsrt:ffi ES module
int js_std_ffi_init(JSContext* ctx, JSModuleDef* m) {
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
    // Build node_modules path (current directory)
    char* node_modules_path = path_join(current_search, "node_modules");
    if (!node_modules_path)
      break;

    // Build package path inside current node_modules
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

    // Additional fallback: examples/node_modules within current directory
    char* examples_dir = path_join(current_search, "examples");
    if (examples_dir) {
      char* examples_node_modules = path_join(examples_dir, "node_modules");
      free(examples_dir);
      if (examples_node_modules) {
        char* examples_package_path = path_join(examples_node_modules, package_name);
        free(examples_node_modules);
        if (examples_package_path) {
          if (access(examples_package_path, F_OK) == 0) {
            JSRT_Debug("find_node_modules_path: found package in examples at '%s'", examples_package_path);
            free(search_dir);
            return examples_package_path;
          }
          free(examples_package_path);
        }
      }
    }

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

static char* resolve_exports_entry(JSContext* ctx, JSValue entry, const char* package_dir, bool is_esm) {
  if (JS_IsString(entry)) {
    const char* target_str = JS_ToCString(ctx, entry);
    if (!target_str) {
      return NULL;
    }

    const char* relative = target_str;
    if (relative[0] == '.' && relative[1] == '/') {
      relative += 2;
    }

    char* full_path = path_join(package_dir, relative);
    JS_FreeCString(ctx, target_str);
    return full_path;
  }

  if (JS_IsArray(ctx, entry)) {
    JSValue length_val = JS_GetPropertyStr(ctx, entry, "length");
    uint32_t length = 0;
    if (!JS_IsUndefined(length_val) && !JS_IsNull(length_val)) {
      JS_ToUint32(ctx, &length, length_val);
    }
    JS_FreeValue(ctx, length_val);

    for (uint32_t i = 0; i < length; i++) {
      JSValue element = JS_GetPropertyUint32(ctx, entry, i);
      if (JS_IsUndefined(element) || JS_IsNull(element)) {
        JS_FreeValue(ctx, element);
        continue;
      }
      char* resolved = resolve_exports_entry(ctx, element, package_dir, is_esm);
      JS_FreeValue(ctx, element);
      if (resolved) {
        return resolved;
      }
    }
    return NULL;
  }

  if (JS_IsObject(entry)) {
    const char* esm_keys[] = {"import", "module", "browser", "default", "require", NULL};
    const char* cjs_keys[] = {"require", "default", "node", "import", NULL};
    const char** keys = is_esm ? esm_keys : cjs_keys;

    for (int i = 0; keys[i]; i++) {
      JSValue prop = JS_GetPropertyStr(ctx, entry, keys[i]);
      if (JS_IsUndefined(prop) || JS_IsNull(prop)) {
        JS_FreeValue(ctx, prop);
        continue;
      }
      char* resolved = resolve_exports_entry(ctx, prop, package_dir, is_esm);
      JS_FreeValue(ctx, prop);
      if (resolved) {
        return resolved;
      }
    }
  }

  return NULL;
}

static bool is_valid_identifier(const char* name) {
  if (!name || !*name) {
    return false;
  }
  if (!(isalpha((unsigned char)name[0]) || name[0] == '_' || name[0] == '$')) {
    return false;
  }
  for (const char* p = name + 1; *p; p++) {
    if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$')) {
      return false;
    }
  }
  return true;
}

static char* resolve_package_exports(JSContext* ctx, JSValue package_json, const char* package_dir, bool is_esm) {
  JSValue exports_val = JS_GetPropertyStr(ctx, package_json, "exports");
  if (JS_IsUndefined(exports_val) || JS_IsNull(exports_val)) {
    JS_FreeValue(ctx, exports_val);
    return NULL;
  }

  char* resolved = NULL;
  if (JS_IsObject(exports_val)) {
    JSValue dot_val = JS_GetPropertyStr(ctx, exports_val, ".");
    if (!JS_IsUndefined(dot_val) && !JS_IsNull(dot_val)) {
      resolved = resolve_exports_entry(ctx, dot_val, package_dir, is_esm);
    }
    JS_FreeValue(ctx, dot_val);

    if (!resolved) {
      resolved = resolve_exports_entry(ctx, exports_val, package_dir, is_esm);
    }
  } else {
    resolved = resolve_exports_entry(ctx, exports_val, package_dir, is_esm);
  }

  JS_FreeValue(ctx, exports_val);
  return resolved;
}

static char* resolve_package_main(const char* package_dir, bool is_esm) {
  JSRT_Debug("resolve_package_main: package_dir='%s', is_esm=%d", package_dir, is_esm);

  char* package_json_path = path_join(package_dir, "package.json");
  if (!package_json_path)
    return NULL;

  JSRT_ReadFileResult json_result = JSRT_ReadFile(package_json_path);
  free(package_json_path);

  if (json_result.error == JSRT_READ_FILE_OK) {
    JSRuntime* rt = JS_NewRuntime();
    JSContext* ctx = JS_NewContext(rt);

    JSValue package_json = JSRT_ParseJSON(ctx, json_result.data);
    JSRT_ReadFileResultFree(&json_result);

    if (!JS_IsNull(package_json) && !JS_IsException(package_json)) {
      char* exports_path = resolve_package_exports(ctx, package_json, package_dir, is_esm);
      if (exports_path) {
        JS_FreeValue(ctx, package_json);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        JSRT_Debug("resolve_package_main: resolved via exports to '%s'", exports_path);
        return exports_path;
      }

      bool entry_point_needs_free = false;
      const char* entry_point = NULL;
      JSValue entry_val = JS_UNDEFINED;

      if (is_esm) {
        entry_val = JS_GetPropertyStr(ctx, package_json, "module");
        if (JS_IsString(entry_val)) {
          entry_point = JS_ToCString(ctx, entry_val);
          entry_point_needs_free = true;
        }
      }

      if (!entry_point) {
        if (!JS_IsUndefined(entry_val))
          JS_FreeValue(ctx, entry_val);
        entry_val = JS_GetPropertyStr(ctx, package_json, "main");
        if (JS_IsString(entry_val)) {
          entry_point = JS_ToCString(ctx, entry_val);
          entry_point_needs_free = true;
        } else if (JS_IsUndefined(entry_val) || JS_IsNull(entry_val)) {
          entry_point = is_esm ? "index.mjs" : "index.js";
        }
      }

      if (!JS_IsUndefined(entry_val))
        JS_FreeValue(ctx, entry_val);

      char* full_path = NULL;
      if (entry_point) {
        const char* relative = entry_point;
        if (relative[0] == '.' && relative[1] == '/')
          relative += 2;
        full_path = path_join(package_dir, relative);
      }

      if (entry_point_needs_free && entry_point)
        JS_FreeCString(ctx, entry_point);

      JS_FreeValue(ctx, package_json);
      JS_FreeContext(ctx);
      JS_FreeRuntime(rt);

      if (full_path) {
        JSRT_Debug("resolve_package_main: resolved to '%s'", full_path);
        return full_path;
      }
    } else {
      JS_FreeValue(ctx, package_json);
      JS_FreeContext(ctx);
      JS_FreeRuntime(rt);
    }
  } else {
    JSRT_ReadFileResultFree(&json_result);
  }

  const char* default_file = is_esm ? "index.mjs" : "index.js";
  char* default_path = path_join(package_dir, default_file);
  JSRT_Debug("resolve_package_main: falling back to '%s'", default_path ? default_path : "NULL");
  return default_path;
}

static char* resolve_npm_module(const char* module_name, const char* base_path, bool is_esm) {
  JSRT_Debug("resolve_npm_module: module_name='%s', base_path='%s', is_esm=%d", module_name,
             base_path ? base_path : "null", is_esm);

#ifdef JSRT_NODE_COMPAT
  if (!is_absolute_path(module_name) && !is_relative_path(module_name) && JSRT_IsNodeModule(module_name)) {
    size_t len = strlen(module_name) + 6;
    char* node_specifier = malloc(len);
    if (!node_specifier)
      return NULL;
    snprintf(node_specifier, len, "node:%s", module_name);
    JSRT_Debug("resolve_npm_module: mapped Node builtin '%s' to '%s'", module_name, node_specifier);
    return node_specifier;
  }
#endif

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

// Check if a package is an ES module by reading its package.json
static bool is_package_esm(const char* package_dir) {
  char* package_json_path = path_join(package_dir, "package.json");
  if (!package_json_path)
    return false;

  JSRT_ReadFileResult json_result = JSRT_ReadFile(package_json_path);
  free(package_json_path);

  if (json_result.error != JSRT_READ_FILE_OK) {
    JSRT_ReadFileResultFree(&json_result);
    return false;  // Default to CommonJS
  }

  // Parse package.json
  JSRuntime* rt = JS_NewRuntime();
  JSContext* ctx = JS_NewContext(rt);

  JSValue package_json = JSRT_ParseJSON(ctx, json_result.data);
  JSRT_ReadFileResultFree(&json_result);

  bool is_esm = false;
  if (!JS_IsNull(package_json) && !JS_IsException(package_json)) {
    const char* type = JSRT_GetPackageType(ctx, package_json);
    if (type && strcmp(type, "module") == 0) {
      is_esm = true;
    }
    if (type) {
      JS_FreeCString(ctx, type);
    }
    JS_FreeValue(ctx, package_json);
  }

  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);

  return is_esm;
}

// Resolve package imports (paths starting with #)
static char* resolve_package_import(const char* import_name, const char* requesting_module_path) {
  if (!import_name || import_name[0] != '#') {
    return NULL;  // Not a package import
  }

  // Find the nearest package.json by walking up from the requesting module
  char* current_dir = requesting_module_path ? get_parent_directory(requesting_module_path) : strdup(".");
  if (!current_dir)
    current_dir = strdup(".");

  char* package_json_path = NULL;
  char* temp_dir = strdup(current_dir);

  // Walk up directories looking for package.json
  while (temp_dir && strlen(temp_dir) > 1) {
    char* test_path = path_join(temp_dir, "package.json");
    if (test_path) {
      JSRT_ReadFileResult result = JSRT_ReadFile(test_path);
      if (result.error == JSRT_READ_FILE_OK) {
        package_json_path = test_path;
        JSRT_ReadFileResultFree(&result);
        break;
      }
      JSRT_ReadFileResultFree(&result);
      free(test_path);
    }

    // Go up one directory
    char* parent = get_parent_directory(temp_dir);
    free(temp_dir);
    temp_dir = parent;
  }

  free(current_dir);
  if (temp_dir)
    free(temp_dir);

  if (!package_json_path) {
    return NULL;  // No package.json found
  }

  // Read and parse package.json
  JSRT_ReadFileResult json_result = JSRT_ReadFile(package_json_path);
  char* package_dir = get_parent_directory(package_json_path);
  free(package_json_path);

  if (json_result.error != JSRT_READ_FILE_OK) {
    JSRT_ReadFileResultFree(&json_result);
    if (package_dir)
      free(package_dir);
    return NULL;
  }

  JSRuntime* rt = JS_NewRuntime();
  JSContext* ctx = JS_NewContext(rt);
  JSValue package_json = JSRT_ParseJSON(ctx, json_result.data);
  JSRT_ReadFileResultFree(&json_result);

  char* resolved_path = NULL;
  if (!JS_IsNull(package_json) && !JS_IsException(package_json)) {
    // Get the imports object
    JSValue imports = JS_GetPropertyStr(ctx, package_json, "imports");
    if (!JS_IsUndefined(imports) && !JS_IsNull(imports)) {
      // Look up the import name
      JSValue import_value = JS_GetPropertyStr(ctx, imports, import_name);
      if (!JS_IsUndefined(import_value) && !JS_IsNull(import_value)) {
        if (JS_IsString(import_value)) {
          // Simple string mapping
          const char* import_path = JS_ToCString(ctx, import_value);
          if (import_path) {
            resolved_path = path_join(package_dir, import_path);
            JS_FreeCString(ctx, import_path);
          }
        } else if (JS_IsObject(import_value)) {
          // Conditional mapping - try 'default' first
          JSValue default_value = JS_GetPropertyStr(ctx, import_value, "default");
          if (!JS_IsUndefined(default_value) && JS_IsString(default_value)) {
            const char* import_path = JS_ToCString(ctx, default_value);
            if (import_path) {
              resolved_path = path_join(package_dir, import_path);
              JS_FreeCString(ctx, import_path);
            }
          }
          JS_FreeValue(ctx, default_value);
        }
      }
      JS_FreeValue(ctx, import_value);
    }
    JS_FreeValue(ctx, imports);
    JS_FreeValue(ctx, package_json);
  }

  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  if (package_dir)
    free(package_dir);

  JSRT_Debug("resolve_package_import: '%s' -> '%s'", import_name, resolved_path ? resolved_path : "NULL");
  return resolved_path;
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
  const char* extensions[] = {".js", ".json", ".mjs", ""};
  size_t base_len = strlen(base_path);

  for (int i = 0; i < 4; i++) {
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

char* JSRT_StdModuleNormalize(JSContext* ctx, const char* module_base_name, const char* module_name, void* opaque) {
  JSRT_Debug("JSRT_StdModuleNormalize: module_name='%s', module_base_name='%s'", module_name,
             module_base_name ? module_base_name : "null");

  // Handle HTTP/HTTPS URLs - validate and return as-is if valid
  if (jsrt_is_http_url(module_name)) {
    JSRT_HttpSecurityResult security_result = jsrt_http_validate_url(module_name);
    if (security_result == JSRT_HTTP_SECURITY_OK) {
      JSRT_Debug("JSRT_ModuleNormalize: validated HTTP URL '%s'", module_name);
      return strdup(module_name);
    }
    JSRT_Debug("JSRT_ModuleNormalize: HTTP URL security validation failed for '%s'", module_name);
    return NULL;  // Security validation failed
  }

  // Handle relative imports from HTTP modules
  if (jsrt_is_http_url(module_base_name) && module_name[0] == '.' &&
      (module_name[1] == '/' || (module_name[1] == '.' && module_name[2] == '/'))) {
    char* resolved = jsrt_resolve_http_relative_import(module_base_name, module_name);
    if (resolved) {
      JSRT_HttpSecurityResult security_result = jsrt_http_validate_url(resolved);
      if (security_result == JSRT_HTTP_SECURITY_OK) {
        JSRT_Debug("JSRT_ModuleNormalize: resolved HTTP relative import to '%s'", resolved);
        return resolved;
      }
      free(resolved);
    }
    JSRT_Debug("JSRT_ModuleNormalize: failed to resolve HTTP relative import '%s' from '%s'", module_name,
               module_base_name);
    return NULL;
  }

  // Handle jsrt: modules specially - don't try to resolve them as files
  if (strncmp(module_name, "jsrt:", 5) == 0) {
    return strdup(module_name);
  }

#ifdef JSRT_NODE_COMPAT
  // Handle node: modules specially - don't try to resolve them as files
  if (strncmp(module_name, "node:", 5) == 0) {
    return strdup(module_name);
  }

  // Compact Node mode: check bare name as node module for ES modules
  JSRT_Runtime* rt = (JSRT_Runtime*)opaque;
  if (rt && rt->compact_node_mode && !is_absolute_path(module_name) && !is_relative_path(module_name) &&
      JSRT_IsNodeModule(module_name)) {
    JSRT_Debug("Compact Node mode (ESM): resolving '%s' as 'node:%s'", module_name, module_name);
    char* prefixed = malloc(strlen(module_name) + 6);
    sprintf(prefixed, "node:%s", module_name);
    return prefixed;
  }
#endif

  // Handle package imports (starting with #)
  if (module_name[0] == '#') {
    char* import_path = resolve_package_import(module_name, module_base_name);
    if (import_path) {
      JSRT_Debug("JSRT_ModuleNormalize: resolved package import to '%s'", import_path);
      return import_path;
    }
    // If package import resolution failed, return error
    return NULL;
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

JSModuleDef* JSRT_StdModuleLoader(JSContext* ctx, const char* module_name, void* opaque) {
  JSRT_Debug("JSRT_StdModuleLoader: loading ES module '%s'", module_name);

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
      // Create jsrt:process module with standard process implementation
      JSModuleDef* m = JS_NewCModule(ctx, module_name, js_std_process_module_init);
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
    JSRT_Debug("JSRT_StdModuleLoader: detected CommonJS module, wrapping as ES module for '%s'", module_name);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue require_func = JS_GetPropertyStr(ctx, global, "require");
    if (JS_IsException(require_func) || !JS_IsFunction(ctx, require_func)) {
      JS_FreeValue(ctx, global);
      JS_FreeValue(ctx, require_func);
      JSRT_Debug("JSRT_StdModuleLoader: global require() not available for '%s'", module_name);
      JSRT_ReadFileResultFree(&file_result);
      return NULL;
    }

    JSValue prev_context = JS_GetPropertyStr(ctx, global, "__esm_module_context");
    JSValue filename_val = JS_NewString(ctx, module_name);
    JS_SetPropertyStr(ctx, global, "__esm_module_context", JS_DupValue(ctx, filename_val));

    JSValue require_arg = JS_DupValue(ctx, filename_val);
    JSValue exports = JS_Call(ctx, require_func, global, 1, &require_arg);
    JS_FreeValue(ctx, require_arg);

    JS_SetPropertyStr(ctx, global, "__esm_module_context", prev_context);

    if (JS_IsException(exports)) {
      JS_FreeValue(ctx, filename_val);
      JS_FreeValue(ctx, require_func);
      JS_FreeValue(ctx, global);
      JSRT_Debug("JSRT_StdModuleLoader: require() failed for '%s'", module_name);
      JSRT_ReadFileResultFree(&file_result);
      return NULL;
    }

    JSPropertyEnum* prop_entries = NULL;
    uint32_t prop_count = 0;
    if (JS_GetOwnPropertyNames(ctx, &prop_entries, &prop_count, exports,
                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY | JS_GPN_SET_ENUM) < 0) {
      JS_FreeValue(ctx, exports);
      JS_FreeValue(ctx, filename_val);
      JS_FreeValue(ctx, require_func);
      JS_FreeValue(ctx, global);
      JSRT_Debug("JSRT_StdModuleLoader: failed to enumerate exports for '%s'", module_name);
      JSRT_ReadFileResultFree(&file_result);
      return NULL;
    }

    size_t escaped_name_len = strlen(module_name) * 2 + 1;
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

    size_t wrapper_size = 1024;
    for (uint32_t i = 0; i < prop_count; i++) {
      JSValue prop_name_val = JS_AtomToString(ctx, prop_entries[i].atom);
      const char* prop_name_str = JS_ToCString(ctx, prop_name_val);
      if (prop_name_str) {
        if (strcmp(prop_name_str, "default") != 0 && strcmp(prop_name_str, "__esModule") != 0 &&
            is_valid_identifier(prop_name_str)) {
          wrapper_size += strlen(prop_name_str) * 3 + 64;
        }
        JS_FreeCString(ctx, prop_name_str);
      }
      JS_FreeValue(ctx, prop_name_val);
    }

    char* wrapper_code = malloc(wrapper_size);
    size_t offset = 0;
    offset += snprintf(wrapper_code + offset, wrapper_size - offset,
                       "// Auto-generated ES module wrapper for CommonJS module\n"
                       "const __cjs_filename = \"%s\";\n"
                       "const __cjs_prev_context = globalThis.__esm_module_context;\n"
                       "let __cjs_exports;\n"
                       "try {\n"
                       "  globalThis.__esm_module_context = __cjs_filename;\n"
                       "  __cjs_exports = globalThis.require(__cjs_filename);\n"
                       "} finally {\n"
                       "  globalThis.__esm_module_context = __cjs_prev_context;\n"
                       "}\n"
                       "export default __cjs_exports;\n",
                       escaped_module_name);

    for (uint32_t i = 0; i < prop_count; i++) {
      JSValue prop_name_val = JS_AtomToString(ctx, prop_entries[i].atom);
      const char* prop_name_str = JS_ToCString(ctx, prop_name_val);
      if (prop_name_str) {
        if (strcmp(prop_name_str, "default") != 0 && strcmp(prop_name_str, "__esModule") != 0 &&
            is_valid_identifier(prop_name_str)) {
          offset += snprintf(wrapper_code + offset, wrapper_size - offset,
                             "export const %s = __cjs_exports != null ? __cjs_exports[\"%s\"] : undefined;\n",
                             prop_name_str, prop_name_str);
        }
        JS_FreeCString(ctx, prop_name_str);
      }
      JS_FreeValue(ctx, prop_name_val);
    }

    JS_FreeValue(ctx, exports);
    js_free(ctx, prop_entries);
    JS_FreeValue(ctx, filename_val);
    JS_FreeValue(ctx, require_func);
    JS_FreeValue(ctx, global);
    free(escaped_module_name);

    JSValue func_val = JS_Eval(ctx, wrapper_code, offset, module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    free(wrapper_code);

    if (JS_IsException(func_val)) {
      JSRT_Debug("JSRT_StdModuleLoader: failed to compile CommonJS wrapper for '%s'", module_name);
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
      JSRT_Debug("JSRT_StdModuleLoader: failed to compile ES module '%s'", module_name);
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

  JSRT_Debug("JSRT_StdModuleLoader: successfully loaded ES module '%s'", module_name);
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
static char* entry_module_path = NULL;

static int get_not_found_strings(const char* module_display, const char* require_display, bool include_require_section,
                                 char** message_out, char** stack_out) {
  if (!message_out || !stack_out)
    return -1;

  if (!module_display)
    module_display = "<unknown module>";

  const char* message_fmt = "Cannot find module '%s'";
  int message_len = snprintf(NULL, 0, message_fmt, module_display);
  if (message_len < 0)
    return -1;

  char* message = malloc((size_t)message_len + 1);
  if (!message)
    return -1;
  snprintf(message, (size_t)message_len + 1, message_fmt, module_display);

  const char* stack_fmt_no_require =
      "Error: %s\n"
      "{\n"
      "  code: 'MODULE_NOT_FOUND',\n"
      "  requireStack: []\n"
      "}\n";

  const char* stack_fmt_with_require =
      "Error: %s\n"
      "Require stack:\n"
      "- %s\n"
      "\n"
      "{\n"
      "  code: 'MODULE_NOT_FOUND',\n"
      "  requireStack: [ '%s' ]\n"
      "}\n";

  const char* stack_fmt = include_require_section ? stack_fmt_with_require : stack_fmt_no_require;
  int stack_len;
  if (include_require_section && require_display) {
    stack_len = snprintf(NULL, 0, stack_fmt, message, require_display, require_display);
  } else {
    stack_len = snprintf(NULL, 0, stack_fmt, message);
  }

  if (stack_len < 0) {
    free(message);
    return -1;
  }

  char* stack = malloc((size_t)stack_len + 1);
  if (!stack) {
    free(message);
    return -1;
  }

  if (include_require_section && require_display) {
    snprintf(stack, (size_t)stack_len + 1, stack_fmt, message, require_display, require_display);
  } else {
    snprintf(stack, (size_t)stack_len + 1, stack_fmt, message);
  }

  *message_out = message;
  *stack_out = stack;
  return 0;
}

void JSRT_StdModuleBuildNotFoundStrings(const char* module_display, const char* require_display,
                                        bool include_require_section, char** message_out, char** stack_out) {
  if (message_out)
    *message_out = NULL;
  if (stack_out)
    *stack_out = NULL;

  if (!message_out || !stack_out)
    return;

  if (get_not_found_strings(module_display, require_display, include_require_section, message_out, stack_out) != 0) {
    if (*message_out) {
      free(*message_out);
      *message_out = NULL;
    }
    if (*stack_out) {
      free(*stack_out);
      *stack_out = NULL;
    }
  }
}

static JSValue js_throw_module_not_found(JSContext* ctx, const char* module_name, const char* require_path) {
  const char* require_display = "<jsrt>";
  if (require_path && require_path[0]) {
    require_display = require_path;
  } else if (entry_module_path && entry_module_path[0]) {
    require_display = entry_module_path;
  }
  char* message = NULL;
  char* stack = NULL;
  JSRT_StdModuleBuildNotFoundStrings(module_name, require_display, true, &message, &stack);
  if (!message || !stack) {
    free(message);
    free(stack);
    return JS_ThrowReferenceError(ctx, "Cannot find module '%s'", module_name);
  }

  JSValue error_obj = JS_NewError(ctx);
  if (JS_IsException(error_obj)) {
    free(message);
    free(stack);
    return JS_EXCEPTION;
  }

  if (JS_DefinePropertyValueStr(ctx, error_obj, "message", JS_NewString(ctx, message), JS_PROP_C_W_E) < 0) {
    JS_FreeValue(ctx, error_obj);
    free(message);
    free(stack);
    return JS_EXCEPTION;
  }

  JSValue require_stack = JS_NewArray(ctx);
  if (JS_IsException(require_stack)) {
    JS_FreeValue(ctx, error_obj);
    free(message);
    free(stack);
    return JS_EXCEPTION;
  }

  if (JS_SetPropertyUint32(ctx, require_stack, 0, JS_NewString(ctx, require_display)) < 0) {
    JS_FreeValue(ctx, require_stack);
    JS_FreeValue(ctx, error_obj);
    free(message);
    free(stack);
    return JS_EXCEPTION;
  }

  if (JS_DefinePropertyValueStr(ctx, error_obj, "requireStack", require_stack, JS_PROP_C_W_E) < 0) {
    JS_FreeValue(ctx, error_obj);
    free(message);
    free(stack);
    return JS_EXCEPTION;
  }

  if (JS_DefinePropertyValueStr(ctx, error_obj, "code", JS_NewString(ctx, "MODULE_NOT_FOUND"), JS_PROP_C_W_E) < 0) {
    JS_FreeValue(ctx, error_obj);
    free(message);
    free(stack);
    return JS_EXCEPTION;
  }

  if (JS_DefinePropertyValueStr(ctx, error_obj, "stack", JS_NewString(ctx, stack), JS_PROP_C_W_E) < 0) {
    JS_FreeValue(ctx, error_obj);
    free(message);
    free(stack);
    return JS_EXCEPTION;
  }

  free(message);
  free(stack);

  return JS_Throw(ctx, error_obj);
}

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
  const char* npm_base_path = effective_module_path ? effective_module_path : entry_module_path;

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
      JSValue result = jsrt_get_process_module(ctx);
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

    if (strcmp(std_module, "wasi") == 0) {
#ifdef JSRT_NODE_COMPAT
      JSValue result = JSRT_LoadNodeModuleCommonJS(ctx, "wasi");
#else
      JSValue result = JSRT_InitNodeWASI(ctx);
#endif
      JS_FreeCString(ctx, module_name);
      free(esm_context_path);
      return result;
    }

    JS_FreeCString(ctx, module_name);
    free(esm_context_path);
    return JS_ThrowReferenceError(ctx, "Unknown jsrt module '%s'", std_module);
  }

#ifdef JSRT_NODE_COMPAT
  // Handle node: modules
  if (strncmp(module_name, "node:", 5) == 0) {
    const char* node_module_name = module_name + 5;
    JS_FreeCString(ctx, module_name);
    JSValue result = JSRT_LoadNodeModuleCommonJS(ctx, node_module_name);
    free(esm_context_path);
    return result;
  }

  // Compact Node mode: try bare name as node module
  JSRT_Runtime* rt = (JSRT_Runtime*)JS_GetContextOpaque(ctx);
  if (rt && rt->compact_node_mode && !is_relative_path(module_name) && !is_absolute_path(module_name) &&
      JSRT_IsNodeModule(module_name)) {
    JSRT_Debug("Compact Node mode: resolving '%s' as 'node:%s'", module_name, module_name);
    JS_FreeCString(ctx, module_name);
    JSValue result = JSRT_LoadNodeModuleCommonJS(ctx, module_name);
    free(esm_context_path);
    return result;
  }
#endif

  // Handle HTTP/HTTPS modules
  if (jsrt_is_http_url(module_name)) {
    JSValue result = jsrt_require_http_module(ctx, module_name);
    JS_FreeCString(ctx, module_name);
    free(esm_context_path);
    return result;
  }

  // Check if this is a bare module name (npm package)
  char* resolved_path;

  if (!is_relative_path(module_name) && !is_absolute_path(module_name)) {
    // Check if this is an npm package and if it's an ES module
    JSRT_Debug("js_require: trying npm module resolution");

    // First, find the package directory to check if it's ES module
    char* start_dir = npm_base_path ? get_parent_directory(npm_base_path) : strdup(".");
    if (!start_dir)
      start_dir = strdup(".");

    char* package_dir = find_node_modules_path(start_dir, module_name);
    free(start_dir);

    // For CommonJS require(), we skip ES module checking to avoid runtime conflicts
    // The ES module detection is handled in the ES module loader path instead

    if (package_dir) {
      free(package_dir);
    }

    resolved_path = resolve_npm_module(module_name, npm_base_path, false);
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
    JSValue thrown = js_throw_module_not_found(ctx, module_name, effective_module_path);
    JS_FreeCString(ctx, module_name);
    free(final_path);
    free(esm_context_path);
    JSRT_ReadFileResultFree(&file_result);
    return thrown;
  }

  // Check if this is a JSON file
  size_t path_len = strlen(final_path);
  if (path_len >= 5 && strcmp(final_path + path_len - 5, ".json") == 0) {
    JSRT_Debug("js_require: loading JSON file: %s", final_path);
    // Parse as JSON
    JSValue json_obj = JS_ParseJSON(ctx, file_result.data, file_result.size, final_path);
    JSRT_ReadFileResultFree(&file_result);

    if (JS_IsException(json_obj)) {
      JS_FreeCString(ctx, module_name);
      free(final_path);
      free(esm_context_path);
      return json_obj;
    }

    // Cache the JSON object
    cache_module(ctx, final_path, json_obj);

    JS_FreeCString(ctx, module_name);
    free(final_path);
    free(esm_context_path);
    return json_obj;
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
  // Use new ES module loader bridge functions
  JS_SetModuleLoaderFunc(rt->rt, jsrt_esm_normalize_callback, jsrt_esm_loader_callback, rt);
}

void JSRT_StdCommonJSInit(JSRT_Runtime* rt) {
  JSRT_Debug("JSRT_StdCommonJSInit: initializing CommonJS support");

  JSValue global = JS_GetGlobalObject(rt->ctx);
  JS_SetPropertyStr(rt->ctx, global, "require", JS_NewCFunction(rt->ctx, js_require, "require", 1));
  JS_FreeValue(rt->ctx, global);
}

void JSRT_StdCommonJSSetEntryPath(const char* path) {
  JSRT_Debug("JSRT_StdCommonJSSetEntryPath: path='%s'", path ? path : "NULL");
  if (entry_module_path) {
    free(entry_module_path);
    entry_module_path = NULL;
  }

  if (path) {
    entry_module_path = strdup(path);
  }
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
  if (current_module_path) {
    free(current_module_path);
    current_module_path = NULL;
  }
  if (entry_module_path) {
    free(entry_module_path);
    entry_module_path = NULL;
  }
}
