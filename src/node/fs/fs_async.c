#include "fs_common.h"

// fs.appendFile(path, data, callback) - callback-based async version
JSValue js_fs_append_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

  // Get data to append
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
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    return JS_UNDEFINED;
  }

  // Append to file
  FILE* file = fopen(path, "ab");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JSValue callback_args[] = {error};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
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
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
  } else {
    // Success - call callback with null error
    JSValue callback_args[] = {JS_NULL};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
  }

  JS_FreeCString(ctx, data);
  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.copyFile(src, dest, callback) - callback-based async version
JSValue js_fs_copy_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "src, dest, and callback are required");
  }

  const char* src = JS_ToCString(ctx, argv[0]);
  if (!src) {
    return JS_EXCEPTION;
  }

  const char* dest = JS_ToCString(ctx, argv[1]);
  if (!dest) {
    JS_FreeCString(ctx, src);
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[2])) {
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Open source file for reading
  FILE* src_file = fopen(src, "rb");
  if (!src_file) {
    JSValue error = create_fs_error(ctx, errno, "open", src);
    JSValue callback_args[] = {error};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_UNDEFINED;
  }

  // Open destination file for writing
  FILE* dest_file = fopen(dest, "wb");
  if (!dest_file) {
    fclose(src_file);
    JSValue error = create_fs_error(ctx, errno, "open", dest);
    JSValue callback_args[] = {error};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_UNDEFINED;
  }

  // Copy file content in chunks
  char buffer[8192];
  size_t bytes_read;
  bool copy_success = true;
  JSValue error = JS_NULL;

  while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
    size_t bytes_written = fwrite(buffer, 1, bytes_read, dest_file);
    if (bytes_written != bytes_read) {
      error = create_fs_error(ctx, errno, "write", dest);
      copy_success = false;
      break;
    }
  }

  // Check for read errors
  if (copy_success && ferror(src_file)) {
    error = create_fs_error(ctx, errno, "read", src);
    copy_success = false;
  }

  fclose(src_file);
  fclose(dest_file);

  // Call callback
  if (copy_success) {
    JSValue callback_args[] = {JS_NULL};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
  } else {
    JSValue callback_args[] = {error};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
  }

  JS_FreeCString(ctx, src);
  JS_FreeCString(ctx, dest);
  return JS_UNDEFINED;
}

// fs.rename(oldPath, newPath, callback) - callback-based async version
JSValue js_fs_rename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "oldPath, newPath, and callback are required");
  }

  const char* old_path = JS_ToCString(ctx, argv[0]);
  if (!old_path) {
    return JS_EXCEPTION;
  }

  const char* new_path = JS_ToCString(ctx, argv[1]);
  if (!new_path) {
    JS_FreeCString(ctx, old_path);
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[2])) {
    JS_FreeCString(ctx, old_path);
    JS_FreeCString(ctx, new_path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  if (rename(old_path, new_path) != 0) {
    JSValue error = create_fs_error(ctx, errno, "rename", old_path);
    JSValue callback_args[] = {error};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
  } else {
    // Success - call callback with null error
    JSValue callback_args[] = {JS_NULL};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
  }

  JS_FreeCString(ctx, old_path);
  JS_FreeCString(ctx, new_path);
  return JS_UNDEFINED;
}

// fs.rmdir(path, callback) - callback-based async version
JSValue js_fs_rmdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

  if (rmdir(path) != 0) {
    JSValue error = create_fs_error(ctx, errno, "rmdir", path);
    JSValue callback_args[] = {error};
    JS_Call(ctx, argv[1], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, error);
  } else {
    // Success - call callback with null error
    JSValue callback_args[] = {JS_NULL};
    JS_Call(ctx, argv[1], JS_UNDEFINED, 1, callback_args);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.access(path, mode, callback) - callback-based async version
JSValue js_fs_access(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "path and callback are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int mode = F_OK;  // Default to checking existence
  JSValue callback;

  if (argc == 2) {
    // fs.access(path, callback)
    callback = argv[1];
  } else {
    // fs.access(path, mode, callback)
    if (JS_IsNumber(argv[1])) {
      int32_t mode_int;
      JS_ToInt32(ctx, &mode_int, argv[1]);
      mode = mode_int;
    }
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  if (access(path, mode) != 0) {
    JSValue error = create_fs_error(ctx, errno, "access", path);
    JSValue callback_args[] = {error};
    JS_Call(ctx, callback, JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, error);
  } else {
    // Success - call callback with null error
    JSValue callback_args[] = {JS_NULL};
    JS_Call(ctx, callback, JS_UNDEFINED, 1, callback_args);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.readFile(path, callback) - callback-based async version
JSValue js_fs_read_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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
JSValue js_fs_write_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    return JS_UNDEFINED;
  }

  // Write file
  FILE* file = fopen(path, "wb");
  if (!file) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JSValue callback_args[] = {error};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
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
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
  } else {
    // Success - call callback with null error
    JSValue callback_args[] = {JS_NULL};
    JSValue ret = JS_Call(ctx, argv[2], JS_UNDEFINED, 1, callback_args);
    JS_FreeValue(ctx, ret);
  }

  JS_FreeCString(ctx, data);
  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}