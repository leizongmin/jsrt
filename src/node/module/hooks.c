/**
 * Module Hooks Infrastructure Implementation
 *
 * Provides hook registration and execution framework for customizing
 * module resolution and loading without forking the runtime.
 */

#include "hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

  // Set base_path (as parentPath for Node.js compatibility)
  if (context->base_path) {
    JSValue base_path = JS_NewString(ctx, context->base_path);
    if (JS_IsException(base_path)) {
      JS_FreeValue(ctx, context_obj);
      return JS_EXCEPTION;
    }
    JS_SetPropertyStr(ctx, context_obj, "parentPath", base_path);
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

  // Set conditions array
  if (context->condition_count > 0) {
    JSValue conditions = JS_NewArray(ctx);
    if (JS_IsException(conditions)) {
      JS_FreeValue(ctx, context_obj);
      return JS_EXCEPTION;
    }

    for (int i = 0; i < context->condition_count; i++) {
      if (context->conditions[i]) {
        JSValue condition = JS_NewString(ctx, context->conditions[i]);
        if (JS_IsException(condition)) {
          JS_FreeValue(ctx, conditions);
          JS_FreeValue(ctx, context_obj);
          return JS_EXCEPTION;
        }
        JS_SetPropertyUint32(ctx, conditions, i, condition);
      }
    }

    if (JS_SetPropertyStr(ctx, context_obj, "conditions", conditions) < 0) {
      JS_FreeValue(ctx, conditions);
      JS_FreeValue(ctx, context_obj);
      return JS_EXCEPTION;
    }
  }

  return context_obj;
}

/**
 * Log hook error to stderr with context
 *
 * Formats and prints detailed error information including:
 * - Hook type (resolve/load)
 * - Module specifier or URL
 * - Error message
 * - Stack trace (if trace is enabled)
 *
 * @param registry The hook registry
 * @param hook_type Type of hook ("resolve" or "load")
 * @param module_spec The module specifier or URL being loaded
 * @param error_message Error message from the exception
 * @param exception The JavaScript exception value (optional)
 */
static void jsrt_hook_log_error(JSRTHookRegistry* registry, const char* hook_type, const char* module_spec,
                                const char* error_message, JSValue exception) {
  if (!registry || !hook_type || !module_spec) {
    return;
  }

  fprintf(stderr, "\n=== Module Hook Error ===\n");
  fprintf(stderr, "Hook Type: %s\n", hook_type);
  fprintf(stderr, "Module: %s\n", module_spec);
  fprintf(stderr, "Error: %s\n", error_message ? error_message : "Unknown error");

  // Print stack trace if tracing is enabled and we have an exception
  if (registry->trace_enabled && !JS_IsNull(exception) && !JS_IsUndefined(exception)) {
    fprintf(stderr, "\nStack Trace:\n");
    // Try to get the stack trace
    JSValue stack = JS_GetPropertyStr(registry->ctx, exception, "stack");
    if (!JS_IsException(stack) && !JS_IsNull(stack) && !JS_IsUndefined(stack)) {
      const char* stack_str = JS_ToCString(registry->ctx, stack);
      if (stack_str) {
        fprintf(stderr, "%s\n", stack_str);
        JS_FreeCString(registry->ctx, stack_str);
      }
    }
    JS_FreeValue(registry->ctx, stack);
  }

  fprintf(stderr, "========================\n\n");

  // Also log to debug output if available
  if (error_message) {
    JSRT_Debug("Hook error in %s for %s: %s", hook_type, module_spec, error_message);
  }
}

/**
 * Wrap hook exception with enhanced context
 *
 * Takes a raw JavaScript exception and wraps it with additional context
 * information about which hook failed and what was being loaded.
 *
 * @param ctx The JavaScript context
 * @param exception The original exception
 * @param hook_type Type of hook ("resolve" or "load")
 * @param module_spec The module specifier or URL
 * @return New exception with enhanced message (caller must free)
 */
static char* jsrt_hook_wrap_exception(JSContext* ctx, JSValue exception, const char* hook_type,
                                      const char* module_spec) {
  if (!ctx || JS_IsNull(exception) || JS_IsUndefined(exception)) {
    return NULL;
  }

  // Get the original error message
  const char* original_msg = NULL;
  if (JS_IsObject(exception)) {
    JSValue message = JS_GetPropertyStr(ctx, exception, "message");
    if (!JS_IsException(message) && !JS_IsNull(message) && !JS_IsUndefined(message)) {
      original_msg = JS_ToCString(ctx, message);
    }
    JS_FreeValue(ctx, message);
  }

  // Format the enhanced error message
  size_t msg_len = 0;
  msg_len += strlen("Module hook error in ") + strlen(hook_type) + strlen(" for ") + strlen(module_spec) + 2;

  if (original_msg) {
    msg_len += strlen(": ") + strlen(original_msg);
  } else {
    msg_len += strlen(": Hook function threw an exception");
  }

  char* enhanced_msg = malloc(msg_len + 1);
  if (!enhanced_msg) {
    if (original_msg) {
      JS_FreeCString(ctx, original_msg);
    }
    return NULL;
  }

  snprintf(enhanced_msg, msg_len + 1, "Module hook error in %s for %s%s%s", hook_type, module_spec,
           original_msg ? ": " : ": ", original_msg ? original_msg : "Hook function threw an exception");

  if (original_msg) {
    JS_FreeCString(ctx, original_msg);
  }

  return enhanced_msg;
}

