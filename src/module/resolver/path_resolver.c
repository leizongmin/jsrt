/**
 * Unified Path Resolver Implementation
 */

#include "path_resolver.h"

#include <stdlib.h>
#include <string.h>

#include "../../util/file.h"
#include "../util/module_debug.h"
#include "npm_resolver.h"
#include "package_json.h"
#include "path_util.h"
#include "specifier.h"

// Helper to check if a file exists
static bool file_exists(const char* path) {
  if (!path)
    return false;

  JSRT_ReadFileResult result = JSRT_ReadFile(path);
  bool exists = (result.error == JSRT_READ_FILE_OK);
  JSRT_ReadFileResultFree(&result);
  return exists;
}

char* jsrt_try_extensions(const char* base_path) {
  if (!base_path) {
    MODULE_DEBUG_RESOLVER("Cannot try extensions: NULL base_path");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Trying extensions for '%s'", base_path);

  // Extensions to try in order
  const char* extensions[] = {".js", ".mjs", ".cjs", ""};

  for (int i = 0; i < 4; i++) {
    const char* ext = extensions[i];
    size_t total_len = strlen(base_path) + strlen(ext) + 1;
    char* full_path = (char*)malloc(total_len);
    if (!full_path)
      continue;

    snprintf(full_path, total_len, "%s%s", base_path, ext);

    if (file_exists(full_path)) {
      MODULE_DEBUG_RESOLVER("Found file with extension '%s': %s", ext, full_path);
      return full_path;
    }

    free(full_path);
  }

  MODULE_DEBUG_RESOLVER("No file found with any extension");
  return NULL;
}

char* jsrt_try_directory_index(const char* dir_path) {
  if (!dir_path) {
    MODULE_DEBUG_RESOLVER("Cannot try directory index: NULL dir_path");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Trying directory index for '%s'", dir_path);

  // Index files to try in order
  const char* index_files[] = {"index.js", "index.mjs", "index.cjs"};

  for (int i = 0; i < 3; i++) {
    char* index_path = jsrt_path_join(dir_path, index_files[i]);
    if (!index_path)
      continue;

    if (file_exists(index_path)) {
      MODULE_DEBUG_RESOLVER("Found directory index: %s", index_path);
      return index_path;
    }

    free(index_path);
  }

  MODULE_DEBUG_RESOLVER("No directory index file found");
  return NULL;
}

char* jsrt_validate_url(const char* url) {
  if (!url) {
    MODULE_DEBUG_RESOLVER("Cannot validate NULL URL");
    return NULL;
  }

  // Check for valid URL protocols
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0 && strncmp(url, "file://", 7) != 0) {
    MODULE_DEBUG_RESOLVER("Invalid URL protocol: %s", url);
    return NULL;
  }

  // For now, just return a copy
  // TODO: Add more validation (e.g., security checks from http/security.h)
  MODULE_DEBUG_RESOLVER("Validated URL: %s", url);
  return strdup(url);
}

JSRT_ResolvedPath* jsrt_resolve_path(JSContext* ctx, const char* specifier, const char* base_path, bool is_esm) {
  if (!ctx || !specifier) {
    MODULE_DEBUG_RESOLVER("Cannot resolve path: NULL ctx or specifier");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Resolving specifier '%s' from base '%s'", specifier, base_path ? base_path : "(null)");

  // Parse the specifier
  JSRT_ModuleSpecifier* spec = jsrt_parse_specifier(specifier);
  if (!spec) {
    MODULE_DEBUG_RESOLVER("Failed to parse specifier");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Specifier type: %s", jsrt_specifier_type_to_string(spec->type));

  // Allocate result
  JSRT_ResolvedPath* result = (JSRT_ResolvedPath*)calloc(1, sizeof(JSRT_ResolvedPath));
  if (!result) {
    jsrt_specifier_free(spec);
    return NULL;
  }

  result->type = spec->type;
  result->is_url = false;
  result->is_builtin = false;

  char* resolved = NULL;

  // Route based on specifier type
  switch (spec->type) {
    case JSRT_SPECIFIER_BUILTIN:
      // jsrt:assert, node:fs - return as-is
      result->is_builtin = true;
      result->protocol = spec->protocol ? strdup(spec->protocol) : NULL;
      resolved = strdup(specifier);
      MODULE_DEBUG_RESOLVER("Builtin module, returning as-is: %s", resolved);
      break;

    case JSRT_SPECIFIER_URL:
      // http://, https://, file:// - validate and return
      result->is_url = true;
      result->protocol = spec->protocol ? strdup(spec->protocol) : NULL;
      resolved = jsrt_validate_url(specifier);
      MODULE_DEBUG_RESOLVER("URL module: %s", resolved ? resolved : "INVALID");
      break;

    case JSRT_SPECIFIER_RELATIVE:
      // ./module, ../utils - resolve against base_path
      if (base_path) {
        resolved = jsrt_resolve_relative_path(base_path, specifier);
      } else {
        // No base path, normalize relative to current directory
        resolved = jsrt_normalize_path(specifier);
      }
      MODULE_DEBUG_RESOLVER("Relative path resolved to: %s", resolved ? resolved : "NULL");
      break;

    case JSRT_SPECIFIER_ABSOLUTE:
      // /path/to/module - normalize and return
      resolved = jsrt_normalize_path(specifier);
      MODULE_DEBUG_RESOLVER("Absolute path normalized to: %s", resolved ? resolved : "NULL");
      break;

    case JSRT_SPECIFIER_IMPORT:
      // #internal/utils - resolve using package imports
      resolved = jsrt_resolve_package_import(ctx, specifier, base_path);
      MODULE_DEBUG_RESOLVER("Package import resolved to: %s", resolved ? resolved : "NULL");
      break;

    case JSRT_SPECIFIER_BARE:
      // lodash, react - resolve using npm algorithm
      resolved = jsrt_resolve_npm_module(ctx, specifier, base_path, is_esm);
      MODULE_DEBUG_RESOLVER("Bare specifier (npm) resolved to: %s", resolved ? resolved : "NULL");
      break;

    default:
      MODULE_DEBUG_RESOLVER("Unknown specifier type");
      break;
  }

  jsrt_specifier_free(spec);

  // If resolution failed, return NULL
  if (!resolved) {
    MODULE_DEBUG_RESOLVER("Path resolution failed");
    jsrt_resolved_path_free(result);
    return NULL;
  }

  // For non-URL, non-builtin paths, try extensions and directory index
  if (!result->is_url && !result->is_builtin) {
    // First check if resolved path exists as-is
    if (file_exists(resolved)) {
      MODULE_DEBUG_RESOLVER("Resolved path exists as-is: %s", resolved);
      result->resolved_path = resolved;
    } else {
      // Try with extensions
      char* with_ext = jsrt_try_extensions(resolved);
      if (with_ext) {
        MODULE_DEBUG_RESOLVER("Resolved path with extension: %s", with_ext);
        free(resolved);
        result->resolved_path = with_ext;
      } else {
        // Try as directory index
        char* dir_index = jsrt_try_directory_index(resolved);
        if (dir_index) {
          MODULE_DEBUG_RESOLVER("Resolved as directory index: %s", dir_index);
          free(resolved);
          result->resolved_path = dir_index;
        } else {
          // Nothing worked, but return the resolved path anyway
          // (loader will handle file not found error)
          MODULE_DEBUG_RESOLVER("No file found, returning resolved path: %s", resolved);
          result->resolved_path = resolved;
        }
      }
    }
  } else {
    // URL or builtin - use as-is
    result->resolved_path = resolved;
  }

  MODULE_DEBUG_RESOLVER("Final resolved path: %s", result->resolved_path);
  return result;
}

void jsrt_resolved_path_free(JSRT_ResolvedPath* path) {
  if (!path)
    return;

  free(path->resolved_path);
  free(path->protocol);
  free(path);
}
