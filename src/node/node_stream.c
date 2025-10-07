#include <stdlib.h>
#include <string.h>
#include "../util/debug.h"
#include "node_modules.h"

// Forward declare class IDs
static JSClassID js_readable_class_id;
static JSClassID js_writable_class_id;
static JSClassID js_transform_class_id;
static JSClassID js_passthrough_class_id;

// Forward declare stream options structure
typedef struct {
  int highWaterMark;
  bool objectMode;
  const char* encoding;
  const char* defaultEncoding;
  bool emitClose;
  bool autoDestroy;
} StreamOptions;

// Stream base class - all streams extend EventEmitter
typedef struct {
  JSValue event_emitter;  // EventEmitter instance (opaque)
  bool readable;
  bool writable;
  bool destroyed;
  bool ended;
  bool errored;
  JSValue error_value;  // Store error object when errored
  JSValue* buffered_data;
  size_t buffer_size;
  size_t buffer_capacity;
  StreamOptions options;  // Stream options

  // Phase 2: Readable stream state
  bool flowing;                // Flowing vs paused mode
  bool reading;                // Currently reading flag
  bool ended_emitted;          // 'end' event emitted flag
  bool readable_emitted;       // 'readable' event emitted flag
  JSValue* pipe_destinations;  // Array of piped destinations
  size_t pipe_count;
  size_t pipe_capacity;
} JSStreamData;

static void js_stream_finalizer(JSRuntime* rt, JSValue obj) {
  JSStreamData* stream = JS_GetOpaque(obj, js_readable_class_id);
  if (!stream) {
    stream = JS_GetOpaque(obj, js_writable_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(obj, js_transform_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(obj, js_passthrough_class_id);
  }

  if (stream) {
    // Free EventEmitter instance
    if (!JS_IsUndefined(stream->event_emitter)) {
      JS_FreeValueRT(rt, stream->event_emitter);
    }
    // Free error value if present
    if (!JS_IsUndefined(stream->error_value)) {
      JS_FreeValueRT(rt, stream->error_value);
    }
    // Free buffered data
    if (stream->buffered_data) {
      for (size_t i = 0; i < stream->buffer_size; i++) {
        JS_FreeValueRT(rt, stream->buffered_data[i]);
      }
      free(stream->buffered_data);
    }
    // Free pipe destinations
    if (stream->pipe_destinations) {
      for (size_t i = 0; i < stream->pipe_count; i++) {
        JS_FreeValueRT(rt, stream->pipe_destinations[i]);
      }
      free(stream->pipe_destinations);
    }
    // Free option strings if allocated
    // Note: For now, options.encoding and options.defaultEncoding are assumed
    // to be string literals or static. If they become dynamic, add free logic here.
    free(stream);
  }
}

static JSClassDef js_readable_class = {"Readable", .finalizer = js_stream_finalizer};
static JSClassDef js_writable_class = {"Writable", .finalizer = js_stream_finalizer};
static JSClassDef js_transform_class = {"Transform", .finalizer = js_stream_finalizer};
static JSClassDef js_passthrough_class = {"PassThrough", .finalizer = js_stream_finalizer};

// Helper: Parse options from constructor arguments
static void parse_stream_options(JSContext* ctx, JSValueConst options_obj, StreamOptions* opts) {
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

  // Parse highWaterMark
  JSValue hwm = JS_GetPropertyStr(ctx, options_obj, "highWaterMark");
  if (!JS_IsUndefined(hwm) && !JS_IsNull(hwm)) {
    int32_t value;
    if (JS_ToInt32(ctx, &value, hwm) == 0 && value >= 0) {
      opts->highWaterMark = value;
    }
  }
  JS_FreeValue(ctx, hwm);

  // Parse objectMode
  JSValue obj_mode = JS_GetPropertyStr(ctx, options_obj, "objectMode");
  if (JS_IsBool(obj_mode)) {
    opts->objectMode = JS_ToBool(ctx, obj_mode);
    if (opts->objectMode) {
      opts->highWaterMark = 16;  // Default 16 objects for object mode
    }
  }
  JS_FreeValue(ctx, obj_mode);

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
static JSValue init_stream_event_emitter(JSContext* ctx, JSValue stream_obj) {
  // Get EventEmitter constructor
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
static void stream_emit(JSContext* ctx, JSStreamData* stream, const char* event_name, int argc, JSValueConst* argv) {
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
static JSValue js_stream_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

static JSValue js_stream_once(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

static JSValue js_stream_emit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

static JSValue js_stream_off(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

static JSValue js_stream_remove_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

static JSValue js_stream_add_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

static JSValue js_stream_remove_all_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

static JSValue js_stream_listener_count(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

// Readable stream implementation
static JSValue js_readable_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
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

// Base method: stream.destroy([error])
static JSValue js_stream_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Try to get opaque data from any stream class
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_writable_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_transform_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_passthrough_class_id);
  }

  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a stream");
  }

  if (stream->destroyed) {
    return this_val;  // Already destroyed
  }

  stream->destroyed = true;

  // If error provided, store it and mark as errored
  if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    stream->errored = true;
    stream->error_value = JS_DupValue(ctx, argv[0]);
    // Emit error event
    stream_emit(ctx, stream, "error", 1, argv);
  }

  // Update destroyed property
  JS_SetPropertyStr(ctx, this_val, "destroyed", JS_NewBool(ctx, true));

  // Emit close event if emitClose option is true
  if (stream->options.emitClose) {
    stream_emit(ctx, stream, "close", 0, NULL);
  }

  return this_val;
}

// Property getter: stream.destroyed
static JSValue js_stream_get_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_writable_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_transform_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_passthrough_class_id);
  }

  if (!stream) {
    return JS_UNDEFINED;
  }

  return JS_NewBool(ctx, stream->destroyed);
}

