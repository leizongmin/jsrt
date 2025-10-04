#include <fcntl.h>
#include "fs_async_libuv.h"

// ============================================================================
// Phase 3: Promise-based fs API
// ============================================================================

// Promise work request structure (extends fs_async_work_t concept)
typedef struct {
  uv_fs_t req;         // libuv fs request (MUST be first for casting)
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

// FileHandle method list
static const JSCFunctionListEntry filehandle_proto_funcs[] = {
    JS_CFUNC_DEF("close", 0, filehandle_close),
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
// fsPromises namespace initialization
// ============================================================================

JSValue JSRT_InitNodeFsPromises(JSContext* ctx) {
  JSValue promises = JS_NewObject(ctx);

  // Add fsPromises.open()
  JS_SetPropertyStr(ctx, promises, "open", JS_NewCFunction(ctx, js_fs_promises_open, "open", 3));

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
  JS_SetClassProto(ctx, filehandle_class_id, proto);
}
