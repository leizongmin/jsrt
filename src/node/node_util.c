#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "node_modules.h"

// Forward declarations
static JSValue js_util_inspect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

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
    if (format[i] == '%' && i + 1 < format_len) {
      char placeholder = format[i + 1];

      // Handle %% escape immediately
      if (placeholder == '%') {
        if (result_pos >= result_capacity - 1) {
          result_capacity *= 2;
          result = realloc(result, result_capacity);
          if (!result) {
            JS_FreeCString(ctx, format);
            JS_ThrowOutOfMemory(ctx);
            return JS_EXCEPTION;
          }
        }
        result[result_pos++] = '%';
        i++;  // Skip the second %
        continue;
      }

      // For other placeholders, check if we have arguments
      if (arg_index >= argc) {
        // No more arguments, treat as literal
        result[result_pos++] = '%';
        if (result_pos >= result_capacity - 1) {
          result_capacity *= 2;
          result = realloc(result, result_capacity);
          if (!result) {
            JS_FreeCString(ctx, format);
            JS_ThrowOutOfMemory(ctx);
            return JS_EXCEPTION;
          }
        }
        result[result_pos++] = placeholder;
        i++;
        continue;
      }

      const char* replacement = NULL;
      char* temp_str = NULL;
      bool should_free = false;

      switch (placeholder) {
        case 's': {  // String placeholder
          replacement = JS_ToCString(ctx, argv[arg_index]);
          should_free = true;
          break;
        }
        case 'd':    // Number placeholder (integer)
        case 'i': {  // Integer placeholder (same as %d)
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
        case 'f': {  // Floating point placeholder
          double num;
          if (JS_ToFloat64(ctx, &num, argv[arg_index]) == 0) {
            temp_str = malloc(64);
            snprintf(temp_str, 64, "%f", num);
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
        case 'o':    // Object placeholder (util.inspect without options)
        case 'O': {  // Object placeholder (util.inspect with options)
          // Use util.inspect() for object formatting
          JSValue inspect_args[1] = {argv[arg_index]};
          JSValue inspect_result = js_util_inspect(ctx, JS_UNDEFINED, 1, inspect_args);
          if (!JS_IsException(inspect_result)) {
            replacement = JS_ToCString(ctx, inspect_result);
            should_free = true;
            JS_FreeValue(ctx, inspect_result);
          }
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

// util.formatWithOptions() implementation - format with custom inspect options
static JSValue js_util_format_with_options(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_NewString(ctx, "");
  }

  // First argument is the inspectOptions object
  JSValue inspect_options = argv[0];

  // Remaining arguments are passed to format()
  // We need to call format with the remaining arguments
  int format_argc = argc - 1;
  JSValueConst* format_argv = &argv[1];

  // For now, we'll just call the regular format() function
  // In the future, we can pass inspect_options to inspect() when used with %o/%O
  return js_util_format(ctx, this_val, format_argc, format_argv);
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

  // Create wrapper function that returns a Promise
  JSValue original_fn = argv[0];

  // Create a wrapper function using JavaScript
  const char* wrapper_code =
      "(function(original) {"
      "  return function(...args) {"
      "    return new Promise((resolve, reject) => {"
      "      args.push((err, ...results) => {"
      "        if (err) reject(err);"
      "        else resolve(results.length <= 1 ? results[0] : results);"
      "      });"
      "      try {"
      "        original.apply(this, args);"
      "      } catch (e) {"
      "        reject(e);"
      "      }"
      "    });"
      "  };"
      "})";

  JSValue wrapper_factory = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<util.promisify>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(wrapper_factory)) {
    return JS_EXCEPTION;
  }

  JSValue wrapper = JS_Call(ctx, wrapper_factory, JS_UNDEFINED, 1, &original_fn);
  JS_FreeValue(ctx, wrapper_factory);

  return wrapper;
}

// util.callbackify() implementation - converts async function to callback style
static JSValue js_util_callbackify(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "util.callbackify requires a function argument");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "util.callbackify argument must be a function");
  }

  JSValue original_fn = argv[0];

  // Create a wrapper function using JavaScript
  const char* wrapper_code =
      "(function(original) {"
      "  return function(...args) {"
      "    const callback = args.pop();"
      "    if (typeof callback !== 'function') {"
      "      throw new TypeError('The last argument must be a callback function');"
      "    }"
      "    original.apply(this, args).then("
      "      (result) => callback(null, result),"
      "      (err) => callback(err)"
      "    );"
      "  };"
      "})";

  JSValue wrapper_factory = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<util.callbackify>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(wrapper_factory)) {
    return JS_EXCEPTION;
  }

  JSValue wrapper = JS_Call(ctx, wrapper_factory, JS_UNDEFINED, 1, &original_fn);
  JS_FreeValue(ctx, wrapper_factory);

  return wrapper;
}

