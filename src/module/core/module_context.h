#ifndef __JSRT_MODULE_CONTEXT_H__
#define __JSRT_MODULE_CONTEXT_H__

/**
 * Module Loader Context
 *
 * Central data structure that holds the state of the module loading system.
 * Each JSContext can have an associated JSRT_ModuleLoader instance.
 */

#include <quickjs.h>

// Forward declarations to avoid circular dependencies
typedef struct JSRT_ModuleCache JSRT_ModuleCache;
typedef struct JSRT_ModuleResolver JSRT_ModuleResolver;
typedef struct JSRT_ModuleDetector JSRT_ModuleDetector;

typedef enum {
  JSRT_MODULE_REQUEST_ESM = 0,
  JSRT_MODULE_REQUEST_COMMONJS = 1,
} JSRT_ModuleRequestType;

/**
 * Main module loader context structure
 *
 * This structure holds all state for the module loading system:
 * - Module cache for loaded modules
 * - Configuration and policies
 * - Statistics for monitoring
 */
typedef struct JSRT_ModuleLoader {
  JSContext* ctx;  // Associated JavaScript context

  // Core components (to be implemented in later phases)
  JSRT_ModuleCache* cache;        // Module cache
  JSRT_ModuleResolver* resolver;  // Module resolver (Phase 2)
  JSRT_ModuleDetector* detector;  // Type detector (Phase 3)

  JSRT_ModuleRequestType current_request_type;

  // Configuration
  int enable_cache;         // Whether caching is enabled (default: 1)
  int enable_http_imports;  // Whether HTTP(S) imports are allowed (default: 0)
  int enable_node_compat;   // Whether Node.js compatibility mode is enabled (default: 1)
  int max_cache_size;       // Maximum cache size (default: 1000)

  // Statistics
  uint64_t loads_total;    // Total number of module loads attempted
  uint64_t loads_success;  // Number of successful loads
  uint64_t loads_failed;   // Number of failed loads
  uint64_t cache_hits;     // Number of cache hits
  uint64_t cache_misses;   // Number of cache misses

  // Memory tracking
  size_t memory_used;  // Approximate memory used by loader (bytes)

} JSRT_ModuleLoader;

/**
 * Create a new module loader instance
 *
 * @param ctx The JavaScript context to associate with this loader
 * @return Pointer to newly created loader, or NULL on failure
 */
JSRT_ModuleLoader* jsrt_module_loader_create(JSContext* ctx);

/**
 * Free module loader and all associated resources
 *
 * @param loader The loader to free (can be NULL)
 */
void jsrt_module_loader_free(JSRT_ModuleLoader* loader);

/**
 * Get module loader from JSContext opaque pointer
 *
 * @param ctx The JavaScript context
 * @return Associated module loader, or NULL if none set
 */
JSRT_ModuleLoader* jsrt_module_loader_get(JSContext* ctx);

/**
 * Set module loader on JSContext opaque pointer
 *
 * @param ctx The JavaScript context
 * @param loader The loader to associate (can be NULL to clear)
 */
void jsrt_module_loader_set(JSContext* ctx, JSRT_ModuleLoader* loader);

/**
 * Reset module loader statistics
 *
 * @param loader The loader whose statistics to reset
 */
void jsrt_module_loader_reset_stats(JSRT_ModuleLoader* loader);

#endif  // __JSRT_MODULE_CONTEXT_H__
