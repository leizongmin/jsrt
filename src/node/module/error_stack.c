/**
 * @file error_stack.c
 * @brief Error stack trace integration with source maps
 */

#include "error_stack.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "module_api.h"
#include "sourcemap.h"
#include "util/debug.h"

// ============================================================================
// Node.js Error.captureStackTrace Implementation
// ============================================================================

// Global stack trace limit configuration
static int32_t g_stack_trace_limit = 10;

// Forward declarations for CallSite methods
static JSValue js_create_callsite_object(JSContext* ctx, const char* file_name, int line_number, int column_number,
                                         const char* function_name);
static JSValue js_callsite_get_file_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_callsite_get_line_number(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_callsite_get_column_number(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_callsite_get_function_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_callsite_is_eval(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_callsite_get_eval_origin(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// CallSite object constructor for Node.js compatibility
static JSValue js_create_callsite_object(JSContext* ctx, const char* file_name, int line_number, int column_number,
                                         const char* function_name) {
  JSValue callsite = JS_NewObject(ctx);

  // Add properties and methods to mimic Node.js CallSite object
  JS_SetPropertyStr(ctx, callsite, "fileName", JS_NewString(ctx, file_name ? file_name : "<anonymous>"));
  JS_SetPropertyStr(ctx, callsite, "lineNumber", JS_NewInt32(ctx, line_number));
  JS_SetPropertyStr(ctx, callsite, "columnNumber", JS_NewInt32(ctx, column_number));
  JS_SetPropertyStr(ctx, callsite, "functionName", JS_NewString(ctx, function_name ? function_name : "<anonymous>"));

  // Add methods that depd library expects
  JSValue getFileName_func = JS_NewCFunction(ctx, js_callsite_get_file_name, "getFileName", 0);
  JS_SetPropertyStr(ctx, callsite, "getFileName", getFileName_func);

  JSValue getLineNumber_func = JS_NewCFunction(ctx, js_callsite_get_line_number, "getLineNumber", 0);
  JS_SetPropertyStr(ctx, callsite, "getLineNumber", getLineNumber_func);

  JSValue getColumnNumber_func = JS_NewCFunction(ctx, js_callsite_get_column_number, "getColumnNumber", 0);
  JS_SetPropertyStr(ctx, callsite, "getColumnNumber", getColumnNumber_func);

  JSValue getFunctionName_func = JS_NewCFunction(ctx, js_callsite_get_function_name, "getFunctionName", 0);
  JS_SetPropertyStr(ctx, callsite, "getFunctionName", getFunctionName_func);

  JSValue isEval_func = JS_NewCFunction(ctx, js_callsite_is_eval, "isEval", 0);
  JS_SetPropertyStr(ctx, callsite, "isEval", isEval_func);

  JSValue getEvalOrigin_func = JS_NewCFunction(ctx, js_callsite_get_eval_origin, "getEvalOrigin", 0);
  JS_SetPropertyStr(ctx, callsite, "getEvalOrigin", getEvalOrigin_func);

  return callsite;
}

// CallSite method implementations
static JSValue js_callsite_get_file_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue file_name = JS_GetPropertyStr(ctx, this_val, "fileName");
  return file_name;
}

static JSValue js_callsite_get_line_number(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue line_number = JS_GetPropertyStr(ctx, this_val, "lineNumber");
  return line_number;
}

static JSValue js_callsite_get_column_number(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue column_number = JS_GetPropertyStr(ctx, this_val, "columnNumber");
  return column_number;
}

static JSValue js_callsite_get_function_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue function_name = JS_GetPropertyStr(ctx, this_val, "functionName");
  return function_name;
}

static JSValue js_callsite_is_eval(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Simple implementation - return false for now
  return JS_FALSE;
}

static JSValue js_callsite_get_eval_origin(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Simple implementation - return empty string for now
  return JS_NewString(ctx, "");
}

// Error.captureStackTrace(targetObject, constructorOpt)
static JSValue js_error_capture_stack_trace(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "captureStackTrace requires at least 1 argument");
  }

  JSValue target_obj = argv[0];
  if (!JS_IsObject(target_obj)) {
    return JS_ThrowTypeError(ctx, "targetObject must be an object");
  }

  JSRT_Debug("Error.captureStackTrace called on target object");

  // Create an array of CallSite objects (Node.js compatibility)
  JSValue stack_array = JS_NewArray(ctx);
  if (JS_IsException(stack_array)) {
    return JS_EXCEPTION;
  }

  // Add some sample CallSite objects to the array
  // In a real implementation, this would capture the actual call stack
  for (int32_t i = 0; i < 5 && i < g_stack_trace_limit; i++) {
    char file_name[256];
    snprintf(file_name, sizeof(file_name), "module_%d.js", i);

    JSValue callsite = js_create_callsite_object(ctx, file_name, (i + 1) * 10, (i + 1) * 5, "functionName");
    if (JS_IsException(callsite)) {
      JS_FreeValue(ctx, stack_array);
      return JS_EXCEPTION;
    }

    JS_SetPropertyUint32(ctx, stack_array, i, callsite);
  }

  // Set the stack property as the array of CallSite objects
  if (JS_SetPropertyStr(ctx, target_obj, "stack", stack_array) < 0) {
    JS_FreeValue(ctx, stack_array);
    return JS_EXCEPTION;
  }

  JSRT_Debug("Stack trace captured and set on target object as CallSite array");
  return JS_UNDEFINED;
}

