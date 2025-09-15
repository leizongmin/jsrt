#include "fs_common.h"

// fs.readFileSync(path[, options])
JSValue js_fs_read_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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
JSValue js_fs_write_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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
JSValue js_fs_exists_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

// fs.unlinkSync(path)
JSValue js_fs_unlink_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

// fs.appendFileSync(path, data[, options])
JSValue js_fs_append_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "path and data are required");
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

  // Open file in append mode
  FILE* file = fopen(path, "ab");
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

// fs.copyFileSync(src, dest[, mode])
JSValue js_fs_copy_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "src and dest are required");
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

  // Open source file for reading
  FILE* src_file = fopen(src, "rb");
  if (!src_file) {
    JSValue error = create_fs_error(ctx, errno, "open", src);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  // Open destination file for writing
  FILE* dest_file = fopen(dest, "wb");
  if (!dest_file) {
    fclose(src_file);
    JSValue error = create_fs_error(ctx, errno, "open", dest);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  // Copy file content in chunks
  char buffer[8192];
  size_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
    size_t bytes_written = fwrite(buffer, 1, bytes_read, dest_file);
    if (bytes_written != bytes_read) {
      fclose(src_file);
      fclose(dest_file);
      JSValue error = create_fs_error(ctx, errno, "write", dest);
      JS_FreeCString(ctx, src);
      JS_FreeCString(ctx, dest);
      return JS_Throw(ctx, error);
    }
  }

  // Check for read errors
  if (ferror(src_file)) {
    fclose(src_file);
    fclose(dest_file);
    JSValue error = create_fs_error(ctx, errno, "read", src);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_Throw(ctx, error);
  }

  fclose(src_file);
  fclose(dest_file);

  JS_FreeCString(ctx, src);
  JS_FreeCString(ctx, dest);

  return JS_UNDEFINED;
}

// fs.renameSync(oldPath, newPath)
JSValue js_fs_rename_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "oldPath and newPath are required");
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

  if (rename(old_path, new_path) != 0) {
    JSValue error = create_fs_error(ctx, errno, "rename", old_path);
    JS_FreeCString(ctx, old_path);
    JS_FreeCString(ctx, new_path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, old_path);
  JS_FreeCString(ctx, new_path);
  return JS_UNDEFINED;
}

// fs.accessSync(path[, mode])
JSValue js_fs_access_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int mode = F_OK;  // Default to checking existence
  if (argc > 1 && JS_IsNumber(argv[1])) {
    int32_t mode_int;
    JS_ToInt32(ctx, &mode_int, argv[1]);
    mode = mode_int;
  }

  if (access(path, mode) != 0) {
    JSValue error = create_fs_error(ctx, errno, "access", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.openSync(path, flags[, mode])
JSValue js_fs_open_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "path and flags are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  const char* flags_str = JS_ToCString(ctx, argv[1]);
  if (!flags_str) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Convert flags string to system flags
  int flags = 0;
  if (strcmp(flags_str, "r") == 0) {
    flags = O_RDONLY;
  } else if (strcmp(flags_str, "r+") == 0) {
    flags = O_RDWR;
  } else if (strcmp(flags_str, "w") == 0) {
    flags = O_WRONLY | O_CREAT | O_TRUNC;
  } else if (strcmp(flags_str, "w+") == 0) {
    flags = O_RDWR | O_CREAT | O_TRUNC;
  } else if (strcmp(flags_str, "a") == 0) {
    flags = O_WRONLY | O_CREAT | O_APPEND;
  } else if (strcmp(flags_str, "a+") == 0) {
    flags = O_RDWR | O_CREAT | O_APPEND;
  } else {
    JS_FreeCString(ctx, path);
    JS_FreeCString(ctx, flags_str);
    return JS_ThrowTypeError(ctx, "Invalid flags");
  }

  mode_t mode = 0644;  // Default mode
  if (argc > 2 && JS_IsNumber(argv[2])) {
    int32_t mode_int;
    JS_ToInt32(ctx, &mode_int, argv[2]);
    mode = mode_int;
  }

  int fd = open(path, flags, mode);
  if (fd < 0) {
    JSValue error = create_fs_error(ctx, errno, "open", path);
    JS_FreeCString(ctx, path);
    JS_FreeCString(ctx, flags_str);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  JS_FreeCString(ctx, flags_str);
  return JS_NewInt32(ctx, fd);
}

// fs.closeSync(fd)
JSValue js_fs_close_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "fd is required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  if (close(fd) < 0) {
    JSValue error = create_fs_error(ctx, errno, "close", NULL);
    return JS_Throw(ctx, error);
  }

  return JS_UNDEFINED;
}

