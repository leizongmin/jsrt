#include "events_internal.h"

// EventEmitter.prototype.on(event, listener)
JSValue js_event_emitter_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "on() requires event name and listener function");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "event");
  if (!JS_IsFunction(ctx, argv[1])) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "listener must be a function");
  }

  if (!is_event_emitter(ctx, this_val)) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "this is not an EventEmitter");
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name)
    return JS_EXCEPTION;

  JSValue events_obj = get_or_create_events(ctx, this_val);
  JSValue listeners = JS_GetPropertyStr(ctx, events_obj, event_name);
  if (JS_IsUndefined(listeners)) {
    listeners = JS_NewArray(ctx);
    JS_SetPropertyStr(ctx, events_obj, event_name, listeners);
  }

  // Add listener to array
  uint32_t length = get_array_length(ctx, listeners);
  JS_SetPropertyUint32(ctx, listeners, length, JS_DupValue(ctx, argv[1]));

  JS_FreeValue(ctx, events_obj);
  JS_FreeCString(ctx, event_name);
  return JS_DupValue(ctx, this_val);  // Return this for chaining
}

// EventEmitter.prototype.addListener(event, listener) - alias for on()
JSValue js_event_emitter_add_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return js_event_emitter_on(ctx, this_val, argc, argv);
}

// EventEmitter.prototype.once(event, listener)
JSValue js_event_emitter_once(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "once() requires event name and listener function");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "event");
  if (!JS_IsFunction(ctx, argv[1])) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "listener must be a function");
  }

  // Create a wrapper function that removes itself after being called
  const char* wrapper_code =
      "(function(emitter, event, listener) {"
      "  function wrapper(...args) {"
      "    emitter.removeListener(event, wrapper);"
      "    return listener.apply(this, args);"
      "  }"
      "  return wrapper;"
      "})";

  JSValue wrapper_factory = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<wrapper>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(wrapper_factory))
    return wrapper_factory;

  JSValue wrapper_args[3] = {this_val, argv[0], argv[1]};
  JSValue wrapper = JS_Call(ctx, wrapper_factory, JS_UNDEFINED, 3, wrapper_args);
  JS_FreeValue(ctx, wrapper_factory);

  if (JS_IsException(wrapper))
    return wrapper;

  // Add the wrapper using on()
  JSValue on_args[2] = {argv[0], wrapper};
  JSValue result = js_event_emitter_on(ctx, this_val, 2, on_args);
  JS_FreeValue(ctx, wrapper);

  return result;
}

// EventEmitter.prototype.removeListener(event, listener)
JSValue js_event_emitter_remove_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "removeListener() requires event name and listener function");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "event");

  if (!is_event_emitter(ctx, this_val)) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "this is not an EventEmitter");
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name)
    return JS_EXCEPTION;

  JSValue events_obj = get_or_create_events(ctx, this_val);
  JSValue listeners = JS_GetPropertyStr(ctx, events_obj, event_name);

  if (!JS_IsUndefined(listeners) && JS_IsArray(ctx, listeners)) {
    // Create a new array without the listener
    JSValue new_listeners = JS_NewArray(ctx);
    uint32_t length = get_array_length(ctx, listeners);

    uint32_t new_index = 0;
    for (uint32_t i = 0; i < length; i++) {
      JSValue current = JS_GetPropertyUint32(ctx, listeners, i);
      // Simple comparison
      if (!JS_StrictEq(ctx, current, argv[1])) {
        JS_SetPropertyUint32(ctx, new_listeners, new_index++, current);
      } else {
        JS_FreeValue(ctx, current);
      }
    }

    JS_SetPropertyStr(ctx, events_obj, event_name, new_listeners);
  }

  JS_FreeValue(ctx, events_obj);
  JS_FreeCString(ctx, event_name);
  return JS_DupValue(ctx, this_val);
}

