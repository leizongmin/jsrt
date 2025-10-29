#include "stream_internal.h"

// PassThrough stream implementation (extends Transform)
JSValue js_passthrough_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_passthrough_class_id);
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
  stream->magic = JS_STREAM_MAGIC;  // Set magic number for validation
  stream->readable = true;
  stream->writable = true;
  stream->destroyed = false;
  stream->ended = false;
  stream->errored = false;
  stream->error_value = JS_UNDEFINED;
  stream->buffer_capacity = 16;
  stream->buffered_data = malloc(sizeof(JSValue) * stream->buffer_capacity);

  // Initialize EventEmitter (stored as "_emitter" property, not in struct)
  init_stream_event_emitter(ctx, obj);

  JS_SetOpaque(obj, stream);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "readable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "writable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  return obj;
}

// PassThrough.prototype.write (pass data through)
static JSValue js_passthrough_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_passthrough_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a passthrough stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  if (!stream->writable) {
    return JS_ThrowTypeError(ctx, "Cannot write to stream");
  }

  // In PassThrough, written data becomes readable
  JSValue chunk = argv[0];

  // Add to buffer
  if (stream->buffer_size >= stream->buffer_capacity) {
    stream->buffer_capacity *= 2;
    stream->buffered_data = realloc(stream->buffered_data, sizeof(JSValue) * stream->buffer_capacity);
  }

  stream->buffered_data[stream->buffer_size++] = JS_DupValue(ctx, chunk);

  return JS_NewBool(ctx, true);
}

// PassThrough.prototype.read (use same as readable)
static JSValue js_passthrough_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_passthrough_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a passthrough stream");
  }

  if (stream->buffer_size > 0) {
    JSValue data = stream->buffered_data[0];
    // Shift buffer
    for (size_t i = 1; i < stream->buffer_size; i++) {
      stream->buffered_data[i - 1] = stream->buffered_data[i];
    }
    stream->buffer_size--;
    return data;
  }

  return JS_NULL;
}

// PassThrough.prototype.push (use same as readable)
static JSValue js_passthrough_push(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_passthrough_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a passthrough stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  JSValue chunk = argv[0];

  // If push(null), end the stream
  if (JS_IsNull(chunk)) {
    stream->ended = true;
    JS_SetPropertyStr(ctx, this_val, "readable", JS_NewBool(ctx, false));
    return JS_NewBool(ctx, false);
  }

  // Add to buffer
  if (stream->buffer_size >= stream->buffer_capacity) {
    stream->buffer_capacity *= 2;
    stream->buffered_data = realloc(stream->buffered_data, sizeof(JSValue) * stream->buffer_capacity);
  }

  stream->buffered_data[stream->buffer_size++] = JS_DupValue(ctx, chunk);

  return JS_NewBool(ctx, true);
}

// Initialize PassThrough prototype with all methods
void js_passthrough_init_prototype(JSContext* ctx, JSValue passthrough_proto) {
  JS_SetPropertyStr(ctx, passthrough_proto, "read", JS_NewCFunction(ctx, js_passthrough_read, "read", 0));
  JS_SetPropertyStr(ctx, passthrough_proto, "push", JS_NewCFunction(ctx, js_passthrough_push, "push", 1));
  JS_SetPropertyStr(ctx, passthrough_proto, "write", JS_NewCFunction(ctx, js_passthrough_write, "write", 1));
  // Note: end() is shared with Writable, added in stream.c
}
