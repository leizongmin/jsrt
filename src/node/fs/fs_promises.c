#include <fcntl.h>
#include <string.h>
#include "fs_async_libuv.h"

// ============================================================================
// Phase 3: Promise-based fs API
// ============================================================================

// Promise work request structure (extends fs_async_work_t concept)
typedef struct {
  uv_fs_t req;         // libuv fs request (MUST be first for casting)
  uv_timer_t timer;    // libuv timer for async operations
  JSContext* ctx;      // QuickJS context
  JSValue resolve;     // Promise resolve function
  JSValue reject;      // Promise reject function
  char* path;          // Primary path (owned, must be freed)
  char* path2;         // Secondary path for operations like rename/link (owned)
  void* buffer;        // Buffer data (owned if allocated)
  size_t buffer_size;  // Buffer size
  int flags;           // Operation flags
  int mode;            // File mode
  int64_t offset;      // File offset for read/write
  int result;          // Custom operation result for non-fs operations
} fs_promise_work_t;

// Forward declaration for FileHandle class
static JSClassID filehandle_class_id;

// FileHandle structure
typedef struct {
  int fd;          // File descriptor
  char* path;      // Path (for error messages, owned)
  JSContext* ctx;  // QuickJS context
  bool closed;     // Closed flag
} FileHandle;

// ============================================================================
// Promise work cleanup
// ============================================================================

static void fs_promise_work_free(fs_promise_work_t* work) {
  if (!work)
    return;

  uv_fs_req_cleanup(&work->req);

  if (work->ctx) {
    if (!JS_IsUndefined(work->resolve)) {
      JS_FreeValue(work->ctx, work->resolve);
    }
    if (!JS_IsUndefined(work->reject)) {
      JS_FreeValue(work->ctx, work->reject);
    }
  }

  if (work->path)
    free(work->path);
  if (work->path2)
    free(work->path2);
  if (work->buffer)
    free(work->buffer);

  free(work);
}

// ============================================================================
// Generic Promise completion callbacks
// ============================================================================

// Generic void completion (operations like unlink, mkdir, rmdir)
static void fs_promise_complete_void(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "operation", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Resolve with undefined
    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// Recursive mkdir timer callback (runs back in main thread)
static void fs_promise_mkdir_recursive_timer_cb(uv_timer_t* timer) {
  fs_promise_work_t* work = (fs_promise_work_t*)timer->data;
  JSContext* ctx = work->ctx;

  if (work->result != 0) {
    // Reject with error
    JSValue error = create_fs_error(ctx, errno, "mkdir", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Resolve with undefined
    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, ret);
  }

  uv_timer_stop(timer);
  uv_close((uv_handle_t*)timer, NULL);  // The fs_promise_work_t will be cleaned up separately
  fs_promise_work_free(work);           // Clean up the work structure
}

// File descriptor completion (operations like open)
static void fs_promise_complete_fd(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "open", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Resolve with FileHandle
    int fd = (int)req->result;

    // Create FileHandle object
    FileHandle* fh = malloc(sizeof(FileHandle));
    if (!fh) {
      // Out of memory - close fd and reject
      uv_fs_t close_req;
      uv_fs_close(fs_get_uv_loop(ctx), &close_req, fd, NULL);
      uv_fs_req_cleanup(&close_req);

      JSValue error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Out of memory"));
      JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx, error);
      JS_FreeValue(ctx, ret);
      fs_promise_work_free(work);
      return;
    }

    fh->fd = fd;
    fh->path = work->path ? strdup(work->path) : NULL;
    fh->ctx = ctx;
    fh->closed = false;

    JSValue obj = JS_NewObjectClass(ctx, filehandle_class_id);
    if (JS_IsException(obj)) {
      if (fh->path)
        free(fh->path);
      free(fh);
      close(fd);
      JSValue error = JS_GetException(ctx);
      JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx, error);
      JS_FreeValue(ctx, ret);
      fs_promise_work_free(work);
      return;
    }

    JS_SetOpaque(obj, fh);

    // Resolve with FileHandle
    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &obj);
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// String completion (operations like readlink, realpath)
static void fs_promise_complete_string(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "operation", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Resolve with string result
    const char* result_str = (const char*)req->ptr;
    JSValue result = JS_NewString(ctx, result_str ? result_str : "");
    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &result);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// ============================================================================
// FileHandle class implementation
// ============================================================================

// FileHandle finalizer - auto-close if not manually closed
static void filehandle_finalizer(JSRuntime* rt, JSValue obj) {
  FileHandle* fh = JS_GetOpaque(obj, filehandle_class_id);
  if (fh) {
    if (!fh->closed && fh->fd >= 0) {
      // Safety net: close fd if not manually closed
      close(fh->fd);
    }
    if (fh->path)
      free(fh->path);
    free(fh);
  }
}

// FileHandle class definition
static JSClassDef filehandle_class = {
    "FileHandle",
    .finalizer = filehandle_finalizer,
};

// FileHandle.close() completion callback
static void filehandle_close_cb(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "close", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Resolve with undefined
    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// FileHandle.close() - returns Promise<void>
static JSValue filehandle_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    // Already closed - resolve immediately
    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) {
      return JS_EXCEPTION;
    }

    JSValue ret = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);

    return promise;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize work request
  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;

  // Mark as closed immediately to prevent double-close
  fh->closed = true;
  int fd = fh->fd;

  // Queue async close operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_close(loop, &work->req, fd, filehandle_close_cb);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "close", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.read() completion callback
static void filehandle_read_cb(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "read", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Resolve with { bytesRead, buffer }
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "bytesRead", JS_NewInt64(ctx, req->result));

    // Create ArrayBuffer from buffer
    JSValue buffer = JS_NewArrayBufferCopy(ctx, work->buffer, work->buffer_size);
    JS_SetPropertyStr(ctx, result, "buffer", buffer);

    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &result);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// FileHandle.read() - returns Promise<{ bytesRead, buffer }>
