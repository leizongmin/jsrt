/**
 * Node.js Async Hooks Implementation
 *
 * Provides minimal async hooks compatibility for React DOM and other packages
 * that depend on async_hooks functionality.
 */

#include "async_hooks.h"
#include <stdlib.h>
#include <string.h>
#include "../../runtime.h"
#include "../../util/debug.h"

// Global async ID counter
static async_id_t next_async_id = 1;

// Create async ID
static JSValue js_async_hooks_create_hook(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Simplified implementation: return a numeric ID
  async_id_t id = next_async_id++;
  return JS_NewInt32(ctx, id);
}

// Create async resource
static JSValue js_async_hooks_create_async_resource(JSContext* ctx, JSValueConst this_val, int argc,
                                                    JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "asyncResource expects at least 2 arguments");
  }

  // Get type and trigger ID
  const char* type = JS_ToCString(ctx, argv[0]);
  if (!type) {
    return JS_EXCEPTION;
  }

  int32_t trigger_id;
  if (JS_ToInt32(ctx, &trigger_id, argv[1]) < 0) {
    JS_FreeCString(ctx, type);
    return JS_EXCEPTION;
  }

  // Create resource object
  JSValue resource = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, resource, "asyncId", JS_NewInt32(ctx, next_async_id++));
  JS_SetPropertyStr(ctx, resource, "triggerAsyncId", JS_NewInt32(ctx, trigger_id));
  JS_SetPropertyStr(ctx, resource, "type", JS_NewString(ctx, type));

  JS_FreeCString(ctx, type);
  return resource;
}

// Get execution async ID
static JSValue js_async_hooks_execution_async_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Simplified: return a fake ID
  return JS_NewInt32(ctx, 1);
}

// Get trigger async ID
static JSValue js_async_hooks_trigger_async_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Simplified: return a fake trigger ID
  return JS_NewInt32(ctx, 0);
}

// Enable/Disable async hooks (stub implementation)
static JSValue js_async_hooks_enable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Stub: return true
  return JS_TRUE;
}

static JSValue js_async_hooks_disable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Stub: return false
  return JS_FALSE;
}

// Initialize async hooks module
JSValue JSRT_InitNodeAsyncHooks(JSContext* ctx) {
  JSValue async_hooks = JS_NewObject(ctx);

  // Main hook creation function
  JS_SetPropertyStr(ctx, async_hooks, "createHook", JS_NewCFunction(ctx, js_async_hooks_create_hook, "createHook", 4));

  // Async resource functions
  JS_SetPropertyStr(ctx, async_hooks, "createAsyncResource",
                    JS_NewCFunction(ctx, js_async_hooks_create_async_resource, "createAsyncResource", 3));

  // Execution tracking
  JS_SetPropertyStr(ctx, async_hooks, "executionAsyncId",
                    JS_NewCFunction(ctx, js_async_hooks_execution_async_id, "executionAsyncId", 0));
  JS_SetPropertyStr(ctx, async_hooks, "triggerAsyncId",
                    JS_NewCFunction(ctx, js_async_hooks_trigger_async_id, "triggerAsyncId", 0));

  // Enable/disable functions
  JS_SetPropertyStr(ctx, async_hooks, "enable", JS_NewCFunction(ctx, js_async_hooks_enable, "enable", 1));
  JS_SetPropertyStr(ctx, async_hooks, "disable", JS_NewCFunction(ctx, js_async_hooks_disable, "disable", 1));

  // Constants for hook types
  JSValue types = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, types, "INIT", JS_NewInt32(ctx, NODE_ASYNC_HOOK_INIT));
  JS_SetPropertyStr(ctx, types, "BEFORE", JS_NewInt32(ctx, NODE_ASYNC_HOOK_BEFORE));
  JS_SetPropertyStr(ctx, types, "AFTER", JS_NewInt32(ctx, NODE_ASYNC_HOOK_AFTER));
  JS_SetPropertyStr(ctx, types, "DESTROY", JS_NewInt32(ctx, NODE_ASYNC_HOOK_DESTROY));
  JS_SetPropertyStr(ctx, types, "PROMISE_RESOLVE", JS_NewInt32(ctx, NODE_ASYNC_HOOK_PROMISE_RESOLVE));
  JS_SetPropertyStr(ctx, async_hooks, "types", types);

  return async_hooks;
}

// ES module initialization
int js_node_async_hooks_init(JSContext* ctx, JSModuleDef* m) {
  JSValue async_hooks = JSRT_InitNodeAsyncHooks(ctx);

  // Export individual functions
  JS_SetModuleExport(ctx, m, "createHook", JS_GetPropertyStr(ctx, async_hooks, "createHook"));
  JS_SetModuleExport(ctx, m, "createAsyncResource", JS_GetPropertyStr(ctx, async_hooks, "createAsyncResource"));
  JS_SetModuleExport(ctx, m, "executionAsyncId", JS_GetPropertyStr(ctx, async_hooks, "executionAsyncId"));
  JS_SetModuleExport(ctx, m, "triggerAsyncId", JS_GetPropertyStr(ctx, async_hooks, "triggerAsyncId"));
  JS_SetModuleExport(ctx, m, "enable", JS_GetPropertyStr(ctx, async_hooks, "enable"));
  JS_SetModuleExport(ctx, m, "disable", JS_GetPropertyStr(ctx, async_hooks, "disable"));

  // Export the whole module as default
  JS_SetModuleExport(ctx, m, "default", async_hooks);

  return 0;
}