// Property getter: stream.errored
static JSValue js_stream_get_errored(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_readable_class_id);
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_writable_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_transform_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_passthrough_class_id);
  }

  if (!stream) {
    return JS_NULL;
  }

  if (stream->errored && !JS_IsUndefined(stream->error_value)) {
    return JS_DupValue(ctx, stream->error_value);
  }

  return JS_NULL;
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

// Writable stream implementation
static JSValue js_writable_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
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

// PassThrough stream implementation (extends Transform)
static JSValue js_passthrough_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
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
  stream->readable = true;
  stream->writable = true;
  stream->destroyed = false;
  stream->ended = false;
  stream->errored = false;
  stream->error_value = JS_UNDEFINED;
  stream->buffer_capacity = 16;
  stream->buffered_data = malloc(sizeof(JSValue) * stream->buffer_capacity);

  // Initialize EventEmitter
  stream->event_emitter = init_stream_event_emitter(ctx, obj);

  JS_SetOpaque(obj, stream);

  // Set properties
  JS_DefinePropertyValueStr(ctx, obj, "readable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "writable", JS_NewBool(ctx, true), JS_PROP_WRITABLE);
  JS_DefinePropertyValueStr(ctx, obj, "destroyed", JS_NewBool(ctx, false), JS_PROP_WRITABLE);

  return obj;
}

// PassThrough.prototype.write (pass data through)
static JSValue js_passthrough_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_passthrough_class_id);
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
  JSStreamData* stream = JS_GetOpaque(this_val, js_passthrough_class_id);
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
  JSStreamData* stream = JS_GetOpaque(this_val, js_passthrough_class_id);
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

