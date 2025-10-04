#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fs_common.h"

#ifdef _WIN32
#include <fileapi.h>
#include <io.h>
#include <windows.h>
#define mkstemp _mktemp_s
#else
#include <sys/stat.h>
#include <sys/statvfs.h>  // For statfs/statvfs
#include <unistd.h>
#endif

// fs.truncateSync(path[, len])
JSValue js_fs_truncate_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Default length is 0
  off_t length = 0;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    int64_t len_val;
    if (JS_ToInt64(ctx, &len_val, argv[1]) < 0) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
    if (len_val < 0) {
      JS_FreeCString(ctx, path);
      return JS_ThrowRangeError(ctx, "length must be >= 0");
    }
    length = (off_t)len_val;
  }

#ifdef _WIN32
  // Windows: Open file and use SetEndOfFile
  HANDLE handle = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    DWORD error = GetLastError();
    JSValue fs_error;

    switch (error) {
      case ERROR_FILE_NOT_FOUND:
      case ERROR_PATH_NOT_FOUND:
        fs_error = create_fs_error(ctx, ENOENT, "truncate", path);
        break;
      case ERROR_ACCESS_DENIED:
        fs_error = create_fs_error(ctx, EACCES, "truncate", path);
        break;
      default:
        fs_error = create_fs_error(ctx, EIO, "truncate", path);
        break;
    }

    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, fs_error);
  }

  // Set file pointer to desired length
  LARGE_INTEGER file_pos;
  file_pos.QuadPart = length;

  if (!SetFilePointerEx(handle, file_pos, NULL, FILE_BEGIN)) {
    CloseHandle(handle);
    JSValue error = create_fs_error(ctx, EIO, "truncate", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  // Truncate at current position
  if (!SetEndOfFile(handle)) {
    CloseHandle(handle);
    JSValue error = create_fs_error(ctx, EIO, "truncate", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  CloseHandle(handle);
#else
  // Unix/Linux: Use truncate()
  if (truncate(path, length) < 0) {
    JSValue error = create_fs_error(ctx, errno, "truncate", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }
#endif

  JS_FreeCString(ctx, path);
  return JS_UNDEFINED;
}

// fs.ftruncateSync(fd[, len])
JSValue js_fs_ftruncate_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "fd is required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  // Default length is 0
  off_t length = 0;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    int64_t len_val;
    if (JS_ToInt64(ctx, &len_val, argv[1]) < 0) {
      return JS_EXCEPTION;
    }
    if (len_val < 0) {
      return JS_ThrowRangeError(ctx, "length must be >= 0");
    }
    length = (off_t)len_val;
  }

#ifdef _WIN32
  // Windows: Use _chsize
  if (_chsize(fd, length) < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "ftruncate", NULL));
  }
#else
  // Unix/Linux: Use ftruncate()
  if (ftruncate(fd, length) < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "ftruncate", NULL));
  }
#endif

  return JS_UNDEFINED;
}

// fs.mkdtempSync(prefix[, options])
JSValue js_fs_mkdtemp_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "prefix is required");
  }

  const char* prefix = JS_ToCString(ctx, argv[0]);
  if (!prefix) {
    return JS_EXCEPTION;
  }

  // Parse options (encoding)
  const char* encoding = "utf8";  // default
  if (argc >= 2 && JS_IsObject(argv[1]) && !JS_IsNull(argv[1])) {
    JSValue enc_val = JS_GetPropertyStr(ctx, argv[1], "encoding");
    if (!JS_IsUndefined(enc_val) && !JS_IsNull(enc_val)) {
      const char* enc_str = JS_ToCString(ctx, enc_val);
      if (enc_str) {
        encoding = enc_str;
      }
    }
    JS_FreeValue(ctx, enc_val);
  } else if (argc >= 2 && JS_IsString(argv[1])) {
    const char* enc_str = JS_ToCString(ctx, argv[1]);
    if (enc_str) {
      encoding = enc_str;
    }
  }

  // Create template with XXXXXX suffix
  size_t prefix_len = strlen(prefix);
  char* template_path = malloc(prefix_len + 7);  // +6 for "XXXXXX" +1 for null terminator
  if (!template_path) {
    JS_FreeCString(ctx, prefix);
    return JS_ThrowOutOfMemory(ctx);
  }

  strcpy(template_path, prefix);
  strcat(template_path, "XXXXXX");

#ifdef _WIN32
  // Windows: Use _mktemp_s and CreateDirectory
  errno_t err = _mktemp_s(template_path, prefix_len + 7);
  if (err != 0) {
    free(template_path);
    JS_FreeCString(ctx, prefix);
    return JS_Throw(ctx, create_fs_error(ctx, EEXIST, "mkdtemp", prefix));
  }

  if (!CreateDirectoryA(template_path, NULL)) {
    DWORD error = GetLastError();
    JSValue fs_error;

    switch (error) {
      case ERROR_ALREADY_EXISTS:
        fs_error = create_fs_error(ctx, EEXIST, "mkdtemp", template_path);
        break;
      case ERROR_PATH_NOT_FOUND:
        fs_error = create_fs_error(ctx, ENOENT, "mkdtemp", template_path);
        break;
      case ERROR_ACCESS_DENIED:
        fs_error = create_fs_error(ctx, EACCES, "mkdtemp", template_path);
        break;
      default:
        fs_error = create_fs_error(ctx, EIO, "mkdtemp", template_path);
        break;
    }

    free(template_path);
    JS_FreeCString(ctx, prefix);
    return JS_Throw(ctx, fs_error);
  }
