#include "timer.h"

#include <inttypes.h>
#include <quickjs.h>
#include <stdlib.h>

#include "../util/jsutils.h"
#include "../util/macro.h"

typedef struct {
  JSRT_Runtime* rt;
  uv_timer_t uv_timer;
  uint64_t timeout;
  bool is_interval;
  uint64_t timer_id;  // Add our own timer ID field
  JSValue timer_obj;  // Reference to the JS timer object (for clearing opaque)
  JSValue this_val;
  int argc;
  JSValue* argv;
  JSValue callback;
} JSRT_Timer;

// Static counter for generating unique timer IDs
static uint64_t next_timer_id = 1;

static void jsrt_timer_free(JSRT_Timer* timer);
static void jsrt_timer_close_callback(uv_handle_t* handle);

static JSValue jsrt_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_set_interval(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_start_timer(bool is_interval, JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue jsrt_stop_timer(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static void jsrt_on_timer_callback(uv_timer_t* timer);

static JSClassID timer_class_id;
static void jsrt_timer_finalizer(JSRuntime* rt, JSValue val) {
  JSRT_Timer* timer = JS_GetOpaque(val, timer_class_id);
  if (timer) {
    // Prevent double-free: clear opaque pointer immediately
    JS_SetOpaque(val, NULL);

    // Stop and close the timer handle
    // The close callback will check if runtime is still valid before freeing JSValues
    uv_timer_stop(&timer->uv_timer);
    if (!uv_is_closing((uv_handle_t*)&timer->uv_timer)) {
      uv_close((uv_handle_t*)&timer->uv_timer, jsrt_timer_close_callback);
    }
  }
}
static JSClassDef timer_class = {
    "Timer",
    .finalizer = jsrt_timer_finalizer,
};
static const JSCFunctionListEntry timer_proto_funcs[] = {};

void JSRT_RuntimeSetupStdTimer(JSRT_Runtime* rt) {
  JSValue timer_proto;
  JS_NewClassID(&timer_class_id);
  JS_NewClass(rt->rt, timer_class_id, &timer_class);
  timer_proto = JS_NewObject(rt->ctx);
  JS_SetPropertyFunctionList(rt->ctx, timer_proto, timer_proto_funcs, countof(timer_proto_funcs));
  JS_SetClassProto(rt->ctx, timer_class_id, timer_proto);

  JS_SetPropertyStr(rt->ctx, rt->global, "setTimeout", JS_NewCFunction(rt->ctx, jsrt_set_timeout, "setTimeout", 2));
  JS_SetPropertyStr(rt->ctx, rt->global, "setInterval", JS_NewCFunction(rt->ctx, jsrt_set_interval, "setInterval", 2));
  JS_SetPropertyStr(rt->ctx, rt->global, "clearTimeout", JS_NewCFunction(rt->ctx, jsrt_stop_timer, "clearTimeout", 1));
  JS_SetPropertyStr(rt->ctx, rt->global, "clearInterval",
                    JS_NewCFunction(rt->ctx, jsrt_stop_timer, "clearInterval", 1));
}

static JSValue jsrt_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return jsrt_start_timer(false, ctx, this_val, argc, argv);
}

static JSValue jsrt_set_interval(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return jsrt_start_timer(true, ctx, this_val, argc, argv);
}

static JSValue jsrt_start_timer(bool is_interval, JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);

  int64_t timeout = 0;
  JSValue callback;
  if (argc < 1) {
    return JS_ThrowTypeError(rt->ctx, "The \"callback\" argument must be of type function. Received undefined");
  } else if (argc == 1) {
    callback = argv[0];
    // timeout remains 0 for missing timeout argument
  } else {
    callback = argv[0];
    if (JS_IsUndefined(argv[1]) || JS_IsNull(argv[1])) {
      // Undefined or null timeout should be treated as 0
      timeout = 0;
    } else {
      int status = JS_ToInt64(rt->ctx, &timeout, argv[1]);
      if (status != 0) {
        JSRT_Debug("failed to convert timeout to int64_t: status=%d, value=%p", status, &argv[1]);
        timeout = 0;  // Default to 0 on conversion failure
      }
      // Negative timeouts should be clamped to 0 (WPT requirement)
      if (timeout < 0) {
        timeout = 0;
      }
    }
  }
  if (!JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(rt->ctx, "The \"callback\" argument must be of type function. Received type %s",
                             JSRT_GetTypeofJSValue(ctx, callback));
  }

  JSRT_Timer* timer = malloc(sizeof(JSRT_Timer));
  timer->rt = rt;
  timer->uv_timer.data = timer;
  timer->timeout = (uint64_t)timeout;
  timer->is_interval = is_interval;
  timer->timer_id = next_timer_id++;  // Assign our own timer ID
  timer->callback = JS_DupValue(rt->ctx, callback);
  timer->this_val = JS_DupValue(rt->ctx, this_val);
  timer->argc = argc - 2;
  if (timer->argc > 0) {
    timer->argv = malloc(timer->argc * sizeof(JSValue));
    for (int i = 0; i < timer->argc; i++) {
      timer->argv[i] = JS_DupValue(rt->ctx, argv[i + 2]);
    }
  } else {
    timer->argv = NULL;
  }

  uv_timer_init(rt->uv_loop, &timer->uv_timer);

  // For intervals, ensure repeat interval is at least 1ms (0 means no repeat in libuv)
  uint64_t repeat_interval = is_interval ? (timer->timeout > 0 ? timer->timeout : 1) : 0;
  int status = uv_timer_start(&timer->uv_timer, jsrt_on_timer_callback, timer->timeout, repeat_interval);
  if (status != 0) {
    return JS_ThrowInternalError(rt->ctx, "uv_timer_start error: status=%d", status);
  }

  JSValueConst result = JS_NewObjectClass(rt->ctx, timer_class_id);
  if (!JS_IsException(result)) {
    // Store reference to timer object for clearing opaque later
    timer->timer_obj = JS_DupValue(rt->ctx, result);
    JS_SetOpaque(result, timer);
    // Use our own timer_id instead of potentially problematic uv_timer.start_id
    JS_SetPropertyStr(rt->ctx, result, "id", JS_NewInt64(rt->ctx, (int64_t)timer->timer_id));
  }

  return result;
}

static JSValue jsrt_stop_timer(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);

  if (argc > 0) {
    JSRT_Timer* timer = JS_GetOpaque(argv[0], timer_class_id);
    if (timer != NULL) {
      // Clear opaque pointer BEFORE freeing to prevent finalizer from accessing freed memory
      JS_SetOpaque(argv[0], NULL);
      jsrt_timer_free(timer);
    }
  }

  return JS_UNDEFINED;
}

