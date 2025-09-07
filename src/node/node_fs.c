#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>  // For _mkdir on Windows
#include <io.h>      // For access function
#else
#include <unistd.h>
#endif
#include "../util/debug.h"
#include "node_modules.h"

// Cross-platform mkdir wrapper
#ifdef _WIN32
#define JSRT_MKDIR(path, mode) _mkdir(path)
#else
#define JSRT_MKDIR(path, mode) mkdir(path, mode)
#endif

// Helper to convert errno to Node.js error code
static const char* errno_to_node_code(int err) {
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
static JSValue create_buffer_from_data(JSContext* ctx, const uint8_t* data, size_t size) {
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
static JSValue create_fs_error(JSContext* ctx, int err, const char* syscall, const char* path) {
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

// fs.readFileSync(path[, options])
static JSValue js_fs_read_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  FILE* file = fopen(path, "rb");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (size < 0) {
    fclose(file);
    JSValue error = create_fs_error(ctx, errno, "stat", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  // Read file content
  char* buffer = malloc(size + 1);
  if (!buffer) {
    fclose(file);
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  size_t read_size = fread(buffer, 1, size, file);
  fclose(file);

  if (read_size != (size_t)size) {
    free(buffer);
    JSValue error = create_fs_error(ctx, errno, "read", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  buffer[size] = '\0';

  // Enhanced Buffer integration - check for encoding options
  JSValue result;
  bool return_string = false;

  if (argc > 1) {
    if (JS_IsString(argv[1])) {
      // Second argument is a string encoding (e.g., 'utf8')
      return_string = true;
    } else if (JS_IsObject(argv[1])) {
      // Second argument is an options object
      JSValue encoding = JS_GetPropertyStr(ctx, argv[1], "encoding");
      if (JS_IsString(encoding)) {
        return_string = true;
      }
      JS_FreeValue(ctx, encoding);
    }
  }

  if (return_string) {
    // Return string for text data when encoding is specified
    result = JS_NewStringLen(ctx, buffer, size);
  } else {
    // Return Buffer for binary data when no encoding specified
    result = create_buffer_from_data(ctx, (const uint8_t*)buffer, size);
  }

  free(buffer);
  JS_FreeCString(ctx, path);

  return result;
}

// fs.writeFileSync(file, data[, options])
static JSValue js_fs_write_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "file and data are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  const uint8_t* data;
  size_t data_len;
  bool is_binary = false;

  if (JS_IsString(argv[1])) {
    // String data
    const char* str_data = JS_ToCStringLen(ctx, &data_len, argv[1]);
    if (!str_data) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
    data = (const uint8_t*)str_data;
  } else {
    // Enhanced Buffer handling - check if it's a TypedArray (Buffer)
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue uint8_array_ctor = JS_GetPropertyStr(ctx, global, "Uint8Array");

    if (JS_IsInstanceOf(ctx, argv[1], uint8_array_ctor) > 0) {
      // It's a Buffer/Uint8Array - get binary data directly
      size_t byte_offset;
      JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[1], &byte_offset, &data_len, NULL);
      if (JS_IsException(array_buffer)) {
        JS_FreeValue(ctx, uint8_array_ctor);
        JS_FreeValue(ctx, global);
        JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
      }

      size_t buffer_size;
      uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, array_buffer);
      JS_FreeValue(ctx, array_buffer);

      if (!buffer_data) {
        JS_FreeValue(ctx, uint8_array_ctor);
        JS_FreeValue(ctx, global);
        JS_FreeCString(ctx, path);
        return JS_ThrowTypeError(ctx, "Failed to get buffer data");
      }

      data = buffer_data + byte_offset;
      is_binary = true;
    } else {
      // For non-Buffer, non-TypedArray objects, reject
      JS_FreeValue(ctx, uint8_array_ctor);
      JS_FreeValue(ctx, global);
      JS_FreeCString(ctx, path);
      return JS_ThrowTypeError(ctx, "data must be string, Buffer, or TypedArray");
    }

    JS_FreeValue(ctx, uint8_array_ctor);
    JS_FreeValue(ctx, global);
  }

  FILE* file = fopen(path, "wb");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JS_FreeCString(ctx, path);
    if (!is_binary) {
      JS_FreeCString(ctx, (const char*)data);
    }
    return JS_Throw(ctx, error);
  }

  size_t written = fwrite(data, 1, data_len, file);
  fclose(file);

  if (written != data_len) {
    JSValue error = create_fs_error(ctx, errno, "write", path);
    JS_FreeCString(ctx, path);
    if (!is_binary) {
      JS_FreeCString(ctx, (const char*)data);
    }
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  if (!is_binary) {
    JS_FreeCString(ctx, (const char*)data);
  }

  return JS_UNDEFINED;
}

// fs.existsSync(path)
static JSValue js_fs_exists_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int result = access(path, F_OK);
  JS_FreeCString(ctx, path);

  return JS_NewBool(ctx, result == 0);
}

// fs.Stats.isFile() method
static JSValue js_fs_stat_is_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue mode_val = JS_GetPropertyStr(ctx, this_val, "_mode");
  if (JS_IsException(mode_val)) {
    return JS_EXCEPTION;
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, mode_val) < 0) {
    JS_FreeValue(ctx, mode_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, mode_val);

  return JS_NewBool(ctx, S_ISREG(mode));
}

// fs.Stats.isDirectory() method
static JSValue js_fs_stat_is_directory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue mode_val = JS_GetPropertyStr(ctx, this_val, "_mode");
  if (JS_IsException(mode_val)) {
    return JS_EXCEPTION;
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, mode_val) < 0) {
    JS_FreeValue(ctx, mode_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, mode_val);

  return JS_NewBool(ctx, S_ISDIR(mode));
}

// fs.statSync(path)
static JSValue js_fs_stat_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    JSValue error = create_fs_error(ctx, errno, "stat", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JSValue stats = JS_NewObject(ctx);

  // Basic stat properties
  JS_SetPropertyStr(ctx, stats, "size", JS_NewInt64(ctx, st.st_size));
  JS_SetPropertyStr(ctx, stats, "mode", JS_NewInt32(ctx, st.st_mode));
  JS_SetPropertyStr(ctx, stats, "uid", JS_NewInt32(ctx, st.st_uid));
  JS_SetPropertyStr(ctx, stats, "gid", JS_NewInt32(ctx, st.st_gid));

  // Time properties (in milliseconds)
  JS_SetPropertyStr(ctx, stats, "atime", JS_NewDate(ctx, st.st_atime * 1000.0));
  JS_SetPropertyStr(ctx, stats, "mtime", JS_NewDate(ctx, st.st_mtime * 1000.0));
  JS_SetPropertyStr(ctx, stats, "ctime", JS_NewDate(ctx, st.st_ctime * 1000.0));

  // Helper methods
  JSValue is_file_func = JS_NewCFunction(ctx, js_fs_stat_is_file, "isFile", 0);
  JSValue is_dir_func = JS_NewCFunction(ctx, js_fs_stat_is_directory, "isDirectory", 0);

  // Store the mode for the methods to access
  JS_SetPropertyStr(ctx, stats, "_mode", JS_NewInt32(ctx, st.st_mode));

  JS_SetPropertyStr(ctx, stats, "isFile", is_file_func);
  JS_SetPropertyStr(ctx, stats, "isDirectory", is_dir_func);

  JS_FreeCString(ctx, path);
  return stats;
}

// fs.readdirSync(path)
static JSValue js_fs_readdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  DIR* dir = opendir(path);
  if (!dir) {
    JSValue error = create_fs_error(ctx, errno, "scandir", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JSValue files = JS_NewArray(ctx);
  struct dirent* entry;
  int index = 0;

  while ((entry = readdir(dir)) != NULL) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    JS_SetPropertyUint32(ctx, files, index++, JS_NewString(ctx, entry->d_name));
  }

  closedir(dir);
  JS_FreeCString(ctx, path);

  return files;
}

// fs.mkdirSync(path[, options])
static JSValue js_fs_mkdir_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  mode_t mode = 0755;  // Default permissions

  // Check for mode in options
  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue mode_val = JS_GetPropertyStr(ctx, argv[1], "mode");
    if (JS_IsNumber(mode_val)) {
      int32_t mode_int;
      JS_ToInt32(ctx, &mode_int, mode_val);
      mode = (mode_t)mode_int;
    }
    JS_FreeValue(ctx, mode_val);
  }

  if (JSRT_MKDIR(path, mode) != 0) {
    JSValue error = create_fs_error(ctx, errno, "mkdir", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.unlinkSync(path)
static JSValue js_fs_unlink_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (unlink(path) != 0) {
    JSValue error = create_fs_error(ctx, errno, "unlink", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.readFile(path, callback) - callback-based async version
static JSValue js_fs_read_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "path and callback are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[1])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // For simplicity, implement as sync operation but call callback asynchronously
  // In a full implementation, this would use libuv for true async I/O

  FILE* file = fopen(path, "rb");
  if (!file) {
    // Call callback with error
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JSValue callback_args[] = {error, JS_UNDEFINED};
    JS_Call(ctx, argv[1], JS_UNDEFINED, 2, callback_args);
    JS_FreeValue(ctx, error);
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
  }

  // Get file size and read content
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (size < 0) {
    fclose(file);
    JSValue error = create_fs_error(ctx, errno, "stat", path);
    JSValue callback_args[] = {error, JS_UNDEFINED};
    JS_Call(ctx, argv[1], JS_UNDEFINED, 2, callback_args);
    JS_FreeValue(ctx, error);
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
  }

  char* buffer = malloc(size + 1);
  if (!buffer) {
    fclose(file);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Out of memory"));
    JSValue callback_args[] = {error, JS_UNDEFINED};
    JS_Call(ctx, argv[1], JS_UNDEFINED, 2, callback_args);
    JS_FreeValue(ctx, error);
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
  }

  size_t read_size = fread(buffer, 1, size, file);
  fclose(file);

  if (read_size != (size_t)size) {
    free(buffer);
    JSValue error = create_fs_error(ctx, errno, "read", path);
    JSValue callback_args[] = {error, JS_UNDEFINED};
    JS_Call(ctx, argv[1], JS_UNDEFINED, 2, callback_args);
    JS_FreeValue(ctx, error);
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
  }

  buffer[size] = '\0';

  // Create Buffer result
  JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
  JSValue buffer_result = JS_UNDEFINED;

  if (!JS_IsException(buffer_module)) {
    JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
    JSValue from_method = JS_GetPropertyStr(ctx, buffer_class, "from");
    if (JS_IsFunction(ctx, from_method)) {
      JSValue str_arg = JS_NewString(ctx, buffer);
      JSValue from_args[] = {str_arg};
      buffer_result = JS_Call(ctx, from_method, buffer_class, 1, from_args);
      JS_FreeValue(ctx, str_arg);
    }
    JS_FreeValue(ctx, from_method);
    JS_FreeValue(ctx, buffer_class);
    JS_FreeValue(ctx, buffer_module);
  }

  // Call callback with success
  JSValue callback_args[] = {JS_NULL, buffer_result};
  JS_Call(ctx, argv[1], JS_UNDEFINED, 2, callback_args);
  JS_FreeValue(ctx, buffer_result);

  free(buffer);
  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.writeFile(path, data, callback) - callback-based async version
static JSValue js_fs_write_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "path, data, and callback are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[2])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Get data to write
  const char* data = NULL;
  size_t data_len = 0;

  if (JS_IsString(argv[1])) {
    data = JS_ToCString(ctx, argv[1]);
    if (data) {
      data_len = strlen(data);
    }
  } else {
    // Try to handle Buffer objects
    JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
    if (!JS_IsException(buffer_module)) {
      JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
      JSValue is_buffer = JS_GetPropertyStr(ctx, buffer_class, "isBuffer");
      if (JS_IsFunction(ctx, is_buffer)) {
        JSValue check_args[] = {argv[1]};
        JSValue is_buf_result = JS_Call(ctx, is_buffer, buffer_class, 1, check_args);
        if (JS_ToBool(ctx, is_buf_result)) {
          // It's a buffer, convert to string for now
          JSValue to_string = JS_GetPropertyStr(ctx, argv[1], "toString");
          if (JS_IsFunction(ctx, to_string)) {
            JSValue str_result = JS_Call(ctx, to_string, argv[1], 0, NULL);
            data = JS_ToCString(ctx, str_result);
            if (data) {
              data_len = strlen(data);
            }
            JS_FreeValue(ctx, str_result);
          }
          JS_FreeValue(ctx, to_string);
        }
        JS_FreeValue(ctx, is_buf_result);
      }
      JS_FreeValue(ctx, is_buffer);
      JS_FreeValue(ctx, buffer_class);
      JS_FreeValue(ctx, buffer_module);
    }
  }

  if (!data) {
    JS_FreeCString(ctx, path);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "data must be a string or Buffer"));
    JSValue callback_args[] = {error};
    JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, error);
    return JS_UNDEFINED;
  }

  // Write file
  FILE* file = fopen(path, "wb");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JSValue callback_args[] = {error};
    JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, error);
    JS_FreeCString(ctx, data);
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
  }

  size_t written = fwrite(data, 1, data_len, file);
  fclose(file);

  if (written != data_len) {
    JSValue error = create_fs_error(ctx, errno, "write", path);
    JSValue callback_args[] = {error};
    JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, error);
  } else {
    // Success - call callback with null error
    JSValue callback_args[] = {JS_NULL};
    JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
  }

  JS_FreeCString(ctx, data);
  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// Module initialization
