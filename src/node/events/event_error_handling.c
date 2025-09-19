#include "events_internal.h"

// Define the error monitor symbol
static JSAtom error_monitor_atom = JS_ATOM_NULL;
static JSAtom capture_rejections_atom = JS_ATOM_NULL;
static JSAtom nodejs_rejection_atom = JS_ATOM_NULL;

// Initialize error handling symbols
void init_error_handling_symbols(JSContext* ctx) {
  if (error_monitor_atom == JS_ATOM_NULL) {
    error_monitor_atom = JS_NewAtom(ctx, "nodejs.errorMonitor");
    capture_rejections_atom = JS_NewAtom(ctx, "captureRejections");
    nodejs_rejection_atom = JS_NewAtom(ctx, "nodejs.rejection");
  }
}

// Get the errorMonitor symbol
JSValue js_events_get_error_monitor(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  init_error_handling_symbols(ctx);

  // Create a symbol for error monitoring
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue symbol_ctor = JS_GetPropertyStr(ctx, global, "Symbol");

  if (!JS_IsUndefined(symbol_ctor)) {
    JSValue symbol_for = JS_GetPropertyStr(ctx, symbol_ctor, "for");
    if (JS_IsFunction(ctx, symbol_for)) {
      JSValue key = JS_NewString(ctx, "nodejs.errorMonitor");
      JSValue args[] = {key};
      JSValue error_monitor_symbol = JS_Call(ctx, symbol_for, symbol_ctor, 1, args);
      JS_FreeValue(ctx, key);
      JS_FreeValue(ctx, symbol_for);
      JS_FreeValue(ctx, symbol_ctor);
      JS_FreeValue(ctx, global);
      return error_monitor_symbol;
    }
    JS_FreeValue(ctx, symbol_for);
  }

  JS_FreeValue(ctx, symbol_ctor);
  JS_FreeValue(ctx, global);

  // Fallback: return a simple string identifier
  return JS_NewString(ctx, "nodejs.errorMonitor");
}

// Enhanced error event handling for EventEmitter
JSValue js_event_emitter_emit_with_error_handling(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "emit() requires at least 1 argument");
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name) {
    return JS_ThrowTypeError(ctx, "Event name must be a string");
  }

  bool is_error_event = strcmp(event_name, "error") == 0;

  // Check for captureRejections setting
  JSValue capture_rejections = JS_GetPropertyStr(ctx, this_val, "captureRejections");
  bool should_capture = JS_ToBool(ctx, capture_rejections);
  JS_FreeValue(ctx, capture_rejections);

  // If this is an error event, first emit to errorMonitor listeners
  if (is_error_event) {
    JSValue error_monitor = js_events_get_error_monitor(ctx, JS_UNDEFINED, 0, NULL);

    // Get errorMonitor listeners
    JSValue events_obj = get_or_create_events(ctx, this_val);
    JSAtom monitor_atom = JS_ValueToAtom(ctx, error_monitor);
    if (monitor_atom != JS_ATOM_NULL) {
      JSValue monitor_listeners = JS_GetProperty(ctx, events_obj, monitor_atom);

      if (JS_IsArray(ctx, monitor_listeners)) {
        uint32_t length = get_array_length(ctx, monitor_listeners);
        for (uint32_t i = 0; i < length; i++) {
          JSValue listener = JS_GetPropertyUint32(ctx, monitor_listeners, i);

          // Call monitor listener with error arguments
          JSValue result = JS_Call(ctx, listener, this_val, argc - 1, argv + 1);

          // Handle async rejections if captureRejections is enabled
          if (should_capture && JS_IsObject(result)) {
            JSValue is_promise = JS_GetPropertyStr(ctx, result, "then");
            if (JS_IsFunction(ctx, is_promise)) {
              // This is a Promise - set up rejection handling
              setup_promise_rejection_handling(ctx, this_val, result);
            }
            JS_FreeValue(ctx, is_promise);
          }

          if (JS_IsException(result)) {
            // Log but don't throw - monitor listeners shouldn't break normal flow
            JS_FreeValue(ctx, result);
          } else {
            JS_FreeValue(ctx, result);
          }

          JS_FreeValue(ctx, listener);
        }
      }

      JS_FreeValue(ctx, monitor_listeners);
      JS_FreeAtom(ctx, monitor_atom);
    }

    JS_FreeValue(ctx, events_obj);
    JS_FreeValue(ctx, error_monitor);
  }

  // Now call the regular emit function
  JSValue regular_result = js_event_emitter_emit(ctx, this_val, argc, argv);

  // For error events, if no listeners and no errorMonitor, throw the error
  if (is_error_event && JS_ToBool(ctx, regular_result) == false) {
    if (argc >= 2) {
      // Throw the error that was passed as the second argument
      JS_Throw(ctx, JS_DupValue(ctx, argv[1]));
      JS_FreeCString(ctx, event_name);
      return JS_EXCEPTION;
    } else {
      // Create a generic error
      JSValue error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Unhandled error event"));
      JS_Throw(ctx, error);
      JS_FreeCString(ctx, event_name);
      return JS_EXCEPTION;
    }
  }

  JS_FreeCString(ctx, event_name);
  return regular_result;
}

