/**
 * JSON Module Loader
 *
 * Loads JSON files as JavaScript objects.
 * Follows Node.js behavior for JSON module loading.
 */

#ifndef JSRT_MODULE_JSON_LOADER_H
#define JSRT_MODULE_JSON_LOADER_H

#include <quickjs.h>
#include "../core/module_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load a JSON file as a JavaScript object
 *
 * This function:
 * 1. Loads JSON file content via protocol dispatcher
 * 2. Parses JSON using JS_ParseJSON
 * 3. Returns the parsed object
 * 4. Caches result in unified cache
 *
 * @param ctx JavaScript context
 * @param loader Module loader instance
 * @param resolved_path Absolute path to JSON file (already resolved)
 * @param specifier Original module specifier (for error messages)
 * @return Parsed JSON object as JSValue, or JS_EXCEPTION on error
 *
 * Error cases:
 * - File not found
 * - Invalid JSON syntax
 * - File read errors
 */
JSValue jsrt_load_json_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* resolved_path,
                              const char* specifier);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_JSON_LOADER_H
