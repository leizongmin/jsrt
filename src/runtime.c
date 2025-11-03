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
#include "module/core/module_context.h"
#include "module/core/module_loader.h"
#include "module/module.h"
#include "module/protocols/file_handler.h"
#include "module/protocols/protocol_registry.h"
#include "node/module/sourcemap.h"
#include "node/net/net_internal.h"
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

static void jsrt_debug_dump_handles(uv_loop_t* loop);

static void JSRT_RuntimeCloseWalkCallback(uv_handle_t* handle, void* arg) {
  if (!uv_is_closing(handle)) {
    // Reference the handle before closing to ensure the event loop processes it
    if (!uv_has_ref(handle)) {
      uv_ref(handle);
    }
    uv_close(handle, NULL);
  }
}

static void jsrt_set_constructor_prototype(JSRT_Runtime* rt, const char* ctor_name, JSValue* args, int argc) {
  JSContext* ctx = rt->ctx;
  JSValue ctor = JS_GetPropertyStr(ctx, rt->global, ctor_name);
  if (JS_IsException(ctor)) {
    JSRT_Debug("Constructor '%s' unavailable", ctor_name);
    JS_FreeValue(ctx, ctor);
    return;
  }

  JSValue existing_proto = JS_GetPropertyStr(ctx, ctor, "prototype");
  bool need_define = !JS_IsObject(existing_proto);
  JS_FreeValue(ctx, existing_proto);

  if (need_define) {
    JSValue instance = JS_CallConstructor(ctx, ctor, argc, args);
    if (!JS_IsException(instance)) {
      JSValue proto = JS_GetPrototype(ctx, instance);
      if (!JS_IsException(proto) && JS_IsObject(proto)) {
        JS_SetPropertyStr(ctx, ctor, "prototype", JS_DupValue(ctx, proto));
        JSRT_Debug("Set %s.prototype via helper", ctor_name);
      } else {
        JSRT_Debug("Failed to derive prototype for %s", ctor_name);
      }
      JS_FreeValue(ctx, proto);
      JS_FreeValue(ctx, instance);
    } else {
      JSRT_Debug("Failed to instantiate %s for prototype setup", ctor_name);
      JS_FreeValue(ctx, instance);
    }
  }

  JS_FreeValue(ctx, ctor);
}

static void jsrt_ensure_fetch_prototypes(JSRT_Runtime* rt) {
  JSContext* ctx = rt->ctx;

  JSValue request_args[1];
  request_args[0] = JS_NewString(ctx, "https://jsrt.local/");
  jsrt_set_constructor_prototype(rt, "Request", request_args, 1);
  JS_FreeValue(ctx, request_args[0]);

  JSValue response_args[1];
  response_args[0] = JS_NULL;
  jsrt_set_constructor_prototype(rt, "Response", response_args, 1);

  jsrt_set_constructor_prototype(rt, "Headers", NULL, 0);
}

/**
 * Setup Error.stack line number fix for CommonJS modules
 * QuickJS reports line numbers relative to wrapper code which has +1 offset
 */
