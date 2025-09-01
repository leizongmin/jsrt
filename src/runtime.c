#include "runtime.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "std/abort.h"
#include "std/base64.h"
#include "std/blob.h"
#include "std/clone.h"
#include "std/console.h"
#include "std/encoding.h"
#include "std/event.h"
#include "std/fetch.h"
#include "std/formdata.h"
#include "std/module.h"
#include "std/performance.h"
#include "std/process.h"
#include "std/streams.h"
#include "std/timer.h"
#include "std/url.h"
#include "util/debug.h"
#include "util/file.h"
#include "util/jsutils.h"
#include "util/path.h"

static void JSRT_RuntimeCloseWalkCallback(uv_handle_t *handle, void *arg) {
  if (!uv_is_closing(handle)) {
    uv_close(handle, NULL);
  }
}

JSRT_Runtime *JSRT_RuntimeNew() {
  JSRT_Runtime *rt = malloc(sizeof(JSRT_Runtime));
  rt->rt = JS_NewRuntime();
  JS_SetRuntimeOpaque(rt->rt, rt);
  rt->ctx = JS_NewContext(rt->rt);
  JS_SetContextOpaque(rt->ctx, rt);

  rt->global = JS_GetGlobalObject(rt->ctx);
  rt->dispose_values_capacity = 16;
  rt->dispose_values_length = 0;
  rt->dispose_values = malloc(rt->dispose_values_capacity * sizeof(JSValue));

  rt->exception_values_capacity = 16;
  rt->exception_values_length = 0;
  rt->exception_values = malloc(rt->exception_values_capacity * sizeof(JSValue));

  rt->uv_loop = malloc(sizeof(uv_loop_t));
  uv_loop_init(rt->uv_loop);
  rt->uv_loop->data = rt;

  JSRT_RuntimeSetupStdConsole(rt);
  JSRT_RuntimeSetupStdTimer(rt);
  JSRT_RuntimeSetupStdEncoding(rt);
  JSRT_RuntimeSetupStdBase64(rt);
  JSRT_RuntimeSetupStdPerformance(rt);
  JSRT_RuntimeSetupStdEvent(rt);
  JSRT_RuntimeSetupStdAbort(rt);
  JSRT_RuntimeSetupStdURL(rt);
  JSRT_RuntimeSetupStdClone(rt);
  JSRT_RuntimeSetupStdStreams(rt);
  JSRT_RuntimeSetupStdBlob(rt);
  JSRT_RuntimeSetupStdFormData(rt);
  JSRT_RuntimeSetupStdFetch(rt);
  JSRT_RuntimeSetupStdProcess(rt);
  JSRT_StdModuleInit(rt);
  JSRT_StdCommonJSInit(rt);

  return rt;
}

void JSRT_RuntimeFree(JSRT_Runtime *rt) {
  // Close all handles before closing the loop
  uv_walk(rt->uv_loop, JSRT_RuntimeCloseWalkCallback, NULL);

  // Run the loop once more to process the close callbacks
  uv_run(rt->uv_loop, UV_RUN_DEFAULT);

  int result = uv_loop_close(rt->uv_loop);
  if (result != 0) {
    JSRT_Debug("uv_loop_close failed: %s", uv_strerror(result));
  }
  free(rt->uv_loop);

  JSRT_RuntimeFreeDisposeValues(rt);
  JSRT_RuntimeFreeExceptionValues(rt);

  // Cleanup module system
  JSRT_StdModuleCleanup(rt->ctx);

  JSRT_RuntimeFreeValue(rt, rt->global);
  rt->global = JS_UNDEFINED;

  JS_RunGC(rt->rt);

  JS_FreeContext(rt->ctx);
  rt->ctx = NULL;
  JS_FreeRuntime(rt->rt);
  rt->rt = NULL;
  free(rt);
}

JSRT_EvalResult JSRT_EvalResultDefault() {
  JSRT_EvalResult res;
  res.rt = NULL;
  res.is_error = false;
  res.error = NULL;
  res.error_length = 0;
  res.value = JS_UNDEFINED;
  return res;
}

