#ifndef __JSRT_MODULE_LOADER_H__
#define __JSRT_MODULE_LOADER_H__

/**
 * Module Loader Core
 *
 * Main entry point for loading modules. Coordinates cache, resolver,
 * detector, and protocol handlers to load and execute modules.
 */

#include <quickjs.h>
#include "module_context.h"

/**
 * Load a module
 *
 * Main dispatcher function that:
 * 1. Checks cache for already-loaded module
 * 2. Resolves module specifier to absolute path/URL
 * 3. Detects module type (ESM, CommonJS, builtin, etc.)
 * 4. Selects appropriate protocol handler (file, http, data, etc.)
 * 5. Loads, compiles, and executes the module
 * 6. Caches the result
 *
 * @param loader The module loader instance
 * @param specifier The module specifier (e.g., "./foo.js", "lodash", "https://...")
 * @param base_path The base path for relative resolution (can be NULL for absolute specifiers)
 * @return Module exports as JSValue, or JS_EXCEPTION on error
 */
JSValue jsrt_load_module(JSRT_ModuleLoader* loader, const char* specifier, const char* base_path);

/**
 * Load a module (version that takes JSContext instead of loader)
 *
 * Convenience function that gets the loader from the context.
 * If no loader is set on the context, creates a temporary one.
 *
 * @param ctx The JavaScript context
 * @param specifier The module specifier
 * @param base_path The base path for relative resolution
 * @return Module exports as JSValue, or JS_EXCEPTION on error
 */
JSValue jsrt_load_module_ctx(JSContext* ctx, const char* specifier, const char* base_path);

/**
 * Preload a module into cache
 *
 * Loads a module and caches it, but doesn't return the exports.
 * Useful for warming up the cache.
 *
 * @param loader The module loader instance
 * @param specifier The module specifier
 * @param base_path The base path for relative resolution
 * @return 0 on success, -1 on failure
 */
int jsrt_preload_module(JSRT_ModuleLoader* loader, const char* specifier, const char* base_path);

/**
 * Clear a module from cache
 *
 * Removes a module from the cache, forcing it to be reloaded on next access.
 * Useful for hot-reloading during development.
 *
 * @param loader The module loader instance
 * @param specifier The module specifier
 * @return 0 on success, -1 if module not in cache
 */
int jsrt_invalidate_module(JSRT_ModuleLoader* loader, const char* specifier);

/**
 * Clear all modules from cache
 *
 * @param loader The module loader instance
 */
void jsrt_invalidate_all_modules(JSRT_ModuleLoader* loader);

#endif  // __JSRT_MODULE_LOADER_H__