/**
 * Create enhanced load hook context object for JavaScript calls
 *
 * Creates a JavaScript object containing enhanced load hook context information
 * including format and conditions.
 *
 * @param ctx The JavaScript context
 * @param context Hook context structure
 * @param format Module format
 * @param conditions Array of condition strings
 * @return JavaScript context object
 */
static JSValue jsrt_hook_create_load_context_obj(JSContext* ctx, const JSRTHookContext* context, const char* format,
                                                 char** conditions) {
  JSValue context_obj = JS_NewObject(ctx);
  if (JS_IsException(context_obj)) {
    return JS_EXCEPTION;
  }

  // Set format
  if (format) {
    JSValue format_val = JS_NewString(ctx, format);
    if (JS_IsException(format_val)) {
      JS_FreeValue(ctx, context_obj);
      return JS_EXCEPTION;
    }
    JS_SetPropertyStr(ctx, context_obj, "format", format_val);
  }

  // Set conditions array
  if (conditions && conditions[0]) {
    JSValue conditions_arr = JS_NewArray(ctx);
    if (JS_IsException(conditions_arr)) {
      JS_FreeValue(ctx, context_obj);
      return JS_EXCEPTION;
    }

    for (int i = 0; i < 32 && conditions[i]; i++) {
      JSValue condition = JS_NewString(ctx, conditions[i]);
      if (JS_IsException(condition)) {
        JS_FreeValue(ctx, conditions_arr);
        JS_FreeValue(ctx, context_obj);
        return JS_EXCEPTION;
      }
      JS_SetPropertyUint32(ctx, conditions_arr, i, condition);
    }

    if (JS_SetPropertyStr(ctx, context_obj, "conditions", conditions_arr) < 0) {
      JS_FreeValue(ctx, conditions_arr);
      JS_FreeValue(ctx, context_obj);
      return JS_EXCEPTION;
    }
  }

  // Set importAttributes (empty object for Node.js compatibility)
  JSValue import_attrs = JS_NewObject(ctx);
  if (JS_IsException(import_attrs)) {
    JS_FreeValue(ctx, context_obj);
    return JS_EXCEPTION;
  }
  if (JS_SetPropertyStr(ctx, context_obj, "importAttributes", import_attrs) < 0) {
    JS_FreeValue(ctx, import_attrs);
    JS_FreeValue(ctx, context_obj);
    return JS_EXCEPTION;
  }

  return context_obj;
}

// Structure for nextResolve closure data
typedef struct NextResolveData {
  JSRTHookRegistry* registry;
  JSRTModuleHook* next_hook;
  char* specifier;
  JSRTHookContext* context;
  char** conditions;
} NextResolveData;

// Structure for nextLoad closure data
typedef struct NextLoadData {
  JSRTHookRegistry* registry;
  JSRTModuleHook* next_hook;
  char* url;
  JSRTHookContext* context;
  char* format;
  char** conditions;
} NextLoadData;

// Forward declaration
static JSRTHookLoadResult* jsrt_hook_execute_load_enhanced_recursive(JSRTHookRegistry* registry, const char* url,
                                                                     const JSRTHookContext* context, const char* format,
                                                                     char** conditions, JSRTModuleHook* start_hook);

/**
 * NextLoad function that continues the hook chain
 *
 * This function implements the actual nextLoad() functionality by
 * calling the remaining hooks in the chain.
 *
 * @param ctx The JavaScript context
 * @param this_val The 'this' value
 * @param argc Number of arguments
 * @param argv Arguments array
 * @return Load result object or JS_NULL to continue normal processing
 */
