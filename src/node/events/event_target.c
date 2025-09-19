#include "events_internal.h"

// EventTarget helper function to check if an object is an EventTarget
bool is_event_target(JSContext* ctx, JSValueConst this_val) {
  return JS_IsObject(this_val) && !JS_IsFunction(ctx, this_val) && !JS_IsError(ctx, this_val);
}

// Helper function to get or create event listeners map for EventTarget
JSValue get_or_create_event_listeners(JSContext* ctx, JSValueConst this_val) {
  JSValue listeners = JS_GetPropertyStr(ctx, this_val, "_eventListeners");
  if (JS_IsUndefined(listeners)) {
    listeners = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, this_val, "_eventListeners", JS_DupValue(ctx, listeners));
  }
  return listeners;
}

// EventTarget constructor
JSValue js_event_target_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj, proto;

  if (JS_IsUndefined(new_target)) {
    return JS_ThrowTypeError(ctx, "EventTarget constructor must be called with 'new'");
  }

  // Create a new object
  obj = JS_NewObject(ctx);
  if (JS_IsException(obj))
    return obj;

  // Get the prototype from new_target (the constructor)
  proto = JS_GetPropertyStr(ctx, new_target, "prototype");
  if (!JS_IsException(proto)) {
    // Set the prototype of the new object
    JS_SetPrototype(ctx, obj, proto);
    JS_FreeValue(ctx, proto);
  }

  // Initialize _eventListeners map
  JSValue listeners = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, obj, "_eventListeners", listeners);

  return obj;
}

// EventTarget.prototype.addEventListener(type, listener[, options])
JSValue js_event_target_add_event_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!is_event_target(ctx, this_val)) {
    return JS_ThrowTypeError(ctx, "addEventListener can only be called on EventTarget objects");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "addEventListener requires at least 2 arguments");
  }

  // Get event type (must be string)
  const char* event_type = JS_ToCString(ctx, argv[0]);
  if (!event_type) {
    return JS_ThrowTypeError(ctx, "Event type must be a string");
  }

  // Get listener (must be function)
  if (!JS_IsFunction(ctx, argv[1])) {
    JS_FreeCString(ctx, event_type);
    return JS_ThrowTypeError(ctx, "Event listener must be a function");
  }

  // Parse options (optional third argument)
  bool once = false;
  bool passive = false;
  bool capture = false;
  JSValue abort_signal = JS_UNDEFINED;

  if (argc >= 3 && JS_IsObject(argv[2])) {
    JSValue options = argv[2];

    // Get 'once' option
    JSValue once_val = JS_GetPropertyStr(ctx, options, "once");
    if (!JS_IsUndefined(once_val)) {
      once = JS_ToBool(ctx, once_val);
    }
    JS_FreeValue(ctx, once_val);

    // Get 'passive' option
    JSValue passive_val = JS_GetPropertyStr(ctx, options, "passive");
    if (!JS_IsUndefined(passive_val)) {
      passive = JS_ToBool(ctx, passive_val);
    }
    JS_FreeValue(ctx, passive_val);

    // Get 'capture' option
    JSValue capture_val = JS_GetPropertyStr(ctx, options, "capture");
    if (!JS_IsUndefined(capture_val)) {
      capture = JS_ToBool(ctx, capture_val);
    }
    JS_FreeValue(ctx, capture_val);

    // Get 'signal' option (AbortSignal)
    abort_signal = JS_GetPropertyStr(ctx, options, "signal");
    if (JS_IsUndefined(abort_signal)) {
      JS_FreeValue(ctx, abort_signal);
      abort_signal = JS_UNDEFINED;
    }
  }

  // Get or create event listeners map
  JSValue listeners_map = get_or_create_event_listeners(ctx, this_val);
  if (JS_IsException(listeners_map)) {
    JS_FreeCString(ctx, event_type);
    JS_FreeValue(ctx, abort_signal);
    return JS_EXCEPTION;
  }

  // Get or create listeners array for this event type
  JSValue listeners_array = JS_GetPropertyStr(ctx, listeners_map, event_type);
  if (JS_IsUndefined(listeners_array)) {
    listeners_array = JS_NewArray(ctx);
    JS_SetPropertyStr(ctx, listeners_map, event_type, JS_DupValue(ctx, listeners_array));
  }

  // Check if listener is already registered (EventTarget only adds once per type)
  uint32_t array_length = get_array_length(ctx, listeners_array);
  for (uint32_t i = 0; i < array_length; i++) {
    JSValue listener_obj = JS_GetPropertyUint32(ctx, listeners_array, i);
    JSValue existing_listener = JS_GetPropertyStr(ctx, listener_obj, "listener");

    // Compare listeners (same function and same capture setting)
    bool same_listener = JS_SameValue(ctx, existing_listener, argv[1]);
    JSValue existing_capture = JS_GetPropertyStr(ctx, listener_obj, "capture");
    bool same_capture = JS_ToBool(ctx, existing_capture) == capture;

    JS_FreeValue(ctx, listener_obj);
    JS_FreeValue(ctx, existing_listener);
    JS_FreeValue(ctx, existing_capture);

    if (same_listener && same_capture) {
      // Listener already exists, don't add duplicate
      JS_FreeValue(ctx, listeners_array);
      JS_FreeValue(ctx, listeners_map);
      JS_FreeCString(ctx, event_type);
      JS_FreeValue(ctx, abort_signal);
      return JS_UNDEFINED;
    }
  }

  // Create listener object with metadata
  JSValue listener_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, listener_obj, "listener", JS_DupValue(ctx, argv[1]));
  JS_SetPropertyStr(ctx, listener_obj, "once", JS_NewBool(ctx, once));
  JS_SetPropertyStr(ctx, listener_obj, "passive", JS_NewBool(ctx, passive));
  JS_SetPropertyStr(ctx, listener_obj, "capture", JS_NewBool(ctx, capture));

  if (!JS_IsUndefined(abort_signal)) {
    JS_SetPropertyStr(ctx, listener_obj, "signal", JS_DupValue(ctx, abort_signal));
  }

  // Add listener to array
  JS_SetPropertyUint32(ctx, listeners_array, array_length, listener_obj);

  // Clean up
  JS_FreeValue(ctx, listeners_array);
  JS_FreeValue(ctx, listeners_map);
  JS_FreeCString(ctx, event_type);
  JS_FreeValue(ctx, abort_signal);

  return JS_UNDEFINED;
}

