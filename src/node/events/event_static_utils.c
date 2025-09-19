#include "events_internal.h"

// events.getEventListeners(emitterOrTarget, eventName)
JSValue js_events_get_event_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "getEventListeners requires 2 arguments: (emitterOrTarget, eventName)");
  }

  JSValue target = argv[0];
  const char* event_name = JS_ToCString(ctx, argv[1]);
  if (!event_name) {
    return JS_ThrowTypeError(ctx, "Event name must be a string");
  }

  JSValue listeners_array = JS_NewArray(ctx);

  // Check if it's an EventEmitter
  if (is_event_emitter(ctx, target)) {
    JSValue events_obj = JS_GetPropertyStr(ctx, target, "_events");
    if (!JS_IsUndefined(events_obj)) {
      JSValue event_listeners = JS_GetPropertyStr(ctx, events_obj, event_name);
      if (JS_IsArray(ctx, event_listeners)) {
        // Copy listeners to new array
        uint32_t length = get_array_length(ctx, event_listeners);
        for (uint32_t i = 0; i < length; i++) {
          JSValue listener = JS_GetPropertyUint32(ctx, event_listeners, i);
          JS_SetPropertyUint32(ctx, listeners_array, i, JS_DupValue(ctx, listener));
          JS_FreeValue(ctx, listener);
        }
      }
      JS_FreeValue(ctx, event_listeners);
    }
    JS_FreeValue(ctx, events_obj);
  }
  // Check if it's an EventTarget
  else if (is_event_target(ctx, target)) {
    JSValue listeners_map = JS_GetPropertyStr(ctx, target, "_eventListeners");
    if (!JS_IsUndefined(listeners_map)) {
      JSValue event_listeners = JS_GetPropertyStr(ctx, listeners_map, event_name);
      if (JS_IsArray(ctx, event_listeners)) {
        // Copy listener functions (extract from listener objects)
        uint32_t length = get_array_length(ctx, event_listeners);
        uint32_t copy_index = 0;
        for (uint32_t i = 0; i < length; i++) {
          JSValue listener_obj = JS_GetPropertyUint32(ctx, event_listeners, i);
          JSValue listener_func = JS_GetPropertyStr(ctx, listener_obj, "listener");
          JS_SetPropertyUint32(ctx, listeners_array, copy_index++, JS_DupValue(ctx, listener_func));
          JS_FreeValue(ctx, listener_func);
          JS_FreeValue(ctx, listener_obj);
        }
      }
      JS_FreeValue(ctx, event_listeners);
    }
    JS_FreeValue(ctx, listeners_map);
  }

  JS_FreeCString(ctx, event_name);
  return listeners_array;
}

// events.once(emitter, name[, options])
JSValue js_events_once(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "once requires at least 2 arguments: (emitter, eventName)");
  }

  JSValue emitter = argv[0];
  JSValue event_name = argv[1];

  // Parse options
  JSValue abort_signal = JS_UNDEFINED;
  if (argc >= 3 && JS_IsObject(argv[2])) {
    abort_signal = JS_GetPropertyStr(ctx, argv[2], "signal");
    if (JS_IsUndefined(abort_signal)) {
      JS_FreeValue(ctx, abort_signal);
      abort_signal = JS_UNDEFINED;
    }
  }

  // Create a Promise that resolves when the event is emitted
  JSValue promise_constructor = JS_GetGlobalObject(ctx);
  JSValue promise_ctor = JS_GetPropertyStr(ctx, promise_constructor, "Promise");
  JS_FreeValue(ctx, promise_constructor);

  // Create executor function
  JSValue executor = JS_NewCFunction(ctx, js_events_once_executor, "executor", 2);

  // Store emitter, event_name, and abort_signal in executor context
  JS_SetPropertyStr(ctx, executor, "_emitter", JS_DupValue(ctx, emitter));
  JS_SetPropertyStr(ctx, executor, "_eventName", JS_DupValue(ctx, event_name));
  // Only set _abortSignal property if we actually have one
  if (!JS_IsUndefined(abort_signal)) {
    JS_SetPropertyStr(ctx, executor, "_abortSignal", JS_DupValue(ctx, abort_signal));
  }
  // If abort_signal is undefined, we don't set the property at all

  // Store executor globally so it can be accessed in the executor function
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, "__current_executor", JS_DupValue(ctx, executor));

  JSValue args[] = {executor};
  JSValue promise = JS_CallConstructor(ctx, promise_ctor, 1, args);

  // Clean up global executor reference
  JSAtom executor_atom = JS_NewAtom(ctx, "__current_executor");
  JS_DeleteProperty(ctx, global, executor_atom, 0);
  JS_FreeAtom(ctx, executor_atom);

  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, executor);

  JS_FreeValue(ctx, promise_ctor);
  JS_FreeValue(ctx, abort_signal);

  return promise;
}

