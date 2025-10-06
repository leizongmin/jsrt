#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "node_modules.h"

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#define PATH_DELIMITER ';'
#define IS_PATH_SEPARATOR(c) ((c) == '\\' || (c) == '/')
#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#define PATH_DELIMITER ':'
#define IS_PATH_SEPARATOR(c) ((c) == '/')
#endif

// Helper: Check if path is absolute
static bool is_absolute_path(const char* path) {
  if (!path || !*path)
    return false;

#ifdef _WIN32
  // Windows: C:\ or C:/ or \\ (UNC) or /
  if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) {
    return path[1] == ':' && path[2] && IS_PATH_SEPARATOR(path[2]);
  }
  return IS_PATH_SEPARATOR(path[0]);
#else
  // Unix: starts with /
  return path[0] == '/';
#endif
}

// Normalize slashes in path
static char* normalize_separators(const char* path) {
  if (!path)
    return NULL;

  char* result = strdup(path);
  if (!result)
    return NULL;

  for (char* p = result; *p; p++) {
#ifdef _WIN32
    if (*p == '/')
      *p = '\\';
#else
    if (*p == '\\')
      *p = '/';
#endif
  }

  return result;
}

// Normalize path by resolving . and .. segments
static char* normalize_path(const char* path) {
  if (!path || !*path)
    return strdup(".");

  // First normalize separators
  char* normalized = normalize_separators(path);
  if (!normalized)
    return NULL;

  bool is_absolute = is_absolute_path(normalized);
  size_t len = strlen(normalized);
  char* result = malloc(len + 2);  // +2 for potential leading / and null terminator
  if (!result) {
    free(normalized);
    return NULL;
  }

  char* segments[256];  // Max 256 path segments
  int segment_count = 0;

#ifdef _WIN32
  // For Windows, check if this is a drive letter path (e.g., "C:")
  bool is_drive_path = false;
  char drive_prefix[4] = {0};  // Store "C:" or similar
  if (is_absolute && strlen(normalized) >= 2 &&
      ((normalized[0] >= 'A' && normalized[0] <= 'Z') || (normalized[0] >= 'a' && normalized[0] <= 'z')) &&
      normalized[1] == ':') {
    is_drive_path = true;
    drive_prefix[0] = normalized[0];
    drive_prefix[1] = ':';
    drive_prefix[2] = '\0';
  }
#endif

  // Split path into segments
  char* token = strtok(normalized, PATH_SEPARATOR_STR);
  while (token && segment_count < 256) {
    if (strcmp(token, ".") == 0) {
      // Skip . segments
      token = strtok(NULL, PATH_SEPARATOR_STR);
      continue;
    }

    if (strcmp(token, "..") == 0) {
      // .. segment - go up one directory
      if (segment_count > 0 && strcmp(segments[segment_count - 1], "..") != 0) {
        // Remove previous segment (unless it's also ..)
        segment_count--;
      } else if (!is_absolute) {
        // Keep .. for relative paths
        segments[segment_count++] = "..";
      }
      // For absolute paths, .. at root is ignored
    } else {
      // Regular segment
      segments[segment_count++] = token;
    }

    token = strtok(NULL, PATH_SEPARATOR_STR);
  }

  // Rebuild path
  result[0] = '\0';

#ifdef _WIN32
  if (is_drive_path) {
    // For Windows drive paths, start with the drive letter
    strcpy(result, drive_prefix);
  } else if (is_absolute) {
    // For other absolute paths (UNC, etc.), add leading separator
    strcpy(result, PATH_SEPARATOR_STR);
  }
#else
  if (is_absolute) {
    strcpy(result, PATH_SEPARATOR_STR);
  }
#endif

  for (int i = 0; i < segment_count; i++) {
#ifdef _WIN32
    if (is_drive_path) {
      // For drive paths, always add separator before each segment
      strcat(result, PATH_SEPARATOR_STR);
      strcat(result, segments[i]);
    } else {
#endif
      if (i > 0) {
        strcat(result, PATH_SEPARATOR_STR);
      }
      strcat(result, segments[i]);
#ifdef _WIN32
    }
#endif
  }

  // Handle empty result
  if (result[0] == '\0') {
    strcpy(result, ".");
  }

  free(normalized);
  return result;
}

