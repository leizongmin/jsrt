#include <stdlib.h>
#include <string.h>
#include "../../util/debug.h"
#include "../node_modules.h"
#include "stream_internal.h"

// Define class IDs (not static - shared across modules)
JSClassID js_readable_class_id;
JSClassID js_writable_class_id;
JSClassID js_transform_class_id;
JSClassID js_passthrough_class_id;

// Finalizer for all stream types
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
    // Note: We do NOT free event_emitter here because it's stored as the "_emitter"
    // property on the stream object and will be freed automatically when the object
    // properties are cleaned up. Freeing it here would cause a double-free.

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
    free(stream);
  }
}

// Class definitions
static JSClassDef js_readable_class = {"Readable", .finalizer = js_stream_finalizer};
static JSClassDef js_writable_class = {"Writable", .finalizer = js_stream_finalizer};
static JSClassDef js_transform_class = {"Transform", .finalizer = js_stream_finalizer};
static JSClassDef js_passthrough_class = {"PassThrough", .finalizer = js_stream_finalizer};

// Base method: stream.destroy([error])
JSValue js_stream_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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
JSValue js_stream_get_errored(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

// Writable.prototype.end - shared with PassThrough
static JSValue js_writable_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSStreamData* stream = JS_GetOpaque(this_val, js_writable_class_id);
  if (!stream) {
    stream = JS_GetOpaque(this_val, js_passthrough_class_id);
  }
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a writable stream");
  }

  stream->writable = false;
  stream->ended = true;
  JS_SetPropertyStr(ctx, this_val, "writable", JS_NewBool(ctx, false));

  return JS_UNDEFINED;
}

// Module initialization
JSValue JSRT_InitNodeStream(JSContext* ctx) {
  JSValue stream_module = JS_NewObject(ctx);

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

  // Initialize stream-specific methods
  js_readable_init_prototype(ctx, readable_proto);
  js_writable_init_prototype(ctx, writable_proto);
  js_passthrough_init_prototype(ctx, passthrough_proto);

  // Add end() method to PassThrough (shared with Writable)
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
