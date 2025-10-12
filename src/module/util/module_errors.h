#ifndef __JSRT_MODULE_ERRORS_H__
#define __JSRT_MODULE_ERRORS_H__

/**
 * Module System Error Codes
 *
 * Standardized error codes for the module loading system.
 * Provides consistent error reporting across all module components.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  // Success
  JSRT_MODULE_OK = 0,

  // Resolution errors (1-99)
  JSRT_MODULE_NOT_FOUND = 1,
  JSRT_MODULE_INVALID_SPECIFIER = 2,
  JSRT_MODULE_AMBIGUOUS_SPECIFIER = 3,
  JSRT_MODULE_RESOLUTION_FAILED = 4,
  JSRT_MODULE_PACKAGE_JSON_INVALID = 5,
  JSRT_MODULE_PACKAGE_JSON_NOT_FOUND = 6,
  JSRT_MODULE_EXPORTS_NOT_FOUND = 7,
  JSRT_MODULE_IMPORT_NOT_FOUND = 8,

  // Loading errors (100-199)
  JSRT_MODULE_LOAD_FAILED = 100,
  JSRT_MODULE_READ_ERROR = 101,
  JSRT_MODULE_PARSE_ERROR = 102,
  JSRT_MODULE_COMPILE_ERROR = 103,
  JSRT_MODULE_EXECUTION_ERROR = 104,
  JSRT_MODULE_CIRCULAR_DEPENDENCY = 105,

  // Type detection errors (200-299)
  JSRT_MODULE_TYPE_UNKNOWN = 200,
  JSRT_MODULE_TYPE_MISMATCH = 201,
  JSRT_MODULE_TYPE_UNSUPPORTED = 202,

  // Protocol errors (300-399)
  JSRT_MODULE_PROTOCOL_UNSUPPORTED = 300,
  JSRT_MODULE_PROTOCOL_SECURITY_ERROR = 301,
  JSRT_MODULE_PROTOCOL_NETWORK_ERROR = 302,
  JSRT_MODULE_PROTOCOL_TIMEOUT = 303,

  // Cache errors (400-499)
  JSRT_MODULE_CACHE_ERROR = 400,
  JSRT_MODULE_CACHE_FULL = 401,
  JSRT_MODULE_CACHE_CORRUPTED = 402,

  // Security errors (500-599)
  JSRT_MODULE_SECURITY_VIOLATION = 500,
  JSRT_MODULE_ACCESS_DENIED = 501,
  JSRT_MODULE_DOMAIN_NOT_ALLOWED = 502,
  JSRT_MODULE_SIZE_LIMIT_EXCEEDED = 503,

  // System errors (600-699)
  JSRT_MODULE_OUT_OF_MEMORY = 600,
  JSRT_MODULE_INTERNAL_ERROR = 601,
  JSRT_MODULE_INVALID_ARGUMENT = 602,
  JSRT_MODULE_INVALID_STATE = 603,

} JSRT_ModuleError;

/**
 * Module error context
 * Provides detailed error information for debugging
 */
typedef struct {
  JSRT_ModuleError code;
  char* message;           // Human-readable error message
  char* module_specifier;  // The module specifier that caused the error
  char* referrer;          // The module that requested this module (if any)
  char* resolved_path;     // The resolved path (if resolution succeeded)
  int line;                // Line number in module (for parse/compile errors)
  int column;              // Column number in module (for parse/compile errors)
} JSRT_ModuleErrorInfo;

/**
 * Error category helpers
 */
static inline int jsrt_module_error_is_resolution(JSRT_ModuleError err) {
  return err >= 1 && err < 100;
}

static inline int jsrt_module_error_is_loading(JSRT_ModuleError err) {
  return err >= 100 && err < 200;
}

static inline int jsrt_module_error_is_type(JSRT_ModuleError err) {
  return err >= 200 && err < 300;
}

static inline int jsrt_module_error_is_protocol(JSRT_ModuleError err) {
  return err >= 300 && err < 400;
}

static inline int jsrt_module_error_is_cache(JSRT_ModuleError err) {
  return err >= 400 && err < 500;
}

static inline int jsrt_module_error_is_security(JSRT_ModuleError err) {
  return err >= 500 && err < 600;
}

static inline int jsrt_module_error_is_system(JSRT_ModuleError err) {
  return err >= 600 && err < 700;
}

/**
 * Convert error code to human-readable string
 */
