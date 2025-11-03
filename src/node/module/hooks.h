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
  bool trace_enabled;     // Trace hook execution (from --trace-module-hooks flag)
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
  char* conditions[32];      // Resolution conditions (array of strings, NULL-terminated)
  int condition_count;       // Number of conditions
  void* user_data;           // User-defined data
} JSRTHookContext;

/**
 * Load Hook Result Source Types
 *
 * Supported source types for load hook results.
 */
typedef enum {
  JSRT_HOOK_SOURCE_STRING = 0,    // String source
  JSRT_HOOK_SOURCE_ARRAY_BUFFER,  // ArrayBuffer source
  JSRT_HOOK_SOURCE_UINT8_ARRAY,   // Uint8Array source
  JSRT_HOOK_SOURCE_UNKNOWN        // Unknown/invalid source
} JSRTHookSourceType;

/**
 * Load Hook Result
 *
 * Structure to hold the result from a load hook, supporting multiple
 * source types as specified in the Node.js module hook API.
 */
typedef struct JSRTHookLoadResult {
  JSRTHookSourceType source_type;  // Type of source data
  char* format;                    // Module format (e.g., "module", "commonjs", "json")
  union {
    char* string;  // String source data
    struct {
      uint8_t* data;  // Binary data
      size_t size;    // Size of binary data
    } binary;         // ArrayBuffer/Uint8Array data
  } source;
  bool short_circuit;  // Whether to short-circuit the hook chain
} JSRTHookLoadResult;

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
 * Execute resolve hooks with enhanced Node.js compatibility
 *
 * Executes resolve hooks with proper Node.js-compatible context including
 * conditions array and support for nextResolve() chaining.
 *
 * @param registry The hook registry
 * @param specifier Module specifier string
 * @param context Hook context information
 * @param conditions Array of condition strings (NULL-terminated)
 * @return Resolved URL as string (caller must free), or NULL to continue normal processing
 */
char* jsrt_hook_execute_resolve_enhanced(JSRTHookRegistry* registry, const char* specifier,
                                         const JSRTHookContext* context, char** conditions);

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
 * Execute load hooks with enhanced Node.js compatibility
 *
 * Executes load hooks with proper Node.js-compatible context including
 * format, conditions array and support for nextLoad() chaining.
 * Supports multiple source types (string, ArrayBuffer, Uint8Array).
 *
 * @param registry The hook registry
 * @param url The resolved module URL
 * @param context Hook context information
 * @param format Module format (e.g., "module", "commonjs", "json")
 * @param conditions Array of condition strings (NULL-terminated)
 * @return Load hook result structure (caller must free), or NULL to continue normal processing
 */
JSRTHookLoadResult* jsrt_hook_execute_load_enhanced(JSRTHookRegistry* registry, const char* url,
                                                    const JSRTHookContext* context, const char* format,
                                                    char** conditions);

/**
 * Free load hook result
 *
 * Frees all memory associated with a load hook result.
 *
 * @param result The load hook result to free
 */
void jsrt_hook_load_result_free(JSRTHookLoadResult* result);

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

/**
 * Enable/disable hook tracing
 *
 * Controls verbose tracing of hook execution to stderr.
 *
 * @param registry The hook registry
 * @param enabled Whether to enable tracing
 */
void jsrt_hook_set_trace(JSRTHookRegistry* registry, bool enabled);

/**
 * Check if hook tracing is enabled
 *
 * @param registry The hook registry
 * @return true if tracing is enabled
 */
bool jsrt_hook_is_trace_enabled(JSRTHookRegistry* registry);

#endif  // __JSRT_MODULE_HOOKS_H__