static JSValue filehandle_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  // Parse arguments: read(buffer, offset, length, position)
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "read requires a buffer");
  }

  size_t buffer_size;
  uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, argv[0]);
  if (!buffer_data) {
    return JS_ThrowTypeError(ctx, "First argument must be an ArrayBuffer");
  }

  // Parse offset (default: 0)
  int64_t offset = 0;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    JS_ToInt64(ctx, &offset, argv[1]);
  }

  // Parse length (default: buffer.length - offset)
  int64_t length = buffer_size - offset;
  if (argc >= 3 && !JS_IsUndefined(argv[2])) {
    JS_ToInt64(ctx, &length, argv[2]);
  }

  // Parse position (default: null = current position)
  int64_t position = -1;
  if (argc >= 4 && !JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
    JS_ToInt64(ctx, &position, argv[3]);
  }

  if (offset < 0 || length < 0 || offset + length > (int64_t)buffer_size) {
    return JS_ThrowRangeError(ctx, "Invalid buffer offset/length");
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Allocate read buffer
  work->buffer = malloc(length);
  if (!work->buffer) {
    free(work);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;
  work->buffer_size = length;
  work->offset = position;

  // Queue async read operation
  uv_buf_t buf = uv_buf_init(work->buffer, length);
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_read(loop, &work->req, fh->fd, &buf, 1, position, filehandle_read_cb);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "read", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.write() completion callback
static void filehandle_write_cb(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "write", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Resolve with { bytesWritten, buffer }
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "bytesWritten", JS_NewInt64(ctx, req->result));

    // Original buffer reference
    JSValue buffer = JS_NewArrayBufferCopy(ctx, work->buffer, work->buffer_size);
    JS_SetPropertyStr(ctx, result, "buffer", buffer);

    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &result);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// FileHandle.write() - returns Promise<{ bytesWritten, buffer }>
static JSValue filehandle_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  // Parse arguments: write(buffer, offset, length, position)
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write requires a buffer");
  }

  size_t buffer_size;
  uint8_t* buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, argv[0]);
  if (!buffer_data) {
    return JS_ThrowTypeError(ctx, "First argument must be an ArrayBuffer");
  }

  // Parse offset (default: 0)
  int64_t offset = 0;
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    JS_ToInt64(ctx, &offset, argv[1]);
  }

  // Parse length (default: buffer.length - offset)
  int64_t length = buffer_size - offset;
  if (argc >= 3 && !JS_IsUndefined(argv[2])) {
    JS_ToInt64(ctx, &length, argv[2]);
  }

  // Parse position (default: null = current position)
  int64_t position = -1;
  if (argc >= 4 && !JS_IsUndefined(argv[3]) && !JS_IsNull(argv[3])) {
    JS_ToInt64(ctx, &position, argv[3]);
  }

  if (offset < 0 || length < 0 || offset + length > (int64_t)buffer_size) {
    return JS_ThrowRangeError(ctx, "Invalid buffer offset/length");
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Copy write buffer
  work->buffer = malloc(length);
  if (!work->buffer) {
    free(work);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(work->buffer, buffer_data + offset, length);

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;
  work->buffer_size = length;
  work->offset = position;

  // Queue async write operation
  uv_buf_t buf = uv_buf_init(work->buffer, length);
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_write(loop, &work->req, fh->fd, &buf, 1, position, filehandle_write_cb);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "write", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// Create Stats object from uv_stat_t
static JSValue create_stats_object_from_uv(JSContext* ctx, const uv_stat_t* st) {
  JSValue stats_obj = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, stats_obj, "dev", JS_NewInt64(ctx, st->st_dev));
  JS_SetPropertyStr(ctx, stats_obj, "mode", JS_NewInt32(ctx, st->st_mode));
  JS_SetPropertyStr(ctx, stats_obj, "nlink", JS_NewInt64(ctx, st->st_nlink));
  JS_SetPropertyStr(ctx, stats_obj, "uid", JS_NewInt64(ctx, st->st_uid));
  JS_SetPropertyStr(ctx, stats_obj, "gid", JS_NewInt64(ctx, st->st_gid));
  JS_SetPropertyStr(ctx, stats_obj, "rdev", JS_NewInt64(ctx, st->st_rdev));
  JS_SetPropertyStr(ctx, stats_obj, "ino", JS_NewInt64(ctx, st->st_ino));
  JS_SetPropertyStr(ctx, stats_obj, "size", JS_NewInt64(ctx, st->st_size));
  JS_SetPropertyStr(ctx, stats_obj, "blksize", JS_NewInt64(ctx, st->st_blksize));
  JS_SetPropertyStr(ctx, stats_obj, "blocks", JS_NewInt64(ctx, st->st_blocks));

  // Store mode for helper methods
  JS_SetPropertyStr(ctx, stats_obj, "_mode", JS_NewInt32(ctx, st->st_mode));

  // Add helper methods
  JSValue is_file_func = JS_NewCFunction(ctx, js_fs_stat_is_file, "isFile", 0);
  JS_SetPropertyStr(ctx, stats_obj, "isFile", is_file_func);

  JSValue is_directory_func = JS_NewCFunction(ctx, js_fs_stat_is_directory, "isDirectory", 0);
  JS_SetPropertyStr(ctx, stats_obj, "isDirectory", is_directory_func);

  return stats_obj;
}

// FileHandle.stat() completion callback
static void filehandle_stat_cb(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "fstat", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Resolve with Stats object
    uv_stat_t* statbuf = &req->statbuf;
    JSValue stats = create_stats_object_from_uv(ctx, statbuf);
    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &stats);
    JS_FreeValue(ctx, stats);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// FileHandle.stat() - returns Promise<Stats>
static JSValue filehandle_stat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;

  // Queue async fstat operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fstat(loop, &work->req, fh->fd, filehandle_stat_cb);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "fstat", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.chmod() - returns Promise<void>
static JSValue filehandle_chmod(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "chmod requires a mode");
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, argv[0])) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;
  work->mode = mode;

  // Queue async fchmod operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fchmod(loop, &work->req, fh->fd, mode, fs_promise_complete_void);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "fchmod", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.chown() - returns Promise<void>
static JSValue filehandle_chown(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "chown requires uid and gid");
  }

  int32_t uid, gid;
  if (JS_ToInt32(ctx, &uid, argv[0]) || JS_ToInt32(ctx, &gid, argv[1])) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;

  // Queue async fchown operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fchown(loop, &work->req, fh->fd, uid, gid, fs_promise_complete_void);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "fchown", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.utimes() - returns Promise<void>
static JSValue filehandle_utimes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "utimes requires atime and mtime");
  }

  double atime, mtime;
  if (JS_ToFloat64(ctx, &atime, argv[0]) || JS_ToFloat64(ctx, &mtime, argv[1])) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;

  // Queue async futimes operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_futime(loop, &work->req, fh->fd, atime, mtime, fs_promise_complete_void);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "futimes", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.truncate() - returns Promise<void>
static JSValue filehandle_truncate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  int64_t len = 0;
  if (argc >= 1 && !JS_IsUndefined(argv[0])) {
    JS_ToInt64(ctx, &len, argv[0]);
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;

  // Queue async ftruncate operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_ftruncate(loop, &work->req, fh->fd, len, fs_promise_complete_void);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "ftruncate", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.sync() - returns Promise<void>
static JSValue filehandle_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;

  // Queue async fsync operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fsync(loop, &work->req, fh->fd, fs_promise_complete_void);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "fsync", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.datasync() - returns Promise<void>
static JSValue filehandle_datasync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;

  // Queue async fdatasync operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fdatasync(loop, &work->req, fh->fd, fs_promise_complete_void);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "fdatasync", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// ============================================================================
// FileHandle convenience methods (readFile, writeFile, appendFile)
// ============================================================================

// FileHandle.readFile() completion callback
static void filehandle_readfile_read_cb(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Read error, reject (but don't close fd - FileHandle manages that)
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "read", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
    return;
  }

  // Read complete, resolve with buffer (don't close fd)
  JSValue buffer = JS_NewArrayBufferCopy(ctx, work->buffer, work->buffer_size);
  JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &buffer);
  JS_FreeValue(ctx, buffer);
  JS_FreeValue(ctx, ret);

  fs_promise_work_free(work);
}

static void filehandle_readfile_fstat_cb(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Stat error, reject (but don't close fd - FileHandle manages that)
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "fstat", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
    return;
  }

  uv_stat_t* statbuf = (uv_stat_t*)req->ptr;
  size_t file_size = statbuf->st_size;

  // Handle empty file
  if (file_size == 0) {
    work->buffer = NULL;
    work->buffer_size = 0;
    // Resolve with empty buffer
    JSValue buffer = JS_NewArrayBufferCopy(ctx, NULL, 0);
    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &buffer);
    JS_FreeValue(ctx, buffer);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
    return;
  }

  // Allocate buffer
  work->buffer = malloc(file_size);
  if (!work->buffer) {
    JSValue error = create_fs_error(ctx, ENOMEM, "malloc", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
    return;
  }

  work->buffer_size = file_size;
  uv_buf_t iov = uv_buf_init(work->buffer, (unsigned int)file_size);
  uv_fs_req_cleanup(req);
  uv_fs_read(loop, req, work->flags, &iov, 1, 0, filehandle_readfile_read_cb);
}

