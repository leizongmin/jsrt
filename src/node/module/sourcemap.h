#ifndef __JSRT_NODE_MODULE_SOURCEMAP_H__
#define __JSRT_NODE_MODULE_SOURCEMAP_H__

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @file sourcemap.h
 * @brief Source Map v3 implementation for node:module
 *
 * Provides source map parsing, lookup, and the SourceMap class for
 * mapping generated code positions to original source positions.
 *
 * Implements:
 * - Source Map v3 parsing (VLQ decoding)
 * - SourceMap JavaScript class
 * - module.findSourceMap() lookup
 * - Error stack trace transformation
 */

/**
 * Source map mapping entry
 * Represents a single position mapping from generated to original code
 */
typedef struct {
  int32_t generated_line;    // 0-indexed line in generated file
  int32_t generated_column;  // 0-indexed column in generated file
  int32_t source_index;      // Index into sources array (-1 if not mapped)
  int32_t original_line;     // 0-indexed line in original file
  int32_t original_column;   // 0-indexed column in original file
  int32_t name_index;        // Index into names array (-1 if not mapped)
} JSRT_SourceMapping;

/**
 * Source map data structure
 * Stores parsed Source Map v3 data
 */
typedef struct {
  char* version;           // Source map version (should be "3")
  char* file;              // Generated file name (optional)
  char* source_root;       // Source root path (optional)
  char** sources;          // Array of source file names
  size_t sources_count;    // Number of source files
  char** sources_content;  // Inlined source content (optional)
  size_t sources_content_count;
  char** names;        // Array of symbol names
  size_t names_count;  // Number of symbol names
  char* mappings;      // Original VLQ-encoded mappings string

  // Decoded mappings (sorted by generated_line, generated_column)
  JSRT_SourceMapping* decoded_mappings;
  size_t decoded_mappings_count;

  JSValue payload;  // Original JSON payload (kept alive)
} JSRT_SourceMap;

/**
 * Source map cache entry
 * Maps file paths to parsed source maps
 */
typedef struct JSRT_SourceMapCacheEntry {
  char* path;                             // File path (key)
  JSRT_SourceMap* source_map;             // Parsed source map
  struct JSRT_SourceMapCacheEntry* next;  // Next in linked list (for collisions)
} JSRT_SourceMapCacheEntry;

/**
 * Source map cache
 * Global cache for parsed source maps
 */
typedef struct JSRT_SourceMapCache {
  JSRT_SourceMapCacheEntry** buckets;  // Hash table buckets
  size_t capacity;                     // Number of buckets
  size_t size;                         // Number of entries
  bool enabled;                        // Source maps enabled flag
} JSRT_SourceMapCache;

// ============================================================================
// Source Map Lifecycle
// ============================================================================

/**
 * Create a new source map from parsed data
 * @param ctx QuickJS context
 * @return Allocated source map or NULL on error
 */
JSRT_SourceMap* jsrt_source_map_new(JSContext* ctx);

/**
 * Free a source map and all its resources
 * @param rt QuickJS runtime
 * @param map Source map to free
 */
void jsrt_source_map_free(JSRuntime* rt, JSRT_SourceMap* map);

// ============================================================================
// Source Map Cache Management
// ============================================================================

/**
 * Initialize source map cache
 * @param ctx QuickJS context
 * @param capacity Initial capacity (number of buckets)
 * @return Initialized cache or NULL on error
 */
JSRT_SourceMapCache* jsrt_source_map_cache_init(JSContext* ctx, size_t capacity);

/**
 * Free source map cache and all cached maps
 * @param rt QuickJS runtime
 * @param cache Cache to free
 */
void jsrt_source_map_cache_free(JSRuntime* rt, JSRT_SourceMapCache* cache);

/**
 * Lookup source map in cache
 * @param cache Source map cache
 * @param path File path
 * @return Cached source map or NULL if not found
 */
JSRT_SourceMap* jsrt_source_map_cache_lookup(JSRT_SourceMapCache* cache, const char* path);

/**
 * Store source map in cache
 * @param ctx QuickJS context
 * @param cache Source map cache
 * @param path File path (key)
 * @param map Source map to cache
 * @return true on success, false on error
 */
bool jsrt_source_map_cache_store(JSContext* ctx, JSRT_SourceMapCache* cache, const char* path, JSRT_SourceMap* map);

/**
 * Enable or disable source map support
 * @param cache Source map cache
 * @param enabled Enable flag
 */
void jsrt_source_map_cache_set_enabled(JSRT_SourceMapCache* cache, bool enabled);

/**
 * Check if source maps are enabled
 * @param cache Source map cache
 * @return true if enabled, false otherwise
 */
bool jsrt_source_map_cache_is_enabled(JSRT_SourceMapCache* cache);

// ============================================================================
// Source Map Parsing (Phase 2 Task 2.2)
// ============================================================================

/**
 * Parse Source Map v3 JSON payload
 * @param ctx QuickJS context
 * @param payload Source map JSON object
 * @return Parsed source map or NULL on error
 */
JSRT_SourceMap* jsrt_source_map_parse(JSContext* ctx, JSValue payload);

// ============================================================================
// SourceMap JavaScript Class (Phase 2 Task 2.3)
// ============================================================================

/**
 * Initialize SourceMap class in module context
 * @param ctx QuickJS context
 * @param module_obj node:module namespace object
 * @return true on success, false on error
 */
bool jsrt_source_map_class_init(JSContext* ctx, JSValue module_obj);

/**
 * Create SourceMap instance from parsed data
 * @param ctx QuickJS context
 * @param map Source map data
 * @return SourceMap instance or JS_EXCEPTION
 */
JSValue jsrt_source_map_create_instance(JSContext* ctx, JSRT_SourceMap* map);

// ============================================================================
// Source Map Lookup (Phase 2 Task 2.6)
// ============================================================================

/**
 * Find source map for a given file path
 * Searches for inline source maps and external .map files
 *
 * @param ctx QuickJS context
 * @param cache Source map cache
 * @param path File path
 * @return SourceMap instance or JS_UNDEFINED if not found
 */
JSValue jsrt_find_source_map(JSContext* ctx, JSRT_SourceMapCache* cache, const char* path);

#endif  // __JSRT_NODE_MODULE_SOURCEMAP_H__