// Helper function for events.once Promise executor
JSValue js_events_once_executor(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_UNDEFINED;
  }

  JSValue resolve = argv[0];
  JSValue reject = argv[1];

  // Get the executor function from global object where we stored it
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue executor_ref = JS_GetPropertyStr(ctx, global, "__current_executor");

  // Get stored values from the executor function
  JSValue emitter = JS_GetPropertyStr(ctx, executor_ref, "_emitter");
  JSValue event_name = JS_GetPropertyStr(ctx, executor_ref, "_eventName");
  JSValue abort_signal = JS_GetPropertyStr(ctx, executor_ref, "_abortSignal");

  JS_FreeValue(ctx, executor_ref);
  // If _abortSignal property was never set, this will be JS_UNDEFINED

  // Create listener function that resolves the promise
  JSValue listener = JS_NewCFunction(ctx, js_events_once_listener, "onceListener", 1);

  // Store resolve and reject functions globally so listener can access them
  JS_SetPropertyStr(ctx, global, "__temp_resolve", JS_DupValue(ctx, resolve));
  JS_SetPropertyStr(ctx, global, "__temp_reject", JS_DupValue(ctx, reject));

  // Add the listener
  if (is_event_emitter(ctx, emitter)) {
    // Store emitter, event_name, and listener in global context for JS eval
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "__temp_emitter", JS_DupValue(ctx, emitter));
    JS_SetPropertyStr(ctx, global, "__temp_event_name", JS_DupValue(ctx, event_name));
    JS_SetPropertyStr(ctx, global, "__temp_listener", JS_DupValue(ctx, listener));

    // Execute JavaScript to call once() method
    const char* call_script = "__temp_emitter.once(__temp_event_name, __temp_listener)";
    JSValue call_result = JS_Eval(ctx, call_script, strlen(call_script), "<once_call>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(call_result)) {
      JSValue exception = JS_GetException(ctx);
      JSValue reject_args[] = {exception};
      JS_Call(ctx, reject, JS_UNDEFINED, 1, reject_args);
      JS_FreeValue(ctx, exception);
    }

    // Clean up global properties
    JSAtom temp_emitter_atom = JS_NewAtom(ctx, "__temp_emitter");
    JSAtom temp_event_name_atom = JS_NewAtom(ctx, "__temp_event_name");
    JSAtom temp_listener_atom = JS_NewAtom(ctx, "__temp_listener");
    JS_DeleteProperty(ctx, global, temp_emitter_atom, 0);
    JS_DeleteProperty(ctx, global, temp_event_name_atom, 0);
    JS_DeleteProperty(ctx, global, temp_listener_atom, 0);
    JS_FreeAtom(ctx, temp_emitter_atom);
    JS_FreeAtom(ctx, temp_event_name_atom);
    JS_FreeAtom(ctx, temp_listener_atom);

    JS_FreeValue(ctx, call_result);
    JS_FreeValue(ctx, global);
  } else if (is_event_target(ctx, emitter)) {
    JSValue add_listener_method = JS_GetPropertyStr(ctx, emitter, "addEventListener");
    if (JS_IsFunction(ctx, add_listener_method)) {
      JSValue options = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx, options, "once", JS_NewBool(ctx, true));
      JSValue listener_args[] = {event_name, listener, options};
      JSValue call_result = JS_Call(ctx, add_listener_method, emitter, 3, listener_args);
      if (JS_IsException(call_result)) {
        // If adding listener fails, reject the promise
        JSValue exception = JS_GetException(ctx);
        JSValue reject_args[] = {exception};
        JS_Call(ctx, reject, JS_UNDEFINED, 1, reject_args);
        JS_FreeValue(ctx, exception);
      }
      JS_FreeValue(ctx, call_result);
      JS_FreeValue(ctx, options);
    }
    JS_FreeValue(ctx, add_listener_method);
  }

  // Handle abort signal if provided and it's a valid AbortSignal
  if (!JS_IsUndefined(abort_signal) && JS_IsObject(abort_signal)) {
    // Check if it has an aborted property and if it's already aborted
    JSValue aborted = JS_GetPropertyStr(ctx, abort_signal, "aborted");
    if (!JS_IsUndefined(aborted) && JS_ToBool(ctx, aborted)) {
      JSValue abort_error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, abort_error, "name", JS_NewString(ctx, "AbortError"));
      JS_SetPropertyStr(ctx, abort_error, "message", JS_NewString(ctx, "The operation was aborted"));
      JSValue reject_args[] = {abort_error};
      JS_Call(ctx, reject, JS_UNDEFINED, 1, reject_args);
      JS_FreeValue(ctx, abort_error);
    }
    JS_FreeValue(ctx, aborted);
  }

  JS_FreeValue(ctx, emitter);
  JS_FreeValue(ctx, event_name);
  JS_FreeValue(ctx, abort_signal);
  JS_FreeValue(ctx, listener);
  JS_FreeValue(ctx, global);

  return JS_UNDEFINED;
}