// FileHandle.readFile() - returns Promise<Buffer>
static JSValue filehandle_readFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;
  work->flags = fh->fd;  // Store fd in flags field

  // Start with fstat to get file size
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fstat(loop, &work->req, fh->fd, filehandle_readfile_fstat_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "fstat", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.writeFile/appendFile() completion callback
static void filehandle_writefile_write_cb(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Write error, reject (but don't close fd - FileHandle manages that)
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "write", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
    return;
  }

  // Write complete, resolve with undefined (don't close fd)
  JSValue undef = JS_UNDEFINED;
  JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &undef);
  JS_FreeValue(ctx, ret);

  fs_promise_work_free(work);
}

// FileHandle.writeFile() - returns Promise<void>
static JSValue filehandle_writeFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "writeFile requires data");
  }

  // Get data to write
  uint8_t* data = NULL;
  size_t data_len = 0;

  if (JS_IsString(argv[0])) {
    const char* str = JS_ToCString(ctx, argv[0]);
    if (str) {
      data_len = strlen(str);
      data = malloc(data_len);
      if (data) {
        memcpy(data, str, data_len);
      }
      JS_FreeCString(ctx, str);
    }
  } else {
    // Try TypedArray/Buffer/ArrayBuffer
    size_t byte_offset, byte_length, bytes_per_element;
    JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[0], &byte_offset, &byte_length, &bytes_per_element);
    if (!JS_IsException(array_buffer)) {
      size_t size;
      uint8_t* buf = JS_GetArrayBuffer(ctx, &size, array_buffer);
      JS_FreeValue(ctx, array_buffer);
      if (buf) {
        data_len = byte_length;
        data = malloc(data_len);
        if (data) {
          memcpy(data, buf + byte_offset, data_len);
        }
      }
    } else {
      JS_GetException(ctx);
      size_t size;
      uint8_t* buf = JS_GetArrayBuffer(ctx, &size, argv[0]);
      if (buf) {
        data_len = size;
        data = malloc(data_len);
        if (data) {
          memcpy(data, buf, data_len);
        }
      }
    }
  }

  if (!data && data_len > 0) {
    return JS_ThrowTypeError(ctx, "data must be a string, Buffer, or ArrayBuffer");
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(data);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    free(data);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;
  work->flags = fh->fd;  // Store fd in flags field
  work->buffer = data;
  work->buffer_size = data_len;

  // Truncate file first, then write
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  // Write data at position 0
  uv_buf_t iov = uv_buf_init((char*)work->buffer, (unsigned int)work->buffer_size);
  int result = uv_fs_write(loop, &work->req, fh->fd, &iov, 1, 0, filehandle_writefile_write_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "write", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.appendFile() - returns Promise<void>
static JSValue filehandle_appendFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "appendFile requires data");
  }

  // Get data to append
  uint8_t* data = NULL;
  size_t data_len = 0;

  if (JS_IsString(argv[0])) {
    const char* str = JS_ToCString(ctx, argv[0]);
    if (str) {
      data_len = strlen(str);
      data = malloc(data_len);
      if (data) {
        memcpy(data, str, data_len);
      }
      JS_FreeCString(ctx, str);
    }
  } else {
    // Try TypedArray/Buffer/ArrayBuffer
    size_t byte_offset, byte_length, bytes_per_element;
    JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[0], &byte_offset, &byte_length, &bytes_per_element);
    if (!JS_IsException(array_buffer)) {
      size_t size;
      uint8_t* buf = JS_GetArrayBuffer(ctx, &size, array_buffer);
      JS_FreeValue(ctx, array_buffer);
      if (buf) {
        data_len = byte_length;
        data = malloc(data_len);
        if (data) {
          memcpy(data, buf + byte_offset, data_len);
        }
      }
    } else {
      JS_GetException(ctx);
      size_t size;
      uint8_t* buf = JS_GetArrayBuffer(ctx, &size, argv[0]);
      if (buf) {
        data_len = size;
        data = malloc(data_len);
        if (data) {
          memcpy(data, buf, data_len);
        }
      }
    }
  }

  if (!data && data_len > 0) {
    return JS_ThrowTypeError(ctx, "data must be a string, Buffer, or ArrayBuffer");
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    free(data);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    free(data);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;
  work->flags = fh->fd;  // Store fd in flags field
  work->buffer = data;
  work->buffer_size = data_len;

  // Append at end of file (position -1)
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  uv_buf_t iov = uv_buf_init((char*)work->buffer, (unsigned int)work->buffer_size);
  int result = uv_fs_write(loop, &work->req, fh->fd, &iov, 1, -1, filehandle_writefile_write_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "write", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// Completion for readv operation - returns { bytesRead, buffers }
static void fs_promise_complete_readv(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "readv", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Create result object { bytesRead, buffers }
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "bytesRead", JS_NewInt64(ctx, req->result));

    // Retrieve the buffers array we stored
    JSValue buffers = *(JSValue*)work->buffer;
    JS_SetPropertyStr(ctx, result, "buffers", JS_DupValue(ctx, buffers));

    // Resolve with result
    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &result);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// Completion for writev operation - returns { bytesWritten, buffers }
static void fs_promise_complete_writev(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "writev", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Create result object { bytesWritten, buffers }
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "bytesWritten", JS_NewInt64(ctx, req->result));

    // Retrieve the buffers array we stored
    JSValue buffers = *(JSValue*)work->buffer;
    JS_SetPropertyStr(ctx, result, "buffers", JS_DupValue(ctx, buffers));

    // Resolve with result
    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &result);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// FileHandle.readv() - vectored read, returns Promise<{ bytesRead, buffers }>
