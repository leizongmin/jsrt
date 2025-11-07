#include <stdlib.h>
#include <string.h>
#include <strings.h>  // For memset
#include "../../util/debug.h"
#include "../node_modules.h"
#include "stream_internal.h"

// Define class IDs (not static - shared across modules)
JSClassID js_readable_class_id;
JSClassID js_writable_class_id;
JSClassID js_duplex_class_id;
JSClassID js_transform_class_id;
JSClassID js_passthrough_class_id;
JSClassID js_stream_class_id;

static JSAtom stream_impl_atom = JS_ATOM_NULL;

static JSStreamData* js_stream_try_get(JSValueConst obj, JSClassID class_id) {
  if (!JS_IsObject(obj))
    return NULL;
  return JS_GetOpaque(obj, class_id);
}

JSValue js_stream_get_impl_holder(JSContext* ctx, JSValueConst this_val, JSClassID class_id) {
  (void)class_id;
  if (stream_impl_atom == JS_ATOM_NULL || !JS_IsObject(this_val))
    return JS_UNDEFINED;

  JSValue holder = JS_GetProperty(ctx, this_val, stream_impl_atom);
  if (JS_IsException(holder))
    return holder;

  if (JS_IsUndefined(holder) || JS_IsNull(holder)) {
    JS_FreeValue(ctx, holder);
    return JS_UNDEFINED;
  }

  return holder;
}

JSStreamData* js_stream_get_data(JSContext* ctx, JSValueConst this_val, JSClassID class_id) {
  JSStreamData* stream = js_stream_try_get(this_val, class_id);
  if (stream) {
    return stream->magic == JS_STREAM_MAGIC ? stream : NULL;
  }

  JSValue holder = js_stream_get_impl_holder(ctx, this_val, class_id);
  if (JS_IsException(holder))
    return NULL;

  if (JS_IsUndefined(holder))
    return NULL;

  stream = js_stream_try_get(holder, class_id);
  if (!stream || stream->magic != JS_STREAM_MAGIC) {
    JS_FreeValue(ctx, holder);
    return NULL;
  }

  JS_FreeValue(ctx, holder);
  return stream;
}

int js_stream_attach_impl(JSContext* ctx, JSValueConst public_obj, JSValue holder) {
  if (stream_impl_atom == JS_ATOM_NULL) {
    stream_impl_atom = JS_NewAtom(ctx, "__jsrt_stream_impl");
  }

  int ret = JS_DefinePropertyValue(ctx, public_obj, stream_impl_atom, holder, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);
  return ret;
}

// Finalizer for all stream types
static void js_stream_finalizer(JSRuntime* rt, JSValue obj) {
  JSStreamData* stream = JS_GetOpaque(obj, js_stream_class_id);
  if (!stream) {
    stream = JS_GetOpaque(obj, js_readable_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(obj, js_writable_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(obj, js_duplex_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(obj, js_transform_class_id);
  }
  if (!stream) {
    stream = JS_GetOpaque(obj, js_passthrough_class_id);
  }

  if (stream) {
    if (stream->magic != JS_STREAM_MAGIC) {
      free(stream);
      return;
    }

    // Note: We do NOT free event_emitter here because it's stored as the "_emitter"
    // property on the stream object and will be freed automatically when the object
    // properties are cleaned up. Freeing it here would cause a double-free.

    // Free error value if present
    if (!JS_IsUndefined(stream->error_value)) {
      JS_FreeValueRT(rt, stream->error_value);
    }

    // Note: Encoding strings (stream->options.encoding, stream->options.defaultEncoding)
    // are not freed here because they may be static strings or shared across calls.
    // If dynamic allocation is added later, proper cleanup should be implemented here.

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
    // Free write callbacks (Phase 3)
    if (stream->write_callbacks) {
      for (size_t i = 0; i < stream->write_callback_count; i++) {
        if (!JS_IsUndefined(stream->write_callbacks[i].callback)) {
          JS_FreeValueRT(rt, stream->write_callbacks[i].callback);
        }
      }
      free(stream->write_callbacks);
    }
    // Free option strings if allocated
    // Note: For now, options.encoding and options.defaultEncoding are assumed
    // to be string literals or static. If they become dynamic, add free logic here.
    stream->magic = 0;
    free(stream);
  }
}

// Class definitions
static JSClassDef js_stream_class = {"Stream", .finalizer = js_stream_finalizer};
static JSClassDef js_readable_class = {"Readable", .finalizer = js_stream_finalizer};
static JSClassDef js_writable_class = {"Writable", .finalizer = js_stream_finalizer};
static JSClassDef js_duplex_class = {"Duplex", .finalizer = js_stream_finalizer};
static JSClassDef js_transform_class = {"Transform", .finalizer = js_stream_finalizer};
static JSClassDef js_passthrough_class = {"PassThrough", .finalizer = js_stream_finalizer};

// Base method: stream.destroy([error])
JSValue js_stream_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_stream_class_id);
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_readable_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_writable_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_duplex_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_transform_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_passthrough_class_id);
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
    stream_emit(ctx, this_val, "error", 1, argv);
  }

  // Update destroyed property
  JSValue destroyed_val = JS_NewBool(ctx, true);
  int ret = JS_SetPropertyStr(ctx, this_val, "destroyed", destroyed_val);
  if (ret < 0) {
    return JS_EXCEPTION;
  }

  // Emit close event if emitClose option is true
  if (stream->options.emitClose) {
    stream_emit(ctx, this_val, "close", 0, NULL);
  }

  return JS_DupValue(ctx, this_val);
}

