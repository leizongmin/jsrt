#ifndef JSRT_NODE_ASYNC_HOOKS_H
#define JSRT_NODE_ASYNC_HOOKS_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

// Async Hook IDs
typedef uint32_t async_id_t;

// Async Hook Types
typedef enum {
  NODE_ASYNC_HOOK_INIT = 0,
  NODE_ASYNC_HOOK_BEFORE = 1,
  NODE_ASYNC_HOOK_AFTER = 2,
  NODE_ASYNC_HOOK_DESTROY = 3,
  NODE_ASYNC_HOOK_PROMISE_RESOLVE = 4,
  NODE_ASYNC_HOOK_TYPE_COUNT = 5
} NodeAsyncHookType;

// Async Resource Types
#define NODE_ASYNC_RESOURCE_TYPE_PROMISE "PROMISE"
#define NODE_ASYNC_RESOURCE_TYPE_TIMEOUT "TIMEOUT"
#define NODE_ASYNC_RESOURCE_TYPE_TICKOBJECT "TickObject"
#define NODE_ASYNC_RESOURCE_TYPE_IMMEDIATE "Immediate"
#define NODE_ASYNC_RESOURCE_TYPE_MICROTASK "Microtask"

// Initialize async hooks module
JSValue JSRT_InitNodeAsyncHooks(JSContext* ctx);

// ES module initialization function
int js_node_async_hooks_init(JSContext* ctx, JSModuleDef* m);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_NODE_ASYNC_HOOKS_H