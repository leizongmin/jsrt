#include "fs_async_libuv.h"
#include "../../runtime.h"

// Get uv_loop from JSContext
uv_loop_t* fs_get_uv_loop(JSContext* ctx) {
  JSRuntime* rt = JS_GetRuntime(ctx);
  JSRT_Runtime* jsrt_rt = (JSRT_Runtime*)JS_GetRuntimeOpaque(rt);
  return jsrt_rt->uv_loop;
}

// Free async work request and all owned resources
void fs_async_work_free(fs_async_work_t* work) {
  if (!work)
    return;

  // Free JS callback
  if (!JS_IsUndefined(work->callback)) {
    JS_FreeValue(work->ctx, work->callback);
  }

  // Free user buffer reference (for read/write operations)
  if (!JS_IsUndefined(work->user_buffer)) {
    JS_FreeValue(work->ctx, work->user_buffer);
  }

  // Free paths
  if (work->path) {
    free(work->path);
  }
  if (work->path2) {
    free(work->path2);
  }

  // Free buffer only if owned (not pointing to user's buffer)
  if (work->buffer && work->owns_buffer) {
    free(work->buffer);
  }

  // Cleanup libuv request
  uv_fs_req_cleanup(&work->req);

  // Free the work structure itself
  free(work);
}

// Generic completion - handles errors and calls JS callback
void fs_async_generic_complete(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  JSValue args[2];

  if (req->result < 0) {
    // Error occurred
    int err = -req->result;  // libuv returns negative errno
    args[0] = create_fs_error(ctx, err, uv_fs_get_type(req) == UV_FS_UNKNOWN ? "unknown" : "operation", work->path);
    args[1] = JS_UNDEFINED;
  } else {
    // Success - subclass will set args[1]
    args[0] = JS_NULL;
    args[1] = JS_UNDEFINED;  // Override in specific handlers
  }

  // Call JS callback
  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);

  // Cleanup
  JS_FreeValue(ctx, args[0]);
  if (!JS_IsUndefined(args[1])) {
    JS_FreeValue(ctx, args[1]);
  }

  fs_async_work_free(work);
}

// Completion for void operations (unlink, mkdir, rmdir, etc)
void fs_async_complete_void(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  JSValue args[2];

  if (req->result < 0) {
    int err = -req->result;
    const char* syscall = "operation";

    // Map fs_type to syscall name
    switch (uv_fs_get_type(req)) {
      case UV_FS_UNLINK:
        syscall = "unlink";
        break;
      case UV_FS_MKDIR:
        syscall = "mkdir";
        break;
      case UV_FS_RMDIR:
        syscall = "rmdir";
        break;
      case UV_FS_RENAME:
        syscall = "rename";
        break;
      case UV_FS_CHMOD:
        syscall = "chmod";
        break;
      case UV_FS_CHOWN:
        syscall = "chown";
        break;
      case UV_FS_UTIME:
        syscall = "utime";
        break;
      case UV_FS_LINK:
        syscall = "link";
        break;
      case UV_FS_SYMLINK:
        syscall = "symlink";
        break;
      case UV_FS_ACCESS:
        syscall = "access";
        break;
      default:
        break;
    }

    args[0] = create_fs_error(ctx, err, syscall, work->path);
    args[1] = JS_UNDEFINED;
  } else {
    args[0] = JS_NULL;
    args[1] = JS_UNDEFINED;
  }

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);

  JS_FreeValue(ctx, args[0]);
  fs_async_work_free(work);
}

// Completion for fd operations (open)
void fs_async_complete_fd(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  JSValue args[2];

  if (req->result < 0) {
    int err = -req->result;
    args[0] = create_fs_error(ctx, err, "open", work->path);
    args[1] = JS_UNDEFINED;
  } else {
    args[0] = JS_NULL;
    args[1] = JS_NewInt32(ctx, (int)req->result);  // fd
  }

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);

  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  fs_async_work_free(work);
}

// Completion for data operations (read, readFile)
void fs_async_complete_data(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  JSValue args[2];

  if (req->result < 0) {
    int err = -req->result;
    args[0] = create_fs_error(ctx, err, "read", work->path);
    args[1] = JS_UNDEFINED;
  } else {
    args[0] = JS_NULL;
    // Create buffer from data
    if (work->buffer && req->result > 0) {
      args[1] = create_buffer_from_data(ctx, (uint8_t*)work->buffer, (size_t)req->result);
    } else {
      args[1] = create_buffer_from_data(ctx, NULL, 0);
    }
  }

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);

  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  fs_async_work_free(work);
}

