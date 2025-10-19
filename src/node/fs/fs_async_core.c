#include <stdlib.h>
#include <string.h>
#include "fs_async_libuv.h"

// ============================================================================
// readFile: Multi-step async operation (open → fstat → read → close)
// ============================================================================

// Step 4: Close the file (final step)
static void readfile_close_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  // At this point, buffer already has data from read
  // Create callback args
  JSValue args[2];
  args[0] = JS_NULL;
  args[1] = create_buffer_from_data(ctx, (uint8_t*)work->buffer, work->buffer_size);

  // Call JS callback
  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 2, args);
  JS_FreeValue(ctx, ret);

  // Cleanup
  JS_FreeValue(ctx, args[1]);
  fs_async_work_free(work);
}

// Step 3: Read the file data
static void readfile_read_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Read error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "read", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Update buffer size with actual bytes read
  work->buffer_size = (size_t)req->result;

  // Step 4: Close the file
  uv_fs_req_cleanup(req);
  uv_fs_close(loop, req, work->flags, readfile_close_cb);  // flags holds fd
}

// Step 2: Get file size via fstat
static void readfile_fstat_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Stat error - close fd and report error
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->flags, NULL);  // Sync close
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "fstat", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Get file size
  uv_stat_t* statbuf = uv_fs_get_statbuf(req);
  size_t file_size = (size_t)statbuf->st_size;

  // Allocate buffer for file data
  work->buffer = malloc(file_size + 1);  // +1 for null terminator (text files)
  work->buffer_size = file_size;
  work->owns_buffer = 1;  // We allocated this buffer, must free it

  if (!work->buffer) {
    // Allocation failed - close fd and report error
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->flags, NULL);
    uv_fs_req_cleanup(&close_req);

    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to allocate buffer"));
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Null-terminate for text files
  ((char*)work->buffer)[file_size] = '\0';

  // Step 3: Read file data
  uv_buf_t iov = uv_buf_init(work->buffer, (unsigned int)file_size);
  uv_fs_req_cleanup(req);
  uv_fs_read(loop, req, work->flags, &iov, 1, 0, readfile_read_cb);  // flags holds fd
}

// Step 1: Open the file
static void readfile_open_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Open error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "open", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Store fd in flags field (reuse field)
  work->flags = (int)req->result;

  // Step 2: Get file size
  uv_fs_req_cleanup(req);
  uv_fs_fstat(loop, req, work->flags, readfile_fstat_cb);
}

// fs.readFile(path, callback) - True async with libuv
JSValue js_fs_read_file_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "readFile requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[1])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize work request
  work->callback = JS_DupValue(ctx, argv[1]);
  work->path = strdup(path);
  work->path2 = NULL;
  work->buffer = NULL;
  work->buffer_size = 0;
  work->flags = 0;
  work->mode = 0;
  work->offset = 0;

  JS_FreeCString(ctx, path);

  // Start async operation: Step 1 - open file
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_open(loop, &work->req, work->path, O_RDONLY, 0, readfile_open_cb);

  if (result < 0) {
    // Immediate error (e.g., invalid arguments)
    JSValue error = create_fs_error(ctx, -result, "open", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// writeFile: Multi-step async operation (open → write → close)
// ============================================================================

// Step 3: Close after write
static void writefile_close_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  // Write completed successfully, report success
  JSValue args[1] = {JS_NULL};
  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
  JS_FreeValue(ctx, ret);

  fs_async_work_free(work);
}

// Step 2: Write data to file
static void writefile_write_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Write error - close fd and report
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->flags, NULL);
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "write", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Step 3: Close file
  uv_fs_req_cleanup(req);
  uv_fs_close(loop, req, work->flags, writefile_close_cb);
}

// Step 1: Open file for writing
static void writefile_open_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Open error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "open", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Store fd
  work->flags = (int)req->result;

  // Step 2: Write data
  uv_buf_t iov = uv_buf_init(work->buffer, (unsigned int)work->buffer_size);
  uv_fs_req_cleanup(req);
  uv_fs_write(loop, req, work->flags, &iov, 1, 0, writefile_write_cb);
}