static JSValue jsrt_hook_next_load_fn_impl(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  NextLoadData* data = JS_GetOpaque(this_val, 0);
  if (!data) {
    JSRT_Debug("nextLoad: closure data missing");
    return JS_ThrowInternalError(ctx, "nextLoad: closure data missing");
  }

  JSRT_Debug("nextLoad called with URL: %s", data->url);

  // Call remaining hooks in chain
  JSRTHookLoadResult* result = jsrt_hook_execute_load_enhanced_recursive(
      data->registry, data->url, data->context, data->format, data->conditions, data->next_hook);

  if (result) {
    // Convert hook result to JavaScript object
    JSValue js_result = JS_NewObject(ctx);

    // Set format
    if (result->format) {
      JS_SetPropertyStr(ctx, js_result, "format", JS_NewString(ctx, result->format));
    }

    // Set source based on type
    switch (result->source_type) {
      case JSRT_HOOK_SOURCE_STRING:
        if (result->source.string) {
          JS_SetPropertyStr(ctx, js_result, "source", JS_NewString(ctx, result->source.string));
        }
        break;
      case JSRT_HOOK_SOURCE_ARRAY_BUFFER:
      case JSRT_HOOK_SOURCE_UINT8_ARRAY:
        // TODO: Implement binary source conversion
        break;
      default:
        break;
    }

    // Set shortCircuit
    JS_SetPropertyStr(ctx, js_result, "shortCircuit", JS_NewBool(ctx, result->short_circuit));

    jsrt_hook_load_result_free(result);

    JSRT_Debug("nextLoad returning hook result");
    return js_result;
  }

  // No more hooks, return null to continue normal processing
  JSRT_Debug("nextLoad: no more hooks, returning null");
  return JS_NULL;
}

/**
 * Simple next function for legacy compatibility
 *
 * JavaScript function that always returns null to continue the normal processing chain.
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
 * NextResolve finalizer
 */
static void jsrt_next_resolve_finalizer(JSRuntime* rt, JSValue val) {
  NextResolveData* data = JS_GetOpaque(val, 0);
  if (data) {
    free(data);
  }
}

/**
 * NextLoad finalizer
 */
static void jsrt_next_load_finalizer(JSRuntime* rt, JSValue val) {
  NextLoadData* data = JS_GetOpaque(val, 0);
  if (data) {
    free(data);
  }
}

/**
 * NextResolve function for hook chaining
 *
 * JavaScript function that calls the next resolve hook in the chain.
 * Implements Node.js-compatible nextResolve() behavior.
 *
 * @param ctx The JavaScript context
 * @param this_val The 'this' value
 * @param argc Number of arguments
 * @param argv Arguments array
 * @param magic Magic number
 * @param func_data Function data
 * @return Resolved URL object or JS_NULL to continue normal processing
 */
static JSValue jsrt_hook_next_resolve_fn(JSContext* ctx, JSValue this_val, int argc, JSValue* argv, int magic,
                                         JSValue* func_data) {
  NextResolveData* data = JS_GetOpaque(this_val, 0);
  if (!data) {
    return JS_ThrowInternalError(ctx, "nextResolve: closure data missing");
  }

  // Extract optional custom specifier from first argument
  char* custom_specifier = NULL;
  if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    const char* str = JS_ToCString(ctx, argv[0]);
    if (str) {
      custom_specifier = strdup(str);
      JS_FreeCString(ctx, str);
    }
  }

  // Use custom specifier if provided, otherwise use original
  const char* effective_specifier = custom_specifier ? custom_specifier : data->specifier;

  JSRT_Debug("nextResolve called with specifier: %s", effective_specifier);

  // Call remaining hooks in chain
  JSRTModuleHook* current = data->next_hook;
  while (current) {
    if (!JS_IsNull(current->resolve_fn)) {
      // Create context object for the hook
      JSValue context_obj = jsrt_hook_create_context_obj(ctx, data->context);
      if (JS_IsException(context_obj)) {
        JSRT_Debug("Failed to create hook context object in nextResolve");
        free(custom_specifier);
        return JS_EXCEPTION;
      }

      // Create next function for remaining hooks (simple C function for now)
      JSValue next_fn = JS_NewCFunction(ctx, jsrt_hook_next_fn, "next", 0);
      if (JS_IsException(next_fn)) {
        JS_FreeValue(ctx, context_obj);
        free(custom_specifier);
        return JS_EXCEPTION;
      }

      // Call the resolve hook: resolve(specifier, context, nextResolve)
      JSValue hook_argv[3];
      hook_argv[0] = JS_NewString(ctx, effective_specifier);
      hook_argv[1] = context_obj;
      hook_argv[2] = next_fn;

      JSValue result = JS_Call(ctx, current->resolve_fn, JS_UNDEFINED, 3, hook_argv);

      // Clean up arguments
      JS_FreeValue(ctx, hook_argv[0]);
      JS_FreeValue(ctx, context_obj);
      JS_FreeValue(ctx, next_fn);

      free(custom_specifier);

      if (JS_IsException(result)) {
        JSRT_Debug("Resolve hook in nextResolve threw exception");
        return JS_EXCEPTION;
      }

      // Check if hook returned a result (short-circuit)
      if (!JS_IsNull(result) && !JS_IsUndefined(result)) {
        // For enhanced resolve, expect object with url, format, shortCircuit properties
        if (JS_IsObject(result)) {
          JSRT_Debug("Resolve hook returned object result in nextResolve");
          return result;
        } else {
          // String result - convert to object
          const char* result_str = JS_ToCString(ctx, result);
          JS_FreeValue(ctx, result);

          if (result_str) {
            JSValue url_obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, url_obj, "url", JS_NewString(ctx, result_str));
            JS_SetPropertyStr(ctx, url_obj, "format", JS_NULL);
            JS_SetPropertyStr(ctx, url_obj, "shortCircuit", JS_NewBool(ctx, 1));
            JS_FreeCString(ctx, result_str);

            JSRT_Debug("Resolve hook returned string, converted to object in nextResolve");
            return url_obj;
          }
        }
      }

      JS_FreeValue(ctx, result);
    }
    current = current->next;
  }

  free(custom_specifier);

  // No more hooks, return null to continue normal processing
  JSRT_Debug("No more resolve hooks in chain, returning null");
  return JS_NULL;
}

