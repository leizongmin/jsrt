/**
 * Module Hooks Infrastructure Implementation
 *
 * Provides hook registration and execution framework for customizing
 * module resolution and loading without forking the runtime.
 */

#include "hooks.h"
#include <stdlib.h>
#include <string.h>
#include "../../util/debug.h"

/**
 * Hook finalizer function
 *
 * Called when a hook is freed to properly clean up JavaScript values.
 *
 * @param rt The QuickJS runtime
 * @param val The hook object being finalized
 */
static void jsrt_hook_finalizer(JSRuntime* rt, JSValue val) {
  JSRTModuleHook* hook = JS_GetOpaque(val, 0);
  if (hook) {
    JSContext* ctx = JS_GetRuntimeOpaque(rt);
    if (ctx) {
      if (!JS_IsNull(hook->resolve_fn)) {
        JS_FreeValue(ctx, hook->resolve_fn);
      }
      if (!JS_IsNull(hook->load_fn)) {
        JS_FreeValue(ctx, hook->load_fn);
      }
    }
    free(hook);
  }
}

/**
 * Create a new hook structure
 *
 * Allocates and initializes a new hook with the provided functions.
 *
 * @param ctx The JavaScript context
 * @param resolve_fn JavaScript function for resolve hook
 * @param load_fn JavaScript function for load hook
 * @return New hook, or NULL on failure
 */
static JSRTModuleHook* jsrt_hook_create(JSContext* ctx, JSValue resolve_fn, JSValue load_fn) {
  JSRTModuleHook* hook = malloc(sizeof(JSRTModuleHook));
  if (!hook) {
    JSRT_Debug("Failed to allocate memory for hook");
    return NULL;
  }

  memset(hook, 0, sizeof(JSRTModuleHook));

  // Store functions with proper reference counting
  hook->resolve_fn = resolve_fn;
  hook->load_fn = load_fn;
  hook->next = NULL;

  // Add references to keep functions alive
  if (!JS_IsNull(resolve_fn)) {
    JS_DupValue(ctx, resolve_fn);
  }
  if (!JS_IsNull(load_fn)) {
    JS_DupValue(ctx, load_fn);
  }

  JSRT_Debug("Created hook: resolve_fn and load_fn registered");

  return hook;
}

/**
 * Free a hook structure
 *
 * Properly cleans up a hook and its JavaScript values.
 *
 * @param ctx The JavaScript context
 * @param hook The hook to free
 */
static void jsrt_hook_free(JSContext* ctx, JSRTModuleHook* hook) {
  if (!hook) {
    return;
  }

  JSRT_Debug("Freeing hook");

  // Release JavaScript function references
  if (!JS_IsNull(hook->resolve_fn)) {
    JS_FreeValue(ctx, hook->resolve_fn);
  }
  if (!JS_IsNull(hook->load_fn)) {
    JS_FreeValue(ctx, hook->load_fn);
  }

  free(hook);
}

/**
 * Create hook context object for JavaScript calls
 *
 * Creates a JavaScript object containing hook context information.
 *
 * @param ctx The JavaScript context
 * @param context Hook context structure
 * @return JavaScript context object
 */
static JSValue jsrt_hook_create_context_obj(JSContext* ctx, const JSRTHookContext* context) {
  JSValue context_obj = JS_NewObject(ctx);
  if (JS_IsException(context_obj)) {
    return JS_EXCEPTION;
  }

  // Set specifier
  if (context->specifier) {
    JSValue specifier = JS_NewString(ctx, context->specifier);
    if (JS_IsException(specifier)) {
      JS_FreeValue(ctx, context_obj);
      return JS_EXCEPTION;
    }
    JS_SetPropertyStr(ctx, context_obj, "specifier", specifier);
  }

  // Set base_path
  if (context->base_path) {
    JSValue base_path = JS_NewString(ctx, context->base_path);
    if (JS_IsException(base_path)) {
      JS_FreeValue(ctx, context_obj);
      return JS_EXCEPTION;
    }
    JS_SetPropertyStr(ctx, context_obj, "basePath", base_path);
  }

  // Set resolved_url
  if (context->resolved_url) {
    JSValue resolved_url = JS_NewString(ctx, context->resolved_url);
    if (JS_IsException(resolved_url)) {
      JS_FreeValue(ctx, context_obj);
      return JS_EXCEPTION;
    }
    JS_SetPropertyStr(ctx, context_obj, "resolvedUrl", resolved_url);
  }

  // Set is_main_module
  JS_SetPropertyStr(ctx, context_obj, "isMain", JS_NewBool(ctx, context->is_main_module));

  return context_obj;
}