// Set up promise rejection handling for async listeners
void setup_promise_rejection_handling(JSContext* ctx, JSValueConst emitter, JSValue promise) {
  // Create a rejection handler
  JSValue rejection_handler = JS_NewCFunction(ctx, handle_async_rejection, "rejectionHandler", 1);
  JS_SetPropertyStr(ctx, rejection_handler, "_emitter", JS_DupValue(ctx, emitter));

  // Attach to promise.catch()
  JSValue catch_method = JS_GetPropertyStr(ctx, promise, "catch");
  if (JS_IsFunction(ctx, catch_method)) {
    JSValue args[] = {rejection_handler};
    JSValue handled_promise = JS_Call(ctx, catch_method, promise, 1, args);
    JS_FreeValue(ctx, handled_promise);
  }
  JS_FreeValue(ctx, catch_method);
  JS_FreeValue(ctx, rejection_handler);
}

// Handle async function rejections
JSValue handle_async_rejection(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_UNDEFINED;
  }

  JSValue emitter = JS_GetPropertyStr(ctx, this_val, "_emitter");
  JSValue error = argv[0];

  // Check if the emitter has a custom rejection handler
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue symbol_ctor = JS_GetPropertyStr(ctx, global, "Symbol");

  if (!JS_IsUndefined(symbol_ctor)) {
    JSValue symbol_for = JS_GetPropertyStr(ctx, symbol_ctor, "for");
    if (JS_IsFunction(ctx, symbol_for)) {
      JSValue key = JS_NewString(ctx, "nodejs.rejection");
      JSValue args[] = {key};
      JSValue rejection_symbol = JS_Call(ctx, symbol_for, symbol_ctor, 1, args);

      JSValue custom_handler = JS_GetProperty(ctx, emitter, JS_ValueToAtom(ctx, rejection_symbol));
      if (JS_IsFunction(ctx, custom_handler)) {
        // Call custom rejection handler
        JSValue handler_args[] = {error};
        JSValue result = JS_Call(ctx, custom_handler, emitter, 1, handler_args);
        JS_FreeValue(ctx, custom_handler);
        JS_FreeValue(ctx, rejection_symbol);
        JS_FreeValue(ctx, key);
        JS_FreeValue(ctx, symbol_for);
        JS_FreeValue(ctx, symbol_ctor);
        JS_FreeValue(ctx, global);
        JS_FreeValue(ctx, emitter);
        return result;
      }

      JS_FreeValue(ctx, custom_handler);
      JS_FreeValue(ctx, rejection_symbol);
      JS_FreeValue(ctx, key);
    }
    JS_FreeValue(ctx, symbol_for);
  }
  JS_FreeValue(ctx, symbol_ctor);
  JS_FreeValue(ctx, global);

  // Fallback: emit as error event
  JSValue emit_method = JS_GetPropertyStr(ctx, emitter, "emit");
  if (JS_IsFunction(ctx, emit_method)) {
    JSValue error_name = JS_NewString(ctx, "error");
    JSValue emit_args[] = {error_name, error};
    JSValue result = JS_Call(ctx, emit_method, emitter, 2, emit_args);
    JS_FreeValue(ctx, error_name);
    JS_FreeValue(ctx, emit_method);
    JS_FreeValue(ctx, emitter);
    return result;
  }

  JS_FreeValue(ctx, emit_method);
  JS_FreeValue(ctx, emitter);
  return JS_UNDEFINED;
}

// Set captureRejections property on EventEmitter
JSValue js_event_emitter_set_capture_rejections(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "setCaptureRejections requires 1 argument");
  }

  bool capture = JS_ToBool(ctx, argv[0]);
  JS_SetPropertyStr(ctx, this_val, "captureRejections", JS_NewBool(ctx, capture));

  return JS_UNDEFINED;
}

// Get captureRejections property
JSValue js_event_emitter_get_capture_rejections(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue capture = JS_GetPropertyStr(ctx, this_val, "captureRejections");
  if (JS_IsUndefined(capture)) {
    JS_FreeValue(ctx, capture);
    return JS_NewBool(ctx, false);
  }
  return capture;
}