static JSValue filehandle_readv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  if (argc < 1 || !JS_IsArray(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "readv requires an array of buffers");
  }

  // Get array length
  JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
  int32_t buffer_count;
  if (JS_ToInt32(ctx, &buffer_count, length_val)) {
    JS_FreeValue(ctx, length_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length_val);

  if (buffer_count <= 0 || buffer_count > 1024) {
    return JS_ThrowRangeError(ctx, "Invalid buffer count");
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;

  // Store buffers array as JSValue for callback
  work->buffer = malloc(sizeof(JSValue));
  if (!work->buffer) {
    fs_promise_work_free(work);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }
  *(JSValue*)work->buffer = JS_DupValue(ctx, argv[0]);
  work->buffer_size = buffer_count;

  // Parse optional position (defaults to current position -1)
  int64_t position = -1;
  if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
    if (JS_ToInt64(ctx, &position, argv[1])) {
      fs_promise_work_free(work);
      JS_FreeValue(ctx, promise);
      return JS_EXCEPTION;
    }
  }

  // Prepare uv_buf_t array
  uv_buf_t* bufs = malloc(sizeof(uv_buf_t) * buffer_count);
  if (!bufs) {
    fs_promise_work_free(work);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Extract buffers
  for (int i = 0; i < buffer_count; i++) {
    JSValue buf_val = JS_GetPropertyUint32(ctx, argv[0], i);
    size_t byte_offset, byte_length, bytes_per_element;
    JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, buf_val, &byte_offset, &byte_length, &bytes_per_element);

    if (JS_IsException(array_buffer)) {
      JS_FreeValue(ctx, buf_val);
      free(bufs);
      fs_promise_work_free(work);
      JS_FreeValue(ctx, promise);
      return JS_ThrowTypeError(ctx, "All elements must be TypedArrays");
    }

    size_t ab_size;
    uint8_t* buffer = JS_GetArrayBuffer(ctx, &ab_size, array_buffer);
    JS_FreeValue(ctx, array_buffer);
    JS_FreeValue(ctx, buf_val);

    if (!buffer) {
      free(bufs);
      fs_promise_work_free(work);
      JS_FreeValue(ctx, promise);
      return JS_ThrowTypeError(ctx, "Invalid buffer");
    }

    bufs[i] = uv_buf_init((char*)(buffer + byte_offset), byte_length);
  }

  // Queue async readv operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_read(loop, &work->req, fh->fd, bufs, buffer_count, position, fs_promise_complete_readv);

  free(bufs);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "readv", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle.writev() - vectored write, returns Promise<{ bytesWritten, buffers }>
static JSValue filehandle_writev(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  FileHandle* fh = JS_GetOpaque(this_val, filehandle_class_id);
  if (!fh) {
    return JS_ThrowTypeError(ctx, "Not a FileHandle");
  }

  if (fh->closed) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "File handle is closed"));
    return JS_Throw(ctx, error);
  }

  if (argc < 1 || !JS_IsArray(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "writev requires an array of buffers");
  }

  // Get array length
  JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
  int32_t buffer_count;
  if (JS_ToInt32(ctx, &buffer_count, length_val)) {
    JS_FreeValue(ctx, length_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length_val);

  if (buffer_count <= 0 || buffer_count > 1024) {
    return JS_ThrowRangeError(ctx, "Invalid buffer count");
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = fh->path ? strdup(fh->path) : NULL;

  // Store buffers array as JSValue for callback
  work->buffer = malloc(sizeof(JSValue));
  if (!work->buffer) {
    fs_promise_work_free(work);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }
  *(JSValue*)work->buffer = JS_DupValue(ctx, argv[0]);
  work->buffer_size = buffer_count;

  // Parse optional position (defaults to current position -1)
  int64_t position = -1;
  if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
    if (JS_ToInt64(ctx, &position, argv[1])) {
      fs_promise_work_free(work);
      JS_FreeValue(ctx, promise);
      return JS_EXCEPTION;
    }
  }

  // Prepare uv_buf_t array
  uv_buf_t* bufs = malloc(sizeof(uv_buf_t) * buffer_count);
  if (!bufs) {
    fs_promise_work_free(work);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Extract buffers
  for (int i = 0; i < buffer_count; i++) {
    JSValue buf_val = JS_GetPropertyUint32(ctx, argv[0], i);
    size_t byte_offset, byte_length, bytes_per_element;
    JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, buf_val, &byte_offset, &byte_length, &bytes_per_element);

    if (JS_IsException(array_buffer)) {
      JS_FreeValue(ctx, buf_val);
      free(bufs);
      fs_promise_work_free(work);
      JS_FreeValue(ctx, promise);
      return JS_ThrowTypeError(ctx, "All elements must be TypedArrays");
    }

    size_t ab_size;
    uint8_t* buffer = JS_GetArrayBuffer(ctx, &ab_size, array_buffer);
    JS_FreeValue(ctx, array_buffer);
    JS_FreeValue(ctx, buf_val);

    if (!buffer) {
      free(bufs);
      fs_promise_work_free(work);
      JS_FreeValue(ctx, promise);
      return JS_ThrowTypeError(ctx, "Invalid buffer");
    }

    bufs[i] = uv_buf_init((char*)(buffer + byte_offset), byte_length);
  }

  // Queue async writev operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_write(loop, &work->req, fh->fd, bufs, buffer_count, position, fs_promise_complete_writev);

  free(bufs);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "writev", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// FileHandle[Symbol.asyncDispose]() - async disposal, returns Promise<void>
static JSValue filehandle_async_dispose(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Symbol.asyncDispose should call close() and return the promise
  return filehandle_close(ctx, this_val, argc, argv);
}

// FileHandle method list
static const JSCFunctionListEntry filehandle_proto_funcs[] = {
    JS_CFUNC_DEF("close", 0, filehandle_close),
    JS_CFUNC_DEF("read", 4, filehandle_read),
    JS_CFUNC_DEF("write", 4, filehandle_write),
    JS_CFUNC_DEF("stat", 0, filehandle_stat),
    JS_CFUNC_DEF("chmod", 1, filehandle_chmod),
    JS_CFUNC_DEF("chown", 2, filehandle_chown),
    JS_CFUNC_DEF("utimes", 2, filehandle_utimes),
    JS_CFUNC_DEF("truncate", 1, filehandle_truncate),
    JS_CFUNC_DEF("sync", 0, filehandle_sync),
    JS_CFUNC_DEF("datasync", 0, filehandle_datasync),
    JS_CFUNC_DEF("readFile", 0, filehandle_readFile),
    JS_CFUNC_DEF("writeFile", 1, filehandle_writeFile),
    JS_CFUNC_DEF("appendFile", 1, filehandle_appendFile),
    JS_CFUNC_DEF("readv", 1, filehandle_readv),
    JS_CFUNC_DEF("writev", 1, filehandle_writev),
};

// ============================================================================
// fsPromises.open() - returns Promise<FileHandle>
// ============================================================================

JSValue js_fs_promises_open(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "open requires at least a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Parse flags (default: 'r')
  int flags = O_RDONLY;
  if (argc >= 2 && JS_IsString(argv[1])) {
    const char* flags_str = JS_ToCString(ctx, argv[1]);
    if (flags_str) {
      if (strcmp(flags_str, "r") == 0)
        flags = O_RDONLY;
      else if (strcmp(flags_str, "r+") == 0)
        flags = O_RDWR;
      else if (strcmp(flags_str, "w") == 0)
        flags = O_WRONLY | O_CREAT | O_TRUNC;
      else if (strcmp(flags_str, "w+") == 0)
        flags = O_RDWR | O_CREAT | O_TRUNC;
      else if (strcmp(flags_str, "a") == 0)
        flags = O_WRONLY | O_CREAT | O_APPEND;
      else if (strcmp(flags_str, "a+") == 0)
        flags = O_RDWR | O_CREAT | O_APPEND;
      JS_FreeCString(ctx, flags_str);
    }
  } else if (argc >= 2 && JS_IsNumber(argv[1])) {
    int32_t flags_int;
    JS_ToInt32(ctx, &flags_int, argv[1]);
    flags = flags_int;
  }

  // Parse mode (default: 0666)
  int mode = 0666;
  if (argc >= 3 && JS_IsNumber(argv[2])) {
    int32_t mode_int;
    JS_ToInt32(ctx, &mode_int, argv[2]);
    mode = mode_int;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize work request
  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Queue async open operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_open(loop, &work->req, work->path, flags, mode, fs_promise_complete_fd);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "open", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// ============================================================================
// fsPromises wrapper methods (wrappers around existing async functions)
// ============================================================================

// fs.promises.stat() - wrapper around existing async stat
JSValue js_fs_promises_stat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "stat requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Queue async stat operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_stat(loop, &work->req, work->path, filehandle_stat_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "stat", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.lstat() - wrapper around existing async lstat
JSValue js_fs_promises_lstat(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "lstat requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Queue async lstat operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_lstat(loop, &work->req, work->path, filehandle_stat_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "lstat", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.unlink() - wrapper
JSValue js_fs_promises_unlink(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "unlink requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Queue async unlink operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_unlink(loop, &work->req, work->path, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "unlink", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.rename() - wrapper
JSValue js_fs_promises_rename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "rename requires old and new paths");
  }

  const char* oldPath = JS_ToCString(ctx, argv[0]);
  const char* newPath = JS_ToCString(ctx, argv[1]);
  if (!oldPath || !newPath) {
    if (oldPath)
      JS_FreeCString(ctx, oldPath);
    if (newPath)
      JS_FreeCString(ctx, newPath);
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, oldPath);
    JS_FreeCString(ctx, newPath);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, oldPath);
    JS_FreeCString(ctx, newPath);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(oldPath);
  work->path2 = strdup(newPath);
  JS_FreeCString(ctx, oldPath);
  JS_FreeCString(ctx, newPath);

  // Queue async rename operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_rename(loop, &work->req, work->path, work->path2, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "rename", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.mkdir() - wrapper
JSValue js_fs_promises_mkdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "mkdir requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Parse mode (default: 0777) and recursive option
  int32_t mode = 0777;
  bool recursive = false;
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue mode_val = JS_GetPropertyStr(ctx, argv[1], "mode");
    if (!JS_IsUndefined(mode_val)) {
      JS_ToInt32(ctx, &mode, mode_val);
    }
    JS_FreeValue(ctx, mode_val);

    JSValue recursive_val = JS_GetPropertyStr(ctx, argv[1], "recursive");
    if (JS_IsBool(recursive_val)) {
      recursive = JS_ToBool(ctx, recursive_val);
    }
    JS_FreeValue(ctx, recursive_val);
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  work->mode = mode;
  work->flags = recursive ? 1 : 0;  // Store recursive flag in flags field
  JS_FreeCString(ctx, path);

  // Queue async mkdir operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result;

  if (recursive) {
    // For recursive mkdir, we need to use our own implementation
    // since uv_fs_mkdir doesn't support recursive creation
    // We'll use a timer to make it async
    work->timer.data = work;                           // Store work pointer in timer
    work->result = mkdir_recursive(work->path, mode);  // Perform the operation
    result = uv_timer_init(loop, &work->timer);
    if (result == 0) {
      result = uv_timer_start(&work->timer, fs_promise_mkdir_recursive_timer_cb, 0, 0);
    }
  } else {
    // Non-recursive: use libuv
    result = uv_fs_mkdir(loop, &work->req, work->path, mode, fs_promise_complete_void);
  }

  if (result < 0) {
    JSValue error;
    if (recursive) {
      // For recursive mkdir, errno is set by mkdir_recursive
      error = create_fs_error(ctx, errno, "mkdir", work->path);
    } else {
      // For non-recursive, result is negative UV error code
      error = create_fs_error(ctx, -result, "mkdir", work->path);
    }
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.rmdir() - wrapper
JSValue js_fs_promises_rmdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "rmdir requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Queue async rmdir operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_rmdir(loop, &work->req, work->path, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "rmdir", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.readlink() - wrapper
JSValue js_fs_promises_readlink(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "readlink requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Queue async readlink operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_readlink(loop, &work->req, work->path, fs_promise_complete_string);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "readlink", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// ============================================================================
// fsPromises recursive and link operations
// ============================================================================

// Readdir completion callback
static void fs_promise_complete_readdir(uv_fs_t* req) {
  fs_promise_work_t* work = (fs_promise_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Reject with error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "readdir", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
  } else {
    // Resolve with array of filenames
    JSValue files_array = JS_NewArray(ctx);
    int index = 0;
    uv_dirent_t dent;

    // Iterate through directory entries
    while (uv_fs_scandir_next(req, &dent) != UV_EOF) {
      JS_SetPropertyUint32(ctx, files_array, index++, JS_NewString(ctx, dent.name));
    }

    JSValue ret = JS_Call(ctx, work->resolve, JS_UNDEFINED, 1, &files_array);
    JS_FreeValue(ctx, files_array);
    JS_FreeValue(ctx, ret);
  }

  fs_promise_work_free(work);
}

// fs.promises.readdir() - returns Promise<string[]>
JSValue js_fs_promises_readdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "readdir requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Queue async scandir operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_scandir(loop, &work->req, work->path, 0, fs_promise_complete_readdir);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "readdir", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.link() - returns Promise<void>
JSValue js_fs_promises_link(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "link requires existingPath and newPath");
  }

  const char* existingPath = JS_ToCString(ctx, argv[0]);
  const char* newPath = JS_ToCString(ctx, argv[1]);
  if (!existingPath || !newPath) {
    if (existingPath)
      JS_FreeCString(ctx, existingPath);
    if (newPath)
      JS_FreeCString(ctx, newPath);
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, existingPath);
    JS_FreeCString(ctx, newPath);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, existingPath);
    JS_FreeCString(ctx, newPath);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(existingPath);
  work->path2 = strdup(newPath);
  JS_FreeCString(ctx, existingPath);
  JS_FreeCString(ctx, newPath);

  // Queue async link operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_link(loop, &work->req, work->path, work->path2, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "link", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.symlink() - returns Promise<void>
JSValue js_fs_promises_symlink(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "symlink requires target and path");
  }

  const char* target = JS_ToCString(ctx, argv[0]);
  const char* path = JS_ToCString(ctx, argv[1]);
  if (!target || !path) {
    if (target)
      JS_FreeCString(ctx, target);
    if (path)
      JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Parse type (optional, mainly for Windows)
  int flags = 0;  // 0 = file symlink, UV_FS_SYMLINK_DIR = directory symlink
  if (argc >= 3 && JS_IsString(argv[2])) {
    const char* type = JS_ToCString(ctx, argv[2]);
    if (type) {
      if (strcmp(type, "dir") == 0 || strcmp(type, "directory") == 0) {
        flags = UV_FS_SYMLINK_DIR;
      }
      JS_FreeCString(ctx, type);
    }
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, target);
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, target);
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(target);  // source
  work->path2 = strdup(path);   // destination
  work->flags = flags;
  JS_FreeCString(ctx, target);
  JS_FreeCString(ctx, path);

  // Queue async symlink operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_symlink(loop, &work->req, work->path, work->path2, flags, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "symlink", work->path2);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.realpath() - returns Promise<string>
JSValue js_fs_promises_realpath(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "realpath requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  // Allocate work request
  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Queue async realpath operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_realpath(loop, &work->req, work->path, fs_promise_complete_string);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "realpath", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// ============================================================================
// Recursive operations - Promise wrappers around sync implementations
// ============================================================================

// Helper to run sync operations in Promise context
// Note: For a truly async implementation, this should use uv_queue_work
// For now, we run synchronously and resolve/reject the promise immediately
static JSValue run_sync_as_promise(JSContext* ctx, JSValue (*sync_fn)(JSContext*, JSValueConst, int, JSValueConst*),
                                   int argc, JSValueConst* argv) {
  // Create Promise
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  // Call sync function
  JSValue result = sync_fn(ctx, JS_UNDEFINED, argc, argv);

  if (JS_IsException(result)) {
    // Reject with error
    JSValue exception = JS_GetException(ctx);
    JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &exception);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, result);
  } else {
    // Resolve with result (or undefined for void operations)
    JSValue ret = JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, result);
  }

  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);

  return promise;
}

