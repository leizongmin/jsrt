/**
 * Path Utility Functions
 *
 * Cross-platform path manipulation utilities for module resolution.
 * Handles Windows and Unix path separators, normalization, and path joining.
 */

#ifndef JSRT_MODULE_PATH_UTIL_H
#define JSRT_MODULE_PATH_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==== Path Separator Utilities ====

/**
 * Check if a character is a path separator (/ or \)
 * @param c Character to check
 * @return true if c is / or \, false otherwise
 */
bool jsrt_is_path_separator(char c);

/**
 * Find the last path separator in a string
 * @param path Path string to search
 * @return Pointer to last separator, or NULL if none found
 */
char* jsrt_find_last_separator(const char* path);

// ==== Path Manipulation ====

/**
 * Normalize path separators to be platform-specific
 * Converts all separators to \ on Windows, / on Unix
 * @param path Path to normalize
 * @return Newly allocated normalized path (caller must free), or NULL on error
 */
char* jsrt_normalize_path(const char* path);

/**
 * Get the parent directory of a path
 * @param path Path to get parent of
 * @return Newly allocated parent path (caller must free), or NULL on error
 * @note Returns "." if no parent exists
 */
char* jsrt_get_parent_directory(const char* path);

/**
 * Join two path components with appropriate separator
 * @param dir Directory path
 * @param file File or subdirectory name
 * @return Newly allocated joined path (caller must free), or NULL on error
 */
char* jsrt_path_join(const char* dir, const char* file);

// ==== Path Type Checking ====

/**
 * Check if a path is absolute
 * @param path Path to check
 * @return true if absolute, false otherwise
 * @note Windows: C:\path or \\UNC or \path
 * @note Unix: /path
 */
bool jsrt_is_absolute_path(const char* path);

/**
 * Check if a path is relative (starts with ./ or ../)
 * @param path Path to check
 * @return true if relative, false otherwise
 */
bool jsrt_is_relative_path(const char* path);

/**
 * Resolve a relative path against a base path
 * @param base_path Base path (file or directory)
 * @param relative_path Relative path to resolve
 * @return Newly allocated resolved path (caller must free), or NULL on error
 */
char* jsrt_resolve_relative_path(const char* base_path, const char* relative_path);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_PATH_UTIL_H