// EventTarget.prototype.removeEventListener(type, listener[, options])
JSValue js_event_target_remove_event_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!is_event_target(ctx, this_val)) {
    return JS_ThrowTypeError(ctx, "removeEventListener can only be called on EventTarget objects");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "removeEventListener requires at least 2 arguments");
  }

  // Get event type
  const char* event_type = JS_ToCString(ctx, argv[0]);
  if (!event_type) {
    return JS_ThrowTypeError(ctx, "Event type must be a string");
  }

  // Parse capture option
  bool capture = false;
  if (argc >= 3 && JS_IsObject(argv[2])) {
    JSValue capture_val = JS_GetPropertyStr(ctx, argv[2], "capture");
    if (!JS_IsUndefined(capture_val)) {
      capture = JS_ToBool(ctx, capture_val);
    }
    JS_FreeValue(ctx, capture_val);
  }

  // Get event listeners map
  JSValue listeners_map = JS_GetPropertyStr(ctx, this_val, "_eventListeners");
  if (JS_IsUndefined(listeners_map)) {
    JS_FreeCString(ctx, event_type);
    return JS_UNDEFINED;  // No listeners registered
  }

  // Get listeners array for this event type
  JSValue listeners_array = JS_GetPropertyStr(ctx, listeners_map, event_type);
  if (JS_IsUndefined(listeners_array)) {
    JS_FreeValue(ctx, listeners_map);
    JS_FreeCString(ctx, event_type);
    return JS_UNDEFINED;  // No listeners for this event type
  }

  // Find and remove matching listener
  uint32_t array_length = get_array_length(ctx, listeners_array);

  // Safety check: ensure array has elements before attempting removal
  if (array_length == 0) {
    JS_FreeValue(ctx, listeners_array);
    JS_FreeValue(ctx, listeners_map);
    JS_FreeCString(ctx, event_type);
    return JS_UNDEFINED;
  }

  for (uint32_t i = 0; i < array_length; i++) {
    JSValue listener_obj = JS_GetPropertyUint32(ctx, listeners_array, i);
    JSValue existing_listener = JS_GetPropertyStr(ctx, listener_obj, "listener");
    JSValue existing_capture = JS_GetPropertyStr(ctx, listener_obj, "capture");

    bool same_listener = JS_SameValue(ctx, existing_listener, argv[1]);
    bool same_capture = JS_ToBool(ctx, existing_capture) == capture;

    if (same_listener && same_capture) {
      // Safe bounds checking: only shift if we have elements to shift
      if (array_length > 1 && i < array_length - 1) {
        // Remove this listener by shifting remaining elements
        for (uint32_t j = i; j < array_length - 1; j++) {
          // Ensure j+1 is within bounds
          if (j + 1 < array_length) {
            JSValue next_listener = JS_GetPropertyUint32(ctx, listeners_array, j + 1);
            JS_SetPropertyUint32(ctx, listeners_array, j, next_listener);
          }
        }
      }

      // Remove the last element (safe since we checked array_length > 0)
      JSAtom prop_atom = JS_NewAtomUInt32(ctx, array_length - 1);
      JS_DeleteProperty(ctx, listeners_array, prop_atom, 0);
      JS_FreeAtom(ctx, prop_atom);

      JS_FreeValue(ctx, listener_obj);
      JS_FreeValue(ctx, existing_listener);
      JS_FreeValue(ctx, existing_capture);
      break;
    }

    JS_FreeValue(ctx, listener_obj);
    JS_FreeValue(ctx, existing_listener);
    JS_FreeValue(ctx, existing_capture);
  }

  // Clean up
  JS_FreeValue(ctx, listeners_array);
  JS_FreeValue(ctx, listeners_map);
  JS_FreeCString(ctx, event_type);

  return JS_UNDEFINED;
}

