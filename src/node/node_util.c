#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "node_modules.h"

// util.format() implementation - Node.js-compatible formatting with %s, %d, %j placeholders
static JSValue js_util_format(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewString(ctx, "");
  }

  const char* format = JS_ToCString(ctx, argv[0]);
  if (!format)
    return JS_EXCEPTION;

  size_t format_len = strlen(format);
  size_t result_capacity = format_len * 2 + 1024;  // Start with generous buffer
  char* result = malloc(result_capacity);
  if (!result) {
    JS_FreeCString(ctx, format);
    JS_ThrowOutOfMemory(ctx);
    return JS_EXCEPTION;
  }

  size_t result_pos = 0;
  int arg_index = 1;

  for (size_t i = 0; i < format_len; i++) {
    if (format[i] == '%' && i + 1 < format_len && arg_index < argc) {
      char placeholder = format[i + 1];
      const char* replacement = NULL;
      char* temp_str = NULL;
      bool should_free = false;

      switch (placeholder) {
        case 's': {  // String placeholder
          replacement = JS_ToCString(ctx, argv[arg_index]);
          should_free = true;
          break;
        }
        case 'd': {  // Number placeholder
          double num;
          if (JS_ToFloat64(ctx, &num, argv[arg_index]) == 0) {
            temp_str = malloc(32);
            snprintf(temp_str, 32, "%.0f", num);
            replacement = temp_str;
            should_free = true;
          } else {
            replacement = "NaN";
          }
          break;
        }
        case 'j': {  // JSON placeholder
          JSValue json_global = JS_GetGlobalObject(ctx);
          JSValue json_obj = JS_GetPropertyStr(ctx, json_global, "JSON");
          JSValue stringify_fn = JS_GetPropertyStr(ctx, json_obj, "stringify");

          if (JS_IsFunction(ctx, stringify_fn)) {
            JSValue json_result = JS_Call(ctx, stringify_fn, json_obj, 1, &argv[arg_index]);
            if (!JS_IsException(json_result)) {
              replacement = JS_ToCString(ctx, json_result);
              should_free = true;
            }
            JS_FreeValue(ctx, json_result);
          }

          JS_FreeValue(ctx, json_global);
          JS_FreeValue(ctx, json_obj);
          JS_FreeValue(ctx, stringify_fn);
          break;
        }
        case '%': {  // Escaped %
          replacement = "%";
          arg_index--;  // Don't consume an argument
          break;
        }
        default: {
          // Unknown placeholder, treat as literal
          result[result_pos++] = '%';
          result[result_pos++] = placeholder;
          i++;  // Skip the placeholder char
          continue;
        }
      }

      if (replacement) {
        size_t repl_len = strlen(replacement);
        // Ensure we have enough space
        while (result_pos + repl_len >= result_capacity) {
          result_capacity *= 2;
          result = realloc(result, result_capacity);
          if (!result) {
            if (should_free) {
              if (temp_str)
                free(temp_str);
              else
                JS_FreeCString(ctx, replacement);
            }
            JS_FreeCString(ctx, format);
            JS_ThrowOutOfMemory(ctx);
            return JS_EXCEPTION;
          }
        }

        strcpy(result + result_pos, replacement);
        result_pos += repl_len;

        if (should_free) {
          if (temp_str) {
            free(temp_str);
          } else {
            JS_FreeCString(ctx, replacement);
          }
        }
      }

      i++;  // Skip the placeholder character
      arg_index++;
    } else {
      // Regular character
      if (result_pos >= result_capacity - 1) {
        result_capacity *= 2;
        result = realloc(result, result_capacity);
        if (!result) {
          JS_FreeCString(ctx, format);
          JS_ThrowOutOfMemory(ctx);
          return JS_EXCEPTION;
        }
      }
      result[result_pos++] = format[i];
    }
  }

  // Append any remaining arguments with spaces (Node.js behavior)
  for (int i = arg_index; i < argc; i++) {
    const char* arg_str = JS_ToCString(ctx, argv[i]);
    if (arg_str) {
      size_t arg_len = strlen(arg_str);
      // Ensure space for " " + arg + null terminator
      while (result_pos + arg_len + 2 >= result_capacity) {
        result_capacity *= 2;
        result = realloc(result, result_capacity);
        if (!result) {
          JS_FreeCString(ctx, arg_str);
          JS_FreeCString(ctx, format);
          JS_ThrowOutOfMemory(ctx);
          return JS_EXCEPTION;
        }
      }

      result[result_pos++] = ' ';
      strcpy(result + result_pos, arg_str);
      result_pos += arg_len;
      JS_FreeCString(ctx, arg_str);
    }
  }

  result[result_pos] = '\0';

  JSValue ret = JS_NewString(ctx, result);
  free(result);
  JS_FreeCString(ctx, format);
  return ret;
}

