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
  // Windows: C:\ or \\ (UNC) or /
  if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) {
    return path[1] == ':';
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

  if (is_absolute) {
    strcpy(result, PATH_SEPARATOR_STR);
  }

  for (int i = 0; i < segment_count; i++) {
    if (i > 0) {
      strcat(result, PATH_SEPARATOR_STR);
    }
    strcat(result, segments[i]);
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

    JS_FreeCString(ctx, part);
  }

  if (result[0] == '\0') {
    strcpy(result, ".");
  }

  // Normalize separators
  char* normalized = normalize_separators(result);
  free(result);

  JSValue ret = JS_NewString(ctx, normalized);
  free(normalized);
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

  // Implementation: Remove duplicate slashes, resolve . and ..
  char* normalized = normalize_separators(path);

  // TODO: Implement full normalization logic

  JSValue ret = JS_NewString(ctx, normalized);
  free(normalized);
  JS_FreeCString(ctx, path);
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

  // TODO: Implement relative path calculation

  return JS_NewString(ctx, "");
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

  // Add properties
  JS_SetPropertyStr(ctx, path_obj, "sep", JS_NewString(ctx, PATH_SEPARATOR_STR));
  char delimiter[2] = {PATH_DELIMITER, '\0'};
  JS_SetPropertyStr(ctx, path_obj, "delimiter", JS_NewString(ctx, delimiter));

  // Platform-specific properties
#ifdef _WIN32
  JSValue win32 = JS_DupValue(ctx, path_obj);
  JS_SetPropertyStr(ctx, path_obj, "win32", win32);
  JSValue posix = JS_NewObject(ctx);  // Minimal posix implementation
  JS_SetPropertyStr(ctx, path_obj, "posix", posix);
#else
  JSValue posix = JS_DupValue(ctx, path_obj);
  JS_SetPropertyStr(ctx, path_obj, "posix", posix);
  JSValue win32 = JS_NewObject(ctx);  // Minimal win32 implementation
  JS_SetPropertyStr(ctx, path_obj, "win32", win32);
#endif

  return path_obj;
}

// Initialize node:path module for ES modules
int js_node_path_init(JSContext* ctx, JSModuleDef* m) {
  JSValue path_module = JSRT_InitNodePath(ctx);

  // Export as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, path_module));

  // Export individual functions
  JSValue join = JS_GetPropertyStr(ctx, path_module, "join");
  JS_SetModuleExport(ctx, m, "join", join);

  JSValue resolve = JS_GetPropertyStr(ctx, path_module, "resolve");
  JS_SetModuleExport(ctx, m, "resolve", resolve);

  JSValue dirname = JS_GetPropertyStr(ctx, path_module, "dirname");
  JS_SetModuleExport(ctx, m, "dirname", dirname);

  JSValue basename = JS_GetPropertyStr(ctx, path_module, "basename");
  JS_SetModuleExport(ctx, m, "basename", basename);

  JSValue extname = JS_GetPropertyStr(ctx, path_module, "extname");
  JS_SetModuleExport(ctx, m, "extname", extname);

  JSValue normalize = JS_GetPropertyStr(ctx, path_module, "normalize");
  JS_SetModuleExport(ctx, m, "normalize", normalize);

  JSValue isAbsolute = JS_GetPropertyStr(ctx, path_module, "isAbsolute");
  JS_SetModuleExport(ctx, m, "isAbsolute", isAbsolute);

  JSValue relative = JS_GetPropertyStr(ctx, path_module, "relative");
  JS_SetModuleExport(ctx, m, "relative", relative);

  JSValue sep = JS_GetPropertyStr(ctx, path_module, "sep");
  JS_SetModuleExport(ctx, m, "sep", sep);

  JSValue delimiter = JS_GetPropertyStr(ctx, path_module, "delimiter");
  JS_SetModuleExport(ctx, m, "delimiter", delimiter);

  JS_FreeValue(ctx, path_module);
  return 0;
}