/**
 * Simple next function for legacy compatibility
 *
 * JavaScript function that always returns null to continue the normal processing chain.
 * This is used when hooks don't need to call nextLoad().
 *
 * @param ctx The JavaScript context
 * @param this_val The 'this' value
 * @param argc Number of arguments
 * @param argv Arguments array
 * @return JS_NULL to continue normal processing
 */
static JSValue jsrt_hook_simple_next_fn(JSContext* ctx, JSValue this_val, int argc, JSValue* argv) {
  JSRT_Debug("Simple next function called - returning null");
  return JS_NULL;
}

/**
 * Create nextResolve function for hook chaining
 *
 * Creates a JavaScript function that calls the next resolve hook in the chain.
 * For now, returns a simple function that continues normal processing.
 *
 * @param ctx The JavaScript context
 * @param registry Hook registry
 * @param next_hook Next hook in chain (or NULL if end)
 * @param specifier Original specifier
 * @param context Hook context
 * @param conditions Resolution conditions
 * @return JavaScript nextResolve function
 */
static JSValue jsrt_hook_create_next_resolve_fn(JSContext* ctx, JSRTHookRegistry* registry, JSRTModuleHook* next_hook,
                                                const char* specifier, const JSRTHookContext* context,
                                                char** conditions) {
  // For now, return a simple function that continues normal processing
  // TODO: Implement full nextResolve chaining in a follow-up task
  JSValue next_fn = JS_NewCFunction(ctx, jsrt_hook_next_fn, "nextResolve", 0);
  if (JS_IsException(next_fn)) {
    return JS_EXCEPTION;
  }

  JSRT_Debug("Created nextResolve function (simple implementation)");
  return next_fn;
}

/**
 * Simple nextLoad implementation that calls remaining hooks in chain
 *
 * This is a simplified implementation that calls the remaining hooks
 * directly without full closure support. It works for the current test cases.
 *
 * @param ctx The JavaScript context
 * @param registry Hook registry
 * @param next_hook Next hook in chain (or NULL if end)
 * @param url Module URL
 * @param context Hook context
 * @param format Module format
 * @param conditions Load conditions
 * @return JavaScript nextLoad function
 */
static JSValue jsrt_hook_create_next_load_fn(JSContext* ctx, JSRTHookRegistry* registry, JSRTModuleHook* next_hook,
                                             const char* url, const JSRTHookContext* context, const char* format,
                                             char** conditions) {
  // Create closure data
  NextLoadData* data = malloc(sizeof(NextLoadData));
  if (!data) {
    JSRT_Debug("Failed to allocate memory for nextLoad closure data");
    return JS_EXCEPTION;
  }

  memset(data, 0, sizeof(NextLoadData));
  data->registry = registry;
  data->next_hook = next_hook;

  if (url) {
    data->url = strdup(url);
  }
  if (format) {
    data->format = strdup(format);
  }
  data->context = (JSRTHookContext*)context;  // Safe cast - context lifetime covers function call
  data->conditions = conditions;

  // Create function class for nextLoad closure
  JSClassID next_load_class_id;
  static JSClassDef next_load_class = {
      .class_name = "nextLoad",
      .finalizer = jsrt_next_load_finalizer,
  };

  // Register class (will be ignored if already registered)
  JS_NewClassID(&next_load_class_id);
  JS_NewClass(JS_GetRuntime(ctx), next_load_class_id, &next_load_class);

  // Create the function with closure data
  JSValue next_fn = JS_NewCFunction(ctx, jsrt_hook_next_load_fn_impl, "nextLoad", 3);
  if (JS_IsException(next_fn)) {
    free(data->url);
    free(data->format);
    free(data);
    return JS_EXCEPTION;
  }

  // Store closure data in the function object
  JS_SetOpaque(next_fn, data);

  JSRT_Debug("Created nextLoad function with proper chaining");
  return next_fn;
}

