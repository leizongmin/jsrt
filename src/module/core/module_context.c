/**
 * Module Loader Context Implementation
 */

#include "module_context.h"
#include <stdlib.h>
#include <string.h>
#include "../util/module_debug.h"
#include "../util/module_errors.h"
#include "module_cache.h"

/**
 * Create a new module loader instance
 */
JSRT_ModuleLoader* jsrt_module_loader_create(JSContext* ctx) {
  if (!ctx) {
    MODULE_DEBUG_ERROR("Cannot create module loader: NULL context");
    return NULL;
  }

  MODULE_DEBUG("Creating module loader for context %p", (void*)ctx);

  // Allocate loader structure
  JSRT_ModuleLoader* loader = (JSRT_ModuleLoader*)js_malloc(ctx, sizeof(JSRT_ModuleLoader));
  if (!loader) {
    MODULE_DEBUG_ERROR("Failed to allocate module loader: out of memory");
    return NULL;
  }

  // Initialize structure
  memset(loader, 0, sizeof(JSRT_ModuleLoader));
  loader->ctx = ctx;

  // Create cache (will be implemented in Task 1.2)
  loader->cache = jsrt_module_cache_create(ctx, 1000);  // Default capacity: 1000
  if (!loader->cache) {
    MODULE_DEBUG_ERROR("Failed to create module cache");
    js_free(ctx, loader);
    return NULL;
  }

  // Initialize configuration with defaults
  loader->enable_cache = 1;
  loader->enable_http_imports = 0;  // Disabled by default for security
  loader->enable_node_compat = 1;
  loader->max_cache_size = 1000;

  // Initialize statistics
  loader->loads_total = 0;
  loader->loads_success = 0;
  loader->loads_failed = 0;
  loader->cache_hits = 0;
  loader->cache_misses = 0;

  // Initialize memory tracking
  loader->memory_used = sizeof(JSRT_ModuleLoader);

  MODULE_DEBUG("Module loader created successfully at %p", (void*)loader);
  MODULE_DEBUG("  - Cache enabled: %d", loader->enable_cache);
  MODULE_DEBUG("  - HTTP imports: %d", loader->enable_http_imports);
  MODULE_DEBUG("  - Node.js compat: %d", loader->enable_node_compat);
  MODULE_DEBUG("  - Max cache size: %d", loader->max_cache_size);

  return loader;
}

/**
 * Free module loader and all associated resources
 */
void jsrt_module_loader_free(JSRT_ModuleLoader* loader) {
  if (!loader) {
    return;
  }

  MODULE_DEBUG("Freeing module loader at %p", (void*)loader);
  MODULE_DEBUG("  - Total loads: %llu", (unsigned long long)loader->loads_total);
  MODULE_DEBUG("  - Successful loads: %llu", (unsigned long long)loader->loads_success);
  MODULE_DEBUG("  - Failed loads: %llu", (unsigned long long)loader->loads_failed);
  MODULE_DEBUG("  - Cache hits: %llu", (unsigned long long)loader->cache_hits);
  MODULE_DEBUG("  - Cache misses: %llu", (unsigned long long)loader->cache_misses);
  MODULE_DEBUG("  - Memory used: %zu bytes", loader->memory_used);

  JSContext* ctx = loader->ctx;

  // Free cache
  if (loader->cache) {
    jsrt_module_cache_free(loader->cache);
    loader->cache = NULL;
  }

  // Free resolver (Phase 2)
  // if (loader->resolver) {
  //   jsrt_module_resolver_free(loader->resolver);
  // }

  // Free detector (Phase 3)
  // if (loader->detector) {
  //   jsrt_module_detector_free(loader->detector);
  // }

  // Free the loader itself
  js_free(ctx, loader);

  MODULE_DEBUG("Module loader freed successfully");
}

/**
 * Get module loader from JSContext opaque pointer
 *
 * Note: This is a placeholder implementation. The actual storage mechanism
 * depends on how jsrt manages context-associated data. We may need to use
 * a global hash map or store it in JSRuntime's opaque pointer.
 */
JSRT_ModuleLoader* jsrt_module_loader_get(JSContext* ctx) {
  if (!ctx) {
    return NULL;
  }

  // TODO: Implement proper storage/retrieval mechanism
  // This needs to be integrated with runtime.c to store the loader
  // in a way that's accessible from the JSContext.
  //
  // Options:
  // 1. Store in JSRuntime opaque pointer (if we have one loader per runtime)
  // 2. Use a global hash map: JSContext* -> JSRT_ModuleLoader*
  // 3. Store in a property of the global object
  //
  // For now, returning NULL as placeholder
  MODULE_DEBUG("Getting module loader for context %p (not yet implemented)", (void*)ctx);
  return NULL;
}

/**
 * Set module loader on JSContext opaque pointer
 */
void jsrt_module_loader_set(JSContext* ctx, JSRT_ModuleLoader* loader) {
  if (!ctx) {
    return;
  }

  // TODO: Implement proper storage mechanism (see jsrt_module_loader_get)
  MODULE_DEBUG("Setting module loader %p for context %p (not yet implemented)", (void*)loader, (void*)ctx);
}

/**
 * Reset module loader statistics
 */
void jsrt_module_loader_reset_stats(JSRT_ModuleLoader* loader) {
  if (!loader) {
    return;
  }

  MODULE_DEBUG("Resetting module loader statistics");

  loader->loads_total = 0;
  loader->loads_success = 0;
  loader->loads_failed = 0;
  loader->cache_hits = 0;
  loader->cache_misses = 0;

  MODULE_DEBUG("Statistics reset complete");
}
