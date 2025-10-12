/**
 * Module Loader Core Implementation
 *
 * This is the main entry point for the module loading system.
 * Currently implements basic structure with cache integration.
 * Phase 2+ will add resolver, detector, and protocol handlers.
 */

#include "module_loader.h"
#include <stdlib.h>
#include <string.h>
#include "../util/module_debug.h"
#include "../util/module_errors.h"
#include "module_cache.h"

/**
 * Normalize module specifier for cache key
 *
 * Placeholder implementation - Phase 2 will implement full normalization
 * via the resolver component.
 *
 * @param specifier The module specifier
 * @param base_path The base path (can be NULL)
 * @return Normalized key (caller must free), or NULL on error
 */
static char* normalize_specifier(const char* specifier, const char* base_path) {
  if (!specifier) {
    return NULL;
  }

  // For now, just duplicate the specifier
  // TODO Phase 2: Implement proper resolution with resolver component
  MODULE_Debug_Loader("Normalizing specifier: %s (base: %s)", specifier, base_path ? base_path : "(none)");

  // Simple implementation: just use specifier as-is
  char* normalized = strdup(specifier);
  if (!normalized) {
    MODULE_Debug_Error("Failed to allocate normalized specifier");
    return NULL;
  }

  MODULE_Debug_Loader("Normalized to: %s", normalized);
  return normalized;
}

/**
 * Load module (placeholder implementation)
 *
 * This is a minimal implementation that demonstrates the structure.
 * Future phases will implement:
 * - Phase 2: Resolver integration
 * - Phase 3: Type detection
 * - Phase 4: Protocol handlers (file, http, data, builtin)
 * - Phase 5: Format loaders (ESM, CommonJS, JSON, WASM)
 */
JSValue jsrt_load_module(JSRT_ModuleLoader* loader, const char* specifier, const char* base_path) {
  if (!loader) {
    MODULE_Debug_Error("Cannot load module: NULL loader");
    return JS_EXCEPTION;
  }

  if (!specifier) {
    MODULE_Debug_Error("Cannot load module: NULL specifier");
    return jsrt_module_throw_error(loader->ctx, JSRT_MODULE_INVALID_SPECIFIER, "Module specifier cannot be NULL");
  }

  MODULE_Debug_Loader("=== Load module: %s (base: %s) ===", specifier, base_path ? base_path : "(none)");

  loader->loads_total++;

  // Step 1: Normalize specifier for cache key
  char* cache_key = normalize_specifier(specifier, base_path);
  if (!cache_key) {
    loader->loads_failed++;
    return jsrt_module_throw_error(loader->ctx, JSRT_MODULE_INTERNAL_ERROR, "Failed to normalize module specifier");
  }

  MODULE_Debug_Cache("Cache key: %s", cache_key);

  // Step 2: Check cache if enabled
  if (loader->enable_cache && loader->cache) {
    JSValue cached = jsrt_module_cache_get(loader->cache, cache_key);
    if (!JS_IsUndefined(cached)) {
      MODULE_Debug_Loader("Cache HIT for: %s", cache_key);
      loader->cache_hits++;
      loader->loads_success++;
      free(cache_key);
      return cached;  // jsrt_module_cache_get already duplicated the value
    }

    MODULE_Debug_Loader("Cache MISS for: %s", cache_key);
    loader->cache_misses++;
  }

  // Step 3: Resolve module specifier to absolute path/URL
  // TODO Phase 2: Call resolver component
  MODULE_Debug_Loader("TODO: Resolve specifier (Phase 2)");
  const char* resolved_path = specifier;  // Placeholder

  // Step 4: Detect module type
  // TODO Phase 3: Call detector component
  MODULE_Debug_Loader("TODO: Detect module type (Phase 3)");
  // ModuleType type = UNKNOWN;  // Placeholder

  // Step 5: Select protocol handler
  // TODO Phase 4: Select handler based on resolved_path protocol
  MODULE_Debug_Loader("TODO: Select protocol handler (Phase 4)");

  // Step 6: Load module content
  // TODO Phase 4: Call protocol handler to load content
  MODULE_Debug_Loader("TODO: Load module content (Phase 4)");

  // Step 7: Parse and execute module
  // TODO Phase 5: Call format loader (ESM, CommonJS, JSON, WASM)
  MODULE_Debug_Loader("TODO: Parse and execute module (Phase 5)");

  // For now, return an error indicating the feature is not yet implemented
  free(cache_key);
  loader->loads_failed++;

  return jsrt_module_throw_error(loader->ctx, JSRT_MODULE_LOAD_FAILED,
                                 "Module loading not fully implemented yet (Phase 1 complete, Phases 2-5 pending)");
}