// fs.readSync(fd, buffer, offset, length, position)
JSValue js_fs_read_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 4) {
    return JS_ThrowTypeError(ctx, "fd, buffer, offset, and length are required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  // For now, we'll work with a simple buffer approach
  // In a full implementation, we'd integrate with the Buffer class
  int32_t offset, length;
  if (JS_ToInt32(ctx, &offset, argv[2]) < 0 || JS_ToInt32(ctx, &length, argv[3]) < 0) {
    return JS_EXCEPTION;
  }

  if (offset < 0 || length < 0) {
    return JS_ThrowRangeError(ctx, "offset and length must be non-negative");
  }

  // Handle position parameter (optional)
  off_t position = -1;
  if (argc > 4 && !JS_IsNull(argv[4])) {
    int32_t pos;
    if (JS_ToInt32(ctx, &pos, argv[4]) < 0) {
      return JS_EXCEPTION;
    }
    position = pos;
  }

  // Allocate buffer for reading
  char* buffer = malloc(length);
  if (!buffer) {
    return JS_ThrowOutOfMemory(ctx);
  }

  ssize_t bytes_read;
  if (position >= 0) {
    // Read from specific position
    bytes_read = pread(fd, buffer, length, position);
  } else {
    // Read from current position
    bytes_read = read(fd, buffer, length);
  }

  if (bytes_read < 0) {
    free(buffer);
    JSValue error = create_fs_error(ctx, errno, "read", NULL);
    return JS_Throw(ctx, error);
  }

  // Create buffer result - for now return as string, should be Buffer
  JSValue result = create_buffer_from_data(ctx, (uint8_t*)buffer, bytes_read);
  free(buffer);
  
  return JS_NewInt32(ctx, bytes_read);
}

// fs.writeSync(fd, buffer[, offset[, length[, position]]])
JSValue js_fs_write_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "fd and data are required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
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
    // TODO: Handle Buffer objects properly
    return JS_ThrowTypeError(ctx, "data must be string or Buffer");
  }

  if (!data) {
    return JS_EXCEPTION;
  }

  // Handle optional parameters
  int32_t offset = 0, length = data_len;
  if (argc > 2 && !JS_IsUndefined(argv[2])) {
    if (JS_ToInt32(ctx, &offset, argv[2]) < 0) {
      JS_FreeCString(ctx, data);
      return JS_EXCEPTION;
    }
  }
  
  if (argc > 3 && !JS_IsUndefined(argv[3])) {
    if (JS_ToInt32(ctx, &length, argv[3]) < 0) {
      JS_FreeCString(ctx, data);
      return JS_EXCEPTION;
    }
  }

  // Validate parameters
  if (offset < 0 || length < 0 || offset + length > (int32_t)data_len) {
    JS_FreeCString(ctx, data);
    return JS_ThrowRangeError(ctx, "Invalid offset or length");
  }

  // Handle position parameter (optional)
  off_t position = -1;
  if (argc > 4 && !JS_IsNull(argv[4])) {
    int32_t pos;
    if (JS_ToInt32(ctx, &pos, argv[4]) < 0) {
      JS_FreeCString(ctx, data);
      return JS_EXCEPTION;
    }
    position = pos;
  }

  ssize_t bytes_written;
  if (position >= 0) {
    // Write to specific position
    bytes_written = pwrite(fd, data + offset, length, position);
  } else {
    // Write to current position
    bytes_written = write(fd, data + offset, length);
  }

  JS_FreeCString(ctx, data);

  if (bytes_written < 0) {
    JSValue error = create_fs_error(ctx, errno, "write", NULL);
    return JS_Throw(ctx, error);
  }

  return JS_NewInt32(ctx, bytes_written);
}

// fs.chmodSync(path, mode)
JSValue js_fs_chmod_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "path and mode are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  if (chmod(path, mode) < 0) {
    JSValue error = create_fs_error(ctx, errno, "chmod", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.chownSync(path, uid, gid)
JSValue js_fs_chown_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "path, uid, and gid are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t uid, gid;
  if (JS_ToInt32(ctx, &uid, argv[1]) < 0 || JS_ToInt32(ctx, &gid, argv[2]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // On Windows, chown is typically not supported
  JS_FreeCString(ctx, path);
  return JS_ThrowError(ctx, "chown is not supported on Windows");
#else
  if (chown(path, uid, gid) < 0) {
    JSValue error = create_fs_error(ctx, errno, "chown", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
#endif
}

// fs.utimesSync(path, atime, mtime)
JSValue js_fs_utimes_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "path, atime, and mtime are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  double atime, mtime;
  if (JS_ToFloat64(ctx, &atime, argv[1]) < 0 || JS_ToFloat64(ctx, &mtime, argv[2]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  struct utimbuf times;
  times.actime = (time_t)atime; 
  times.modtime = (time_t)mtime;

  if (utime(path, &times) < 0) {
    JSValue error = create_fs_error(ctx, errno, "utime", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}