/**
 * Path Utility Functions Implementation
 */

#include "path_util.h"

#include <stdlib.h>
#include <string.h>

#include "../util/module_debug.h"

#ifdef _WIN32
#define PLATFORM_PATH_SEPARATOR '\\'
#define PLATFORM_PATH_SEPARATOR_STR "\\"
#else
#define PLATFORM_PATH_SEPARATOR '/'
#define PLATFORM_PATH_SEPARATOR_STR "/"
#endif

// ==== Path Separator Utilities ====

bool jsrt_is_path_separator(char c) {
  return c == '/' || c == '\\';
}

char* jsrt_find_last_separator(const char* path) {
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

// ==== Path Manipulation ====

char* jsrt_normalize_path(const char* path) {
  if (!path)
    return NULL;

  size_t len = strlen(path);
  char* normalized = (char*)malloc(len + 1);
  if (!normalized)
    return NULL;

  // Convert path separators to platform-specific ones
  for (size_t i = 0; i < len; i++) {
#ifdef _WIN32
    if (path[i] == '/')
      normalized[i] = '\\';
    else
      normalized[i] = path[i];
#else
    if (path[i] == '\\')
      normalized[i] = '/';
    else
      normalized[i] = path[i];
#endif
  }
  normalized[len] = '\0';

  MODULE_DEBUG_RESOLVER("Normalized '%s' to '%s'", path, normalized);
  return normalized;
}

char* jsrt_get_parent_directory(const char* path) {
  if (!path)
    return NULL;

  char* normalized = jsrt_normalize_path(path);
  if (!normalized)
    return NULL;

  char* last_sep = jsrt_find_last_separator(normalized);
  if (last_sep && last_sep != normalized) {
    // Normal case: found separator not at beginning
    *last_sep = '\0';
    MODULE_DEBUG_RESOLVER("Parent of '%s' is '%s'", path, normalized);
    return normalized;
  } else if (last_sep == normalized) {
    // Path is at root (e.g., "/file" -> "/")
    normalized[1] = '\0';
    MODULE_DEBUG_RESOLVER("Parent of '%s' is '%s' (root)", path, normalized);
    return normalized;
  } else {
    // No separator found, return current directory
    free(normalized);
    MODULE_DEBUG_RESOLVER("Parent of '%s' is '.' (no separator)", path);
    return strdup(".");
  }
}

char* jsrt_path_join(const char* dir, const char* file) {
  if (!dir || !file)
    return NULL;

  size_t dir_len = strlen(dir);
  size_t file_len = strlen(file);

  // Check if dir already ends with separator
  bool has_trailing_sep = (dir_len > 0 && jsrt_is_path_separator(dir[dir_len - 1]));

  // Calculate total length: dir + separator (if needed) + file + null terminator
  size_t total_len = dir_len + file_len + (has_trailing_sep ? 0 : 1) + 1;
  char* result = (char*)malloc(total_len);
  if (!result)
    return NULL;

  // Build the joined path
  strcpy(result, dir);
  if (!has_trailing_sep) {
    strcat(result, PLATFORM_PATH_SEPARATOR_STR);
  }
  strcat(result, file);

  // Normalize the result
  char* normalized = jsrt_normalize_path(result);
  free(result);

  MODULE_DEBUG_RESOLVER("Joined '%s' + '%s' = '%s'", dir, file, normalized);
  return normalized;
}

// ==== Path Type Checking ====

bool jsrt_is_absolute_path(const char* path) {
  if (!path)
    return false;

#ifdef _WIN32
  // Windows: starts with drive letter (C:) or UNC path (\\) or single separator
  return (strlen(path) >= 3 && path[1] == ':' && jsrt_is_path_separator(path[2])) ||
         (strlen(path) >= 2 && path[0] == '\\' && path[1] == '\\') || jsrt_is_path_separator(path[0]);
#else
  // Unix: starts with /
  return path[0] == '/';
#endif
}

bool jsrt_is_relative_path(const char* path) {
  if (!path)
    return false;

  // Check for ./ or ../
  return (path[0] == '.' && (jsrt_is_path_separator(path[1]) || (path[1] == '.' && jsrt_is_path_separator(path[2]))));
}

char* jsrt_resolve_relative_path(const char* base_path, const char* relative_path) {
  if (!base_path || !relative_path)
    return NULL;

  MODULE_DEBUG_RESOLVER("Resolving '%s' relative to '%s'", relative_path, base_path);

  char* base_dir = jsrt_get_parent_directory(base_path);
  if (!base_dir)
    return NULL;

  // Handle ./ and ../ prefixes
  const char* clean_relative = relative_path;
  char* current_base = base_dir;

  // Skip leading "./" or "../" sequences
  while (clean_relative[0] == '.') {
    if (jsrt_is_path_separator(clean_relative[1])) {
      // Skip "./"
      clean_relative += 2;
      MODULE_DEBUG_RESOLVER("Skipped './' prefix, now at '%s'", clean_relative);
    } else if (clean_relative[1] == '.' && jsrt_is_path_separator(clean_relative[2])) {
      // Handle "../" - go up one level
      char* parent_dir = jsrt_get_parent_directory(current_base);
      free(current_base);
      if (!parent_dir)
        return NULL;
      current_base = parent_dir;
      clean_relative += 3;
      MODULE_DEBUG_RESOLVER("Handled '../' prefix, base now '%s', path now '%s'", current_base, clean_relative);
    } else {
      // Not a relative path prefix
      break;
    }
  }

  // Join the cleaned relative path with the final base directory
  char* result = jsrt_path_join(current_base, clean_relative);
  free(current_base);

  MODULE_DEBUG_RESOLVER("Resolved to '%s'", result);
  return result;
}
