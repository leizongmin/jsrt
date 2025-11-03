#ifndef __JSRT_MODULE_HOOKS_H__
#define __JSRT_MODULE_HOOKS_H__

/**
 * Module Hooks Infrastructure
 *
 * Provides hook registration and execution framework for customizing
 * module resolution and loading without forking the runtime.
 *
 * Maintains compatibility with Node.js hook semantics for synchronous workflows.
 */

#include <quickjs.h>
#include <stdbool.h>

/**
 * Hook Types
 *
 * Hooks allow embedders to intercept and customize module loading behavior.
 * They are executed in LIFO order (last registered, first called).
 */
typedef enum {
  JSRT_HOOK_RESOLVE = 0,  // Module resolution hooks
  JSRT_HOOK_LOAD = 1,     // Module loading hooks
  JSRT_HOOK_COUNT = 2     // Number of hook types
} JSRT_HookType;

/**
 * Module Hook Structure
 *
 * Represents a single hook in the hook chain. Each hook contains
 * JavaScript functions for different phases of module loading.
 */
typedef struct JSRTModuleHook {
  JSValue resolve_fn;           // resolve(specifier, context, next) - optional
  JSValue load_fn;              // load(url, context, next) - optional
  struct JSRTModuleHook* next;  // Next in chain (LIFO order)
} JSRTModuleHook;

/**
 * Hook Registry
 *
 * Manages all registered hooks and provides execution infrastructure.
 */
typedef struct JSRTHookRegistry {
  JSContext* ctx;         // JavaScript context for finalizers
  JSRTModuleHook* hooks;  // Hook chain (LIFO)
  int hook_count;         // Number of registered hooks
  bool initialized;       // Registry initialization state
} JSRTHookRegistry;

/**
 * Hook Context
 *
 * Context object passed to hook functions to provide information
 * about the current module loading operation.
 */
typedef struct JSRTHookContext {
  const char* specifier;     // Original module specifier
  const char* base_path;     // Base path for resolution
  const char* resolved_url;  // Resolved URL (if available)
  bool is_main_module;       // Whether this is the main module
  void* user_data;           // User-defined data
} JSRTHookContext;

/**
 * Initialize hook registry
 *
 * Creates and initializes a new hook registry for the given context.
 *
 * @param ctx The JavaScript context
 * @return Initialized registry, or NULL on failure
 */
JSRTHookRegistry* jsrt_hook_registry_init(JSContext* ctx);

/**
 * Free hook registry
 *
 * Cleans up all registered hooks and frees the registry.
 *
 * @param registry The hook registry to free
 */
void jsrt_hook_registry_free(JSRTHookRegistry* registry);

/**
 * Register a module hook
 *
 * Registers a new hook for customizing module loading. The hook will be
 * called in LIFO order (last registered, first called).
 *
 * @param registry The hook registry
 * @param resolve_fn JavaScript function for resolve hook (can be JS_NULL)
 * @param load_fn JavaScript function for load hook (can be JS_NULL)
 * @return 0 on success, -1 on failure
 */
int jsrt_hook_register(JSRTHookRegistry* registry, JSValue resolve_fn, JSValue load_fn);

/**
 * Execute resolve hooks
 *
 * Executes all registered resolve hooks in LIFO order. If any hook
 * returns a non-null value, the chain is short-circuited.
 *
 * @param registry The hook registry
 * @param context Hook context information
 * @return Resolved URL as string (caller must free), or NULL to continue normal processing
 */
char* jsrt_hook_execute_resolve(JSRTHookRegistry* registry, const JSRTHookContext* context);

/**
 * Execute load hooks
 *
 * Executes all registered load hooks in LIFO order. If any hook
 * returns a non-null value, the chain is short-circuited.
 *
 * @param registry The hook registry
 * @param context Hook context information
 * @param url The resolved module URL
 * @return Module source as string (caller must free), or NULL to continue normal processing
 */
char* jsrt_hook_execute_load(JSRTHookRegistry* registry, const JSRTHookContext* context, const char* url);

/**
 * Get hook count
 *
 * Returns the number of registered hooks.
 *
 * @param registry The hook registry
 * @return Number of hooks
 */
int jsrt_hook_get_count(JSRTHookRegistry* registry);

/**
 * Clear all hooks
 *
 * Removes all registered hooks from the registry.
 *
 * @param registry The hook registry
 */
void jsrt_hook_clear_all(JSRTHookRegistry* registry);

#endif  // __JSRT_MODULE_HOOKS_H__