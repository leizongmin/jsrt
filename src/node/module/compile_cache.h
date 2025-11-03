#ifndef __JSRT_NODE_MODULE_COMPILE_CACHE_H__
#define __JSRT_NODE_MODULE_COMPILE_CACHE_H__

#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * @file compile_cache.h
 * @brief Bytecode compilation cache for faster module loading
 *
 * Implements a disk-based cache for compiled JavaScript modules to improve
 * startup performance by avoiding repeated compilation of unchanged files.
 *
 * Features:
 * - Persistent bytecode cache (~/.jsrt/compile-cache/)
 * - Version-aware cache invalidation (jsrt + QuickJS versions)
 * - Modification time validation
 * - Portable mode (content-based hashing)
 * - Atomic writes (temp file + rename)
 * - Graceful error handling
 */

/**
 * Cache status codes for module.enableCompileCache()
 */
typedef enum {
  JSRT_COMPILE_CACHE_ENABLED = 0,          // Successfully enabled
  JSRT_COMPILE_CACHE_ALREADY_ENABLED = 1,  // Already enabled
  JSRT_COMPILE_CACHE_FAILED = -1,          // Failed to enable
  JSRT_COMPILE_CACHE_DISABLED = -2         // Disabled
} JSRT_CompileCacheStatus;

// Default cache size limit: 100MB
#define DEFAULT_CACHE_SIZE_LIMIT (100 * 1024 * 1024)

/**
 * Cache LRU entry for tracking access order
 */
typedef struct JSRT_CacheLRUEntry {
  char* key;                        // Cache entry key
  time_t access_time;               // Last access time
  size_t size;                      // Size of cache entry (bytes)
  struct JSRT_CacheLRUEntry* next;  // Next entry in LRU list
  struct JSRT_CacheLRUEntry* prev;  // Previous entry in LRU list
} JSRT_CacheLRUEntry;

/**
 * Cache configuration
 */
typedef struct JSRT_CompileCacheConfig {
  char* directory;      // Cache directory path
  bool portable;        // Use content-based hashing (slower, relocatable)
  bool allow_enable;    // Runtime/CLI toggle allowing enablement
  bool enabled;         // Cache enabled flag
  uint64_t hits;        // Cache hit count
  uint64_t misses;      // Cache miss count
  uint64_t writes;      // Cache write count
  uint64_t errors;      // Cache error count
  uint64_t evictions;   // Cache eviction count
  size_t size_limit;    // Maximum cache size in bytes
  size_t current_size;  // Current cache size in bytes

  // LRU tracking for eviction
  JSRT_CacheLRUEntry* lru_head;  // Most recently used
  JSRT_CacheLRUEntry* lru_tail;  // Least recently used
} JSRT_CompileCacheConfig;

/**
 * Cache entry metadata
 */
typedef struct {
  char* source_path;       // Absolute path to source file
  time_t mtime;            // File modification time
  char* jsrt_version;      // JSRT runtime version
  char* quickjs_version;   // QuickJS engine version
  size_t bytecode_size;    // Size of bytecode in bytes
  uint8_t* bytecode_hash;  // SHA-256 hash of bytecode (32 bytes)
} JSRT_CacheEntryMetadata;

// ============================================================================
// Cache Lifecycle
// ============================================================================

/**
 * Initialize compilation cache system
 * @param ctx QuickJS context
 * @return Cache configuration or NULL on error
 */
JSRT_CompileCacheConfig* jsrt_compile_cache_init(JSContext* ctx);

/**
 * Free compilation cache resources
 * @param config Cache configuration to free
 */
void jsrt_compile_cache_free(JSRT_CompileCacheConfig* config);

// ============================================================================
// Cache Configuration
// ============================================================================

/**
 * Enable compilation cache with specified directory
 * @param ctx QuickJS context
 * @param config Cache configuration
 * @param directory Cache directory (NULL for default ~/.jsrt/compile-cache/)
 * @param portable Use content-based hashing
 * @return Status code
 */
JSRT_CompileCacheStatus jsrt_compile_cache_enable(JSContext* ctx, JSRT_CompileCacheConfig* config,
                                                  const char* directory, bool portable);

/**
 * Disable compilation cache
 * @param config Cache configuration
 */
void jsrt_compile_cache_disable(JSRT_CompileCacheConfig* config);

/**
 * Allow or disallow compilation cache usage
 * @param config Cache configuration
 * @param allowed Whether enabling the cache is permitted
 */
void jsrt_compile_cache_set_allowed(JSRT_CompileCacheConfig* config, bool allowed);

/**
 * Get cache directory path
 * @param config Cache configuration
 * @return Cache directory or NULL if disabled
 */
const char* jsrt_compile_cache_get_directory(JSRT_CompileCacheConfig* config);