// fs.writeFile(path, data, callback) - True async with libuv
JSValue js_fs_write_file_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "writeFile requires path, data, and callback");
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
      JS_GetException(ctx);
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

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    free(data);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize work request
  work->callback = JS_DupValue(ctx, argv[2]);
  work->path = strdup(path);
  work->buffer = data;  // Transfer ownership
  work->buffer_size = data_len;
  work->owns_buffer = 1;  // We allocated this buffer, must free it
  work->flags = 0;

  JS_FreeCString(ctx, path);

  // Start async operation: Step 1 - open file
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_open(loop, &work->req, work->path, O_WRONLY | O_CREAT | O_TRUNC, 0644, writefile_open_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "open", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// Simple async operations using libuv fs API
// ============================================================================

// fs.unlink(path, callback) - True async
JSValue js_fs_unlink_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "unlink requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[1])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[1]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_unlink(loop, &work->req, work->path, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "unlink", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.mkdir(path, mode, callback) - True async
JSValue js_fs_mkdir_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "mkdir requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int mode = 0777;  // Default mode
  JSValue callback = argv[1];

  if (argc >= 3) {
    // mkdir(path, mode, callback)
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

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_mkdir(loop, &work->req, work->path, mode, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "mkdir", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.rmdir(path, callback) - True async
JSValue js_fs_rmdir_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "rmdir requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[1])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[1]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_rmdir(loop, &work->req, work->path, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "rmdir", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.rename(oldPath, newPath, callback) - True async
JSValue js_fs_rename_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "rename requires oldPath, newPath, and callback");
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

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, old_path);
    JS_FreeCString(ctx, new_path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[2]);
  work->path = strdup(old_path);
  work->path2 = strdup(new_path);
  JS_FreeCString(ctx, old_path);
  JS_FreeCString(ctx, new_path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_rename(loop, &work->req, work->path, work->path2, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "rename", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.access(path, mode, callback) - True async
JSValue js_fs_access_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "access requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int mode = F_OK;
  JSValue callback = argv[1];

  if (argc >= 3) {
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

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(path);
  work->mode = mode;
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_access(loop, &work->req, work->path, work->mode, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "access", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// Stat operations (Task 2.6)
// ============================================================================

// fs.stat(path, callback) - True async
JSValue js_fs_stat_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "stat requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[1])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[1]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_stat(loop, &work->req, work->path, fs_async_complete_stat);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "stat", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.lstat(path, callback) - True async
JSValue js_fs_lstat_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "lstat requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[1])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[1]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_lstat(loop, &work->req, work->path, fs_async_complete_stat);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "lstat", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.fstat(fd, callback) - True async
JSValue js_fs_fstat_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "fstat requires fd and callback");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "fd must be a number");
  }

  if (!JS_IsFunction(ctx, argv[1])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[1]);
  work->path = NULL;  // fstat doesn't use path

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fstat(loop, &work->req, fd, fs_async_complete_stat);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "fstat", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// Chmod operations (Task 2.7)
// ============================================================================

// fs.chmod(path, mode, callback) - True async
JSValue js_fs_chmod_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "chmod requires path, mode, and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "mode must be a number");
  }

  if (!JS_IsFunction(ctx, argv[2])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[2]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_chmod(loop, &work->req, work->path, mode, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "chmod", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.fchmod(fd, mode, callback) - True async
JSValue js_fs_fchmod_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "fchmod requires fd, mode, and callback");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "fd must be a number");
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, argv[1]) < 0) {
    return JS_ThrowTypeError(ctx, "mode must be a number");
  }

  if (!JS_IsFunction(ctx, argv[2])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[2]);
  work->path = NULL;

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fchmod(loop, &work->req, fd, mode, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "fchmod", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.lchmod(path, mode, callback) - True async (Unix only)
JSValue js_fs_lchmod_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  return JS_ThrowTypeError(ctx, "lchmod is not supported on Windows");
#else
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "lchmod requires path, mode, and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t mode;
  if (JS_ToInt32(ctx, &mode, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "mode must be a number");
  }

  if (!JS_IsFunction(ctx, argv[2])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[2]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Note: lchmod is not supported by libuv and many platforms don't support it
  // We throw an error instead of silently using chmod (which follows symlinks)
  JSValue error = JS_NewError(ctx);
  JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, "lchmod is not implemented"),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  JS_DefinePropertyValueStr(ctx, error, "code", JS_NewString(ctx, "ERR_METHOD_NOT_IMPLEMENTED"),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  JSValue args[1] = {error};
  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, error);
  fs_async_work_free(work);

  return JS_UNDEFINED;
#endif
}

// ============================================================================
// Chown operations (Task 2.8)
// ============================================================================