// node:path.join implementation
static JSValue js_path_join(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc == 0) {
    return JS_NewString(ctx, ".");
  }

  // Calculate total length needed
  size_t total_len = 0;
  for (int i = 0; i < argc; i++) {
    size_t len;
    const char* str = JS_ToCStringLen(ctx, &len, argv[i]);
    if (!str)
      return JS_EXCEPTION;
    total_len += len + 1;  // +1 for separator
    JS_FreeCString(ctx, str);
  }

  // Allocate result buffer
  char* result = malloc(total_len + 1);
  if (!result) {
    JS_ThrowOutOfMemory(ctx);
    return JS_EXCEPTION;
  }

  result[0] = '\0';
  bool first = true;

  // Join all paths
  for (int i = 0; i < argc; i++) {
    const char* part = JS_ToCString(ctx, argv[i]);
    if (!part) {
      free(result);
      return JS_EXCEPTION;
    }

    if (*part) {  // Skip empty strings
      // If this part is an absolute path, reset the result
      if (is_absolute_path(part)) {
        result[0] = '\0';
        strcat(result, part);
        first = false;
      } else {
        // Remove leading separators from part if not first
        while (!first && IS_PATH_SEPARATOR(*part)) {
          part++;
        }

        if (!first && strlen(result) > 0 && !IS_PATH_SEPARATOR(result[strlen(result) - 1])) {
          strcat(result, PATH_SEPARATOR_STR);
        }
        strcat(result, part);
        first = false;
      }
    }

    JS_FreeCString(ctx, part);
  }

  if (result[0] == '\0') {
    strcpy(result, ".");
  }

  // Normalize separators and resolve . and .. segments
  char* temp_normalized = normalize_separators(result);
  free(result);

  // Then apply full normalization to handle . and .. segments
  char* fully_normalized = normalize_path(temp_normalized);
  free(temp_normalized);

  JSValue ret = JS_NewString(ctx, fully_normalized);
  free(fully_normalized);
  return ret;
}

// node:path.resolve implementation
static JSValue js_path_resolve(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  char resolved[4096] = "";
  bool absolute = false;

  // Process arguments from right to left
  for (int i = argc - 1; i >= 0 && !absolute; i--) {
    const char* path = JS_ToCString(ctx, argv[i]);
    if (!path)
      return JS_EXCEPTION;

    if (*path) {
      if (resolved[0]) {
        // Prepend path separator if needed
        char temp[4096];
        snprintf(temp, sizeof(temp), "%s%s%s", path,
                 IS_PATH_SEPARATOR(path[strlen(path) - 1]) ? "" : PATH_SEPARATOR_STR, resolved);
        strcpy(resolved, temp);
      } else {
        strcpy(resolved, path);
      }

      absolute = is_absolute_path(path);
    }

    JS_FreeCString(ctx, path);
  }

  // If still not absolute, prepend current working directory
  if (!absolute) {
    char cwd[2048];
    if (getcwd(cwd, sizeof(cwd))) {
      char temp[4096];
      snprintf(temp, sizeof(temp), "%s%s%s", cwd, PATH_SEPARATOR_STR, resolved);
      strcpy(resolved, temp);
    }
  }

  // Normalize the result (resolves . and .. segments)
  char* normalized = normalize_path(resolved);
  JSValue ret = JS_NewString(ctx, normalized);
  free(normalized);
  return ret;
}

// node:path.normalize implementation
static JSValue js_path_normalize(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "path.normalize requires a path argument");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "path");

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path)
    return JS_EXCEPTION;

  // Use the normalize_path function which handles . and .. segments
  char* normalized = normalize_path(path);

  JS_FreeCString(ctx, path);

  if (!normalized) {
    JS_ThrowOutOfMemory(ctx);
    return JS_EXCEPTION;
  }

  JSValue ret = JS_NewString(ctx, normalized);
  free(normalized);
  return ret;
}

// node:path.isAbsolute implementation
static JSValue js_path_is_absolute(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path)
    return JS_EXCEPTION;

  bool is_abs = is_absolute_path(path);
  JS_FreeCString(ctx, path);

  return JS_NewBool(ctx, is_abs);
}

