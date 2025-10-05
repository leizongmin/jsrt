#include <stdlib.h>
#include <string.h>
#include "fs_async_libuv.h"

// ============================================================================
// readv: Vectored read operation
// ============================================================================

static void fs_readv_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Read error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "read", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);

    // Free JSValue stored in work->buffer
    JSValue* buffers_ref_cleanup = (JSValue*)work->buffer;
    if (buffers_ref_cleanup) {
      JS_FreeValue(ctx, *buffers_ref_cleanup);
      free(buffers_ref_cleanup);
      work->buffer = NULL;
    }

    fs_async_work_free(work);
    return;
  }

  // Success: callback(null, bytesRead, buffers)
  size_t bytes_read = (size_t)req->result;

  // The buffers array is stored in work->buffer as JSValue*
  JSValue* buffers_ref = (JSValue*)work->buffer;
  JSValue buffers_array = *buffers_ref;

  JSValue args[3];
  args[0] = JS_NULL;                          // err = null
  args[1] = JS_NewInt64(ctx, bytes_read);     // bytesRead
  args[2] = JS_DupValue(ctx, buffers_array);  // buffers (duplicate for callback)

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 3, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, args[2]);

  // Free JSValue stored in work->buffer before calling fs_async_work_free
  JSValue* buffers_ref_cleanup = (JSValue*)work->buffer;
  if (buffers_ref_cleanup) {
    JS_FreeValue(ctx, *buffers_ref_cleanup);
    free(buffers_ref_cleanup);
    work->buffer = NULL;  // Prevent double-free in fs_async_work_free
  }

  fs_async_work_free(work);
}

// fs.readv(fd, buffers, position, callback)
JSValue js_fs_readv_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "readv requires fd, buffers, and callback");
  }

  // Parse fd
  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "fd must be a number");
  }

  // Parse buffers array
  if (!JS_IsArray(ctx, argv[1])) {
    return JS_ThrowTypeError(ctx, "buffers must be an array");
  }

  JSValue length_val = JS_GetPropertyStr(ctx, argv[1], "length");
  uint32_t num_buffers;
  if (JS_ToUint32(ctx, &num_buffers, length_val) < 0) {
    JS_FreeValue(ctx, length_val);
    return JS_ThrowTypeError(ctx, "invalid buffers array");
  }
  JS_FreeValue(ctx, length_val);

  if (num_buffers == 0) {
    return JS_ThrowTypeError(ctx, "buffers array cannot be empty");
  }

  // Parse position (can be null for current position)
  int64_t position = -1;
  JSValue callback = argv[2];
  if (argc >= 4) {
    if (!JS_IsNull(argv[2])) {
      if (JS_ToInt64(ctx, &position, argv[2]) < 0) {
        return JS_ThrowTypeError(ctx, "position must be a number or null");
      }
    }
    callback = argv[3];
  }

  // Parse callback
  if (!JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Allocate uv_buf_t array
  uv_buf_t* bufs = malloc(num_buffers * sizeof(uv_buf_t));
  if (!bufs) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Fill uv_buf_t array from JS buffers
  for (uint32_t i = 0; i < num_buffers; i++) {
    JSValue buf_val = JS_GetPropertyUint32(ctx, argv[1], i);
    size_t buf_size;
    uint8_t* buf_data = JS_GetArrayBuffer(ctx, &buf_size, buf_val);
    JS_FreeValue(ctx, buf_val);

    if (!buf_data) {
      free(bufs);
      return JS_ThrowTypeError(ctx, "buffers must contain ArrayBuffer objects");
    }

    bufs[i] = uv_buf_init((char*)buf_data, (unsigned int)buf_size);
  }

  // Allocate work request
  fs_async_work_t* work = calloc(1, sizeof(fs_async_work_t));
  if (!work) {
    free(bufs);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Store buffers array reference
  JSValue* buffers_ref = malloc(sizeof(JSValue));
  if (!buffers_ref) {
    free(bufs);
    free(work);
    return JS_ThrowOutOfMemory(ctx);
  }
  *buffers_ref = JS_DupValue(ctx, argv[1]);

  work->ctx = ctx;
  work->callback = JS_DupValue(ctx, callback);
  work->path = NULL;
  work->buffer = (void*)buffers_ref;  // Store JSValue* for cleanup
  work->buffer_size = num_buffers;    // Store count for cleanup
  work->offset = position;

  // Queue async readv operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_read(loop, &work->req, fd, bufs, num_buffers, position, fs_readv_cb);

  // Free bufs array (uv_fs_read copies it)
  free(bufs);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "read", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, *buffers_ref);
    free(buffers_ref);
    free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// writev: Vectored write operation
// ============================================================================

static void fs_writev_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  if (req->result < 0) {
    // Write error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "write", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);

    // Free JSValue stored in work->buffer
    JSValue* buffers_ref_cleanup = (JSValue*)work->buffer;
    if (buffers_ref_cleanup) {
      JS_FreeValue(ctx, *buffers_ref_cleanup);
      free(buffers_ref_cleanup);
      work->buffer = NULL;
    }

    fs_async_work_free(work);
    return;
  }

  // Success: callback(null, bytesWritten, buffers)
  size_t bytes_written = (size_t)req->result;

  // The buffers array is stored in work->buffer as JSValue*
  JSValue* buffers_ref = (JSValue*)work->buffer;
  JSValue buffers_array = *buffers_ref;

  JSValue args[3];
  args[0] = JS_NULL;                          // err = null
  args[1] = JS_NewInt64(ctx, bytes_written);  // bytesWritten
  args[2] = JS_DupValue(ctx, buffers_array);  // buffers

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 3, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, args[2]);

  // Free JSValue stored in work->buffer before calling fs_async_work_free
  JSValue* buffers_ref_cleanup = (JSValue*)work->buffer;
  if (buffers_ref_cleanup) {
    JS_FreeValue(ctx, *buffers_ref_cleanup);
    free(buffers_ref_cleanup);
    work->buffer = NULL;  // Prevent double-free in fs_async_work_free
  }

  fs_async_work_free(work);
}

