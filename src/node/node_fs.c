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

  // Check if encoding is specified (simplified - always return string for now)
  JSValue result;
  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue encoding = JS_GetPropertyStr(ctx, argv[1], "encoding");
    if (JS_IsString(encoding)) {
      result = JS_NewStringLen(ctx, buffer, size);
    } else {
      // Return Buffer for binary data
      JSValue buffer_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Buffer");
      if (!JS_IsUndefined(buffer_ctor)) {
        JSValue from_func = JS_GetPropertyStr(ctx, buffer_ctor, "from");
        if (JS_IsFunction(ctx, from_func)) {
          JSValue args[1] = {JS_NewStringLen(ctx, buffer, size)};
          result = JS_Call(ctx, from_func, buffer_ctor, 1, args);
          JS_FreeValue(ctx, args[0]);
        } else {
          result = JS_NewStringLen(ctx, buffer, size);
        }
        JS_FreeValue(ctx, from_func);
      } else {
        result = JS_NewStringLen(ctx, buffer, size);
      }
      JS_FreeValue(ctx, buffer_ctor);
    }
    JS_FreeValue(ctx, encoding);
  } else {
    result = JS_NewStringLen(ctx, buffer, size);
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

  const char* data;
  size_t data_len;

  if (JS_IsString(argv[1])) {
    data = JS_ToCStringLen(ctx, &data_len, argv[1]);
    if (!data) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
  } else {
    // Try to handle Buffer objects
    JSValue to_string = JS_GetPropertyStr(ctx, argv[1], "toString");
    if (JS_IsFunction(ctx, to_string)) {
      JSValue str_result = JS_Call(ctx, to_string, argv[1], 0, NULL);
      data = JS_ToCStringLen(ctx, &data_len, str_result);
      JS_FreeValue(ctx, str_result);
      JS_FreeValue(ctx, to_string);
      if (!data) {
        JS_FreeCString(ctx, path);
        return JS_EXCEPTION;
      }
    } else {
      JS_FreeValue(ctx, to_string);
      JS_FreeCString(ctx, path);
      return JS_ThrowTypeError(ctx, "data must be string or Buffer");
    }
  }

  FILE* file = fopen(path, "wb");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JS_FreeCString(ctx, path);
    JS_FreeCString(ctx, data);
    return JS_Throw(ctx, error);
  }

  size_t written = fwrite(data, 1, data_len, file);
  fclose(file);

  if (written != data_len) {
    JSValue error = create_fs_error(ctx, errno, "write", path);
    JS_FreeCString(ctx, path);
    JS_FreeCString(ctx, data);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  JS_FreeCString(ctx, data);

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
  JSValue is_file_func = JS_NewCFunction(ctx, NULL, "isFile", 0);
  JSValue is_dir_func = JS_NewCFunction(ctx, NULL, "isDirectory", 0);

  // Simplified implementation - store the mode for later checks
  JS_SetPropertyStr(ctx, is_file_func, "_mode", JS_NewInt32(ctx, st.st_mode));
  JS_SetPropertyStr(ctx, is_dir_func, "_mode", JS_NewInt32(ctx, st.st_mode));

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