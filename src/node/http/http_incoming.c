#include "../../runtime.h"
#include "../stream/stream_internal.h"
#include "http_internal.h"

// IncomingMessage class implementation
// Phase 4: Now implements Readable stream interface

// Forward declarations for stream methods
JSValue js_http_incoming_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_is_paused(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_pipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_unpipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_set_encoding(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// IncomingMessage constructor
JSValue js_http_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_http_request_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSHttpRequest* req = calloc(1, sizeof(JSHttpRequest));
  if (!req) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  req->ctx = ctx;
  req->request_obj = JS_DupValue(ctx, obj);
  req->headers = JS_NewObject(ctx);

  // Phase 4: Initialize Readable stream
  req->stream = calloc(1, sizeof(JSStreamData));
  if (!req->stream) {
    JS_FreeValue(ctx, req->request_obj);
    JS_FreeValue(ctx, req->headers);
    free(req);
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize stream with default options
  req->stream->readable = true;
  req->stream->writable = false;
  req->stream->destroyed = false;
  req->stream->ended = false;
  req->stream->errored = false;
  req->stream->error_value = JS_UNDEFINED;
  req->stream->buffer_capacity = 16;
  req->stream->buffer_size = 0;
  req->stream->buffered_data = malloc(sizeof(JSValue) * req->stream->buffer_capacity);
  // CRITICAL FIX #3: Check malloc() return value
  if (!req->stream->buffered_data) {
    free(req->stream);
    JS_FreeValue(ctx, req->request_obj);
    JS_FreeValue(ctx, req->headers);
    free(req);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize Readable-specific state
  req->stream->flowing = true;  // Start in flowing mode
  req->stream->reading = false;
  req->stream->ended_emitted = false;
  req->stream->readable_emitted = false;
  req->stream->pipe_destinations = NULL;
  req->stream->pipe_count = 0;
  req->stream->pipe_capacity = 0;

  // Set default options
  req->stream->options.highWaterMark = 16384;  // 16KB default
  req->stream->options.objectMode = false;
  req->stream->options.encoding = NULL;
  req->stream->options.defaultEncoding = "utf8";
  req->stream->options.emitClose = true;
  req->stream->options.autoDestroy = true;

  JS_SetOpaque(obj, req);

  // Add default properties
  JS_SetPropertyStr(ctx, obj, "method", JS_NewString(ctx, "GET"));
  JS_SetPropertyStr(ctx, obj, "url", JS_NewString(ctx, "/"));
  JS_SetPropertyStr(ctx, obj, "httpVersion", JS_NewString(ctx, "1.1"));
  JS_SetPropertyStr(ctx, obj, "headers", JS_DupValue(ctx, req->headers));

  // Phase 4: Add Readable stream properties
  JS_SetPropertyStr(ctx, obj, "readable", JS_NewBool(ctx, true));
  JS_SetPropertyStr(ctx, obj, "readableEnded", JS_NewBool(ctx, false));

  // Phase 4: Add Readable stream methods
  JS_SetPropertyStr(ctx, obj, "pause", JS_NewCFunction(ctx, js_http_incoming_pause, "pause", 0));
  JS_SetPropertyStr(ctx, obj, "resume", JS_NewCFunction(ctx, js_http_incoming_resume, "resume", 0));
  JS_SetPropertyStr(ctx, obj, "isPaused", JS_NewCFunction(ctx, js_http_incoming_is_paused, "isPaused", 0));
  JS_SetPropertyStr(ctx, obj, "pipe", JS_NewCFunction(ctx, js_http_incoming_pipe, "pipe", 2));
  JS_SetPropertyStr(ctx, obj, "unpipe", JS_NewCFunction(ctx, js_http_incoming_unpipe, "unpipe", 1));
  JS_SetPropertyStr(ctx, obj, "read", JS_NewCFunction(ctx, js_http_incoming_read, "read", 1));
  JS_SetPropertyStr(ctx, obj, "setEncoding", JS_NewCFunction(ctx, js_http_incoming_set_encoding, "setEncoding", 1));

  // Add EventEmitter methods (IncomingMessage extends EventEmitter)
  setup_event_emitter_inheritance(ctx, obj);

  return obj;
}

// Phase 4: Readable stream method implementations

// IncomingMessage.prototype.pause()
JSValue js_http_incoming_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpRequest* req = JS_GetOpaque(this_val, js_http_request_class_id);
  if (!req || !req->stream) {
    return JS_ThrowTypeError(ctx, "Not an IncomingMessage");
  }

  if (req->stream->flowing) {
    req->stream->flowing = false;

    // Pause the underlying socket if available
    if (!JS_IsUndefined(req->socket)) {
      JSValue pause_method = JS_GetPropertyStr(ctx, req->socket, "pause");
      if (JS_IsFunction(ctx, pause_method)) {
        JSValue result = JS_Call(ctx, pause_method, req->socket, 0, NULL);
        JS_FreeValue(ctx, result);
      }
      JS_FreeValue(ctx, pause_method);
    }

    // Emit pause event
    JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue event_name = JS_NewString(ctx, "pause");
      JSValue result = JS_Call(ctx, emit, this_val, 1, &event_name);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, event_name);
    }
    JS_FreeValue(ctx, emit);
  }

  return JS_DupValue(ctx, this_val);  // Return this for chaining
}