void jsrt_on_timer_callback(uv_timer_t* uv_timer) {
  JSRT_Timer* timer = uv_timer->data;
  if (!timer) {
    JSRT_Debug("Timer callback called with NULL timer data");
    return;
  }

  JSValue this_val = timer->this_val;
  int argc = timer->argc;
  JSValue* argv = timer->argv;
  JSValue callback = timer->callback;

  JSValue ret = JS_Call(timer->rt->ctx, callback, this_val, argc, argv);
  if (JS_IsException(ret)) {
    JSValue e = JS_GetException(timer->rt->ctx);
    JSRT_RuntimeAddExceptionValue(timer->rt, e);
  }
  JSRT_RuntimeFreeValue(timer->rt, ret);

  // Drain any microtasks scheduled during the timer callback to ensure
  // Promise reactions and nextTick handlers run before the event loop continues.
  if (timer->rt) {
    JSRuntime* qjs_runtime = JS_GetRuntime(timer->rt->ctx);
    while (JS_IsJobPending(qjs_runtime)) {
      if (!JSRT_RuntimeRunTicket(timer->rt)) {
        break;
      }
    }
  }

  if (!timer->is_interval && !uv_is_closing((uv_handle_t*)&timer->uv_timer)) {
    // Clear opaque pointer BEFORE freeing to prevent finalizer from accessing freed memory
    JS_SetOpaque(timer->timer_obj, NULL);
    jsrt_timer_free(timer);
  }
}

// Close callback that safely frees the timer after handle is closed
static void jsrt_timer_close_callback(uv_handle_t* handle) {
  if (!handle || !handle->data) {
    JSRT_Debug("Timer close callback called with NULL handle or data");
    return;
  }

  JSRT_Timer* timer = (JSRT_Timer*)handle->data;
  JSRT_Debug("TimerCloseCallback: timer=%p id=%" PRIu64, timer, timer->timer_id);

  // Only free JSValues if runtime and context are still valid
  // During runtime teardown, these may already be freed
  if (timer->rt && timer->rt->ctx) {
    JSRT_RuntimeFreeValue(timer->rt, timer->timer_obj);
    timer->timer_obj = JS_UNDEFINED;
    JSRT_RuntimeFreeValue(timer->rt, timer->callback);
    timer->callback = JS_UNDEFINED;
    JSRT_RuntimeFreeValue(timer->rt, timer->this_val);
    timer->this_val = JS_UNDEFINED;

    // Free the copied arguments
    if (timer->argv) {
      for (int i = 0; i < timer->argc; i++) {
        JSRT_RuntimeFreeValue(timer->rt, timer->argv[i]);
      }
      free(timer->argv);
      timer->argv = NULL;
    }
  } else {
    // Runtime is being torn down, just free the argv array
    if (timer->argv) {
      free(timer->argv);
      timer->argv = NULL;
    }
  }

  free(timer);
}

static void jsrt_timer_free(JSRT_Timer* timer) {
  if (!timer) {
    JSRT_Debug("Timer free called with NULL timer");
    return;
  }

  JSRT_Debug("TimerFree: timer=%p id=%" PRIu64, timer, timer->timer_id);
  int status = uv_timer_stop(&timer->uv_timer);
  JSRT_Debug("uv_timer_stop: id=%" PRIu64 " status=%d", timer->timer_id, status);

  // Close the handle with proper callback to ensure safe cleanup
  uv_close((uv_handle_t*)&timer->uv_timer, jsrt_timer_close_callback);
}
