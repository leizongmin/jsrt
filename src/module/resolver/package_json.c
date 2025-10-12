/**
 * Package.json Parser Implementation
 */

#include "package_json.h"

#include <stdlib.h>
#include <string.h>

#include "../../util/file.h"
#include "../../util/json.h"
#include "../util/module_debug.h"
#include "path_util.h"

// Helper to get string property from JSON object
static char* get_string_property(JSContext* ctx, JSValue obj, const char* prop_name) {
  if (JS_IsUndefined(obj) || JS_IsNull(obj))
    return NULL;

  JSValue prop = JS_GetPropertyStr(ctx, obj, prop_name);
  if (JS_IsUndefined(prop) || JS_IsNull(prop)) {
    JS_FreeValue(ctx, prop);
    return NULL;
  }

  if (!JS_IsString(prop)) {
    JS_FreeValue(ctx, prop);
    return NULL;
  }

  const char* str = JS_ToCString(ctx, prop);
  char* result = str ? strdup(str) : NULL;
  JS_FreeCString(ctx, str);
  JS_FreeValue(ctx, prop);

  return result;
}

JSRT_PackageJson* jsrt_parse_package_json_file(JSContext* ctx, const char* json_path) {
  if (!ctx || !json_path) {
    MODULE_DEBUG_RESOLVER("Cannot parse package.json: NULL ctx or path");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Parsing package.json from '%s'", json_path);

  // Read file
  JSRT_ReadFileResult file_result = JSRT_ReadFile(json_path);
  if (file_result.error != JSRT_READ_FILE_OK) {
    MODULE_DEBUG_RESOLVER("Failed to read package.json: %s", JSRT_ReadFileErrorToString(file_result.error));
    JSRT_ReadFileResultFree(&file_result);
    return NULL;
  }

  // Parse JSON
  JSValue json_obj = JSRT_ParseJSON(ctx, file_result.data);
  JSRT_ReadFileResultFree(&file_result);

  if (JS_IsNull(json_obj) || JS_IsException(json_obj)) {
    MODULE_DEBUG_RESOLVER("Failed to parse package.json as JSON");
    JS_FreeValue(ctx, json_obj);
    return NULL;
  }

  // Allocate structure
  JSRT_PackageJson* pkg = (JSRT_PackageJson*)calloc(1, sizeof(JSRT_PackageJson));
  if (!pkg) {
    JS_FreeValue(ctx, json_obj);
    return NULL;
  }

  pkg->ctx = ctx;

  // Get directory path
  pkg->dir_path = jsrt_get_parent_directory(json_path);

  // Parse "type" field
  pkg->type = get_string_property(ctx, json_obj, "type");
  MODULE_DEBUG_RESOLVER("  type: %s", pkg->type ? pkg->type : "(not set)");

  // Parse "main" field
  pkg->main = get_string_property(ctx, json_obj, "main");
  MODULE_DEBUG_RESOLVER("  main: %s", pkg->main ? pkg->main : "(not set)");

  // Parse "module" field
  pkg->module = get_string_property(ctx, json_obj, "module");
  MODULE_DEBUG_RESOLVER("  module: %s", pkg->module ? pkg->module : "(not set)");

  // Parse "exports" field (keep as JSValue)
  pkg->exports = JS_GetPropertyStr(ctx, json_obj, "exports");
  if (JS_IsUndefined(pkg->exports)) {
    pkg->exports = JS_NULL;
  }

  // Parse "imports" field (keep as JSValue)
  pkg->imports = JS_GetPropertyStr(ctx, json_obj, "imports");
  if (JS_IsUndefined(pkg->imports)) {
    pkg->imports = JS_NULL;
  }

  JS_FreeValue(ctx, json_obj);

  MODULE_DEBUG_RESOLVER("Successfully parsed package.json from '%s'", json_path);
  return pkg;
}

JSRT_PackageJson* jsrt_parse_package_json(JSContext* ctx, const char* dir_path) {
  if (!ctx || !dir_path) {
    MODULE_DEBUG_RESOLVER("Cannot parse package.json: NULL ctx or dir_path");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Searching for package.json starting from '%s'", dir_path);

  // Walk up directory tree looking for package.json
  char* current_dir = strdup(dir_path);
  if (!current_dir)
    return NULL;

  JSRT_PackageJson* result = NULL;

  while (current_dir && strlen(current_dir) > 1) {
    char* json_path = jsrt_path_join(current_dir, "package.json");
    if (!json_path)
      break;

    // Check if package.json exists
    JSRT_ReadFileResult test_result = JSRT_ReadFile(json_path);
    if (test_result.error == JSRT_READ_FILE_OK) {
      JSRT_ReadFileResultFree(&test_result);
      MODULE_DEBUG_RESOLVER("Found package.json at '%s'", json_path);

      // Parse it
      result = jsrt_parse_package_json_file(ctx, json_path);
      free(json_path);
      break;
    }

    JSRT_ReadFileResultFree(&test_result);
    free(json_path);

    // Move up one directory
    char* parent = jsrt_get_parent_directory(current_dir);
    if (!parent || strcmp(parent, current_dir) == 0) {
      free(parent);
      break;
    }

    free(current_dir);
    current_dir = parent;
  }

  free(current_dir);

  if (!result) {
    MODULE_DEBUG_RESOLVER("No package.json found in directory tree from '%s'", dir_path);
  }

  return result;
}

void jsrt_package_json_free(JSRT_PackageJson* pkg) {
  if (!pkg)
    return;

  free(pkg->type);
  free(pkg->main);
  free(pkg->module);
  free(pkg->dir_path);

  if (pkg->ctx) {
    JS_FreeValue(pkg->ctx, pkg->exports);
    JS_FreeValue(pkg->ctx, pkg->imports);
  }

  free(pkg);
}

bool jsrt_package_is_esm(const JSRT_PackageJson* pkg) {
  if (!pkg || !pkg->type)
    return false;

  return strcmp(pkg->type, "module") == 0;
}

char* jsrt_package_get_main(const JSRT_PackageJson* pkg, bool is_esm) {
  if (!pkg)
    return NULL;

  // For ESM, prefer "module" over "main"
  if (is_esm && pkg->module) {
    MODULE_DEBUG_RESOLVER("Using 'module' field: %s", pkg->module);
    return strdup(pkg->module);
  }

  // Use "main" field
  if (pkg->main) {
    MODULE_DEBUG_RESOLVER("Using 'main' field: %s", pkg->main);
    return strdup(pkg->main);
  }

  MODULE_DEBUG_RESOLVER("No main/module field found");
  return NULL;
}

char* jsrt_package_resolve_exports(const JSRT_PackageJson* pkg, const char* subpath, bool is_esm) {
  if (!pkg || !subpath)
    return NULL;

  if (JS_IsNull(pkg->exports) || JS_IsUndefined(pkg->exports)) {
    MODULE_DEBUG_RESOLVER("No exports field in package.json");
    return NULL;
  }

  JSContext* ctx = pkg->ctx;

  // If exports is a string, it only matches "."
  if (JS_IsString(pkg->exports)) {
    if (strcmp(subpath, ".") == 0) {
      const char* export_path = JS_ToCString(ctx, pkg->exports);
      char* result = export_path ? strdup(export_path) : NULL;
      JS_FreeCString(ctx, export_path);
      MODULE_DEBUG_RESOLVER("Exports field is string: %s", result ? result : "NULL");
      return result;
    }
    return NULL;
  }

  // If exports is an object, look up the subpath
  if (!JS_IsObject(pkg->exports)) {
    MODULE_DEBUG_RESOLVER("Exports field is not object or string");
    return NULL;
  }

  // Look up the subpath
  JSValue export_value = JS_GetPropertyStr(ctx, pkg->exports, subpath);
  if (JS_IsUndefined(export_value)) {
    JS_FreeValue(ctx, export_value);
    MODULE_DEBUG_RESOLVER("Subpath '%s' not found in exports", subpath);
    return NULL;
  }

  char* result = NULL;

  // If it's a string, that's the resolved path
  if (JS_IsString(export_value)) {
    const char* export_path = JS_ToCString(ctx, export_value);
    result = export_path ? strdup(export_path) : NULL;
    JS_FreeCString(ctx, export_path);
    MODULE_DEBUG_RESOLVER("Resolved exports['%s'] to: %s", subpath, result ? result : "NULL");
  }
  // If it's an object, handle conditional exports
  else if (JS_IsObject(export_value)) {
    // Try "import" for ESM, "require" for CommonJS, then "default"
    const char* conditions[] = {is_esm ? "import" : "require", "default", NULL};
    for (int i = 0; conditions[i]; i++) {
      JSValue cond_value = JS_GetPropertyStr(ctx, export_value, conditions[i]);
      if (!JS_IsUndefined(cond_value) && JS_IsString(cond_value)) {
        const char* cond_path = JS_ToCString(ctx, cond_value);
        result = cond_path ? strdup(cond_path) : NULL;
        JS_FreeCString(ctx, cond_path);
        JS_FreeValue(ctx, cond_value);
        MODULE_DEBUG_RESOLVER("Resolved exports['%s']['%s'] to: %s", subpath, conditions[i], result ? result : "NULL");
        break;
      }
      JS_FreeValue(ctx, cond_value);
    }
  }

  JS_FreeValue(ctx, export_value);
  return result;
}

char* jsrt_package_resolve_imports(const JSRT_PackageJson* pkg, const char* import_name) {
  if (!pkg || !import_name)
    return NULL;

  if (JS_IsNull(pkg->imports) || JS_IsUndefined(pkg->imports)) {
    MODULE_DEBUG_RESOLVER("No imports field in package.json");
    return NULL;
  }

  if (!JS_IsObject(pkg->imports)) {
    MODULE_DEBUG_RESOLVER("Imports field is not an object");
    return NULL;
  }

  JSContext* ctx = pkg->ctx;

  // Look up the import name
  JSValue import_value = JS_GetPropertyStr(ctx, pkg->imports, import_name);
  if (JS_IsUndefined(import_value)) {
    JS_FreeValue(ctx, import_value);
    MODULE_DEBUG_RESOLVER("Import '%s' not found in imports", import_name);
    return NULL;
  }

  char* result = NULL;

  // If it's a string, that's the resolved path
  if (JS_IsString(import_value)) {
    const char* import_path = JS_ToCString(ctx, import_value);
    result = import_path ? strdup(import_path) : NULL;
    JS_FreeCString(ctx, import_path);
    MODULE_DEBUG_RESOLVER("Resolved imports['%s'] to: %s", import_name, result ? result : "NULL");
  }
  // If it's an object, try "default" condition
  else if (JS_IsObject(import_value)) {
    JSValue default_value = JS_GetPropertyStr(ctx, import_value, "default");
    if (!JS_IsUndefined(default_value) && JS_IsString(default_value)) {
      const char* default_path = JS_ToCString(ctx, default_value);
      result = default_path ? strdup(default_path) : NULL;
      JS_FreeCString(ctx, default_path);
      MODULE_DEBUG_RESOLVER("Resolved imports['%s']['default'] to: %s", import_name, result ? result : "NULL");
    }
    JS_FreeValue(ctx, default_value);
  }

  JS_FreeValue(ctx, import_value);
  return result;
}
