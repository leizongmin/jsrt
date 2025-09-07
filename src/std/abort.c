#include "abort.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../jsrt.h"
#include "../util/debug.h"
#include "event.h"

// Forward declare class IDs
static JSClassID JSRT_AbortControllerClassID;
static JSClassID JSRT_AbortSignalClassID;

// Forward declare internal functions
static void JSRT_AbortSignal_DoAbort(JSContext* ctx, JSValue signal_val, JSValue reason);

// Timeout callback structures and functions
typedef struct {
  JSContext* ctx;
  JSValue signal;
  JSValue reason;
  uv_timer_t* timer;
} JSRT_TimeoutData;

static void jsrt_timeout_close_callback(uv_handle_t* handle) {
  JSRT_TimeoutData* data = (JSRT_TimeoutData*)handle->data;
  free(data);
  free(handle);
}

static void jsrt_timeout_callback(uv_timer_t* timer) {
  JSRT_TimeoutData* data = (JSRT_TimeoutData*)timer->data;

  // Abort the signal with TimeoutError
  JSRT_AbortSignal_DoAbort(data->ctx, data->signal, data->reason);

  // Cleanup
  JS_FreeValue(data->ctx, data->signal);
  JS_FreeValue(data->ctx, data->reason);
  uv_timer_stop(timer);
  uv_close((uv_handle_t*)timer, jsrt_timeout_close_callback);
}

// AbortSignal.any() now uses JavaScript closures instead of C structures

// AbortSignal implementation (extends EventTarget)
typedef struct {
  JSValue event_target;  // EventTarget object this extends
  bool aborted;
  JSValue reason;
} JSRT_AbortSignal;

static void JSRT_AbortSignalFinalize(JSRuntime* rt, JSValue val) {
  JSRT_AbortSignal* signal = JS_GetOpaque(val, JSRT_AbortSignalClassID);
  if (signal) {
    JS_FreeValueRT(rt, signal->event_target);
    if (!JS_IsUndefined(signal->reason)) {
      JS_FreeValueRT(rt, signal->reason);
    }
    free(signal);
  }
}

static JSClassDef JSRT_AbortSignalClass = {
    .class_name = "AbortSignal",
    .finalizer = JSRT_AbortSignalFinalize,
};

static JSValue JSRT_AbortSignalConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  // AbortSignal constructor should not be called directly
  return JS_ThrowTypeError(ctx, "Illegal constructor");
}

// Create an AbortSignal instance
static JSValue JSRT_CreateAbortSignal(JSContext* ctx, bool aborted, JSValue reason) {
  JSRT_AbortSignal* signal = malloc(sizeof(JSRT_AbortSignal));
  signal->aborted = aborted;
  signal->reason = JS_DupValue(ctx, reason);

  // Create EventTarget for the signal
  JSValue event_target_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "EventTarget");
  JSValue event_target = JS_CallConstructor(ctx, event_target_ctor, 0, NULL);
  signal->event_target = event_target;
  JS_FreeValue(ctx, event_target_ctor);

  JSValue obj = JS_NewObjectClass(ctx, JSRT_AbortSignalClassID);
  JS_SetOpaque(obj, signal);

  return obj;
}

static JSValue JSRT_AbortSignalGetAborted(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_AbortSignal* signal = JS_GetOpaque2(ctx, this_val, JSRT_AbortSignalClassID);
  if (!signal) {
    return JS_EXCEPTION;
  }
  return JS_NewBool(ctx, signal->aborted);
}

static JSValue JSRT_AbortSignalGetReason(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_AbortSignal* signal = JS_GetOpaque2(ctx, this_val, JSRT_AbortSignalClassID);
  if (!signal) {
    return JS_EXCEPTION;
  }
  return JS_DupValue(ctx, signal->reason);
}

static JSValue JSRT_AbortSignalAddEventListener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_AbortSignal* signal = JS_GetOpaque2(ctx, this_val, JSRT_AbortSignalClassID);
  if (!signal) {
    return JS_EXCEPTION;
  }

  // Delegate to EventTarget
  JSValue addEventListener = JS_GetPropertyStr(ctx, signal->event_target, "addEventListener");
  JSValue result = JS_Call(ctx, addEventListener, signal->event_target, argc, argv);
  JS_FreeValue(ctx, addEventListener);
  return result;
}

