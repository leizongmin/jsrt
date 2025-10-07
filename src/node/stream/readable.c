#include "stream_internal.h"

// Readable stream implementation
JSValue js_readable_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_readable_class_id);
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
  stream->readable = true;
  stream->writable = false;
  stream->destroyed = false;
  stream->ended = false;
  stream->errored = false;
  stream->error_value = JS_UNDEFINED;
  stream->buffer_capacity = 16;
  stream->buffered_data = malloc(sizeof(JSValue) * stream->buffer_capacity);

  // Initialize Phase 2: Readable state
  stream->flowing = false;  // Start in paused mode
  stream->reading = false;
  stream->ended_emitted = false;
  stream->readable_emitted = false;
  stream->pipe_destinations = NULL;
  stream->pipe_count = 0;
  stream->pipe_capacity = 0;

  // Initialize EventEmitter
  stream->event_emitter = init_stream_event_emitter(ctx, obj);

  JS_SetOpaque(obj, stream);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "readable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  return obj;
}

// Readable.prototype.read([size])
static JSValue js_readable_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  // If stream has ended and no data, return null
  if (stream->ended && stream->buffer_size == 0) {
    return JS_NULL;
  }

  // Parse size parameter (optional)
  int32_t size = -1;  // -1 means read all available
  if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    if (JS_ToInt32(ctx, &size, argv[0]) < 0) {
      return JS_EXCEPTION;
    }
  }

  // If no data available, return null
  if (stream->buffer_size == 0) {
    // Mark that we're ready to read
    stream->reading = true;

    // If ended, emit 'end' event if not yet emitted
    if (stream->ended && !stream->ended_emitted) {
      stream->ended_emitted = true;
      stream_emit(ctx, stream, "end", 0, NULL);
    }

    return JS_NULL;
  }

  // For simplicity, read one chunk at a time (size parameter support can be enhanced later)
  JSValue data = stream->buffered_data[0];

  // Shift buffer
  for (size_t i = 1; i < stream->buffer_size; i++) {
    stream->buffered_data[i - 1] = stream->buffered_data[i];
  }
  stream->buffer_size--;

  // Reset readable_emitted flag so 'readable' can be emitted again
  stream->readable_emitted = false;

  // If buffer is now below highWaterMark, trigger _read()
  // (For now, we don't have _read() callback, but this is where it would be called)

  // If ended and buffer is empty, emit 'end'
  if (stream->ended && stream->buffer_size == 0 && !stream->ended_emitted) {
    stream->ended_emitted = true;
    stream_emit(ctx, stream, "end", 0, NULL);
  }

  return data;
}

// Readable.prototype.push(chunk, [encoding])
static JSValue js_readable_push(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  JSValue chunk = argv[0];

  // If push(null), end the stream
  if (JS_IsNull(chunk)) {
    stream->ended = true;
    JS_SetPropertyStr(ctx, this_val, "readable", JS_NewBool(ctx, false));

    // Emit 'end' event if not already emitted and buffer is empty
    if (!stream->ended_emitted && stream->buffer_size == 0) {
      stream->ended_emitted = true;
      stream_emit(ctx, stream, "end", 0, NULL);
    }

    return JS_NewBool(ctx, false);
  }

  // Add to buffer
  if (stream->buffer_size >= stream->buffer_capacity) {
    stream->buffer_capacity *= 2;
    stream->buffered_data = realloc(stream->buffered_data, sizeof(JSValue) * stream->buffer_capacity);
  }

  stream->buffered_data[stream->buffer_size++] = JS_DupValue(ctx, chunk);

  // If in flowing mode, emit 'data' event immediately
  if (stream->flowing) {
    // Emit data for all buffered chunks
    while (stream->buffer_size > 0 && stream->flowing) {
      JSValue data = stream->buffered_data[0];

      // Shift buffer
      for (size_t i = 1; i < stream->buffer_size; i++) {
        stream->buffered_data[i - 1] = stream->buffered_data[i];
      }
      stream->buffer_size--;

      // Emit 'data' event (stream_emit will dup the value for listeners)
      stream_emit(ctx, stream, "data", 1, &data);

      // Free the data value after emitting (we own it from the shift)
      JS_FreeValue(ctx, data);
    }

    // If ended and buffer is empty, emit 'end'
    if (stream->ended && stream->buffer_size == 0 && !stream->ended_emitted) {
      stream->ended_emitted = true;
      stream_emit(ctx, stream, "end", 0, NULL);
    }
  } else {
    // In paused mode, emit 'readable' event if not already emitted
    if (!stream->readable_emitted && stream->buffer_size > 0) {
      stream->readable_emitted = true;
      stream_emit(ctx, stream, "readable", 0, NULL);
    }
  }

  // Return true if buffer is below highWaterMark, false otherwise (backpressure)
  // For object mode, compare count; for byte mode, we'd need to calculate size
  int highWaterMark = stream->options.highWaterMark;
  bool backpressure = (stream->buffer_size >= (size_t)highWaterMark);

  return JS_NewBool(ctx, !backpressure);
}

