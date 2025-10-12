/**
 * Package.json Parser
 *
 * Parses and caches package.json files for module resolution.
 */

#ifndef JSRT_MODULE_PACKAGE_JSON_H
#define JSRT_MODULE_PACKAGE_JSON_H

#include <quickjs.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parsed package.json structure
 */
typedef struct {
  char* type;       // "module" or "commonjs"
  char* main;       // Main entry point (string)
  char* module;     // ES module entry point (string)
  JSValue exports;  // Exports map (JSON object or string)
  JSValue imports;  // Imports map (JSON object)
  char* dir_path;   // Directory containing package.json
  JSContext* ctx;   // Context for JSValue lifecycle
} JSRT_PackageJson;

/**
 * Parse package.json from a directory
 * Walks up directory tree to find nearest package.json
 * @param ctx JavaScript context
 * @param dir_path Starting directory path
 * @return Parsed package.json (caller must free with jsrt_package_json_free)
 * @note Returns NULL if no package.json found or parse error
 */
JSRT_PackageJson* jsrt_parse_package_json(JSContext* ctx, const char* dir_path);

/**
 * Parse package.json from exact path
 * @param ctx JavaScript context
 * @param json_path Exact path to package.json file
 * @return Parsed package.json (caller must free with jsrt_package_json_free)
 * @note Returns NULL if file doesn't exist or parse error
 */
JSRT_PackageJson* jsrt_parse_package_json_file(JSContext* ctx, const char* json_path);

/**
 * Free a package.json structure
 * @param pkg Package.json to free
 */
void jsrt_package_json_free(JSRT_PackageJson* pkg);

/**
 * Check if package is ES module (type === "module")
 * @param pkg Package.json structure
 * @return true if ES module, false if CommonJS or unknown
 */
bool jsrt_package_is_esm(const JSRT_PackageJson* pkg);

/**
 * Get main entry point from package.json
 * Prefers "module" over "main" for ESM, "main" for CommonJS
 * @param pkg Package.json structure
 * @param is_esm Whether we're resolving for ESM
 * @return Entry point path relative to package root, or NULL if not found
 * @note Caller must free returned string
 */
char* jsrt_package_get_main(const JSRT_PackageJson* pkg, bool is_esm);

/**
 * Resolve exports field for a subpath
 * @param pkg Package.json structure
 * @param subpath Subpath to resolve (e.g., "." or "./array")
 * @param is_esm Whether we're resolving for ESM (for conditional exports)
 * @return Resolved path relative to package root, or NULL if not found
 * @note Caller must free returned string
 */
char* jsrt_package_resolve_exports(const JSRT_PackageJson* pkg, const char* subpath, bool is_esm);

/**
 * Resolve imports field for a package import
 * @param pkg Package.json structure
 * @param import_name Import name (e.g., "#internal/utils")
 * @return Resolved path relative to package root, or NULL if not found
 * @note Caller must free returned string
 */
char* jsrt_package_resolve_imports(const JSRT_PackageJson* pkg, const char* import_name);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_PACKAGE_JSON_H
