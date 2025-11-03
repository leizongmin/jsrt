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

/**
 * Cache configuration
 */
typedef struct JSRT_CompileCacheConfig {
  char* directory;  // Cache directory path
  bool portable;    // Use content-based hashing (slower, relocatable)
  bool enabled;     // Cache enabled flag
  uint64_t hits;    // Cache hit count
  uint64_t misses;  // Cache miss count
  uint64_t writes;  // Cache write count
  uint64_t errors;  // Cache error count
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
 */
void jsrt_compile_cache_get_stats(JSRT_CompileCacheConfig* config, uint64_t* hits, uint64_t* misses, uint64_t* writes,
                                  uint64_t* errors);

/**
 * Flush pending cache entries to disk
 * @param config Cache configuration
 * @return Number of entries flushed
 */
int jsrt_compile_cache_flush(JSRT_CompileCacheConfig* config);

#endif  // __JSRT_NODE_MODULE_COMPILE_CACHE_H__