// node:path.dirname implementation
static JSValue js_path_dirname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "path.dirname requires a path argument");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "path");

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path)
    return JS_EXCEPTION;

  size_t len = strlen(path);

  // Handle empty path
  if (len == 0) {
    JS_FreeCString(ctx, path);
    return JS_NewString(ctx, ".");
  }

  // Find last separator
  int last_sep = -1;
  for (int i = len - 1; i >= 0; i--) {
    if (IS_PATH_SEPARATOR(path[i])) {
      last_sep = i;
      break;
    }
  }

  JSValue result;
  if (last_sep == -1) {
    // No separator found
    result = JS_NewString(ctx, ".");
  } else if (last_sep == 0) {
    // Root directory
    result = JS_NewString(ctx, PATH_SEPARATOR_STR);
  } else {
    // Return everything before the last separator
    char* dir = malloc(last_sep + 1);
    memcpy(dir, path, last_sep);
    dir[last_sep] = '\0';
    result = JS_NewString(ctx, dir);
    free(dir);
  }

  JS_FreeCString(ctx, path);
  return result;
}

// node:path.basename implementation
static JSValue js_path_basename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "path.basename requires a path argument");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "path");

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path)
    return JS_EXCEPTION;

  size_t len = strlen(path);

  // Skip trailing separators
  while (len > 0 && IS_PATH_SEPARATOR(path[len - 1])) {
    len--;
  }

  // Find last separator
  int start = 0;
  for (int i = len - 1; i >= 0; i--) {
    if (IS_PATH_SEPARATOR(path[i])) {
      start = i + 1;
      break;
    }
  }

  // Extract basename
  size_t basename_len = len - start;
  char* basename = malloc(basename_len + 1);
  memcpy(basename, path + start, basename_len);
  basename[basename_len] = '\0';

  // Handle optional extension parameter
  if (argc >= 2 && JS_IsString(argv[1])) {
    const char* ext = JS_ToCString(ctx, argv[1]);
    if (ext) {
      size_t ext_len = strlen(ext);
      if (basename_len >= ext_len && strcmp(basename + basename_len - ext_len, ext) == 0) {
        basename[basename_len - ext_len] = '\0';
      }
      JS_FreeCString(ctx, ext);
    }
  }

  JSValue result = JS_NewString(ctx, basename);
  free(basename);
  JS_FreeCString(ctx, path);
  return result;
}

// node:path.extname implementation
static JSValue js_path_extname(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "path.extname requires a path argument");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "path");

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path)
    return JS_EXCEPTION;

  size_t len = strlen(path);

  // Find last dot after last separator
  int last_dot = -1;
  int last_sep = -1;

  for (int i = len - 1; i >= 0; i--) {
    if (path[i] == '.' && last_dot == -1) {
      last_dot = i;
    } else if (IS_PATH_SEPARATOR(path[i])) {
      last_sep = i;
      break;
    }
  }

  JSValue result;
  if (last_dot > last_sep && last_dot > 0) {
    result = JS_NewString(ctx, path + last_dot);
  } else {
    result = JS_NewString(ctx, "");
  }

  JS_FreeCString(ctx, path);
  return result;
}

