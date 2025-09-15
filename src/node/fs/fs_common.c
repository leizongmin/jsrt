#include "fs_common.h"

// Helper function to create directories recursively
int mkdir_recursive(const char* path, mode_t mode) {
  char* path_copy = strdup(path);
  if (!path_copy) {
    return -1;
  }

  char* p = path_copy;

  // Skip leading slash on Unix or drive letter on Windows
  if (*p == '/') {
    p++;
  } else if (strlen(p) > 2 && p[1] == ':' && (p[2] == '\\' || p[2] == '/')) {
    p += 3;  // Skip "C:\" or "C:/"
  }

  while (*p) {
    // Find next directory separator
    while (*p && *p != '/' && *p != '\\') {
      p++;
    }

    if (*p) {
      *p = '\0';  // Temporarily terminate string

      // Try to create this directory level
      if (JSRT_MKDIR(path_copy, mode) != 0) {
        if (errno != EEXIST) {
          free(path_copy);
          return -1;
        }
      }

      *p = '/';  // Restore separator
      p++;
    }
  }

  // Create the final directory
  int result = JSRT_MKDIR(path_copy, mode);
  if (result != 0 && errno == EEXIST) {
    result = 0;  // Directory already exists, that's OK
  }

  free(path_copy);
  return result;
}

// Helper to convert errno to Node.js error code
const char* errno_to_node_code(int err) {
  switch (err) {
    case ENOENT:
      return "ENOENT";
    case EACCES:
      return "EACCES";
    case EEXIST:
      return "EEXIST";
    case EISDIR:
      return "EISDIR";
    case ENOTDIR:
      return "ENOTDIR";
    case EMFILE:
      return "EMFILE";
    case ENFILE:
      return "ENFILE";
    case ENOSPC:
      return "ENOSPC";
    default:
      return "UNKNOWN";
  }
}

// Helper to create a Buffer-like object without circular dependencies
JSValue create_buffer_from_data(JSContext* ctx, const uint8_t* data, size_t size) {
  // Create an ArrayBuffer with the data
  JSValue array_buffer = JS_NewArrayBufferCopy(ctx, data, size);
  if (JS_IsException(array_buffer)) {
    return JS_EXCEPTION;
  }

  // Create a Uint8Array from the ArrayBuffer
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JSValue uint8_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

  JS_FreeValue(ctx, array_buffer);
  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, global);

  // Add a toString method that converts bytes to string (like Buffer does)
  if (!JS_IsException(uint8_array)) {
    // Create a simple toString function that converts the byte array to string
    const char* toString_code =
        "(function() {"
        "  let str = '';"
        "  for (let i = 0; i < this.length; i++) {"
        "    str += String.fromCharCode(this[i]);"
        "  }"
        "  return str;"
        "})";

    JSValue toString_func =
        JS_Eval(ctx, toString_code, strlen(toString_code), "<buffer_toString>", JS_EVAL_TYPE_GLOBAL);
    if (!JS_IsException(toString_func)) {
      JS_SetPropertyStr(ctx, uint8_array, "toString", toString_func);
    } else {
      JS_FreeValue(ctx, toString_func);
    }
  }

  return uint8_array;
}

// Helper to create Node.js fs error
JSValue create_fs_error(JSContext* ctx, int err, const char* syscall, const char* path) {
  JSValue error = JS_NewError(ctx);

  char message[256];
  if (path) {
    snprintf(message, sizeof(message), "%s: %s, %s '%s'", errno_to_node_code(err), strerror(err), syscall, path);
  } else {
    snprintf(message, sizeof(message), "%s: %s, %s", errno_to_node_code(err), strerror(err), syscall);
  }

  JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));
  JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, errno_to_node_code(err)));
  JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, err));
  JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, syscall));
  if (path) {
    JS_SetPropertyStr(ctx, error, "path", JS_NewString(ctx, path));
  }

  return error;
}