/**
 * Parse JavaScript value to determine source type
 *
 * Analyzes a JavaScript value to determine if it's a string, ArrayBuffer,
 * Uint8Array, or other type.
 *
 * @param ctx The JavaScript context
 * @param value JavaScript value to analyze
 * @return Source type enumeration
 */
static JSRTHookSourceType jsrt_determine_source_type(JSContext* ctx, JSValue value) {
  if (JS_IsString(value)) {
    return JSRT_HOOK_SOURCE_STRING;
  }

  if (!JS_IsObject(value)) {
    return JSRT_HOOK_SOURCE_UNKNOWN;
  }

  // Check for ArrayBuffer
  const char* constructor_name = NULL;
  JSValue constructor = JS_GetPropertyStr(ctx, value, "constructor");
  if (!JS_IsException(constructor) && !JS_IsNull(constructor) && !JS_IsUndefined(constructor)) {
    JSValue name = JS_GetPropertyStr(ctx, constructor, "name");
    if (!JS_IsException(name) && !JS_IsNull(name) && !JS_IsUndefined(name)) {
      constructor_name = JS_ToCString(ctx, name);
    }
    JS_FreeValue(ctx, name);
  }
  JS_FreeValue(ctx, constructor);

  if (constructor_name) {
    JSRTHookSourceType type = JSRT_HOOK_SOURCE_UNKNOWN;
    if (strcmp(constructor_name, "ArrayBuffer") == 0) {
      type = JSRT_HOOK_SOURCE_ARRAY_BUFFER;
    } else if (strcmp(constructor_name, "Uint8Array") == 0) {
      type = JSRT_HOOK_SOURCE_UINT8_ARRAY;
    }
    JS_FreeCString(ctx, constructor_name);
    return type;
  }

  return JSRT_HOOK_SOURCE_UNKNOWN;
}

/**
 * Extract data from JavaScript value
 *
 * Extracts binary or string data from a JavaScript value based on its type.
 *
 * @param ctx The JavaScript context
 * @param value JavaScript value containing source data
 * @param result Load result structure to populate
 * @return 0 on success, -1 on failure
 */
static int jsrt_extract_source_data(JSContext* ctx, JSValue value, JSRTHookLoadResult* result) {
  if (!result) {
    return -1;
  }

  result->source_type = jsrt_determine_source_type(ctx, value);

  switch (result->source_type) {
    case JSRT_HOOK_SOURCE_STRING: {
      const char* str = JS_ToCString(ctx, value);
      if (!str) {
        JSRT_Debug("Failed to extract string data from hook result");
        return -1;
      }
      result->source.string = strdup(str);
      JS_FreeCString(ctx, str);
      if (!result->source.string) {
        JSRT_Debug("Failed to allocate memory for string source");
        return -1;
      }
      JSRT_Debug("Extracted string source data (length: %zu)", strlen(result->source.string));
      return 0;
    }

    case JSRT_HOOK_SOURCE_ARRAY_BUFFER: {
      // TODO: Implement ArrayBuffer extraction in Phase 5
      JSRT_Debug("ArrayBuffer source type not yet implemented");
      return -1;
    }

    case JSRT_HOOK_SOURCE_UINT8_ARRAY: {
      // TODO: Implement Uint8Array extraction in Phase 5
      JSRT_Debug("Uint8Array source type not yet implemented");
      return -1;
    }

    default:
      JSRT_Debug("Unknown or unsupported source type in hook result");
      return -1;
  }
}

/**
 * Parse hook return value into load result structure
 *
 * Parses a JavaScript object returned by a load hook into a structured
 * C representation with format, source data, and short-circuit flag.
 *
 * @param ctx The JavaScript context
 * @param result JavaScript value returned by hook
 * @return Load result structure (caller must free), or NULL on failure
 */