/**
 * Load module using context
 */
JSValue jsrt_load_module_ctx(JSContext* ctx, const char* specifier, const char* base_path) {
  if (!ctx) {
    MODULE_Debug_Error("Cannot load module: NULL context");
    return JS_EXCEPTION;
  }

  // Try to get existing loader from context
  JSRT_ModuleLoader* loader = jsrt_module_loader_get(ctx);

  // If no loader, create a temporary one
  int temporary_loader = 0;
  if (!loader) {
    MODULE_Debug_Loader("No loader found on context, creating temporary loader");
    loader = jsrt_module_loader_create(ctx);
    if (!loader) {
      return jsrt_module_throw_error(ctx, JSRT_MODULE_INTERNAL_ERROR, "Failed to create module loader");
    }
    temporary_loader = 1;
  }

  // Load the module
  JSValue result = jsrt_load_module(loader, specifier, base_path);

  // Clean up temporary loader
  if (temporary_loader) {
    MODULE_Debug_Loader("Cleaning up temporary loader");
    jsrt_module_loader_free(loader);
  }

  return result;
}

/**
 * Preload a module
 */
int jsrt_preload_module(JSRT_ModuleLoader* loader, const char* specifier, const char* base_path) {
  if (!loader || !specifier) {
    MODULE_Debug_Error("Invalid arguments to jsrt_preload_module");
    return -1;
  }

  MODULE_Debug_Loader("Preloading module: %s", specifier);

  JSValue result = jsrt_load_module(loader, specifier, base_path);
  if (JS_IsException(result)) {
    MODULE_Debug_Error("Failed to preload module: %s", specifier);
    return -1;
  }

  // Free the result since we don't need it
  JS_FreeValue(loader->ctx, result);

  MODULE_Debug_Loader("Module preloaded successfully: %s", specifier);
  return 0;
}

/**
 * Invalidate a module from cache
 */
int jsrt_invalidate_module(JSRT_ModuleLoader* loader, const char* specifier) {
  if (!loader || !specifier) {
    MODULE_Debug_Error("Invalid arguments to jsrt_invalidate_module");
    return -1;
  }

  if (!loader->cache) {
    MODULE_Debug_Error("No cache available");
    return -1;
  }

  MODULE_Debug_Loader("Invalidating module: %s", specifier);

  // Normalize specifier to match cache key
  char* cache_key = normalize_specifier(specifier, NULL);
  if (!cache_key) {
    return -1;
  }

  int result = jsrt_module_cache_remove(loader->cache, cache_key);
  free(cache_key);

  if (result == 0) {
    MODULE_Debug_Loader("Module invalidated successfully: %s", specifier);
  } else {
    MODULE_Debug_Loader("Module not found in cache: %s", specifier);
  }

  return result;
}

/**
 * Invalidate all modules
 */
void jsrt_invalidate_all_modules(JSRT_ModuleLoader* loader) {
  if (!loader) {
    MODULE_Debug_Error("Invalid arguments to jsrt_invalidate_all_modules");
    return;
  }

  if (!loader->cache) {
    MODULE_Debug_Error("No cache available");
    return;
  }

  MODULE_Debug_Loader("Invalidating all modules");
  jsrt_module_cache_clear(loader->cache);
  MODULE_Debug_Loader("All modules invalidated");
}
