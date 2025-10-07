#include "../../util/debug.h"
#include "stream_internal.h"

// Writable stream implementation
JSValue js_writable_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_writable_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSStreamData* stream = calloc(1, sizeof(JSStreamData));
  if (!stream) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Parse options (first argument)
  parse_stream_options(ctx, argc > 0 ? argv[0] : JS_UNDEFINED, &stream->options);

  // Initialize base state
  stream->readable = false;
  stream->writable = true;
  stream->destroyed = false;
  stream->ended = false;
  stream->errored = false;
  stream->error_value = JS_UNDEFINED;

  // Initialize write buffer
  stream->buffered_data = malloc(sizeof(JSValue) * 16);
  if (!stream->buffered_data) {
    free(stream);
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }
  stream->buffer_size = 0;
  stream->buffer_capacity = 16;

  // Initialize writable-specific state
  stream->writable_ended = false;
  stream->writable_finished = false;
  stream->writable_corked = 0;
  stream->write_callback_count = 0;
  stream->write_callback_capacity = 0;
  stream->write_callbacks = NULL;
  stream->need_drain = false;

  // Initialize EventEmitter
  stream->event_emitter = init_stream_event_emitter(ctx, obj);

  JS_SetOpaque(obj, stream);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "writable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  return obj;
}

// Helper: Calculate buffer size in bytes or object count
static size_t calculate_buffer_size(JSStreamData* stream) {
  if (stream->options.objectMode) {
    return stream->buffer_size;  // In object mode, count objects
  } else {
    // In byte mode, sum up chunk sizes (simplified for now)
    return stream->buffer_size;  // Each chunk counts as 1 unit for now
  }
}

// Helper: Queue a write callback
static int queue_write_callback(JSContext* ctx, JSStreamData* stream, JSValue callback) {
  if (JS_IsUndefined(callback) || JS_IsNull(callback)) {
    return 0;  // No callback to queue
  }

  // Expand callback array if needed
  if (stream->write_callback_count >= stream->write_callback_capacity) {
    size_t new_capacity = stream->write_callback_capacity == 0 ? 4 : stream->write_callback_capacity * 2;
    WriteCallback* new_callbacks = realloc(stream->write_callbacks, sizeof(WriteCallback) * new_capacity);
    if (!new_callbacks) {
      return -1;  // Out of memory
    }
    stream->write_callbacks = new_callbacks;
    stream->write_callback_capacity = new_capacity;
  }

  // Queue the callback
  stream->write_callbacks[stream->write_callback_count].callback = JS_DupValue(ctx, callback);
  stream->write_callback_count++;
  return 0;
}

// Helper: Process all queued callbacks
static void process_write_callbacks(JSContext* ctx, JSStreamData* stream, JSValue error) {
  for (size_t i = 0; i < stream->write_callback_count; i++) {
    JSValue callback = stream->write_callbacks[i].callback;
    if (!JS_IsUndefined(callback)) {
      JSValue args[1];
      int argc = 0;
      if (!JS_IsUndefined(error) && !JS_IsNull(error)) {
        args[0] = error;
        argc = 1;
      }
      JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, argc, argc > 0 ? args : NULL);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, callback);
    }
  }
  stream->write_callback_count = 0;
}

