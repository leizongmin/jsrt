#include "fs_common.h"
#ifdef _WIN32
#include <io.h>
#else
#include <sys/uio.h>  // For readv/writev and struct iovec
#endif

// Helper function to parse file flags
static int parse_file_flags(const char* flags) {
  if (strcmp(flags, "r") == 0)
    return O_RDONLY;
  if (strcmp(flags, "r+") == 0)
    return O_RDWR;
  if (strcmp(flags, "w") == 0)
    return O_WRONLY | O_CREAT | O_TRUNC;
  if (strcmp(flags, "w+") == 0)
    return O_RDWR | O_CREAT | O_TRUNC;
  if (strcmp(flags, "a") == 0)
    return O_WRONLY | O_CREAT | O_APPEND;
  if (strcmp(flags, "a+") == 0)
    return O_RDWR | O_CREAT | O_APPEND;
  return -1;  // Invalid flags
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

  int flags = parse_file_flags(flags_str);
  if (flags == -1) {
    JS_FreeCString(ctx, path);
    JS_FreeCString(ctx, flags_str);
    return JS_ThrowTypeError(ctx, "Invalid flags");
  }

  mode_t mode = 0666;  // Default mode
  if (argc >= 3) {
    int32_t mode_val;
    if (JS_ToInt32(ctx, &mode_val, argv[2]) < 0) {
      JS_FreeCString(ctx, path);
      JS_FreeCString(ctx, flags_str);
      return JS_EXCEPTION;
    }
    mode = (mode_t)mode_val;
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
    return JS_Throw(ctx, create_fs_error(ctx, errno, "close", NULL));
  }

  return JS_UNDEFINED;
}

// fs.readSync(fd, buffer, offset, length, position)
JSValue js_fs_read_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 5) {
    return JS_ThrowTypeError(ctx, "fd, buffer, offset, length, and position are required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  // Get buffer
  size_t buffer_size;
  uint8_t* buffer = JS_GetArrayBuffer(ctx, &buffer_size, argv[1]);
  if (!buffer) {
    return JS_ThrowTypeError(ctx, "buffer must be a Buffer or ArrayBuffer");
  }

  int32_t offset, length;
  if (JS_ToInt32(ctx, &offset, argv[2]) < 0 || JS_ToInt32(ctx, &length, argv[3]) < 0) {
    return JS_EXCEPTION;
  }

  // Check bounds
  if (offset < 0 || length < 0 || (size_t)(offset + length) > buffer_size) {
    return JS_ThrowRangeError(ctx, "offset + length exceeds buffer size");
  }

  // Handle position parameter
  off_t position = -1;
  if (!JS_IsNull(argv[4])) {
    int64_t pos_val;
    if (JS_ToInt64(ctx, &pos_val, argv[4]) < 0) {
      return JS_EXCEPTION;
    }
    position = (off_t)pos_val;
  }

  ssize_t bytes_read;
  if (position >= 0) {
#ifdef _WIN32
    // Windows: Use _lseeki64 and _read
    off_t original_pos = _lseeki64(fd, 0, SEEK_CUR);
    if (original_pos == -1) {
      return JS_Throw(ctx, create_fs_error(ctx, errno, "lseek", NULL));
    }
    if (_lseeki64(fd, position, SEEK_SET) == -1) {
      return JS_Throw(ctx, create_fs_error(ctx, errno, "lseek", NULL));
    }
    bytes_read = _read(fd, buffer + offset, length);
    _lseeki64(fd, original_pos, SEEK_SET);  // Restore position
#else
    bytes_read = pread(fd, buffer + offset, length, position);
#endif
  } else {
#ifdef _WIN32
    bytes_read = _read(fd, buffer + offset, length);
#else
    bytes_read = read(fd, buffer + offset, length);
#endif
  }

  if (bytes_read < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "read", NULL));
  }

  return JS_NewInt32(ctx, (int32_t)bytes_read);
}