// node:path.relative implementation
static JSValue js_path_relative(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "path.relative requires from and to arguments");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "from");
  NODE_ARG_REQUIRE_STRING(ctx, argv[1], "to");

  const char* from = JS_ToCString(ctx, argv[0]);
  const char* to = JS_ToCString(ctx, argv[1]);

  if (!from || !to) {
    if (from)
      JS_FreeCString(ctx, from);
    if (to)
      JS_FreeCString(ctx, to);
    return JS_EXCEPTION;
  }

  // Normalize paths to resolve . and .. segments
  char* from_normalized = normalize_path(from);
  char* to_normalized = normalize_path(to);

  JS_FreeCString(ctx, from);
  JS_FreeCString(ctx, to);

  if (!from_normalized || !to_normalized) {
    if (from_normalized)
      free(from_normalized);
    if (to_normalized)
      free(to_normalized);
    JS_ThrowOutOfMemory(ctx);
    return JS_EXCEPTION;
  }

  // Convert relative paths to absolute using current working directory
  char from_abs[4096], to_abs[4096];

  if (!is_absolute_path(from_normalized)) {
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) {
      snprintf(from_abs, sizeof(from_abs), "%s%s%s", cwd, PATH_SEPARATOR_STR, from_normalized);
    } else {
      strcpy(from_abs, from_normalized);
    }
  } else {
    strcpy(from_abs, from_normalized);
  }

  if (!is_absolute_path(to_normalized)) {
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) {
      snprintf(to_abs, sizeof(to_abs), "%s%s%s", cwd, PATH_SEPARATOR_STR, to_normalized);
    } else {
      strcpy(to_abs, to_normalized);
    }
  } else {
    strcpy(to_abs, to_normalized);
  }

  free(from_normalized);
  free(to_normalized);

  // Split paths into segments
  char from_copy[4096], to_copy[4096];
  strcpy(from_copy, from_abs);
  strcpy(to_copy, to_abs);

  char* from_segments[256];
  char* to_segments[256];
  int from_count = 0, to_count = 0;

  // Split from path
  char* token = strtok(from_copy, PATH_SEPARATOR_STR);
  while (token && from_count < 256) {
    if (*token) {  // Skip empty segments
      from_segments[from_count++] = strdup(token);
    }
    token = strtok(NULL, PATH_SEPARATOR_STR);
  }

  // Split to path
  token = strtok(to_copy, PATH_SEPARATOR_STR);
  while (token && to_count < 256) {
    if (*token) {  // Skip empty segments
      to_segments[to_count++] = strdup(token);
    }
    token = strtok(NULL, PATH_SEPARATOR_STR);
  }

  // Find common prefix
  int common = 0;
  while (common < from_count && common < to_count && strcmp(from_segments[common], to_segments[common]) == 0) {
    common++;
  }

  // Build relative path
  char result[4096] = "";

  // Add .. for each remaining segment in from path
  for (int i = common; i < from_count; i++) {
    if (strlen(result) > 0) {
      strcat(result, PATH_SEPARATOR_STR);
    }
    strcat(result, "..");
  }

  // Add remaining segments from to path
  for (int i = common; i < to_count; i++) {
    if (strlen(result) > 0) {
      strcat(result, PATH_SEPARATOR_STR);
    }
    strcat(result, to_segments[i]);
  }

  // Clean up allocated segments
  for (int i = 0; i < from_count; i++) {
    free(from_segments[i]);
  }
  for (int i = 0; i < to_count; i++) {
    free(to_segments[i]);
  }

  // Handle empty result
  if (result[0] == '\0') {
    strcpy(result, ".");
  }

  return JS_NewString(ctx, result);
}

