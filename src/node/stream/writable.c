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

  // Initialize EventEmitter
  stream->event_emitter = init_stream_event_emitter(ctx, obj);

  JS_SetOpaque(obj, stream);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "writable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  return obj;
}

// Writable.prototype.write
static JSValue js_writable_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  if (argc < 1) {
    return JS_NewBool(ctx, false);
  }

  if (!stream->writable) {
    return JS_ThrowTypeError(ctx, "Cannot write to stream");
  }

  return JS_NewBool(ctx, true);
}

// Writable.prototype.end
static JSValue js_writable_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  stream->writable = false;
  stream->ended = true;
  JS_SetPropertyStr(ctx, this_val, "writable", JS_NewBool(ctx, false));

  return JS_UNDEFINED;
}

// Initialize Writable prototype with all methods
void js_writable_init_prototype(JSContext* ctx, JSValue writable_proto) {
  JS_SetPropertyStr(ctx, writable_proto, "write", JS_NewCFunction(ctx, js_writable_write, "write", 1));
  JS_SetPropertyStr(ctx, writable_proto, "end", JS_NewCFunction(ctx, js_writable_end, "end", 0));
}