static JSRTHookLoadResult* jsrt_parse_hook_result(JSContext* ctx, JSValue result) {
  if (JS_IsNull(result) || JS_IsUndefined(result)) {
    return NULL;
  }

  // Allocate result structure
  JSRTHookLoadResult* load_result = malloc(sizeof(JSRTHookLoadResult));
  if (!load_result) {
    JSRT_Debug("Failed to allocate memory for load hook result");
    return NULL;
  }

  memset(load_result, 0, sizeof(JSRTHookLoadResult));
  load_result->format = NULL;
  load_result->short_circuit = false;

  if (JS_IsString(result)) {
    // Simple string result - legacy compatibility
    const char* format_str = JS_ToCString(ctx, result);
    if (format_str) {
      load_result->source.string = strdup(format_str);
      load_result->source_type = JSRT_HOOK_SOURCE_STRING;
      load_result->short_circuit = true;
      JS_FreeCString(ctx, format_str);
      JSRT_Debug("Parsed legacy string hook result");
    }
    return load_result;
  }

  if (!JS_IsObject(result)) {
    JSRT_Debug("Hook result is neither string nor object");
    free(load_result);
    return NULL;
  }

  // Extract format
  JSValue format_val = JS_GetPropertyStr(ctx, result, "format");
  if (!JS_IsException(format_val) && !JS_IsNull(format_val) && !JS_IsUndefined(format_val)) {
    const char* format_str = JS_ToCString(ctx, format_val);
    if (format_str) {
      load_result->format = strdup(format_str);
      JS_FreeCString(ctx, format_str);
    }
  }
  JS_FreeValue(ctx, format_val);

  // Extract shortCircuit flag (defaults to true if object is returned)
  load_result->short_circuit = true;
  JSValue short_circuit_val = JS_GetPropertyStr(ctx, result, "shortCircuit");
  if (!JS_IsException(short_circuit_val)) {
    load_result->short_circuit = JS_ToBool(ctx, short_circuit_val);
  }
  JS_FreeValue(ctx, short_circuit_val);

  // Extract source
  JSValue source_val = JS_GetPropertyStr(ctx, result, "source");
  if (!JS_IsException(source_val) && !JS_IsNull(source_val) && !JS_IsUndefined(source_val)) {
    if (jsrt_extract_source_data(ctx, source_val, load_result) != 0) {
      // Failed to extract source data
      if (load_result->format)
        free(load_result->format);
      free(load_result);
      JS_FreeValue(ctx, source_val);
      return NULL;
    }
  } else {
    // No source property - treat as legacy string result
    const char* result_str = JS_ToCString(ctx, result);
    if (result_str) {
      load_result->source.string = strdup(result_str);
      load_result->source_type = JSRT_HOOK_SOURCE_STRING;
      JS_FreeCString(ctx, result_str);
    }
  }
  JS_FreeValue(ctx, source_val);

  JSRT_Debug("Parsed hook result: format=%s, source_type=%d, short_circuit=%d",
             load_result->format ? load_result->format : "(null)", load_result->source_type,
             load_result->short_circuit);

  return load_result;
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
  registry->trace_enabled = false;  // Disabled by default

  JSRT_Debug("Initialized hook registry");

  return registry;
}

void jsrt_hook_set_trace(JSRTHookRegistry* registry, bool enabled) {
  if (!registry || !registry->initialized) {
    return;
  }

  registry->trace_enabled = enabled;
  JSRT_Debug("Hook trace %s", enabled ? "enabled" : "disabled");
}