void JSRT_EvalResultFree(JSRT_EvalResult *res) {
  if (res->error != NULL) {
    free(res->error);
    res->error = NULL;
    res->error_length = 0;
    res->is_error = false;
  }
  if (res->rt != NULL) {
    JSRT_RuntimeFreeValue(res->rt, res->value);
    res->value = JS_UNDEFINED;
    res->rt = NULL;
  }
}

char *JSRT_RuntimeGetExceptionString(JSRT_Runtime *rt, JSValue e) {
  const char *error = JS_ToCString(rt->ctx, e);
  char str[2048];

  JSValue stack_val = JS_GetPropertyStr(rt->ctx, e, "stack");
  if (JS_IsString(stack_val)) {
    const char *stack = JS_ToCString(rt->ctx, stack_val);
    snprintf(str, sizeof(str), "%s\n%s", error, stack);
    JS_FreeCString(rt->ctx, stack);
  } else {
    snprintf(str, sizeof(str), "%s", error);
  }

  JS_FreeCString(rt->ctx, error);
  JSRT_RuntimeFreeValue(rt, stack_val);

  JSRT_Debug("get exception string: str=%s", str);
  return strdup(str);
}

JSRT_EvalResult JSRT_RuntimeEval(JSRT_Runtime *rt, const char *filename, const char *code, size_t length) {
  JSRT_EvalResult result;
  result.rt = rt;
  result.is_error = false;
  result.error = NULL;
  result.error_length = 0;

  bool is_module = JSRT_PathHasSuffix(filename, ".mjs") || JS_DetectModule((const char *)code, length);
  int eval_flags = is_module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;

  // JSRT_Debug("eval: filename=%s module=%d code=\n%s", filename, is_module, code);

  result.value = JS_Eval(rt->ctx, code, length, filename, eval_flags);

  if (JS_IsException(result.value)) {
    result.is_error = true;
    JSValue e = JS_GetException(result.rt->ctx);
    result.error = JSRT_RuntimeGetExceptionString(rt, e);
    result.error_length = strlen(result.error);
    JSRT_RuntimeFreeValue(rt, e);
  }

  return result;
}

JSRT_EvalResult JSRT_RuntimeAwaitEvalResult(JSRT_Runtime *rt, JSRT_EvalResult *res) {
  JSRT_EvalResult new_result = JSRT_EvalResultDefault();
  new_result.rt = rt;

  int state;
  bool loop = true;

  while (loop) {
    state = JS_PromiseState(rt->ctx, res->value);
    JSRT_Debug("await eval result: state=%d", state);
    if (state == JS_PROMISE_FULFILLED) {
      new_result.value = JS_PromiseResult(rt->ctx, res->value);
      JSRT_RuntimeFreeValue(rt, res->value);
      res->value = JS_UNDEFINED;
      break;
    } else if (state == JS_PROMISE_REJECTED) {
      new_result.value = JS_Throw(rt->ctx, JS_PromiseResult(rt->ctx, res->value));
      new_result.is_error = true;
      JSValue e = JS_GetException(rt->ctx);
      new_result.error = JSRT_RuntimeGetExceptionString(rt, e);
      new_result.error_length = strlen(new_result.error);
      JSRT_RuntimeFreeValue(rt, e);
      JSRT_RuntimeFreeValue(rt, res->value);
      res->value = JS_UNDEFINED;
      break;
    } else if (state == JS_PROMISE_PENDING) {
      loop = JSRT_RuntimeRunTicket(rt);
    } else {
      new_result.value = res->value;
      res->value = JS_UNDEFINED;
      break;
    }
  }

  return new_result;
}