// fs.promises.rm() - returns Promise<void>
JSValue js_fs_promises_rm(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "rm requires a path");
  }

  // Call rmSync and wrap in Promise
  return run_sync_as_promise(ctx, js_fs_rm_sync, argc, argv);
}

// fs.promises.cp() - returns Promise<void>
JSValue js_fs_promises_cp(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "cp requires src and dest");
  }

  // Call cpSync and wrap in Promise
  return run_sync_as_promise(ctx, js_fs_cp_sync, argc, argv);
}

// ============================================================================
// fsPromises metadata operations
// ============================================================================

// fs.promises.chmod() - returns Promise<void>
JSValue js_fs_promises_chmod(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "chmod requires path and mode");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, argv[1])) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_chmod(loop, &work->req, work->path, mode, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "chmod", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.lchmod() - returns Promise<void>
JSValue js_fs_promises_lchmod(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "lchmod requires path and mode");
  }

  // Note: libuv doesn't have uv_fs_lchmod, reject immediately with ENOSYS
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    return JS_EXCEPTION;
  }

  JSValue error = create_fs_error(ctx, UV_ENOSYS, "lchmod", JS_ToCString(ctx, argv[0]));
  JSValue ret = JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
  JS_FreeValue(ctx, error);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);

  return promise;
}