// node:path.parse implementation
static JSValue js_path_parse(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "path.parse requires a path argument");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "path");

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path)
    return JS_EXCEPTION;

  size_t len = strlen(path);

  // Create result object
  JSValue result = JS_NewObject(ctx);
  if (JS_IsException(result)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Extract root component (platform-specific)
  char root[256] = "";
  int root_end = 0;

#ifdef _WIN32
  // Windows: Check for drive letter (C:\) or UNC path (\\server\share)
  if (len >= 2 && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':') {
    // Drive letter path
    root[0] = path[0];
    root[1] = ':';
    if (len > 2 && IS_PATH_SEPARATOR(path[2])) {
      root[2] = '\\';
      root[3] = '\0';
      root_end = 3;
    } else {
      root[2] = '\0';
      root_end = 2;
    }
  } else if (len >= 2 && IS_PATH_SEPARATOR(path[0]) && IS_PATH_SEPARATOR(path[1])) {
    // UNC path: \\server\share
    strcpy(root, "\\\\");
    root_end = 2;
  } else if (len >= 1 && IS_PATH_SEPARATOR(path[0])) {
    // Just a leading separator
    strcpy(root, "\\");
    root_end = 1;
  }
#else
  // Unix: Check for leading /
  if (len >= 1 && path[0] == '/') {
    strcpy(root, "/");
    root_end = 1;
  }
#endif

  // Get dirname
  char dir[4096] = "";
  if (len > 0) {
    // Special case: if path is just "/" or just root, dir should equal root
    if (strcmp(path, "/") == 0 || strcmp(root, path) == 0) {
      strcpy(dir, path);
    } else {
      // Skip trailing separators first (same as basename logic)
      size_t effective_len = len;
      while (effective_len > 0 && IS_PATH_SEPARATOR(path[effective_len - 1])) {
        effective_len--;
      }

      // Find last separator before the basename
      int last_sep = -1;
      for (int i = effective_len - 1; i >= 0; i--) {
        if (IS_PATH_SEPARATOR(path[i])) {
          last_sep = i;
          break;
        }
      }

      if (last_sep > 0) {
        // Copy everything up to (but not including) the last separator
        size_t dir_len = last_sep;
        if (dir_len >= sizeof(dir))
          dir_len = sizeof(dir) - 1;
        memcpy(dir, path, dir_len);
        dir[dir_len] = '\0';
      } else if (last_sep == 0) {
        // Root directory
        strcpy(dir, PATH_SEPARATOR_STR);
      }
    }
  }

  // Get basename
  char base[4096] = "";
  if (len > 0) {
    // Skip trailing separators
    size_t effective_len = len;
    while (effective_len > 0 && IS_PATH_SEPARATOR(path[effective_len - 1])) {
      effective_len--;
    }

    // Find last separator
    int start = 0;
    for (int i = effective_len - 1; i >= 0; i--) {
      if (IS_PATH_SEPARATOR(path[i])) {
        start = i + 1;
        break;
      }
    }

    // Extract basename
    size_t basename_len = effective_len - start;
    if (basename_len >= sizeof(base))
      basename_len = sizeof(base) - 1;
    memcpy(base, path + start, basename_len);
    base[basename_len] = '\0';
  }

  // Get extension (from basename)
  char ext[256] = "";
  char name[4096] = "";
  if (strlen(base) > 0) {
    // Find last dot in basename
    int last_dot = -1;
    size_t base_len = strlen(base);

    for (int i = base_len - 1; i >= 0; i--) {
      if (base[i] == '.') {
        last_dot = i;
        break;
      }
    }

    // Extension is only valid if:
    // 1. There's a dot
    // 2. The dot is not the first character (hidden files on Unix)
    // 3. The dot is not the last character
    if (last_dot > 0 && last_dot < (int)base_len - 1) {
      strcpy(ext, base + last_dot);
      // Name is base without extension
      size_t name_len = last_dot;
      if (name_len >= sizeof(name))
        name_len = sizeof(name) - 1;
      memcpy(name, base, name_len);
      name[name_len] = '\0';
    } else {
      // No extension, name is same as base
      strcpy(name, base);
    }
  }

  // Set properties on result object
  JS_SetPropertyStr(ctx, result, "root", JS_NewString(ctx, root));
  JS_SetPropertyStr(ctx, result, "dir", JS_NewString(ctx, dir));
  JS_SetPropertyStr(ctx, result, "base", JS_NewString(ctx, base));
  JS_SetPropertyStr(ctx, result, "ext", JS_NewString(ctx, ext));
  JS_SetPropertyStr(ctx, result, "name", JS_NewString(ctx, name));

  JS_FreeCString(ctx, path);
  return result;
}

// node:path.format implementation
static JSValue js_path_format(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "path.format requires a pathObject argument");
  }

  if (!JS_IsObject(argv[0])) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "pathObject must be an object");
  }

  JSValue pathObj = argv[0];
  char result[4096] = "";

  // Extract properties from object
  JSValue dir_val = JS_GetPropertyStr(ctx, pathObj, "dir");
  JSValue root_val = JS_GetPropertyStr(ctx, pathObj, "root");
  JSValue base_val = JS_GetPropertyStr(ctx, pathObj, "base");
  JSValue name_val = JS_GetPropertyStr(ctx, pathObj, "name");
  JSValue ext_val = JS_GetPropertyStr(ctx, pathObj, "ext");

  const char* dir = NULL;
  const char* root = NULL;
  const char* base = NULL;
  const char* name = NULL;
  const char* ext = NULL;

  // Convert to C strings (NULL if undefined/null)
  if (JS_IsString(dir_val))
    dir = JS_ToCString(ctx, dir_val);
  if (JS_IsString(root_val))
    root = JS_ToCString(ctx, root_val);
  if (JS_IsString(base_val))
    base = JS_ToCString(ctx, base_val);
  if (JS_IsString(name_val))
    name = JS_ToCString(ctx, name_val);
  if (JS_IsString(ext_val))
    ext = JS_ToCString(ctx, ext_val);

  // Apply Node.js priority rules:
  // 1. pathObject.dir + pathObject.base takes precedence
  // 2. If no dir, use pathObject.root
  // 3. If no base, construct from name + ext

  if (dir && *dir) {
    // Use dir as the base
    strcpy(result, dir);
  } else if (root && *root) {
    // Use root as the base
    strcpy(result, root);
  }

  // Determine the filename part
  char filename[4096] = "";
  if (base && *base) {
    // Use base directly
    strcpy(filename, base);
  } else if (name || ext) {
    // Construct from name + ext
    if (name && *name) {
      strcpy(filename, name);
    }
    if (ext && *ext) {
      strcat(filename, ext);
    }
  }

  // Combine dir/root with filename
  if (strlen(filename) > 0) {
    if (strlen(result) > 0) {
      // Add separator if needed
      size_t result_len = strlen(result);
      if (!IS_PATH_SEPARATOR(result[result_len - 1])) {
        strcat(result, PATH_SEPARATOR_STR);
      }
    }
    strcat(result, filename);
  }

  // Handle empty result (return '.')
  if (result[0] == '\0') {
    strcpy(result, ".");
  }

  // Free all C strings
  if (dir)
    JS_FreeCString(ctx, dir);
  if (root)
    JS_FreeCString(ctx, root);
  if (base)
    JS_FreeCString(ctx, base);
  if (name)
    JS_FreeCString(ctx, name);
  if (ext)
    JS_FreeCString(ctx, ext);

  // Free JSValues
  JS_FreeValue(ctx, dir_val);
  JS_FreeValue(ctx, root_val);
  JS_FreeValue(ctx, base_val);
  JS_FreeValue(ctx, name_val);
  JS_FreeValue(ctx, ext_val);

  return JS_NewString(ctx, result);
}

