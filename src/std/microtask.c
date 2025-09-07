#include "microtask.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../util/debug.h"

// Microtask callback wrapper function
static JSValue microtask_job_func(JSContext* ctx, int argc, JSValueConst* argv) {
  // The callback function is passed as the first argument
  if (argc > 0 && JS_IsFunction(ctx, argv[0])) {
    JSValue result = JS_Call(ctx, argv[0], JS_UNDEFINED, 0, NULL);
    if (JS_IsException(result)) {
      // Log the error but don't propagate it (microtasks should not throw)
      JSRT_Debug("Microtask threw an exception");
    }
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, argv[0]);  // Free the callback function
  }
  return JS_UNDEFINED;
}

static JSValue js_queue_microtask(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "queueMicrotask requires 1 argument");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "queueMicrotask argument must be a function");
  }

  // Use JS_EnqueueJob with a wrapper function
  JSValue job_callback = JS_DupValue(ctx, argv[0]);
  JSValueConst job_args[] = {job_callback};
  int result = JS_EnqueueJob(ctx, microtask_job_func, 1, job_args);

  if (result < 0) {
    JS_FreeValue(ctx, job_callback);
    return JS_ThrowInternalError(ctx, "Failed to enqueue microtask");
  }

  return JS_UNDEFINED;
}

void JSRT_RuntimeSetupStdMicrotask(JSRT_Runtime* rt) {
  JSRT_Debug("JSRT_RuntimeSetupStdMicrotask: initializing queueMicrotask");

  // Add queueMicrotask as global function
  JS_SetPropertyStr(rt->ctx, rt->global, "queueMicrotask",
                    JS_NewCFunction(rt->ctx, js_queue_microtask, "queueMicrotask", 1));

  JSRT_Debug("queueMicrotask setup completed");
}