// Helper function for once listener
JSValue js_events_once_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Get resolve function from global object
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JSValue resolve = JS_GetPropertyStr(ctx, global_obj, "__temp_resolve");

  // Always create an array to match Node.js behavior
  JSValue args_array = JS_NewArray(ctx);
  for (int i = 0; i < argc; i++) {
    JS_SetPropertyUint32(ctx, args_array, i, JS_DupValue(ctx, argv[i]));
  }

  JSValue resolve_args[] = {args_array};
  JS_Call(ctx, resolve, JS_UNDEFINED, 1, resolve_args);
  JS_FreeValue(ctx, args_array);

  // Clean up global variables
  JSAtom resolve_atom = JS_NewAtom(ctx, "__temp_resolve");
  JSAtom reject_atom = JS_NewAtom(ctx, "__temp_reject");
  JS_DeleteProperty(ctx, global_obj, resolve_atom, 0);
  JS_DeleteProperty(ctx, global_obj, reject_atom, 0);
  JS_FreeAtom(ctx, resolve_atom);
  JS_FreeAtom(ctx, reject_atom);

  JS_FreeValue(ctx, resolve);
  JS_FreeValue(ctx, global_obj);
  return JS_UNDEFINED;
}

// events.setMaxListeners(n[, ...eventTargets])
JSValue js_events_set_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "setMaxListeners requires at least 1 argument: (maxListeners)");
  }

  int32_t max_listeners;
  if (JS_ToInt32(ctx, &max_listeners, argv[0]) < 0) {
    return JS_ThrowTypeError(ctx, "Max listeners must be a number");
  }

  if (max_listeners < 0) {
    return JS_ThrowRangeError(ctx, "Max listeners must be non-negative");
  }

  // If no targets specified, set global default
  if (argc == 1) {
    // Set a global default (we'll use a global property)
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "_defaultMaxListeners", JS_NewInt32(ctx, max_listeners));
    JS_FreeValue(ctx, global);
  } else {
    // Set max listeners for specific targets
    for (int i = 1; i < argc; i++) {
      JSValue target = argv[i];
      if (is_event_emitter(ctx, target) || is_event_target(ctx, target)) {
        JS_SetPropertyStr(ctx, target, "_maxListeners", JS_NewInt32(ctx, max_listeners));
      }
    }
  }

  return JS_UNDEFINED;
}