// Readable.prototype.pause()
static JSValue js_readable_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  if (stream->flowing) {
    stream->flowing = false;
    stream_emit(ctx, stream, "pause", 0, NULL);
  }

  return JS_DupValue(ctx, this_val);  // Return this for chaining
}

// Readable.prototype.resume()
static JSValue js_readable_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  if (!stream->flowing) {
    stream->flowing = true;
    stream_emit(ctx, stream, "resume", 0, NULL);

    // Emit queued data in flowing mode
    while (stream->buffer_size > 0 && stream->flowing) {
      JSValue data = stream->buffered_data[0];

      // Shift buffer
      for (size_t i = 1; i < stream->buffer_size; i++) {
        stream->buffered_data[i - 1] = stream->buffered_data[i];
      }
      stream->buffer_size--;

      // Emit 'data' event
      stream_emit(ctx, stream, "data", 1, &data);

      // Free the data value after emitting
      JS_FreeValue(ctx, data);
    }

    // If ended and buffer is empty, emit 'end'
    if (stream->ended && stream->buffer_size == 0 && !stream->ended_emitted) {
      stream->ended_emitted = true;
      stream_emit(ctx, stream, "end", 0, NULL);
    }
  }

  return JS_DupValue(ctx, this_val);  // Return this for chaining
}

// Readable.prototype.isPaused()
static JSValue js_readable_is_paused(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    return JS_UNDEFINED;
  }

  return JS_NewBool(ctx, !stream->flowing);
}

// Readable.prototype.setEncoding(encoding)
static JSValue js_readable_set_encoding(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  if (argc > 0 && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0])) {
    const char* enc = JS_ToCString(ctx, argv[0]);
    if (enc) {
      // Note: For now, we just accept the encoding but don't apply it
      // In a full implementation, we'd need to convert buffers to strings
      stream->options.encoding = enc;
      // TODO: Should strdup enc if we want to own it
      JS_FreeCString(ctx, enc);
    }
  }

  return JS_DupValue(ctx, this_val);  // Return this for chaining
}

// Readable.prototype.readable property getter
static JSValue js_readable_get_readable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    return JS_UNDEFINED;
  }

  return JS_NewBool(ctx, stream->readable && !stream->destroyed);
}

// Readable.prototype.pipe(destination, [options])
static JSValue js_readable_pipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* src = JS_GetOpaque(this_val, js_readable_class_id);
  if (!src) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
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
  if (src->pipe_destinations == NULL) {
    src->pipe_capacity = 4;
    src->pipe_destinations = malloc(sizeof(JSValue) * src->pipe_capacity);
  } else if (src->pipe_count >= src->pipe_capacity) {
    src->pipe_capacity *= 2;
    src->pipe_destinations = realloc(src->pipe_destinations, sizeof(JSValue) * src->pipe_capacity);
  }

  src->pipe_destinations[src->pipe_count++] = JS_DupValue(ctx, dest);

  // Emit 'pipe' event
  stream_emit(ctx, src, "pipe", 1, &dest);

  // Switch to flowing mode
  if (!src->flowing) {
    src->flowing = true;
    stream_emit(ctx, src, "resume", 0, NULL);

    // Emit queued data
    while (src->buffer_size > 0 && src->flowing) {
      JSValue data = src->buffered_data[0];

      // Shift buffer
      for (size_t i = 1; i < src->buffer_size; i++) {
        src->buffered_data[i - 1] = src->buffered_data[i];
      }
      src->buffer_size--;

      // Write to destination (simplified - doesn't handle backpressure yet)
      JSValue write_method = JS_GetPropertyStr(ctx, dest, "write");
      if (JS_IsFunction(ctx, write_method)) {
        JSValue result = JS_Call(ctx, write_method, dest, 1, &data);
        JS_FreeValue(ctx, result);
      }
      JS_FreeValue(ctx, write_method);

      // Emit 'data' event
      stream_emit(ctx, src, "data", 1, &data);

      JS_FreeValue(ctx, data);
    }
  }

  return dest;  // Return destination for chaining
}

