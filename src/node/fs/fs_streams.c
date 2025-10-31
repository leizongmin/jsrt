/**
 * fs stream implementation - createReadStream and createWriteStream
 *
 * This module provides streaming file I/O compatible with Node.js fs.createReadStream()
 * and fs.createWriteStream() APIs, with full pipe() support and backpressure handling.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../stream/stream_internal.h"
#include "fs_common.h"

// Forward declaration
extern void js_std_dump_error(JSContext* ctx);

// ReadStream context - stores file descriptor and read state
typedef struct {
  int fd;
  bool auto_close;
  bool closed;
  char* path;
  uint64_t bytes_read;
  uint64_t start;
  uint64_t end;
  uint64_t pos;
} FSReadStreamContext;

// WriteStream context - stores file descriptor and write state
typedef struct {
  int fd;
  bool auto_close;
  bool closed;
  char* path;
  uint64_t bytes_written;
  uint64_t start;
  uint64_t pos;
} FSWriteStreamContext;

// Free FSReadStreamContext
static void free_read_context(FSReadStreamContext* ctx) {
  if (!ctx)
    return;
  if (ctx->path) {
    free(ctx->path);
  }
  if (ctx->fd >= 0 && ctx->auto_close && !ctx->closed) {
    close(ctx->fd);
  }
  free(ctx);
}

// Free FSWriteStreamContext
static void free_write_context(FSWriteStreamContext* ctx) {
  if (!ctx)
    return;
  if (ctx->path) {
    free(ctx->path);
  }
  if (ctx->fd >= 0 && ctx->auto_close && !ctx->closed) {
    close(ctx->fd);
  }
  free(ctx);
}

// Read method for ReadStream - reads from file
static JSValue js_fs_read_stream_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Get stream data
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_readable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  // Get file context from stream's private data (stored as property)
  JSValue ctx_val = JS_GetPropertyStr(ctx, this_val, "__fs_context");
  if (JS_IsUndefined(ctx_val)) {
    return JS_NULL;
  }

  FSReadStreamContext* fs_ctx = JS_GetOpaque(ctx_val, 0);
  JS_FreeValue(ctx, ctx_val);

  if (!fs_ctx || fs_ctx->fd < 0 || fs_ctx->closed) {
    return JS_NULL;
  }

  // If we have buffered data, return it first
  if (stream->buffer_size > 0) {
    JSValue data = stream->buffered_data[0];
    // Shift buffer
    for (size_t i = 1; i < stream->buffer_size; i++) {
      stream->buffered_data[i - 1] = stream->buffered_data[i];
    }
    stream->buffer_size--;
    return data;
  }

  // Read from file
  size_t chunk_size = stream->options.highWaterMark > 0 ? stream->options.highWaterMark : 65536;

  // Check if we've reached the end position
  if (fs_ctx->end > 0 && fs_ctx->pos >= fs_ctx->end) {
    // Close file if autoClose is true
    if (fs_ctx->auto_close && !fs_ctx->closed) {
      close(fs_ctx->fd);
      fs_ctx->closed = true;
    }
    stream->ended = true;
    stream_emit(ctx, this_val, "end", 0, NULL);
    return JS_NULL;
  }

  // Limit chunk size to not exceed end position
  if (fs_ctx->end > 0) {
    uint64_t remaining = fs_ctx->end - fs_ctx->pos;
    if (chunk_size > remaining) {
      chunk_size = remaining;
    }
  }

  uint8_t* buffer = malloc(chunk_size);
  if (!buffer) {
    return JS_ThrowOutOfMemory(ctx);
  }

  ssize_t bytes_read = read(fs_ctx->fd, buffer, chunk_size);

  if (bytes_read < 0) {
    // Error reading
    free(buffer);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, strerror(errno)));
    JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, errno));
    stream_emit(ctx, this_val, "error", 1, &error);
    JS_FreeValue(ctx, error);

    // Close file
    if (fs_ctx->auto_close && !fs_ctx->closed) {
      close(fs_ctx->fd);
      fs_ctx->closed = true;
    }
    return JS_NULL;
  }

  if (bytes_read == 0) {
    // EOF reached
    free(buffer);
    if (fs_ctx->auto_close && !fs_ctx->closed) {
      close(fs_ctx->fd);
      fs_ctx->closed = true;
    }
    stream->ended = true;
    stream_emit(ctx, this_val, "end", 0, NULL);
    return JS_NULL;
  }

  // Update position
  fs_ctx->pos += bytes_read;
  fs_ctx->bytes_read += bytes_read;

  // Create buffer value
  JSValue data = JS_NewArrayBufferCopy(ctx, buffer, bytes_read);
  free(buffer);

  return data;
}

// Write method for WriteStream - writes to file
static JSValue js_fs_write_stream_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write() requires at least 1 argument");
  }

  // Get stream data
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (stream->writable_ended) {
    return JS_ThrowTypeError(ctx, "write after end");
  }

  // Get file context
  JSValue ctx_val = JS_GetPropertyStr(ctx, this_val, "__fs_context");
  if (JS_IsUndefined(ctx_val)) {
    return JS_NewBool(ctx, false);
  }

  FSWriteStreamContext* fs_ctx = JS_GetOpaque(ctx_val, 0);
  JS_FreeValue(ctx, ctx_val);

  if (!fs_ctx || fs_ctx->fd < 0 || fs_ctx->closed) {
    return JS_NewBool(ctx, false);
  }

  // Convert chunk to buffer
  size_t size;
  uint8_t* data = NULL;
  bool is_buffer = false;

  // Try to get as ArrayBuffer first
  data = JS_GetArrayBuffer(ctx, &size, argv[0]);
  if (data) {
    is_buffer = true;
  } else {
    // If not a buffer, convert to string
    const char* str = JS_ToCString(ctx, argv[0]);
    if (!str) {
      return JS_EXCEPTION;
    }
    size = strlen(str);
    data = (uint8_t*)str;
    is_buffer = false;
  }

  // Write to file
  ssize_t bytes_written = write(fs_ctx->fd, data, size);

  // Free string if we converted
  if (!is_buffer) {
    JS_FreeCString(ctx, (const char*)data);
  }

  if (bytes_written < 0) {
    // Error writing
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, strerror(errno)));
    JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, errno));
    stream_emit(ctx, this_val, "error", 1, &error);
    JS_FreeValue(ctx, error);
    return JS_NewBool(ctx, false);
  }

  // Update position and bytes written
  fs_ctx->pos += bytes_written;
  fs_ctx->bytes_written += bytes_written;

  // Emit 'bytesWritten' update
  JS_SetPropertyStr(ctx, this_val, "bytesWritten", JS_NewInt64(ctx, fs_ctx->bytes_written));

  // Call callback if provided (3rd argument)
  if (argc >= 3 && JS_IsFunction(ctx, argv[2])) {
    JSValue result = JS_Call(ctx, argv[2], JS_UNDEFINED, 0, NULL);
    if (JS_IsException(result)) {
      js_std_dump_error(ctx);
    }
    JS_FreeValue(ctx, result);
  }

  // Return true (no backpressure for file writes)
  return JS_NewBool(ctx, true);
}

// End method for WriteStream
static JSValue js_fs_write_stream_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Get stream data
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (stream->writable_ended) {
    return JS_UNDEFINED;
  }

  // Write final chunk if provided
  if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    js_fs_write_stream_write(ctx, this_val, argc, argv);
  }

  // Get file context and close file
  JSValue ctx_val = JS_GetPropertyStr(ctx, this_val, "__fs_context");
  if (!JS_IsUndefined(ctx_val)) {
    FSWriteStreamContext* fs_ctx = JS_GetOpaque(ctx_val, 0);
    if (fs_ctx && fs_ctx->fd >= 0 && fs_ctx->auto_close && !fs_ctx->closed) {
      close(fs_ctx->fd);
      fs_ctx->closed = true;
    }
    JS_FreeValue(ctx, ctx_val);
  }

  stream->writable_ended = true;
  stream->writable_finished = true;

  // Emit 'finish' event
  stream_emit(ctx, this_val, "finish", 0, NULL);

  // Emit 'close' event if emitClose option is true
  if (stream->options.emitClose) {
    stream_emit(ctx, this_val, "close", 0, NULL);
  }

  return JS_UNDEFINED;
}

// fs.createReadStream(path[, options])
JSValue js_fs_create_read_stream(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "createReadStream() requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Parse options
  int flags = O_RDONLY;
  mode_t mode = 0666;
  bool auto_close = true;
  uint64_t start = 0;
  uint64_t end = 0;             // 0 means no limit
  int high_water_mark = 65536;  // Default 64KB

  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue flags_val = JS_GetPropertyStr(ctx, argv[1], "flags");
    if (!JS_IsUndefined(flags_val)) {
      const char* flags_str = JS_ToCString(ctx, flags_val);
      if (flags_str) {
        // Parse flags string (simplified)
        if (strcmp(flags_str, "r") == 0)
          flags = O_RDONLY;
        else if (strcmp(flags_str, "r+") == 0)
          flags = O_RDWR;
        JS_FreeCString(ctx, flags_str);
      }
    }
    JS_FreeValue(ctx, flags_val);

    JSValue mode_val = JS_GetPropertyStr(ctx, argv[1], "mode");
    if (!JS_IsUndefined(mode_val)) {
      int32_t mode_int;
      if (JS_ToInt32(ctx, &mode_int, mode_val) == 0) {
        mode = mode_int;
      }
    }
    JS_FreeValue(ctx, mode_val);

    JSValue auto_close_val = JS_GetPropertyStr(ctx, argv[1], "autoClose");
    if (JS_IsBool(auto_close_val)) {
      auto_close = JS_ToBool(ctx, auto_close_val);
    }
    JS_FreeValue(ctx, auto_close_val);

    JSValue start_val = JS_GetPropertyStr(ctx, argv[1], "start");
    if (!JS_IsUndefined(start_val)) {
      int64_t start_int;
      if (JS_ToInt64(ctx, &start_int, start_val) == 0) {
        start = start_int;
      }
    }
    JS_FreeValue(ctx, start_val);

    JSValue end_val = JS_GetPropertyStr(ctx, argv[1], "end");
    if (!JS_IsUndefined(end_val)) {
      int64_t end_int;
      if (JS_ToInt64(ctx, &end_int, end_val) == 0) {
        end = end_int + 1;  // end is inclusive in Node.js
      }
    }
    JS_FreeValue(ctx, end_val);

    JSValue hwm_val = JS_GetPropertyStr(ctx, argv[1], "highWaterMark");
    if (!JS_IsUndefined(hwm_val)) {
      int32_t hwm_int;
      if (JS_ToInt32(ctx, &hwm_int, hwm_val) == 0) {
        high_water_mark = hwm_int;
      }
    }
    JS_FreeValue(ctx, hwm_val);
  }

  // Open file
  int fd = open(path, flags, mode);
  if (fd < 0) {
    JS_FreeCString(ctx, path);
    return create_fs_error(ctx, errno, "open", path);
  }

  // Seek to start position if specified
  if (start > 0) {
    if (lseek(fd, start, SEEK_SET) < 0) {
      close(fd);
      JS_FreeCString(ctx, path);
      return create_fs_error(ctx, errno, "lseek", path);
    }
  }

  // Create Readable stream with options
  JSValue options = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, options, "highWaterMark", JS_NewInt32(ctx, high_water_mark));

  JSValue argv_stream[1] = {options};
  JSValue read_stream = js_readable_constructor(ctx, JS_UNDEFINED, 1, argv_stream);
  JS_FreeValue(ctx, options);

  if (JS_IsException(read_stream)) {
    close(fd);
    JS_FreeCString(ctx, path);
    return read_stream;
  }

  // Create and attach file context
  FSReadStreamContext* fs_ctx = calloc(1, sizeof(FSReadStreamContext));
  if (!fs_ctx) {
    close(fd);
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, read_stream);
    return JS_ThrowOutOfMemory(ctx);
  }

  fs_ctx->fd = fd;
  fs_ctx->auto_close = auto_close;
  fs_ctx->closed = false;
  fs_ctx->path = strdup(path);
  fs_ctx->bytes_read = 0;
  fs_ctx->start = start;
  fs_ctx->end = end;
  fs_ctx->pos = start;

  // Store context as property (will be freed by stream finalizer)
  JSValue ctx_obj = JS_NewObjectClass(ctx, 0);
  JS_SetOpaque(ctx_obj, fs_ctx);
  JS_SetPropertyStr(ctx, read_stream, "__fs_context", ctx_obj);

  // Override read method
  JS_SetPropertyStr(ctx, read_stream, "read", JS_NewCFunction(ctx, js_fs_read_stream_read, "read", 1));

  // Set additional properties
  JS_SetPropertyStr(ctx, read_stream, "path", JS_NewString(ctx, path));
  JS_SetPropertyStr(ctx, read_stream, "fd", JS_NewInt32(ctx, fd));
  JS_SetPropertyStr(ctx, read_stream, "bytesRead", JS_NewInt64(ctx, 0));
  JS_SetPropertyStr(ctx, read_stream, "pending", JS_NewBool(ctx, false));

  JS_FreeCString(ctx, path);
  return read_stream;
}

// fs.createWriteStream(path[, options])
JSValue js_fs_create_write_stream(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "createWriteStream() requires a path");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Parse options
  int flags = O_WRONLY | O_CREAT | O_TRUNC;
  mode_t mode = 0666;
  bool auto_close = true;
  uint64_t start = 0;

  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue flags_val = JS_GetPropertyStr(ctx, argv[1], "flags");
    if (!JS_IsUndefined(flags_val)) {
      const char* flags_str = JS_ToCString(ctx, flags_val);
      if (flags_str) {
        // Parse flags string
        if (strcmp(flags_str, "w") == 0)
          flags = O_WRONLY | O_CREAT | O_TRUNC;
        else if (strcmp(flags_str, "wx") == 0 || strcmp(flags_str, "xw") == 0)
          flags = O_WRONLY | O_CREAT | O_EXCL;
        else if (strcmp(flags_str, "w+") == 0)
          flags = O_RDWR | O_CREAT | O_TRUNC;
        else if (strcmp(flags_str, "wx+") == 0 || strcmp(flags_str, "xw+") == 0)
          flags = O_RDWR | O_CREAT | O_EXCL;
        else if (strcmp(flags_str, "a") == 0)
          flags = O_WRONLY | O_CREAT | O_APPEND;
        else if (strcmp(flags_str, "ax") == 0 || strcmp(flags_str, "xa") == 0)
          flags = O_WRONLY | O_CREAT | O_EXCL | O_APPEND;
        else if (strcmp(flags_str, "a+") == 0)
          flags = O_RDWR | O_CREAT | O_APPEND;
        else if (strcmp(flags_str, "ax+") == 0 || strcmp(flags_str, "xa+") == 0)
          flags = O_RDWR | O_CREAT | O_EXCL | O_APPEND;
        JS_FreeCString(ctx, flags_str);
      }
    }
    JS_FreeValue(ctx, flags_val);

    JSValue mode_val = JS_GetPropertyStr(ctx, argv[1], "mode");
    if (!JS_IsUndefined(mode_val)) {
      int32_t mode_int;
      if (JS_ToInt32(ctx, &mode_int, mode_val) == 0) {
        mode = mode_int;
      }
    }
    JS_FreeValue(ctx, mode_val);

    JSValue auto_close_val = JS_GetPropertyStr(ctx, argv[1], "autoClose");
    if (JS_IsBool(auto_close_val)) {
      auto_close = JS_ToBool(ctx, auto_close_val);
    }
    JS_FreeValue(ctx, auto_close_val);

    JSValue start_val = JS_GetPropertyStr(ctx, argv[1], "start");
    if (!JS_IsUndefined(start_val)) {
      int64_t start_int;
      if (JS_ToInt64(ctx, &start_int, start_val) == 0) {
        start = start_int;
      }
    }
    JS_FreeValue(ctx, start_val);
  }

  // Open file
  int fd = open(path, flags, mode);
  if (fd < 0) {
    JS_FreeCString(ctx, path);
    return create_fs_error(ctx, errno, "open", path);
  }

  // Seek to start position if specified
  if (start > 0) {
    if (lseek(fd, start, SEEK_SET) < 0) {
      close(fd);
      JS_FreeCString(ctx, path);
      return create_fs_error(ctx, errno, "lseek", path);
    }
  }

  // Create Writable stream
  JSValue write_stream = js_writable_constructor(ctx, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(write_stream)) {
    close(fd);
    JS_FreeCString(ctx, path);
    return write_stream;
  }

  // Create and attach file context
  FSWriteStreamContext* fs_ctx = calloc(1, sizeof(FSWriteStreamContext));
  if (!fs_ctx) {
    close(fd);
    JS_FreeCString(ctx, path);
    JS_FreeValue(ctx, write_stream);
    return JS_ThrowOutOfMemory(ctx);
  }

  fs_ctx->fd = fd;
  fs_ctx->auto_close = auto_close;
  fs_ctx->closed = false;
  fs_ctx->path = strdup(path);
  fs_ctx->bytes_written = 0;
  fs_ctx->start = start;
  fs_ctx->pos = start;

  // Store context as property
  JSValue ctx_obj = JS_NewObjectClass(ctx, 0);
  JS_SetOpaque(ctx_obj, fs_ctx);
  JS_SetPropertyStr(ctx, write_stream, "__fs_context", ctx_obj);

  // Override write and end methods
  JS_SetPropertyStr(ctx, write_stream, "write", JS_NewCFunction(ctx, js_fs_write_stream_write, "write", 3));
  JS_SetPropertyStr(ctx, write_stream, "end", JS_NewCFunction(ctx, js_fs_write_stream_end, "end", 3));

  // Set additional properties
  JS_SetPropertyStr(ctx, write_stream, "path", JS_NewString(ctx, path));
  JS_SetPropertyStr(ctx, write_stream, "fd", JS_NewInt32(ctx, fd));
  JS_SetPropertyStr(ctx, write_stream, "bytesWritten", JS_NewInt64(ctx, 0));
  JS_SetPropertyStr(ctx, write_stream, "pending", JS_NewBool(ctx, false));

  JS_FreeCString(ctx, path);
  return write_stream;
}
