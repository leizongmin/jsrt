#include "../../util/debug.h"
#include "stream_internal.h"

// Transform stream implementation - extends Duplex with transformation logic
// Reuses Duplex patterns and adds _transform() callback support

JSValue js_transform_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_transform_class_id);
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

  // Initialize as Duplex (both readable and writable)
  stream->readable = true;
  stream->writable = true;
  stream->destroyed = false;
  stream->ended = false;
  stream->errored = false;
  stream->error_value = JS_UNDEFINED;
  stream->buffer_capacity = 16;
  stream->buffered_data = malloc(sizeof(JSValue) * stream->buffer_capacity);

  // Initialize Readable state
  stream->flowing = false;
  stream->reading = false;
  stream->ended_emitted = false;
  stream->readable_emitted = false;
  stream->pipe_destinations = NULL;
  stream->pipe_count = 0;
  stream->pipe_capacity = 0;

  // Initialize Writable state
  stream->writable_ended = false;
  stream->writable_finished = false;
  stream->writable_corked = 0;
  stream->need_drain = false;
  stream->write_callbacks = NULL;
  stream->write_callback_count = 0;
  stream->write_callback_capacity = 0;

  JS_SetOpaque(obj, stream);

  // Initialize EventEmitter
  init_stream_event_emitter(ctx, obj);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "readable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "writable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  // Store transform callback if provided
  if (argc > 0 && JS_IsObject(argv[0])) {
    JSValue transform_fn = JS_GetPropertyStr(ctx, argv[0], "transform");
    if (JS_IsFunction(ctx, transform_fn)) {
      JS_SetPropertyStr(ctx, obj, "_transform", transform_fn);
    } else {
      JS_FreeValue(ctx, transform_fn);
    }

    JSValue flush_fn = JS_GetPropertyStr(ctx, argv[0], "flush");
    if (JS_IsFunction(ctx, flush_fn)) {
      JS_SetPropertyStr(ctx, obj, "_flush", flush_fn);
    } else {
      JS_FreeValue(ctx, flush_fn);
    }
  }

  return obj;
}

// Transform.prototype._transform - default passthrough
// Subclasses should override this
static JSValue js_transform_default_transform(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Default implementation: just push the chunk through
  if (argc < 3) {
    return JS_UNDEFINED;
  }

  JSValue chunk = argv[0];
  // JSValue encoding = argv[1];  // Unused in default
  JSValue callback = argv[2];

  // Call this.push(chunk)
  JSValue push_fn = JS_GetPropertyStr(ctx, this_val, "push");
  if (JS_IsFunction(ctx, push_fn)) {
    JSValue result = JS_Call(ctx, push_fn, this_val, 1, &chunk);
    JS_FreeValue(ctx, result);
  }
  JS_FreeValue(ctx, push_fn);

  // Call callback()
  if (JS_IsFunction(ctx, callback)) {
    JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, result);
  }

  return JS_UNDEFINED;
}

// Transform.prototype.write - calls _transform()
static JSValue js_transform_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_transform_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a transform stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  if (!stream->writable || stream->writable_ended) {
    return JS_ThrowTypeError(ctx, "Cannot write after end");
  }

  JSValue chunk = argv[0];
  JSValue encoding = argc > 1 && !JS_IsFunction(ctx, argv[1]) ? argv[1] : JS_NewString(ctx, "utf8");
  JSValue callback = JS_UNDEFINED;

  // Parse callback (last argument if it's a function)
  if (argc >= 2 && JS_IsFunction(ctx, argv[argc - 1])) {
    callback = argv[argc - 1];
  }

  // Get _transform method
  JSValue transform_fn = JS_GetPropertyStr(ctx, this_val, "_transform");
  if (!JS_IsFunction(ctx, transform_fn)) {
    // Use default transform if not provided
    JS_FreeValue(ctx, transform_fn);
    transform_fn = JS_NewCFunction(ctx, js_transform_default_transform, "_transform", 3);
  }

  // Create callback that will be passed to _transform
  // For now, use the provided callback or a no-op
  JSValue transform_callback = JS_IsFunction(ctx, callback) ? callback : JS_NewCFunction(ctx, NULL, "callback", 0);

  // Call _transform(chunk, encoding, callback)
  JSValue transform_args[] = {chunk, encoding, transform_callback};
  JSValue result = JS_Call(ctx, transform_fn, this_val, 3, transform_args);

  JS_FreeValue(ctx, transform_fn);
  if (argc <= 1 || JS_IsFunction(ctx, argv[1])) {
    JS_FreeValue(ctx, encoding);
  }
  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeValue(ctx, transform_callback);
  }

  if (JS_IsException(result)) {
    return result;
  }
  JS_FreeValue(ctx, result);

  // Check backpressure
  int highWaterMark = stream->options.highWaterMark;
  bool backpressure = (stream->buffer_size >= (size_t)highWaterMark);

  return JS_NewBool(ctx, !backpressure);
}

