/**
 * Unified Path Resolver Implementation
 */

#include "path_resolver.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "../../node/module/hooks.h"
#include "../../runtime.h"
#include "../../util/file.h"
#include "../util/module_debug.h"
#include "npm_resolver.h"
#include "package_json.h"
#include "path_util.h"
#include "specifier.h"

// Helper to check if a path is a directory
static bool is_directory(const char* path) {
  if (!path)
    return false;

  struct stat st;
  if (stat(path, &st) != 0) {
    return false;
  }
  return S_ISDIR(st.st_mode);
}

// Helper to check if a file exists (and is a regular file)
static bool file_exists(const char* path) {
  if (!path)
    return false;

  struct stat st;
  if (stat(path, &st) != 0) {
    return false;
  }

  // Only return true for regular files, not directories
  return S_ISREG(st.st_mode);
}

char* jsrt_try_extensions(const char* base_path) {
  if (!base_path) {
    MODULE_DEBUG_RESOLVER("Cannot try extensions: NULL base_path");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Trying extensions for '%s'", base_path);

  // Extensions to try in order (Node.js compatible order)
  const char* extensions[] = {".js", ".json", ".mjs", ".cjs", ""};

  for (int i = 0; i < 5; i++) {
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

  // Check for resolve hooks first (Task 4.3: Resolve Hook Execution)
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (rt && rt->hook_registry && jsrt_hook_get_count(rt->hook_registry) > 0) {
    MODULE_DEBUG_RESOLVER("Executing resolve hooks before normal resolution");

    // Build hook context
    JSRTHookContext hook_context = {0};
    hook_context.specifier = specifier;
    hook_context.base_path = base_path;
    hook_context.resolved_url = NULL;
    hook_context.is_main_module = (base_path == NULL);  // No base path means main module
    hook_context.condition_count = 0;
    hook_context.user_data = NULL;

    // Set default conditions for Node.js compatibility
    char* conditions[] = {"node", "default", NULL};

    // Execute enhanced resolve hooks
    char* hook_result = jsrt_hook_execute_resolve_enhanced(rt->hook_registry, specifier, &hook_context, conditions);
    if (hook_result) {
      MODULE_DEBUG_RESOLVER("Resolve hook returned: %s", hook_result);

      // Create resolved path from hook result
      JSRT_ResolvedPath* hook_resolved = (JSRT_ResolvedPath*)calloc(1, sizeof(JSRT_ResolvedPath));
      if (hook_resolved) {
        hook_resolved->resolved_path = hook_result;
        hook_resolved->type = JSRT_SPECIFIER_BARE;  // Hook result type
        hook_resolved->is_url = (strstr(hook_result, "://") != NULL);
        hook_resolved->is_builtin = false;

        // Extract protocol if URL
        if (hook_resolved->is_url) {
          if (strncmp(hook_result, "http://", 7) == 0) {
            hook_resolved->protocol = strdup("http");
          } else if (strncmp(hook_result, "https://", 8) == 0) {
            hook_resolved->protocol = strdup("https");
          } else if (strncmp(hook_result, "file://", 7) == 0) {
            hook_resolved->protocol = strdup("file");
          }
        }

        MODULE_DEBUG_RESOLVER("Using hook resolution result: %s", hook_result);
        return hook_resolved;
      }

      free(hook_result);  // Cleanup if allocation failed
    }

    MODULE_DEBUG_RESOLVER("Resolve hooks did not return a result, continuing with normal resolution");
  }

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
    // First check if resolved path is a directory
    if (is_directory(resolved)) {
      MODULE_DEBUG_RESOLVER("Resolved path is a directory: %s", resolved);

      // Try package.json main field first
      char* package_main = jsrt_resolve_package_main(ctx, resolved, is_esm);
      if (package_main) {
        MODULE_DEBUG_RESOLVER("Resolved directory via package.json main: %s", package_main);
        free(resolved);
        result->resolved_path = package_main;
      } else {
        // Fallback to directory index
        char* dir_index = jsrt_try_directory_index(resolved);
        if (dir_index) {
          MODULE_DEBUG_RESOLVER("Resolved directory via index file: %s", dir_index);
          free(resolved);
          result->resolved_path = dir_index;
        } else {
          MODULE_DEBUG_RESOLVER("No main or index found in directory: %s", resolved);
          result->resolved_path = resolved;
        }
      }
    } else if (file_exists(resolved)) {
      // It's a regular file
      MODULE_DEBUG_RESOLVER("Resolved path exists as file: %s", resolved);
      result->resolved_path = resolved;
    } else {
      // Path doesn't exist as file or directory - try extensions
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