// Property getter: stream.destroyed
JSValue js_stream_get_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_stream_class_id);
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_readable_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_writable_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_duplex_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_transform_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_passthrough_class_id);
  }

  if (!stream) {
    return JS_UNDEFINED;
  }

  return JS_NewBool(ctx, stream->destroyed);
}

// Property getter: stream.errored
JSValue js_stream_get_errored(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_stream_class_id);
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_readable_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_writable_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_duplex_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_transform_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_passthrough_class_id);
  }

  if (!stream) {
    return JS_NULL;
  }

  if (stream->errored && !JS_IsUndefined(stream->error_value)) {
    return JS_DupValue(ctx, stream->error_value);
  }

  return JS_NULL;
}

// Writable.prototype.end - shared with PassThrough, Duplex, Transform
static JSValue js_writable_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_writable_class_id);
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_duplex_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_transform_class_id);
  }
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_passthrough_class_id);
  }
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  stream->writable = false;
  stream->ended = true;
  JS_SetPropertyStr(ctx, this_val, "writable", JS_NewBool(ctx, false));

  return JS_UNDEFINED;
}

// Base Stream constructor
JSValue js_stream_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSStreamData* stream = malloc(sizeof(JSStreamData));
  if (!stream) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize stream data
  memset(stream, 0, sizeof(JSStreamData));
  stream->magic = JS_STREAM_MAGIC;
  stream->readable = false;
  stream->writable = false;
  stream->destroyed = false;
  stream->ended = false;
  stream->errored = false;
  stream->error_value = JS_UNDEFINED;

  // Set default options
  stream->options.highWaterMark = 16 * 1024;  // 16KB default
  stream->options.objectMode = false;
  stream->options.encoding = NULL;
  stream->options.defaultEncoding = "utf8";
  stream->options.emitClose = true;
  stream->options.autoDestroy = true;

  JSValue obj = JS_NewObjectClass(ctx, js_stream_class_id);
  if (JS_IsException(obj)) {
    free(stream);
    return obj;
  }

  JS_SetOpaque(obj, stream);

  // Initialize event emitter functionality
  JSValue emitter = init_stream_event_emitter(ctx, obj);
  if (JS_IsException(emitter)) {
    JS_FreeValue(ctx, obj);
    free(stream);
    return emitter;
  }
  JS_FreeValue(ctx, emitter);

  // Add default stream properties
  JS_SetPropertyStr(ctx, obj, "readable", JS_NewBool(ctx, stream->readable));
  JS_SetPropertyStr(ctx, obj, "writable", JS_NewBool(ctx, stream->writable));
  JS_SetPropertyStr(ctx, obj, "destroyed", JS_NewBool(ctx, stream->destroyed));

  return obj;
}

