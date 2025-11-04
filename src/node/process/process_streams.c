/**
 * Process stdio streams - Real stream wrappers for stdin/stdout/stderr
 *
 * This module provides true Node.js stream objects for process.stdin, stdout, and stderr
 * that support piping, events, and all standard stream methods.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../stream/stream_internal.h"
#include "process.h"

// Forward declarations
extern void js_std_dump_error(JSContext* ctx);

// EventEmitter wrapper functions from stream module
extern JSValue js_stream_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_stream_once(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_stream_emit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_stream_off(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_stream_remove_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_stream_add_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_stream_remove_all_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_stream_listener_count(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Readable stream methods from stream module
extern JSValue js_readable_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_readable_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_readable_is_paused(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_readable_set_encoding(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_readable_pipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_readable_unpipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
extern JSValue js_readable_push(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Read data from file descriptor (for stdin)
static JSValue js_stdin_stream_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_readable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  // If stream has ended and no buffered data, return null
  if (stream->ended && stream->buffer_size == 0) {
    return JS_NULL;
  }

  // If we have buffered data, return it
  if (stream->buffer_size > 0) {
    JSValue data = stream->buffered_data[0];

    // Shift buffer
    for (size_t i = 1; i < stream->buffer_size; i++) {
      stream->buffered_data[i - 1] = stream->buffered_data[i];
    }
    stream->buffer_size--;

    return data;
  }

  // Try to read from stdin
  // For now, return null (non-blocking behavior)
  // In a full implementation, this would use libuv to read asynchronously
  return JS_NULL;
}

// Internal _read callback for stdin
static JSValue js_stdin_internal_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // This would be called to trigger async reading
  // For now, it's a no-op
  return JS_UNDEFINED;
}

// Write data to file descriptor (for stdout/stderr)
static JSValue js_stdout_stream_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write() requires at least 1 argument");
  }

  if (stream->writable_ended) {
    return JS_ThrowTypeError(ctx, "write after end");
  }

  // Convert argument to string
  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_EXCEPTION;
  }

  // Write to stdout
  fputs(str, stdout);
  fflush(stdout);
  JS_FreeCString(ctx, str);

  // Call callback if provided (3rd argument)
  if (argc >= 3 && JS_IsFunction(ctx, argv[2])) {
    JSValue result = JS_Call(ctx, argv[2], JS_UNDEFINED, 0, NULL);
    if (JS_IsException(result)) {
      js_std_dump_error(ctx);
    }
    JS_FreeValue(ctx, result);
  }

  // Check if buffer is over high water mark (for backpressure)
  size_t buffer_size = stream->buffer_size;
  bool should_drain = buffer_size >= (size_t)stream->options.highWaterMark;

  // Return true if below high water mark, false if backpressure
  return JS_NewBool(ctx, !should_drain);
}

// Write data to stdin (for process.stdin write compatibility)
static JSValue js_stdin_stream_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write() requires at least 1 argument");
  }

  // Convert argument to string
  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_EXCEPTION;
  }

  // Write to stdin file descriptor (this is unusual but Node.js supports it)
  // Note: Writing to stdin is a no-op in most cases, but we handle it gracefully
  ssize_t result = write(STDIN_FILENO, str, strlen(str));
  JS_FreeCString(ctx, str);

  // Always return true for stdin (no backpressure handling)
  return JS_NewBool(ctx, true);
}

static JSValue js_stderr_stream_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write() requires at least 1 argument");
  }

  if (stream->writable_ended) {
    return JS_ThrowTypeError(ctx, "write after end");
  }

  // Convert argument to string
  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_EXCEPTION;
  }

  // Write to stderr
  fputs(str, stderr);
  fflush(stderr);
  JS_FreeCString(ctx, str);

  // Call callback if provided
  if (argc >= 3 && JS_IsFunction(ctx, argv[2])) {
    JSValue result = JS_Call(ctx, argv[2], JS_UNDEFINED, 0, NULL);
    if (JS_IsException(result)) {
      js_std_dump_error(ctx);
    }
    JS_FreeValue(ctx, result);
  }

  // Check if buffer is over high water mark
  size_t buffer_size = stream->buffer_size;
  bool should_drain = buffer_size >= (size_t)stream->options.highWaterMark;

  return JS_NewBool(ctx, !should_drain);
}

// End method for stdout/stderr
static JSValue js_stdout_stream_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (stream->writable_ended) {
    return JS_UNDEFINED;
  }

  // Write final chunk if provided
  if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    js_stdout_stream_write(ctx, this_val, argc, argv);
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

// Helper: Add EventEmitter methods to stream object
static void add_event_emitter_methods(JSContext* ctx, JSValue stream_obj) {
  // Add EventEmitter wrapper methods
  JS_SetPropertyStr(ctx, stream_obj, "on", JS_NewCFunction(ctx, js_stream_on, "on", 2));
  JS_SetPropertyStr(ctx, stream_obj, "once", JS_NewCFunction(ctx, js_stream_once, "once", 2));
  JS_SetPropertyStr(ctx, stream_obj, "emit", JS_NewCFunction(ctx, js_stream_emit, "emit", 1));
  JS_SetPropertyStr(ctx, stream_obj, "off", JS_NewCFunction(ctx, js_stream_off, "off", 2));
  JS_SetPropertyStr(ctx, stream_obj, "removeListener",
                    JS_NewCFunction(ctx, js_stream_remove_listener, "removeListener", 2));
  JS_SetPropertyStr(ctx, stream_obj, "addListener", JS_NewCFunction(ctx, js_stream_add_listener, "addListener", 2));
  JS_SetPropertyStr(ctx, stream_obj, "removeAllListeners",
                    JS_NewCFunction(ctx, js_stream_remove_all_listeners, "removeAllListeners", 1));
  JS_SetPropertyStr(ctx, stream_obj, "listenerCount",
                    JS_NewCFunction(ctx, js_stream_listener_count, "listenerCount", 1));
}

// Helper: Add Readable methods to stream object
static void add_readable_methods(JSContext* ctx, JSValue stream_obj) {
  // Add Readable methods (pause, resume, pipe, etc.)
  JS_SetPropertyStr(ctx, stream_obj, "pause", JS_NewCFunction(ctx, js_readable_pause, "pause", 0));
  JS_SetPropertyStr(ctx, stream_obj, "resume", JS_NewCFunction(ctx, js_readable_resume, "resume", 0));
  JS_SetPropertyStr(ctx, stream_obj, "isPaused", JS_NewCFunction(ctx, js_readable_is_paused, "isPaused", 0));
  JS_SetPropertyStr(ctx, stream_obj, "setEncoding", JS_NewCFunction(ctx, js_readable_set_encoding, "setEncoding", 1));
  JS_SetPropertyStr(ctx, stream_obj, "pipe", JS_NewCFunction(ctx, js_readable_pipe, "pipe", 2));
  JS_SetPropertyStr(ctx, stream_obj, "unpipe", JS_NewCFunction(ctx, js_readable_unpipe, "unpipe", 1));
  JS_SetPropertyStr(ctx, stream_obj, "push", JS_NewCFunction(ctx, js_readable_push, "push", 2));
}

// Create stdin as a Readable stream
JSValue jsrt_create_stdin_stream(JSContext* ctx) {
  // Ensure stream classes are initialized before creating any streams
  extern void jsrt_stream_init_classes(JSContext * ctx);
  jsrt_stream_init_classes(ctx);

  // Create a Readable stream
  JSValue stdin_obj = js_readable_constructor(ctx, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(stdin_obj)) {
    return stdin_obj;
  }

  // Get the stream data
  JSStreamData* stream = js_stream_get_data(ctx, stdin_obj, js_readable_class_id);
  if (!stream) {
    JS_FreeValue(ctx, stdin_obj);
    return JS_ThrowTypeError(ctx, "Failed to create stdin stream");
  }

  // Add EventEmitter methods (needed because stream module may not be loaded yet)
  add_event_emitter_methods(ctx, stdin_obj);

  // Add Readable methods (pause, resume, pipe, etc.)
  add_readable_methods(ctx, stdin_obj);

  // Override the read method to read from stdin
  JS_SetPropertyStr(ctx, stdin_obj, "read", JS_NewCFunction(ctx, js_stdin_stream_read, "read", 1));
  JS_SetPropertyStr(ctx, stdin_obj, "_read", JS_NewCFunction(ctx, js_stdin_internal_read, "_read", 1));

  // Add write method for stdin compatibility (Node.js process.stdin is writable)
  JS_SetPropertyStr(ctx, stdin_obj, "write", JS_NewCFunction(ctx, js_stdin_stream_write, "write", 3));

  // Set isTTY property
  JS_SetPropertyStr(ctx, stdin_obj, "isTTY", JS_NewBool(ctx, isatty(STDIN_FILENO)));

  // Set fd property
  JS_SetPropertyStr(ctx, stdin_obj, "fd", JS_NewInt32(ctx, STDIN_FILENO));

  return stdin_obj;
}

// Create stdout as a Writable stream
JSValue jsrt_create_stdout_stream(JSContext* ctx) {
  // Ensure stream classes are initialized
  extern void jsrt_stream_init_classes(JSContext * ctx);
  jsrt_stream_init_classes(ctx);

  // Create a Writable stream
  JSValue stdout_obj = js_writable_constructor(ctx, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(stdout_obj)) {
    return stdout_obj;
  }

  // Get the stream data
  JSStreamData* stream = js_stream_get_data(ctx, stdout_obj, js_writable_class_id);
  if (!stream) {
    JS_FreeValue(ctx, stdout_obj);
    return JS_ThrowTypeError(ctx, "Failed to create stdout stream");
  }

  // Add EventEmitter methods (needed because stream module may not be loaded yet)
  add_event_emitter_methods(ctx, stdout_obj);

  // Override the write and end methods
  JS_SetPropertyStr(ctx, stdout_obj, "write", JS_NewCFunction(ctx, js_stdout_stream_write, "write", 3));
  JS_SetPropertyStr(ctx, stdout_obj, "end", JS_NewCFunction(ctx, js_stdout_stream_end, "end", 3));

  // Set isTTY property
  JS_SetPropertyStr(ctx, stdout_obj, "isTTY", JS_NewBool(ctx, isatty(STDOUT_FILENO)));

  // Set fd property
  JS_SetPropertyStr(ctx, stdout_obj, "fd", JS_NewInt32(ctx, STDOUT_FILENO));

  return stdout_obj;
}

// Create stderr as a Writable stream
JSValue jsrt_create_stderr_stream(JSContext* ctx) {
  // Ensure stream classes are initialized
  extern void jsrt_stream_init_classes(JSContext * ctx);
  jsrt_stream_init_classes(ctx);

  // Create a Writable stream
  JSValue stderr_obj = js_writable_constructor(ctx, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(stderr_obj)) {
    return stderr_obj;
  }

  // Get the stream data
  JSStreamData* stream = js_stream_get_data(ctx, stderr_obj, js_writable_class_id);
  if (!stream) {
    JS_FreeValue(ctx, stderr_obj);
    return JS_ThrowTypeError(ctx, "Failed to create stderr stream");
  }

  // Add EventEmitter methods (needed because stream module may not be loaded yet)
  add_event_emitter_methods(ctx, stderr_obj);

  // Override the write and end methods
  JS_SetPropertyStr(ctx, stderr_obj, "write", JS_NewCFunction(ctx, js_stderr_stream_write, "write", 3));
  JS_SetPropertyStr(ctx, stderr_obj, "end", JS_NewCFunction(ctx, js_stdout_stream_end, "end", 3));

  // Set isTTY property
  JS_SetPropertyStr(ctx, stderr_obj, "isTTY", JS_NewBool(ctx, isatty(STDERR_FILENO)));

  // Set fd property
  JS_SetPropertyStr(ctx, stderr_obj, "fd", JS_NewInt32(ctx, STDERR_FILENO));

  return stderr_obj;
}
