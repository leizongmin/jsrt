#include "fs_common.h"
#ifdef _WIN32
#include <io.h>
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