// node:path.toNamespacedPath implementation
static JSValue js_path_to_namespaced(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "path.toNamespacedPath requires a path argument");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "path");

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path)
    return JS_EXCEPTION;

#ifdef _WIN32
  size_t len = strlen(path);

  // Check if already namespaced
  if (len >= 4 && path[0] == '\\' && path[1] == '\\' && path[2] == '?' && path[3] == '\\') {
    // Already namespaced, return as-is
    JSValue result = JS_NewString(ctx, path);
    JS_FreeCString(ctx, path);
    return result;
  }

  // Only namespace absolute paths
  if (!is_absolute_path(path)) {
    // Return relative paths unchanged
    JSValue result = JS_NewString(ctx, path);
    JS_FreeCString(ctx, path);
    return result;
  }

  char result[4096] = "";

  // Handle UNC paths: \\server\share -> \\?\UNC\server\share
  if (len >= 2 && path[0] == '\\' && path[1] == '\\') {
    strcpy(result, "\\\\?\\UNC\\");
    strcat(result, path + 2);  // Skip the initial \\
  } else {
    // Handle regular absolute paths: C:\path -> \\?\C:\path
    strcpy(result, "\\\\?\\");
    strcat(result, path);
  }

  JS_FreeCString(ctx, path);
  return JS_NewString(ctx, result);
#else
  // Unix: return path unchanged (no-op)
  JSValue result = JS_NewString(ctx, path);
  JS_FreeCString(ctx, path);
  return result;
#endif
}