// util.inspect() implementation - object inspection
static JSValue js_util_inspect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_NewString(ctx, "undefined");
  }

  // Use JSON.stringify for basic object inspection
  JSValue json_stringify = JS_GetGlobalObject(ctx);
  JSValue json_obj = JS_GetPropertyStr(ctx, json_stringify, "JSON");
  JSValue stringify_fn = JS_GetPropertyStr(ctx, json_obj, "stringify");

  JSValue result;
  if (JS_IsFunction(ctx, stringify_fn)) {
    JSValue stringify_args[3] = {argv[0], JS_NULL, JS_NewInt32(ctx, 2)};  // Pretty print with 2 spaces
    result = JS_Call(ctx, stringify_fn, json_obj, 3, stringify_args);
    JS_FreeValue(ctx, stringify_args[2]);
  } else {
    // Fallback to toString
    result = JS_ToString(ctx, argv[0]);
  }

  JS_FreeValue(ctx, json_stringify);
  JS_FreeValue(ctx, json_obj);
  JS_FreeValue(ctx, stringify_fn);

  return result;
}

// util.isArray() implementation
static JSValue js_util_is_array(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, JS_IsArray(ctx, argv[0]));
}

// util.isObject() implementation
static JSValue js_util_is_object(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }

  JSValue val = argv[0];
  return JS_NewBool(ctx, JS_IsObject(val) && !JS_IsArray(ctx, val) && !JS_IsFunction(ctx, val));
}

// util.isString() implementation
static JSValue js_util_is_string(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, JS_IsString(argv[0]));
}

// util.isNumber() implementation
static JSValue js_util_is_number(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, JS_IsNumber(argv[0]));
}

// util.isBoolean() implementation
static JSValue js_util_is_boolean(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, JS_IsBool(argv[0]));
}

// util.isFunction() implementation
static JSValue js_util_is_function(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, JS_IsFunction(ctx, argv[0]));
}

// util.isNull() implementation
static JSValue js_util_is_null(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, JS_IsNull(argv[0]));
}

// util.isUndefined() implementation
static JSValue js_util_is_undefined(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_TRUE;  // No argument means undefined
  }
  return JS_NewBool(ctx, JS_IsUndefined(argv[0]));
}

// util.promisify() implementation - converts callback-style function to promise
static JSValue js_util_promisify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "util.promisify requires a function argument");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "util.promisify argument must be a function");
  }

  // Return a wrapper function that returns a promise
  // This is a simplified implementation - a full implementation would be more complex
  return JS_DupValue(ctx, argv[0]);  // For now, just return the original function
}