// IncomingMessage.prototype.resume()
JSValue js_http_incoming_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpRequest* req = JS_GetOpaque(this_val, js_http_request_class_id);
  if (!req || !req->stream) {
    return JS_ThrowTypeError(ctx, "Not an IncomingMessage");
  }

  if (!req->stream->flowing) {
    req->stream->flowing = true;

    // Resume the underlying socket if available
    if (!JS_IsUndefined(req->socket)) {
      JSValue resume_method = JS_GetPropertyStr(ctx, req->socket, "resume");
      if (JS_IsFunction(ctx, resume_method)) {
        JSValue result = JS_Call(ctx, resume_method, req->socket, 0, NULL);
        JS_FreeValue(ctx, result);
      }
      JS_FreeValue(ctx, resume_method);
    }

    // Emit resume event
    JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue event_name = JS_NewString(ctx, "resume");
      JSValue result = JS_Call(ctx, emit, this_val, 1, &event_name);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, event_name);
    }
    JS_FreeValue(ctx, emit);

    // Emit buffered data in flowing mode
    while (req->stream->buffer_size > 0 && req->stream->flowing) {
      JSValue data = req->stream->buffered_data[0];

      // Shift buffer
      for (size_t i = 1; i < req->stream->buffer_size; i++) {
        req->stream->buffered_data[i - 1] = req->stream->buffered_data[i];
      }
      req->stream->buffer_size--;

      // Emit 'data' event
      emit = JS_GetPropertyStr(ctx, this_val, "emit");
      if (JS_IsFunction(ctx, emit)) {
        JSValue event_args[2] = {JS_NewString(ctx, "data"), JS_DupValue(ctx, data)};
        JSValue result = JS_Call(ctx, emit, this_val, 2, event_args);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, event_args[0]);
        JS_FreeValue(ctx, event_args[1]);
      }
      JS_FreeValue(ctx, emit);

      JS_FreeValue(ctx, data);
    }

    // If ended and buffer is empty, emit 'end'
    if (req->stream->ended && req->stream->buffer_size == 0 && !req->stream->ended_emitted) {
      req->stream->ended_emitted = true;
      emit = JS_GetPropertyStr(ctx, this_val, "emit");
      if (JS_IsFunction(ctx, emit)) {
        JSValue event_name = JS_NewString(ctx, "end");
        JSValue result = JS_Call(ctx, emit, this_val, 1, &event_name);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, event_name);
      }
      JS_FreeValue(ctx, emit);
    }
  }

  return JS_DupValue(ctx, this_val);  // Return this for chaining
}

// IncomingMessage.prototype.isPaused()
JSValue js_http_incoming_is_paused(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpRequest* req = JS_GetOpaque(this_val, js_http_request_class_id);
  if (!req || !req->stream) {
    return JS_UNDEFINED;
  }

  return JS_NewBool(ctx, !req->stream->flowing);
}