static inline const char* jsrt_module_error_to_string(JSRT_ModuleError err) {
  switch (err) {
    case JSRT_MODULE_OK:
      return "Success";

    // Resolution errors
    case JSRT_MODULE_NOT_FOUND:
      return "Module not found";
    case JSRT_MODULE_INVALID_SPECIFIER:
      return "Invalid module specifier";
    case JSRT_MODULE_AMBIGUOUS_SPECIFIER:
      return "Ambiguous module specifier";
    case JSRT_MODULE_RESOLUTION_FAILED:
      return "Module resolution failed";
    case JSRT_MODULE_PACKAGE_JSON_INVALID:
      return "Invalid package.json";
    case JSRT_MODULE_PACKAGE_JSON_NOT_FOUND:
      return "package.json not found";
    case JSRT_MODULE_EXPORTS_NOT_FOUND:
      return "Export not found in package.json";
    case JSRT_MODULE_IMPORT_NOT_FOUND:
      return "Import not found in package.json";

    // Loading errors
    case JSRT_MODULE_LOAD_FAILED:
      return "Module load failed";
    case JSRT_MODULE_READ_ERROR:
      return "Module read error";
    case JSRT_MODULE_PARSE_ERROR:
      return "Module parse error";
    case JSRT_MODULE_COMPILE_ERROR:
      return "Module compile error";
    case JSRT_MODULE_EXECUTION_ERROR:
      return "Module execution error";
    case JSRT_MODULE_CIRCULAR_DEPENDENCY:
      return "Circular dependency detected";

    // Type detection errors
    case JSRT_MODULE_TYPE_UNKNOWN:
      return "Unknown module type";
    case JSRT_MODULE_TYPE_MISMATCH:
      return "Module type mismatch";
    case JSRT_MODULE_TYPE_UNSUPPORTED:
      return "Unsupported module type";

    // Protocol errors
    case JSRT_MODULE_PROTOCOL_UNSUPPORTED:
      return "Unsupported protocol";
    case JSRT_MODULE_PROTOCOL_SECURITY_ERROR:
      return "Protocol security error";
    case JSRT_MODULE_PROTOCOL_NETWORK_ERROR:
      return "Protocol network error";
    case JSRT_MODULE_PROTOCOL_TIMEOUT:
      return "Protocol timeout";

    // Cache errors
    case JSRT_MODULE_CACHE_ERROR:
      return "Cache error";
    case JSRT_MODULE_CACHE_FULL:
      return "Cache full";
    case JSRT_MODULE_CACHE_CORRUPTED:
      return "Cache corrupted";

    // Security errors
    case JSRT_MODULE_SECURITY_VIOLATION:
      return "Security violation";
    case JSRT_MODULE_ACCESS_DENIED:
      return "Access denied";
    case JSRT_MODULE_DOMAIN_NOT_ALLOWED:
      return "Domain not allowed";
    case JSRT_MODULE_SIZE_LIMIT_EXCEEDED:
      return "Size limit exceeded";

    // System errors
    case JSRT_MODULE_OUT_OF_MEMORY:
      return "Out of memory";
    case JSRT_MODULE_INTERNAL_ERROR:
      return "Internal error";
    case JSRT_MODULE_INVALID_ARGUMENT:
      return "Invalid argument";
    case JSRT_MODULE_INVALID_STATE:
      return "Invalid state";

    default:
      return "Unknown error";
  }
}

/**
 * Create error info structure
 */
static inline JSRT_ModuleErrorInfo jsrt_module_error_create(JSRT_ModuleError code, const char* message,
                                                            const char* module_specifier) {
  JSRT_ModuleErrorInfo info = {.code = code,
                               .message = message ? strdup(message) : strdup(jsrt_module_error_to_string(code)),
                               .module_specifier = module_specifier ? strdup(module_specifier) : NULL,
                               .referrer = NULL,
                               .resolved_path = NULL,
                               .line = -1,
                               .column = -1};
  return info;
}

/**
 * Free error info structure
 */
static inline void jsrt_module_error_free(JSRT_ModuleErrorInfo* info) {
  if (info) {
    if (info->message)
      free(info->message);
    if (info->module_specifier)
      free(info->module_specifier);
    if (info->referrer)
      free(info->referrer);
    if (info->resolved_path)
      free(info->resolved_path);
  }
}

// Forward declaration for JSValue
#ifndef JS_VALUE_DEFINED
struct JSValue;
typedef struct JSValue JSValue;
#define JS_VALUE_DEFINED
#endif

// Forward declaration for JSContext
#ifndef JS_CONTEXT_DEFINED
struct JSContext;
typedef struct JSContext JSContext;
#define JS_CONTEXT_DEFINED
#endif

/**
 * Throw a module error as a JavaScript exception
 * (Implemented in module_errors.c)
 */
JSValue jsrt_module_throw_error(JSContext* ctx, JSRT_ModuleError error_code, const char* message, ...);

/**
 * Get error category as string
 * (Implemented in module_errors.c)
 */
const char* jsrt_module_get_error_category(JSRT_ModuleError error_code);

/**
 * Create error info with formatted message
 * (Implemented in module_errors.c)
 */
JSRT_ModuleErrorInfo jsrt_module_error_create_fmt(JSRT_ModuleError code, const char* module_specifier,
                                                  const char* message_fmt, ...);

/**
 * Convert error info to JavaScript Error object
 * (Implemented in module_errors.c)
 */
JSValue jsrt_module_error_to_js(JSContext* ctx, const JSRT_ModuleErrorInfo* info);

/**
 * Throw error from error info structure
 * (Implemented in module_errors.c)
 */
JSValue jsrt_module_throw_error_info(JSContext* ctx, const JSRT_ModuleErrorInfo* info);

#endif  // __JSRT_MODULE_ERRORS_H__
