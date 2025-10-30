#include <string.h>
#include <sys/time.h>
#include "process.h"

// Global variables for process information
char** jsrt_argv = NULL;
int jsrt_argc = 0;
char* jsrt_argv0 = NULL;
struct timeval jsrt_start_time = {0, 0};

// Global process module object - initialize with zero, set to JS_UNDEFINED at runtime
static JSValue g_process_module = {0};

// Initialize the unified process module
JSValue jsrt_init_unified_process_module(JSContext* ctx) {
  // Create the main process object
  JSValue process = JS_NewObject(ctx);
  if (JS_IsException(process)) {
    return JS_EXCEPTION;
  }

  // Initialize all components
  jsrt_process_init_platform();
  jsrt_process_init_basic();
  jsrt_process_init_versions();
  jsrt_process_init_env();
  jsrt_process_init_timing();
  jsrt_process_init_control();
  jsrt_process_init_nodejs();
  jsrt_process_init_properties();
  jsrt_process_init_signals();
  jsrt_process_init_events();
  jsrt_process_init_resources();

  // Basic process information properties (as getters for Node.js compatibility)
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "pid"), JS_NewCFunction(ctx, js_process_get_pid, "get pid", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "ppid"),
                          JS_NewCFunction(ctx, js_process_get_ppid, "get ppid", 0), JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "argv"),
                          JS_NewCFunction(ctx, js_process_get_argv, "get argv", 0), JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "argv0"),
                          JS_NewCFunction(ctx, js_process_get_argv0, "get argv0", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "platform"),
                          JS_NewCFunction(ctx, js_process_get_platform, "get platform", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "arch"),
                          JS_NewCFunction(ctx, js_process_get_arch, "get arch", 0), JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  // Version information (as getters for Node.js compatibility)
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "version"),
                          JS_NewCFunction(ctx, js_process_get_version, "get version", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "versions"),
                          JS_NewCFunction(ctx, js_process_get_versions, "get versions", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  // Environment variables (as getter for Node.js compatibility)
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "env"), JS_NewCFunction(ctx, js_process_get_env, "get env", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  // Timing functions
  JS_SetPropertyStr(ctx, process, "uptime", JS_NewCFunction(ctx, js_process_uptime, "uptime", 0));

  // hrtime with bigint() method
  JSValue hrtime = JS_NewCFunction(ctx, js_process_hrtime, "hrtime", 1);
  JS_SetPropertyStr(ctx, hrtime, "bigint", JS_NewCFunction(ctx, js_process_hrtime_bigint, "bigint", 0));
  JS_SetPropertyStr(ctx, process, "hrtime", hrtime);

  // Process control functions
  JS_SetPropertyStr(ctx, process, "exit", JS_NewCFunction(ctx, js_process_exit, "exit", 1));
  JS_SetPropertyStr(ctx, process, "cwd", JS_NewCFunction(ctx, js_process_cwd, "cwd", 0));
  JS_SetPropertyStr(ctx, process, "chdir", JS_NewCFunction(ctx, js_process_chdir, "chdir", 1));

  // Node.js specific functions
  JS_SetPropertyStr(ctx, process, "nextTick", JS_NewCFunction(ctx, js_process_nextTick, "nextTick", 1));

  // Memory and resource monitoring
  JSValue memoryUsage = JS_NewCFunction(ctx, js_process_memoryUsage, "memoryUsage", 0);
  JS_SetPropertyStr(ctx, memoryUsage, "rss", JS_NewCFunction(ctx, js_process_memory_usage_rss, "rss", 0));
  JS_SetPropertyStr(ctx, process, "memoryUsage", memoryUsage);

  JS_SetPropertyStr(ctx, process, "cpuUsage", JS_NewCFunction(ctx, js_process_cpu_usage, "cpuUsage", 1));
  JS_SetPropertyStr(ctx, process, "resourceUsage", JS_NewCFunction(ctx, js_process_resource_usage, "resourceUsage", 0));
  JS_SetPropertyStr(ctx, process, "availableMemory",
                    JS_NewCFunction(ctx, js_process_available_memory, "availableMemory", 0));
  JS_SetPropertyStr(ctx, process, "constrainedMemory",
                    JS_NewCFunction(ctx, js_process_constrained_memory, "constrainedMemory", 0));

  // Signal handling (signals.c)
  JS_SetPropertyStr(ctx, process, "kill", JS_NewCFunction(ctx, js_process_kill, "kill", 2));

  // Event handling (events.c)
  JS_SetPropertyStr(ctx, process, "on", JS_NewCFunction(ctx, js_process_on_events, "on", 2));
  JS_SetPropertyStr(ctx, process, "emit", JS_NewCFunction(ctx, js_process_emit_events, "emit", 1));
  JS_SetPropertyStr(ctx, process, "emitWarning", JS_NewCFunction(ctx, js_process_emit_warning, "emitWarning", 3));
  JS_SetPropertyStr(
      ctx, process, "setUncaughtExceptionCaptureCallback",
      JS_NewCFunction(ctx, js_process_set_uncaught_exception_capture, "setUncaughtExceptionCaptureCallback", 1));
  JS_SetPropertyStr(
      ctx, process, "hasUncaughtExceptionCaptureCallback",
      JS_NewCFunction(ctx, js_process_has_uncaught_exception_capture, "hasUncaughtExceptionCaptureCallback", 0));

  // Standard I/O streams
  JS_SetPropertyStr(ctx, process, "stdout", jsrt_create_stdout(ctx));
  JS_SetPropertyStr(ctx, process, "stderr", jsrt_create_stderr(ctx));
  JS_SetPropertyStr(ctx, process, "stdin", jsrt_create_stdin(ctx));

  // Additional properties from properties.c
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "execPath"),
                          JS_NewCFunction(ctx, js_process_get_execPath, "get execPath", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "execArgv"),
                          JS_NewCFunction(ctx, js_process_get_execArgv, "get execArgv", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "exitCode"),
                          JS_NewCFunction(ctx, js_process_get_exitCode, "get exitCode", 0),
                          JS_NewCFunction(ctx, js_process_set_exitCode, "set exitCode", 1), JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "title"),
                          JS_NewCFunction(ctx, js_process_get_title, "get title", 0),
                          JS_NewCFunction(ctx, js_process_set_title, "set title", 1), JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "config"),
                          JS_NewCFunction(ctx, js_process_get_config, "get config", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "release"),
                          JS_NewCFunction(ctx, js_process_get_release, "get release", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(ctx, process, JS_NewAtom(ctx, "features"),
                          JS_NewCFunction(ctx, js_process_get_features, "get features", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  // Store the module globally for reuse
  g_process_module = JS_DupValue(ctx, process);

  return process;
}

// Get the unified process module (for jsrt:process)
JSValue jsrt_get_process_module(JSContext* ctx) {
  // Check if module is uninitialized (zero-initialized or JS_UNDEFINED)
  if (g_process_module.tag == 0 || JS_IsUndefined(g_process_module)) {
    return jsrt_init_unified_process_module(ctx);
  }
  return JS_DupValue(ctx, g_process_module);
}

// Get the unified process module (for node:process)
JSValue node_get_process_module(JSContext* ctx) {
  // Both jsrt:process and node:process return the same module
  return jsrt_get_process_module(ctx);
}

// Module initialization for jsrt:process
JSValue jsrt_init_module_process(JSContext* ctx) {
  return jsrt_get_process_module(ctx);
}

// Module initialization for node:process
JSValue node_init_module_process(JSContext* ctx) {
  return node_get_process_module(ctx);
}

// Cleanup function
void jsrt_process_cleanup(JSContext* ctx) {
  if (g_process_module.tag != 0 && !JS_IsUndefined(g_process_module)) {
    JS_FreeValue(ctx, g_process_module);
    g_process_module = (JSValue){0};
  }
  jsrt_process_cleanup_properties();
  jsrt_process_cleanup_signals(ctx);
  jsrt_process_cleanup_events(ctx);
}

// Legacy compatibility functions for existing code

// Create process module (legacy jsrt:process style)
JSValue JSRT_CreateProcessModule(JSContext* ctx) {
  return jsrt_get_process_module(ctx);
}

// Initialize Node.js process module (legacy node:process style)
JSValue JSRT_InitNodeProcess(JSContext* ctx) {
  return node_get_process_module(ctx);
}

// Unified initialization function for both modules (ES module style)
int js_unified_process_init(JSContext* ctx, JSModuleDef* m) {
  JSValue process_obj = jsrt_get_process_module(ctx);
  if (JS_IsException(process_obj)) {
    return -1;
  }

  // Export as default
  JS_SetModuleExport(ctx, m, "default", process_obj);
  return 0;
}

// Runtime setup function (replaces old JSRT_RuntimeSetupStdProcess)
void JSRT_RuntimeSetupStdProcess(JSRT_Runtime* rt) {
  // Initialize start time if not already done
  if (jsrt_start_time.tv_sec == 0 && jsrt_start_time.tv_usec == 0) {
    jsrt_process_gettimeofday(&jsrt_start_time, NULL);
  }

  // Create process object and set it as global
  JSValue process_obj = jsrt_get_process_module(rt->ctx);
  JS_SetPropertyStr(rt->ctx, rt->global, "process", process_obj);

  // Setup IPC if this is a forked child process
  jsrt_process_setup_ipc(rt->ctx, process_obj, rt);

  // Setup signal handling
  jsrt_process_setup_signals(rt->ctx, process_obj, rt->uv_loop);

  // Setup event handling
  jsrt_process_setup_events(rt->ctx, process_obj);
}