// fs.chown(path, uid, gid, callback) - True async
JSValue js_fs_chown_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  return JS_ThrowTypeError(ctx, "chown is not supported on Windows");
#else
  if (argc < 4) {
    return JS_ThrowTypeError(ctx, "chown requires path, uid, gid, and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t uid, gid;
  if (JS_ToInt32(ctx, &uid, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "uid must be a number");
  }
  if (JS_ToInt32(ctx, &gid, argv[2]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "gid must be a number");
  }

  if (!JS_IsFunction(ctx, argv[3])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[3]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_chown(loop, &work->req, work->path, (uv_uid_t)uid, (uv_gid_t)gid, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "chown", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
#endif
}

// fs.fchown(fd, uid, gid, callback) - True async
JSValue js_fs_fchown_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  return JS_ThrowTypeError(ctx, "fchown is not supported on Windows");
#else
  if (argc < 4) {
    return JS_ThrowTypeError(ctx, "fchown requires fd, uid, gid, and callback");
  }

  int32_t fd, uid, gid;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "fd must be a number");
  }
  if (JS_ToInt32(ctx, &uid, argv[1]) < 0) {
    return JS_ThrowTypeError(ctx, "uid must be a number");
  }
  if (JS_ToInt32(ctx, &gid, argv[2]) < 0) {
    return JS_ThrowTypeError(ctx, "gid must be a number");
  }

  if (!JS_IsFunction(ctx, argv[3])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[3]);
  work->path = NULL;

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fchown(loop, &work->req, fd, (uv_uid_t)uid, (uv_gid_t)gid, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "fchown", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
#endif
}

// fs.lchown(path, uid, gid, callback) - True async
JSValue js_fs_lchown_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
#ifdef _WIN32
  return JS_ThrowTypeError(ctx, "lchown is not supported on Windows");
#else
  if (argc < 4) {
    return JS_ThrowTypeError(ctx, "lchown requires path, uid, gid, and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int32_t uid, gid;
  if (JS_ToInt32(ctx, &uid, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "uid must be a number");
  }
  if (JS_ToInt32(ctx, &gid, argv[2]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "gid must be a number");
  }

  if (!JS_IsFunction(ctx, argv[3])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[3]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_lchown(loop, &work->req, work->path, (uv_uid_t)uid, (uv_gid_t)gid, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "lchown", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
#endif
}

// ============================================================================
// Utime operations (Task 2.9)
// ============================================================================

// fs.utimes(path, atime, mtime, callback) - True async
JSValue js_fs_utimes_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 4) {
    return JS_ThrowTypeError(ctx, "utimes requires path, atime, mtime, and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  double atime, mtime;
  if (JS_ToFloat64(ctx, &atime, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "atime must be a number");
  }
  if (JS_ToFloat64(ctx, &mtime, argv[2]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "mtime must be a number");
  }

  if (!JS_IsFunction(ctx, argv[3])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[3]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_utime(loop, &work->req, work->path, atime, mtime, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "utimes", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.futimes(fd, atime, mtime, callback) - True async
JSValue js_fs_futimes_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 4) {
    return JS_ThrowTypeError(ctx, "futimes requires fd, atime, mtime, and callback");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "fd must be a number");
  }

  double atime, mtime;
  if (JS_ToFloat64(ctx, &atime, argv[1]) < 0) {
    return JS_ThrowTypeError(ctx, "atime must be a number");
  }
  if (JS_ToFloat64(ctx, &mtime, argv[2]) < 0) {
    return JS_ThrowTypeError(ctx, "mtime must be a number");
  }

  if (!JS_IsFunction(ctx, argv[3])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[3]);
  work->path = NULL;

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_futime(loop, &work->req, fd, atime, mtime, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "futimes", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.lutimes(path, atime, mtime, callback) - True async
JSValue js_fs_lutimes_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 4) {
    return JS_ThrowTypeError(ctx, "lutimes requires path, atime, mtime, and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  double atime, mtime;
  if (JS_ToFloat64(ctx, &atime, argv[1]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "atime must be a number");
  }
  if (JS_ToFloat64(ctx, &mtime, argv[2]) < 0) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "mtime must be a number");
  }

  if (!JS_IsFunction(ctx, argv[3])) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[3]);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_lutime(loop, &work->req, work->path, atime, mtime, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "lutimes", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// Link operations (Task 2.16)
// ============================================================================

// fs.link(existingPath, newPath, callback) - True async
JSValue js_fs_link_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "link requires existingPath, newPath, and callback");
  }

  const char* existing_path = JS_ToCString(ctx, argv[0]);
  if (!existing_path) {
    return JS_EXCEPTION;
  }

  const char* new_path = JS_ToCString(ctx, argv[1]);
  if (!new_path) {
    JS_FreeCString(ctx, existing_path);
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, argv[2])) {
    JS_FreeCString(ctx, existing_path);
    JS_FreeCString(ctx, new_path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, existing_path);
    JS_FreeCString(ctx, new_path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[2]);
  work->path = strdup(existing_path);
  work->path2 = strdup(new_path);
  JS_FreeCString(ctx, existing_path);
  JS_FreeCString(ctx, new_path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_link(loop, &work->req, work->path, work->path2, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "link", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.symlink(target, path, [type,] callback) - True async
JSValue js_fs_symlink_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "symlink requires target, path, and callback");
  }

  const char* target = JS_ToCString(ctx, argv[0]);
  if (!target) {
    return JS_EXCEPTION;
  }

  const char* path = JS_ToCString(ctx, argv[1]);
  if (!path) {
    JS_FreeCString(ctx, target);
    return JS_EXCEPTION;
  }

  int flags = 0;  // Default type
  JSValue callback = argv[2];

  if (argc >= 4) {
    // symlink(target, path, type, callback)
    if (JS_IsString(argv[2])) {
      const char* type = JS_ToCString(ctx, argv[2]);
      if (type) {
        if (strcmp(type, "dir") == 0) {
          flags = UV_FS_SYMLINK_DIR;
        } else if (strcmp(type, "junction") == 0) {
          flags = UV_FS_SYMLINK_JUNCTION;
        }
        JS_FreeCString(ctx, type);
      }
    }
    callback = argv[3];
  }

  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, target);
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, target);
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(target);
  work->path2 = strdup(path);
  JS_FreeCString(ctx, target);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_symlink(loop, &work->req, work->path, work->path2, flags, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "symlink", work->path2);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.readlink(path, [options,] callback) - True async
JSValue js_fs_readlink_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "readlink requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (argc >= 3) {
    // Skip options if provided
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_readlink(loop, &work->req, work->path, fs_async_complete_string);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "readlink", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.realpath(path, [options,] callback) - True async
JSValue js_fs_realpath_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "realpath requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (argc >= 3) {
    // Skip options if provided
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_realpath(loop, &work->req, work->path, fs_async_complete_string);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "realpath", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// Open/Close operations (Task 2.14)
// ============================================================================

// fs.open(path, flags, [mode,] callback) - True async
JSValue js_fs_open_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "open requires path, flags, and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Parse flags
  int flags = 0;
  if (JS_IsString(argv[1])) {
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
  } else if (JS_IsNumber(argv[1])) {
    int32_t flags_int;
    JS_ToInt32(ctx, &flags_int, argv[1]);
    flags = flags_int;
  }

  int mode = 0666;
  JSValue callback = argv[2];

  if (argc >= 4) {
    // open(path, flags, mode, callback)
    if (JS_IsNumber(argv[2])) {
      int32_t mode_int;
      JS_ToInt32(ctx, &mode_int, argv[2]);
      mode = mode_int;
    }
    callback = argv[3];
  }

  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_open(loop, &work->req, work->path, flags, mode, fs_async_complete_fd);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "open", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// fs.close(fd, callback) - True async
JSValue js_fs_close_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "close requires fd and callback");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "fd must be a number");
  }

  if (!JS_IsFunction(ctx, argv[1])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, argv[1]);
  work->path = NULL;

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_close(loop, &work->req, fd, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "close", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// Readdir operation (Task 2.11)
// ============================================================================

// fs.readdir(path, [options,] callback) - True async
JSValue js_fs_readdir_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "readdir requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (argc >= 3) {
    // Skip options if provided
    callback = argv[2];
  }

  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_scandir(loop, &work->req, work->path, 0, fs_async_complete_readdir);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "readdir", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// appendFile: Multi-step async operation (open → write → close) (Task 2.15)