/**
 * Next function for hook chaining
 *
 * JavaScript function that calls the next hook in the chain.
 * For now, this returns null to continue the normal processing chain.
 *
 * @param ctx The JavaScript context
 * @param this_val The 'this' value
 * @param argc Number of arguments
 * @param argv Arguments array
 * @return JS_NULL to continue normal processing
 */
static JSValue jsrt_hook_next_fn(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  return JS_NULL;
}

/**
 * Create next function for hook chaining
 *
 * Creates a JavaScript function that calls the next hook in the chain.
 * For now, this returns a function that always returns null to continue
 * the normal processing chain.
 *
 * @param ctx The JavaScript context
 * @return JavaScript next function
 */
static JSValue jsrt_hook_create_next_fn(JSContext* ctx) {
  // Create a function that returns null to continue normal processing
  return JS_NewCFunction(ctx, jsrt_hook_next_fn, "next", 0);
}

JSRTHookRegistry* jsrt_hook_registry_init(JSContext* ctx) {
  if (!ctx) {
    JSRT_Debug("Cannot initialize hook registry: null context");
    return NULL;
  }

  JSRTHookRegistry* registry = malloc(sizeof(JSRTHookRegistry));
  if (!registry) {
    JSRT_Debug("Failed to allocate memory for hook registry");
    return NULL;
  }

  memset(registry, 0, sizeof(JSRTHookRegistry));
  registry->ctx = ctx;
  registry->hooks = NULL;
  registry->hook_count = 0;
  registry->initialized = true;

  JSRT_Debug("Initialized hook registry");

  return registry;
}

void jsrt_hook_registry_free(JSRTHookRegistry* registry) {
  if (!registry) {
    return;
  }

  JSRT_Debug("Freeing hook registry with %d hooks", registry->hook_count);

  // Free all hooks in the chain
  JSRTModuleHook* current = registry->hooks;
  while (current) {
    JSRTModuleHook* next = current->next;
    jsrt_hook_free(registry->ctx, current);
    current = next;
  }

  free(registry);
}

int jsrt_hook_register(JSRTHookRegistry* registry, JSValue resolve_fn, JSValue load_fn) {
  if (!registry || !registry->initialized) {
    JSRT_Debug("Cannot register hook: registry not initialized");
    return -1;
  }

  // Validate that at least one function is provided
  if (JS_IsNull(resolve_fn) && JS_IsNull(load_fn)) {
    JSRT_Debug("Cannot register hook: both resolve_fn and load_fn are null");
    return -1;
  }

  // Validate function types
  if (!JS_IsNull(resolve_fn) && !JS_IsFunction(registry->ctx, resolve_fn)) {
    JSRT_Debug("Cannot register hook: resolve_fn is not a function");
    return -1;
  }

  if (!JS_IsNull(load_fn) && !JS_IsFunction(registry->ctx, load_fn)) {
    JSRT_Debug("Cannot register hook: load_fn is not a function");
    return -1;
  }

  JSRTModuleHook* hook = jsrt_hook_create(registry->ctx, resolve_fn, load_fn);
  if (!hook) {
    JSRT_Debug("Failed to create hook");
    return -1;
  }

  // Add to front of chain (LIFO order)
  hook->next = registry->hooks;
  registry->hooks = hook;
  registry->hook_count++;

  JSRT_Debug("Registered hook (total: %d)", registry->hook_count);

  return 0;
}

