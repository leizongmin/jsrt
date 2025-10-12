/**
 * Builtin Module Loader Implementation
 */

#include "builtin_loader.h"
#include <stdlib.h>
#include <string.h>
#include "../core/module_cache.h"
#include "../util/module_debug.h"
#include "../util/module_errors.h"

// Forward declarations for builtin modules
#ifdef JSRT_NODE_COMPAT
#include "../../node/node_modules.h"
#endif

// jsrt: module initializers (from src/std/)
extern JSValue JSRT_CreateAssertModule(JSContext* ctx);
extern JSValue JSRT_CreateFFIModule(JSContext* ctx);
extern JSValue jsrt_get_process_module(JSContext* ctx);

/**
 * Check if specifier is a builtin
 */
bool jsrt_is_builtin_specifier(const char* specifier) {
  if (!specifier) {
    return false;
  }

  return (strncmp(specifier, "jsrt:", 5) == 0) || (strncmp(specifier, "node:", 5) == 0);
}

/**
 * Get builtin protocol
 */
const char* jsrt_get_builtin_protocol(const char* specifier) {
  if (!specifier) {
    return NULL;
  }

  if (strncmp(specifier, "jsrt:", 5) == 0) {
    return "jsrt";
  }

  if (strncmp(specifier, "node:", 5) == 0) {
    return "node";
  }

  return NULL;
}

/**
 * Extract builtin module name
 */
char* jsrt_extract_builtin_name(const char* specifier) {
  if (!specifier) {
    return NULL;
  }

  const char* colon = strchr(specifier, ':');
  if (!colon) {
    return NULL;
  }

  // Skip the colon and return the rest
  return strdup(colon + 1);
}

/**
 * Load jsrt: module
 */
static JSValue load_jsrt_module(JSContext* ctx, const char* module_name) {
  MODULE_Debug_Loader("Loading jsrt module: %s", module_name);

  if (strcmp(module_name, "assert") == 0) {
    return JSRT_CreateAssertModule(ctx);
  }

  if (strcmp(module_name, "process") == 0) {
    return jsrt_get_process_module(ctx);
  }

  if (strcmp(module_name, "ffi") == 0) {
    return JSRT_CreateFFIModule(ctx);
  }

  return jsrt_module_throw_error(ctx, JSRT_MODULE_NOT_FOUND, "Unknown jsrt module: %s", module_name);
}

/**
 * Load node: module
 */
static JSValue load_node_module(JSContext* ctx, const char* module_name) {
#ifdef JSRT_NODE_COMPAT
  MODULE_Debug_Loader("Loading node module: %s", module_name);

  // Check if it's a valid Node.js module
  if (!JSRT_IsNodeModule(module_name)) {
    return jsrt_module_throw_error(ctx, JSRT_MODULE_NOT_FOUND, "Unknown node module: %s", module_name);
  }

  // Load via the node module system
  return JSRT_LoadNodeModuleCommonJS(ctx, module_name);
#else
  (void)ctx;
  (void)module_name;
  return jsrt_module_throw_error(ctx, JSRT_MODULE_NOT_FOUND, "Node.js compatibility not enabled");
#endif
}

/**
 * Load builtin module
 */
JSValue jsrt_load_builtin_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* specifier) {
  if (!ctx || !loader || !specifier) {
    MODULE_Debug_Error("Invalid arguments to jsrt_load_builtin_module");
    return JS_EXCEPTION;
  }

  MODULE_Debug_Loader("=== Loading builtin module: %s ===", specifier);

  // Check cache first
  if (loader->enable_cache && loader->cache) {
    JSValue cached = jsrt_module_cache_get(loader->cache, specifier);
    if (!JS_IsUndefined(cached)) {
      MODULE_Debug_Loader("Cache HIT for builtin module: %s", specifier);
      return cached;
    }
  }

  // Extract protocol and module name
  const char* protocol = jsrt_get_builtin_protocol(specifier);
  if (!protocol) {
    return jsrt_module_throw_error(ctx, JSRT_MODULE_INVALID_SPECIFIER, "Invalid builtin specifier: %s", specifier);
  }

  char* module_name = jsrt_extract_builtin_name(specifier);
  if (!module_name) {
    return jsrt_module_throw_error(ctx, JSRT_MODULE_INVALID_SPECIFIER, "Failed to extract module name from: %s",
                                   specifier);
  }

  MODULE_Debug_Loader("Protocol: %s, Module: %s", protocol, module_name);

  // Load the appropriate builtin
  JSValue result = JS_UNDEFINED;

  if (strcmp(protocol, "jsrt") == 0) {
    result = load_jsrt_module(ctx, module_name);
  } else if (strcmp(protocol, "node") == 0) {
    result = load_node_module(ctx, module_name);
  } else {
    free(module_name);
    return jsrt_module_throw_error(ctx, JSRT_MODULE_INVALID_SPECIFIER, "Unknown builtin protocol: %s", protocol);
  }

  free(module_name);

  if (JS_IsException(result)) {
    return JS_EXCEPTION;
  }

  // Cache the result
  if (loader->enable_cache && loader->cache) {
    jsrt_module_cache_put(loader->cache, specifier, result);
  }

  MODULE_Debug_Loader("Successfully loaded builtin module: %s", specifier);
  return result;
}