// EventEmitter.prototype.emit(event, ...args)
JSValue js_event_emitter_emit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "emit() requires event name");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "event");

  if (!is_event_emitter(ctx, this_val)) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "this is not an EventEmitter");
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name)
    return JS_EXCEPTION;

  JSValue events_obj = get_or_create_events(ctx, this_val);
  JSValue listeners = JS_GetPropertyStr(ctx, events_obj, event_name);
  bool had_listeners = false;

  if (!JS_IsUndefined(listeners) && JS_IsArray(ctx, listeners)) {
    uint32_t length = get_array_length(ctx, listeners);

    if (length > 0) {
      had_listeners = true;

      // Prepare arguments for listeners (skip the event name)
      JSValueConst* listener_args = (argc > 1) ? &argv[1] : NULL;
      int listener_argc = (argc > 1) ? argc - 1 : 0;

      // Call each listener
      for (uint32_t i = 0; i < length; i++) {
        JSValue listener = JS_GetPropertyUint32(ctx, listeners, i);
        if (JS_IsFunction(ctx, listener)) {
          JSValue result = JS_Call(ctx, listener, this_val, listener_argc, listener_args);
          if (JS_IsException(result)) {
            // In Node.js, errors in event listeners are handled by the 'error' event
            // For now, just continue without logging
            JS_FreeValue(ctx, result);
          } else {
            JS_FreeValue(ctx, result);
          }
        }
        JS_FreeValue(ctx, listener);
      }
    }
  }

  JS_FreeValue(ctx, events_obj);
  JS_FreeCString(ctx, event_name);
  return JS_NewBool(ctx, had_listeners);
}

// EventEmitter.prototype.listenerCount(event)
JSValue js_event_emitter_listener_count(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewInt32(ctx, 0);
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "event");

  if (!is_event_emitter(ctx, this_val)) {
    return JS_NewInt32(ctx, 0);
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name)
    return JS_EXCEPTION;

  JSValue events_obj = get_or_create_events(ctx, this_val);
  JSValue listeners = JS_GetPropertyStr(ctx, events_obj, event_name);
  uint32_t count = 0;

  if (!JS_IsUndefined(listeners) && JS_IsArray(ctx, listeners)) {
    count = get_array_length(ctx, listeners);
  }

  JS_FreeValue(ctx, events_obj);
  JS_FreeCString(ctx, event_name);
  return JS_NewUint32(ctx, count);
}

// EventEmitter.prototype.removeAllListeners(event?)
JSValue js_event_emitter_remove_all_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!is_event_emitter(ctx, this_val)) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "this is not an EventEmitter");
  }

  JSValue events_obj = get_or_create_events(ctx, this_val);

  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    // Remove listeners for specific event
    NODE_ARG_REQUIRE_STRING(ctx, argv[0], "event");

    const char* event_name = JS_ToCString(ctx, argv[0]);
    if (!event_name)
      return JS_EXCEPTION;

    // Create a property atom for deletion
    JSAtom prop_atom = JS_NewAtom(ctx, event_name);
    JS_DeleteProperty(ctx, events_obj, prop_atom, 0);
    JS_FreeAtom(ctx, prop_atom);
    JS_FreeCString(ctx, event_name);
  } else {
    // Remove all listeners for all events
    JS_SetPropertyStr(ctx, this_val, "_events", JS_NewObject(ctx));
  }

  JS_FreeValue(ctx, events_obj);
  return JS_DupValue(ctx, this_val);
}

// EventEmitter constructor
JSValue js_event_emitter_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue proto, emitter;

  if (JS_IsUndefined(new_target)) {
    // Called as function, not constructor
    return JS_ThrowTypeError(ctx, "EventEmitter constructor must be called with 'new'");
  }

  // Create a new object
  emitter = JS_NewObject(ctx);
  if (JS_IsException(emitter))
    return emitter;

  // Get the prototype from new_target (the constructor)
  proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if (!JS_IsException(proto)) {
    // Set the prototype of the new object
    JS_SetPrototype(ctx, emitter, proto);
    JS_FreeValue(ctx, proto);
  }

  // Initialize _events property
  JS_SetPropertyStr(ctx, emitter, "_events", JS_NewObject(ctx));

  return emitter;
}