#include <string.h>
#include "stream_internal.h"

// Helper: Parse options from constructor arguments
void parse_stream_options(JSContext* ctx, JSValueConst options_obj, StreamOptions* opts) {
  // Set defaults
  opts->highWaterMark = 16384;  // 16KB default for byte streams
  opts->objectMode = false;
  opts->encoding = NULL;
  opts->defaultEncoding = "utf8";
  opts->emitClose = true;
  opts->autoDestroy = true;

  if (JS_IsUndefined(options_obj) || JS_IsNull(options_obj)) {
    return;
  }

  // Parse objectMode first (affects highWaterMark default)
  JSValue obj_mode = JS_GetPropertyStr(ctx, options_obj, "objectMode");
  if (JS_IsBool(obj_mode)) {
    opts->objectMode = JS_ToBool(ctx, obj_mode);
    if (opts->objectMode) {
      opts->highWaterMark = 16;  // Default 16 objects for object mode
    }
  }
  JS_FreeValue(ctx, obj_mode);

  // Parse highWaterMark (overrides objectMode default if explicitly provided)
  JSValue hwm = JS_GetPropertyStr(ctx, options_obj, "highWaterMark");
  if (!JS_IsUndefined(hwm) && !JS_IsNull(hwm)) {
    int32_t value;
    if (JS_ToInt32(ctx, &value, hwm) == 0 && value >= 0) {
      opts->highWaterMark = value;
    }
  }
  JS_FreeValue(ctx, hwm);

  // Parse encoding
  JSValue enc = JS_GetPropertyStr(ctx, options_obj, "encoding");
  if (!JS_IsUndefined(enc) && !JS_IsNull(enc)) {
    const char* enc_str = JS_ToCString(ctx, enc);
    if (enc_str) {
      opts->encoding = enc_str;  // TODO: Consider strdup for ownership
    }
  }
  JS_FreeValue(ctx, enc);

  // Parse defaultEncoding
  JSValue def_enc = JS_GetPropertyStr(ctx, options_obj, "defaultEncoding");
  if (!JS_IsUndefined(def_enc) && !JS_IsNull(def_enc)) {
    const char* def_enc_str = JS_ToCString(ctx, def_enc);
    if (def_enc_str) {
      opts->defaultEncoding = def_enc_str;  // TODO: Consider strdup for ownership
    }
  }
  JS_FreeValue(ctx, def_enc);

  // Parse emitClose
  JSValue emit_close = JS_GetPropertyStr(ctx, options_obj, "emitClose");
  if (JS_IsBool(emit_close)) {
    opts->emitClose = JS_ToBool(ctx, emit_close);
  }
  JS_FreeValue(ctx, emit_close);

  // Parse autoDestroy
  JSValue auto_destroy = JS_GetPropertyStr(ctx, options_obj, "autoDestroy");
  if (JS_IsBool(auto_destroy)) {
    opts->autoDestroy = JS_ToBool(ctx, auto_destroy);
  }
  JS_FreeValue(ctx, auto_destroy);
}

// Helper: Initialize EventEmitter for a stream
JSValue init_stream_event_emitter(JSContext* ctx, JSValue stream_obj) {
  // Get EventEmitter constructor from global object
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue emitter_ctor = JS_GetPropertyStr(ctx, global, "EventEmitter");
  JS_FreeValue(ctx, global);

  if (JS_IsException(emitter_ctor) || JS_IsUndefined(emitter_ctor)) {
    JS_FreeValue(ctx, emitter_ctor);
    return JS_UNDEFINED;
  }

  // Create EventEmitter instance
  JSValue emitter = JS_CallConstructor(ctx, emitter_ctor, 0, NULL);
  JS_FreeValue(ctx, emitter_ctor);

  if (JS_IsException(emitter)) {
    return JS_UNDEFINED;
  }

  // Store emitter as an internal property
  JS_SetPropertyStr(ctx, stream_obj, "_emitter", JS_DupValue(ctx, emitter));

  return emitter;
}

// Helper: Emit an event on stream
void stream_emit(JSContext* ctx, JSStreamData* stream, const char* event_name, int argc, JSValueConst* argv) {
  if (JS_IsUndefined(stream->event_emitter)) {
    return;
  }

  JSValue emit_method = JS_GetPropertyStr(ctx, stream->event_emitter, "emit");
  if (JS_IsException(emit_method) || JS_IsUndefined(emit_method)) {
    JS_FreeValue(ctx, emit_method);
    return;
  }

  // Build arguments array: [event_name, ...argv]
  JSValue* args = malloc(sizeof(JSValue) * (argc + 1));
  args[0] = JS_NewString(ctx, event_name);
  for (int i = 0; i < argc; i++) {
    args[i + 1] = JS_DupValue(ctx, argv[i]);
  }

  JSValue result = JS_Call(ctx, emit_method, stream->event_emitter, argc + 1, args);

  // Cleanup
  for (int i = 0; i < argc + 1; i++) {
    JS_FreeValue(ctx, args[i]);
  }
  free(args);
  JS_FreeValue(ctx, emit_method);
  JS_FreeValue(ctx, result);
}