/**
 * Check if cache is enabled
 * @param config Cache configuration
 * @return true if enabled, false otherwise
 */
bool jsrt_compile_cache_is_enabled(JSRT_CompileCacheConfig* config);

// ============================================================================
// Cache Directory Management
// ============================================================================

/**
 * Create cache directory structure
 * Creates ~/.jsrt/compile-cache/ if it doesn't exist
 * @param directory Cache directory path
 * @return true on success, false on error
 */
bool jsrt_compile_cache_create_directory(const char* directory);

/**
 * Write version file to cache directory
 * Stores jsrt and QuickJS versions for cache validation
 * @param directory Cache directory path
 * @return true on success, false on error
 */
bool jsrt_compile_cache_write_version(const char* directory);

/**
 * Validate cache directory version
 * Checks if cached bytecode is compatible with current runtime
 * @param directory Cache directory path
 * @return true if versions match, false otherwise
 */
bool jsrt_compile_cache_validate_version(const char* directory);

// ============================================================================
// Cache Key Generation
// ============================================================================

/**
 * Generate cache key for a source file
 * In non-portable mode: hash(path + mtime)
 * In portable mode: hash(content)
 *
 * @param source_path Absolute path to source file
 * @param portable Use content-based hashing
 * @return Cache key (hex string, caller must free) or NULL on error
 */
char* jsrt_compile_cache_generate_key(const char* source_path, bool portable);

// ============================================================================
// Cache Lookup
// ============================================================================

/**
 * Lookup compiled bytecode in cache
 * @param ctx QuickJS context
 * @param config Cache configuration
 * @param source_path Absolute path to source file
 * @return Compiled module or JS_UNDEFINED if cache miss
 */
JSValue jsrt_compile_cache_lookup(JSContext* ctx, JSRT_CompileCacheConfig* config, const char* source_path);

// ============================================================================
// Cache Storage
// ============================================================================

/**
 * Store compiled bytecode in cache
 * @param ctx QuickJS context
 * @param config Cache configuration
 * @param source_path Absolute path to source file
 * @param bytecode Compiled bytecode
 * @return true on success, false on error
 */
bool jsrt_compile_cache_store(JSContext* ctx, JSRT_CompileCacheConfig* config, const char* source_path,
                              JSValue bytecode);

// ============================================================================
// Cache Statistics
// ============================================================================

/**
 * Get cache statistics
 * @param config Cache configuration
 * @param hits Output: cache hits
 * @param misses Output: cache misses
 * @param writes Output: cache writes
 * @param errors Output: cache errors
 * @param evictions Output: cache evictions
 * @param current_size Output: current cache size in bytes
 * @param size_limit Output: cache size limit in bytes
 */
void jsrt_compile_cache_get_stats(JSRT_CompileCacheConfig* config, uint64_t* hits, uint64_t* misses, uint64_t* writes,
                                  uint64_t* errors, uint64_t* evictions, size_t* current_size, size_t* size_limit);

/**
 * Flush pending cache entries to disk
 * @param config Cache configuration
 * @return Number of entries flushed
 */
int jsrt_compile_cache_flush(JSRT_CompileCacheConfig* config);

// ============================================================================
// Cache Maintenance
// ============================================================================

/**
 * Clear all compile cache entries
 * @param config Cache configuration
 * @return Number of entries removed
 */
int jsrt_compile_cache_clear(JSRT_CompileCacheConfig* config);

/**
 * Perform startup cleanup - remove stale entries
 * @param config Cache configuration
 * @return Number of entries removed
 */
int jsrt_compile_cache_startup_cleanup(JSRT_CompileCacheConfig* config);

/**
 * Check cache size and perform LRU eviction if needed
 * @param config Cache configuration
 * @param added_size Size of new entry being added
 * @return true if eviction was performed, false otherwise
 */
bool jsrt_compile_cache_maybe_evict(JSRT_CompileCacheConfig* config, size_t added_size);

/**
 * Update LRU tracking for a cache entry
 * @param config Cache configuration
 * @param key Cache entry key
 * @param size Size of the cache entry
 */
void jsrt_compile_cache_update_lru(JSRT_CompileCacheConfig* config, const char* key, size_t size);

/**
 * Remove cache entry from LRU tracking
 * @param config Cache configuration
 * @param key Cache entry key
 */
void jsrt_compile_cache_remove_lru(JSRT_CompileCacheConfig* config, const char* key);

/**
 * Get on-disk cache size by scanning directory
 * @param directory Cache directory path
 * @return Total size in bytes
 */
size_t jsrt_compile_cache_get_disk_size(const char* directory);

#endif  // __JSRT_NODE_MODULE_COMPILE_CACHE_H__