static JSValue JSRT_AbortSignalRemoveEventListener(JSContext* ctx, JSValueConst this_val, int argc,
                                                   JSValueConst* argv) {
  JSRT_AbortSignal* signal = JS_GetOpaque2(ctx, this_val, JSRT_AbortSignalClassID);
  if (!signal) {
    return JS_EXCEPTION;
  }

  // Delegate to EventTarget
  JSValue removeEventListener = JS_GetPropertyStr(ctx, signal->event_target, "removeEventListener");
  JSValue result = JS_Call(ctx, removeEventListener, signal->event_target, argc, argv);
  JS_FreeValue(ctx, removeEventListener);
  return result;
}

static JSValue JSRT_AbortSignalDispatchEvent(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_AbortSignal* signal = JS_GetOpaque2(ctx, this_val, JSRT_AbortSignalClassID);
  if (!signal) {
    return JS_EXCEPTION;
  }

  // Set the event's internal target field to this AbortSignal before dispatching
  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    extern JSClassID JSRT_EventClassID;

    typedef struct {
      char* type;
      JSValue target;
      JSValue currentTarget;
      bool bubbles;
      bool cancelable;
      bool defaultPrevented;
      bool stopPropagationFlag;
      bool stopImmediatePropagationFlag;
    } JSRT_Event_Internal;

    JSRT_Event_Internal* event_struct = JS_GetOpaque(argv[0], JSRT_EventClassID);
    if (event_struct) {
      // Free any existing target and set new one
      if (!JS_IsUndefined(event_struct->target)) {
        JS_FreeValue(ctx, event_struct->target);
      }
      event_struct->target = JS_DupValue(ctx, this_val);
    }
  }

  // Delegate to EventTarget
  JSValue dispatchEvent = JS_GetPropertyStr(ctx, signal->event_target, "dispatchEvent");
  JSValue result = JS_Call(ctx, dispatchEvent, signal->event_target, argc, argv);
  JS_FreeValue(ctx, dispatchEvent);
  return result;
}

// Static methods
static JSValue JSRT_AbortSignalAbort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue reason = JS_UNDEFINED;
  if (argc > 0) {
    reason = argv[0];
  } else {
    // Create a DOMException for the default reason
    reason = JS_NewString(ctx, "AbortError");
  }

  JSValue signal = JSRT_CreateAbortSignal(ctx, true, reason);
  return signal;
}

static JSValue JSRT_AbortSignalTimeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "AbortSignal.timeout() requires 1 argument");
  }

  int64_t delay;
  if (JS_ToInt64(ctx, &delay, argv[0])) {
    return JS_EXCEPTION;
  }

  if (delay < 0) {
    return JS_ThrowRangeError(ctx, "timeout must be non-negative");
  }

  JSValue signal = JSRT_CreateAbortSignal(ctx, false, JS_UNDEFINED);

  // Create TimeoutError DOMException for the abort reason
  JSValue dom_exception_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "DOMException");
  JSValue args[2] = {JS_NewString(ctx, "The operation timed out."), JS_NewString(ctx, "TimeoutError")};
  JSValue timeout_reason = JS_CallConstructor(ctx, dom_exception_ctor, 2, args);
  JS_FreeValue(ctx, dom_exception_ctor);
  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);

  // Set up libuv timer for the timeout
  if (delay > 0) {
    // Get the runtime for libuv loop access
    JSRuntime* rt = JS_GetRuntime(ctx);
    JSRT_Runtime* jsrt_rt = JS_GetRuntimeOpaque(rt);

    if (jsrt_rt && jsrt_rt->uv_loop) {
      uv_timer_t* timer = malloc(sizeof(uv_timer_t));
      uv_timer_init(jsrt_rt->uv_loop, timer);

      // Store signal and reason in timer data for callback
      JSRT_TimeoutData* timeout_data = malloc(sizeof(JSRT_TimeoutData));
      timeout_data->ctx = ctx;
      timeout_data->signal = JS_DupValue(ctx, signal);
      timeout_data->reason = JS_DupValue(ctx, timeout_reason);
      timeout_data->timer = timer;
      timer->data = timeout_data;

      // Start the timer - callback will abort the signal when timeout occurs
      uv_timer_start(timer, jsrt_timeout_callback, delay, 0);
    }
  }

  JS_FreeValue(ctx, timeout_reason);

  return signal;
}