// Wrapper methods for EventEmitter on streams
JSValue js_stream_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue emitter = JS_GetPropertyStr(ctx, this_val, "_emitter");
  if (JS_IsUndefined(emitter)) {
    JS_FreeValue(ctx, emitter);
    return JS_ThrowTypeError(ctx, "Stream has no EventEmitter");
  }

  JSValue on_method = JS_GetPropertyStr(ctx, emitter, "on");
  JSValue result = JS_Call(ctx, on_method, emitter, argc, argv);
  JS_FreeValue(ctx, on_method);
  JS_FreeValue(ctx, emitter);

  // Return this for chaining
  if (!JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    return JS_DupValue(ctx, this_val);
  }
  return result;
}

JSValue js_stream_once(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue emitter = JS_GetPropertyStr(ctx, this_val, "_emitter");
  if (JS_IsUndefined(emitter)) {
    JS_FreeValue(ctx, emitter);
    return JS_ThrowTypeError(ctx, "Stream has no EventEmitter");
  }

  JSValue once_method = JS_GetPropertyStr(ctx, emitter, "once");
  JSValue result = JS_Call(ctx, once_method, emitter, argc, argv);
  JS_FreeValue(ctx, once_method);
  JS_FreeValue(ctx, emitter);

  if (!JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    return JS_DupValue(ctx, this_val);
  }
  return result;
}

JSValue js_stream_emit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue emitter = JS_GetPropertyStr(ctx, this_val, "_emitter");
  if (JS_IsUndefined(emitter)) {
    JS_FreeValue(ctx, emitter);
    return JS_ThrowTypeError(ctx, "Stream has no EventEmitter");
  }

  JSValue emit_method = JS_GetPropertyStr(ctx, emitter, "emit");
  JSValue result = JS_Call(ctx, emit_method, emitter, argc, argv);
  JS_FreeValue(ctx, emit_method);
  JS_FreeValue(ctx, emitter);
  return result;
}

JSValue js_stream_off(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue emitter = JS_GetPropertyStr(ctx, this_val, "_emitter");
  if (JS_IsUndefined(emitter)) {
    JS_FreeValue(ctx, emitter);
    return JS_ThrowTypeError(ctx, "Stream has no EventEmitter");
  }

  JSValue off_method = JS_GetPropertyStr(ctx, emitter, "off");
  JSValue result = JS_Call(ctx, off_method, emitter, argc, argv);
  JS_FreeValue(ctx, off_method);
  JS_FreeValue(ctx, emitter);

  if (!JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    return JS_DupValue(ctx, this_val);
  }
  return result;
}

JSValue js_stream_remove_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue emitter = JS_GetPropertyStr(ctx, this_val, "_emitter");
  if (JS_IsUndefined(emitter)) {
    JS_FreeValue(ctx, emitter);
    return JS_ThrowTypeError(ctx, "Stream has no EventEmitter");
  }

  JSValue method = JS_GetPropertyStr(ctx, emitter, "removeListener");
  JSValue result = JS_Call(ctx, method, emitter, argc, argv);
  JS_FreeValue(ctx, method);
  JS_FreeValue(ctx, emitter);

  if (!JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    return JS_DupValue(ctx, this_val);
  }
  return result;
}

JSValue js_stream_add_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue emitter = JS_GetPropertyStr(ctx, this_val, "_emitter");
  if (JS_IsUndefined(emitter)) {
    JS_FreeValue(ctx, emitter);
    return JS_ThrowTypeError(ctx, "Stream has no EventEmitter");
  }

  JSValue method = JS_GetPropertyStr(ctx, emitter, "addListener");
  JSValue result = JS_Call(ctx, method, emitter, argc, argv);
  JS_FreeValue(ctx, method);
  JS_FreeValue(ctx, emitter);

  if (!JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    return JS_DupValue(ctx, this_val);
  }
  return result;
}

JSValue js_stream_remove_all_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue emitter = JS_GetPropertyStr(ctx, this_val, "_emitter");
  if (JS_IsUndefined(emitter)) {
    JS_FreeValue(ctx, emitter);
    return JS_ThrowTypeError(ctx, "Stream has no EventEmitter");
  }

  JSValue method = JS_GetPropertyStr(ctx, emitter, "removeAllListeners");
  JSValue result = JS_Call(ctx, method, emitter, argc, argv);
  JS_FreeValue(ctx, method);
  JS_FreeValue(ctx, emitter);

  if (!JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    return JS_DupValue(ctx, this_val);
  }
  return result;
}

JSValue js_stream_listener_count(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue emitter = JS_GetPropertyStr(ctx, this_val, "_emitter");
  if (JS_IsUndefined(emitter)) {
    JS_FreeValue(ctx, emitter);
    return JS_ThrowTypeError(ctx, "Stream has no EventEmitter");
  }

  JSValue method = JS_GetPropertyStr(ctx, emitter, "listenerCount");
  JSValue result = JS_Call(ctx, method, emitter, argc, argv);
  JS_FreeValue(ctx, method);
  JS_FreeValue(ctx, emitter);
  return result;
}