// events.getMaxListeners(emitterOrTarget)
JSValue js_events_get_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "getMaxListeners requires 1 argument: (emitterOrTarget)");
  }

  JSValue target = argv[0];

  // Check for target-specific max listeners
  JSValue max_listeners = JS_GetPropertyStr(ctx, target, "_maxListeners");
  if (!JS_IsUndefined(max_listeners)) {
    return max_listeners;
  }
  JS_FreeValue(ctx, max_listeners);

  // Check for global default
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue default_max = JS_GetPropertyStr(ctx, global, "_defaultMaxListeners");
  JS_FreeValue(ctx, global);

  if (!JS_IsUndefined(default_max)) {
    return default_max;
  }
  JS_FreeValue(ctx, default_max);

  // Return Node.js default of 10
  return JS_NewInt32(ctx, 10);
}

// events.addAbortListener(signal, listener)
JSValue js_events_add_abort_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "addAbortListener requires 2 arguments: (signal, listener)");
  }

  JSValue signal = argv[0];
  JSValue listener = argv[1];

  if (!JS_IsFunction(ctx, listener)) {
    return JS_ThrowTypeError(ctx, "Listener must be a function");
  }

  // Check if signal has addEventListener method (AbortSignal-like)
  JSValue add_event_listener = JS_GetPropertyStr(ctx, signal, "addEventListener");
  if (JS_IsFunction(ctx, add_event_listener)) {
    // Use addEventListener with once: true
    JSValue options = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, options, "once", JS_NewBool(ctx, true));

    JSValue args[] = {JS_NewString(ctx, "abort"), listener, options};
    JS_Call(ctx, add_event_listener, signal, 3, args);

    JS_FreeValue(ctx, args[0]);  // Free the "abort" string
    JS_FreeValue(ctx, options);
  }
  JS_FreeValue(ctx, add_event_listener);

  // Return a disposable object
  JSValue disposable = JS_NewObject(ctx);
  JSValue dispose_fn = JS_NewCFunction(ctx, js_events_abort_disposable, "dispose", 0);
  JS_SetPropertyStr(ctx, dispose_fn, "_signal", JS_DupValue(ctx, signal));
  JS_SetPropertyStr(ctx, dispose_fn, "_listener", JS_DupValue(ctx, listener));
  JS_SetPropertyStr(ctx, disposable, "dispose", dispose_fn);

  // Add Symbol.dispose if available
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue symbol_ctor = JS_GetPropertyStr(ctx, global, "Symbol");
  if (!JS_IsUndefined(symbol_ctor)) {
    JSValue dispose_symbol = JS_GetPropertyStr(ctx, symbol_ctor, "dispose");
    if (!JS_IsUndefined(dispose_symbol)) {
      JSAtom dispose_atom = JS_ValueToAtom(ctx, dispose_symbol);
      if (dispose_atom != JS_ATOM_NULL) {
        JS_SetProperty(ctx, disposable, dispose_atom, JS_DupValue(ctx, dispose_fn));
        JS_FreeAtom(ctx, dispose_atom);
      }
    }
    JS_FreeValue(ctx, dispose_symbol);
  }
  JS_FreeValue(ctx, symbol_ctor);
  JS_FreeValue(ctx, global);

  return disposable;
}

// Helper function for abort listener disposable
JSValue js_events_abort_disposable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue signal = JS_GetPropertyStr(ctx, this_val, "_signal");
  JSValue listener = JS_GetPropertyStr(ctx, this_val, "_listener");

  // Try to remove the listener
  JSValue remove_event_listener = JS_GetPropertyStr(ctx, signal, "removeEventListener");
  if (JS_IsFunction(ctx, remove_event_listener)) {
    JSValue args[] = {JS_NewString(ctx, "abort"), listener};
    JS_Call(ctx, remove_event_listener, signal, 2, args);
    JS_FreeValue(ctx, args[0]);  // Free the "abort" string
  }
  JS_FreeValue(ctx, remove_event_listener);

  JS_FreeValue(ctx, signal);
  JS_FreeValue(ctx, listener);

  return JS_UNDEFINED;
}