// Writable.prototype.write(chunk, [encoding], [callback])
static JSValue js_writable_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  if (!stream->writable) {
    JSValue err = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, err, "message", JS_NewString(ctx, "write after end"));
    stream_emit(ctx, stream, "error", 1, &err);
    JS_FreeValue(ctx, err);
    return JS_NewBool(ctx, false);
  }

  if (stream->destroyed) {
    return JS_NewBool(ctx, false);
  }

  // Parse arguments: write(chunk, [encoding], [callback])
  JSValue chunk = argv[0];
  JSValue callback = JS_UNDEFINED;

  // Determine which argument is the callback
  if (argc >= 3) {
    callback = argv[2];
  } else if (argc >= 2 && JS_IsFunction(ctx, argv[1])) {
    callback = argv[1];
  }

  // If corked, just buffer the write
  if (stream->writable_corked > 0) {
    // Expand buffer if needed
    if (stream->buffer_size >= stream->buffer_capacity) {
      size_t new_capacity = stream->buffer_capacity * 2;
      JSValue* new_buffer = realloc(stream->buffered_data, sizeof(JSValue) * new_capacity);
      if (!new_buffer) {
        return JS_ThrowOutOfMemory(ctx);
      }
      stream->buffered_data = new_buffer;
      stream->buffer_capacity = new_capacity;
    }

    stream->buffered_data[stream->buffer_size++] = JS_DupValue(ctx, chunk);

    if (!JS_IsUndefined(callback)) {
      queue_write_callback(ctx, stream, callback);
    }

    return JS_NewBool(ctx, true);
  }

  // Add chunk to buffer
  if (stream->buffer_size >= stream->buffer_capacity) {
    size_t new_capacity = stream->buffer_capacity * 2;
    JSValue* new_buffer = realloc(stream->buffered_data, sizeof(JSValue) * new_capacity);
    if (!new_buffer) {
      return JS_ThrowOutOfMemory(ctx);
    }
    stream->buffered_data = new_buffer;
    stream->buffer_capacity = new_capacity;
  }

  stream->buffered_data[stream->buffer_size++] = JS_DupValue(ctx, chunk);

  // Check if we're over high water mark (backpressure)
  size_t current_size = calculate_buffer_size(stream);
  bool backpressure = current_size > (size_t)stream->options.highWaterMark;

  if (backpressure) {
    stream->need_drain = true;
  }

  // Process write immediately (call callback synchronously)
  if (!JS_IsUndefined(callback) && !JS_IsNull(callback)) {
    // Call the callback immediately (no error)
    JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, result);
  }

  // Return false if backpressure, true otherwise
  return JS_NewBool(ctx, !backpressure);
}

// Writable.prototype.cork()
static JSValue js_writable_cork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  stream->writable_corked++;
  return JS_UNDEFINED;
}

// Writable.prototype.uncork()
static JSValue js_writable_uncork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (stream->writable_corked > 0) {
    stream->writable_corked--;

    // If uncorked completely, process buffered writes
    if (stream->writable_corked == 0 && stream->write_callback_count > 0) {
      process_write_callbacks(ctx, stream, JS_UNDEFINED);
    }
  }

  return JS_UNDEFINED;
}

// Writable.prototype.setDefaultEncoding(encoding)
static JSValue js_writable_set_default_encoding(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "encoding is required");
  }

  const char* encoding = JS_ToCString(ctx, argv[0]);
  if (!encoding) {
    return JS_EXCEPTION;
  }

  // Validate encoding (basic validation)
  if (strcmp(encoding, "utf8") != 0 && strcmp(encoding, "utf-8") != 0 && strcmp(encoding, "ascii") != 0 &&
      strcmp(encoding, "base64") != 0 && strcmp(encoding, "hex") != 0 && strcmp(encoding, "binary") != 0) {
    JS_FreeCString(ctx, encoding);
    return JS_ThrowTypeError(ctx, "Unknown encoding");
  }

  // For now, just store the encoding name (leak managed by static strings)
  stream->options.defaultEncoding = encoding;

  return this_val;
}

// Writable.prototype.end([chunk], [encoding], [callback])
static JSValue js_writable_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (stream->writable_ended) {
    return JS_UNDEFINED;  // Already ended
  }

  // Parse arguments: end([chunk], [encoding], [callback])
  JSValue callback = JS_UNDEFINED;
  JSValue chunk = JS_UNDEFINED;

  if (argc >= 1) {
    // Check if first arg is a function (callback only)
    if (JS_IsFunction(ctx, argv[0])) {
      callback = argv[0];
    } else {
      chunk = argv[0];
      if (argc >= 2) {
        if (JS_IsFunction(ctx, argv[1])) {
          callback = argv[1];
        } else if (argc >= 3 && JS_IsFunction(ctx, argv[2])) {
          callback = argv[2];
        }
      }
    }
  }

  // If chunk provided, write it first
  if (!JS_IsUndefined(chunk) && !JS_IsNull(chunk)) {
    JSValue write_args[2] = {chunk, callback};
    JSValue write_result = js_writable_write(ctx, this_val, JS_IsUndefined(callback) ? 1 : 2, write_args);
    JS_FreeValue(ctx, write_result);
  } else if (!JS_IsUndefined(callback)) {
    // If only callback provided, register it for finish event
    queue_write_callback(ctx, stream, callback);
  }

  stream->writable = false;
  stream->ended = true;
  stream->writable_ended = true;

  JS_SetPropertyStr(ctx, this_val, "writable", JS_NewBool(ctx, false));

  // Process any pending callbacks
  if (stream->write_callback_count > 0) {
    process_write_callbacks(ctx, stream, JS_UNDEFINED);
  }

  // Mark as finished and emit 'finish' event
  stream->writable_finished = true;
  stream_emit(ctx, stream, "finish", 0, NULL);

  return JS_UNDEFINED;
}