// IncomingMessage.prototype.pipe(destination, [options])
JSValue js_http_incoming_pipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpRequest* req = JS_GetOpaque(this_val, js_http_request_class_id);
  if (!req || !req->stream) {
    return JS_ThrowTypeError(ctx, "Not an IncomingMessage");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "pipe() requires destination argument");
  }

  JSValue dest = argv[0];

  // Parse options (end: true by default)
  bool end_on_finish = true;
  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue end_opt = JS_GetPropertyStr(ctx, argv[1], "end");
    if (JS_IsBool(end_opt)) {
      end_on_finish = JS_ToBool(ctx, end_opt);
    }
    JS_FreeValue(ctx, end_opt);
  }

  // Add to pipe destinations
  if (req->stream->pipe_destinations == NULL) {
    req->stream->pipe_capacity = 4;
    req->stream->pipe_destinations = malloc(sizeof(JSValue) * req->stream->pipe_capacity);
    // CRITICAL FIX #3: Check malloc() return value
    if (!req->stream->pipe_destinations) {
      return JS_ThrowOutOfMemory(ctx);
    }
  } else if (req->stream->pipe_count >= req->stream->pipe_capacity) {
    size_t new_capacity = req->stream->pipe_capacity * 2;
    // CRITICAL FIX #3: Check realloc() return value
    JSValue* new_destinations = realloc(req->stream->pipe_destinations, sizeof(JSValue) * new_capacity);
    if (!new_destinations) {
      return JS_ThrowOutOfMemory(ctx);
    }
    req->stream->pipe_destinations = new_destinations;
    req->stream->pipe_capacity = new_capacity;
  }

  req->stream->pipe_destinations[req->stream->pipe_count++] = JS_DupValue(ctx, dest);

  // Emit 'pipe' event
  JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue event_args[2] = {JS_NewString(ctx, "pipe"), JS_DupValue(ctx, dest)};
    JSValue result = JS_Call(ctx, emit, this_val, 2, event_args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, event_args[0]);
    JS_FreeValue(ctx, event_args[1]);
  }
  JS_FreeValue(ctx, emit);

  // Switch to flowing mode (automatically call resume)
  if (!req->stream->flowing) {
    js_http_incoming_resume(ctx, this_val, 0, NULL);
  }

  return JS_DupValue(ctx, dest);  // Return destination for chaining
}

// IncomingMessage.prototype.unpipe([destination])
JSValue js_http_incoming_unpipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpRequest* req = JS_GetOpaque(this_val, js_http_request_class_id);
  if (!req || !req->stream) {
    return JS_ThrowTypeError(ctx, "Not an IncomingMessage");
  }

  if (req->stream->pipe_destinations == NULL || req->stream->pipe_count == 0) {
    return JS_DupValue(ctx, this_val);  // No pipes to remove
  }

  JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");

  // If no destination specified, unpipe all
  if (argc == 0 || JS_IsUndefined(argv[0])) {
    for (size_t i = 0; i < req->stream->pipe_count; i++) {
      JSValue dest = req->stream->pipe_destinations[i];
      if (JS_IsFunction(ctx, emit)) {
        JSValue event_args[2] = {JS_NewString(ctx, "unpipe"), JS_DupValue(ctx, dest)};
        JSValue result = JS_Call(ctx, emit, this_val, 2, event_args);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, event_args[0]);
        JS_FreeValue(ctx, event_args[1]);
      }
      JS_FreeValue(ctx, dest);
    }
    req->stream->pipe_count = 0;
  } else {
    // Unpipe specific destination
    JSValue dest_to_remove = argv[0];

    for (size_t i = 0; i < req->stream->pipe_count; i++) {
      if (JS_VALUE_GET_PTR(req->stream->pipe_destinations[i]) == JS_VALUE_GET_PTR(dest_to_remove)) {
        JSValue dest = req->stream->pipe_destinations[i];
        if (JS_IsFunction(ctx, emit)) {
          JSValue event_args[2] = {JS_NewString(ctx, "unpipe"), JS_DupValue(ctx, dest)};
          JSValue result = JS_Call(ctx, emit, this_val, 2, event_args);
          JS_FreeValue(ctx, result);
          JS_FreeValue(ctx, event_args[0]);
          JS_FreeValue(ctx, event_args[1]);
        }
        JS_FreeValue(ctx, dest);

        // Shift remaining destinations
        for (size_t j = i + 1; j < req->stream->pipe_count; j++) {
          req->stream->pipe_destinations[j - 1] = req->stream->pipe_destinations[j];
        }
        req->stream->pipe_count--;
        break;
      }
    }
  }

  JS_FreeValue(ctx, emit);
  return JS_DupValue(ctx, this_val);  // Return this for chaining
}

