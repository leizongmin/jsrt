#include <quickjs.h>
#include <stdlib.h>
#include <string.h>
#include "../../util/debug.h"
#include "process.h"

// Forward declaration from quickjs-libc
void js_std_dump_error(JSContext* ctx);

// Event listener structure
typedef struct EventListener {
  char* event_name;
  JSValue callback;
  struct EventListener* next;
} EventListener;

// Global event state
static EventListener* g_event_listeners = NULL;
static JSValue g_process_obj_ref = {0};
static JSContext* g_ctx = NULL;

// Uncaught exception capture callback
static JSValue g_uncaught_exception_capture = {0};

// Add event listener
static void add_event_listener(const char* event_name, JSValue callback) {
  EventListener* listener = malloc(sizeof(EventListener));
  if (!listener)
    return;

  listener->event_name = strdup(event_name);
  listener->callback = JS_DupValue(g_ctx, callback);
  listener->next = g_event_listeners;
  g_event_listeners = listener;

  JSRT_Debug("Added event listener for '%s'", event_name);
}

// Emit event to all listeners
static bool emit_event(const char* event_name, int argc, JSValueConst* argv) {
  if (!g_ctx) {
    return false;
  }

  bool emitted = false;
  EventListener* listener = g_event_listeners;

  while (listener) {
    if (strcmp(listener->event_name, event_name) == 0 && JS_IsFunction(g_ctx, listener->callback)) {
      JSValue result = JS_Call(g_ctx, listener->callback, JS_UNDEFINED, argc, argv);
      if (JS_IsException(result)) {
        JSRT_Debug("Error in event listener for '%s'", event_name);
        js_std_dump_error(g_ctx);
      }
      JS_FreeValue(g_ctx, result);
      emitted = true;
    }
    listener = listener->next;
  }

  return emitted;
}

// Check if event has listeners
static bool has_listeners(const char* event_name) {
  EventListener* listener = g_event_listeners;
  while (listener) {
    if (strcmp(listener->event_name, event_name) == 0) {
      return true;
    }
    listener = listener->next;
  }
  return false;
}

// process.on(event, callback)
JSValue js_process_on_events(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_UNDEFINED;
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, event_name);
    return JS_ThrowTypeError(ctx, "Callback must be a function");
  }

  add_event_listener(event_name, callback);
  JS_FreeCString(ctx, event_name);

  return this_val;  // Return process for chaining
}

// process.emit(event, ...args)
JSValue js_process_emit_events(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name) {
    return JS_EXCEPTION;
  }

  bool emitted = emit_event(event_name, argc - 1, argv + 1);
  JS_FreeCString(ctx, event_name);

  return JS_NewBool(ctx, emitted);
}

// process.removeListener(event, callback)
JSValue js_process_remove_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_UNDEFINED;
  }

  const char* event_name = JS_ToCString(ctx, argv[0]);
  if (!event_name) {
    return JS_EXCEPTION;
  }

  JSValue callback = argv[1];
  if (!JS_IsFunction(ctx, callback)) {
    JS_FreeCString(ctx, event_name);
    return JS_ThrowTypeError(ctx, "Callback must be a function");
  }

  // Remove the listener from the global list
  EventListener** current = &g_event_listeners;
  while (*current) {
    EventListener* listener = *current;
    if (strcmp(listener->event_name, event_name) == 0) {
      // Compare JS values by reference
      bool is_equal = JS_StrictEq(ctx, listener->callback, callback);

      if (is_equal) {
        // Remove this listener
        *current = listener->next;
        free(listener->event_name);
        JS_FreeValue(ctx, listener->callback);
        free(listener);

        JSRT_Debug("Removed event listener for '%s'", event_name);
        break;
      }
    }
    current = &listener->next;
  }

  JS_FreeCString(ctx, event_name);
  return this_val;  // Return process for chaining
}

// Emit 'exit' event
void jsrt_process_emit_exit(JSContext* ctx, int exit_code) {
  JSRT_Debug("Emitting 'exit' event with code %d", exit_code);

  JSValue code_val = JS_NewInt32(ctx, exit_code);
  emit_event("exit", 1, &code_val);
  JS_FreeValue(ctx, code_val);
}

// Emit 'beforeExit' event
void jsrt_process_emit_before_exit(JSContext* ctx, int exit_code) {
  JSRT_Debug("Emitting 'beforeExit' event with code %d", exit_code);

  JSValue code_val = JS_NewInt32(ctx, exit_code);
  emit_event("beforeExit", 1, &code_val);
  JS_FreeValue(ctx, code_val);
}