static void jsrt_setup_error_stack_fix(JSRT_Runtime* rt) {
  JSContext* ctx = rt->ctx;

  // Inline Error.stack fix script
  // QuickJS doesn't use a getter for Error.prototype.stack, it's a regular property
  // We need to override it when errors are created, not at the prototype level
  // CommonJS wrapper adds 2 lines (function header + registration), so subtract 2 from line numbers
  const char* error_stack_fix =
      "globalThis.__jsrt_cjs_modules=globalThis.__jsrt_cjs_modules||new Set();"
      // Store cwd for path normalization
      "const __jsrt_cwd=process.cwd()+'/';"
      // Wrap Error constructor to fix stack traces
      "const OrigError=Error;"
      "globalThis.Error=function Error(...args){"
      "const err=new OrigError(...args);"
      "if(err.stack){"
      "let s=err.stack;"
      // Always remove wrapper frame from stack trace
      "s=s.split('\\n').filter(l=>!l.includes('<error_stack_fix_cjs>')).join('\\n');"
      // Adjust line numbers for CommonJS modules (before path conversion)
      "if(globalThis.__jsrt_cjs_modules&&globalThis.__jsrt_cjs_modules.size>0){"
      "for(const f of globalThis.__jsrt_cjs_modules){"
      "if(s.includes(f)){"
      "const e=f.replace(/[.*+?^${}()|[\\]\\\\]/g,'\\\\$&');"
      "const r=new RegExp('('+e+'):(\\\\d+):','g');"
      "s=s.replace(r,(m,f,l)=>{"
      "const adjusted=parseInt(l)-2;"
      "return f+':'+(adjusted>0?adjusted:1)+':';"
      "});"
      "}"
      "}"
      "}"
      // Convert absolute paths to relative paths for consistency with ESM
      "s=s.replace(new RegExp(__jsrt_cwd.replace(/[.*+?^${}()|[\\]\\\\]/g,'\\\\$&'),'g'),'');"
      "err.stack=s;"
      "}"
      "return err;"
      "};"
      "Error.prototype=OrigError.prototype;"
      "Error.prototype.constructor=Error;"
      // Copy static properties
      "Object.setPrototypeOf(Error,OrigError);";

  JSValue result = JS_Eval(ctx, error_stack_fix, strlen(error_stack_fix), "<error_stack_fix_cjs>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result)) {
    JSRT_Debug("Failed to setup Error.stack fix for CommonJS");
    js_std_dump_error(ctx);
  }
  JS_FreeValue(ctx, result);
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

  // Align Node.js global alias
  JSValue global_alias = JS_DupValue(rt->ctx, rt->global);
  if (JS_DefinePropertyValueStr(rt->ctx, rt->global, "global", global_alias, JS_PROP_C_W_E) < 0) {
    JSRT_Debug("Failed to define global alias");
  } else {
    JSRT_Debug("Defined global alias successfully");
  }

  rt->exception_values_capacity = 16;
  rt->exception_values_length = 0;
  rt->exception_values = malloc(rt->exception_values_capacity * sizeof(JSValue));

  rt->uv_loop = malloc(sizeof(uv_loop_t));
  uv_loop_init(rt->uv_loop);
  rt->uv_loop->data = rt;

  rt->compact_node_mode = false;

  // Initialize protocol registry for new module system
  jsrt_init_protocol_handlers();

  // Register default protocol handlers (file://, http://, https://)
  jsrt_file_handler_init();

  // Create and initialize new module loader
  rt->module_loader = jsrt_module_loader_create(rt->ctx);
  if (!rt->module_loader) {
    JSRT_Debug("Failed to create module loader");
  }

  // Initialize source map cache (16 buckets default)
  rt->source_map_cache = jsrt_source_map_cache_init(rt->ctx, 16);
  if (!rt->source_map_cache) {
    JSRT_Debug("Failed to create source map cache");
  }

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
  jsrt_ensure_fetch_prototypes(rt);
  JSRT_RuntimeSetupStdCrypto(rt);
  JSRT_RuntimeSetupStdFFI(rt);
  JSRT_RuntimeSetupStdProcess(rt);
  JSRT_RuntimeSetupStdWebAssembly(rt);
  JSRT_StdModuleInit(rt);
  JSRT_StdCommonJSInit(rt);

  // Setup Error.stack line number fix for CommonJS modules
  jsrt_setup_error_stack_fix(rt);

  return rt;
}