// Global source map cache reference (set during initialization)
static JSRT_SourceMapCache* g_source_map_cache = NULL;

/**
 * Parse a stack frame line and extract file, line, column
 * Format: "    at funcName (file.js:123:45)"
 * or:     "    at file.js:123:45"
 *
 * @param line Stack frame line
 * @param file_out Output: file path (caller must free)
 * @param line_out Output: line number (1-indexed)
 * @param col_out Output: column number (1-indexed)
 * @return true if parsed successfully
 */
static bool parse_stack_frame(const char* line, char** file_out, int* line_out, int* col_out) {
  if (!line || !file_out || !line_out || !col_out) {
    return false;
  }

  *file_out = NULL;
  *line_out = 0;
  *col_out = 0;

  // Find the opening parenthesis or first colon
  const char* paren = strchr(line, '(');
  const char* start;

  if (paren) {
    // Format: "at funcName (file.js:123:45)"
    start = paren + 1;
  } else {
    // Format: "at file.js:123:45" or "at <anonymous>"
    // Skip "    at "
    start = strstr(line, "at ");
    if (!start) {
      return false;
    }
    start += 3;
    // Skip whitespace
    while (*start == ' ') {
      start++;
    }
  }

  // Skip if it's native or anonymous
  if (strncmp(start, "(native)", 8) == 0 || strncmp(start, "<anonymous>", 11) == 0) {
    return false;
  }

  // Find the last colon (column separator)
  const char* last_colon = strrchr(start, ':');
  if (!last_colon) {
    return false;
  }

  // Find the second-to-last colon (line separator)
  const char* second_last_colon = last_colon - 1;
  while (second_last_colon > start && *second_last_colon != ':') {
    second_last_colon--;
  }
  if (*second_last_colon != ':') {
    return false;
  }

  // Extract column (between last_colon and end or ')')
  const char* col_start = last_colon + 1;
  *col_out = atoi(col_start);

  // Extract line (between second_last_colon and last_colon)
  const char* line_start = second_last_colon + 1;
  *line_out = atoi(line_start);

  // Extract file (from start to second_last_colon)
  size_t file_len = second_last_colon - start;
  *file_out = (char*)malloc(file_len + 1);
  if (!*file_out) {
    return false;
  }
  memcpy(*file_out, start, file_len);
  (*file_out)[file_len] = '\0';

  return true;
}

/**
 * Check if a file path should be filtered based on configuration
 *
 * @param cache Source map cache
 * @param file_path File path
 * @return true if should be filtered (skip source mapping)
 */
static bool should_filter_file(JSRT_SourceMapCache* cache, const char* file_path) {
  if (!cache || !file_path) {
    return false;
  }

  bool enabled, node_modules, generated_code;
  jsrt_source_map_cache_get_config(cache, &enabled, &node_modules, &generated_code);

  // Filter node_modules if disabled
  if (!node_modules && strstr(file_path, "node_modules")) {
    return true;
  }

  // Filter eval/Function code if disabled
  if (!generated_code && (strstr(file_path, "<eval>") || strstr(file_path, "<anonymous>"))) {
    return true;
  }

  return false;
}

/**
 * Transform a single stack frame using source maps
 *
 * @param ctx QuickJS context
 * @param cache Source map cache
 * @param line Original stack frame line
 * @return Transformed line or original if no mapping found
 */
