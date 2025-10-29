#include "../../util/debug.h"
#include "stream_internal.h"

// Duplex stream implementation - combines Readable and Writable
// Reuses patterns from streams.c (lines 279-283, 988-994) and Phase 2/3

JSValue js_duplex_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_duplex_class_id);
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

  // Parse allowHalfOpen option (default: true)
  bool allow_half_open = true;
  if (argc > 0 && JS_IsObject(argv[0])) {
    JSValue allow_half_open_opt = JS_GetPropertyStr(ctx, argv[0], "allowHalfOpen");
    if (JS_IsBool(allow_half_open_opt)) {
      allow_half_open = JS_ToBool(ctx, allow_half_open_opt);
    }
    JS_FreeValue(ctx, allow_half_open_opt);
  }

  // Initialize base state (both readable and writable)
  stream->magic = JS_STREAM_MAGIC;  // Set magic number for validation
  stream->readable = true;
  stream->writable = true;
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

  // Initialize Phase 3: Writable state
  stream->writable_ended = false;
  stream->writable_finished = false;
  stream->writable_corked = 0;
  stream->need_drain = false;
  stream->write_callbacks = NULL;
  stream->write_callback_count = 0;
  stream->write_callback_capacity = 0;

  // Set opaque BEFORE initializing EventEmitter
  JS_SetOpaque(obj, stream);

  // Initialize EventEmitter (stored as "_emitter" property)
  init_stream_event_emitter(ctx, obj);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "readable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "writable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  // Store allowHalfOpen as property
  JS_DefinePropertyValueStr(ctx, obj, "_allowHalfOpen", JS_NewBool(ctx, allow_half_open), JS_PROP_WRITABLE);

  return obj;
}

// Duplex.prototype.read - reuses Readable.read() logic
static JSValue js_duplex_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_duplex_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a duplex stream");
  }

  // Same logic as Readable.read()
  if (stream->ended && stream->buffer_size == 0) {
    return JS_NULL;
  }

  int32_t size = -1;
  if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    if (JS_ToInt32(ctx, &size, argv[0]) < 0) {
      return JS_EXCEPTION;
    }
  }

  if (stream->buffer_size == 0) {
    stream->reading = true;

    if (stream->ended && !stream->ended_emitted) {
      stream->ended_emitted = true;
      stream_emit(ctx, this_val, "end", 0, NULL);

      // If allowHalfOpen is false, end the writable side too
      JSValue allow_half_open = JS_GetPropertyStr(ctx, this_val, "_allowHalfOpen");
      bool half_open = JS_ToBool(ctx, allow_half_open);
      JS_FreeValue(ctx, allow_half_open);

      if (!half_open && stream->writable && !stream->writable_ended) {
        stream->writable = false;
        stream->writable_ended = true;
        JS_SetPropertyStr(ctx, this_val, "writable", JS_NewBool(ctx, false));
        stream_emit(ctx, this_val, "finish", 0, NULL);
      }
    }

    return JS_NULL;
  }

  JSValue data = stream->buffered_data[0];

  for (size_t i = 1; i < stream->buffer_size; i++) {
    stream->buffered_data[i - 1] = stream->buffered_data[i];
  }
  stream->buffer_size--;

  stream->readable_emitted = false;

  if (stream->ended && stream->buffer_size == 0 && !stream->ended_emitted) {
    stream->ended_emitted = true;
    stream_emit(ctx, this_val, "end", 0, NULL);

    // Check allowHalfOpen for writable side
    JSValue allow_half_open = JS_GetPropertyStr(ctx, this_val, "_allowHalfOpen");
    bool half_open = JS_ToBool(ctx, allow_half_open);
    JS_FreeValue(ctx, allow_half_open);

    if (!half_open && stream->writable && !stream->writable_ended) {
      stream->writable = false;
      stream->writable_ended = true;
      JS_SetPropertyStr(ctx, this_val, "writable", JS_NewBool(ctx, false));
      stream_emit(ctx, this_val, "finish", 0, NULL);
    }
  }

  return data;
}

// Duplex.prototype.write - reuses Writable.write() logic
static JSValue js_duplex_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_duplex_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a duplex stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  if (!stream->writable || stream->writable_ended) {
    return JS_ThrowTypeError(ctx, "Cannot write after end");
  }

  JSValue chunk = argv[0];
  JSValue callback = JS_UNDEFINED;

  // Parse callback (last argument if it's a function)
  if (argc >= 2 && JS_IsFunction(ctx, argv[argc - 1])) {
    callback = argv[argc - 1];
  }

  // If corked, just buffer the data (don't emit yet)
  // For now, simplified: just check highWaterMark for backpressure

  // Store callback if provided
  if (JS_IsFunction(ctx, callback)) {
    if (stream->write_callbacks == NULL) {
      stream->write_callback_capacity = 4;
      stream->write_callbacks = malloc(sizeof(WriteCallback) * stream->write_callback_capacity);
    } else if (stream->write_callback_count >= stream->write_callback_capacity) {
      stream->write_callback_capacity *= 2;
      stream->write_callbacks =
          realloc(stream->write_callbacks, sizeof(WriteCallback) * stream->write_callback_capacity);
    }
    stream->write_callbacks[stream->write_callback_count].callback = JS_DupValue(ctx, callback);
    stream->write_callback_count++;

    // Call callback immediately (simplified - should be async)
    JSValue result = JS_Call(ctx, callback, this_val, 0, NULL);
    if (!JS_IsException(result)) {
      JS_FreeValue(ctx, result);
    }
  }

  // Check backpressure
  int highWaterMark = stream->options.highWaterMark;
  bool backpressure = (stream->buffer_size >= (size_t)highWaterMark);

  if (backpressure && !stream->need_drain) {
    stream->need_drain = true;
  }

  return JS_NewBool(ctx, !backpressure);
}

// Initialize Duplex prototype with methods from both Readable and Writable
void js_duplex_init_prototype(JSContext* ctx, JSValue duplex_proto) {
  // Readable methods
  JS_SetPropertyStr(ctx, duplex_proto, "read", JS_NewCFunction(ctx, js_duplex_read, "read", 1));
  JS_SetPropertyStr(ctx, duplex_proto, "push", JS_NewCFunction(ctx, js_duplex_read, "push", 2));  // Simplified

  // Writable methods
  JS_SetPropertyStr(ctx, duplex_proto, "write", JS_NewCFunction(ctx, js_duplex_write, "write", 3));
  // end() is shared, added in stream.c

  // Note: Full implementation would include all Readable/Writable methods
  // pause(), resume(), pipe(), unpipe(), cork(), uncork(), etc.
}