// Stream.prototype.pipe - essential method for Node.js stream compatibility
JSValue js_stream_pipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "pipe() requires at least one argument");
  }

  JSValue destination = argv[0];

  // Check if destination has writable side
  JSValue has_writable = JS_GetPropertyStr(ctx, destination, "writable");
  bool dest_is_writable = JS_ToBool(ctx, has_writable);
  JS_FreeValue(ctx, has_writable);

  if (!dest_is_writable) {
    return JS_ThrowTypeError(ctx, "Cannot pipe to non-writable stream");
  }

  // Get options if provided
  JSValue options = argc > 1 ? argv[1] : JS_UNDEFINED;

  // Simple pipe implementation - add destination to pipe list
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_stream_class_id);
  if (!stream) {
    stream = js_stream_get_data(ctx, this_val, js_readable_class_id);
  }

  if (stream && !stream->destroyed) {
    // Expand pipe destinations array if needed
    if (stream->pipe_count >= stream->pipe_capacity) {
      size_t new_capacity = stream->pipe_capacity * 2 + 1;
      JSValue* new_destinations = realloc(stream->pipe_destinations, new_capacity * sizeof(JSValue));
      if (new_destinations) {
        stream->pipe_destinations = new_destinations;
        stream->pipe_capacity = new_capacity;
      }
    }

    // Add destination to pipe list
    if (stream->pipe_count < stream->pipe_capacity) {
      stream->pipe_destinations[stream->pipe_count] = JS_DupValue(ctx, destination);
      stream->pipe_count++;
    }

    // Emit 'pipe' event on this stream
    stream_emit(ctx, this_val, "pipe", 1, &destination);
  }

  // Return destination for chaining
  return JS_DupValue(ctx, destination);
}

// Initialize stream classes - must be called before creating any streams
void jsrt_stream_init_classes(JSContext* ctx) {
  static bool classes_initialized = false;
  if (classes_initialized) {
    return;  // Already initialized
  }

  // Register class IDs
  JS_NewClassID(&js_stream_class_id);
  JS_NewClassID(&js_readable_class_id);
  JS_NewClassID(&js_writable_class_id);
  JS_NewClassID(&js_duplex_class_id);
  JS_NewClassID(&js_transform_class_id);
  JS_NewClassID(&js_passthrough_class_id);

  // Create class definitions
  JS_NewClass(JS_GetRuntime(ctx), js_stream_class_id, &js_stream_class);
  JS_NewClass(JS_GetRuntime(ctx), js_readable_class_id, &js_readable_class);
  JS_NewClass(JS_GetRuntime(ctx), js_writable_class_id, &js_writable_class);
  JS_NewClass(JS_GetRuntime(ctx), js_duplex_class_id, &js_duplex_class);
  JS_NewClass(JS_GetRuntime(ctx), js_transform_class_id, &js_transform_class);
  JS_NewClass(JS_GetRuntime(ctx), js_passthrough_class_id, &js_passthrough_class);

  classes_initialized = true;
}

