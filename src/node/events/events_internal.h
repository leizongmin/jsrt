#ifndef EVENTS_INTERNAL_H
#define EVENTS_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../util/debug.h"
#include "../node_modules.h"

// Security and validation constants
#define MAX_EVENT_NAME_LENGTH 1024
#define MAX_LISTENERS_PER_EVENT 10000

// Input validation macros for security
#define VALIDATE_EVENT_NAME(ctx, name)                                                                     \
  do {                                                                                                     \
    if (!name || strlen(name) == 0 || strlen(name) > MAX_EVENT_NAME_LENGTH) {                              \
      return JS_ThrowTypeError(ctx, "Invalid event name: must be non-empty string under 1024 characters"); \
    }                                                                                                      \
  } while (0)

#define VALIDATE_LISTENER_FUNCTION(ctx, listener)                   \
  do {                                                              \
    if (!JS_IsFunction(ctx, listener)) {                            \
      return JS_ThrowTypeError(ctx, "Listener must be a function"); \
    }                                                               \
  } while (0)

#define VALIDATE_LISTENER_COUNT(ctx, count)                                          \
  do {                                                                               \
    if (count > MAX_LISTENERS_PER_EVENT) {                                           \
      return JS_ThrowRangeError(ctx, "Too many listeners: maximum 10000 per event"); \
    }                                                                                \
  } while (0)

#define SAFE_BOUNDS_CHECK(ctx, index, array_length)                \
  do {                                                             \
    if (index >= array_length) {                                   \
      return JS_ThrowRangeError(ctx, "Array index out of bounds"); \
    }                                                              \
  } while (0)

// Helper function declarations
bool is_event_emitter(JSContext* ctx, JSValueConst this_val);
JSValue get_or_create_events(JSContext* ctx, JSValueConst this_val);
uint32_t get_array_length(JSContext* ctx, JSValueConst array);
JSValue get_or_create_max_listeners(JSContext* ctx, JSValueConst this_val);

// Global variable for once wrapper communication
extern JSValue current_once_wrapper;

// Native wrapper functions for security
JSValue create_once_wrapper(JSContext* ctx, JSValueConst emitter, JSValueConst event_name, JSValueConst listener);
JSValue once_wrapper_function(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue create_prepend_once_wrapper(JSContext* ctx, JSValueConst emitter, JSValueConst event_name,
                                    JSValueConst listener);
JSValue prepend_once_wrapper_function(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Core EventEmitter method declarations
JSValue js_event_emitter_on(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_add_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_once(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_remove_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_emit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_listener_count(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_remove_all_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Enhanced EventEmitter method declarations
JSValue js_event_emitter_prepend_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_prepend_once_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_event_names(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_raw_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_off(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_set_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_get_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Constructor declaration
JSValue js_event_emitter_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

// EventTarget class declarations
JSValue js_event_target_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_event_target_add_event_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_target_remove_event_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_target_dispatch_event(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Event class declarations
JSValue js_event_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_custom_event_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

// Event method declarations
JSValue js_event_prevent_default(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_stop_propagation(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_stop_immediate_propagation(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// EventTarget helper functions
bool is_event_target(JSContext* ctx, JSValueConst this_val);
JSValue get_or_create_event_listeners(JSContext* ctx, JSValueConst this_val);

// Static utility method declarations
JSValue js_events_get_event_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_events_once(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_events_once_executor(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_events_once_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_events_set_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_events_get_max_listeners(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_events_add_abort_listener(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_events_abort_disposable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Error handling enhancement declarations
void init_error_handling_symbols(JSContext* ctx);
JSValue js_events_get_error_monitor(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_emit_with_error_handling(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void setup_promise_rejection_handling(JSContext* ctx, JSValueConst emitter, JSValue promise);
JSValue handle_async_rejection(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_set_capture_rejections(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_event_emitter_get_capture_rejections(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

#endif  // EVENTS_INTERNAL_H