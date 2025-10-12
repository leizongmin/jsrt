/**
 * Builtin Module Loader
 *
 * Loads jsrt: and node: builtin modules.
 * Integrates with existing node_modules.c registry.
 */

#ifndef JSRT_MODULE_BUILTIN_LOADER_H
#define JSRT_MODULE_BUILTIN_LOADER_H

#include <quickjs.h>
#include <stdbool.h>
#include "../core/module_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load a builtin module (jsrt: or node: prefix)
 *
 * This function:
 * 1. Parses specifier to extract module name
 *    - jsrt:fs -> "fs"
 *    - node:crypto -> "crypto"
 * 2. Looks up in node_modules.c registry
 * 3. Initializes native module if not already done
 * 4. Returns module exports
 * 5. Caches result
 *
 * Supports:
 * - jsrt: prefix for jsrt-specific modules
 * - node: prefix for Node.js compatibility modules
 * - Lazy initialization of native modules
 * - Module dependency resolution
 *
 * @param ctx JavaScript context
 * @param loader Module loader instance
 * @param specifier Module specifier with prefix (e.g., "jsrt:fs", "node:crypto")
 * @return Module exports as JSValue, or JS_EXCEPTION on error
 *
 * Error cases:
 * - Invalid specifier format
 * - Unknown builtin module
 * - Module initialization failure
 */
JSValue jsrt_load_builtin_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* specifier);

/**
 * Check if a specifier is a builtin module
 *
 * @param specifier Module specifier to check
 * @return true if specifier has jsrt: or node: prefix, false otherwise
 */
bool jsrt_is_builtin_specifier(const char* specifier);

/**
 * Extract module name from builtin specifier
 *
 * Examples:
 *   jsrt:fs -> fs
 *   node:crypto -> crypto
 *
 * @param specifier Builtin specifier
 * @return Allocated module name (caller must free), or NULL on error
 */
char* jsrt_extract_builtin_name(const char* specifier);

/**
 * Get builtin protocol from specifier
 *
 * @param specifier Builtin specifier
 * @return "jsrt" or "node", or NULL if not a builtin specifier
 */
const char* jsrt_get_builtin_protocol(const char* specifier);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_BUILTIN_LOADER_H