// Module initialization
JSValue JSRT_InitNodeStream(JSContext* ctx) {
  JSValue stream_module = JS_NewObject(ctx);

  if (stream_impl_atom == JS_ATOM_NULL) {
    stream_impl_atom = JS_NewAtom(ctx, "__jsrt_stream_impl");
  }

  // Ensure EventEmitter is available globally (needed for stream event handling)
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue existing_ee = JS_GetPropertyStr(ctx, global, "EventEmitter");
  if (JS_IsUndefined(existing_ee)) {
    // Load EventEmitter from node:events and register globally
    extern JSValue JSRT_InitNodeEvents(JSContext * ctx);
    JSValue events_module = JSRT_InitNodeEvents(ctx);
    if (!JS_IsException(events_module) && !JS_IsUndefined(events_module)) {
      JSValue ee_ctor = JS_GetPropertyStr(ctx, events_module, "EventEmitter");
      if (!JS_IsException(ee_ctor) && !JS_IsUndefined(ee_ctor)) {
        JS_SetPropertyStr(ctx, global, "EventEmitter", ee_ctor);
      } else {
        JS_FreeValue(ctx, ee_ctor);
      }
      JS_FreeValue(ctx, events_module);
    }
  } else {
    JS_FreeValue(ctx, existing_ee);
  }
  JS_FreeValue(ctx, global);

  // Ensure stream classes are initialized
  jsrt_stream_init_classes(ctx);

  // Create constructors
  JSValue stream_ctor = JS_NewCFunction2(ctx, js_stream_constructor, "Stream", 0, JS_CFUNC_constructor, 0);
  JSValue readable_ctor = JS_NewCFunction2(ctx, js_readable_constructor, "Readable", 1, JS_CFUNC_constructor, 0);
  JSValue writable_ctor = JS_NewCFunction2(ctx, js_writable_constructor, "Writable", 1, JS_CFUNC_constructor, 0);
  JSValue duplex_ctor = JS_NewCFunction2(ctx, js_duplex_constructor, "Duplex", 1, JS_CFUNC_constructor, 0);
  JSValue transform_ctor = JS_NewCFunction2(ctx, js_transform_constructor, "Transform", 1, JS_CFUNC_constructor, 0);
  JSValue passthrough_ctor =
      JS_NewCFunction2(ctx, js_passthrough_constructor, "PassThrough", 0, JS_CFUNC_constructor, 0);
  JSValue transform_init_fn = JS_NewCFunction(ctx, js_transform_initialize, "__jsrt_initTransform", 1);
  JSValue transform_wrapper = JS_UNDEFINED;

  // Create prototypes
  JSValue stream_proto = JS_NewObject(ctx);
  JSValue readable_proto = JS_NewObject(ctx);
  JSValue writable_proto = JS_NewObject(ctx);
  JSValue duplex_proto = JS_NewObject(ctx);
  JSValue transform_proto = JS_NewObject(ctx);
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

  // Add EventEmitter methods to base Stream prototype
  JS_SetPropertyStr(ctx, stream_proto, "on", JS_DupValue(ctx, on_method));
  JS_SetPropertyStr(ctx, stream_proto, "once", JS_DupValue(ctx, once_method));
  JS_SetPropertyStr(ctx, stream_proto, "emit", JS_DupValue(ctx, emit_method));
  JS_SetPropertyStr(ctx, stream_proto, "off", JS_DupValue(ctx, off_method));
  JS_SetPropertyStr(ctx, stream_proto, "removeListener", JS_DupValue(ctx, remove_listener_method));
  JS_SetPropertyStr(ctx, stream_proto, "addListener", JS_DupValue(ctx, add_listener_method));
  JS_SetPropertyStr(ctx, stream_proto, "removeAllListeners", JS_DupValue(ctx, remove_all_method));
  JS_SetPropertyStr(ctx, stream_proto, "listenerCount", JS_DupValue(ctx, listener_count_method));

  // Add pipe method to base Stream prototype (essential for Node.js compatibility)
  JS_SetPropertyStr(ctx, stream_proto, "pipe", JS_NewCFunction(ctx, js_stream_pipe, "pipe", 1));

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

  // Add to duplex prototype
  JS_SetPropertyStr(ctx, duplex_proto, "on", JS_DupValue(ctx, on_method));
  JS_SetPropertyStr(ctx, duplex_proto, "once", JS_DupValue(ctx, once_method));
  JS_SetPropertyStr(ctx, duplex_proto, "emit", JS_DupValue(ctx, emit_method));
  JS_SetPropertyStr(ctx, duplex_proto, "off", JS_DupValue(ctx, off_method));
  JS_SetPropertyStr(ctx, duplex_proto, "removeListener", JS_DupValue(ctx, remove_listener_method));
  JS_SetPropertyStr(ctx, duplex_proto, "addListener", JS_DupValue(ctx, add_listener_method));
  JS_SetPropertyStr(ctx, duplex_proto, "removeAllListeners", JS_DupValue(ctx, remove_all_method));
  JS_SetPropertyStr(ctx, duplex_proto, "listenerCount", JS_DupValue(ctx, listener_count_method));

  // Add to transform prototype
  JS_SetPropertyStr(ctx, transform_proto, "on", JS_DupValue(ctx, on_method));
  JS_SetPropertyStr(ctx, transform_proto, "once", JS_DupValue(ctx, once_method));
  JS_SetPropertyStr(ctx, transform_proto, "emit", JS_DupValue(ctx, emit_method));
  JS_SetPropertyStr(ctx, transform_proto, "off", JS_DupValue(ctx, off_method));
  JS_SetPropertyStr(ctx, transform_proto, "removeListener", JS_DupValue(ctx, remove_listener_method));
  JS_SetPropertyStr(ctx, transform_proto, "addListener", JS_DupValue(ctx, add_listener_method));
  JS_SetPropertyStr(ctx, transform_proto, "removeAllListeners", JS_DupValue(ctx, remove_all_method));
  JS_SetPropertyStr(ctx, transform_proto, "listenerCount", JS_DupValue(ctx, listener_count_method));

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
  JS_SetPropertyStr(ctx, duplex_proto, "destroy", JS_DupValue(ctx, destroy_method));
  JS_SetPropertyStr(ctx, transform_proto, "destroy", JS_DupValue(ctx, destroy_method));
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
  JS_DefinePropertyGetSet(ctx, duplex_proto, destroyed_atom, JS_DupValue(ctx, get_destroyed), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, transform_proto, destroyed_atom, JS_DupValue(ctx, get_destroyed), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, passthrough_proto, destroyed_atom, get_destroyed, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, destroyed_atom);

  // Define errored property getter on all prototypes
  JSAtom errored_atom = JS_NewAtom(ctx, "errored");
  JS_DefinePropertyGetSet(ctx, readable_proto, errored_atom, JS_DupValue(ctx, get_errored), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, writable_proto, errored_atom, JS_DupValue(ctx, get_errored), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, duplex_proto, errored_atom, JS_DupValue(ctx, get_errored), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, transform_proto, errored_atom, JS_DupValue(ctx, get_errored), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, passthrough_proto, errored_atom, get_errored, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, errored_atom);

  // Initialize stream-specific methods
  js_readable_init_prototype(ctx, readable_proto);
  js_writable_init_prototype(ctx, writable_proto);
  js_duplex_init_prototype(ctx, duplex_proto);
  js_transform_init_prototype(ctx, transform_proto);
  js_passthrough_init_prototype(ctx, passthrough_proto);

  // Add end() method to Duplex, Transform and PassThrough (shared with Writable)
  JS_SetPropertyStr(ctx, duplex_proto, "end", JS_NewCFunction(ctx, js_writable_end, "end", 0));
  JS_SetPropertyStr(ctx, transform_proto, "end", JS_NewCFunction(ctx, js_writable_end, "end", 0));
  JS_SetPropertyStr(ctx, passthrough_proto, "end", JS_NewCFunction(ctx, js_writable_end, "end", 0));

  // Set up prototype chain with base Stream class:
  // Stream -> Readable -> Duplex -> Transform -> PassThrough
  // Stream -> Writable (separate branch)
  JS_SetPrototype(ctx, readable_proto, stream_proto);
  JS_SetPrototype(ctx, writable_proto, stream_proto);
  JS_SetPrototype(ctx, duplex_proto, readable_proto);
  JS_SetPrototype(ctx, transform_proto, duplex_proto);
  JS_SetPrototype(ctx, passthrough_proto, transform_proto);

  // Set prototypes
  JS_SetPropertyStr(ctx, stream_ctor, "prototype", stream_proto);
  JS_SetPropertyStr(ctx, readable_ctor, "prototype", readable_proto);
  JS_SetPropertyStr(ctx, writable_ctor, "prototype", writable_proto);
  JS_SetPropertyStr(ctx, duplex_ctor, "prototype", duplex_proto);
  JS_SetPropertyStr(ctx, transform_ctor, "prototype", transform_proto);
  JS_SetPropertyStr(ctx, passthrough_ctor, "prototype", passthrough_proto);

  // Set constructor property on prototypes
  JS_SetPropertyStr(ctx, stream_proto, "constructor", JS_DupValue(ctx, stream_ctor));
  JS_SetPropertyStr(ctx, readable_proto, "constructor", JS_DupValue(ctx, readable_ctor));
  JS_SetPropertyStr(ctx, writable_proto, "constructor", JS_DupValue(ctx, writable_ctor));
  JS_SetPropertyStr(ctx, duplex_proto, "constructor", JS_DupValue(ctx, duplex_ctor));
  JS_SetPropertyStr(ctx, transform_proto, "constructor", JS_DupValue(ctx, transform_ctor));
  JS_SetPropertyStr(ctx, passthrough_proto, "constructor", JS_DupValue(ctx, passthrough_ctor));

  // Set class prototypes
  JS_SetClassProto(ctx, js_stream_class_id, JS_DupValue(ctx, stream_proto));
  JS_SetClassProto(ctx, js_readable_class_id, JS_DupValue(ctx, readable_proto));
  JS_SetClassProto(ctx, js_writable_class_id, JS_DupValue(ctx, writable_proto));
  JS_SetClassProto(ctx, js_duplex_class_id, JS_DupValue(ctx, duplex_proto));
  JS_SetClassProto(ctx, js_transform_class_id, JS_DupValue(ctx, transform_proto));
  JS_SetClassProto(ctx, js_passthrough_class_id, JS_DupValue(ctx, passthrough_proto));

  const char* transform_wrapper_src =
      "(function(nativeCtor, init) {\n"
      "  'use strict';\n"
      "  function Transform(options) {\n"
      "    if (!(this instanceof Transform)) {\n"
      "      return new Transform(options);\n"
      "    }\n"
      "    init.call(this, options);\n"
      "  }\n"
      "  Transform.prototype = nativeCtor.prototype;\n"
      "  Object.defineProperty(Transform.prototype, 'constructor', {\n"
      "    value: Transform,\n"
      "    writable: true,\n"
      "    configurable: true\n"
      "  });\n"
      "  Object.setPrototypeOf(Transform, nativeCtor);\n"
      "  Transform.__native = nativeCtor;\n"
      "  return Transform;\n"
      "})";

  JSValue wrapper_factory = JS_Eval(ctx, transform_wrapper_src, strlen(transform_wrapper_src),
                                    "<jsrt:stream-transform-wrapper>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(wrapper_factory)) {
    JS_FreeValue(ctx, transform_init_fn);
    JS_FreeValue(ctx, stream_module);
    return wrapper_factory;
  }

  JSValue factory_args[2] = {JS_DupValue(ctx, transform_ctor), JS_DupValue(ctx, transform_init_fn)};
  JSValue transform_wrapper_local = JS_Call(ctx, wrapper_factory, JS_UNDEFINED, 2, factory_args);
  JS_FreeValue(ctx, factory_args[0]);
  JS_FreeValue(ctx, factory_args[1]);
  JS_FreeValue(ctx, wrapper_factory);
  JS_FreeValue(ctx, transform_init_fn);
  if (JS_IsException(transform_wrapper_local)) {
    JS_FreeValue(ctx, stream_module);
    return transform_wrapper_local;
  }

  transform_wrapper = transform_wrapper_local;

  // Add to module
  JS_SetPropertyStr(ctx, stream_module, "Stream", stream_ctor);
  JS_SetPropertyStr(ctx, stream_module, "Readable", readable_ctor);
  JS_SetPropertyStr(ctx, stream_module, "Writable", writable_ctor);
  JS_SetPropertyStr(ctx, stream_module, "Duplex", duplex_ctor);
  JS_SetPropertyStr(ctx, stream_module, "Transform", transform_wrapper);
  JS_SetPropertyStr(ctx, stream_module, "PassThrough", passthrough_ctor);

  // Node.js compatibility: make the main module object behave like the Stream constructor
  // when used with `extends` or `new`. This allows packages like mute-stream to work.
  // In Node.js, `require('stream')` returns a constructor function, not just an object.

  // Copy essential properties from module to the constructor
  JS_SetPropertyStr(ctx, stream_ctor, "Readable", JS_DupValue(ctx, readable_ctor));
  JS_SetPropertyStr(ctx, stream_ctor, "Writable", JS_DupValue(ctx, writable_ctor));
  JS_SetPropertyStr(ctx, stream_ctor, "Duplex", JS_DupValue(ctx, duplex_ctor));
  JS_SetPropertyStr(ctx, stream_ctor, "Transform", JS_DupValue(ctx, transform_wrapper));
  JS_SetPropertyStr(ctx, stream_ctor, "PassThrough", JS_DupValue(ctx, passthrough_ctor));

  // Copy the utility functions too (Phase 5 utilities must be initialized first)
  js_stream_init_utilities(ctx, stream_module);
  JS_SetPropertyStr(ctx, stream_ctor, "pipeline", JS_GetPropertyStr(ctx, stream_module, "pipeline"));
  JS_SetPropertyStr(ctx, stream_ctor, "finished", JS_GetPropertyStr(ctx, stream_module, "finished"));

  // Return the constructor as the module (like Node.js does)
  return stream_ctor;
}

// ES Module support
int js_node_stream_init(JSContext* ctx, JSModuleDef* m) {
  JSValue stream_module = JSRT_InitNodeStream(ctx);

  // Export individual classes
  JS_SetModuleExport(ctx, m, "Stream", JS_GetPropertyStr(ctx, stream_module, "Stream"));
  JS_SetModuleExport(ctx, m, "Readable", JS_GetPropertyStr(ctx, stream_module, "Readable"));
  JS_SetModuleExport(ctx, m, "Writable", JS_GetPropertyStr(ctx, stream_module, "Writable"));
  JS_SetModuleExport(ctx, m, "Duplex", JS_GetPropertyStr(ctx, stream_module, "Duplex"));
  JS_SetModuleExport(ctx, m, "Transform", JS_GetPropertyStr(ctx, stream_module, "Transform"));
  JS_SetModuleExport(ctx, m, "PassThrough", JS_GetPropertyStr(ctx, stream_module, "PassThrough"));

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", stream_module);

  return 0;
}