// Initialize node:util module for CommonJS
JSValue JSRT_InitNodeUtil(JSContext* ctx) {
  JSValue util_obj = JS_NewObject(ctx);

  // Add utility functions
  JS_SetPropertyStr(ctx, util_obj, "format", JS_NewCFunction(ctx, js_util_format, "format", 0));
  JS_SetPropertyStr(ctx, util_obj, "inspect", JS_NewCFunction(ctx, js_util_inspect, "inspect", 1));

  // Type checking functions
  JS_SetPropertyStr(ctx, util_obj, "isArray", JS_NewCFunction(ctx, js_util_is_array, "isArray", 1));
  JS_SetPropertyStr(ctx, util_obj, "isObject", JS_NewCFunction(ctx, js_util_is_object, "isObject", 1));
  JS_SetPropertyStr(ctx, util_obj, "isString", JS_NewCFunction(ctx, js_util_is_string, "isString", 1));
  JS_SetPropertyStr(ctx, util_obj, "isNumber", JS_NewCFunction(ctx, js_util_is_number, "isNumber", 1));
  JS_SetPropertyStr(ctx, util_obj, "isBoolean", JS_NewCFunction(ctx, js_util_is_boolean, "isBoolean", 1));
  JS_SetPropertyStr(ctx, util_obj, "isFunction", JS_NewCFunction(ctx, js_util_is_function, "isFunction", 1));
  JS_SetPropertyStr(ctx, util_obj, "isNull", JS_NewCFunction(ctx, js_util_is_null, "isNull", 1));
  JS_SetPropertyStr(ctx, util_obj, "isUndefined", JS_NewCFunction(ctx, js_util_is_undefined, "isUndefined", 1));

  // Promise utilities
  JS_SetPropertyStr(ctx, util_obj, "promisify", JS_NewCFunction(ctx, js_util_promisify, "promisify", 1));

  return util_obj;
}

// Initialize node:util module for ES modules
int js_node_util_init(JSContext* ctx, JSModuleDef* m) {
  JSValue util_module = JSRT_InitNodeUtil(ctx);

  // Export as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, util_module));

  // Export individual functions
  JSValue format = JS_GetPropertyStr(ctx, util_module, "format");
  JS_SetModuleExport(ctx, m, "format", JS_DupValue(ctx, format));
  JS_FreeValue(ctx, format);

  JSValue inspect = JS_GetPropertyStr(ctx, util_module, "inspect");
  JS_SetModuleExport(ctx, m, "inspect", JS_DupValue(ctx, inspect));
  JS_FreeValue(ctx, inspect);

  JSValue isArray = JS_GetPropertyStr(ctx, util_module, "isArray");
  JS_SetModuleExport(ctx, m, "isArray", JS_DupValue(ctx, isArray));
  JS_FreeValue(ctx, isArray);

  JSValue isObject = JS_GetPropertyStr(ctx, util_module, "isObject");
  JS_SetModuleExport(ctx, m, "isObject", JS_DupValue(ctx, isObject));
  JS_FreeValue(ctx, isObject);

  JSValue isString = JS_GetPropertyStr(ctx, util_module, "isString");
  JS_SetModuleExport(ctx, m, "isString", JS_DupValue(ctx, isString));
  JS_FreeValue(ctx, isString);

  JSValue isNumber = JS_GetPropertyStr(ctx, util_module, "isNumber");
  JS_SetModuleExport(ctx, m, "isNumber", JS_DupValue(ctx, isNumber));
  JS_FreeValue(ctx, isNumber);

  JSValue isBoolean = JS_GetPropertyStr(ctx, util_module, "isBoolean");
  JS_SetModuleExport(ctx, m, "isBoolean", JS_DupValue(ctx, isBoolean));
  JS_FreeValue(ctx, isBoolean);

  JSValue isFunction = JS_GetPropertyStr(ctx, util_module, "isFunction");
  JS_SetModuleExport(ctx, m, "isFunction", JS_DupValue(ctx, isFunction));
  JS_FreeValue(ctx, isFunction);

  JSValue isNull = JS_GetPropertyStr(ctx, util_module, "isNull");
  JS_SetModuleExport(ctx, m, "isNull", JS_DupValue(ctx, isNull));
  JS_FreeValue(ctx, isNull);

  JSValue isUndefined = JS_GetPropertyStr(ctx, util_module, "isUndefined");
  JS_SetModuleExport(ctx, m, "isUndefined", JS_DupValue(ctx, isUndefined));
  JS_FreeValue(ctx, isUndefined);

  JSValue promisify = JS_GetPropertyStr(ctx, util_module, "promisify");
  JS_SetModuleExport(ctx, m, "promisify", JS_DupValue(ctx, promisify));
  JS_FreeValue(ctx, promisify);

  JS_FreeValue(ctx, util_module);
  return 0;
}