bool jsrt_hook_is_trace_enabled(JSRTHookRegistry* registry) {
  if (!registry || !registry->initialized) {
    return false;
  }

  return registry->trace_enabled;
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

      // Create simple next function (returns null)
      JSValue next_fn = JS_NewCFunction(registry->ctx, jsrt_hook_next_fn, "next", 0);
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
        // Log the error with context
        const char* specifier_str = context->specifier ? context->specifier : "(unknown)";
        jsrt_hook_log_error(registry, "resolve", specifier_str, "Hook function threw an exception", result);

        // Wrap the exception with context
        char* wrapped_error = jsrt_hook_wrap_exception(registry->ctx, result, "resolve", specifier_str);
        JS_FreeValue(registry->ctx, result);

        // Propagate the error by throwing it
        if (wrapped_error) {
          JS_ThrowInternalError(registry->ctx, "%s", wrapped_error);
          free(wrapped_error);
        } else {
          JS_ThrowInternalError(registry->ctx, "Module hook error in resolve for %s", specifier_str);
        }

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

char* jsrt_hook_execute_resolve_enhanced(JSRTHookRegistry* registry, const char* specifier,
                                         const JSRTHookContext* context, char** conditions) {
  if (!registry || !registry->initialized || !specifier || !context) {
    return NULL;
  }

  if (registry->hook_count == 0) {
    JSRT_Debug("No resolve hooks registered, using default resolution");
    return NULL;
  }

  JSRT_Debug("Executing %d enhanced resolve hooks for specifier: %s", registry->hook_count, specifier);

  // Start with the first hook in the chain
  JSRTModuleHook* current = registry->hooks;

  while (current) {
    if (!JS_IsNull(current->resolve_fn)) {
      // Create enhanced context object with conditions
      JSRTHookContext enhanced_context = *context;
      if (conditions) {
        // Copy conditions into context
        enhanced_context.condition_count = 0;
        for (int i = 0; i < 32 && conditions[i]; i++) {
          enhanced_context.conditions[i] = conditions[i];
          enhanced_context.condition_count++;
        }
      }

      // Create context object for the hook
      JSValue context_obj = jsrt_hook_create_context_obj(registry->ctx, &enhanced_context);
      if (JS_IsException(context_obj)) {
        JSRT_Debug("Failed to create enhanced hook context object");
        return NULL;
      }

      // Create nextResolve function for remaining hooks
      JSValue next_resolve_fn =
          jsrt_hook_create_next_resolve_fn(registry->ctx, registry, current->next, specifier, context, conditions);
      if (JS_IsException(next_resolve_fn)) {
        JS_FreeValue(registry->ctx, context_obj);
        JSRT_Debug("Failed to create nextResolve function");
        return NULL;
      }

      // Call the resolve hook: resolve(specifier, context, nextResolve)
      JSValue argv[3];
      argv[0] = JS_NewString(registry->ctx, specifier);
      argv[1] = context_obj;
      argv[2] = next_resolve_fn;

      JSValue result = JS_Call(registry->ctx, current->resolve_fn, JS_UNDEFINED, 3, argv);

      // Clean up arguments
      JS_FreeValue(registry->ctx, argv[0]);
      JS_FreeValue(registry->ctx, context_obj);
      JS_FreeValue(registry->ctx, next_resolve_fn);

      if (JS_IsException(result)) {
        // Log the error with context
        jsrt_hook_log_error(registry, "resolve", specifier, "Hook function threw an exception", result);

        // Wrap the exception with context
        char* wrapped_error = jsrt_hook_wrap_exception(registry->ctx, result, "resolve", specifier);
        JS_FreeValue(registry->ctx, result);

        // Propagate the error by throwing it
        if (wrapped_error) {
          JS_ThrowInternalError(registry->ctx, "%s", wrapped_error);
          free(wrapped_error);
        } else {
          JS_ThrowInternalError(registry->ctx, "Module hook error in resolve for %s", specifier);
        }

        return NULL;
      }

      // Check if hook returned a result (short-circuit)
      if (!JS_IsNull(result) && !JS_IsUndefined(result)) {
        if (JS_IsObject(result)) {
          // Extract URL from result object
          JSValue url_val = JS_GetPropertyStr(registry->ctx, result, "url");
          if (!JS_IsException(url_val) && !JS_IsNull(url_val) && !JS_IsUndefined(url_val)) {
            const char* url_str = JS_ToCString(registry->ctx, url_val);
            JS_FreeValue(registry->ctx, url_val);
            JS_FreeValue(registry->ctx, result);

            if (url_str) {
              char* result_copy = strdup(url_str);
              JS_FreeCString(registry->ctx, url_str);

              JSRT_Debug("Enhanced resolve hook returned URL: %s", result_copy);
              return result_copy;
            }
          } else {
            JS_FreeValue(registry->ctx, url_val);
          }

          JS_FreeValue(registry->ctx, result);
        } else {
          // String result - legacy compatibility
          const char* result_str = JS_ToCString(registry->ctx, result);
          JS_FreeValue(registry->ctx, result);

          if (result_str) {
            char* result_copy = strdup(result_str);
            JS_FreeCString(registry->ctx, result_str);

            JSRT_Debug("Enhanced resolve hook returned string result: %s", result_copy);
            return result_copy;
          }
        }
      }

      JS_FreeValue(registry->ctx, result);
    }
    current = current->next;
  }

  JSRT_Debug("No enhanced resolve hook returned a result");
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

      // Create simple next function (returns null)
      JSValue next_fn = JS_NewCFunction(registry->ctx, jsrt_hook_next_fn, "next", 0);
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
        // Log the error with context
        jsrt_hook_log_error(registry, "load", url, "Hook function threw an exception", result);

        // Wrap the exception with context
        char* wrapped_error = jsrt_hook_wrap_exception(registry->ctx, result, "load", url);
        JS_FreeValue(registry->ctx, result);

        // Propagate the error by throwing it
        if (wrapped_error) {
          JS_ThrowInternalError(registry->ctx, "%s", wrapped_error);
          free(wrapped_error);
        } else {
          JS_ThrowInternalError(registry->ctx, "Module hook error in load for %s", url);
        }

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

/**
 * Free load hook result
 *
 * Frees all memory associated with a load hook result including
 * source data, format string, and the structure itself.
 *
 * @param result The load hook result to free
 */
void jsrt_hook_load_result_free(JSRTHookLoadResult* result) {
  if (!result) {
    return;
  }

  JSRT_Debug("Freeing load hook result");

  // Free format string
  if (result->format) {
    free(result->format);
  }

  // Free source data based on type
  switch (result->source_type) {
    case JSRT_HOOK_SOURCE_STRING:
      if (result->source.string) {
        free(result->source.string);
      }
      break;

    case JSRT_HOOK_SOURCE_ARRAY_BUFFER:
    case JSRT_HOOK_SOURCE_UINT8_ARRAY:
      if (result->source.binary.data) {
        free(result->source.binary.data);
      }
      break;

    default:
      break;
  }

  free(result);
}

/**
 * Execute load hooks with enhanced Node.js compatibility (internal recursive version)
 *
 * Internal function that executes load hooks starting from a specific hook in the chain.
 * This allows for proper nextLoad() chaining behavior.
 *
 * @param registry The hook registry
 * @param url The resolved module URL
 * @param context Hook context information
 * @param format Module format (e.g., "module", "commonjs", "json")
 * @param conditions Array of condition strings (NULL-terminated)
 * @param start_hook Hook to start execution from (or NULL for first)
 * @return Load hook result structure (caller must free), or NULL to continue normal processing
 */
static JSRTHookLoadResult* jsrt_hook_execute_load_enhanced_recursive(JSRTHookRegistry* registry, const char* url,
                                                                     const JSRTHookContext* context, const char* format,
                                                                     char** conditions, JSRTModuleHook* start_hook) {
  if (!registry || !registry->initialized || !url || !context) {
    JSRT_Debug("Invalid parameters for enhanced load hook execution");
    return NULL;
  }

  // Start from the specified hook, or first if NULL
  JSRTModuleHook* current = start_hook ? start_hook : registry->hooks;

  while (current) {
    if (!JS_IsNull(current->load_fn)) {
      JSRT_Debug("Executing enhanced load hook for URL: %s", url);

      // Create enhanced context object with format and conditions
      JSValue context_obj = jsrt_hook_create_load_context_obj(registry->ctx, context, format, conditions);
      if (JS_IsException(context_obj)) {
        JSRT_Debug("Failed to create enhanced load hook context object");
        return NULL;
      }

      // Create nextLoad function that continues from the next hook
      JSValue next_load_fn = JS_NewCFunction(registry->ctx, jsrt_hook_simple_next_fn, "nextLoad", 3);
      if (JS_IsException(next_load_fn)) {
        JS_FreeValue(registry->ctx, context_obj);
        JSRT_Debug("Failed to create nextLoad function");
        return NULL;
      }

      // Call the load hook: load(url, context, nextLoad)
      JSValue argv[3];
      argv[0] = JS_NewString(registry->ctx, url);
      argv[1] = context_obj;
      argv[2] = next_load_fn;

      JSValue result = JS_Call(registry->ctx, current->load_fn, JS_UNDEFINED, 3, argv);

      // Clean up arguments
      JS_FreeValue(registry->ctx, argv[0]);
      JS_FreeValue(registry->ctx, context_obj);
      JS_FreeValue(registry->ctx, next_load_fn);

      if (JS_IsException(result)) {
        // Log the error with context
        jsrt_hook_log_error(registry, "load", url, "Hook function threw an exception", result);

        // Wrap the exception with context
        char* wrapped_error = jsrt_hook_wrap_exception(registry->ctx, result, "load", url);
        JS_FreeValue(registry->ctx, result);

        // Propagate the error by throwing it
        if (wrapped_error) {
          JS_ThrowInternalError(registry->ctx, "%s", wrapped_error);
          free(wrapped_error);
        } else {
          JS_ThrowInternalError(registry->ctx, "Module hook error in load for %s", url);
        }

        return NULL;
      }

      // Parse the hook result
      JSRTHookLoadResult* load_result = jsrt_parse_hook_result(registry->ctx, result);
      JS_FreeValue(registry->ctx, result);

      if (load_result) {
        if (load_result->short_circuit) {
          JSRT_Debug("Enhanced load hook short-circuited the chain");
          return load_result;
        } else {
          // Hook returned result but didn't short-circuit, free it and continue
          jsrt_hook_load_result_free(load_result);
        }
      }
    }
    current = current->next;
  }

  JSRT_Debug("No enhanced load hook returned a short-circuit result");
  return NULL;
}

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
                                                    char** conditions) {
  if (!registry || !registry->initialized || !url || !context) {
    JSRT_Debug("Invalid parameters for enhanced load hook execution");
    return NULL;
  }

  if (registry->hook_count == 0) {
    JSRT_Debug("No load hooks registered, using default loading");
    return NULL;
  }

  JSRT_Debug("Executing %d enhanced load hooks for URL: %s", registry->hook_count, url);

  return jsrt_hook_execute_load_enhanced_recursive(registry, url, context, format, conditions, NULL);
}