void JSRT_RuntimeFree(JSRT_Runtime* rt) {
  // Free JavaScript objects first to trigger finalizers while loop is still alive
  JSRT_RuntimeFreeDisposeValues(rt);
  JSRT_RuntimeFreeExceptionValues(rt);

  // Cleanup old module system (still needed for compatibility)
  JSRT_StdModuleCleanup(rt->ctx);

  // Cleanup new module loader
  if (rt->module_loader) {
    jsrt_module_loader_free(rt->module_loader);
    rt->module_loader = NULL;
  }

  // Cleanup source map cache
  if (rt->source_map_cache) {
    jsrt_source_map_cache_free(rt->rt, rt->source_map_cache);
    rt->source_map_cache = NULL;
  }

  // Cleanup protocol registry
  jsrt_cleanup_protocol_handlers();

  // Cleanup FFI module
  JSRT_RuntimeCleanupStdFFI(rt->ctx);

  JSRT_RuntimeFreeValue(rt, rt->global);
  rt->global = JS_UNDEFINED;

  // Run GC to trigger finalizers while loop is still active
  // Finalizers will close their handles via uv_close()
  JS_RunGC(rt->rt);

  // Run the loop to process all close callbacks from finalizers
  // This must happen before uv_walk to avoid accessing freed memory
  uv_run(rt->uv_loop, UV_RUN_DEFAULT);

  // Now close any remaining handles that weren't managed by finalizers
  // (timers, async handles, etc.)
  JSRT_Debug("Active handles before uv_walk:");
  jsrt_debug_dump_handles(rt->uv_loop);

  uv_walk(rt->uv_loop, JSRT_RuntimeCloseWalkCallback, NULL);

  // Run the loop until it's completely idle
  // UV_RUN_DEFAULT will process all pending events and return when there's nothing left
  uv_run(rt->uv_loop, UV_RUN_DEFAULT);

  JSRT_Debug("Active handles after final uv_run:");
  jsrt_debug_dump_handles(rt->uv_loop);

  int result = uv_loop_close(rt->uv_loop);
  if (result != 0) {
    // Loop couldn't close cleanly - there are handles still closing
    // This can happen with unreferenced handles that have pending close callbacks
    // It's safe to ignore this and free the loop anyway
    JSRT_Debug("uv_loop_close failed (handles still closing): %s", uv_strerror(result));
  }

  free(rt->uv_loop);

  // Clean up deferred net module structs after loop is closed
  jsrt_net_cleanup_deferred();

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

static bool jsrt_append_str(char** buf, size_t* capacity, size_t* length, const char* text) {
  if (!text)
    text = "";
  size_t add = strlen(text);
  if (*length + add + 1 > *capacity) {
    size_t new_capacity = *capacity ? *capacity : 128;
    while (new_capacity < *length + add + 1) {
      new_capacity *= 2;
    }
    char* new_buf = realloc(*buf, new_capacity);
    if (!new_buf)
      return false;
    *buf = new_buf;
    *capacity = new_capacity;
  }
  memcpy(*buf + *length, text, add);
  *length += add;
  (*buf)[*length] = '\0';
  return true;
}

static bool jsrt_append_char(char** buf, size_t* capacity, size_t* length, char c) {
  char tmp[2] = {c, '\0'};
  return jsrt_append_str(buf, capacity, length, tmp);
}

char* JSRT_RuntimeGetExceptionString(JSRT_Runtime* rt, JSValue e) {
  const char* error = NULL;
  bool error_needs_free_cstring = true;  // Track if error came from JS_ToCString

  // Handle special case where exception is an uninitialized value
  // This can happen with ES module loading errors
  if (JS_IsUninitialized(e)) {
    char str[2048];
    snprintf(str, sizeof(str), "Uninitialized exception");
    return strdup(str);
  }
  // First try to get the 'message' property if it's an Error object
  else if (JS_IsObject(e)) {
    JSValue message_val = JS_GetPropertyStr(rt->ctx, e, "message");
    if (JS_IsString(message_val)) {
      error = JS_ToCString(rt->ctx, message_val);
    }
    JS_FreeValue(rt->ctx, message_val);
  }

  // Fallback to JS_ToCString
  if (!error) {
    error = JS_ToCString(rt->ctx, e);
    // If JS_ToCString returns "[unsupported type]", replace with better message
    if (error && strcmp(error, "[unsupported type]") == 0) {
      char str[2048];
      snprintf(str, sizeof(str), "Error: Exception object has unsupported type");
      JS_FreeCString(rt->ctx, error);
      return strdup(str);
    }
  }

  char str[2048];

  bool handled_module_not_found = false;
  char* module_str = NULL;
  size_t module_cap = 0;
  size_t module_len = 0;

  JSValue code_val = JS_GetPropertyStr(rt->ctx, e, "code");
  if (JS_IsString(code_val)) {
    const char* code = JS_ToCString(rt->ctx, code_val);
    if (code && strcmp(code, "MODULE_NOT_FOUND") == 0) {
      JSValue require_stack_val = JS_GetPropertyStr(rt->ctx, e, "requireStack");
      uint32_t require_len = 0;
      bool has_require_stack = false;
      if (!JS_IsUndefined(require_stack_val) && !JS_IsNull(require_stack_val)) {
        JSValue len_val = JS_GetPropertyStr(rt->ctx, require_stack_val, "length");
        if (!JS_IsException(len_val)) {
          if (JS_ToUint32(rt->ctx, &require_len, len_val) >= 0) {
            has_require_stack = require_len > 0;
          }
        }
        JS_FreeValue(rt->ctx, len_val);
      }

      char** entries = NULL;
      if (has_require_stack) {
        entries = calloc(require_len, sizeof(char*));
        if (!entries) {
          require_len = 0;
          has_require_stack = false;
        }
      }

      if (has_require_stack) {
        for (uint32_t i = 0; i < require_len; i++) {
          JSValue item = JS_GetPropertyUint32(rt->ctx, require_stack_val, i);
          if (JS_IsString(item)) {
            const char* entry = JS_ToCString(rt->ctx, item);
            entries[i] = entry ? strdup(entry) : NULL;
            JS_FreeCString(rt->ctx, entry);
          }
          JS_FreeValue(rt->ctx, item);
          if (!entries[i]) {
            entries[i] = strdup("<unknown>");
          }
        }
      }

      const char* error_line = error ? error : "Error";
      handled_module_not_found = jsrt_append_str(&module_str, &module_cap, &module_len, error_line);

      if (handled_module_not_found && has_require_stack) {
        handled_module_not_found =
            handled_module_not_found && jsrt_append_str(&module_str, &module_cap, &module_len, "\nRequire stack:\n");
        for (uint32_t i = 0; handled_module_not_found && i < require_len; i++) {
          handled_module_not_found =
              jsrt_append_str(&module_str, &module_cap, &module_len, "- ") &&
              jsrt_append_str(&module_str, &module_cap, &module_len, entries[i] ? entries[i] : "<unknown>") &&
              jsrt_append_char(&module_str, &module_cap, &module_len, '\n');
        }
      }

      if (handled_module_not_found) {
        if (has_require_stack) {
          handled_module_not_found = jsrt_append_str(&module_str, &module_cap, &module_len,
                                                     "\n{\n  code: 'MODULE_NOT_FOUND',\n  requireStack: [ ");
          for (uint32_t i = 0; handled_module_not_found && i < require_len; i++) {
            if (i > 0)
              handled_module_not_found =
                  handled_module_not_found && jsrt_append_str(&module_str, &module_cap, &module_len, ", ");
            handled_module_not_found =
                handled_module_not_found && jsrt_append_char(&module_str, &module_cap, &module_len, '\'') &&
                jsrt_append_str(&module_str, &module_cap, &module_len, entries[i] ? entries[i] : "<unknown>") &&
                jsrt_append_char(&module_str, &module_cap, &module_len, '\'');
          }
          if (handled_module_not_found) {
            handled_module_not_found = jsrt_append_str(&module_str, &module_cap, &module_len, " ]\n}\n");
          }
        } else {
          handled_module_not_found = jsrt_append_str(&module_str, &module_cap, &module_len,
                                                     "\n{\n  code: 'MODULE_NOT_FOUND',\n  requireStack: []\n}\n");
        }
      }

      if (!handled_module_not_found && module_str) {
        free(module_str);
        module_str = NULL;
      }

      if (entries) {
        for (uint32_t i = 0; i < require_len; i++) {
          free(entries[i]);
        }
        free(entries);
      }

      JS_FreeValue(rt->ctx, require_stack_val);
    }
    JS_FreeCString(rt->ctx, code);
  }
  JS_FreeValue(rt->ctx, code_val);

  if (handled_module_not_found && module_str) {
    JS_FreeCString(rt->ctx, error);
    JSRT_Debug("get exception string: str=%s", module_str);
    return module_str;
  }

  JSValue stack_val = JS_GetPropertyStr(rt->ctx, e, "stack");
  if (JS_IsString(stack_val)) {
    const char* stack = JS_ToCString(rt->ctx, stack_val);
    if (stack) {
      if (error) {
        size_t error_len = strlen(error);
        if (error_len > 0 && strncmp(stack, error, error_len) == 0 &&
            (stack[error_len] == '\n' || stack[error_len] == '\0')) {
          snprintf(str, sizeof(str), "%s", stack);
        } else {
          snprintf(str, sizeof(str), "%s\n%s", error, stack);
        }
      } else {
        snprintf(str, sizeof(str), "%s", stack);
      }
    } else {
      snprintf(str, sizeof(str), "%s", error ? error : "");
    }
    JS_FreeCString(rt->ctx, stack);
  } else {
    snprintf(str, sizeof(str), "%s", error ? error : "");
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

  // Process ONLY immediate pending JS jobs, no event loop processing
  // This prevents hanging when server.listen() creates libuv handles
  // The main event loop in JSRT_RuntimeRun will handle ALL async operations
  JSRT_Debug("await eval result: processing immediate pending JS jobs only");

  // Process pending JS jobs (e.g., module initialization, synchronous promises)
  for (int i = 0; i < 3; i++) {
    if (!JS_IsJobPending(rt->rt)) {
      JSRT_Debug("No more pending jobs after %d cycles", i);
      break;
    }

    bool js_ret = JSRT_RuntimeRunTicket(rt);
    JSRT_Debug("await eval result: cycle %d, js_ret=%d", i, js_ret);

    if (!js_ret) {
      JSRT_Debug("JavaScript execution failed");
      break;
    }
  }

  // Do NOT run uv_run here - it causes hanging when server.listen() is called
  // console.log and other I/O will be flushed by JSRT_RuntimeRun

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
#ifdef DEBUG
      if (counter % 10 == 0) {
        JSRT_Debug("uv_loop still alive counter=%llu", (unsigned long long)counter);
        uv_print_active_handles(rt->uv_loop, stderr);
        fflush(stderr);
      }
#endif
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
void JSRT_RuntimeSetCompactNodeMode(JSRT_Runtime* rt, bool enabled) {
  rt->compact_node_mode = enabled;
  JSRT_Debug("Compact Node.js mode: %s", enabled ? "enabled" : "disabled");
}

static void jsrt_debug_handle_walker(uv_handle_t* handle, void* arg) {
#ifdef DEBUG
  (void)arg;  // Unused in release builds
  const char* type_name = uv_handle_type_name(handle->type);
  JSRT_Debug("active handle type=%s ref=%d closing=%d", type_name ? type_name : "unknown", uv_has_ref(handle),
             uv_is_closing(handle));
#else
  (void)handle;
  (void)arg;
#endif
}

static void jsrt_debug_dump_handles(uv_loop_t* loop) {
#ifdef DEBUG
  uv_walk(loop, jsrt_debug_handle_walker, NULL);
#else
  (void)loop;
#endif
}
