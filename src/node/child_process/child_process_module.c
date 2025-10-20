#include "../../util/debug.h"
#include "child_process_internal.h"

// Class definition for ChildProcess
static JSClassDef js_child_process_class = {
    "ChildProcess",
    .finalizer = js_child_process_finalizer,
};

JSClassID js_child_process_class_id;

// CommonJS module initialization
JSValue JSRT_InitNodeChildProcess(JSContext* ctx) {
  JSRT_Debug("Initializing child_process module");

  JSValue cp = JS_NewObject(ctx);

  // Register ChildProcess class
  JS_NewClassID(&js_child_process_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_child_process_class_id, &js_child_process_class);

  // Create ChildProcess prototype
  JSValue child_proto = JS_NewObject(ctx);

  // Add ChildProcess methods
  JS_SetPropertyStr(ctx, child_proto, "kill", JS_NewCFunction(ctx, js_child_process_kill, "kill", 1));
  JS_SetPropertyStr(ctx, child_proto, "send", JS_NewCFunction(ctx, js_child_process_send, "send", 2));
  JS_SetPropertyStr(ctx, child_proto, "disconnect", JS_NewCFunction(ctx, js_child_process_disconnect, "disconnect", 0));
  JS_SetPropertyStr(ctx, child_proto, "ref", JS_NewCFunction(ctx, js_child_process_ref, "ref", 0));
  JS_SetPropertyStr(ctx, child_proto, "unref", JS_NewCFunction(ctx, js_child_process_unref, "unref", 0));

  JS_SetClassProto(ctx, js_child_process_class_id, child_proto);

  // Export module functions
  JS_SetPropertyStr(ctx, cp, "spawn", JS_NewCFunction(ctx, js_child_process_spawn, "spawn", 3));
  JS_SetPropertyStr(ctx, cp, "exec", JS_NewCFunction(ctx, js_child_process_exec, "exec", 3));
  JS_SetPropertyStr(ctx, cp, "execFile", JS_NewCFunction(ctx, js_child_process_exec_file, "execFile", 4));
  JS_SetPropertyStr(ctx, cp, "fork", JS_NewCFunction(ctx, js_child_process_fork, "fork", 3));
  JS_SetPropertyStr(ctx, cp, "spawnSync", JS_NewCFunction(ctx, js_child_process_spawn_sync, "spawnSync", 3));
  JS_SetPropertyStr(ctx, cp, "execSync", JS_NewCFunction(ctx, js_child_process_exec_sync, "execSync", 2));
  JS_SetPropertyStr(ctx, cp, "execFileSync", JS_NewCFunction(ctx, js_child_process_exec_file_sync, "execFileSync", 3));

  return cp;
}

// ES Module initialization
int js_node_child_process_init(JSContext* ctx, JSModuleDef* m) {
  JSValue cp = JSRT_InitNodeChildProcess(ctx);

  // Set module exports
  JS_SetModuleExport(ctx, m, "spawn", JS_GetPropertyStr(ctx, cp, "spawn"));
  JS_SetModuleExport(ctx, m, "exec", JS_GetPropertyStr(ctx, cp, "exec"));
  JS_SetModuleExport(ctx, m, "execFile", JS_GetPropertyStr(ctx, cp, "execFile"));
  JS_SetModuleExport(ctx, m, "fork", JS_GetPropertyStr(ctx, cp, "fork"));
  JS_SetModuleExport(ctx, m, "spawnSync", JS_GetPropertyStr(ctx, cp, "spawnSync"));
  JS_SetModuleExport(ctx, m, "execSync", JS_GetPropertyStr(ctx, cp, "execSync"));
  JS_SetModuleExport(ctx, m, "execFileSync", JS_GetPropertyStr(ctx, cp, "execFileSync"));
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, cp));

  JS_FreeValue(ctx, cp);
  return 0;
}
