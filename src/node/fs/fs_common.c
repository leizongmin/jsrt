#include "fs_common.h"

// Forward declaration
static JSValue js_buffer_to_string_simple(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

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

// Helper to create a Buffer object with proper _isBuffer flag
JSValue create_buffer_from_data(JSContext* ctx, const uint8_t* data, size_t size) {
  // Create an ArrayBuffer with the data
  JSValue array_buffer = JS_NewArrayBufferCopy(ctx, data, size);
  if (JS_IsException(array_buffer)) {
    return JS_EXCEPTION;
  }

  // Create Uint8Array from ArrayBuffer
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");
  JSValue uint8_array = JS_CallConstructor(ctx, uint8_array_ctor, 1, &array_buffer);

  // Add Buffer-specific properties
  if (!JS_IsException(uint8_array)) {
    // Add a special marker to identify this as a Buffer
    JS_SetPropertyStr(ctx, uint8_array, "_isBuffer", JS_TRUE);

    // Add toString method (simplified version)
    JSValue toString_func = JS_NewCFunction(ctx, js_buffer_to_string_simple, "toString", 1);
    JS_SetPropertyStr(ctx, uint8_array, "toString", toString_func);
  }

  JS_FreeValue(ctx, array_buffer);
  JS_FreeValue(ctx, uint8_array_ctor);
  JS_FreeValue(ctx, global);
  return uint8_array;
}

// Simple toString implementation for Buffer
static JSValue js_buffer_to_string_simple(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  size_t size;
  uint8_t* data = JS_GetArrayBuffer(ctx, &size, this_val);
  if (!data) {
    // Try to get as TypedArray
    size_t byte_offset;
    JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, this_val, &byte_offset, &size, NULL);
    if (!JS_IsException(array_buffer)) {
      size_t buffer_size;
      uint8_t* buffer = JS_GetArrayBuffer(ctx, &buffer_size, array_buffer);
      JS_FreeValue(ctx, array_buffer);
      if (buffer) {
        data = buffer + byte_offset;
      }
    }
  }

  if (!data) {
    return JS_ThrowTypeError(ctx, "Invalid buffer");
  }

  // Convert bytes to string (assuming UTF-8)
  return JS_NewStringLen(ctx, (const char*)data, size);
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