// ============================================================================

// Step 3: Close after append
static void appendfile_close_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  // Append completed successfully, report success
  JSValue args[1] = {JS_NULL};
  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
  JS_FreeValue(ctx, ret);

  fs_async_work_free(work);
}

// Step 2: Write data to file
static void appendfile_write_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Write error - close fd and report
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->flags, NULL);
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "write", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Step 3: Close file
  uv_fs_req_cleanup(req);
  uv_fs_close(loop, req, work->flags, appendfile_close_cb);
}

// Step 1: Open file for appending
static void appendfile_open_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Open error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "open", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Store fd
  work->flags = (int)req->result;

  // Step 2: Write data (append at end)
  uv_buf_t iov = uv_buf_init(work->buffer, (unsigned int)work->buffer_size);
  uv_fs_req_cleanup(req);
  uv_fs_write(loop, req, work->flags, &iov, 1, -1, appendfile_write_cb);  // -1 = append at end
}

// fs.appendFile(path, data, callback) - True async with libuv
JSValue js_fs_append_file_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "appendFile requires path, data, and callback");
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
      JS_GetException(ctx);
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

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    free(data);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize work request
  work->callback = JS_DupValue(ctx, argv[2]);
  work->path = strdup(path);
  work->buffer = data;  // Transfer ownership
  work->buffer_size = data_len;
  work->owns_buffer = 1;  // We allocated this buffer, must free it
  work->flags = 0;

  JS_FreeCString(ctx, path);

  // Start async operation: Step 1 - open file for appending
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_open(loop, &work->req, work->path, O_WRONLY | O_CREAT | O_APPEND, 0644, appendfile_open_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "open", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// Buffer I/O operations (Task 2.10): fs.read, fs.write
// ============================================================================

// Completion callback for fs.read()
static void fs_read_cb(uv_fs_t* req) {
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
    fs_async_work_free(work);
    return;
  }

  // Success: callback(null, bytesRead, buffer)
  // Data was already read into user's buffer by uv_fs_read
  // Just return bytes read and the user's buffer (same buffer object)
  size_t bytes_read = (size_t)req->result;

  JSValue args[3];
  args[0] = JS_NULL;                              // err = null
  args[1] = JS_NewInt64(ctx, bytes_read);         // bytesRead
  args[2] = JS_DupValue(ctx, work->user_buffer);  // Return same buffer object

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 3, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, args[2]);

  fs_async_work_free(work);
}

