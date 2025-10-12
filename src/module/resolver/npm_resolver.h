/**
 * NPM Module Resolver
 *
 * Resolves bare module specifiers using Node.js node_modules algorithm.
 */

#ifndef JSRT_MODULE_NPM_RESOLVER_H
#define JSRT_MODULE_NPM_RESOLVER_H

#include <quickjs.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Find node_modules directory containing a package
 * Walks up directory tree looking for node_modules/package_name
 * @param start_dir Starting directory for search
 * @param package_name Package name to find
 * @return Full path to package directory, or NULL if not found
 * @note Caller must free returned string
 */
char* jsrt_find_node_modules(const char* start_dir, const char* package_name);

/**
 * Resolve package main entry point
 * Uses package.json "main"/"module" field or falls back to index.js
 * @param ctx JavaScript context (for package.json parsing)
 * @param package_dir Package directory path
 * @param is_esm Whether to prefer ES module entry points
 * @return Full path to package entry point, or NULL if not found
 * @note Caller must free returned string
 */
char* jsrt_resolve_package_main(JSContext* ctx, const char* package_dir, bool is_esm);

/**
 * Resolve an npm module specifier
 * Handles both simple packages (lodash) and subpath imports (lodash/array)
 * @param ctx JavaScript context (for package.json parsing)
 * @param module_name Module specifier (e.g., "lodash" or "lodash/array")
 * @param base_path Base path for resolution (requesting module's path)
 * @param is_esm Whether to prefer ES module entry points
 * @return Resolved module path, or NULL if not found
 * @note Caller must free returned string
 */
char* jsrt_resolve_npm_module(JSContext* ctx, const char* module_name, const char* base_path, bool is_esm);

/**
 * Resolve package exports field
 * @param ctx JavaScript context
 * @param package_dir Package directory path
 * @param subpath Subpath to resolve (e.g., "." or "./array")
 * @param is_esm Whether we're resolving for ESM
 * @return Resolved path, or NULL if not found
 * @note Caller must free returned string
 */
char* jsrt_resolve_package_exports(JSContext* ctx, const char* package_dir, const char* subpath, bool is_esm);

/**
 * Resolve package imports field (#internal/utils)
 * @param ctx JavaScript context
 * @param import_name Import name (e.g., "#internal/utils")
 * @param requesting_module_path Path of module making the import
 * @return Resolved path, or NULL if not found
 * @note Caller must free returned string
 */
char* jsrt_resolve_package_import(JSContext* ctx, const char* import_name, const char* requesting_module_path);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_NPM_RESOLVER_H