// IncomingMessage.prototype.read([size])
JSValue js_http_incoming_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpRequest* req = JS_GetOpaque(this_val, js_http_request_class_id);
  if (!req || !req->stream) {
    return JS_ThrowTypeError(ctx, "Not an IncomingMessage");
  }

  // If stream has ended and no data, return null
  if (req->stream->ended && req->stream->buffer_size == 0) {
    return JS_NULL;
  }

  // If no data available, return null
  if (req->stream->buffer_size == 0) {
    req->stream->reading = true;

    // If ended, emit 'end' event if not yet emitted
    if (req->stream->ended && !req->stream->ended_emitted) {
      req->stream->ended_emitted = true;
      JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
      if (JS_IsFunction(ctx, emit)) {
        JSValue event_name = JS_NewString(ctx, "end");
        JSValue result = JS_Call(ctx, emit, this_val, 1, &event_name);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, event_name);
      }
      JS_FreeValue(ctx, emit);
    }

    return JS_NULL;
  }

  // Read one chunk at a time
  JSValue data = req->stream->buffered_data[0];

  // Shift buffer
  for (size_t i = 1; i < req->stream->buffer_size; i++) {
    req->stream->buffered_data[i - 1] = req->stream->buffered_data[i];
  }
  req->stream->buffer_size--;

  // If ended and buffer is empty, emit 'end'
  if (req->stream->ended && req->stream->buffer_size == 0 && !req->stream->ended_emitted) {
    req->stream->ended_emitted = true;
    JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue event_name = JS_NewString(ctx, "end");
      JSValue result = JS_Call(ctx, emit, this_val, 1, &event_name);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, event_name);
    }
    JS_FreeValue(ctx, emit);
  }

  return data;
}

// IncomingMessage.prototype.setEncoding(encoding)
JSValue js_http_incoming_set_encoding(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpRequest* req = JS_GetOpaque(this_val, js_http_request_class_id);
  if (!req || !req->stream) {
    return JS_ThrowTypeError(ctx, "Not an IncomingMessage");
  }

  if (argc > 0 && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0])) {
    const char* enc = JS_ToCString(ctx, argv[0]);
    if (enc) {
      // CRITICAL FIX #1: Free old encoding if exists
      if (req->stream->options.encoding) {
        free((void*)req->stream->options.encoding);
      }
      // Duplicate the string for storage (prevent use-after-free)
      req->stream->options.encoding = strdup(enc);
      JS_FreeCString(ctx, enc);
    }
  }

  return JS_DupValue(ctx, this_val);  // Return this for chaining
}

// Helper function to push data into the IncomingMessage stream
// Called from parser callbacks (Task 4.1.2)
void js_http_incoming_push_data(JSContext* ctx, JSValue incoming_msg, const char* data, size_t length) {
  JSHttpRequest* req = JS_GetOpaque(incoming_msg, js_http_request_class_id);
  if (!req || !req->stream) {
    return;
  }

  // Create Buffer from data
  JSValue chunk = JS_NewStringLen(ctx, data, length);

  // Add to buffer with overflow protection
  if (req->stream->buffer_size >= req->stream->buffer_capacity) {
// CRITICAL FIX #2: Check maximum buffer size (64KB limit)
#define MAX_STREAM_BUFFER_SIZE 65536
    if (req->stream->buffer_capacity >= MAX_STREAM_BUFFER_SIZE) {
      // Emit error - buffer too large
      JSValue emit = JS_GetPropertyStr(ctx, incoming_msg, "emit");
      if (JS_IsFunction(ctx, emit)) {
        JSValue error = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Stream buffer overflow - too much data"));
        JSValue args[] = {JS_NewString(ctx, "error"), error};
        JSValue result = JS_Call(ctx, emit, incoming_msg, 2, args);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
      }
      JS_FreeValue(ctx, emit);
      JS_FreeValue(ctx, chunk);
      return;
    }

    // CRITICAL FIX #3: Check realloc() return value
    size_t new_capacity = req->stream->buffer_capacity * 2;
    JSValue* new_buffer = realloc(req->stream->buffered_data, sizeof(JSValue) * new_capacity);
    if (!new_buffer) {
      // Allocation failed - emit error
      JSValue emit = JS_GetPropertyStr(ctx, incoming_msg, "emit");
      if (JS_IsFunction(ctx, emit)) {
        JSValue error = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Out of memory"));
        JSValue args[] = {JS_NewString(ctx, "error"), error};
        JSValue result = JS_Call(ctx, emit, incoming_msg, 2, args);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
      }
      JS_FreeValue(ctx, emit);
      JS_FreeValue(ctx, chunk);
      return;
    }
    req->stream->buffered_data = new_buffer;
    req->stream->buffer_capacity = new_capacity;
  }

  req->stream->buffered_data[req->stream->buffer_size++] = chunk;

  // If in flowing mode, emit 'data' event immediately
  if (req->stream->flowing) {
    while (req->stream->buffer_size > 0 && req->stream->flowing) {
      JSValue data = req->stream->buffered_data[0];

      // Shift buffer
      for (size_t i = 1; i < req->stream->buffer_size; i++) {
        req->stream->buffered_data[i - 1] = req->stream->buffered_data[i];
      }
      req->stream->buffer_size--;

      // Write to all piped destinations
      if (req->stream->pipe_destinations != NULL && req->stream->pipe_count > 0) {
        for (size_t i = 0; i < req->stream->pipe_count; i++) {
          JSValue dest = req->stream->pipe_destinations[i];
          JSValue write_method = JS_GetPropertyStr(ctx, dest, "write");
          if (JS_IsFunction(ctx, write_method)) {
            JSValue result = JS_Call(ctx, write_method, dest, 1, &data);
            if (!JS_IsException(result)) {
              JS_FreeValue(ctx, result);
            }
          }
          JS_FreeValue(ctx, write_method);
        }
      }

      // Emit 'data' event
      JSValue emit = JS_GetPropertyStr(ctx, incoming_msg, "emit");
      if (JS_IsFunction(ctx, emit)) {
        JSValue event_args[2] = {JS_NewString(ctx, "data"), JS_DupValue(ctx, data)};
        JSValue result = JS_Call(ctx, emit, incoming_msg, 2, event_args);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, event_args[0]);
        JS_FreeValue(ctx, event_args[1]);
      }
      JS_FreeValue(ctx, emit);

      JS_FreeValue(ctx, data);
    }
  }
}