// fs.read(fd, buffer, offset, length, position, callback) - True async
JSValue js_fs_read_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 6) {
    return JS_ThrowTypeError(ctx, "read requires fd, buffer, offset, length, position, and callback");
  }

  // Parse fd
  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "fd must be a number");
  }

  // Parse buffer (supports Buffer/TypedArray/ArrayBuffer)
  size_t buffer_size;
  uint8_t* buffer_data = NULL;
  size_t byte_offset = 0;

  // First try to get as TypedArray (for Buffer objects)
  JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[1], &byte_offset, &buffer_size, NULL);
  if (!JS_IsException(array_buffer)) {
    size_t ab_size;
    buffer_data = JS_GetArrayBuffer(ctx, &ab_size, array_buffer);
    JS_FreeValue(ctx, array_buffer);
    if (buffer_data) {
      buffer_data += byte_offset;
    }
  } else {
    // Fallback: try to get as ArrayBuffer directly
    buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, argv[1]);
  }

  if (!buffer_data) {
    return JS_ThrowTypeError(ctx, "buffer must be a Buffer, TypedArray, or ArrayBuffer");
  }

  // Parse offset
  int64_t offset;
  if (JS_ToInt64(ctx, &offset, argv[2]) < 0) {
    return JS_ThrowTypeError(ctx, "offset must be a number");
  }

  // Parse length
  int64_t length;
  if (JS_ToInt64(ctx, &length, argv[3]) < 0) {
    return JS_ThrowTypeError(ctx, "length must be a number");
  }

  // Parse position (can be null for current position)
  int64_t position = -1;  // -1 means current position
  if (!JS_IsNull(argv[4])) {
    if (JS_ToInt64(ctx, &position, argv[4]) < 0) {
      return JS_ThrowTypeError(ctx, "position must be a number or null");
    }
  }

  // Parse callback
  if (!JS_IsFunction(ctx, argv[5])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Validate offset/length
  if (offset < 0 || length < 0 || offset + length > (int64_t)buffer_size) {
    return JS_ThrowRangeError(ctx, "Invalid offset/length for buffer");
  }

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Store user's buffer reference (must DupValue to preserve it)
  work->callback = JS_DupValue(ctx, argv[5]);
  work->user_buffer = JS_DupValue(ctx, argv[1]);  // Preserve user's buffer
  work->path = NULL;                              // read doesn't use path
  work->buffer = (void*)(buffer_data + offset);   // Point to user's buffer at offset
  work->buffer_size = length;
  work->buffer_offset = offset;
  work->offset = position;
  work->owns_buffer = 0;  // Buffer points to user's buffer, don't free it

  // Queue async read operation - read directly into user's buffer
  uv_buf_t iov = uv_buf_init((char*)work->buffer, (unsigned int)length);
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_read(loop, &work->req, fd, &iov, 1, position, fs_read_cb);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "read", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// Completion callback for fs.write()
static void fs_write_cb(uv_fs_t* req) {
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
    fs_async_work_free(work);
    return;
  }

  // Success: callback(null, bytesWritten, buffer)
  // Return user's buffer (same buffer object that was passed in)
  size_t bytes_written = (size_t)req->result;

  JSValue args[3];
  args[0] = JS_NULL;                              // err = null
  args[1] = JS_NewInt64(ctx, bytes_written);      // bytesWritten
  args[2] = JS_DupValue(ctx, work->user_buffer);  // Return same buffer object

  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 3, args);
  JS_FreeValue(ctx, ret);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, args[2]);

  fs_async_work_free(work);
}