JSValue JSRT_InitNodeFs(JSContext* ctx) {
  JSValue fs_module = JS_NewObject(ctx);

  // Synchronous file operations
  JS_SetPropertyStr(ctx, fs_module, "readFileSync", JS_NewCFunction(ctx, js_fs_read_file_sync, "readFileSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "writeFileSync", JS_NewCFunction(ctx, js_fs_write_file_sync, "writeFileSync", 3));
  JS_SetPropertyStr(ctx, fs_module, "existsSync", JS_NewCFunction(ctx, js_fs_exists_sync, "existsSync", 1));
  JS_SetPropertyStr(ctx, fs_module, "statSync", JS_NewCFunction(ctx, js_fs_stat_sync, "statSync", 1));
  JS_SetPropertyStr(ctx, fs_module, "readdirSync", JS_NewCFunction(ctx, js_fs_readdir_sync, "readdirSync", 1));
  JS_SetPropertyStr(ctx, fs_module, "mkdirSync", JS_NewCFunction(ctx, js_fs_mkdir_sync, "mkdirSync", 2));
  JS_SetPropertyStr(ctx, fs_module, "unlinkSync", JS_NewCFunction(ctx, js_fs_unlink_sync, "unlinkSync", 1));

  // Asynchronous file operations
  JS_SetPropertyStr(ctx, fs_module, "readFile", JS_NewCFunction(ctx, js_fs_read_file, "readFile", 2));
  JS_SetPropertyStr(ctx, fs_module, "writeFile", JS_NewCFunction(ctx, js_fs_write_file, "writeFile", 3));

  // Constants
  JSValue constants = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, constants, "F_OK", JS_NewInt32(ctx, F_OK));
  JS_SetPropertyStr(ctx, constants, "R_OK", JS_NewInt32(ctx, R_OK));
  JS_SetPropertyStr(ctx, constants, "W_OK", JS_NewInt32(ctx, W_OK));
  JS_SetPropertyStr(ctx, constants, "X_OK", JS_NewInt32(ctx, X_OK));
  JS_SetPropertyStr(ctx, fs_module, "constants", constants);

  return fs_module;
}

// ES Module support
int js_node_fs_init(JSContext* ctx, JSModuleDef* m) {
  JSValue fs_module = JSRT_InitNodeFs(ctx);

  // Export individual functions
  JS_SetModuleExport(ctx, m, "readFileSync", JS_GetPropertyStr(ctx, fs_module, "readFileSync"));
  JS_SetModuleExport(ctx, m, "writeFileSync", JS_GetPropertyStr(ctx, fs_module, "writeFileSync"));
  JS_SetModuleExport(ctx, m, "readFile", JS_GetPropertyStr(ctx, fs_module, "readFile"));
  JS_SetModuleExport(ctx, m, "writeFile", JS_GetPropertyStr(ctx, fs_module, "writeFile"));
  JS_SetModuleExport(ctx, m, "existsSync", JS_GetPropertyStr(ctx, fs_module, "existsSync"));
  JS_SetModuleExport(ctx, m, "statSync", JS_GetPropertyStr(ctx, fs_module, "statSync"));
  JS_SetModuleExport(ctx, m, "readdirSync", JS_GetPropertyStr(ctx, fs_module, "readdirSync"));
  JS_SetModuleExport(ctx, m, "mkdirSync", JS_GetPropertyStr(ctx, fs_module, "mkdirSync"));
  JS_SetModuleExport(ctx, m, "unlinkSync", JS_GetPropertyStr(ctx, fs_module, "unlinkSync"));
  JS_SetModuleExport(ctx, m, "constants", JS_GetPropertyStr(ctx, fs_module, "constants"));

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", fs_module);

  return 0;
}