// EventTarget.prototype.dispatchEvent(event)
JSValue js_event_target_dispatch_event(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!is_event_target(ctx, this_val)) {
    return JS_ThrowTypeError(ctx, "dispatchEvent can only be called on EventTarget objects");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "dispatchEvent requires an event argument");
  }

  JSValue event = argv[0];
  if (!JS_IsObject(event)) {
    return JS_ThrowTypeError(ctx, "Event must be an object");
  }

  // Get event type from event object
  JSValue type_val = JS_GetPropertyStr(ctx, event, "type");
  if (JS_IsUndefined(type_val)) {
    JS_FreeValue(ctx, type_val);
    return JS_ThrowTypeError(ctx, "Event must have a type property");
  }

  const char* event_type = JS_ToCString(ctx, type_val);
  if (!event_type) {
    JS_FreeValue(ctx, type_val);
    return JS_ThrowTypeError(ctx, "Event type must be a string");
  }

  // Set event target and currentTarget
  JS_SetPropertyStr(ctx, event, "target", JS_DupValue(ctx, this_val));
  JS_SetPropertyStr(ctx, event, "currentTarget", JS_DupValue(ctx, this_val));

  // Get event listeners
  JSValue listeners_map = JS_GetPropertyStr(ctx, this_val, "_eventListeners");
  if (JS_IsUndefined(listeners_map)) {
    JS_FreeValue(ctx, type_val);
    JS_FreeCString(ctx, event_type);
    return JS_NewBool(ctx, true);  // No listeners, event not canceled
  }

  JSValue listeners_array = JS_GetPropertyStr(ctx, listeners_map, event_type);
  if (JS_IsUndefined(listeners_array)) {
    JS_FreeValue(ctx, listeners_map);
    JS_FreeValue(ctx, type_val);
    JS_FreeCString(ctx, event_type);
    return JS_NewBool(ctx, true);  // No listeners for this event type
  }

  // Invoke listeners in order
  uint32_t array_length = get_array_length(ctx, listeners_array);
  JSValue listeners_to_remove = JS_NewArray(ctx);
  uint32_t remove_count = 0;

  for (uint32_t i = 0; i < array_length; i++) {
    JSValue listener_obj = JS_GetPropertyUint32(ctx, listeners_array, i);
    JSValue listener_func = JS_GetPropertyStr(ctx, listener_obj, "listener");
    JSValue once_val = JS_GetPropertyStr(ctx, listener_obj, "once");

    bool is_once = JS_ToBool(ctx, once_val);

    // Call the listener with the event
    JSValue result = JS_Call(ctx, listener_func, this_val, 1, &event);

    // Handle any exceptions from the listener
    if (JS_IsException(result)) {
      // Log error but continue with other listeners (just free the exception)
      // In production, you might want to log this error
      JS_FreeValue(ctx, result);
    } else {
      JS_FreeValue(ctx, result);
    }

    // Mark for removal if 'once' option was set
    if (is_once) {
      JS_SetPropertyUint32(ctx, listeners_to_remove, remove_count++, JS_NewInt32(ctx, i));
    }

    JS_FreeValue(ctx, listener_obj);
    JS_FreeValue(ctx, listener_func);
    JS_FreeValue(ctx, once_val);
  }

  // Remove 'once' listeners (iterate backwards to maintain indices)
  for (int32_t i = remove_count - 1; i >= 0; i--) {
    JSValue index_val = JS_GetPropertyUint32(ctx, listeners_to_remove, i);
    int32_t index = 0;
    JS_ToInt32(ctx, &index, index_val);

    // Shift elements to remove the listener
    for (uint32_t j = index; j < array_length - 1; j++) {
      JSValue next_listener = JS_GetPropertyUint32(ctx, listeners_array, j + 1);
      JS_SetPropertyUint32(ctx, listeners_array, j, next_listener);
    }
    JSAtom prop_atom = JS_NewAtomUInt32(ctx, --array_length);
    JS_DeleteProperty(ctx, listeners_array, prop_atom, 0);
    JS_FreeAtom(ctx, prop_atom);

    JS_FreeValue(ctx, index_val);
  }

  // Check if event was canceled (defaultPrevented property)
  JSValue default_prevented = JS_GetPropertyStr(ctx, event, "defaultPrevented");
  bool was_canceled = JS_ToBool(ctx, default_prevented);

  // Clean up
  JS_FreeValue(ctx, listeners_to_remove);
  JS_FreeValue(ctx, listeners_array);
  JS_FreeValue(ctx, listeners_map);
  JS_FreeValue(ctx, type_val);
  JS_FreeValue(ctx, default_prevented);
  JS_FreeCString(ctx, event_type);

  return JS_NewBool(ctx, !was_canceled);
}