// Helper function to signal end of IncomingMessage stream
// Called from parser callbacks (Task 4.1.3)
void js_http_incoming_end(JSContext* ctx, JSValue incoming_msg) {
  JSHttpRequest* req = JS_GetOpaque(incoming_msg, js_http_request_class_id);
  if (!req || !req->stream) {
    return;
  }

  req->stream->ended = true;
  JS_SetPropertyStr(ctx, incoming_msg, "readable", JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, incoming_msg, "readableEnded", JS_NewBool(ctx, true));

  // If buffer is empty or in flowing mode, emit 'end' immediately
  if ((req->stream->buffer_size == 0 || req->stream->flowing) && !req->stream->ended_emitted) {
    req->stream->ended_emitted = true;
    JSValue emit = JS_GetPropertyStr(ctx, incoming_msg, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue event_name = JS_NewString(ctx, "end");
      JSValue result = JS_Call(ctx, emit, incoming_msg, 1, &event_name);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, event_name);
    }
    JS_FreeValue(ctx, emit);

    // End all piped destinations
    if (req->stream->pipe_destinations != NULL) {
      for (size_t i = 0; i < req->stream->pipe_count; i++) {
        JSValue dest = req->stream->pipe_destinations[i];
        JSValue end_method = JS_GetPropertyStr(ctx, dest, "end");
        if (JS_IsFunction(ctx, end_method)) {
          JSValue result = JS_Call(ctx, end_method, dest, 0, NULL);
          JS_FreeValue(ctx, result);
        }
        JS_FreeValue(ctx, end_method);
      }
    }
  }

  JSRT_Runtime* runtime = JS_GetContextOpaque(ctx);
  if (runtime) {
    JSRuntime* qjs_runtime = JS_GetRuntime(ctx);
    while (JS_IsJobPending(qjs_runtime)) {
      if (!JSRT_RuntimeRunTicket(runtime)) {
        break;
      }
    }
  }
}

// IncomingMessage finalizer
void js_http_request_finalizer(JSRuntime* rt, JSValue val) {
  JSHttpRequest* req = JS_GetOpaque(val, js_http_request_class_id);
  if (req) {
    free(req->method);
    free(req->url);
    free(req->http_version);
    JS_FreeValueRT(rt, req->headers);
    JS_FreeValueRT(rt, req->socket);

    // Phase 4: Free stream data
    if (req->stream) {
      // Free error value if present
      if (!JS_IsUndefined(req->stream->error_value)) {
        JS_FreeValueRT(rt, req->stream->error_value);
      }
      // Free buffered data
      if (req->stream->buffered_data) {
        for (size_t i = 0; i < req->stream->buffer_size; i++) {
          JS_FreeValueRT(rt, req->stream->buffered_data[i]);
        }
        free(req->stream->buffered_data);
      }
      // Free pipe destinations
      if (req->stream->pipe_destinations) {
        for (size_t i = 0; i < req->stream->pipe_count; i++) {
          JS_FreeValueRT(rt, req->stream->pipe_destinations[i]);
        }
        free(req->stream->pipe_destinations);
      }
      // CRITICAL FIX #1: Free encoding string if set
      if (req->stream->options.encoding) {
        free((void*)req->stream->options.encoding);
      }
      free(req->stream);
    }

    free(req);
  }
}
