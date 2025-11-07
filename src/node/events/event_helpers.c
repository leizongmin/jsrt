#include "events_internal.h"

// Helper: Check if this is an EventEmitter (has _events property or inherits from EventEmitter)
bool is_event_emitter(JSContext* ctx, JSValueConst this_val) {
  // First check if _events property already exists
  JSValue events_prop = JS_GetPropertyStr(ctx, this_val, "_events");
  if (!JS_IsUndefined(events_prop)) {
    JS_FreeValue(ctx, events_prop);
    return true;
  }
  JS_FreeValue(ctx, events_prop);

  // Check if this object inherits EventEmitter methods through util.inherits()
  // by checking for characteristic EventEmitter methods in the prototype chain
  JSValue on_method = JS_GetPropertyStr(ctx, this_val, "on");
  JSValue emit_method = JS_GetPropertyStr(ctx, this_val, "emit");

  bool has_emitter_methods = JS_IsFunction(ctx, on_method) && JS_IsFunction(ctx, emit_method);

  JS_FreeValue(ctx, on_method);
  JS_FreeValue(ctx, emit_method);

  return has_emitter_methods;
}

// Helper: Get or create events object
JSValue get_or_create_events(JSContext* ctx, JSValueConst this_val) {
  JSValue events_obj = JS_GetPropertyStr(ctx, this_val, "_events");
  if (JS_IsUndefined(events_obj)) {
    JS_FreeValue(ctx, events_obj);
    events_obj = JS_NewObject(ctx);
    if (JS_IsException(events_obj)) {
      return events_obj;
    }
    JS_SetPropertyStr(ctx, this_val, "_events", JS_DupValue(ctx, events_obj));
  }
  return events_obj;
}

// Helper: Get array length
uint32_t get_array_length(JSContext* ctx, JSValueConst array) {
  JSValue length_val = JS_GetPropertyStr(ctx, array, "length");
  uint32_t length = 0;
  if (JS_IsNumber(length_val)) {
    JS_ToUint32(ctx, &length, length_val);
  }
  JS_FreeValue(ctx, length_val);
  return length;
}

// Helper: Get or create _maxListeners property
JSValue get_or_create_max_listeners(JSContext* ctx, JSValueConst this_val) {
  JSValue max_listeners = JS_GetPropertyStr(ctx, this_val, "_maxListeners");
  if (JS_IsUndefined(max_listeners)) {
    // Check for global default only for new instances
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue default_max = JS_GetPropertyStr(ctx, global, "_defaultMaxListeners");
    JS_FreeValue(ctx, global);

    if (!JS_IsUndefined(default_max)) {
      max_listeners = JS_DupValue(ctx, default_max);
    } else {
      // Default max listeners is 10
      max_listeners = JS_NewInt32(ctx, 10);
    }
    // Store the value on the instance
    JS_SetPropertyStr(ctx, this_val, "_maxListeners", JS_DupValue(ctx, max_listeners));
  }
  return max_listeners;
}

// Global variable to store the current wrapper function being called
JSValue current_once_wrapper = {0};

// Native secure wrapper for once() functionality
JSValue once_wrapper_function(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Use the global reference to the current wrapper function
  JSValue wrapper_func = current_once_wrapper;

  if (JS_IsUndefined(wrapper_func)) {
    return JS_ThrowTypeError(ctx, "Invalid once wrapper state");
  }

  // Get stored data from the wrapper function properties
  JSValue emitter = JS_GetPropertyStr(ctx, wrapper_func, "_emitter");
  JSValue event_name = JS_GetPropertyStr(ctx, wrapper_func, "_event_name");
  JSValue listener = JS_GetPropertyStr(ctx, wrapper_func, "_listener");

  if (JS_IsUndefined(emitter) || JS_IsUndefined(event_name) || JS_IsUndefined(listener)) {
    JS_FreeValue(ctx, emitter);
    JS_FreeValue(ctx, event_name);
    JS_FreeValue(ctx, listener);
    return JS_ThrowTypeError(ctx, "Invalid once wrapper properties");
  }

  // Call the original listener with the emitter as 'this' context
  JSValue result = JS_Call(ctx, listener, emitter, argc, argv);

  // Remove this wrapper from the emitter after calling the listener
  JSValue removeListener = JS_GetPropertyStr(ctx, emitter, "removeListener");
  if (JS_IsFunction(ctx, removeListener)) {
    JSValue remove_args[] = {event_name, wrapper_func};
    JSValue remove_result = JS_Call(ctx, removeListener, emitter, 2, remove_args);
    JS_FreeValue(ctx, remove_result);
  }
  JS_FreeValue(ctx, removeListener);

  JS_FreeValue(ctx, emitter);
  JS_FreeValue(ctx, event_name);
  JS_FreeValue(ctx, listener);

  return result;
}

// Create a secure native wrapper for once functionality
JSValue create_once_wrapper(JSContext* ctx, JSValueConst emitter, JSValueConst event_name, JSValueConst listener) {
  if (!JS_IsFunction(ctx, listener)) {
    return JS_ThrowTypeError(ctx, "Listener must be a function");
  }

  // Create the wrapper function
  JSValue wrapper = JS_NewCFunction(ctx, once_wrapper_function, "onceWrapper", 0);
  if (JS_IsException(wrapper)) {
    return wrapper;
  }

  // Store the emitter, event name and listener in the wrapper function
  JS_SetPropertyStr(ctx, wrapper, "_emitter", JS_DupValue(ctx, emitter));
  JS_SetPropertyStr(ctx, wrapper, "_event_name", JS_DupValue(ctx, event_name));
  JS_SetPropertyStr(ctx, wrapper, "_listener", JS_DupValue(ctx, listener));

  return wrapper;
}

// Native secure wrapper for prependOnce() functionality
JSValue prepend_once_wrapper_function(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Same implementation as once_wrapper_function since the behavior is identical
  return once_wrapper_function(ctx, this_val, argc, argv);
}

// Create a secure native wrapper for prependOnce functionality
JSValue create_prepend_once_wrapper(JSContext* ctx, JSValueConst emitter, JSValueConst event_name,
                                    JSValueConst listener) {
  if (!JS_IsFunction(ctx, listener)) {
    return JS_ThrowTypeError(ctx, "Listener must be a function");
  }

  // Create the wrapper function
  JSValue wrapper = JS_NewCFunction(ctx, prepend_once_wrapper_function, "prependOnceWrapper", 0);
  if (JS_IsException(wrapper)) {
    return wrapper;
  }

  // Store the emitter, event name and listener in the wrapper function
  JS_SetPropertyStr(ctx, wrapper, "_emitter", JS_DupValue(ctx, emitter));
  JS_SetPropertyStr(ctx, wrapper, "_event_name", JS_DupValue(ctx, event_name));
  JS_SetPropertyStr(ctx, wrapper, "_listener", JS_DupValue(ctx, listener));

  return wrapper;
}