// Initialize node:path module for CommonJS
JSValue JSRT_InitNodePath(JSContext* ctx) {
  JSValue path_obj = JS_NewObject(ctx);

  // Add methods
  JS_SetPropertyStr(ctx, path_obj, "join", JS_NewCFunction(ctx, js_path_join, "join", 0));
  JS_SetPropertyStr(ctx, path_obj, "resolve", JS_NewCFunction(ctx, js_path_resolve, "resolve", 0));
  JS_SetPropertyStr(ctx, path_obj, "normalize", JS_NewCFunction(ctx, js_path_normalize, "normalize", 1));
  JS_SetPropertyStr(ctx, path_obj, "isAbsolute", JS_NewCFunction(ctx, js_path_is_absolute, "isAbsolute", 1));
  JS_SetPropertyStr(ctx, path_obj, "dirname", JS_NewCFunction(ctx, js_path_dirname, "dirname", 1));
  JS_SetPropertyStr(ctx, path_obj, "basename", JS_NewCFunction(ctx, js_path_basename, "basename", 2));
  JS_SetPropertyStr(ctx, path_obj, "extname", JS_NewCFunction(ctx, js_path_extname, "extname", 1));
  JS_SetPropertyStr(ctx, path_obj, "relative", JS_NewCFunction(ctx, js_path_relative, "relative", 2));
  JS_SetPropertyStr(ctx, path_obj, "parse", JS_NewCFunction(ctx, js_path_parse, "parse", 1));
  JS_SetPropertyStr(ctx, path_obj, "format", JS_NewCFunction(ctx, js_path_format, "format", 1));
  JS_SetPropertyStr(ctx, path_obj, "toNamespacedPath",
                    JS_NewCFunction(ctx, js_path_to_namespaced, "toNamespacedPath", 1));

  // Add properties
  JS_SetPropertyStr(ctx, path_obj, "sep", JS_NewString(ctx, PATH_SEPARATOR_STR));
  char delimiter[2] = {PATH_DELIMITER, '\0'};
  JS_SetPropertyStr(ctx, path_obj, "delimiter", JS_NewString(ctx, delimiter));

  // Platform-specific properties - both posix and win32 should have all methods
  // Create both posix and win32 objects with full method implementations
  JSValue posix = JS_NewObject(ctx);
  JSValue win32 = JS_NewObject(ctx);

  // Copy all methods to both posix and win32
  JSValue join_val = JS_GetPropertyStr(ctx, path_obj, "join");
  JS_SetPropertyStr(ctx, posix, "join", JS_DupValue(ctx, join_val));
  JS_SetPropertyStr(ctx, win32, "join", JS_DupValue(ctx, join_val));
  JS_FreeValue(ctx, join_val);

  JSValue resolve_val = JS_GetPropertyStr(ctx, path_obj, "resolve");
  JS_SetPropertyStr(ctx, posix, "resolve", JS_DupValue(ctx, resolve_val));
  JS_SetPropertyStr(ctx, win32, "resolve", JS_DupValue(ctx, resolve_val));
  JS_FreeValue(ctx, resolve_val);

  JSValue normalize_val = JS_GetPropertyStr(ctx, path_obj, "normalize");
  JS_SetPropertyStr(ctx, posix, "normalize", JS_DupValue(ctx, normalize_val));
  JS_SetPropertyStr(ctx, win32, "normalize", JS_DupValue(ctx, normalize_val));
  JS_FreeValue(ctx, normalize_val);

  JSValue isAbsolute_val = JS_GetPropertyStr(ctx, path_obj, "isAbsolute");
  JS_SetPropertyStr(ctx, posix, "isAbsolute", JS_DupValue(ctx, isAbsolute_val));
  JS_SetPropertyStr(ctx, win32, "isAbsolute", JS_DupValue(ctx, isAbsolute_val));
  JS_FreeValue(ctx, isAbsolute_val);

  JSValue dirname_val = JS_GetPropertyStr(ctx, path_obj, "dirname");
  JS_SetPropertyStr(ctx, posix, "dirname", JS_DupValue(ctx, dirname_val));
  JS_SetPropertyStr(ctx, win32, "dirname", JS_DupValue(ctx, dirname_val));
  JS_FreeValue(ctx, dirname_val);

  JSValue basename_val = JS_GetPropertyStr(ctx, path_obj, "basename");
  JS_SetPropertyStr(ctx, posix, "basename", JS_DupValue(ctx, basename_val));
  JS_SetPropertyStr(ctx, win32, "basename", JS_DupValue(ctx, basename_val));
  JS_FreeValue(ctx, basename_val);

  JSValue extname_val = JS_GetPropertyStr(ctx, path_obj, "extname");
  JS_SetPropertyStr(ctx, posix, "extname", JS_DupValue(ctx, extname_val));
  JS_SetPropertyStr(ctx, win32, "extname", JS_DupValue(ctx, extname_val));
  JS_FreeValue(ctx, extname_val);

  JSValue relative_val = JS_GetPropertyStr(ctx, path_obj, "relative");
  JS_SetPropertyStr(ctx, posix, "relative", JS_DupValue(ctx, relative_val));
  JS_SetPropertyStr(ctx, win32, "relative", JS_DupValue(ctx, relative_val));
  JS_FreeValue(ctx, relative_val);

  JSValue parse_val = JS_GetPropertyStr(ctx, path_obj, "parse");
  JS_SetPropertyStr(ctx, posix, "parse", JS_DupValue(ctx, parse_val));
  JS_SetPropertyStr(ctx, win32, "parse", JS_DupValue(ctx, parse_val));
  JS_FreeValue(ctx, parse_val);

  JSValue format_val = JS_GetPropertyStr(ctx, path_obj, "format");
  JS_SetPropertyStr(ctx, posix, "format", JS_DupValue(ctx, format_val));
  JS_SetPropertyStr(ctx, win32, "format", JS_DupValue(ctx, format_val));
  JS_FreeValue(ctx, format_val);

  JSValue toNamespacedPath_val = JS_GetPropertyStr(ctx, path_obj, "toNamespacedPath");
  JS_SetPropertyStr(ctx, posix, "toNamespacedPath", JS_DupValue(ctx, toNamespacedPath_val));
  JS_SetPropertyStr(ctx, win32, "toNamespacedPath", JS_DupValue(ctx, toNamespacedPath_val));
  JS_FreeValue(ctx, toNamespacedPath_val);

  // Add platform-specific properties
  JS_SetPropertyStr(ctx, posix, "sep", JS_NewString(ctx, "/"));
  JS_SetPropertyStr(ctx, posix, "delimiter", JS_NewString(ctx, ":"));
  JS_SetPropertyStr(ctx, win32, "sep", JS_NewString(ctx, "\\"));
  JS_SetPropertyStr(ctx, win32, "delimiter", JS_NewString(ctx, ";"));

  // Set the objects on the main path object
  JS_SetPropertyStr(ctx, path_obj, "posix", posix);
  JS_SetPropertyStr(ctx, path_obj, "win32", win32);

  return path_obj;
}

