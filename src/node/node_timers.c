#include <stdlib.h>
#include <string.h>
#include "../runtime.h"
#include "../util/debug.h"
#include "node_modules.h"

// Node.js immediate timer implementation
// setImmediate schedules a callback to be invoked in the next iteration of the event loop

typedef struct {
  JSContext* ctx;
  JSValue callback;
  JSValue* args;
  int argc;
  uint64_t immediate_id;
  uv_check_t check_handle;
  bool is_cleared;
} NodeImmediate;

static uint64_t next_immediate_id = 1;
static NodeImmediate** immediates = NULL;
static size_t immediate_count = 0;
static size_t immediate_capacity = 0;

// Add immediate to tracking array
static void add_immediate(NodeImmediate* immediate) {
  if (immediate_count >= immediate_capacity) {
    immediate_capacity = immediate_capacity ? immediate_capacity * 2 : 8;
    immediates = realloc(immediates, immediate_capacity * sizeof(NodeImmediate*));
  }
  immediates[immediate_count++] = immediate;
}

// Remove immediate from tracking array
static void remove_immediate(uint64_t immediate_id) {
  for (size_t i = 0; i < immediate_count; i++) {
    if (immediates[i] && immediates[i]->immediate_id == immediate_id) {
      immediates[i] = NULL;  // Mark as removed
      break;
    }
  }
}

// UV check callback - executes the immediate callback
static void immediate_check_callback(uv_check_t* check) {
  NodeImmediate* immediate = (NodeImmediate*)check->data;

  if (immediate->is_cleared) {
    return;
  }

  JSContext* ctx = immediate->ctx;

  // Execute the callback
  JSValue result = JS_Call(ctx, immediate->callback, JS_UNDEFINED, immediate->argc, immediate->args);

  if (JS_IsException(result)) {
    JSRT_Debug("setImmediate callback threw an exception");
  }

  JS_FreeValue(ctx, result);

  // Stop and close the check handle
  uv_check_stop(check);
  uv_close((uv_handle_t*)check, NULL);

  // Clean up
  JS_FreeValue(ctx, immediate->callback);
  for (int i = 0; i < immediate->argc; i++) {
    JS_FreeValue(ctx, immediate->args[i]);
  }
  free(immediate->args);

  // Remove from tracking
  remove_immediate(immediate->immediate_id);

  free(immediate);
}

// setImmediate(callback[, ...args])
static JSValue js_set_immediate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "setImmediate requires a callback function");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "setImmediate callback must be a function");
  }

  // Create immediate structure
  NodeImmediate* immediate = malloc(sizeof(NodeImmediate));
  immediate->ctx = ctx;
  immediate->callback = JS_DupValue(ctx, argv[0]);
  immediate->immediate_id = next_immediate_id++;
  immediate->is_cleared = false;

  // Copy additional arguments
  immediate->argc = argc - 1;
  if (immediate->argc > 0) {
    immediate->args = malloc(immediate->argc * sizeof(JSValue));
    for (int i = 0; i < immediate->argc; i++) {
      immediate->args[i] = JS_DupValue(ctx, argv[i + 1]);
    }
  } else {
    immediate->args = NULL;
  }

  // Get runtime and event loop
  JSRuntime* rt = JS_GetRuntime(ctx);
  JSRT_Runtime* jsrt_rt = (JSRT_Runtime*)JS_GetRuntimeOpaque(rt);
  uv_loop_t* loop = jsrt_rt->uv_loop;

  // Initialize UV check handle
  uv_check_init(loop, &immediate->check_handle);
  immediate->check_handle.data = immediate;

  // Start the check - this will execute in the next loop iteration
  uv_check_start(&immediate->check_handle, immediate_check_callback);

  // Track the immediate
  add_immediate(immediate);

  // Return the immediate ID (as number for simplicity)
  return JS_NewInt64(ctx, immediate->immediate_id);
}

// clearImmediate(immediate)
static JSValue js_clear_immediate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_UNDEFINED;
  }

  uint64_t immediate_id;
  if (JS_ToInt64(ctx, (int64_t*)&immediate_id, argv[0]) < 0) {
    return JS_UNDEFINED;  // Silently ignore invalid IDs like Node.js
  }

  // Find and clear the immediate
  for (size_t i = 0; i < immediate_count; i++) {
    if (immediates[i] && immediates[i]->immediate_id == immediate_id) {
      immediates[i]->is_cleared = true;
      uv_check_stop(&immediates[i]->check_handle);
      uv_close((uv_handle_t*)&immediates[i]->check_handle, NULL);
      break;
    }
  }

  return JS_UNDEFINED;
}

// Initialize Node.js timer globals
JSValue JSRT_InitNodeTimers(JSContext* ctx) {
  JSValue timers = JS_NewObject(ctx);

  JS_SetPropertyStr(ctx, timers, "setImmediate", JS_NewCFunction(ctx, js_set_immediate, "setImmediate", 1));
  JS_SetPropertyStr(ctx, timers, "clearImmediate", JS_NewCFunction(ctx, js_clear_immediate, "clearImmediate", 1));

  return timers;
}

// Add Node.js timer globals to global object
void JSRT_AddNodeTimerGlobals(JSContext* ctx) {
  JSValue global = JS_GetGlobalObject(ctx);

  JS_SetPropertyStr(ctx, global, "setImmediate", JS_NewCFunction(ctx, js_set_immediate, "setImmediate", 1));
  JS_SetPropertyStr(ctx, global, "clearImmediate", JS_NewCFunction(ctx, js_clear_immediate, "clearImmediate", 1));

  JS_FreeValue(ctx, global);
}