// fs.promises.chown() - returns Promise<void>
JSValue js_fs_promises_chown(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "chown requires path, uid, and gid");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t uid, gid;
  if (JS_ToInt32(ctx, &uid, argv[1]) || JS_ToInt32(ctx, &gid, argv[2])) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_chown(loop, &work->req, work->path, uid, gid, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "chown", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.lchown() - returns Promise<void>
JSValue js_fs_promises_lchown(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "lchown requires path, uid, and gid");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t uid, gid;
  if (JS_ToInt32(ctx, &uid, argv[1]) || JS_ToInt32(ctx, &gid, argv[2])) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_lchown(loop, &work->req, work->path, uid, gid, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "lchown", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.utimes() - returns Promise<void>
JSValue js_fs_promises_utimes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "utimes requires path, atime, and mtime");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  double atime, mtime;
  if (JS_ToFloat64(ctx, &atime, argv[1]) || JS_ToFloat64(ctx, &mtime, argv[2])) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_utime(loop, &work->req, work->path, atime, mtime, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "utimes", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.lutimes() - returns Promise<void>
JSValue js_fs_promises_lutimes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "lutimes requires path, atime, and mtime");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  double atime, mtime;
  if (JS_ToFloat64(ctx, &atime, argv[1]) || JS_ToFloat64(ctx, &mtime, argv[2])) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_lutime(loop, &work->req, work->path, atime, mtime, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "lutimes", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.access() - returns Promise<void> or rejects
JSValue js_fs_promises_access(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "access requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t mode = 0;  // F_OK
  if (argc >= 2 && !JS_IsUndefined(argv[1])) {
    JS_ToInt32(ctx, &mode, argv[1]);
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_access(loop, &work->req, work->path, mode, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "access", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// ============================================================================
// File I/O operations (readFile, writeFile, appendFile)
// ============================================================================

// Multi-step Promise-based readFile
typedef struct {
  fs_promise_work_t base;
  int fd;
  uint8_t* buffer;
  size_t size;
  size_t bytes_read;
} readfile_promise_work_t;

static void readfile_promise_close_cb(uv_fs_t* req) {
  readfile_promise_work_t* work = (readfile_promise_work_t*)req;
  JSContext* ctx = work->base.ctx;

  // Create ArrayBuffer with the data (handles empty buffer)
  JSValue buffer = JS_NewArrayBufferCopy(ctx, work->buffer ? work->buffer : (const uint8_t*)"", work->bytes_read);
  JSValue ret = JS_Call(ctx, work->base.resolve, JS_UNDEFINED, 1, &buffer);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, buffer);

  if (work->buffer) {
    free(work->buffer);
  }
  fs_promise_work_free(&work->base);
}

static void readfile_promise_read_cb(uv_fs_t* req) {
  readfile_promise_work_t* work = (readfile_promise_work_t*)req;
  JSContext* ctx = work->base.ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->fd, NULL);
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "read", work->base.path);
    JSValue ret = JS_Call(ctx, work->base.reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);

    free(work->buffer);
    fs_promise_work_free(&work->base);
    return;
  }

  work->bytes_read = (size_t)req->result;

  // Close file
  uv_fs_req_cleanup(req);
  uv_fs_close(loop, req, work->fd, readfile_promise_close_cb);
}

static void readfile_promise_stat_cb(uv_fs_t* req) {
  readfile_promise_work_t* work = (readfile_promise_work_t*)req;
  JSContext* ctx = work->base.ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->fd, NULL);
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "fstat", work->base.path);
    JSValue ret = JS_Call(ctx, work->base.reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);

    fs_promise_work_free(&work->base);
    return;
  }

  uv_stat_t* stat = (uv_stat_t*)req->ptr;
  work->size = stat->st_size;

  // Handle empty file case (malloc(0) is implementation-defined)
  if (work->size == 0) {
    work->buffer = NULL;
    work->bytes_read = 0;

    // Skip read, go directly to close
    uv_fs_req_cleanup(req);
    uv_fs_close(loop, req, work->fd, readfile_promise_close_cb);
    return;
  }

  work->buffer = malloc(work->size);
  if (!work->buffer) {
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->fd, NULL);
    uv_fs_req_cleanup(&close_req);

    JSValue error = JS_NewError(ctx);
    JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, "Out of memory"), JS_PROP_C_W_E);
    JSValue ret = JS_Call(ctx, work->base.reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);

    fs_promise_work_free(&work->base);
    return;
  }

  // Read file contents
  uv_buf_t buf = uv_buf_init((char*)work->buffer, (unsigned int)work->size);
  uv_fs_req_cleanup(req);
  uv_fs_read(loop, req, work->fd, &buf, 1, 0, readfile_promise_read_cb);
}

static void readfile_promise_open_cb(uv_fs_t* req) {
  readfile_promise_work_t* work = (readfile_promise_work_t*)req;
  JSContext* ctx = work->base.ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "open", work->base.path);
    JSValue ret = JS_Call(ctx, work->base.reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);

    fs_promise_work_free(&work->base);
    return;
  }

  work->fd = (int)req->result;

  // Get file size with fstat
  uv_fs_req_cleanup(req);
  uv_fs_fstat(loop, req, work->fd, readfile_promise_stat_cb);
}

// fs.promises.readFile(path) - returns Promise<ArrayBuffer>
JSValue js_fs_promises_readFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "readFile requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  readfile_promise_work_t* work = calloc(1, sizeof(readfile_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->base.ctx = ctx;
  work->base.resolve = resolving_funcs[0];
  work->base.reject = resolving_funcs[1];
  work->base.path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_open(loop, (uv_fs_t*)&work->base.req, work->base.path, O_RDONLY, 0, readfile_promise_open_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "open", work->base.path);
    JSValue ret = JS_Call(ctx, work->base.reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(&work->base);
  }

  return promise;
}