// fs.write(fd, buffer, offset, length, position, callback) - True async
JSValue js_fs_write_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 6) {
    return JS_ThrowTypeError(ctx, "write requires fd, buffer, offset, length, position, and callback");
  }

  // Parse fd
  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "fd must be a number");
  }

  // Parse buffer (supports Buffer/TypedArray/ArrayBuffer)
  size_t buffer_size;
  uint8_t* buffer_data = NULL;
  size_t byte_offset = 0;

  // First try to get as TypedArray (for Buffer objects)
  JSValue array_buffer = JS_GetTypedArrayBuffer(ctx, argv[1], &byte_offset, &buffer_size, NULL);
  if (!JS_IsException(array_buffer)) {
    size_t ab_size;
    buffer_data = JS_GetArrayBuffer(ctx, &ab_size, array_buffer);
    JS_FreeValue(ctx, array_buffer);
    if (buffer_data) {
      buffer_data += byte_offset;
    }
  } else {
    // Fallback: try to get as ArrayBuffer directly
    buffer_data = JS_GetArrayBuffer(ctx, &buffer_size, argv[1]);
  }

  if (!buffer_data) {
    return JS_ThrowTypeError(ctx, "buffer must be a Buffer, TypedArray, or ArrayBuffer");
  }

  // Parse offset
  int64_t offset;
  if (JS_ToInt64(ctx, &offset, argv[2]) < 0) {
    return JS_ThrowTypeError(ctx, "offset must be a number");
  }

  // Parse length
  int64_t length;
  if (JS_ToInt64(ctx, &length, argv[3]) < 0) {
    return JS_ThrowTypeError(ctx, "length must be a number");
  }

  // Parse position (can be null for current position)
  int64_t position = -1;  // -1 means current position
  if (!JS_IsNull(argv[4])) {
    if (JS_ToInt64(ctx, &position, argv[4]) < 0) {
      return JS_ThrowTypeError(ctx, "position must be a number or null");
    }
  }

  // Parse callback
  if (!JS_IsFunction(ctx, argv[5])) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Validate offset/length
  if (offset < 0 || length < 0 || offset + length > (int64_t)buffer_size) {
    return JS_ThrowRangeError(ctx, "Invalid offset/length for buffer");
  }

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Store user's buffer reference and write directly from it
  work->callback = JS_DupValue(ctx, argv[5]);
  work->user_buffer = JS_DupValue(ctx, argv[1]);  // Preserve user's buffer
  work->path = NULL;                              // write doesn't use path
  work->buffer = (void*)(buffer_data + offset);   // Point to user's buffer at offset
  work->buffer_size = length;
  work->buffer_offset = offset;
  work->offset = position;
  work->owns_buffer = 0;  // Buffer points to user's buffer, don't free it

  // Queue async write operation - write directly from user's buffer
  uv_buf_t iov = uv_buf_init((char*)work->buffer, (unsigned int)length);
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_write(loop, &work->req, fd, &iov, 1, position, fs_write_cb);

  if (result < 0) {
    // Immediate error
    JSValue error = create_fs_error(ctx, -result, "write", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// copyFile: Multi-step async operation (open src → open dest → fstat → read → write → close both) (Task 2.15)
// ============================================================================

// Forward declarations for copyFile callbacks
static void copyfile_read_cb(uv_fs_t* req);

// Completion callback - closes both fds
static void copyfile_cleanup_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;

  // Copy completed successfully
  JSValue args[1] = {JS_NULL};
  JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
  JS_FreeValue(ctx, ret);

  fs_async_work_free(work);
}

// Step 6: Close destination fd
static void copyfile_close_dest_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  uv_loop_t* loop = fs_get_uv_loop(work->ctx);

  // Now close source fd (stored in mode field)
  uv_fs_req_cleanup(req);
  uv_fs_close(loop, req, work->mode, copyfile_cleanup_cb);
}