bool JSRT_RuntimeRun(JSRT_Runtime *rt) {
  uint64_t counter = 0;
  for (;; counter++) {
    // JSRT_Debug("runtime run loop: counter=%d", counter);
    int ret = uv_run(rt->uv_loop, UV_RUN_NOWAIT);
    if (ret < 0) {
      JSRT_Debug("uv_run error: ret=%d", ret);
      return false;
    }
    // JSRT_Debug("runtime run loop: uv_run ret=%d", ret);

    if (!JSRT_RuntimeRunTicket(rt)) {
      return false;
    }

    if (!JSRT_RuntimeProcessUnhandledExceptionValues(rt)) {
      return false;
    }

    if (JS_IsJobPending(rt->rt)) {
      // JSRT_Debug("runtime run loop: js has pending job, counter=%d", counter);
      continue;
    }
    if (uv_loop_alive(rt->uv_loop)) {
      // JSRT_Debug("runtime run loop: async tasks are not completed, counter=%d", counter);
      continue;
    }
    break;
  }
  return true;
}

bool JSRT_RuntimeRunTicket(JSRT_Runtime *rt) {
  JSContext *ctx1;
  int status = JS_ExecutePendingJob(JS_GetRuntime(rt->ctx), &ctx1);
  // JSRT_Debug("runtime execute pending job: status=%d", status);
  if (status < 0) {
    JSValue e = JS_GetException(rt->ctx);
    char *s = JSRT_RuntimeGetExceptionString(rt, e);
    fprintf(stderr, "%s\n", s);
    free(s);
    JSRT_RuntimeFreeValue(rt, e);
    return false;
  }

  return true;
}

void JSRT_RuntimeAddDisposeValue(JSRT_Runtime *rt, JSValue value) {
  if (rt->dispose_values_length == rt->dispose_values_capacity) {
    rt->dispose_values_capacity *= 2;
    rt->dispose_values = realloc(rt->dispose_values, rt->dispose_values_capacity * sizeof(JSValue));
  }
  rt->dispose_values[rt->dispose_values_length] = value;
  rt->dispose_values_length++;
  JSRT_Debug("add dispose value: value=%p", &value);
}

void JSRT_RuntimeFreeDisposeValues(JSRT_Runtime *rt) {
  for (size_t i = 0; i < rt->dispose_values_length; i++) {
    JSRT_Debug("free dispose value: value=%p", &rt->dispose_values[i]);
    JSRT_RuntimeFreeValue(rt, rt->dispose_values[i]);
  }
  free(rt->dispose_values);
  rt->dispose_values = NULL;
  rt->dispose_values_capacity = 0;
  rt->dispose_values_length = 0;
}

void JSRT_RuntimeAddExceptionValue(JSRT_Runtime *rt, JSValue e) {
  if (rt->exception_values_length == rt->exception_values_capacity) {
    rt->exception_values_capacity *= 2;
    rt->exception_values = realloc(rt->exception_values, rt->exception_values_capacity * sizeof(JSValue));
  }
  rt->exception_values[rt->exception_values_length] = e;
  rt->exception_values_length++;
}

void JSRT_RuntimeFreeExceptionValues(JSRT_Runtime *rt) {
  for (size_t i = 0; i < rt->exception_values_length; i++) {
    char *s = JSRT_RuntimeGetExceptionString(rt, rt->exception_values[i]);
    JSRT_Debug("free unhandled exception value: value=%p %s", &rt->exception_values[i], s);
    free(s);
    JSRT_RuntimeFreeValue(rt, rt->exception_values[i]);
  }
  free(rt->exception_values);
  rt->exception_values = NULL;
  rt->exception_values_capacity = 0;
  rt->exception_values_length = 0;
}

bool JSRT_RuntimeProcessUnhandledExceptionValues(JSRT_Runtime *rt) {
  for (size_t i = 0; i < rt->exception_values_length; i++) {
    char *s = JSRT_RuntimeGetExceptionString(rt, rt->exception_values[i]);
    // TODO: emit "error" event
    fprintf(stderr, "%s\n", s);
    free(s);
    JSRT_RuntimeFreeValue(rt, rt->exception_values[i]);
  }
  rt->exception_values_length = 0;
  return true;
}