// Multi-step Promise-based writeFile
typedef struct {
  fs_promise_work_t base;
  int fd;
  uint8_t* buffer;
  size_t size;
  int flags;  // O_WRONLY|O_CREAT|O_TRUNC or O_WRONLY|O_CREAT|O_APPEND
} writefile_promise_work_t;

static void writefile_promise_close_cb(uv_fs_t* req) {
  writefile_promise_work_t* work = (writefile_promise_work_t*)req;
  JSContext* ctx = work->base.ctx;

  // Success - resolve with undefined
  JSValue undef = JS_UNDEFINED;
  JSValue ret = JS_Call(ctx, work->base.resolve, JS_UNDEFINED, 1, &undef);
  JS_FreeValue(ctx, ret);

  free(work->buffer);
  fs_promise_work_free(&work->base);
}

static void writefile_promise_write_cb(uv_fs_t* req) {
  writefile_promise_work_t* work = (writefile_promise_work_t*)req;
  JSContext* ctx = work->base.ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->fd, NULL);
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "write", work->base.path);
    JSValue ret = JS_Call(ctx, work->base.reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);

    free(work->buffer);
    fs_promise_work_free(&work->base);
    return;
  }

  // Close file
  uv_fs_req_cleanup(req);
  uv_fs_close(loop, req, work->fd, writefile_promise_close_cb);
}

static void writefile_promise_open_cb(uv_fs_t* req) {
  writefile_promise_work_t* work = (writefile_promise_work_t*)req;
  JSContext* ctx = work->base.ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "open", work->base.path);
    JSValue ret = JS_Call(ctx, work->base.reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);

    free(work->buffer);
    fs_promise_work_free(&work->base);
    return;
  }

  work->fd = (int)req->result;

  // Write data
  // For append mode, use -1 as offset to append at end of file
  int64_t offset = (work->flags & O_APPEND) ? -1 : 0;
  uv_buf_t buf = uv_buf_init((char*)work->buffer, (unsigned int)work->size);
  uv_fs_req_cleanup(req);
  uv_fs_write(loop, req, work->fd, &buf, 1, offset, writefile_promise_write_cb);
}

// fs.promises.writeFile(path, data) - returns Promise<void>
JSValue js_fs_promises_writeFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "writeFile requires path and data");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Get data to write
  uint8_t* data = NULL;
  size_t data_len = 0;

  if (JS_IsString(argv[1])) {
    const char* str = JS_ToCString(ctx, argv[1]);
    if (str) {
      data_len = strlen(str);
      data = malloc(data_len);
      if (data) {
        memcpy(data, str, data_len);
      }
      JS_FreeCString(ctx, str);
    }
  } else {
    // Try to get as TypedArray (includes Buffer) or ArrayBuffer
    size_t byte_offset, byte_length, bytes_per_element;
    JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[1], &byte_offset, &byte_length, &bytes_per_element);
    if (!JS_IsException(array_buffer)) {
      size_t size;
      uint8_t* buf = JS_GetArrayBuffer(ctx, &size, array_buffer);
      JS_FreeValue(ctx, array_buffer);
      if (buf) {
        data_len = byte_length;
        data = malloc(data_len);
        if (data) {
          memcpy(data, buf + byte_offset, data_len);
        }
      }
    } else {
      // Clear exception and try direct ArrayBuffer
      JSValue exc = JS_GetException(ctx);
      JS_FreeValue(ctx, exc);
      size_t size;
      uint8_t* buf = JS_GetArrayBuffer(ctx, &size, argv[1]);
      if (buf) {
        data_len = size;
        data = malloc(data_len);
        if (data) {
          memcpy(data, buf, data_len);
        }
      }
    }
  }

  if (!data) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "data must be a string, Buffer, or ArrayBuffer");
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    free(data);
    return JS_EXCEPTION;
  }

  writefile_promise_work_t* work = calloc(1, sizeof(writefile_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    free(data);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->base.ctx = ctx;
  work->base.resolve = resolving_funcs[0];
  work->base.reject = resolving_funcs[1];
  work->base.path = strdup(path);
  work->buffer = data;
  work->size = data_len;
  work->flags = O_WRONLY | O_CREAT | O_TRUNC;
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result =
      uv_fs_open(loop, (uv_fs_t*)&work->base.req, work->base.path, work->flags, 0644, writefile_promise_open_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "open", work->base.path);
    JSValue ret = JS_Call(ctx, work->base.reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    free(work->buffer);
    fs_promise_work_free(&work->base);
  }

  return promise;
}

// fs.promises.appendFile(path, data) - returns Promise<void>
JSValue js_fs_promises_appendFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "appendFile requires path and data");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Get data to write
  uint8_t* data = NULL;
  size_t data_len = 0;

  if (JS_IsString(argv[1])) {
    const char* str = JS_ToCString(ctx, argv[1]);
    if (str) {
      data_len = strlen(str);
      data = malloc(data_len);
      if (data) {
        memcpy(data, str, data_len);
      }
      JS_FreeCString(ctx, str);
    }
  } else {
    // Try to get as TypedArray (includes Buffer) or ArrayBuffer
    size_t byte_offset, byte_length, bytes_per_element;
    JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[1], &byte_offset, &byte_length, &bytes_per_element);
    if (!JS_IsException(array_buffer)) {
      size_t size;
      uint8_t* buf = JS_GetArrayBuffer(ctx, &size, array_buffer);
      JS_FreeValue(ctx, array_buffer);
      if (buf) {
        data_len = byte_length;
        data = malloc(data_len);
        if (data) {
          memcpy(data, buf + byte_offset, data_len);
        }
      }
    } else {
      // Clear exception and try direct ArrayBuffer
      JSValue exc = JS_GetException(ctx);
      JS_FreeValue(ctx, exc);
      size_t size;
      uint8_t* buf = JS_GetArrayBuffer(ctx, &size, argv[1]);
      if (buf) {
        data_len = size;
        data = malloc(data_len);
        if (data) {
          memcpy(data, buf, data_len);
        }
      }
    }
  }

  if (!data) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "data must be a string, Buffer, or ArrayBuffer");
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    free(data);
    return JS_EXCEPTION;
  }

  writefile_promise_work_t* work = calloc(1, sizeof(writefile_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    free(data);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->base.ctx = ctx;
  work->base.resolve = resolving_funcs[0];
  work->base.reject = resolving_funcs[1];
  work->base.path = strdup(path);
  work->buffer = data;
  work->size = data_len;
  work->flags = O_WRONLY | O_CREAT | O_APPEND;  // Append mode
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result =
      uv_fs_open(loop, (uv_fs_t*)&work->base.req, work->base.path, work->flags, 0644, writefile_promise_open_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "open", work->base.path);
    JSValue ret = JS_Call(ctx, work->base.reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    free(work->buffer);
    fs_promise_work_free(&work->base);
  }

  return promise;
}

// ============================================================================
// Additional Promise APIs (Phase B1)
// ============================================================================

// fs.promises.mkdtemp() - returns Promise<string>
JSValue js_fs_promises_mkdtemp(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "mkdtemp requires a prefix");
  }

  const char* prefix = JS_ToCString(ctx, argv[0]);
  if (!prefix) {
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, prefix);
    return JS_EXCEPTION;
  }

  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, prefix);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(prefix);
  JS_FreeCString(ctx, prefix);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_mkdtemp(loop, &work->req, work->path, fs_promise_complete_string);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "mkdtemp", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.truncate() - returns Promise<void>
