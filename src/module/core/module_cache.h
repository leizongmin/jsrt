#ifndef __JSRT_MODULE_CACHE_H__
#define __JSRT_MODULE_CACHE_H__

/**
 * Module Cache
 *
 * Hash map-based cache for storing loaded module exports.
 * Provides O(1) lookup, insertion, and deletion.
 */

#include <quickjs.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Cache entry structure
 */
typedef struct JSRT_ModuleCacheEntry {
  char* key;                           // Module key (normalized path or URL)
  JSValue exports;                     // Cached module exports (JS value)
  uint64_t load_time;                  // Timestamp when module was loaded
  uint64_t access_count;               // Number of times this entry was accessed
  uint64_t last_access;                // Timestamp of last access
  struct JSRT_ModuleCacheEntry* next;  // For collision chaining
} JSRT_ModuleCacheEntry;

/**
 * Module cache structure
 */
typedef struct JSRT_ModuleCache {
  JSContext* ctx;                   // Associated JavaScript context
  JSRT_ModuleCacheEntry** buckets;  // Hash table buckets
  size_t capacity;                  // Total number of buckets
  size_t size;                      // Current number of entries
  size_t max_size;                  // Maximum number of entries allowed

  // Statistics
  uint64_t hits;        // Number of cache hits
  uint64_t misses;      // Number of cache misses
  uint64_t evictions;   // Number of cache evictions
  uint64_t collisions;  // Number of hash collisions

  // Memory tracking
  size_t memory_used;  // Approximate memory used (bytes)

} JSRT_ModuleCache;

/**
 * Create a new module cache
 *
 * @param ctx The JavaScript context
 * @param capacity Initial capacity (number of buckets)
 * @return Pointer to newly created cache, or NULL on failure
 */
JSRT_ModuleCache* jsrt_module_cache_create(JSContext* ctx, size_t capacity);

/**
 * Get module exports from cache
 *
 * @param cache The cache to search
 * @param key The module key (normalized path or URL)
 * @return Cached JSValue exports, or JS_UNDEFINED if not found
 *         Caller should NOT free the returned value (it's still cached)
 */
JSValue jsrt_module_cache_get(JSRT_ModuleCache* cache, const char* key);

/**
 * Store module exports in cache
 *
 * @param cache The cache to store in
 * @param key The module key (normalized path or URL)
 * @param exports The module exports to cache (will be duplicated)
 * @return 0 on success, -1 on failure
 */
int jsrt_module_cache_put(JSRT_ModuleCache* cache, const char* key, JSValue exports);

/**
 * Check if a module exists in cache
 *
 * @param cache The cache to check
 * @param key The module key
 * @return 1 if exists, 0 if not
 */
int jsrt_module_cache_has(JSRT_ModuleCache* cache, const char* key);

/**
 * Remove a module from cache
 *
 * @param cache The cache to remove from
 * @param key The module key
 * @return 0 on success, -1 if not found
 */
int jsrt_module_cache_remove(JSRT_ModuleCache* cache, const char* key);

/**
 * Clear all entries from cache
 *
 * @param cache The cache to clear
 */
void jsrt_module_cache_clear(JSRT_ModuleCache* cache);

/**
 * Free cache and all entries
 *
 * @param cache The cache to free (can be NULL)
 */
void jsrt_module_cache_free(JSRT_ModuleCache* cache);

/**
 * Get cache statistics
 *
 * @param cache The cache
 * @param hits Output: number of hits
 * @param misses Output: number of misses
 * @param size Output: current number of entries
 * @param memory_used Output: approximate memory used in bytes
 */
void jsrt_module_cache_get_stats(JSRT_ModuleCache* cache, uint64_t* hits, uint64_t* misses, size_t* size,
                                 size_t* memory_used);

/**
 * Reset cache statistics
 *
 * @param cache The cache
 */
void jsrt_module_cache_reset_stats(JSRT_ModuleCache* cache);

#endif  // __JSRT_MODULE_CACHE_H__
