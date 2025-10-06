#include "runtime.h"

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>

#include "crypto/crypto.h"
#include "http/fetch.h"
#include "node/process/process.h"
#include "node/process/process_node.h"
#include "std/abort.h"
#include "std/base64.h"
#include "std/blob.h"
#include "std/clone.h"
#include "std/console.h"
#include "std/dom.h"
#include "std/encoding.h"
#include "std/event.h"
#include "std/fetch.h"
#include "std/ffi.h"
#include "std/formdata.h"
#include "std/microtask.h"
#include "std/module.h"
#include "std/navigator.h"
#include "std/performance.h"
#include "std/streams.h"
#include "std/timer.h"
#include "std/webassembly.h"
#include "url/url.h"
#include "util/debug.h"
#include "util/file.h"
#include "util/jsutils.h"
#include "util/path.h"

static void JSRT_RuntimeCloseWalkCallback(uv_handle_t* handle, void* arg) {
  if (!uv_is_closing(handle)) {
    uv_close(handle, NULL);
  }
}

// Type tags from net module (must match net_internal.h)
#define NET_TYPE_SOCKET 0x534F434B  // 'SOCK' in hex
#define NET_TYPE_SERVER 0x53525652  // 'SRVR' in hex

// Cleanup walk callback to free net module memory after handles are closed
static void JSRT_RuntimeCleanupWalkCallback(uv_handle_t* handle, void* arg) {
  if (handle->data && handle->type == UV_TCP) {
    // Check the type tag to determine if this is a socket or server
    uint32_t* type_tag_ptr = (uint32_t*)handle->data;
    uint32_t type_tag = *type_tag_ptr;

    if (type_tag == NET_TYPE_SOCKET) {
      // This is a JSNetConnection
      // Offset calculated with offsetof(): 456 bytes
      size_t host_offset = 456;
      char** host_ptr = (char**)((char*)handle->data + host_offset);
      if (*host_ptr) {
        free(*host_ptr);
      }
    } else if (type_tag == NET_TYPE_SERVER) {
      // This is a JSNetServer
      // Offset calculated with offsetof(): 272 bytes
      size_t host_offset = 272;
      char** host_ptr = (char**)((char*)handle->data + host_offset);
      if (*host_ptr) {
        free(*host_ptr);
      }
    }

    // Free the main struct
    handle->data = NULL;
    free(type_tag_ptr);
  }
}

JSRT_Runtime* JSRT_RuntimeNew() {
  JSRT_Runtime* rt = malloc(sizeof(JSRT_Runtime));
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
  JSRT_RuntimeSetupStdDOM(rt);
  JSRT_RuntimeSetupStdClone(rt);
  JSRT_RuntimeSetupStdMicrotask(rt);  // Add queueMicrotask for WinterCG compliance
  JSRT_RuntimeSetupNavigator(rt);     // Add navigator for WinterTC compliance
  JSRT_RuntimeSetupStdStreams(rt);
  JSRT_RuntimeSetupStdBlob(rt);
  JSRT_RuntimeSetupStdFormData(rt);
  // JSRT_RuntimeSetupStdFetch(rt);  // Replaced with llhttp version
  JSRT_RuntimeSetupHttpFetch(rt);
  JSRT_RuntimeSetupStdCrypto(rt);
  JSRT_RuntimeSetupStdFFI(rt);
  JSRT_RuntimeSetupStdProcess(rt);
  JSRT_RuntimeSetupStdWebAssembly(rt);
  JSRT_StdModuleInit(rt);
  JSRT_StdCommonJSInit(rt);

  return rt;
}