// Initialize node:path module for ES modules
int js_node_path_init(JSContext* ctx, JSModuleDef* m) {
  JSValue path_module = JSRT_InitNodePath(ctx);

  // Export as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, path_module));

  // Export individual functions - properly manage memory
  JSValue join = JS_GetPropertyStr(ctx, path_module, "join");
  JS_SetModuleExport(ctx, m, "join", JS_DupValue(ctx, join));
  JS_FreeValue(ctx, join);

  JSValue resolve = JS_GetPropertyStr(ctx, path_module, "resolve");
  JS_SetModuleExport(ctx, m, "resolve", JS_DupValue(ctx, resolve));
  JS_FreeValue(ctx, resolve);

  JSValue dirname = JS_GetPropertyStr(ctx, path_module, "dirname");
  JS_SetModuleExport(ctx, m, "dirname", JS_DupValue(ctx, dirname));
  JS_FreeValue(ctx, dirname);

  JSValue basename = JS_GetPropertyStr(ctx, path_module, "basename");
  JS_SetModuleExport(ctx, m, "basename", JS_DupValue(ctx, basename));
  JS_FreeValue(ctx, basename);

  JSValue extname = JS_GetPropertyStr(ctx, path_module, "extname");
  JS_SetModuleExport(ctx, m, "extname", JS_DupValue(ctx, extname));
  JS_FreeValue(ctx, extname);

  JSValue normalize = JS_GetPropertyStr(ctx, path_module, "normalize");
  JS_SetModuleExport(ctx, m, "normalize", JS_DupValue(ctx, normalize));
  JS_FreeValue(ctx, normalize);

  JSValue isAbsolute = JS_GetPropertyStr(ctx, path_module, "isAbsolute");
  JS_SetModuleExport(ctx, m, "isAbsolute", JS_DupValue(ctx, isAbsolute));
  JS_FreeValue(ctx, isAbsolute);

  JSValue relative = JS_GetPropertyStr(ctx, path_module, "relative");
  JS_SetModuleExport(ctx, m, "relative", JS_DupValue(ctx, relative));
  JS_FreeValue(ctx, relative);

  JSValue sep = JS_GetPropertyStr(ctx, path_module, "sep");
  JS_SetModuleExport(ctx, m, "sep", JS_DupValue(ctx, sep));
  JS_FreeValue(ctx, sep);

  JSValue delimiter = JS_GetPropertyStr(ctx, path_module, "delimiter");
  JS_SetModuleExport(ctx, m, "delimiter", JS_DupValue(ctx, delimiter));
  JS_FreeValue(ctx, delimiter);

  JSValue parse = JS_GetPropertyStr(ctx, path_module, "parse");
  JS_SetModuleExport(ctx, m, "parse", JS_DupValue(ctx, parse));
  JS_FreeValue(ctx, parse);

  JSValue format = JS_GetPropertyStr(ctx, path_module, "format");
  JS_SetModuleExport(ctx, m, "format", JS_DupValue(ctx, format));
  JS_FreeValue(ctx, format);

  JSValue toNamespacedPath = JS_GetPropertyStr(ctx, path_module, "toNamespacedPath");
  JS_SetModuleExport(ctx, m, "toNamespacedPath", JS_DupValue(ctx, toNamespacedPath));
  JS_FreeValue(ctx, toNamespacedPath);

  JS_FreeValue(ctx, path_module);
  return 0;
}