JSValue js_fs_promises_truncate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "truncate requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int64_t len = 0;
  if (argc >= 2 && JS_ToInt64(ctx, &len, argv[1])) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, path);
    return JS_EXCEPTION;
  }

  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_ftruncate(loop, &work->req, -1, len, fs_promise_complete_void);  // Will use path-based truncate

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "truncate", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// fs.promises.copyFile() - returns Promise<void>
JSValue js_fs_promises_copyFile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "copyFile requires source and destination");
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

  int flags = 0;
  if (argc >= 3 && JS_ToInt32(ctx, &flags, argv[2])) {
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_EXCEPTION;
  }

  fs_promise_work_t* work = calloc(1, sizeof(fs_promise_work_t));
  if (!work) {
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    JS_FreeValue(ctx, promise);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->ctx = ctx;
  work->resolve = resolving_funcs[0];
  work->reject = resolving_funcs[1];
  work->path = strdup(src);
  work->path2 = strdup(dest);
  work->flags = flags;
  JS_FreeCString(ctx, src);
  JS_FreeCString(ctx, dest);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_copyfile(loop, &work->req, work->path, work->path2, flags, fs_promise_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "copyfile", work->path);
    JSValue ret = JS_Call(ctx, work->reject, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, ret);
    fs_promise_work_free(work);
  }

  return promise;
}

// ============================================================================
// fsPromises namespace initialization
// ============================================================================

JSValue JSRT_InitNodeFsPromises(JSContext* ctx) {
  JSValue promises = JS_NewObject(ctx);

  // FileHandle-based API
  JS_SetPropertyStr(ctx, promises, "open", JS_NewCFunction(ctx, js_fs_promises_open, "open", 3));

  // High-level file I/O
  JS_SetPropertyStr(ctx, promises, "readFile", JS_NewCFunction(ctx, js_fs_promises_readFile, "readFile", 1));
  JS_SetPropertyStr(ctx, promises, "writeFile", JS_NewCFunction(ctx, js_fs_promises_writeFile, "writeFile", 2));
  JS_SetPropertyStr(ctx, promises, "appendFile", JS_NewCFunction(ctx, js_fs_promises_appendFile, "appendFile", 2));

  // Path-based wrappers
  JS_SetPropertyStr(ctx, promises, "stat", JS_NewCFunction(ctx, js_fs_promises_stat, "stat", 1));
  JS_SetPropertyStr(ctx, promises, "lstat", JS_NewCFunction(ctx, js_fs_promises_lstat, "lstat", 1));
  JS_SetPropertyStr(ctx, promises, "unlink", JS_NewCFunction(ctx, js_fs_promises_unlink, "unlink", 1));
  JS_SetPropertyStr(ctx, promises, "rename", JS_NewCFunction(ctx, js_fs_promises_rename, "rename", 2));
  JS_SetPropertyStr(ctx, promises, "mkdir", JS_NewCFunction(ctx, js_fs_promises_mkdir, "mkdir", 2));
  JS_SetPropertyStr(ctx, promises, "rmdir", JS_NewCFunction(ctx, js_fs_promises_rmdir, "rmdir", 1));
  JS_SetPropertyStr(ctx, promises, "readdir", JS_NewCFunction(ctx, js_fs_promises_readdir, "readdir", 2));
  JS_SetPropertyStr(ctx, promises, "readlink", JS_NewCFunction(ctx, js_fs_promises_readlink, "readlink", 1));

  // Link operations
  JS_SetPropertyStr(ctx, promises, "link", JS_NewCFunction(ctx, js_fs_promises_link, "link", 2));
  JS_SetPropertyStr(ctx, promises, "symlink", JS_NewCFunction(ctx, js_fs_promises_symlink, "symlink", 3));
  JS_SetPropertyStr(ctx, promises, "realpath", JS_NewCFunction(ctx, js_fs_promises_realpath, "realpath", 2));

  // Recursive operations
  JS_SetPropertyStr(ctx, promises, "rm", JS_NewCFunction(ctx, js_fs_promises_rm, "rm", 2));
  JS_SetPropertyStr(ctx, promises, "cp", JS_NewCFunction(ctx, js_fs_promises_cp, "cp", 3));

  // Metadata operations
  JS_SetPropertyStr(ctx, promises, "chmod", JS_NewCFunction(ctx, js_fs_promises_chmod, "chmod", 2));
  JS_SetPropertyStr(ctx, promises, "lchmod", JS_NewCFunction(ctx, js_fs_promises_lchmod, "lchmod", 2));
  JS_SetPropertyStr(ctx, promises, "chown", JS_NewCFunction(ctx, js_fs_promises_chown, "chown", 3));
  JS_SetPropertyStr(ctx, promises, "lchown", JS_NewCFunction(ctx, js_fs_promises_lchown, "lchown", 3));
  JS_SetPropertyStr(ctx, promises, "utimes", JS_NewCFunction(ctx, js_fs_promises_utimes, "utimes", 3));
  JS_SetPropertyStr(ctx, promises, "lutimes", JS_NewCFunction(ctx, js_fs_promises_lutimes, "lutimes", 3));
  JS_SetPropertyStr(ctx, promises, "access", JS_NewCFunction(ctx, js_fs_promises_access, "access", 2));

  // Phase B1: Additional Promise APIs
  JS_SetPropertyStr(ctx, promises, "mkdtemp", JS_NewCFunction(ctx, js_fs_promises_mkdtemp, "mkdtemp", 1));
  JS_SetPropertyStr(ctx, promises, "truncate", JS_NewCFunction(ctx, js_fs_promises_truncate, "truncate", 2));
  JS_SetPropertyStr(ctx, promises, "copyFile", JS_NewCFunction(ctx, js_fs_promises_copyFile, "copyFile", 3));

  return promises;
}

// ============================================================================
// Module initialization
// ============================================================================

void fs_promises_init(JSContext* ctx) {
  // Register FileHandle class
  JS_NewClassID(&filehandle_class_id);
  JS_NewClass(JS_GetRuntime(ctx), filehandle_class_id, &filehandle_class);

  // Create prototype with methods
  JSValue proto = JS_NewObject(ctx);
  JS_SetPropertyFunctionList(ctx, proto, filehandle_proto_funcs,
                             sizeof(filehandle_proto_funcs) / sizeof(filehandle_proto_funcs[0]));

  // Add Symbol.asyncDispose method
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue symbol_obj = JS_GetPropertyStr(ctx, global, "Symbol");
  JSValue async_dispose_symbol = JS_GetPropertyStr(ctx, symbol_obj, "asyncDispose");
  JS_FreeValue(ctx, symbol_obj);
  JS_FreeValue(ctx, global);

  if (!JS_IsUndefined(async_dispose_symbol)) {
    JSValue dispose_method = JS_NewCFunction(ctx, filehandle_async_dispose, "[Symbol.asyncDispose]", 0);
    JS_SetProperty(ctx, proto, JS_ValueToAtom(ctx, async_dispose_symbol), dispose_method);
  }
  JS_FreeValue(ctx, async_dispose_symbol);

  JS_SetClassProto(ctx, filehandle_class_id, proto);
}
