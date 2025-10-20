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
  JS_SetPropertyStr(ctx, process, "hrtime", JS_NewCFunction(ctx, js_process_hrtime, "hrtime", 1));

  // Process control functions
  JS_SetPropertyStr(ctx, process, "exit", JS_NewCFunction(ctx, js_process_exit, "exit", 1));
  JS_SetPropertyStr(ctx, process, "cwd", JS_NewCFunction(ctx, js_process_cwd, "cwd", 0));
  JS_SetPropertyStr(ctx, process, "chdir", JS_NewCFunction(ctx, js_process_chdir, "chdir", 1));

  // Node.js specific functions
  JS_SetPropertyStr(ctx, process, "nextTick", JS_NewCFunction(ctx, js_process_nextTick, "nextTick", 1));
  JS_SetPropertyStr(ctx, process, "memoryUsage", JS_NewCFunction(ctx, js_process_memoryUsage, "memoryUsage", 0));

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

  // TODO: Setup IPC if this is a forked child process
  // This requires more complex integration to properly inherit the pipe from parent
  // For now, child-side IPC is not automatically configured
  // jsrt_process_setup_ipc(rt->ctx, process_obj, rt);
}