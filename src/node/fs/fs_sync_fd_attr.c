#include "fs_common.h"

// fs.fchmodSync(fd, mode)
JSValue js_fs_fchmod_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "fd and mode are required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  int32_t mode_val;
  if (JS_ToInt32(ctx, &mode_val, argv[1]) < 0) {
    return JS_EXCEPTION;
  }

  mode_t mode = (mode_t)mode_val;

#ifdef _WIN32
  // Windows doesn't have fchmod, use _chmod with file path from fd
  // This is a limitation - we'll return an error for now
  return JS_ThrowError(ctx, "fchmod is not supported on Windows");
#else
  if (fchmod(fd, mode) < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "fchmod", NULL));
  }
#endif

  return JS_UNDEFINED;
}

// fs.fchownSync(fd, uid, gid)
JSValue js_fs_fchown_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "fd, uid, and gid are required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  int32_t uid;
  if (JS_ToInt32(ctx, &uid, argv[1]) < 0) {
    return JS_EXCEPTION;
  }

  int32_t gid;
  if (JS_ToInt32(ctx, &gid, argv[2]) < 0) {
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // Windows doesn't have the concept of file ownership like Unix
  return JS_ThrowError(ctx, "fchown is not supported on Windows");
#else
  if (fchown(fd, (uid_t)uid, (gid_t)gid) < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "fchown", NULL));
  }
#endif

  return JS_UNDEFINED;
}

// fs.lchownSync(path, uid, gid)
JSValue js_fs_lchown_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "path, uid, and gid are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t uid;
  if (JS_ToInt32(ctx, &uid, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  int32_t gid;
  if (JS_ToInt32(ctx, &gid, argv[2]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // Windows doesn't have the concept of file ownership like Unix
  JS_FreeCString(ctx, path);
  return JS_ThrowError(ctx, "lchown is not supported on Windows");
#else
  if (lchown(path, (uid_t)uid, (gid_t)gid) < 0) {
    JSValue error = create_fs_error(ctx, errno, "lchown", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
#endif

  return JS_UNDEFINED;
}

// fs.futimesSync(fd, atime, mtime)
JSValue js_fs_futimes_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "fd, atime, and mtime are required");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  double atime_ms, mtime_ms;
  if (JS_ToFloat64(ctx, &atime_ms, argv[1]) < 0) {
    return JS_EXCEPTION;
  }
  if (JS_ToFloat64(ctx, &mtime_ms, argv[2]) < 0) {
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // Windows has _futime but with different structure
  struct _utimbuf times;
  times.actime = (time_t)(atime_ms / 1000.0);
  times.modtime = (time_t)(mtime_ms / 1000.0);

  if (_futime(fd, &times) < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "futime", NULL));
  }
#else
  // Use futimens for better precision (nanoseconds)
  struct timespec times[2];
  times[0].tv_sec = (time_t)(atime_ms / 1000.0);
  times[0].tv_nsec = (long)((atime_ms - times[0].tv_sec * 1000.0) * 1000000.0);
  times[1].tv_sec = (time_t)(mtime_ms / 1000.0);
  times[1].tv_nsec = (long)((mtime_ms - times[1].tv_sec * 1000.0) * 1000000.0);

  if (futimens(fd, times) < 0) {
    return JS_Throw(ctx, create_fs_error(ctx, errno, "futimes", NULL));
  }
#endif

  return JS_UNDEFINED;
}

// fs.lutimesSync(path, atime, mtime)
JSValue js_fs_lutimes_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "path, atime, and mtime are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  double atime_ms, mtime_ms;
  if (JS_ToFloat64(ctx, &atime_ms, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }
  if (JS_ToFloat64(ctx, &mtime_ms, argv[2]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

#ifdef _WIN32
  // Windows doesn't have lutimes
  JS_FreeCString(ctx, path);
  return JS_ThrowError(ctx, "lutimes is not supported on Windows");
#else
  // Use utimensat with AT_SYMLINK_NOFOLLOW flag
  struct timespec times[2];
  times[0].tv_sec = (time_t)(atime_ms / 1000.0);
  times[0].tv_nsec = (long)((atime_ms - times[0].tv_sec * 1000.0) * 1000000.0);
  times[1].tv_sec = (time_t)(mtime_ms / 1000.0);
  times[1].tv_nsec = (long)((mtime_ms - times[1].tv_sec * 1000.0) * 1000000.0);

  if (utimensat(AT_FDCWD, path, times, AT_SYMLINK_NOFOLLOW) < 0) {
    JSValue error = create_fs_error(ctx, errno, "lutimes", path);
    JS_FreeCString(ctx, path);
    return JS_Throw(ctx, error);
  }

  JS_FreeCString(ctx, path);
#endif

  return JS_UNDEFINED;
}

// fs.lchmodSync(path, mode) - NOT IMPLEMENTED
// lchmod is not available on Linux and many other systems
// This function exists for API compatibility but will throw an error
JSValue js_fs_lchmod_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "path and mode are required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t mode_val;
  if (JS_ToInt32(ctx, &mode_val, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // lchmod is not available on most systems (including Linux)
  // It's only available on some BSDs and macOS
  JSValue error = JS_NewError(ctx);
  JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "lchmod is not implemented on this platform"));
  JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ERR_METHOD_NOT_IMPLEMENTED"));
  JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, "lchmod"));
  JS_SetPropertyStr(ctx, error, "path", JS_NewString(ctx, path));

  JS_FreeCString(ctx, path);
  return JS_Throw(ctx, error);
}