// Property getters

// writable.writable
static JSValue js_writable_get_writable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_UNDEFINED;
  }
  return JS_NewBool(ctx, stream->writable);
}

// writable.writableEnded
static JSValue js_writable_get_writable_ended(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_UNDEFINED;
  }
  return JS_NewBool(ctx, stream->writable_ended);
}

// writable.writableFinished
static JSValue js_writable_get_writable_finished(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_UNDEFINED;
  }
  return JS_NewBool(ctx, stream->writable_finished);
}

// writable.writableLength
static JSValue js_writable_get_writable_length(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_UNDEFINED;
  }
  return JS_NewInt32(ctx, (int32_t)calculate_buffer_size(stream));
}

// writable.writableHighWaterMark
static JSValue js_writable_get_writable_high_water_mark(JSContext* ctx, JSValueConst this_val, int argc,
                                                        JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_UNDEFINED;
  }
  return JS_NewInt32(ctx, stream->options.highWaterMark);
}

// writable.writableCorked
static JSValue js_writable_get_writable_corked(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_UNDEFINED;
  }
  return JS_NewInt32(ctx, stream->writable_corked);
}

// writable.writableObjectMode
static JSValue js_writable_get_writable_object_mode(JSContext* ctx, JSValueConst this_val, int argc,
                                                    JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_UNDEFINED;
  }
  return JS_NewBool(ctx, stream->options.objectMode);
}

// Initialize Writable prototype with all methods
void js_writable_init_prototype(JSContext* ctx, JSValue writable_proto) {
  // Methods
  JS_SetPropertyStr(ctx, writable_proto, "write", JS_NewCFunction(ctx, js_writable_write, "write", 3));
  JS_SetPropertyStr(ctx, writable_proto, "end", JS_NewCFunction(ctx, js_writable_end, "end", 3));
  JS_SetPropertyStr(ctx, writable_proto, "cork", JS_NewCFunction(ctx, js_writable_cork, "cork", 0));
  JS_SetPropertyStr(ctx, writable_proto, "uncork", JS_NewCFunction(ctx, js_writable_uncork, "uncork", 0));
  JS_SetPropertyStr(ctx, writable_proto, "setDefaultEncoding",
                    JS_NewCFunction(ctx, js_writable_set_default_encoding, "setDefaultEncoding", 1));

  // Property getters
  JSAtom writable_atom = JS_NewAtom(ctx, "writable");
  JS_DefinePropertyGetSet(ctx, writable_proto, writable_atom,
                          JS_NewCFunction(ctx, js_writable_get_writable, "get writable", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_atom);

  JSAtom writable_ended_atom = JS_NewAtom(ctx, "writableEnded");
  JS_DefinePropertyGetSet(ctx, writable_proto, writable_ended_atom,
                          JS_NewCFunction(ctx, js_writable_get_writable_ended, "get writableEnded", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_ended_atom);

  JSAtom writable_finished_atom = JS_NewAtom(ctx, "writableFinished");
  JS_DefinePropertyGetSet(ctx, writable_proto, writable_finished_atom,
                          JS_NewCFunction(ctx, js_writable_get_writable_finished, "get writableFinished", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_finished_atom);

  JSAtom writable_length_atom = JS_NewAtom(ctx, "writableLength");
  JS_DefinePropertyGetSet(ctx, writable_proto, writable_length_atom,
                          JS_NewCFunction(ctx, js_writable_get_writable_length, "get writableLength", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_length_atom);

  JSAtom writable_hwm_atom = JS_NewAtom(ctx, "writableHighWaterMark");
  JS_DefinePropertyGetSet(
      ctx, writable_proto, writable_hwm_atom,
      JS_NewCFunction(ctx, js_writable_get_writable_high_water_mark, "get writableHighWaterMark", 0), JS_UNDEFINED,
      JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_hwm_atom);

  JSAtom writable_corked_atom = JS_NewAtom(ctx, "writableCorked");
  JS_DefinePropertyGetSet(ctx, writable_proto, writable_corked_atom,
                          JS_NewCFunction(ctx, js_writable_get_writable_corked, "get writableCorked", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_corked_atom);

  JSAtom writable_obj_mode_atom = JS_NewAtom(ctx, "writableObjectMode");
  JS_DefinePropertyGetSet(ctx, writable_proto, writable_obj_mode_atom,
                          JS_NewCFunction(ctx, js_writable_get_writable_object_mode, "get writableObjectMode", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_obj_mode_atom);
}