char* jsrt_hook_execute_resolve(JSRTHookRegistry* registry, const JSRTHookContext* context) {
  if (!registry || !registry->initialized || !context) {
    return NULL;
  }

  if (registry->hook_count == 0) {
    return NULL;
  }

  JSRT_Debug("Executing %d resolve hooks for specifier: %s", registry->hook_count,
             context->specifier ? context->specifier : "(null)");

  JSRTModuleHook* current = registry->hooks;
  while (current) {
    if (!JS_IsNull(current->resolve_fn)) {
      // Create context object for the hook
      JSValue context_obj = jsrt_hook_create_context_obj(registry->ctx, context);
      if (JS_IsException(context_obj)) {
        JSRT_Debug("Failed to create hook context object");
        return NULL;
      }

      // Create next function
      JSValue next_fn = jsrt_hook_create_next_fn(registry->ctx);
      if (JS_IsException(next_fn)) {
        JS_FreeValue(registry->ctx, context_obj);
        JSRT_Debug("Failed to create next function");
        return NULL;
      }

      // Call the resolve hook: resolve(specifier, context, next)
      JSValue argv[3];
      argv[0] = context->specifier ? JS_NewString(registry->ctx, context->specifier) : JS_NULL;
      argv[1] = context_obj;
      argv[2] = next_fn;

      JSValue result = JS_Call(registry->ctx, current->resolve_fn, JS_UNDEFINED, 3, argv);

      // Clean up arguments
      if (!JS_IsNull(argv[0])) {
        JS_FreeValue(registry->ctx, argv[0]);
      }
      JS_FreeValue(registry->ctx, context_obj);
      JS_FreeValue(registry->ctx, next_fn);

      if (JS_IsException(result)) {
        JSRT_Debug("Resolve hook threw exception");
        JS_FreeValue(registry->ctx, result);
        return NULL;
      }

      // Check if hook returned a result (short-circuit)
      if (!JS_IsNull(result) && !JS_IsUndefined(result)) {
        const char* result_str = JS_ToCString(registry->ctx, result);
        JS_FreeValue(registry->ctx, result);

        if (result_str) {
          char* result_copy = strdup(result_str);
          JS_FreeCString(registry->ctx, result_str);

          JSRT_Debug("Resolve hook returned result: %s", result_copy);
          return result_copy;
        }
      }

      JS_FreeValue(registry->ctx, result);
    }
    current = current->next;
  }

  JSRT_Debug("No resolve hook returned a result");
  return NULL;
}

char* jsrt_hook_execute_load(JSRTHookRegistry* registry, const JSRTHookContext* context, const char* url) {
  if (!registry || !registry->initialized || !context || !url) {
    return NULL;
  }

  if (registry->hook_count == 0) {
    return NULL;
  }

  JSRT_Debug("Executing %d load hooks for URL: %s", registry->hook_count, url);

  JSRTModuleHook* current = registry->hooks;
  while (current) {
    if (!JS_IsNull(current->load_fn)) {
      // Create context object for the hook
      JSValue context_obj = jsrt_hook_create_context_obj(registry->ctx, context);
      if (JS_IsException(context_obj)) {
        JSRT_Debug("Failed to create hook context object");
        return NULL;
      }

      // Create next function
      JSValue next_fn = jsrt_hook_create_next_fn(registry->ctx);
      if (JS_IsException(next_fn)) {
        JS_FreeValue(registry->ctx, context_obj);
        JSRT_Debug("Failed to create next function");
        return NULL;
      }

      // Call the load hook: load(url, context, next)
      JSValue argv[3];
      argv[0] = JS_NewString(registry->ctx, url);
      argv[1] = context_obj;
      argv[2] = next_fn;

      JSValue result = JS_Call(registry->ctx, current->load_fn, JS_UNDEFINED, 3, argv);

      // Clean up arguments
      JS_FreeValue(registry->ctx, argv[0]);
      JS_FreeValue(registry->ctx, context_obj);
      JS_FreeValue(registry->ctx, next_fn);

      if (JS_IsException(result)) {
        JSRT_Debug("Load hook threw exception");
        JS_FreeValue(registry->ctx, result);
        return NULL;
      }

      // Check if hook returned a result (short-circuit)
      if (!JS_IsNull(result) && !JS_IsUndefined(result)) {
        const char* result_str = JS_ToCString(registry->ctx, result);
        JS_FreeValue(registry->ctx, result);

        if (result_str) {
          char* result_copy = strdup(result_str);
          JS_FreeCString(registry->ctx, result_str);

          JSRT_Debug("Load hook returned result (length: %zu)", strlen(result_copy));
          return result_copy;
        }
      }

      JS_FreeValue(registry->ctx, result);
    }
    current = current->next;
  }

  JSRT_Debug("No load hook returned a result");
  return NULL;
}

int jsrt_hook_get_count(JSRTHookRegistry* registry) {
  if (!registry || !registry->initialized) {
    return 0;
  }
  return registry->hook_count;
}

void jsrt_hook_clear_all(JSRTHookRegistry* registry) {
  if (!registry || !registry->initialized) {
    return;
  }

  JSRT_Debug("Clearing all hooks (%d hooks)", registry->hook_count);

  // Free all hooks in the chain
  JSRTModuleHook* current = registry->hooks;
  while (current) {
    JSRTModuleHook* next = current->next;
    jsrt_hook_free(registry->ctx, current);
    current = next;
  }

  registry->hooks = NULL;
  registry->hook_count = 0;
}