// fs.writev(fd, buffers, position, callback)
JSValue js_fs_writev_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "writev requires fd, buffers, and callback");
  }

  // Parse fd
  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "fd must be a number");
  }

  // Parse buffers array
  if (!JS_IsArray(ctx, argv[1])) {
    return JS_ThrowTypeError(ctx, "buffers must be an array");
  }

  JSValue length_val = JS_GetPropertyStr(ctx, argv[1], "length");
  uint32_t num_buffers;
  if (JS_ToUint32(ctx, &num_buffers, length_val) < 0) {
    JS_FreeValue(ctx, length_val);
    return JS_ThrowTypeError(ctx, "invalid buffers array");
  }
  JS_FreeValue(ctx, length_val);

  if (num_buffers == 0) {
    return JS_ThrowTypeError(ctx, "buffers array cannot be empty");
  }

  // Parse position (can be null for current position)
  int64_t position = -1;
  JSValue callback = argv[2];
  if (argc >= 4) {
    if (!JS_IsNull(argv[2])) {
      if (JS_ToInt64(ctx, &position, argv[2]) < 0) {
        return JS_ThrowTypeError(ctx, "position must be a number or null");
      }
    }
    callback = argv[3];
  }

  // Parse callback
  if (!JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Allocate uv_buf_t array
  uv_buf_t* bufs = malloc(num_buffers * sizeof(uv_buf_t));
  if (!bufs) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Fill uv_buf_t array from JS buffers
  for (uint32_t i = 0; i < num_buffers; i++) {
    JSValue buf_val = JS_GetPropertyUint32(ctx, argv[1], i);
    size_t buf_size;
    uint8_t* buf_data = JS_GetArrayBuffer(ctx, &buf_size, buf_val);
    JS_FreeValue(ctx, buf_val);

    if (!buf_data) {
      free(bufs);
      return JS_ThrowTypeError(ctx, "buffers must contain ArrayBuffer objects");
    }

    bufs[i] = uv_buf_init((char*)buf_data, (unsigned int)buf_size);
  }

  // Allocate work request
  fs_async_work_t* work = calloc(1, sizeof(fs_async_work_t));
  if (!work) {
    free(bufs);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Store buffers array reference
  JSValue* buffers_ref = malloc(sizeof(JSValue));
  if (!buffers_ref) {
    free(bufs);
    free(work);
    return JS_ThrowOutOfMemory(ctx);
  }
  *buffers_ref = JS_DupValue(ctx, argv[1]);

  work->ctx = ctx;
  work->callback = JS_DupValue(ctx, callback);
  work->path = NULL;
  work->buffer = (void*)buffers_ref;
  work->buffer_size = num_buffers;
  work->offset = position;

  // Queue async writev operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_write(loop, &work->req, fd, bufs, num_buffers, position, fs_writev_cb);

  // Free bufs array (uv_fs_write copies it)
  free(bufs);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "write", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, *buffers_ref);
    free(buffers_ref);
    free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// rm: Recursive remove (async wrapper - simplified for now)
// ============================================================================

// fs.rm(path, options, callback) - async recursive remove
JSValue js_fs_rm_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "rm requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (argc >= 3) {
    callback = argv[2];  // rm(path, options, callback)
  }

  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Call sync rmSync and report result asynchronously
  // For a full implementation, this should use uv_queue_work
  JSValue sync_args[2] = {argv[0], argc >= 3 ? argv[1] : JS_UNDEFINED};
  JSValue result = js_fs_rm_sync(ctx, this_val, argc >= 3 ? 2 : 1, sync_args);

  if (JS_IsException(result)) {
    // Extract error and call callback
    JSValue exception = JS_GetException(ctx);
    JSValue args[1] = {exception};
    JSValue ret = JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, exception);
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
  }

  // Success - call callback with null error
  JSValue args[1] = {JS_NULL};
  JSValue ret = JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, result);
  JS_FreeCString(ctx, path);

  return JS_UNDEFINED;
}

// ============================================================================
// cp: Recursive copy (async wrapper - simplified for now)
// ============================================================================

// fs.cp(src, dest, options, callback) - async recursive copy
JSValue js_fs_cp_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "cp requires src, dest, and callback");
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

  JSValue callback = argv[2];
  if (argc >= 4) {
    callback = argv[3];  // cp(src, dest, options, callback)
  }

  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Call sync cpSync and report result asynchronously
  JSValue sync_args[3] = {argv[0], argv[1], argc >= 4 ? argv[2] : JS_UNDEFINED};
  JSValue result = js_fs_cp_sync(ctx, this_val, argc >= 4 ? 3 : 2, sync_args);

  if (JS_IsException(result)) {
    // Extract error and call callback
    JSValue exception = JS_GetException(ctx);
    JSValue args[1] = {exception};
    JSValue ret = JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, exception);
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_UNDEFINED;
  }

  // Success
  JSValue args[1] = {JS_NULL};
  JSValue ret = JS_Call(ctx, callback, JS_UNDEFINED, 1, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, result);
  JS_FreeCString(ctx, src);
  JS_FreeCString(ctx, dest);

  return JS_UNDEFINED;
}