#else
  // Unix/Linux: Use mkdtemp()
  char* result = mkdtemp(template_path);
  if (!result) {
    JSValue error = create_fs_error(ctx, errno, "mkdtemp", prefix);
    free(template_path);
    JS_FreeCString(ctx, prefix);
    return JS_Throw(ctx, error);
  }
#endif

  JS_FreeCString(ctx, prefix);

  JSValue result_val;
  // Handle encoding
  if (strcmp(encoding, "buffer") == 0) {
    // Return as Buffer
    result_val = create_buffer_from_data(ctx, (const uint8_t*)template_path, strlen(template_path));
  } else {
    // Return as string (utf8 default)
    result_val = JS_NewString(ctx, template_path);
  }

  free(template_path);
  return result_val;
}

// fs.fsyncSync(fd)
JSValue js_fs_fsync_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "fd is required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // Windows: Use FlushFileBuffers
  HANDLE handle = (HANDLE)_get_osfhandle(fd);
  if (handle == INVALID_HANDLE_VALUE) {
    return JS_Throw(ctx, create_fs_error(ctx, EBADF, "fsync", NULL));
  }

  if (!FlushFileBuffers(handle)) {
    DWORD error = GetLastError();
    JSValue fs_error;

    switch (error) {
      case ERROR_INVALID_HANDLE:
        fs_error = create_fs_error(ctx, EBADF, "fsync", NULL);
        break;
      case ERROR_ACCESS_DENIED:
        fs_error = create_fs_error(ctx, EACCES, "fsync", NULL);
        break;
      default:
        fs_error = create_fs_error(ctx, EIO, "fsync", NULL);
        break;
    }

    return JS_Throw(ctx, fs_error);
  }
#else
  // Unix/Linux: Use fsync()
  if (fsync(fd) < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "fsync", NULL));
  }
#endif

  return JS_UNDEFINED;
}

// fs.fdatasyncSync(fd)
JSValue js_fs_fdatasync_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "fd is required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // Windows: Same as fsync (no separate fdatasync)
  return js_fs_fsync_sync(ctx, this_val, argc, argv);
#else
// Unix/Linux: Use fdatasync() if available, fallback to fsync()
#ifdef _POSIX_SYNCHRONIZED_IO
  if (fdatasync(fd) < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "fdatasync", NULL));
  }
#else
  if (fsync(fd) < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "fdatasync", NULL));
  }
#endif
#endif

  return JS_UNDEFINED;
}

// fs.statfsSync(path)
JSValue js_fs_statfs_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path is required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // Windows: Use GetDiskFreeSpaceEx
  ULARGE_INTEGER available_bytes, total_bytes, free_bytes;

  if (!GetDiskFreeSpaceExA(path, &available_bytes, &total_bytes, &free_bytes)) {
    DWORD error = GetLastError();
    JSValue fs_error;

    switch (error) {
      case ERROR_PATH_NOT_FOUND:
      case ERROR_FILE_NOT_FOUND:
        fs_error = create_fs_error(ctx, ENOENT, "statfs", path);
        break;
      case ERROR_ACCESS_DENIED:
        fs_error = create_fs_error(ctx, EACCES, "statfs", path);
        break;
      default:
        fs_error = create_fs_error(ctx, EIO, "statfs", path);
        break;
    }

    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, fs_error);
  }

  JSValue result = JS_NewObject(ctx);

  // Note: Windows doesn't provide all the info that Unix statfs does
  // We'll provide what we can
  JS_SetPropertyStr(ctx, result, "type", JS_NewInt64(ctx, 0));      // Not available on Windows
  JS_SetPropertyStr(ctx, result, "bsize", JS_NewInt64(ctx, 4096));  // Assume 4KB block size
  JS_SetPropertyStr(ctx, result, "blocks", JS_NewInt64(ctx, total_bytes.QuadPart / 4096));
  JS_SetPropertyStr(ctx, result, "bfree", JS_NewInt64(ctx, free_bytes.QuadPart / 4096));
  JS_SetPropertyStr(ctx, result, "bavail", JS_NewInt64(ctx, available_bytes.QuadPart / 4096));
  JS_SetPropertyStr(ctx, result, "files", JS_NewInt64(ctx, 0));  // Not available on Windows
  JS_SetPropertyStr(ctx, result, "ffree", JS_NewInt64(ctx, 0));  // Not available on Windows

  JS_FreeCString(ctx, path);
  return result;

#else
  // Unix/Linux: Use statvfs
  struct statvfs buf;

  if (statvfs(path, &buf) < 0) {
    JSValue error = create_fs_error(ctx, errno, "statfs", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JSValue result = JS_NewObject(ctx);

  // File system type
  JS_SetPropertyStr(ctx, result, "type", JS_NewInt64(ctx, buf.f_fsid));
  // Block size
  JS_SetPropertyStr(ctx, result, "bsize", JS_NewInt64(ctx, buf.f_bsize));
  // Total blocks
  JS_SetPropertyStr(ctx, result, "blocks", JS_NewInt64(ctx, buf.f_blocks));
  // Free blocks
  JS_SetPropertyStr(ctx, result, "bfree", JS_NewInt64(ctx, buf.f_bfree));
  // Available blocks (for unprivileged users)
  JS_SetPropertyStr(ctx, result, "bavail", JS_NewInt64(ctx, buf.f_bavail));
  // Total inodes
  JS_SetPropertyStr(ctx, result, "files", JS_NewInt64(ctx, buf.f_files));
  // Free inodes
  JS_SetPropertyStr(ctx, result, "ffree", JS_NewInt64(ctx, buf.f_ffree));

  JS_FreeCString(ctx, path);
  return result;
#endif
}