static JSValue JSRT_AbortSignalAny(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "AbortSignal.any() requires 1 argument");
  }

  JSValue signals_val = argv[0];

  // Simplified iterable check - just check if it's array-like
  JSValue length_val = JS_GetPropertyStr(ctx, signals_val, "length");
  if (JS_IsUndefined(length_val)) {
    JS_FreeValue(ctx, length_val);
    return JS_ThrowTypeError(ctx, "AbortSignal.any() argument must be iterable");
  }

  int32_t length;
  if (JS_ToInt32(ctx, &length, length_val)) {
    JS_FreeValue(ctx, length_val);
    return JS_EXCEPTION;
  }
  JS_FreeValue(ctx, length_val);

  if (length == 0) {
    // Return a never-aborted signal
    return JSRT_CreateAbortSignal(ctx, false, JS_UNDEFINED);
  }

  // Check if any signal is already aborted
  for (int32_t i = 0; i < length; i++) {
    JSValue item = JS_GetPropertyUint32(ctx, signals_val, i);
    if (JS_IsException(item)) {
      return item;
    }

    JSRT_AbortSignal* signal = JS_GetOpaque2(ctx, item, JSRT_AbortSignalClassID);
    if (!signal) {
      JS_FreeValue(ctx, item);
      return JS_ThrowTypeError(ctx, "AbortSignal.any() all elements must be AbortSignal objects");
    }

    if (signal->aborted) {
      // Create an AbortController and abort it immediately with the same reason
      JSValue controller_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "AbortController");
      JSValue controller = JS_CallConstructor(ctx, controller_ctor, 0, NULL);
      JS_FreeValue(ctx, controller_ctor);

      if (JS_IsException(controller)) {
        JS_FreeValue(ctx, item);
        return JS_EXCEPTION;
      }

      // Get the controller's signal
      JSValue result_signal = JS_GetPropertyStr(ctx, controller, "signal");

      // Abort the controller with the same reason
      JSValue abort_method = JS_GetPropertyStr(ctx, controller, "abort");
      JSValue abort_result = JS_Call(ctx, abort_method, controller, 1, &signal->reason);

      // Clean up
      JS_FreeValue(ctx, abort_method);
      JS_FreeValue(ctx, abort_result);
      JS_FreeValue(ctx, controller);
      JS_FreeValue(ctx, item);

      return result_signal;
    }
    JS_FreeValue(ctx, item);
  }

  // Create an AbortController for the result (like the JavaScript polyfill)
  JSValue controller_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "AbortController");
  JSValue result_controller = JS_CallConstructor(ctx, controller_ctor, 0, NULL);
  JS_FreeValue(ctx, controller_ctor);

  if (JS_IsException(result_controller)) {
    return JS_EXCEPTION;
  }

  // Get the controller's signal
  JSValue result_signal = JS_GetPropertyStr(ctx, result_controller, "signal");
  if (JS_IsException(result_signal)) {
    JS_FreeValue(ctx, result_controller);
    return JS_EXCEPTION;
  }

  // Set up event listeners on each input signal using JavaScript closures
  for (int32_t i = 0; i < length; i++) {
    JSValue item = JS_GetPropertyUint32(ctx, signals_val, i);
    if (JS_IsException(item)) {
      continue;
    }

    JSRT_AbortSignal* input_signal = JS_GetOpaque2(ctx, item, JSRT_AbortSignalClassID);
    if (!input_signal) {
      JS_FreeValue(ctx, item);
      continue;
    }

    // Create a JavaScript function that captures both controller and input signal
    // This gives the listener access to the input signal's reason property
    const char* js_listener_code =
        "(function(controller, inputSignal) {"
        "  return function(event) {"
        "    var resultSignal = controller.signal;"
        "    if (!resultSignal.aborted) {"
        "      controller.abort(inputSignal.reason);"
        "    }"
        "  };"
        "})";

    JSValue closure_factory =
        JS_Eval(ctx, js_listener_code, strlen(js_listener_code), "<AbortSignal.any>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(closure_factory)) {
      JS_FreeValue(ctx, item);
      continue;
    }

    // Call the factory with our controller AND the input signal to create the actual listener
    JSValue factory_args[] = {result_controller, item};
    JSValue listener_func = JS_Call(ctx, closure_factory, JS_UNDEFINED, 2, factory_args);
    JS_FreeValue(ctx, closure_factory);

    if (JS_IsException(listener_func)) {
      JS_FreeValue(ctx, item);
      continue;
    }

    // Add the JavaScript event listener to input signal
    JSValue addEventListener = JS_GetPropertyStr(ctx, item, "addEventListener");
    if (JS_IsException(addEventListener)) {
      JS_FreeValue(ctx, listener_func);
      JS_FreeValue(ctx, item);
      continue;
    }

    JSValue listener_args[] = {JS_NewString(ctx, "abort"), listener_func};
    JSValue add_result = JS_Call(ctx, addEventListener, item, 2, listener_args);

    // Check if addEventListener succeeded (no debug output in production)
    if (JS_IsException(add_result)) {
      // Silent failure, continue with other signals
    }

    // Clean up
    JS_FreeValue(ctx, addEventListener);
    JS_FreeValue(ctx, listener_args[0]);
    JS_FreeValue(ctx, listener_func);
    JS_FreeValue(ctx, add_result);
    JS_FreeValue(ctx, item);
  }

  // Don't free the controller here - it needs to stay alive for the event listeners
  return result_signal;
}