// util.deprecate() implementation - mark function as deprecated
static JSValue js_util_deprecate(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "util.deprecate requires function and message arguments");
  }

  if (!JS_IsFunction(ctx, argv[0])) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "First argument must be a function");
  }

  JSValue original_fn = argv[0];
  const char* message = JS_ToCString(ctx, argv[1]);
  if (!message) {
    return JS_EXCEPTION;
  }

  // Create deprecation code (extract code parameter if provided)
  const char* code = "DEP0000";
  if (argc > 2 && JS_IsString(argv[2])) {
    code = JS_ToCString(ctx, argv[2]);
  }

  // Create wrapper that emits warning on first call
  char wrapper_code[1024];
  snprintf(wrapper_code, sizeof(wrapper_code),
           "(function(fn, msg, code) {"
           "  let warned = false;"
           "  return function(...args) {"
           "    if (!warned) {"
           "      warned = true;"
           "      console.warn('[' + code + '] DeprecationWarning: ' + msg);"
           "    }"
           "    return fn.apply(this, args);"
           "  };"
           "})");

  JSValue wrapper_factory = JS_Eval(ctx, wrapper_code, strlen(wrapper_code), "<util.deprecate>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(wrapper_factory)) {
    JS_FreeCString(ctx, message);
    if (argc > 2)
      JS_FreeCString(ctx, code);
    return JS_EXCEPTION;
  }

  JSValue args[3] = {original_fn, argv[1], JS_NewString(ctx, code)};
  JSValue wrapper = JS_Call(ctx, wrapper_factory, JS_UNDEFINED, 3, args);

  JS_FreeValue(ctx, wrapper_factory);
  JS_FreeValue(ctx, args[2]);
  JS_FreeCString(ctx, message);
  if (argc > 2)
    JS_FreeCString(ctx, code);

  return wrapper;
}

// util.debuglog() implementation - conditional debug logging
static JSValue js_util_debuglog(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "util.debuglog requires a section argument");
  }

  const char* section = JS_ToCString(ctx, argv[0]);
  if (!section) {
    return JS_EXCEPTION;
  }

  // Check NODE_DEBUG environment variable
  // For simplicity, create a no-op function by default
  const char* debug_code =
      "(function(section) {"
      "  const enabled = false;"  // TODO: Check NODE_DEBUG env var
      "  const fn = function(...args) {"
      "    if (enabled) {"
      "      console.error(section + ':', ...args);"
      "    }"
      "  };"
      "  fn.enabled = enabled;"
      "  return fn;"
      "})";

  JSValue factory = JS_Eval(ctx, debug_code, strlen(debug_code), "<util.debuglog>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(factory)) {
    JS_FreeCString(ctx, section);
    return JS_EXCEPTION;
  }

  JSValue debug_fn = JS_Call(ctx, factory, JS_UNDEFINED, 1, argv);
  JS_FreeValue(ctx, factory);
  JS_FreeCString(ctx, section);

  return debug_fn;
}

// util.inherits() implementation - prototypal inheritance (legacy)
static JSValue js_util_inherits(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return node_throw_error(ctx, NODE_ERR_MISSING_ARGS, "util.inherits requires constructor and superConstructor");
  }

  if (!JS_IsFunction(ctx, argv[0]) || !JS_IsFunction(ctx, argv[1])) {
    return node_throw_error(ctx, NODE_ERR_INVALID_ARG_TYPE, "Both arguments must be constructor functions");
  }

  // Set up prototype chain: constructor.prototype = Object.create(superConstructor.prototype)
  JSValue super_proto = JS_GetPropertyStr(ctx, argv[1], "prototype");
  if (JS_IsException(super_proto)) {
    return JS_EXCEPTION;
  }

  JSValue object_ctor = JS_GetGlobalObject(ctx);
  JSValue object_obj = JS_GetPropertyStr(ctx, object_ctor, "Object");
  JSValue create_fn = JS_GetPropertyStr(ctx, object_obj, "create");

  JSValue new_proto = JS_Call(ctx, create_fn, object_obj, 1, &super_proto);

  JS_FreeValue(ctx, super_proto);
  JS_FreeValue(ctx, create_fn);
  JS_FreeValue(ctx, object_obj);
  JS_FreeValue(ctx, object_ctor);

  if (JS_IsException(new_proto)) {
    return JS_EXCEPTION;
  }

  // Set constructor property
  JS_SetPropertyStr(ctx, new_proto, "constructor", JS_DupValue(ctx, argv[0]));

  // Set the prototype
  JS_SetPropertyStr(ctx, argv[0], "prototype", new_proto);

  // Set super_ property
  JS_SetPropertyStr(ctx, argv[0], "super_", JS_DupValue(ctx, argv[1]));

  return JS_UNDEFINED;
}