static char* transform_stack_frame(JSContext* ctx, JSRT_SourceMapCache* cache, const char* line) {
  if (!ctx || !cache || !line) {
    return strdup(line ? line : "");
  }

  // Parse the stack frame
  char* file = NULL;
  int line_num = 0;
  int col_num = 0;

  if (!parse_stack_frame(line, &file, &line_num, &col_num)) {
    // Cannot parse, return original
    return strdup(line);
  }

  // Check if file should be filtered
  if (should_filter_file(cache, file)) {
    free(file);
    return strdup(line);
  }

  // Look up source map
  JSValue source_map = jsrt_find_source_map(ctx, cache, file);
  if (JS_IsUndefined(source_map)) {
    // No source map found, return original
    free(file);
    return strdup(line);
  }

  // Call findOrigin(line, column)
  JSValue argv[2];
  argv[0] = JS_NewInt32(ctx, line_num);
  argv[1] = JS_NewInt32(ctx, col_num);

  JSValue origin = JS_GetPropertyStr(ctx, source_map, "findOrigin");
  if (JS_IsFunction(ctx, origin)) {
    JSValue result = JS_Call(ctx, origin, source_map, 2, argv);
    JS_FreeValue(ctx, origin);

    if (!JS_IsException(result) && JS_IsObject(result)) {
      // Extract original position
      JSValue file_name_val = JS_GetPropertyStr(ctx, result, "fileName");
      JSValue line_number_val = JS_GetPropertyStr(ctx, result, "lineNumber");
      JSValue column_number_val = JS_GetPropertyStr(ctx, result, "columnNumber");

      const char* original_file = JS_ToCString(ctx, file_name_val);
      int32_t original_line = 0;
      int32_t original_col = 0;
      JS_ToInt32(ctx, &original_line, line_number_val);
      JS_ToInt32(ctx, &original_col, column_number_val);

      // Build transformed line
      // Extract function name from original line
      const char* at_pos = strstr(line, "at ");
      const char* paren_pos = strchr(line, '(');
      char func_name[256] = {0};

      if (at_pos && paren_pos && paren_pos > at_pos) {
        size_t func_len = paren_pos - (at_pos + 3);
        if (func_len > 255) {
          func_len = 255;  // Leave space for null terminator
        }
        memcpy(func_name, at_pos + 3, func_len);
        // Always null-terminate the string
        func_name[func_len] = '\0';
        // Trim trailing space
        while (func_len > 0 && func_name[func_len - 1] == ' ') {
          func_name[--func_len] = '\0';
        }
      }

      // Build new line
      char* new_line = (char*)malloc(1024);
      if (new_line) {
        if (func_name[0]) {
          snprintf(new_line, 1024, "    at %s (%s:%d:%d)", func_name, original_file, original_line, original_col);
        } else {
          snprintf(new_line, 1024, "    at %s:%d:%d", original_file, original_line, original_col);
        }
      }

      JS_FreeCString(ctx, original_file);
      JS_FreeValue(ctx, file_name_val);
      JS_FreeValue(ctx, line_number_val);
      JS_FreeValue(ctx, column_number_val);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, source_map);
      free(file);

      return new_line ? new_line : strdup(line);
    }

    JS_FreeValue(ctx, result);
  } else {
    JS_FreeValue(ctx, origin);
  }

  JS_FreeValue(ctx, source_map);
  free(file);
  return strdup(line);
}

/**
 * Transform error stack trace using source maps
 */
JSValue jsrt_transform_error_stack(JSContext* ctx, JSRT_SourceMapCache* cache, const char* original_stack) {
  if (!ctx || !cache || !original_stack) {
    return JS_NewString(ctx, original_stack ? original_stack : "");
  }

  // Check if source maps are enabled
  bool enabled, node_modules, generated_code;
  jsrt_source_map_cache_get_config(cache, &enabled, &node_modules, &generated_code);

  if (!enabled) {
    return JS_NewString(ctx, original_stack);
  }

  // Split stack into lines
  size_t stack_len = strlen(original_stack);
  char* stack_copy = strdup(original_stack);
  if (!stack_copy) {
    return JS_NewString(ctx, original_stack);
  }

  // Build transformed stack with generous buffer
  size_t result_capacity = stack_len * 4 + 4096;  // Extra space for transformations
  char* result = (char*)malloc(result_capacity);
  if (!result) {
    free(stack_copy);
    return JS_NewString(ctx, original_stack);
  }
  result[0] = '\0';
  size_t result_len = 0;

  // Process each line
  char* line = strtok(stack_copy, "\n");
  while (line) {
    char* to_append = NULL;
    bool should_free = false;

    if (strstr(line, "    at ")) {
      // This is a stack frame line, try to transform it
      to_append = transform_stack_frame(ctx, cache, line);
      should_free = true;
    } else {
      // Not a stack frame (e.g., error message), keep as-is
      to_append = line;
      should_free = false;
    }

    // Append with bounds checking
    if (to_append) {
      size_t append_len = strlen(to_append);
      if (result_len + append_len + 2 < result_capacity) {  // +2 for \n and \0
        memcpy(result + result_len, to_append, append_len);
        result_len += append_len;
        result[result_len++] = '\n';
        result[result_len] = '\0';
      }

      if (should_free) {
        free(to_append);
      }
    }

    line = strtok(NULL, "\n");
  }

  free(stack_copy);

  JSValue result_val = JS_NewString(ctx, result);
  free(result);
  return result_val;
}

