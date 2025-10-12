/**
 * NPM Module Resolver Implementation
 */

#include "npm_resolver.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

#include "../../util/file.h"
#include "../util/module_debug.h"
#include "package_json.h"
#include "path_util.h"

char* jsrt_find_node_modules(const char* start_dir, const char* package_name) {
  if (!start_dir || !package_name) {
    MODULE_DEBUG_RESOLVER("Cannot find node_modules: NULL start_dir or package_name");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Searching for '%s' in node_modules from '%s'", package_name, start_dir);

  // Start from the given directory and walk up the tree
  char* current_dir = strdup(start_dir);
  if (!current_dir)
    return NULL;

  char* result = NULL;

  while (current_dir && strlen(current_dir) > 1) {
    // Build node_modules path
    char* node_modules_path = jsrt_path_join(current_dir, "node_modules");
    if (!node_modules_path)
      break;

    // Build package path
    char* package_path = jsrt_path_join(node_modules_path, package_name);
    free(node_modules_path);

    if (!package_path)
      break;

    // Check if the package directory exists
    if (access(package_path, F_OK) == 0) {
      MODULE_DEBUG_RESOLVER("Found package at '%s'", package_path);
      result = package_path;
      break;
    }

    free(package_path);

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
    MODULE_DEBUG_RESOLVER("Package '%s' not found in any node_modules", package_name);
  }

  return result;
}

char* jsrt_resolve_package_main(JSContext* ctx, const char* package_dir, bool is_esm) {
  if (!ctx || !package_dir) {
    MODULE_DEBUG_RESOLVER("Cannot resolve package main: NULL ctx or package_dir");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Resolving main entry for package at '%s' (is_esm=%d)", package_dir, is_esm);

  // Try to read package.json
  char* package_json_path = jsrt_path_join(package_dir, "package.json");
  if (!package_json_path)
    return NULL;

  JSRT_PackageJson* pkg = jsrt_parse_package_json_file(ctx, package_json_path);
  free(package_json_path);

  char* main_entry = NULL;

  if (pkg) {
    // Get main entry from package.json
    char* entry_point = jsrt_package_get_main(pkg, is_esm);
    if (entry_point) {
      main_entry = jsrt_path_join(package_dir, entry_point);
      free(entry_point);
    }
    jsrt_package_json_free(pkg);
  }

  // Fallback to index.js or index.mjs
  if (!main_entry) {
    const char* default_file = is_esm ? "index.mjs" : "index.js";
    main_entry = jsrt_path_join(package_dir, default_file);
    MODULE_DEBUG_RESOLVER("No main in package.json, falling back to '%s'", default_file);
  }

  MODULE_DEBUG_RESOLVER("Resolved package main to '%s'", main_entry ? main_entry : "NULL");
  return main_entry;
}

char* jsrt_resolve_package_exports(JSContext* ctx, const char* package_dir, const char* subpath, bool is_esm) {
  if (!ctx || !package_dir || !subpath) {
    MODULE_DEBUG_RESOLVER("Cannot resolve package exports: NULL parameter");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Resolving exports for '%s' in package '%s' (is_esm=%d)", subpath, package_dir, is_esm);

  char* package_json_path = jsrt_path_join(package_dir, "package.json");
  if (!package_json_path)
    return NULL;

  JSRT_PackageJson* pkg = jsrt_parse_package_json_file(ctx, package_json_path);
  free(package_json_path);

  if (!pkg) {
    MODULE_DEBUG_RESOLVER("No package.json found");
    return NULL;
  }

  char* export_path = jsrt_package_resolve_exports(pkg, subpath, is_esm);
  if (!export_path) {
    jsrt_package_json_free(pkg);
    return NULL;
  }

  // Join with package directory
  char* full_path = jsrt_path_join(package_dir, export_path);
  free(export_path);
  jsrt_package_json_free(pkg);

  MODULE_DEBUG_RESOLVER("Resolved exports to '%s'", full_path ? full_path : "NULL");
  return full_path;
}

char* jsrt_resolve_package_import(JSContext* ctx, const char* import_name, const char* requesting_module_path) {
  if (!ctx || !import_name || !requesting_module_path) {
    MODULE_DEBUG_RESOLVER("Cannot resolve package import: NULL parameter");
    return NULL;
  }

  if (import_name[0] != '#') {
    MODULE_DEBUG_RESOLVER("Not a package import (doesn't start with #): '%s'", import_name);
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Resolving package import '%s' from '%s'", import_name, requesting_module_path);

  // Find the nearest package.json by walking up from requesting module
  char* module_dir = jsrt_get_parent_directory(requesting_module_path);
  if (!module_dir)
    module_dir = strdup(".");

  JSRT_PackageJson* pkg = jsrt_parse_package_json(ctx, module_dir);
  free(module_dir);

  if (!pkg) {
    MODULE_DEBUG_RESOLVER("No package.json found for import resolution");
    return NULL;
  }

  char* import_path = jsrt_package_resolve_imports(pkg, import_name);
  if (!import_path) {
    jsrt_package_json_free(pkg);
    return NULL;
  }

  // Join with package directory
  char* full_path = jsrt_path_join(pkg->dir_path, import_path);
  free(import_path);
  jsrt_package_json_free(pkg);

  MODULE_DEBUG_RESOLVER("Resolved package import to '%s'", full_path ? full_path : "NULL");
  return full_path;
}

char* jsrt_resolve_npm_module(JSContext* ctx, const char* module_name, const char* base_path, bool is_esm) {
  if (!ctx || !module_name) {
    MODULE_DEBUG_RESOLVER("Cannot resolve npm module: NULL ctx or module_name");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Resolving npm module '%s' from base '%s' (is_esm=%d)", module_name,
                        base_path ? base_path : "(null)", is_esm);

  // Determine the starting directory for resolution
  char* start_dir = NULL;
  if (base_path) {
    start_dir = jsrt_get_parent_directory(base_path);
    if (!start_dir) {
      start_dir = strdup(".");
    }
  } else {
    start_dir = strdup(".");
  }

  // Extract package name and subpath (e.g., "lodash/array" -> "lodash", "array")
  char* package_name = NULL;
  char* subpath = NULL;

  // Handle scoped packages (@org/package or @org/package/subpath)
  if (module_name[0] == '@') {
    const char* first_slash = strchr(module_name + 1, '/');
    if (!first_slash) {
      // Just "@org" - invalid
      free(start_dir);
      return NULL;
    }

    const char* second_slash = strchr(first_slash + 1, '/');
    if (!second_slash) {
      // "@org/package" - no subpath
      package_name = strdup(module_name);
      subpath = NULL;
    } else {
      // "@org/package/subpath"
      size_t pkg_len = second_slash - module_name;
      package_name = (char*)malloc(pkg_len + 1);
      if (package_name) {
        strncpy(package_name, module_name, pkg_len);
        package_name[pkg_len] = '\0';
      }
      subpath = strdup(second_slash + 1);
    }
  } else {
    // Regular package
    const char* slash = strchr(module_name, '/');
    if (!slash) {
      // Just "package" - no subpath
      package_name = strdup(module_name);
      subpath = NULL;
    } else {
      // "package/subpath"
      size_t pkg_len = slash - module_name;
      package_name = (char*)malloc(pkg_len + 1);
      if (package_name) {
        strncpy(package_name, module_name, pkg_len);
        package_name[pkg_len] = '\0';
      }
      subpath = strdup(slash + 1);
    }
  }

  if (!package_name) {
    free(start_dir);
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Parsed as package='%s', subpath='%s'", package_name, subpath ? subpath : "(none)");

  // Find the package directory
  char* package_dir = jsrt_find_node_modules(start_dir, package_name);
  free(start_dir);
  free(package_name);

  if (!package_dir) {
    free(subpath);
    return NULL;
  }

  char* result = NULL;

  // If there's a subpath, try to resolve it
  if (subpath) {
    // First try exports field
    size_t subpath_len = strlen(subpath) + 3;  // "./" + subpath + \0
    char* subpath_with_dot = (char*)malloc(subpath_len);
    if (subpath_with_dot) {
      snprintf(subpath_with_dot, subpath_len, "./%s", subpath);
      result = jsrt_resolve_package_exports(ctx, package_dir, subpath_with_dot, is_esm);
      free(subpath_with_dot);
    }

    // If exports didn't work, try direct path
    if (!result) {
      result = jsrt_path_join(package_dir, subpath);
    }

    free(subpath);
  } else {
    // No subpath - resolve package main
    // First try exports field with "."
    result = jsrt_resolve_package_exports(ctx, package_dir, ".", is_esm);

    // If exports didn't work, use traditional main resolution
    if (!result) {
      result = jsrt_resolve_package_main(ctx, package_dir, is_esm);
    }
  }

  free(package_dir);

  MODULE_DEBUG_RESOLVER("Resolved npm module to '%s'", result ? result : "NULL");
  return result;
}