// fs.writeSync(fd, data[, offset[, length[, position]]])
JSValue js_fs_write_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "fd and data are required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  // Get data to write
  const char* data;
  size_t data_size;
  int32_t offset = 0, length = -1;
  off_t position = -1;

  if (JS_IsString(argv[1])) {
    data = JS_ToCStringLen(ctx, &data_size, argv[1]);
    if (!data) {
      return JS_EXCEPTION;
    }
    length = (int32_t)data_size;
  } else {
    // Assume it's a Buffer or ArrayBuffer
    data = (const char*)JS_GetArrayBuffer(ctx, &data_size, argv[1]);
    if (!data) {
      return JS_ThrowTypeError(ctx, "data must be a string or Buffer");
    }
    length = (int32_t)data_size;
  }

  // Parse optional parameters
  if (argc >= 3 && !JS_IsUndefined(argv[2])) {
    if (JS_ToInt32(ctx, &offset, argv[2]) < 0) {
      if (JS_IsString(argv[1]))
        JS_FreeCString(ctx, data);
      return JS_EXCEPTION;
    }
  }

  if (argc >= 4 && !JS_IsUndefined(argv[3])) {
    if (JS_ToInt32(ctx, &length, argv[3]) < 0) {
      if (JS_IsString(argv[1]))
        JS_FreeCString(ctx, data);
      return JS_EXCEPTION;
    }
  }

  if (argc >= 5 && !JS_IsNull(argv[4]) && !JS_IsUndefined(argv[4])) {
    int64_t pos_val;
    if (JS_ToInt64(ctx, &pos_val, argv[4]) < 0) {
      if (JS_IsString(argv[1]))
        JS_FreeCString(ctx, data);
      return JS_EXCEPTION;
    }
    position = (off_t)pos_val;
  }

  // Check bounds
  if (offset < 0 || length < 0 || (size_t)(offset + length) > data_size) {
    if (JS_IsString(argv[1]))
      JS_FreeCString(ctx, data);
    return JS_ThrowRangeError(ctx, "offset + length exceeds data size");
  }

  ssize_t bytes_written;
  if (position >= 0) {
#ifdef _WIN32
    // Windows: Use _lseeki64 and _write
    off_t original_pos = _lseeki64(fd, 0, SEEK_CUR);
    if (original_pos == -1) {
      if (JS_IsString(argv[1]))
        JS_FreeCString(ctx, data);
      return JS_Throw(ctx, create_fs_error(ctx, errno, "lseek", NULL));
    }
    if (_lseeki64(fd, position, SEEK_SET) == -1) {
      if (JS_IsString(argv[1]))
        JS_FreeCString(ctx, data);
      return JS_Throw(ctx, create_fs_error(ctx, errno, "lseek", NULL));
    }
    bytes_written = _write(fd, data + offset, length);
    _lseeki64(fd, original_pos, SEEK_SET);  // Restore position
#else
    bytes_written = pwrite(fd, data + offset, length, position);
#endif
  } else {
#ifdef _WIN32
    bytes_written = _write(fd, data + offset, length);
#else
    bytes_written = write(fd, data + offset, length);
#endif
  }

  if (JS_IsString(argv[1])) {
    JS_FreeCString(ctx, data);
  }

  if (bytes_written < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "write", NULL));
  }

  return JS_NewInt32(ctx, (int32_t)bytes_written);
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

#ifdef _WIN32
  // Windows has limited permission support, simulate with _chmod
  int result = _chmod(path, (mode & 0200) ? _S_IWRITE : _S_IREAD);
#else
  int result = chmod(path, (mode_t)mode);
#endif

  if (result < 0) {
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
  // Windows doesn't support chown, return success for compatibility
  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
#else
  if (chown(path, (uid_t)uid, (gid_t)gid) < 0) {
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

// fs.readvSync(fd, buffers[, position])
JSValue js_fs_readv_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "fd and buffers are required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  // Check if buffers is an array
  if (!JS_IsArray(ctx, argv[1])) {
    return JS_ThrowTypeError(ctx, "buffers must be an array");
  }

  // Get array length
  JSValue length_val = JS_GetPropertyStr(ctx, argv[1], "length");
  int32_t num_buffers;
  if (JS_ToInt32(ctx, &num_buffers, length_val) < 0) {
    JS_FreeValue(ctx, length_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length_val);

  if (num_buffers <= 0) {
    return JS_ThrowTypeError(ctx, "buffers array must not be empty");
  }

#ifdef _WIN32
  // Windows doesn't have readv, emulate it
  off_t position = -1;
  if (argc >= 3 && !JS_IsNull(argv[2]) && !JS_IsUndefined(argv[2])) {
    int64_t pos_val;
    if (JS_ToInt64(ctx, &pos_val, argv[2]) < 0) {
      return JS_EXCEPTION;
    }
    position = (off_t)pos_val;
    if (_lseeki64(fd, position, SEEK_SET) == -1) {
      return JS_Throw(ctx, create_fs_error(ctx, errno, "lseek", NULL));
    }
  }

  ssize_t total_read = 0;
  for (int32_t i = 0; i < num_buffers; i++) {
    JSValue buf = JS_GetPropertyUint32(ctx, argv[1], i);
    if (JS_IsException(buf)) {
      return JS_EXCEPTION;
    }

    size_t buf_size;
    uint8_t* buffer = JS_GetArrayBuffer(ctx, &buf_size, buf);
    JS_FreeValue(ctx, buf);

    if (!buffer) {
      return JS_ThrowTypeError(ctx, "all elements must be ArrayBuffers");
    }

    ssize_t bytes_read = _read(fd, buffer, buf_size);
    if (bytes_read < 0) {
      return JS_Throw(ctx, create_fs_error(ctx, errno, "read", NULL));
    }

    total_read += bytes_read;
    if (bytes_read < (ssize_t)buf_size) {
      break;  // EOF or partial read
    }
  }

  return JS_NewInt32(ctx, (int32_t)total_read);
#else
  // Use readv on Unix systems
  struct iovec* iov = js_malloc(ctx, sizeof(struct iovec) * num_buffers);
  if (!iov) {
    return JS_EXCEPTION;
  }

  // Populate iovec array
  for (int32_t i = 0; i < num_buffers; i++) {
    JSValue buf = JS_GetPropertyUint32(ctx, argv[1], i);
    if (JS_IsException(buf)) {
      js_free(ctx, iov);
      return JS_EXCEPTION;
    }

    size_t buf_size;
    uint8_t* buffer = JS_GetArrayBuffer(ctx, &buf_size, buf);
    JS_FreeValue(ctx, buf);

    if (!buffer) {
      js_free(ctx, iov);
      return JS_ThrowTypeError(ctx, "all elements must be ArrayBuffers");
    }

    iov[i].iov_base = buffer;
    iov[i].iov_len = buf_size;
  }

  // Handle position parameter
  ssize_t bytes_read;
  if (argc >= 3 && !JS_IsNull(argv[2]) && !JS_IsUndefined(argv[2])) {
    int64_t position;
    if (JS_ToInt64(ctx, &position, argv[2]) < 0) {
      js_free(ctx, iov);
      return JS_EXCEPTION;
    }
    bytes_read = preadv(fd, iov, num_buffers, (off_t)position);
  } else {
    bytes_read = readv(fd, iov, num_buffers);
  }

  js_free(ctx, iov);

  if (bytes_read < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "read", NULL));
  }

  return JS_NewInt32(ctx, (int32_t)bytes_read);
#endif
}

