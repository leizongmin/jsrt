#include "events_internal.h"

// EventEmitter.prototype.prependListener(event, listener)
JSValue js_event_emitter_prepend_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "prependListener() requires event name and listener function");
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

  // Create new array with prepended listener
  JSValue new_listeners = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, new_listeners, 0, JS_DupValue(ctx, argv[1]));

  // Copy existing listeners
  uint32_t length = get_array_length(ctx, listeners);
  for (uint32_t i = 0; i < length; i++) {
    JSValue existing = JS_GetPropertyUint32(ctx, listeners, i);
    JS_SetPropertyUint32(ctx, new_listeners, i + 1, existing);
  }

  JS_SetPropertyStr(ctx, events_obj, event_name, new_listeners);
  JS_FreeValue(ctx, events_obj);
  JS_FreeCString(ctx, event_name);
  return JS_DupValue(ctx, this_val);
}

// EventEmitter.prototype.prependOnceListener(event, listener)
JSValue js_event_emitter_prepend_once_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS,
                            "prependOnceListener() requires event name and listener function");
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "event");
  if (!JS_IsFunction(ctx, argv[1])) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "listener must be a function");
  }

  // Use secure native wrapper instead of JS_Eval to prevent code injection
  JSValue wrapper = create_prepend_once_wrapper(ctx, this_val, argv[0], argv[1]);
  if (JS_IsException(wrapper))
    return wrapper;

  // Add the wrapper using prependListener()
  JSValue prepend_args[2] = {argv[0], wrapper};
  JSValue result = js_event_emitter_prepend_listener(ctx, this_val, 2, prepend_args);
  JS_FreeValue(ctx, wrapper);

  return result;
}

// EventEmitter.prototype.eventNames()
JSValue js_event_emitter_event_names(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!is_event_emitter(ctx, this_val)) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "this is not an EventEmitter");
  }

  JSValue events_obj = get_or_create_events(ctx, this_val);
  JSValue result = JS_NewArray(ctx);

  JSPropertyEnum* props;
  uint32_t prop_count;
  if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, events_obj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
    for (uint32_t i = 0; i < prop_count; i++) {
      JSValue prop_name = JS_AtomToString(ctx, props[i].atom);
      JS_SetPropertyUint32(ctx, result, i, prop_name);
    }
    js_free(ctx, props);
  }

  JS_FreeValue(ctx, events_obj);
  return result;
}

// EventEmitter.prototype.listeners(event)
JSValue js_event_emitter_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewArray(ctx);
  }

  NODE_ARG_REQUIRE_STRING(ctx, argv[0], "event");

  if (!is_event_emitter(ctx, this_val)) {
    return JS_NewArray(ctx);
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name)
    return JS_EXCEPTION;

  JSValue events_obj = get_or_create_events(ctx, this_val);
  JSValue listeners = JS_GetPropertyStr(ctx, events_obj, event_name);
  JSValue result = JS_NewArray(ctx);

  if (!JS_IsUndefined(listeners) && JS_IsArray(ctx, listeners)) {
    uint32_t length = get_array_length(ctx, listeners);
    for (uint32_t i = 0; i < length; i++) {
      JSValue listener = JS_GetPropertyUint32(ctx, listeners, i);
      JS_SetPropertyUint32(ctx, result, i, listener);
    }
  }

  JS_FreeValue(ctx, events_obj);
  JS_FreeCString(ctx, event_name);
  return result;
}

// EventEmitter.prototype.rawListeners(event) - same as listeners() for now
JSValue js_event_emitter_raw_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return js_event_emitter_listeners(ctx, this_val, argc, argv);
}

// EventEmitter.prototype.off(event, listener) - alias for removeListener
JSValue js_event_emitter_off(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return js_event_emitter_remove_listener(ctx, this_val, argc, argv);
}

// EventEmitter.prototype.setMaxListeners(n)
JSValue js_event_emitter_set_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "setMaxListeners() requires a number argument");
  }

  if (!JS_IsNumber(argv[0])) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "n must be a number");
  }

  if (!is_event_emitter(ctx, this_val)) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "this is not an EventEmitter");
  }

  // Validate the number is non-negative
  int32_t n;
  if (JS_ToInt32(ctx, &n, argv[0])) {
    return JS_EXCEPTION;
  }
  if (n < 0) {
    return node_throw_error(ctx, NODE_ERR_OUT_OF_RANGE, "n must be non-negative");
  }

  JS_SetPropertyStr(ctx, this_val, "_maxListeners", JS_DupValue(ctx, argv[0]));
  return JS_DupValue(ctx, this_val);
}

// EventEmitter.prototype.getMaxListeners()
JSValue js_event_emitter_get_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!is_event_emitter(ctx, this_val)) {
    return JS_NewInt32(ctx, 10);  // Default value
  }

  JSValue max_listeners = get_or_create_max_listeners(ctx, this_val);
  return max_listeners;  // Already duped by get_or_create_max_listeners
}