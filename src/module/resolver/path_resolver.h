/**
 * Unified Path Resolver
 *
 * Main orchestrator for module path resolution.
 * Routes to appropriate resolver based on specifier type.
 */

#ifndef JSRT_MODULE_PATH_RESOLVER_H
#define JSRT_MODULE_PATH_RESOLVER_H

#include <quickjs.h>
#include <stdbool.h>

#include "specifier.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolved path information
 */
typedef struct {
  char* resolved_path;      // Final resolved absolute path/URL
  bool is_url;              // Is this a URL (http://, https://)?
  bool is_builtin;          // Is this jsrt:/node: builtin?
  char* protocol;           // Protocol (file, http, https, jsrt, node, etc.)
  JSRT_SpecifierType type;  // Specifier type
} JSRT_ResolvedPath;

/**
 * Resolve a module specifier to an absolute path or URL
 * This is the main entry point for path resolution
 * @param ctx JavaScript context (for package.json parsing)
 * @param specifier Module specifier string
 * @param base_path Base path for relative resolution (requesting module's path)
 * @return Resolved path info (caller must free with jsrt_resolved_path_free)
 * @note Returns NULL if resolution fails
 */
JSRT_ResolvedPath* jsrt_resolve_path(JSContext* ctx, const char* specifier, const char* base_path, bool is_esm);

/**
 * Free a resolved path structure
 * @param path Resolved path to free
 */
void jsrt_resolved_path_free(JSRT_ResolvedPath* path);

/**
 * Try file extensions (.js, .mjs, .cjs) on a base path
 * @param base_path Base path without extension
 * @return Path with extension if file exists, or NULL if none found
 * @note Caller must free returned string
 */
char* jsrt_try_extensions(const char* base_path);

/**
 * Try directory index files (index.js, index.mjs, index.cjs)
 * @param dir_path Directory path
 * @return Path to index file if exists, or NULL if none found
 * @note Caller must free returned string
 */
char* jsrt_try_directory_index(const char* dir_path);

/**
 * Validate and normalize a URL specifier
 * @param url URL to validate
 * @return Validated URL, or NULL if invalid
 * @note Caller must free returned string
 */
char* jsrt_validate_url(const char* url);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_PATH_RESOLVER_H