// Step 5: After write, close dest fd
static void copyfile_write_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Write error - close both fds
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->flags, NULL);  // dest fd
    uv_fs_req_cleanup(&close_req);
    uv_fs_close(loop, &close_req, work->mode, NULL);  // src fd
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "write", work->path2);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Read next chunk from source (continue loop)
  uv_buf_t iov = uv_buf_init(work->buffer, 8192);  // 8KB chunks
  uv_fs_req_cleanup(req);
  uv_fs_read(loop, req, work->mode, &iov, 1, work->offset, copyfile_read_cb);
}

// Step 4: Read chunk from source
static void copyfile_read_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Read error - close both fds
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->flags, NULL);  // dest fd
    uv_fs_req_cleanup(&close_req);
    uv_fs_close(loop, &close_req, work->mode, NULL);  // src fd
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "read", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  if (req->result == 0) {
    // EOF - all data copied, close destination fd
    uv_fs_req_cleanup(req);
    uv_fs_close(loop, req, work->flags, copyfile_close_dest_cb);
    return;
  }

  // Write the chunk to destination
  size_t bytes_read = (size_t)req->result;
  work->offset += bytes_read;  // Track position
  uv_buf_t iov = uv_buf_init(work->buffer, (unsigned int)bytes_read);
  uv_fs_req_cleanup(req);
  uv_fs_write(loop, req, work->flags, &iov, 1, -1, copyfile_write_cb);  // -1 = append
}

// Step 3: After fstat, start reading
static void copyfile_fstat_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Stat error - close both fds
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->flags, NULL);  // dest fd
    uv_fs_req_cleanup(&close_req);
    uv_fs_close(loop, &close_req, work->mode, NULL);  // src fd
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "fstat", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Get file size
  uv_stat_t* statbuf = uv_fs_get_statbuf(req);
  work->buffer_size = (size_t)statbuf->st_size;
  work->offset = 0;  // Start at beginning

  // Allocate buffer for copying (8KB chunks)
  work->buffer = malloc(8192);
  work->owns_buffer = 1;  // We allocated this buffer, must free it
  if (!work->buffer) {
    // Allocation failed - close both fds
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->flags, NULL);
    uv_fs_req_cleanup(&close_req);
    uv_fs_close(loop, &close_req, work->mode, NULL);
    uv_fs_req_cleanup(&close_req);

    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to allocate buffer"));
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Start reading from source file
  uv_buf_t iov = uv_buf_init(work->buffer, 8192);
  uv_fs_req_cleanup(req);
  uv_fs_read(loop, req, work->mode, &iov, 1, 0, copyfile_read_cb);  // Read from offset 0
}

// Step 2: Open destination file
static void copyfile_open_dest_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Dest open error - close source fd
    int err = -req->result;
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, work->mode, NULL);  // mode holds src fd
    uv_fs_req_cleanup(&close_req);

    JSValue error = create_fs_error(ctx, err, "open", work->path2);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Store dest fd in flags field
  work->flags = (int)req->result;

  // Step 3: Get source file size
  uv_fs_req_cleanup(req);
  uv_fs_fstat(loop, req, work->mode, copyfile_fstat_cb);  // mode holds src fd
}

// Step 1: Open source file
static void copyfile_open_src_cb(uv_fs_t* req) {
  fs_async_work_t* work = (fs_async_work_t*)req;
  JSContext* ctx = work->ctx;
  uv_loop_t* loop = fs_get_uv_loop(ctx);

  if (req->result < 0) {
    // Source open error
    int err = -req->result;
    JSValue error = create_fs_error(ctx, err, "open", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return;
  }

  // Store source fd in mode field (reuse)
  work->mode = (int)req->result;

  // Step 2: Open destination file
  uv_fs_req_cleanup(req);
  uv_fs_open(loop, req, work->path2, O_WRONLY | O_CREAT | O_TRUNC, 0644, copyfile_open_dest_cb);
}

// fs.copyFile(src, dest, callback) - True async with libuv
JSValue js_fs_copy_file_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 3) {
    return JS_ThrowTypeError(ctx, "copyFile requires src, dest, and callback");
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

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, src);
    JS_FreeCString(ctx, dest);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize work request
  work->callback = JS_DupValue(ctx, argv[2]);
  work->path = strdup(src);    // source path
  work->path2 = strdup(dest);  // destination path
  work->buffer = NULL;         // Will be allocated after fstat
  work->buffer_size = 0;
  work->flags = 0;  // Will hold dest fd
  work->mode = 0;   // Will hold src fd
  work->offset = 0;

  JS_FreeCString(ctx, src);
  JS_FreeCString(ctx, dest);

  // Start async operation: Step 1 - open source file
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_open(loop, &work->req, work->path, O_RDONLY, 0, copyfile_open_src_cb);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "open", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// truncate/ftruncate: Truncate file to specified length
// ============================================================================