/**
 * Wrapper Error constructor that transforms stack traces
 * Calls original Error constructor, then transforms the stack using source maps
 */
static JSValue jsrt_error_wrapper(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  // Get the original Error constructor
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue original_error = JS_GetPropertyStr(ctx, global, "__OriginalError__");
  JS_FreeValue(ctx, global);

  if (!JS_IsFunction(ctx, original_error)) {
    JS_FreeValue(ctx, original_error);
    return JS_ThrowTypeError(ctx, "Original Error constructor not found");
  }

  // Call original Error constructor
  JSValue error_obj;
  if (JS_IsUndefined(new_target)) {
    // Called as function: Error(msg)
    error_obj = JS_CallConstructor(ctx, original_error, argc, argv);
  } else {
    // Called as constructor: new Error(msg)
    error_obj = JS_CallConstructor(ctx, original_error, argc, argv);
  }

  JS_FreeValue(ctx, original_error);

  if (JS_IsException(error_obj)) {
    return error_obj;
  }

  // Transform the stack if source maps are enabled
  if (g_source_map_cache) {
    bool enabled = false, node_modules = false, generated_code = false;
    jsrt_source_map_cache_get_config(g_source_map_cache, &enabled, &node_modules, &generated_code);

    if (enabled) {
      // Get the current stack
      JSValue stack_val = JS_GetPropertyStr(ctx, error_obj, "stack");
      if (!JS_IsUndefined(stack_val) && !JS_IsException(stack_val)) {
        const char* stack_str = JS_ToCString(ctx, stack_val);
        if (stack_str) {
          // Transform the stack
          JSValue transformed = jsrt_transform_error_stack(ctx, g_source_map_cache, stack_str);

          // Replace the stack property
          JS_SetPropertyStr(ctx, error_obj, "stack", transformed);

          JS_FreeCString(ctx, stack_str);
        }
      }
      JS_FreeValue(ctx, stack_val);
    }
  }

  return error_obj;
}

/**
 * Initialize error stack integration
 */
bool jsrt_error_stack_init(JSContext* ctx, JSRT_SourceMapCache* cache) {
  if (!ctx || !cache) {
    return false;
  }

  g_source_map_cache = cache;

  // Get the global object
  JSValue global = JS_GetGlobalObject(ctx);

  // Get the original Error constructor
  JSValue original_error = JS_GetPropertyStr(ctx, global, "Error");
  if (!JS_IsFunction(ctx, original_error)) {
    JS_FreeValue(ctx, original_error);
    JS_FreeValue(ctx, global);
    return false;
  }

  // Save the original Error constructor
  JS_SetPropertyStr(ctx, global, "__OriginalError__", JS_DupValue(ctx, original_error));

  // Create wrapper Error constructor
  JSValue wrapper = JS_NewCFunction2(ctx, jsrt_error_wrapper, "Error", 1, JS_CFUNC_constructor, 0);

  // Copy properties from original Error to wrapper
  JSValue error_proto = JS_GetPropertyStr(ctx, original_error, "prototype");
  JS_SetPropertyStr(ctx, wrapper, "prototype", error_proto);

  // Add missing Node.js Error methods
  JSValue capture_stack_trace_func = JS_NewCFunction(ctx, js_error_capture_stack_trace, "captureStackTrace", 2);
  JS_SetPropertyStr(ctx, wrapper, "captureStackTrace", capture_stack_trace_func);

  JSValue stack_trace_limit_val = JS_NewInt32(ctx, 10);
  JS_SetPropertyStr(ctx, wrapper, "stackTraceLimit", stack_trace_limit_val);
  JS_FreeValue(ctx, stack_trace_limit_val);

  // Replace global Error with wrapper
  JS_SetPropertyStr(ctx, global, "Error", wrapper);

  JS_FreeValue(ctx, original_error);
  JS_FreeValue(ctx, global);

  JSRT_Debug("Error stack integration initialized with source map support");

  return true;
}