// Module initialization
JSValue JSRT_InitNodeStream(JSContext* ctx) {
  JSValue stream_module = JS_NewObject(ctx);

  // Register class IDs
  JS_NewClassID(&js_readable_class_id);
  JS_NewClassID(&js_writable_class_id);
  JS_NewClassID(&js_transform_class_id);
  JS_NewClassID(&js_passthrough_class_id);

  // Create class definitions
  JS_NewClass(JS_GetRuntime(ctx), js_readable_class_id, &js_readable_class);
  JS_NewClass(JS_GetRuntime(ctx), js_writable_class_id, &js_writable_class);
  JS_NewClass(JS_GetRuntime(ctx), js_transform_class_id, &js_transform_class);
  JS_NewClass(JS_GetRuntime(ctx), js_passthrough_class_id, &js_passthrough_class);

  // Create constructors
  JSValue readable_ctor = JS_NewCFunction2(ctx, js_readable_constructor, "Readable", 1, JS_CFUNC_constructor, 0);
  JSValue writable_ctor = JS_NewCFunction2(ctx, js_writable_constructor, "Writable", 1, JS_CFUNC_constructor, 0);
  JSValue passthrough_ctor =
      JS_NewCFunction2(ctx, js_passthrough_constructor, "PassThrough", 0, JS_CFUNC_constructor, 0);

  // Create prototypes
  JSValue readable_proto = JS_NewObject(ctx);
  JSValue writable_proto = JS_NewObject(ctx);
  JSValue passthrough_proto = JS_NewObject(ctx);

  // Add EventEmitter wrapper methods (common to all streams)
  JSValue on_method = JS_NewCFunction(ctx, js_stream_on, "on", 2);
  JSValue once_method = JS_NewCFunction(ctx, js_stream_once, "once", 2);
  JSValue emit_method = JS_NewCFunction(ctx, js_stream_emit, "emit", 1);
  JSValue off_method = JS_NewCFunction(ctx, js_stream_off, "off", 2);
  JSValue remove_listener_method = JS_NewCFunction(ctx, js_stream_remove_listener, "removeListener", 2);
  JSValue add_listener_method = JS_NewCFunction(ctx, js_stream_add_listener, "addListener", 2);
  JSValue remove_all_method = JS_NewCFunction(ctx, js_stream_remove_all_listeners, "removeAllListeners", 1);
  JSValue listener_count_method = JS_NewCFunction(ctx, js_stream_listener_count, "listenerCount", 1);

  // Add to readable prototype
  JS_SetPropertyStr(ctx, readable_proto, "on", JS_DupValue(ctx, on_method));
  JS_SetPropertyStr(ctx, readable_proto, "once", JS_DupValue(ctx, once_method));
  JS_SetPropertyStr(ctx, readable_proto, "emit", JS_DupValue(ctx, emit_method));
  JS_SetPropertyStr(ctx, readable_proto, "off", JS_DupValue(ctx, off_method));
  JS_SetPropertyStr(ctx, readable_proto, "removeListener", JS_DupValue(ctx, remove_listener_method));
  JS_SetPropertyStr(ctx, readable_proto, "addListener", JS_DupValue(ctx, add_listener_method));
  JS_SetPropertyStr(ctx, readable_proto, "removeAllListeners", JS_DupValue(ctx, remove_all_method));
  JS_SetPropertyStr(ctx, readable_proto, "listenerCount", JS_DupValue(ctx, listener_count_method));

  // Add to writable prototype
  JS_SetPropertyStr(ctx, writable_proto, "on", JS_DupValue(ctx, on_method));
  JS_SetPropertyStr(ctx, writable_proto, "once", JS_DupValue(ctx, once_method));
  JS_SetPropertyStr(ctx, writable_proto, "emit", JS_DupValue(ctx, emit_method));
  JS_SetPropertyStr(ctx, writable_proto, "off", JS_DupValue(ctx, off_method));
  JS_SetPropertyStr(ctx, writable_proto, "removeListener", JS_DupValue(ctx, remove_listener_method));
  JS_SetPropertyStr(ctx, writable_proto, "addListener", JS_DupValue(ctx, add_listener_method));
  JS_SetPropertyStr(ctx, writable_proto, "removeAllListeners", JS_DupValue(ctx, remove_all_method));
  JS_SetPropertyStr(ctx, writable_proto, "listenerCount", JS_DupValue(ctx, listener_count_method));

  // Add to passthrough prototype
  JS_SetPropertyStr(ctx, passthrough_proto, "on", on_method);
  JS_SetPropertyStr(ctx, passthrough_proto, "once", once_method);
  JS_SetPropertyStr(ctx, passthrough_proto, "emit", emit_method);
  JS_SetPropertyStr(ctx, passthrough_proto, "off", off_method);
  JS_SetPropertyStr(ctx, passthrough_proto, "removeListener", remove_listener_method);
  JS_SetPropertyStr(ctx, passthrough_proto, "addListener", add_listener_method);
  JS_SetPropertyStr(ctx, passthrough_proto, "removeAllListeners", remove_all_method);
  JS_SetPropertyStr(ctx, passthrough_proto, "listenerCount", listener_count_method);

  // Add base methods (common to all streams)
  JSValue destroy_method = JS_NewCFunction(ctx, js_stream_destroy, "destroy", 1);
  JS_SetPropertyStr(ctx, readable_proto, "destroy", JS_DupValue(ctx, destroy_method));
  JS_SetPropertyStr(ctx, writable_proto, "destroy", JS_DupValue(ctx, destroy_method));
  JS_SetPropertyStr(ctx, passthrough_proto, "destroy", destroy_method);

  // Add base property getters
  JSValue get_destroyed = JS_NewCFunction(ctx, js_stream_get_destroyed, "get destroyed", 0);
  JSValue get_errored = JS_NewCFunction(ctx, js_stream_get_errored, "get errored", 0);

  // Define destroyed property getter on all prototypes
  JSAtom destroyed_atom = JS_NewAtom(ctx, "destroyed");
  JS_DefinePropertyGetSet(ctx, readable_proto, destroyed_atom, JS_DupValue(ctx, get_destroyed), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, writable_proto, destroyed_atom, JS_DupValue(ctx, get_destroyed), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, passthrough_proto, destroyed_atom, get_destroyed, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, destroyed_atom);

  // Define errored property getter on all prototypes
  JSAtom errored_atom = JS_NewAtom(ctx, "errored");
  JS_DefinePropertyGetSet(ctx, readable_proto, errored_atom, JS_DupValue(ctx, get_errored), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, writable_proto, errored_atom, JS_DupValue(ctx, get_errored), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, passthrough_proto, errored_atom, get_errored, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, errored_atom);

  // Add methods to Readable prototype
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

  // Add methods to Writable prototype
  JS_SetPropertyStr(ctx, writable_proto, "write", JS_NewCFunction(ctx, js_writable_write, "write", 1));
  JS_SetPropertyStr(ctx, writable_proto, "end", JS_NewCFunction(ctx, js_writable_end, "end", 0));

  // Add methods to PassThrough prototype (inherits from both Readable and Writable)
  JS_SetPropertyStr(ctx, passthrough_proto, "read", JS_NewCFunction(ctx, js_passthrough_read, "read", 0));
  JS_SetPropertyStr(ctx, passthrough_proto, "push", JS_NewCFunction(ctx, js_passthrough_push, "push", 1));
  JS_SetPropertyStr(ctx, passthrough_proto, "write", JS_NewCFunction(ctx, js_passthrough_write, "write", 1));
  JS_SetPropertyStr(ctx, passthrough_proto, "end", JS_NewCFunction(ctx, js_writable_end, "end", 0));

  // Set prototypes
  JS_SetPropertyStr(ctx, readable_ctor, "prototype", readable_proto);
  JS_SetPropertyStr(ctx, writable_ctor, "prototype", writable_proto);
  JS_SetPropertyStr(ctx, passthrough_ctor, "prototype", passthrough_proto);

  // Set constructor property on prototypes
  JS_SetPropertyStr(ctx, readable_proto, "constructor", JS_DupValue(ctx, readable_ctor));
  JS_SetPropertyStr(ctx, writable_proto, "constructor", JS_DupValue(ctx, writable_ctor));
  JS_SetPropertyStr(ctx, passthrough_proto, "constructor", JS_DupValue(ctx, passthrough_ctor));

  // Set class prototypes
  JS_SetClassProto(ctx, js_readable_class_id, JS_DupValue(ctx, readable_proto));
  JS_SetClassProto(ctx, js_writable_class_id, JS_DupValue(ctx, writable_proto));
  JS_SetClassProto(ctx, js_passthrough_class_id, JS_DupValue(ctx, passthrough_proto));
  JS_SetClassProto(ctx, js_transform_class_id, JS_DupValue(ctx, passthrough_proto));

  // Add to module
  JS_SetPropertyStr(ctx, stream_module, "Readable", readable_ctor);
  JS_SetPropertyStr(ctx, stream_module, "Writable", writable_ctor);
  JS_SetPropertyStr(ctx, stream_module, "PassThrough", passthrough_ctor);

  // Transform is an alias for PassThrough in this basic implementation
  JS_SetPropertyStr(ctx, stream_module, "Transform", JS_DupValue(ctx, passthrough_ctor));

  return stream_module;
}

// ES Module support
int js_node_stream_init(JSContext* ctx, JSModuleDef* m) {
  JSValue stream_module = JSRT_InitNodeStream(ctx);

  // Export individual classes
  JS_SetModuleExport(ctx, m, "Readable", JS_GetPropertyStr(ctx, stream_module, "Readable"));
  JS_SetModuleExport(ctx, m, "Writable", JS_GetPropertyStr(ctx, stream_module, "Writable"));
  JS_SetModuleExport(ctx, m, "Transform", JS_GetPropertyStr(ctx, stream_module, "Transform"));
  JS_SetModuleExport(ctx, m, "PassThrough", JS_GetPropertyStr(ctx, stream_module, "PassThrough"));

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", stream_module);

  return 0;
}