void JSRT_RuntimeFree(JSRT_Runtime* rt) {
  // Close all handles before closing the loop
  uv_walk(rt->uv_loop, JSRT_RuntimeCloseWalkCallback, NULL);

  // Run the loop once more to process the close callbacks
  uv_run(rt->uv_loop, UV_RUN_DEFAULT);

  int result = uv_loop_close(rt->uv_loop);
  if (result != 0) {
    JSRT_Debug("uv_loop_close failed: %s", uv_strerror(result));
  }

  // Clean up net module memory AFTER closing the loop
  // This prevents use-after-free while handles are still in libuv's internal queues
  uv_walk(rt->uv_loop, JSRT_RuntimeCleanupWalkCallback, NULL);

  free(rt->uv_loop);

  JSRT_RuntimeFreeDisposeValues(rt);
  JSRT_RuntimeFreeExceptionValues(rt);

  // Cleanup module system
  JSRT_StdModuleCleanup(rt->ctx);

  // Cleanup FFI module
  JSRT_RuntimeCleanupStdFFI(rt->ctx);

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

void JSRT_EvalResultFree(JSRT_EvalResult* res) {
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

char* JSRT_RuntimeGetExceptionString(JSRT_Runtime* rt, JSValue e) {
  const char* error = JS_ToCString(rt->ctx, e);
  char str[2048];

  JSValue stack_val = JS_GetPropertyStr(rt->ctx, e, "stack");
  if (JS_IsString(stack_val)) {
    const char* stack = JS_ToCString(rt->ctx, stack_val);
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

JSRT_EvalResult JSRT_RuntimeEval(JSRT_Runtime* rt, const char* filename, const char* code, size_t length) {
  JSRT_EvalResult result;
  result.rt = rt;
  result.is_error = false;
  result.error = NULL;
  result.error_length = 0;

  bool is_module = JSRT_PathHasSuffix(filename, ".mjs") || JS_DetectModule((const char*)code, length);
  int eval_flags = is_module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;

  // JSRT_Debug("eval: filename=%s module=%d code=\n%s", filename, is_module, code);

  result.value = JS_Eval(rt->ctx, code, length, filename, eval_flags);

  // For ES modules, we need to ensure the module code executes properly
  if (is_module && !JS_IsException(result.value)) {
    // ES modules are evaluated asynchronously - we need to run pending jobs
    // to ensure the module body executes
    JSRT_Debug("ES module evaluation - running pending jobs to execute module code");

    // Run pending jobs multiple times to ensure module execution
    for (int i = 0; i < 10; i++) {
      if (!JS_IsJobPending(rt->rt))
        break;
      int job_result = JS_ExecutePendingJob(rt->rt, &rt->ctx);
      JSRT_Debug("ES module job execution cycle %d: result=%d", i, job_result);
      if (job_result < 0) {
        JSRT_Debug("ES module job execution failed");
        break;
      }
    }
  }

  if (JS_IsException(result.value)) {
    result.is_error = true;
    JSValue e = JS_GetException(result.rt->ctx);
    result.error = JSRT_RuntimeGetExceptionString(rt, e);
    result.error_length = strlen(result.error);
    JSRT_RuntimeFreeValue(rt, e);
  }

  return result;
}

JSRT_EvalResult JSRT_RuntimeAwaitEvalResult(JSRT_Runtime* rt, JSRT_EvalResult* res) {
  JSRT_EvalResult new_result = JSRT_EvalResultDefault();
  new_result.rt = rt;

  // Skip Promise waiting entirely for now - just run the event loops
  // and return the original result to avoid blocking
  JSRT_Debug("await eval result: skipping Promise state checks to avoid blocking");

  // Run event loops to process any pending operations
  // Give more cycles for server setup and don't break too early
  for (int i = 0; i < 50; i++) {
    int uv_ret = uv_run(rt->uv_loop, UV_RUN_NOWAIT);
    bool has_js_jobs = JS_IsJobPending(rt->rt);
    bool js_ret = JSRT_RuntimeRunTicket(rt);
    JSRT_Debug("await eval result: cycle %d, uv_ret=%d, js_ret=%d", i, uv_ret, js_ret);

    if (!js_ret) {
      JSRT_Debug("JavaScript execution failed");
      break;
    }

    // Only break if we've had several cycles with no work
    // This gives time for async operations like server.listen() to set up
    if (uv_ret == 0 && !JS_IsJobPending(rt->rt)) {
      if (i > 5) {  // Allow at least a few cycles for setup
        JSRT_Debug("No more pending work after %d cycles, breaking", i);
        break;
      }
    }
  }

  // Just return the original value without checking Promise state
  new_result.value = res->value;
  res->value = JS_UNDEFINED;
  return new_result;
}

bool JSRT_RuntimeRun(JSRT_Runtime* rt) {
  uint64_t counter = 0;
  for (;; counter++) {
    // JSRT_Debug("runtime run loop: counter=%d", counter);

    // Check if we have pending JavaScript jobs
    bool has_js_jobs = JS_IsJobPending(rt->rt);

    // If we have JavaScript jobs, use NOWAIT to process them quickly
    // If we only have async I/O, use DEFAULT to block and wait efficiently
    uv_run_mode mode = has_js_jobs ? UV_RUN_NOWAIT : UV_RUN_DEFAULT;

    int ret = uv_run(rt->uv_loop, mode);
    if (ret < 0) {
      JSRT_Debug("uv_run error: ret=%d", ret);
      return false;
    }
    // JSRT_Debug("runtime run loop: uv_run ret=%d, mode=%s", ret, has_js_jobs ? "NOWAIT" : "DEFAULT");

    if (!JSRT_RuntimeRunTicket(rt)) {
      return false;
    }

    if (!JSRT_RuntimeProcessUnhandledExceptionValues(rt)) {
      return false;
    }

    // Check if we still have work to do
    if (JS_IsJobPending(rt->rt)) {
      // JSRT_Debug("runtime run loop: js has pending job, counter=%d", counter);
      continue;
    }

    if (uv_loop_alive(rt->uv_loop)) {
      // JSRT_Debug("runtime run loop: async tasks are not completed, counter=%d", counter);
      // If we used DEFAULT mode and there are still active handles,
      // it means we processed some events, continue the loop
      continue;
    }

    // No more work to do
    break;
  }
  return true;
}

bool JSRT_RuntimeRunTicket(JSRT_Runtime* rt) {
  JSContext* ctx1;
  int status = JS_ExecutePendingJob(JS_GetRuntime(rt->ctx), &ctx1);
  // JSRT_Debug("runtime execute pending job: status=%d", status);
  if (status < 0) {
    JSValue e = JS_GetException(rt->ctx);
    char* s = JSRT_RuntimeGetExceptionString(rt, e);
    fprintf(stderr, "%s\n", s);
    free(s);
    JSRT_RuntimeFreeValue(rt, e);
    return false;
  }

  // Execute nextTick callbacks after processing pending jobs
  jsrt_process_execute_next_tick(rt->ctx);

  return true;
}

void JSRT_RuntimeAddDisposeValue(JSRT_Runtime* rt, JSValue value) {
  if (rt->dispose_values_length == rt->dispose_values_capacity) {
    rt->dispose_values_capacity *= 2;
    rt->dispose_values = realloc(rt->dispose_values, rt->dispose_values_capacity * sizeof(JSValue));
  }
  rt->dispose_values[rt->dispose_values_length] = value;
  rt->dispose_values_length++;
  JSRT_Debug("add dispose value: value=%p", &value);
}

void JSRT_RuntimeFreeDisposeValues(JSRT_Runtime* rt) {
  for (size_t i = 0; i < rt->dispose_values_length; i++) {
    JSRT_Debug("free dispose value: value=%p", &rt->dispose_values[i]);
    JSRT_RuntimeFreeValue(rt, rt->dispose_values[i]);
  }
  free(rt->dispose_values);
  rt->dispose_values = NULL;
  rt->dispose_values_capacity = 0;
  rt->dispose_values_length = 0;
}

void JSRT_RuntimeAddExceptionValue(JSRT_Runtime* rt, JSValue e) {
  if (rt->exception_values_length == rt->exception_values_capacity) {
    rt->exception_values_capacity *= 2;
    rt->exception_values = realloc(rt->exception_values, rt->exception_values_capacity * sizeof(JSValue));
  }
  rt->exception_values[rt->exception_values_length] = e;
  rt->exception_values_length++;
}

void JSRT_RuntimeFreeExceptionValues(JSRT_Runtime* rt) {
  for (size_t i = 0; i < rt->exception_values_length; i++) {
    char* s = JSRT_RuntimeGetExceptionString(rt, rt->exception_values[i]);
    JSRT_Debug("free unhandled exception value: value=%p %s", &rt->exception_values[i], s);
    free(s);
    JSRT_RuntimeFreeValue(rt, rt->exception_values[i]);
  }
  free(rt->exception_values);
  rt->exception_values = NULL;
  rt->exception_values_capacity = 0;
  rt->exception_values_length = 0;
}

bool JSRT_RuntimeProcessUnhandledExceptionValues(JSRT_Runtime* rt) {
  for (size_t i = 0; i < rt->exception_values_length; i++) {
    char* s = JSRT_RuntimeGetExceptionString(rt, rt->exception_values[i]);
    // TODO: emit "error" event
    fprintf(stderr, "%s\n", s);
    free(s);
    JSRT_RuntimeFreeValue(rt, rt->exception_values[i]);
  }
  rt->exception_values_length = 0;
  return true;
}

JSRT_CompileResult JSRT_RuntimeCompileToBytecode(JSRT_Runtime* rt, const char* filename, const char* code,
                                                 size_t length) {
  JSRT_CompileResult result = {0};

  // Detect if this is a module or script
  bool is_module = JSRT_PathHasSuffix(filename, ".mjs") || JS_DetectModule((const char*)code, length);

  // Compile the JavaScript code with appropriate type
  int eval_flags = JS_EVAL_FLAG_COMPILE_ONLY | (is_module ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);
  JSValue val = JS_Eval(rt->ctx, code, length, filename, eval_flags);

  if (JS_IsException(val)) {
    // Get exception string
    JSValue exception = JS_GetException(rt->ctx);
    result.error = JSRT_RuntimeGetExceptionString(rt, exception);
    JS_FreeValue(rt->ctx, exception);
    return result;
  }

  // Write the compiled function to bytecode
  size_t out_buf_len;
  uint8_t* out_buf = JS_WriteObject(rt->ctx, &out_buf_len, val, JS_WRITE_OBJ_BYTECODE);
  JS_FreeValue(rt->ctx, val);

  if (!out_buf) {
    result.error = strdup("Failed to write bytecode");
    return result;
  }

  result.data = out_buf;
  result.size = out_buf_len;
  return result;
}

void JSRT_CompileResultFree(JSRT_CompileResult* result) {
  if (result->data) {
    // QuickJS allocated memory needs to be freed with js_free_rt or regular free
    // Since JS_WriteObject allocates with js_malloc which uses malloc internally
    free(result->data);
    result->data = NULL;
  }
  if (result->error) {
    free(result->error);
    result->error = NULL;
  }
  result->size = 0;
}