// Readable.prototype.unpipe([destination])
static JSValue js_readable_unpipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* src = JS_GetOpaque(this_val, js_readable_class_id);
  if (!src) {
    return JS_ThrowTypeError(ctx, "Not a readable stream");
  }

  if (src->pipe_destinations == NULL || src->pipe_count == 0) {
    return JS_DupValue(ctx, this_val);  // No pipes to remove
  }

  // If no destination specified, unpipe all
  if (argc == 0 || JS_IsUndefined(argv[0])) {
    for (size_t i = 0; i < src->pipe_count; i++) {
      JSValue dest = src->pipe_destinations[i];
      stream_emit(ctx, src, "unpipe", 1, &dest);
      JS_FreeValue(ctx, dest);
    }
    src->pipe_count = 0;
  } else {
    // Unpipe specific destination
    JSValue dest_to_remove = argv[0];

    for (size_t i = 0; i < src->pipe_count; i++) {
      // Simple equality check (could be enhanced)
      if (JS_VALUE_GET_PTR(src->pipe_destinations[i]) == JS_VALUE_GET_PTR(dest_to_remove)) {
        JSValue dest = src->pipe_destinations[i];
        stream_emit(ctx, src, "unpipe", 1, &dest);
        JS_FreeValue(ctx, dest);

        // Shift remaining destinations
        for (size_t j = i + 1; j < src->pipe_count; j++) {
          src->pipe_destinations[j - 1] = src->pipe_destinations[j];
        }
        src->pipe_count--;
        break;
      }
    }
  }

  return JS_DupValue(ctx, this_val);  // Return this for chaining
}

// Initialize Readable prototype with all methods
void js_readable_init_prototype(JSContext* ctx, JSValue readable_proto) {
  JS_SetPropertyStr(ctx, readable_proto, "read", JS_NewCFunction(ctx, js_readable_read, "read", 1));
  JS_SetPropertyStr(ctx, readable_proto, "push", JS_NewCFunction(ctx, js_readable_push, "push", 2));
  JS_SetPropertyStr(ctx, readable_proto, "pause", JS_NewCFunction(ctx, js_readable_pause, "pause", 0));
  JS_SetPropertyStr(ctx, readable_proto, "resume", JS_NewCFunction(ctx, js_readable_resume, "resume", 0));
  JS_SetPropertyStr(ctx, readable_proto, "isPaused", JS_NewCFunction(ctx, js_readable_is_paused, "isPaused", 0));
  JS_SetPropertyStr(ctx, readable_proto, "setEncoding",
                    JS_NewCFunction(ctx, js_readable_set_encoding, "setEncoding", 1));
  JS_SetPropertyStr(ctx, readable_proto, "pipe", JS_NewCFunction(ctx, js_readable_pipe, "pipe", 2));
  JS_SetPropertyStr(ctx, readable_proto, "unpipe", JS_NewCFunction(ctx, js_readable_unpipe, "unpipe", 1));

  // Add readable property getter
  JSValue get_readable = JS_NewCFunction(ctx, js_readable_get_readable, "get readable", 0);
  JSAtom readable_atom = JS_NewAtom(ctx, "readable");
  JS_DefinePropertyGetSet(ctx, readable_proto, readable_atom, get_readable, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, readable_atom);
}