// Helper function to abort a signal programmatically (similar to controller.abort())
static void JSRT_AbortSignal_DoAbort(JSContext* ctx, JSValue signal_val, JSValue reason) {
  JSRT_AbortSignal* signal = JS_GetOpaque2(ctx, signal_val, JSRT_AbortSignalClassID);
  if (!signal || signal->aborted) {
    return;  // Already aborted or invalid
  }

  // Set reason
  JS_FreeValue(ctx, signal->reason);
  signal->reason = JS_DupValue(ctx, reason);
  signal->aborted = true;

  // Create abort event with proper target
  JSValue event_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Event");
  JSValue abort_event = JS_CallConstructor(ctx, event_ctor, 1, (JSValueConst[]){JS_NewString(ctx, "abort")});
  JS_FreeValue(ctx, event_ctor);

  // Set the event's internal target field to this signal
  // We need to access the Event structure directly since target is read-only
  extern JSClassID JSRT_EventClassID;  // Declare the class ID from event.c

  // Get the Event structure and set its target field
  typedef struct {
    char* type;
    JSValue target;
    JSValue currentTarget;
    bool bubbles;
    bool cancelable;
    bool defaultPrevented;
    bool stopPropagationFlag;
    bool stopImmediatePropagationFlag;
  } JSRT_Event_Internal;

  JSRT_Event_Internal* event_struct = JS_GetOpaque(abort_event, JSRT_EventClassID);
  if (event_struct) {
    // Free any existing target and set new one
    if (!JS_IsUndefined(event_struct->target)) {
      JS_FreeValue(ctx, event_struct->target);
    }
    event_struct->target = JS_DupValue(ctx, signal_val);
  }

  // Call onabort handler if it exists
  JSValue onabort = JS_GetPropertyStr(ctx, signal_val, "onabort");
  if (!JS_IsUndefined(onabort) && !JS_IsNull(onabort)) {
    JSValue onabort_result = JS_Call(ctx, onabort, signal_val, 1, &abort_event);
    JS_FreeValue(ctx, onabort_result);
  }
  JS_FreeValue(ctx, onabort);

  // Dispatch the event through the signal itself (not the underlying EventTarget)
  JSValue dispatchEvent = JS_GetPropertyStr(ctx, signal_val, "dispatchEvent");
  JSValue result = JS_Call(ctx, dispatchEvent, signal_val, 1, &abort_event);
  JS_FreeValue(ctx, dispatchEvent);
  JS_FreeValue(ctx, result);
  JS_FreeValue(ctx, abort_event);
}

// JSRT_AbortAnyEventListener removed - now using JavaScript closures for better event integration

// AbortController implementation
typedef struct {
  JSValue signal;
} JSRT_AbortController;

static void JSRT_AbortControllerFinalize(JSRuntime* rt, JSValue val) {
  JSRT_AbortController* controller = JS_GetOpaque(val, JSRT_AbortControllerClassID);
  if (controller) {
    JS_FreeValueRT(rt, controller->signal);
    free(controller);
  }
}

static JSClassDef JSRT_AbortControllerClass = {
    .class_name = "AbortController",
    .finalizer = JSRT_AbortControllerFinalize,
};

static JSValue JSRT_AbortControllerConstructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSRT_AbortController* controller = malloc(sizeof(JSRT_AbortController));

  // Create associated AbortSignal
  controller->signal = JSRT_CreateAbortSignal(ctx, false, JS_UNDEFINED);

  JSValue obj = JS_NewObjectClass(ctx, JSRT_AbortControllerClassID);
  JS_SetOpaque(obj, controller);
  return obj;
}

static JSValue JSRT_AbortControllerGetSignal(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_AbortController* controller = JS_GetOpaque2(ctx, this_val, JSRT_AbortControllerClassID);
  if (!controller) {
    return JS_EXCEPTION;
  }
  return JS_DupValue(ctx, controller->signal);
}