// Completion for stat operations (stat, lstat, fstat)
void fs_async_complete_stat(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  JSValue args[2];

  if (req->result < 0) {
    int err = -req->result;
    const char* syscall = uv_fs_get_type(req) == UV_FS_LSTAT   ? "lstat"
                          : uv_fs_get_type(req) == UV_FS_FSTAT ? "fstat"
                                                               : "stat";
    args[0] = create_fs_error(ctx, err, syscall, work->path);
    args[1] = JS_UNDEFINED;
  } else {
    args[0] = JS_NULL;

    // Create Stats object
    uv_stat_t* statbuf = uv_fs_get_statbuf(req);
    JSValue stats_obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, stats_obj, "dev", JS_NewInt64(ctx, statbuf->st_dev));
    JS_SetPropertyStr(ctx, stats_obj, "mode", JS_NewInt32(ctx, statbuf->st_mode));
    JS_SetPropertyStr(ctx, stats_obj, "nlink", JS_NewInt64(ctx, statbuf->st_nlink));
    JS_SetPropertyStr(ctx, stats_obj, "uid", JS_NewInt64(ctx, statbuf->st_uid));
    JS_SetPropertyStr(ctx, stats_obj, "gid", JS_NewInt64(ctx, statbuf->st_gid));
    JS_SetPropertyStr(ctx, stats_obj, "rdev", JS_NewInt64(ctx, statbuf->st_rdev));
    JS_SetPropertyStr(ctx, stats_obj, "ino", JS_NewInt64(ctx, statbuf->st_ino));
    JS_SetPropertyStr(ctx, stats_obj, "size", JS_NewInt64(ctx, statbuf->st_size));
    JS_SetPropertyStr(ctx, stats_obj, "blksize", JS_NewInt64(ctx, statbuf->st_blksize));
    JS_SetPropertyStr(ctx, stats_obj, "blocks", JS_NewInt64(ctx, statbuf->st_blocks));

    // Store mode for helper methods to access (with underscore prefix)
    JS_SetPropertyStr(ctx, stats_obj, "_mode", JS_NewInt32(ctx, statbuf->st_mode));

    // Add helper methods
    JSValue is_file_func = JS_NewCFunction(ctx, js_fs_stat_is_file, "isFile", 0);
    JSValue is_dir_func = JS_NewCFunction(ctx, js_fs_stat_is_directory, "isDirectory", 0);
    JS_SetPropertyStr(ctx, stats_obj, "isFile", is_file_func);
    JS_SetPropertyStr(ctx, stats_obj, "isDirectory", is_dir_func);

    args[1] = stats_obj;
  }

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);

  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  fs_async_work_free(work);
}

// Completion for string operations (readlink, realpath)
void fs_async_complete_string(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  JSValue args[2];

  if (req->result < 0) {
    int err = -req->result;
    const char* syscall = uv_fs_get_type(req) == UV_FS_READLINK ? "readlink" : "realpath";
    args[0] = create_fs_error(ctx, err, syscall, work->path);
    args[1] = JS_UNDEFINED;
  } else {
    args[0] = JS_NULL;
    const char* result_str = (const char*)uv_fs_get_ptr(req);
    args[1] = JS_NewString(ctx, result_str ? result_str : "");
  }

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);

  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  fs_async_work_free(work);
}

// Completion for readdir
void fs_async_complete_readdir(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  JSValue args[2];

  if (req->result < 0) {
    int err = -req->result;
    args[0] = create_fs_error(ctx, err, "readdir", work->path);
    args[1] = JS_UNDEFINED;
  } else {
    args[0] = JS_NULL;

    // Create array of filenames
    JSValue files_array = JS_NewArray(ctx);
    int index = 0;
    uv_dirent_t dent;

    // Iterate through directory entries
    while (uv_fs_scandir_next(req, &dent) != UV_EOF) {
      JS_SetPropertyUint32(ctx, files_array, index++, JS_NewString(ctx, dent.name));
    }

    args[1] = files_array;
  }

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);

  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  fs_async_work_free(work);
}