// process.emitWarning(warning[, options])
JSValue js_process_emit_warning(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "warning argument is required");
  }

  // Create warning object
  JSValue warning_obj = JS_NewObject(ctx);

  // Handle string or Error object
  if (JS_IsString(argv[0])) {
    const char* message = JS_ToCString(ctx, argv[0]);
    JS_SetPropertyStr(ctx, warning_obj, "message", JS_NewString(ctx, message));
    JS_SetPropertyStr(ctx, warning_obj, "name", JS_NewString(ctx, "Warning"));
    JS_FreeCString(ctx, message);
  } else if (JS_IsError(ctx, argv[0])) {
    // Copy properties from error object
    JSValue msg = JS_GetPropertyStr(ctx, argv[0], "message");
    JSValue name = JS_GetPropertyStr(ctx, argv[0], "name");
    JS_SetPropertyStr(ctx, warning_obj, "message", msg);
    JS_SetPropertyStr(ctx, warning_obj, "name", name);
  } else {
    JS_FreeValue(ctx, warning_obj);
    return JS_ThrowTypeError(ctx, "warning must be a string or Error");
  }

  // Parse optional type, code arguments
  if (argc >= 2 && JS_IsString(argv[1])) {
    const char* type = JS_ToCString(ctx, argv[1]);
    JS_SetPropertyStr(ctx, warning_obj, "name", JS_NewString(ctx, type));
    JS_FreeCString(ctx, type);
  }

  if (argc >= 3 && JS_IsString(argv[2])) {
    const char* code = JS_ToCString(ctx, argv[2]);
    JS_SetPropertyStr(ctx, warning_obj, "code", JS_NewString(ctx, code));
    JS_FreeCString(ctx, code);
  }

  JSRT_Debug("Emitting warning");

  // Emit 'warning' event
  bool handled = emit_event("warning", 1, &warning_obj);

  // If no listeners, print to stderr (default behavior)
  if (!handled) {
    JSValue msg = JS_GetPropertyStr(ctx, warning_obj, "message");
    const char* message = JS_ToCString(ctx, msg);
    fprintf(stderr, "(node) Warning: %s\n", message);
    JS_FreeCString(ctx, message);
    JS_FreeValue(ctx, msg);
  }

  JS_FreeValue(ctx, warning_obj);
  return JS_UNDEFINED;
}

// Emit 'uncaughtException' event
bool jsrt_process_emit_uncaught_exception(JSContext* ctx, JSValue error) {
  JSRT_Debug("Emitting 'uncaughtException' event");

  // Check if capture callback is set
  if (!JS_IsUndefined(g_uncaught_exception_capture)) {
    JSValue result = JS_Call(ctx, g_uncaught_exception_capture, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, result);
    return true;  // Handled by capture callback
  }

  // Emit uncaughtExceptionMonitor first (for logging/telemetry)
  emit_event("uncaughtExceptionMonitor", 1, &error);

  // Emit uncaughtException event
  bool handled = emit_event("uncaughtException", 1, &error);

  if (!handled) {
    // Default behavior: print stack trace
    fprintf(stderr, "Uncaught exception:\n");
    js_std_dump_error(ctx);
  }

  return handled;
}

// Emit 'unhandledRejection' event
bool jsrt_process_emit_unhandled_rejection(JSContext* ctx, JSValue reason, JSValue promise) {
  JSRT_Debug("Emitting 'unhandledRejection' event");

  JSValueConst argv[2] = {reason, promise};
  bool handled = emit_event("unhandledRejection", 2, argv);

  if (!handled) {
    // Default behavior: print warning
    fprintf(stderr, "(node) UnhandledPromiseRejectionWarning: ");
    if (JS_IsError(ctx, reason)) {
      js_std_dump_error(ctx);
    } else {
      const char* str = JS_ToCString(ctx, reason);
      fprintf(stderr, "%s\n", str ? str : "Unknown error");
      JS_FreeCString(ctx, str);
    }
  }

  return handled;
}

// Emit 'rejectionHandled' event
void jsrt_process_emit_rejection_handled(JSContext* ctx, JSValue promise) {
  JSRT_Debug("Emitting 'rejectionHandled' event");
  emit_event("rejectionHandled", 1, &promise);
}

// process.setUncaughtExceptionCaptureCallback(fn)
JSValue js_process_set_uncaught_exception_capture(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Check if there are already uncaughtException listeners
  if (!JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0]) && has_listeners("uncaughtException")) {
    return JS_ThrowInternalError(ctx, "Cannot set uncaught exception capture callback when handlers are already set");
  }

  // Free old callback
  if (!JS_IsUndefined(g_uncaught_exception_capture)) {
    JS_FreeValue(ctx, g_uncaught_exception_capture);
    g_uncaught_exception_capture = JS_UNDEFINED;
  }

  // Set new callback
  if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
    // Clear callback
    g_uncaught_exception_capture = JS_UNDEFINED;
  } else if (JS_IsFunction(ctx, argv[0])) {
    g_uncaught_exception_capture = JS_DupValue(ctx, argv[0]);
  } else {
    return JS_ThrowTypeError(ctx, "Callback must be a function or null");
  }

  return JS_UNDEFINED;
}

// process.hasUncaughtExceptionCaptureCallback()
JSValue js_process_has_uncaught_exception_capture(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewBool(ctx, !JS_IsUndefined(g_uncaught_exception_capture));
}

// Setup event system for process object
void jsrt_process_setup_events(JSContext* ctx, JSValue process_obj) {
  g_ctx = ctx;
  g_process_obj_ref = JS_DupValue(ctx, process_obj);

  JSRT_Debug("Process event system initialized");
}

// Cleanup event system
void jsrt_process_cleanup_events(JSContext* ctx) {
  // Free all event listeners
  EventListener* listener = g_event_listeners;
  while (listener) {
    EventListener* next = listener->next;
    free(listener->event_name);
    if (ctx) {
      JS_FreeValue(ctx, listener->callback);
    }
    free(listener);
    listener = next;
  }
  g_event_listeners = NULL;

  // Free capture callback
  if (ctx && !JS_IsUndefined(g_uncaught_exception_capture)) {
    JS_FreeValue(ctx, g_uncaught_exception_capture);
    g_uncaught_exception_capture = JS_UNDEFINED;
  }

  // Free process object reference
  if (ctx && !JS_IsUndefined(g_process_obj_ref)) {
    JS_FreeValue(ctx, g_process_obj_ref);
    g_process_obj_ref = (JSValue){0};
  }

  g_ctx = NULL;

  JSRT_Debug("Process event system cleaned up");
}

// Initialize events module
void jsrt_process_init_events(void) {
  g_uncaught_exception_capture = JS_UNDEFINED;
  JSRT_Debug("Process events module initialized");
}