// util.isDeepStrictEqual() implementation - deep equality check
static JSValue js_util_is_deep_strict_equal(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_FALSE;
  }

  // Use JavaScript implementation for simplicity
  const char* equal_code =
      "(function(a, b) {"
      "  if (a === b) return true;"
      "  if (a == null || b == null) return false;"
      "  if (typeof a !== 'object' || typeof b !== 'object') return false;"
      "  const keysA = Object.keys(a);"
      "  const keysB = Object.keys(b);"
      "  if (keysA.length !== keysB.length) return false;"
      "  for (const key of keysA) {"
      "    if (!keysB.includes(key)) return false;"
      "    if (!arguments.callee(a[key], b[key])) return false;"
      "  }"
      "  return true;"
      "})";

  JSValue equal_fn = JS_Eval(ctx, equal_code, strlen(equal_code), "<util.isDeepStrictEqual>", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(equal_fn)) {
    return JS_EXCEPTION;
  }

  JSValue result = JS_Call(ctx, equal_fn, JS_UNDEFINED, 2, argv);
  JS_FreeValue(ctx, equal_fn);

  return result;
}

// Initialize node:util module for CommonJS
JSValue JSRT_InitNodeUtil(JSContext* ctx) {
  JSValue util_obj = JS_NewObject(ctx);

  // Add utility functions
  JS_SetPropertyStr(ctx, util_obj, "format", JS_NewCFunction(ctx, js_util_format, "format", 0));
  JS_SetPropertyStr(ctx, util_obj, "formatWithOptions",
                    JS_NewCFunction(ctx, js_util_format_with_options, "formatWithOptions", 2));
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
  JS_SetPropertyStr(ctx, util_obj, "callbackify", JS_NewCFunction(ctx, js_util_callbackify, "callbackify", 1));

  // Deprecation and debugging
  JS_SetPropertyStr(ctx, util_obj, "deprecate", JS_NewCFunction(ctx, js_util_deprecate, "deprecate", 2));
  JS_SetPropertyStr(ctx, util_obj, "debuglog", JS_NewCFunction(ctx, js_util_debuglog, "debuglog", 1));

  // Legacy and additional utilities
  JS_SetPropertyStr(ctx, util_obj, "inherits", JS_NewCFunction(ctx, js_util_inherits, "inherits", 2));
  JS_SetPropertyStr(ctx, util_obj, "isDeepStrictEqual",
                    JS_NewCFunction(ctx, js_util_is_deep_strict_equal, "isDeepStrictEqual", 2));

  // util.types namespace - modern type checking API
  JSValue types_obj = JS_NewObject(ctx);

  // Reuse existing type checkers
  JS_SetPropertyStr(ctx, types_obj, "isDate", JS_NewCFunction(ctx, js_util_is_object, "isDate", 1));  // Simplified
  JS_SetPropertyStr(ctx, types_obj, "isPromise",
                    JS_NewCFunction(ctx, js_util_is_object, "isPromise", 1));                             // Simplified
  JS_SetPropertyStr(ctx, types_obj, "isRegExp", JS_NewCFunction(ctx, js_util_is_object, "isRegExp", 1));  // Simplified
  JS_SetPropertyStr(ctx, types_obj, "isArrayBuffer",
                    JS_NewCFunction(ctx, js_util_is_object, "isArrayBuffer", 1));  // Simplified

  JS_SetPropertyStr(ctx, util_obj, "types", types_obj);

  // TextEncoder and TextDecoder - export from global scope
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue text_encoder = JS_GetPropertyStr(ctx, global, "TextEncoder");
  JSValue text_decoder = JS_GetPropertyStr(ctx, global, "TextDecoder");

  JS_SetPropertyStr(ctx, util_obj, "TextEncoder", JS_DupValue(ctx, text_encoder));
  JS_SetPropertyStr(ctx, util_obj, "TextDecoder", JS_DupValue(ctx, text_decoder));

  JS_FreeValue(ctx, text_encoder);
  JS_FreeValue(ctx, text_decoder);
  JS_FreeValue(ctx, global);

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

  JSValue formatWithOptions = JS_GetPropertyStr(ctx, util_module, "formatWithOptions");
  JS_SetModuleExport(ctx, m, "formatWithOptions", JS_DupValue(ctx, formatWithOptions));
  JS_FreeValue(ctx, formatWithOptions);

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

  // Export TextEncoder and TextDecoder
  JSValue textEncoder = JS_GetPropertyStr(ctx, util_module, "TextEncoder");
  JS_SetModuleExport(ctx, m, "TextEncoder", JS_DupValue(ctx, textEncoder));
  JS_FreeValue(ctx, textEncoder);

  JSValue textDecoder = JS_GetPropertyStr(ctx, util_module, "TextDecoder");
  JS_SetModuleExport(ctx, m, "TextDecoder", JS_DupValue(ctx, textDecoder));
  JS_FreeValue(ctx, textDecoder);

  JS_FreeValue(ctx, util_module);
  return 0;
}