// fs.writevSync(fd, buffers[, position])
JSValue js_fs_writev_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "fd and buffers are required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  // Check if buffers is an array
  if (!JS_IsArray(ctx, argv[1])) {
    return JS_ThrowTypeError(ctx, "buffers must be an array");
  }

  // Get array length
  JSValue length_val = JS_GetPropertyStr(ctx, argv[1], "length");
  int32_t num_buffers;
  if (JS_ToInt32(ctx, &num_buffers, length_val) < 0) {
    JS_FreeValue(ctx, length_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length_val);

  if (num_buffers <= 0) {
    return JS_ThrowTypeError(ctx, "buffers array must not be empty");
  }

#ifdef _WIN32
  // Windows doesn't have writev, emulate it
  off_t position = -1;
  if (argc >= 3 && !JS_IsNull(argv[2]) && !JS_IsUndefined(argv[2])) {
    int64_t pos_val;
    if (JS_ToInt64(ctx, &pos_val, argv[2]) < 0) {
      return JS_EXCEPTION;
    }
    position = (off_t)pos_val;
    if (_lseeki64(fd, position, SEEK_SET) == -1) {
      return JS_Throw(ctx, create_fs_error(ctx, errno, "lseek", NULL));
    }
  }

  ssize_t total_written = 0;
  for (int32_t i = 0; i < num_buffers; i++) {
    JSValue buf = JS_GetPropertyUint32(ctx, argv[1], i);
    if (JS_IsException(buf)) {
      return JS_EXCEPTION;
    }

    size_t buf_size;
    const uint8_t* buffer = JS_GetArrayBuffer(ctx, &buf_size, buf);
    JS_FreeValue(ctx, buf);

    if (!buffer) {
      return JS_ThrowTypeError(ctx, "all elements must be ArrayBuffers");
    }

    ssize_t bytes_written = _write(fd, buffer, buf_size);
    if (bytes_written < 0) {
      return JS_Throw(ctx, create_fs_error(ctx, errno, "write", NULL));
    }

    total_written += bytes_written;
    if (bytes_written < (ssize_t)buf_size) {
      break;  // Partial write
    }
  }

  return JS_NewInt32(ctx, (int32_t)total_written);
#else
  // Use writev on Unix systems
  struct iovec* iov = js_malloc(ctx, sizeof(struct iovec) * num_buffers);
  if (!iov) {
    return JS_EXCEPTION;
  }

  // Populate iovec array
  for (int32_t i = 0; i < num_buffers; i++) {
    JSValue buf = JS_GetPropertyUint32(ctx, argv[1], i);
    if (JS_IsException(buf)) {
      js_free(ctx, iov);
      return JS_EXCEPTION;
    }

    size_t buf_size;
    const uint8_t* buffer = JS_GetArrayBuffer(ctx, &buf_size, buf);
    JS_FreeValue(ctx, buf);

    if (!buffer) {
      js_free(ctx, iov);
      return JS_ThrowTypeError(ctx, "all elements must be ArrayBuffers");
    }

    iov[i].iov_base = (void*)buffer;
    iov[i].iov_len = buf_size;
  }

  // Handle position parameter
  ssize_t bytes_written;
  if (argc >= 3 && !JS_IsNull(argv[2]) && !JS_IsUndefined(argv[2])) {
    int64_t position;
    if (JS_ToInt64(ctx, &position, argv[2]) < 0) {
      js_free(ctx, iov);
      return JS_EXCEPTION;
    }
    bytes_written = pwritev(fd, iov, num_buffers, (off_t)position);
  } else {
    bytes_written = writev(fd, iov, num_buffers);
  }

  js_free(ctx, iov);

  if (bytes_written < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "write", NULL));
  }

  return JS_NewInt32(ctx, (int32_t)bytes_written);
#endif
}