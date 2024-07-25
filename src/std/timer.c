#include "timer.h"

#include <quickjs.h>
#include <stdlib.h>

#include "util/jsutils.h"
#include "util/macro.h"

typedef struct {
  JSRT_Runtime *rt;
  uv_timer_t uv_timer;
  uint64_t timeout;
  bool is_interval;
  JSValueConst this_val;
  int argc;
  JSValueConst *argv;
  JSValue callback;
} JSRT_Timer;

static void jsrt_timer_free(JSRT_Timer *timer);

static JSValue jsrt_set_timeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_set_interval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_start_timer(bool is_interval, JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue jsrt_stop_timer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static void jsrt_on_timer_callback(uv_timer_t *timer);

static JSClassID timer_class_id;
static void jsrt_timer_finalizer(JSRuntime *rt, JSValue val) {
  JSRT_Timer *timer = JS_GetOpaque(val, timer_class_id);
  if (timer) {
    // JSRT_Debug("TimerFinalizer: timer=%p id=%d", timer, timer->uv_timer.start_id);
  }
}
static JSClassDef timer_class = {
    "Timer",
    .finalizer = jsrt_timer_finalizer,
};
static const JSCFunctionListEntry timer_proto_funcs[] = {};

void JSRT_RuntimeSetupStdTimer(JSRT_Runtime *rt) {
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

static JSValue jsrt_set_timeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  return jsrt_start_timer(false, ctx, this_val, argc, argv);
}

static JSValue jsrt_set_interval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  return jsrt_start_timer(true, ctx, this_val, argc, argv);
}

static JSValue jsrt_start_timer(bool is_interval, JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Runtime *rt = JS_GetContextOpaque(ctx);

  int64_t timeout = 0;
  JSValue callback;
  if (argc < 1) {
    return JS_ThrowTypeError(rt->ctx, "The \"callback\" argument must be of type function. Received undefined");
  } else if (argc == 1) {
    callback = argv[0];
  } else {
    callback = argv[0];
    int status = JS_ToInt64(rt->ctx, &timeout, argv[1]);
    if (status != 0) {
      JSRT_Debug("failed to convert timeout to int64_t: status=%d, value=%p", status, &argv[1]);
    }
  }
  if (!JS_IsFunction(ctx, callback)) {
    return JS_ThrowTypeError(rt->ctx, "The \"callback\" argument must be of type function. Received type %s",
                             JSRT_GetTypeofJSValue(ctx, callback));
  }

  JSRT_Timer *timer = malloc(sizeof(JSRT_Timer));
  timer->rt = rt;
  timer->uv_timer.data = timer;
  timer->timeout = (uint64_t)timeout;
  timer->is_interval = is_interval;
  timer->callback = JS_DupValue(rt->ctx, callback);
  timer->this_val = this_val;
  timer->argc = argc - 2;
  timer->argv = timer->argc > 0 ? argv + 2 : NULL;

  uv_timer_init(rt->uv_loop, &timer->uv_timer);

  int status =
      uv_timer_start(&timer->uv_timer, jsrt_on_timer_callback, timer->timeout, is_interval ? timer->timeout : 0);
  if (status != 0) {
    return JS_ThrowInternalError(rt->ctx, "uv_timer_start error: status=%d", status);
  }

  JSValueConst result = JS_NewObjectClass(rt->ctx, timer_class_id);
  if (!JS_IsException(result)) {
    JS_SetOpaque(result, timer);
    JS_SetPropertyStr(rt->ctx, result, "id", JS_NewInt64(rt->ctx, (int64_t)timer->uv_timer.start_id));
  }

  return result;
}

static JSValue jsrt_stop_timer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Runtime *rt = JS_GetContextOpaque(ctx);

  if (argc > 0) {
    JSRT_Timer *timer = JS_GetOpaque(argv[0], timer_class_id);
    if (timer != NULL) {
      jsrt_timer_free(timer);
    }
  }

  return JS_UNDEFINED;
}

void jsrt_on_timer_callback(uv_timer_t *uv_timer) {
  JSRT_Timer *timer = uv_timer->data;
  JSValueConst this_val = timer->this_val;
  int argc = timer->argc;
  JSValue *argv = timer->argv;
  JSValue callback = timer->callback;

  JSValue ret = JS_Call(timer->rt->ctx, callback, this_val, argc, argv);
  if (JS_IsException(ret)) {
    JSValue e = JS_GetException(timer->rt->ctx);
    JSRT_RuntimeAddExceptionValue(timer->rt, e);
  }
  JSRT_RuntimeFreeValue(timer->rt, ret);

  if (!timer->is_interval) {
    jsrt_timer_free(timer);
  }
}

static void jsrt_timer_free(JSRT_Timer *timer) {
  JSRT_Debug("TimerFree: timer=%p id=%llu", timer, timer->uv_timer.start_id);
  int status = uv_timer_stop(&timer->uv_timer);
  JSRT_Debug("uv_timer_stop: id=%llu status=%d", timer->uv_timer.start_id, status);
  JSRT_RuntimeFreeValue(timer->rt, timer->callback);
  timer->callback = JS_UNDEFINED;
  JSRT_RuntimeFreeValue(timer->rt, timer->this_val);
  timer->this_val = JS_UNDEFINED;
  free(timer);
}