JSValue js_fs_truncate_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "truncate requires at least path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Parse length (optional, defaults to 0)
  int64_t length = 0;
  if (argc >= 3 && !JS_IsUndefined(argv[1]) && !JS_IsFunction(ctx, argv[1])) {
    if (JS_ToInt64(ctx, &length, argv[1])) {
      JS_FreeCString(ctx, path);
      return JS_EXCEPTION;
    }
  }

  // Get callback
  JSValue callback = (argc >= 3 && !JS_IsFunction(ctx, argv[1])) ? argv[2] : argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Use ftruncate with open-truncate-close pattern
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_open(loop, &work->req, work->path, O_WRONLY, 0, NULL);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "truncate", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
    return JS_UNDEFINED;
  }

  int fd = (int)result;
  uv_fs_req_cleanup(&work->req);

  // Now truncate
  result = uv_fs_ftruncate(loop, &work->req, fd, length, fs_async_complete_void);

  // Close fd after truncate
  uv_fs_t close_req;
  uv_fs_close(loop, &close_req, fd, NULL);
  uv_fs_req_cleanup(&close_req);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "truncate", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

JSValue js_fs_ftruncate_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "ftruncate requires at least fd and callback");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0])) {
    return JS_EXCEPTION;
  }

  // Parse length (optional, defaults to 0)
  int64_t length = 0;
  if (argc >= 3 && !JS_IsUndefined(argv[1]) && !JS_IsFunction(ctx, argv[1])) {
    if (JS_ToInt64(ctx, &length, argv[1])) {
      return JS_EXCEPTION;
    }
  }

  // Get callback
  JSValue callback = (argc >= 3 && !JS_IsFunction(ctx, argv[1])) ? argv[2] : argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);

  // Queue async ftruncate operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_ftruncate(loop, &work->req, fd, length, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "ftruncate", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// fsync/fdatasync: Synchronize file data to disk
// ============================================================================

JSValue js_fs_fsync_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "fsync requires fd and callback");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0])) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);

  // Queue async fsync operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fsync(loop, &work->req, fd, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "fsync", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

JSValue js_fs_fdatasync_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "fdatasync requires fd and callback");
  }

  int32_t fd;
  if (JS_ToInt32(ctx, &fd, argv[0])) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);

  // Queue async fdatasync operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_fdatasync(loop, &work->req, fd, fs_async_complete_void);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "fdatasync", NULL);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// mkdtemp: Create temporary directory
// ============================================================================

JSValue js_fs_mkdtemp_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "mkdtemp requires prefix and callback");
  }

  const char* prefix = JS_ToCString(ctx, argv[0]);
  if (!prefix) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, prefix);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, prefix);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(prefix);
  JS_FreeCString(ctx, prefix);

  // Queue async mkdtemp operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_mkdtemp(loop, &work->req, work->path, fs_async_complete_string);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "mkdtemp", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// statfs: Get filesystem statistics
// ============================================================================

JSValue js_fs_statfs_async(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "statfs requires path and callback");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "callback must be a function");
  }

  // Allocate work request
  fs_async_work_t* work = fs_async_work_new(ctx);
  if (!work) {
    JS_FreeCString(ctx, path);
    return JS_ThrowOutOfMemory(ctx);
  }

  work->callback = JS_DupValue(ctx, callback);
  work->path = strdup(path);
  JS_FreeCString(ctx, path);

  // Queue async statfs operation
  uv_loop_t* loop = fs_get_uv_loop(ctx);
  int result = uv_fs_statfs(loop, &work->req, work->path, fs_async_complete_statfs);

  if (result < 0) {
    JSValue error = create_fs_error(ctx, -result, "statfs", work->path);
    JSValue args[1] = {error};
    JSValue ret = JS_Call(ctx, work->callback, JS_UNDEFINED, 1, args);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, error);
    fs_async_work_free(work);
  }

  return JS_UNDEFINED;
}