static JSValue JSRT_AbortControllerAbort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_AbortController* controller = JS_GetOpaque2(ctx, this_val, JSRT_AbortControllerClassID);
  if (!controller) {
    return JS_EXCEPTION;
  }

  // Get the signal
  JSRT_AbortSignal* signal = JS_GetOpaque2(ctx, controller->signal, JSRT_AbortSignalClassID);
  if (!signal) {
    return JS_EXCEPTION;
  }

  // If already aborted, do nothing
  if (signal->aborted) {
    return JS_UNDEFINED;
  }

  // Set reason
  JSValue reason;
  if (argc > 0) {
    reason = argv[0];
  } else {
    // Create default DOMException for AbortError
    JSValue dom_exception_ctor = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "DOMException");
    JSValue args[2] = {JS_NewString(ctx, "The operation was aborted."), JS_NewString(ctx, "AbortError")};
    reason = JS_CallConstructor(ctx, dom_exception_ctor, 2, args);
    JS_FreeValue(ctx, dom_exception_ctor);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
  }

  // Use the centralized abort function to ensure consistent behavior
  JSRT_AbortSignal_DoAbort(ctx, controller->signal, reason);

  return JS_UNDEFINED;
}

// Setup function
void JSRT_RuntimeSetupStdAbort(JSRT_Runtime* rt) {
  JSContext* ctx = rt->ctx;

  JSRT_Debug("JSRT_RuntimeSetupStdAbort: initializing AbortController/AbortSignal API");

  // Register AbortSignal class
  JS_NewClassID(&JSRT_AbortSignalClassID);
  JS_NewClass(rt->rt, JSRT_AbortSignalClassID, &JSRT_AbortSignalClass);

  JSValue signal_proto = JS_NewObject(ctx);

  // Properties
  JSValue get_aborted = JS_NewCFunction(ctx, JSRT_AbortSignalGetAborted, "get aborted", 0);
  JSValue get_reason = JS_NewCFunction(ctx, JSRT_AbortSignalGetReason, "get reason", 0);
  JSAtom aborted_atom = JS_NewAtom(ctx, "aborted");
  JSAtom reason_atom = JS_NewAtom(ctx, "reason");
  JS_DefinePropertyGetSet(ctx, signal_proto, aborted_atom, get_aborted, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, signal_proto, reason_atom, get_reason, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, aborted_atom);
  JS_FreeAtom(ctx, reason_atom);

  // EventTarget methods
  JS_SetPropertyStr(ctx, signal_proto, "addEventListener",
                    JS_NewCFunction(ctx, JSRT_AbortSignalAddEventListener, "addEventListener", 3));
  JS_SetPropertyStr(ctx, signal_proto, "removeEventListener",
                    JS_NewCFunction(ctx, JSRT_AbortSignalRemoveEventListener, "removeEventListener", 3));
  JS_SetPropertyStr(ctx, signal_proto, "dispatchEvent",
                    JS_NewCFunction(ctx, JSRT_AbortSignalDispatchEvent, "dispatchEvent", 1));

  JS_SetClassProto(ctx, JSRT_AbortSignalClassID, signal_proto);

  JSValue signal_ctor = JS_NewCFunction2(ctx, JSRT_AbortSignalConstructor, "AbortSignal", 0, JS_CFUNC_constructor, 0);

  // Static methods
  JS_SetPropertyStr(ctx, signal_ctor, "abort", JS_NewCFunction(ctx, JSRT_AbortSignalAbort, "abort", 1));
  JS_SetPropertyStr(ctx, signal_ctor, "timeout", JS_NewCFunction(ctx, JSRT_AbortSignalTimeout, "timeout", 1));
  JS_SetPropertyStr(ctx, signal_ctor, "any", JS_NewCFunction(ctx, JSRT_AbortSignalAny, "any", 1));

  JS_SetPropertyStr(ctx, rt->global, "AbortSignal", signal_ctor);

  // Register AbortController class
  JS_NewClassID(&JSRT_AbortControllerClassID);
  JS_NewClass(rt->rt, JSRT_AbortControllerClassID, &JSRT_AbortControllerClass);

  JSValue controller_proto = JS_NewObject(ctx);

  // Properties
  JSValue get_signal = JS_NewCFunction(ctx, JSRT_AbortControllerGetSignal, "get signal", 0);
  JSAtom signal_atom = JS_NewAtom(ctx, "signal");
  JS_DefinePropertyGetSet(ctx, controller_proto, signal_atom, get_signal, JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, signal_atom);

  // Methods
  JS_SetPropertyStr(ctx, controller_proto, "abort", JS_NewCFunction(ctx, JSRT_AbortControllerAbort, "abort", 1));

  JS_SetClassProto(ctx, JSRT_AbortControllerClassID, controller_proto);

  JSValue controller_ctor =
      JS_NewCFunction2(ctx, JSRT_AbortControllerConstructor, "AbortController", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, rt->global, "AbortController", controller_ctor);

  JSRT_Debug("AbortController/AbortSignal API setup completed");
}