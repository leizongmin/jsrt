/**
 * ES Module Loader
 *
 * Loads and executes ECMAScript modules with proper support for:
 * - import/export statements
 * - import.meta.url
 * - import.meta.resolve()
 * - Top-level await
 * - Dynamic import()
 */

#ifndef JSRT_MODULE_ESM_LOADER_H
#define JSRT_MODULE_ESM_LOADER_H

#include <quickjs.h>
#include "../core/module_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load and execute an ES module
 *
 * This function:
 * 1. Loads module content via protocol dispatcher
 * 2. Sets up import.meta.url (file:// URL of module)
 * 3. Sets up import.meta.resolve() function
 * 4. Compiles code as ES module (JS_EVAL_TYPE_MODULE)
 * 5. Handles top-level await if present
 * 6. Extracts JSModuleDef* from evaluation
 * 7. Caches module definition
 *
 * Supports:
 * - Named and default exports
 * - import.meta.url
 * - import.meta.resolve()
 * - Top-level await
 * - Dynamic import()
 *
 * @param ctx JavaScript context
 * @param loader Module loader instance
 * @param resolved_path Absolute path to module file (already resolved)
 * @param specifier Original module specifier (for error messages)
 * @return JSModuleDef* for the module, or NULL on error
 *
 * Error cases:
 * - File not found
 * - Syntax errors in module code
 * - Runtime errors during module initialization
 */
JSModuleDef* jsrt_load_esm_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* resolved_path,
                                  const char* specifier);

/**
 * Set up import.meta for an ES module
 *
 * Creates import.meta object with:
 * - url: file:// URL of the module
 * - resolve: Function to resolve specifiers relative to this module
 *
 * @param ctx JavaScript context
 * @param module Module definition
 * @param loader Module loader instance
 * @param resolved_path Module's absolute path
 * @return 0 on success, -1 on error
 */
int jsrt_setup_import_meta(JSContext* ctx, JSModuleDef* module, JSRT_ModuleLoader* loader, const char* resolved_path);

/**
 * Create file:// URL from file path
 *
 * Converts an absolute file path to a file:// URL.
 * Handles both Unix and Windows paths.
 *
 * @param path Absolute file path
 * @return Allocated file:// URL string, or NULL on error (caller must free)
 *
 * Examples:
 *   /home/user/file.js -> file:///home/user/file.js
 *   C:\Users\file.js -> file:///C:/Users/file.js
 */
char* jsrt_path_to_file_url(const char* path);

/**
 * Create import.meta.resolve() function
 *
 * Creates a function that resolves specifiers relative to the current module.
 * Signature: resolve(specifier) -> URL string
 *
 * @param ctx JavaScript context
 * @param loader Module loader instance
 * @param module_path Module's absolute path (for relative resolution)
 * @return resolve function as JSValue, or JS_EXCEPTION on error
 */
JSValue jsrt_create_import_meta_resolve(JSContext* ctx, JSRT_ModuleLoader* loader, const char* module_path);

/**
 * Get exports from an ES module
 *
 * Evaluates the module and returns an object with all exports.
 * For use in CommonJS interop (when requiring an ES module).
 *
 * @param ctx JavaScript context
 * @param module Module definition
 * @return Object with module exports, or JS_EXCEPTION on error
 */
JSValue jsrt_get_esm_exports(JSContext* ctx, JSModuleDef* module);

/**
 * QuickJS module normalize callback (bridge function)
 *
 * This callback is registered with QuickJS via JS_SetModuleLoaderFunc.
 * It resolves module specifiers to absolute paths using the new resolver.
 *
 * @param ctx JavaScript context
 * @param module_base_name Base module path (importer)
 * @param module_name Module specifier to resolve
 * @param opaque User data (JSRT_Runtime*)
 * @return Resolved path (caller must free), or NULL on error
 */
char* jsrt_esm_normalize_callback(JSContext* ctx, const char* module_base_name, const char* module_name, void* opaque);

/**
 * QuickJS module loader callback (bridge function)
 *
 * This callback is registered with QuickJS via JS_SetModuleLoaderFunc.
 * It loads ES modules using the new loader system.
 *
 * @param ctx JavaScript context
 * @param module_name Normalized module path
 * @param opaque User data (JSRT_Runtime*)
 * @return Compiled module definition, or NULL on error
 */
JSModuleDef* jsrt_esm_loader_callback(JSContext* ctx, const char* module_name, void* opaque);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_ESM_LOADER_H
