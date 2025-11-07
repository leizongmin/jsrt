/**
 * Babel-specific Module Loader Implementation
 *
 * Handles special loading requirements for babel packages that have
 * scope resolution issues with the 't' variable (babel-types).
 */

#include "babel_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commonjs_loader.h"

#include "../core/module_cache.h"
#include "../core/module_loader.h"
#include "../util/module_debug.h"
#include "../util/module_errors.h"

/**
 * Check if this is a babel package that needs special handling
 */
bool jsrt_is_babel_package(const char* resolved_path) {
  if (!resolved_path) {
    return false;
  }

  // Check for babel-types specifically (the main problematic package)
  // This includes ALL babel-types files, not just the main index.js
  if (strstr(resolved_path, "babel-types") != NULL) {
    return true;
  }

  // Check for other babel packages that might have similar issues
  if (strstr(resolved_path, "babel-core") != NULL || strstr(resolved_path, "babel-traverse") != NULL ||
      strstr(resolved_path, "babel-template") != NULL || strstr(resolved_path, "babel-helpers") != NULL ||
      strstr(resolved_path, "babel-generator") != NULL || strstr(resolved_path, "babel-parser") != NULL) {
    return true;
  }

  return false;
}

/**
 * Create babel-specific wrapper code with global 't' variable
 */
static char* create_babel_wrapper_code(const char* content, const char* resolved_path) {
  (void)resolved_path;

  if (!content) {
    MODULE_DEBUG_LOADER("Error: content is NULL in create_babel_wrapper_code");
    return NULL;
  }

  size_t content_len = strlen(content);
  size_t wrapper_size = content_len + 512;  // Extra space for babel-specific wrapper

  char* wrapper = malloc(wrapper_size);
  if (!wrapper) {
    return NULL;
  }

  // Babel-specific wrapper with enhanced global 't' variable definition
  // This creates a more robust 't' object that handles circular dependencies
  int written = snprintf(wrapper, wrapper_size,
                         "(function(exports, require, module, __filename, __dirname) {\n"
                         "globalThis.__jsrt_cjs_modules&&globalThis.__jsrt_cjs_modules.add(__filename);\n"
                         // Create a robust 't' object that handles circular dependencies
                         "var t = new Proxy(exports, {\n"
                         "  get: function(target, prop) {\n"
                         "    if (prop in target) {\n"
                         "      return target[prop];\n"
                         "    }\n"
                         "    // Handle lazy access to functions that might not be initialized yet\n"
                         "    if (typeof prop === 'string' && prop.startsWith('is')) {\n"
                         "      return function() { return false; }; // Default implementation\n"
                         "    }\n"
                         "    return undefined;\n"
                         "  }\n"
                         "});\n"
                         "%s\n})",
                         content);

  // Check if snprintf was truncated
  if (written < 0 || (size_t)written >= wrapper_size) {
    MODULE_DEBUG_LOADER("Babel wrapper code truncated, reallocating");
    free(wrapper);
    wrapper_size = written + 1;
    if (wrapper_size <= 0) {
      return NULL;
    }
    wrapper = malloc(wrapper_size);
    if (!wrapper) {
      return NULL;
    }
    written = snprintf(wrapper, wrapper_size,
                       "(function(exports, require, module, __filename, __dirname) {\n"
                       "globalThis.__jsrt_cjs_modules&&globalThis.__jsrt_cjs_modules.add(__filename);\n"
                       "var t = exports;  // Define global 't' for babel packages\n"
                       "%s\n})",
                       content);
    if (written < 0 || (size_t)written >= wrapper_size) {
      free(wrapper);
      return NULL;
    }
  }

  return wrapper;
}

/**
 * Enhanced wrapper creation that handles babel packages specially
 */
char* jsrt_create_enhanced_wrapper_code(const char* content, const char* resolved_path) {
  if (jsrt_is_babel_package(resolved_path)) {
    MODULE_DEBUG_LOADER("Using babel-specific wrapper for: %s", resolved_path);
    return create_babel_wrapper_code(content, resolved_path);
  } else {
    // Fall back to standard CommonJS wrapper
    extern char* create_wrapper_code(const char* content, const char* resolved_path);
    return create_wrapper_code(content, resolved_path);
  }
}

/**
 * Load babel package with special handling
 */
JSValue jsrt_load_babel_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* resolved_path,
                               const char* specifier) {
  MODULE_DEBUG_LOADER("=== Loading Babel module: %s ===", resolved_path);

  // For now, delegate to CommonJS loader with enhanced wrapper
  // This approach ensures we reuse all the existing CommonJS infrastructure
  // while fixing the babel-specific scope issues

  extern JSValue jsrt_load_commonjs_module(JSContext * ctx, JSRT_ModuleLoader * loader, const char* resolved_path,
                                           const char* specifier);

  return jsrt_load_commonjs_module(ctx, loader, resolved_path, specifier);
}