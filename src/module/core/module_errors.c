/**
 * Module Error Handling Implementation
 *
 * Provides functions for throwing JavaScript errors with module-specific context.
 */

#include "../util/module_errors.h"
#include <quickjs.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util/module_debug.h"

/**
 * Throw a module error as a JavaScript exception
 *
 * Creates a JavaScript Error object with the specified error code and message,
 * and throws it in the given context.
 *
 * @param ctx The JavaScript context
 * @param error_code The module error code
 * @param message Optional custom message (NULL to use default)
 * @param ... Format arguments for message (printf-style)
 * @return JS_EXCEPTION
 */
JSValue jsrt_module_throw_error(JSContext* ctx, JSRT_ModuleError error_code, const char* message, ...) {
  if (!ctx) {
    MODULE_Debug_Error("Cannot throw error: NULL context");
    return JS_EXCEPTION;
  }

  // Build error message
  char error_msg[1024];
  if (message) {
    va_list args;
    va_start(args, message);
    vsnprintf(error_msg, sizeof(error_msg), message, args);
    va_end(args);
  } else {
    // Use default error message
    snprintf(error_msg, sizeof(error_msg), "%s", jsrt_module_error_to_string(error_code));
  }

  MODULE_Debug_Error("Throwing error: [%d] %s", error_code, error_msg);

  // Create JavaScript Error object
  JSValue error = JS_NewError(ctx);
  if (JS_IsException(error)) {
    return JS_EXCEPTION;
  }

  // Set error message
  JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, error_msg),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  // Set error code property
  JS_DefinePropertyValueStr(ctx, error, "code", JS_NewInt32(ctx, error_code), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  // Set error category
  const char* category = jsrt_module_get_error_category(error_code);
  JS_DefinePropertyValueStr(ctx, error, "category", JS_NewString(ctx, category),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  // Throw the error
  JSValue result = JS_Throw(ctx, error);

  return result;
}

/**
 * Get error category as string
 *
 * @param error_code The error code
 * @return String describing the error category
 */
const char* jsrt_module_get_error_category(JSRT_ModuleError error_code) {
  if (jsrt_module_error_is_resolution(error_code)) {
    return "MODULE_RESOLUTION";
  } else if (jsrt_module_error_is_loading(error_code)) {
    return "MODULE_LOADING";
  } else if (jsrt_module_error_is_type(error_code)) {
    return "MODULE_TYPE";
  } else if (jsrt_module_error_is_protocol(error_code)) {
    return "MODULE_PROTOCOL";
  } else if (jsrt_module_error_is_cache(error_code)) {
    return "MODULE_CACHE";
  } else if (jsrt_module_error_is_security(error_code)) {
    return "MODULE_SECURITY";
  } else if (jsrt_module_error_is_system(error_code)) {
    return "MODULE_SYSTEM";
  } else if (error_code == JSRT_MODULE_OK) {
    return "SUCCESS";
  } else {
    return "UNKNOWN";
  }
}

/**
 * Create a detailed error info structure with formatting
 *
 * @param code Error code
 * @param module_specifier Module specifier (can be NULL)
 * @param message_fmt Format string for custom message
 * @param ... Format arguments
 * @return Error info structure (caller must free with jsrt_module_error_free)
 */
JSRT_ModuleErrorInfo jsrt_module_error_create_fmt(JSRT_ModuleError code, const char* module_specifier,
                                                  const char* message_fmt, ...) {
  char message[1024];

  if (message_fmt) {
    va_list args;
    va_start(args, message_fmt);
    vsnprintf(message, sizeof(message), message_fmt, args);
    va_end(args);
  } else {
    snprintf(message, sizeof(message), "%s", jsrt_module_error_to_string(code));
  }

  return jsrt_module_error_create(code, message, module_specifier);
}

/**
 * Convert error info to JavaScript Error object
 *
 * @param ctx JavaScript context
 * @param info Error info structure
 * @return JavaScript Error object
 */
JSValue jsrt_module_error_to_js(JSContext* ctx, const JSRT_ModuleErrorInfo* info) {
  if (!ctx || !info) {
    MODULE_Debug_Error("Invalid arguments to jsrt_module_error_to_js");
    return JS_EXCEPTION;
  }

  // Create Error object
  JSValue error = JS_NewError(ctx);
  if (JS_IsException(error)) {
    return JS_EXCEPTION;
  }

  // Set message
  if (info->message) {
    JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, info->message),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  }

  // Set code
  JS_DefinePropertyValueStr(ctx, error, "code", JS_NewInt32(ctx, info->code), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  // Set category
  const char* category = jsrt_module_get_error_category(info->code);
  JS_DefinePropertyValueStr(ctx, error, "category", JS_NewString(ctx, category),
                            JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  // Set module specifier if available
  if (info->module_specifier) {
    JS_DefinePropertyValueStr(ctx, error, "specifier", JS_NewString(ctx, info->module_specifier),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  }

  // Set referrer if available
  if (info->referrer) {
    JS_DefinePropertyValueStr(ctx, error, "referrer", JS_NewString(ctx, info->referrer),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  }

  // Set resolved path if available
  if (info->resolved_path) {
    JS_DefinePropertyValueStr(ctx, error, "resolvedPath", JS_NewString(ctx, info->resolved_path),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  }

  // Set line/column if available
  if (info->line >= 0) {
    JS_DefinePropertyValueStr(ctx, error, "line", JS_NewInt32(ctx, info->line),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  }
  if (info->column >= 0) {
    JS_DefinePropertyValueStr(ctx, error, "column", JS_NewInt32(ctx, info->column),
                              JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
  }

  return error;
}

/**
 * Throw error from error info structure
 *
 * @param ctx JavaScript context
 * @param info Error info structure
 * @return JS_EXCEPTION
 */
JSValue jsrt_module_throw_error_info(JSContext* ctx, const JSRT_ModuleErrorInfo* info) {
  if (!ctx || !info) {
    MODULE_Debug_Error("Invalid arguments to jsrt_module_throw_error_info");
    return JS_EXCEPTION;
  }

  MODULE_Debug_Error("Throwing error from info: [%d] %s", info->code, info->message ? info->message : "");
  if (info->module_specifier) {
    MODULE_Debug_Error("  - Specifier: %s", info->module_specifier);
  }
  if (info->referrer) {
    MODULE_Debug_Error("  - Referrer: %s", info->referrer);
  }
  if (info->resolved_path) {
    MODULE_Debug_Error("  - Resolved path: %s", info->resolved_path);
  }

  JSValue error = jsrt_module_error_to_js(ctx, info);
  if (JS_IsException(error)) {
    return JS_EXCEPTION;
  }

  return JS_Throw(ctx, error);
}
