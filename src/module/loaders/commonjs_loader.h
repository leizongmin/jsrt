/**
 * CommonJS Module Loader
 *
 * Loads and executes CommonJS modules with proper environment:
 * - module.exports object
 * - exports shorthand
 * - require() function
 * - __filename and __dirname
 * - Circular dependency detection
 */

#ifndef JSRT_MODULE_COMMONJS_LOADER_H
#define JSRT_MODULE_COMMONJS_LOADER_H

#include <quickjs.h>
#include <stdbool.h>
#include "../core/module_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load and execute a CommonJS module
 *
 * This function:
 * 1. Loads module content via protocol dispatcher
 * 2. Creates module object with exports
 * 3. Wraps code in CommonJS environment (exports, require, module, __filename, __dirname)
 * 4. Executes the wrapped code
 * 5. Extracts module.exports (handles reassignment)
 * 6. Caches result in unified cache
 *
 * Supports:
 * - module.exports = ... (complete replacement)
 * - exports.foo = ... (property assignment)
 * - Circular dependency detection
 * - Proper __filename/__dirname on all platforms
 *
 * @param ctx JavaScript context
 * @param loader Module loader instance
 * @param resolved_path Absolute path to module file (already resolved)
 * @param specifier Original module specifier (for error messages)
 * @return Module exports as JSValue, or JS_EXCEPTION on error
 *
 * Error cases:
 * - File not found
 * - Syntax errors in module code
 * - Runtime errors during module execution
 * - Circular dependency errors
 */
JSValue jsrt_load_commonjs_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* resolved_path,
                                  const char* specifier);

/**
 * Create require() function bound to a specific module path
 *
 * This creates a require() function that resolves relative paths
 * relative to the given module_path.
 *
 * @param ctx JavaScript context
 * @param loader Module loader instance
 * @param module_path Path of the requiring module (for relative resolution)
 * @return require function as JSValue, or JS_EXCEPTION on error
 */
JSValue jsrt_create_require_function(JSContext* ctx, JSRT_ModuleLoader* loader, const char* module_path);

/**
 * Check if a module is currently being loaded (circular dependency check)
 *
 * @param loader Module loader instance
 * @param resolved_path Module path to check
 * @return true if module is in loading stack, false otherwise
 */
bool jsrt_is_loading_commonjs(JSRT_ModuleLoader* loader, const char* resolved_path);

/**
 * Mark a module as loading (push to loading stack)
 *
 * @param loader Module loader instance
 * @param resolved_path Module path to mark
 * @return 0 on success, -1 on error
 */
int jsrt_push_loading_commonjs(JSRT_ModuleLoader* loader, const char* resolved_path);

/**
 * Mark a module as done loading (pop from loading stack)
 *
 * @param loader Module loader instance
 * @return 0 on success, -1 on error
 */
int jsrt_pop_loading_commonjs(JSRT_ModuleLoader* loader);

/**
 * Create CommonJS wrapper code
 *
 * @param content Module source content
 * @param resolved_path Module file path
 * @return Allocated wrapper code string (caller must free), or NULL on error
 */
char* create_wrapper_code(const char* content, const char* resolved_path);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_COMMONJS_LOADER_H