// Transform.prototype.read - reuses Duplex.read() logic
static JSValue js_transform_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_transform_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a transform stream");
  }

  // Same logic as Duplex/Readable.read()
  if (stream->ended && stream->buffer_size == 0) {
    return JS_NULL;
  }

  if (stream->buffer_size == 0) {
    stream->reading = true;

    if (stream->ended && !stream->ended_emitted) {
      stream->ended_emitted = true;
      stream_emit(ctx, this_val, "end", 0, NULL);
    }

    return JS_NULL;
  }

  JSValue data = stream->buffered_data[0];

  for (size_t i = 1; i < stream->buffer_size; i++) {
    stream->buffered_data[i - 1] = stream->buffered_data[i];
  }
  stream->buffer_size--;

  return data;
}

// Transform.prototype.push - adds transformed data to readable side
static JSValue js_transform_push(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_transform_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a transform stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  JSValue chunk = argv[0];

  // If push(null), end the readable stream
  if (JS_IsNull(chunk)) {
    stream->ended = true;
    JS_SetPropertyStr(ctx, this_val, "readable", JS_NewBool(ctx, false));

    if (!stream->ended_emitted && stream->buffer_size == 0) {
      stream->ended_emitted = true;
      stream_emit(ctx, this_val, "end", 0, NULL);
    }

    return JS_NewBool(ctx, false);
  }

  // Add to buffer
  if (stream->buffer_size >= stream->buffer_capacity) {
    stream->buffer_capacity *= 2;
    stream->buffered_data = realloc(stream->buffered_data, sizeof(JSValue) * stream->buffer_capacity);
  }

  stream->buffered_data[stream->buffer_size++] = JS_DupValue(ctx, chunk);

  // If in flowing mode, emit 'data' event
  if (stream->flowing) {
    while (stream->buffer_size > 0 && stream->flowing) {
      JSValue data = stream->buffered_data[0];

      for (size_t i = 1; i < stream->buffer_size; i++) {
        stream->buffered_data[i - 1] = stream->buffered_data[i];
      }
      stream->buffer_size--;

      stream_emit(ctx, this_val, "data", 1, &data);
      JS_FreeValue(ctx, data);
    }
  } else {
    // In paused mode, emit 'readable' event
    if (!stream->readable_emitted && stream->buffer_size > 0) {
      stream->readable_emitted = true;
      stream_emit(ctx, this_val, "readable", 0, NULL);
    }
  }

  // Return backpressure signal
  int highWaterMark = stream->options.highWaterMark;
  bool backpressure = (stream->buffer_size >= (size_t)highWaterMark);

  return JS_NewBool(ctx, !backpressure);
}

// Initialize Transform prototype
void js_transform_init_prototype(JSContext* ctx, JSValue transform_proto) {
  JS_SetPropertyStr(ctx, transform_proto, "read", JS_NewCFunction(ctx, js_transform_read, "read", 1));
  JS_SetPropertyStr(ctx, transform_proto, "write", JS_NewCFunction(ctx, js_transform_write, "write", 3));
  JS_SetPropertyStr(ctx, transform_proto, "push", JS_NewCFunction(ctx, js_transform_push, "push", 2));
  JS_SetPropertyStr(ctx, transform_proto, "_transform",
                    JS_NewCFunction(ctx, js_transform_default_transform, "_transform", 3));
  // end() is shared, added in stream.c
}
