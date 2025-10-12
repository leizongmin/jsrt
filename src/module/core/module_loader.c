/**
 * Module Loader Core Implementation
 *
 * This is the main entry point for the module loading system.
 * Integrates all phases: cache, resolver, detector, protocols, and loaders.
 */

#include "module_loader.h"
#include <stdlib.h>
#include <string.h>
#include "../detector/format_detector.h"
#include "../loaders/builtin_loader.h"
#include "../loaders/commonjs_loader.h"
#include "../loaders/esm_loader.h"
#include "../protocols/protocol_dispatcher.h"
#include "../resolver/path_resolver.h"
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
 * Load module - Full implementation integrating all phases
 *
 * Complete pipeline:
 * - Phase 1: Cache check
 * - Phase 2: Path resolution
 * - Phase 3: Format detection
 * - Phase 4: Protocol handling
 * - Phase 5: Format loading (ESM, CommonJS, builtin)
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

  // Step 1: Check if it's a builtin module (jsrt: or node:)
  if (jsrt_is_builtin_specifier(specifier)) {
    MODULE_Debug_Loader("Detected builtin module specifier");
    JSValue result = jsrt_load_builtin_module(loader->ctx, loader, specifier);
    if (JS_IsException(result)) {
      loader->loads_failed++;
    } else {
      loader->loads_success++;
    }
    return result;
  }

  // Step 2: Resolve module specifier to absolute path/URL
  JSRT_ResolvedPath* resolved = jsrt_resolve_path(loader->ctx, specifier, base_path);
  if (!resolved) {
    loader->loads_failed++;
    return jsrt_module_throw_error(loader->ctx, JSRT_MODULE_NOT_FOUND, "Cannot resolve module specifier: %s",
                                   specifier);
  }

  MODULE_Debug_Loader("Resolved to: %s (protocol: %s)", resolved->resolved_path,
                      resolved->protocol ? resolved->protocol : "file");

  // Step 3: Check cache with resolved path
  if (loader->enable_cache && loader->cache) {
    JSValue cached = jsrt_module_cache_get(loader->cache, resolved->resolved_path);
    if (!JS_IsUndefined(cached)) {
      MODULE_Debug_Loader("Cache HIT for: %s", resolved->resolved_path);
      loader->cache_hits++;
      loader->loads_success++;
      jsrt_resolved_path_free(resolved);
      return cached;  // jsrt_module_cache_get already duplicated the value
    }

    MODULE_Debug_Loader("Cache MISS for: %s", resolved->resolved_path);
    loader->cache_misses++;
  }

  // Step 4: Detect module format
  JSRT_ModuleFormat format = jsrt_detect_module_format(loader->ctx, resolved->resolved_path, NULL, 0);
  MODULE_Debug_Loader("Detected format: %s", jsrt_module_format_to_string(format));

  // Step 5: Load module based on format
  JSValue result = JS_UNDEFINED;

  switch (format) {
    case JSRT_MODULE_FORMAT_COMMONJS:
      MODULE_Debug_Loader("Loading as CommonJS module");
      result = jsrt_load_commonjs_module(loader->ctx, loader, resolved->resolved_path, specifier);
      break;

    case JSRT_MODULE_FORMAT_ESM:
      MODULE_Debug_Loader("Loading as ES module");
      // For ES modules, we need to return the module's exports as a value
      // This is a temporary wrapper - full ESM support needs QuickJS integration
      {
        JSModuleDef* esm_module = jsrt_load_esm_module(loader->ctx, loader, resolved->resolved_path, specifier);
        if (esm_module) {
          // Get exports from the module
          result = jsrt_get_esm_exports(loader->ctx, esm_module);
        } else {
          result = JS_EXCEPTION;
        }
      }
      break;

    case JSRT_MODULE_FORMAT_JSON:
      MODULE_Debug_Loader("Loading as JSON module");
      // TODO: Implement JSON loader
      result = jsrt_module_throw_error(loader->ctx, JSRT_MODULE_LOAD_FAILED, "JSON modules not yet implemented");
      break;

    case JSRT_MODULE_FORMAT_UNKNOWN:
    default:
      MODULE_Debug_Loader("Unknown module format, defaulting to CommonJS");
      // Default to CommonJS for backward compatibility
      result = jsrt_load_commonjs_module(loader->ctx, loader, resolved->resolved_path, specifier);
      break;
  }

  jsrt_resolved_path_free(resolved);

  if (JS_IsException(result)) {
    loader->loads_failed++;
  } else {
    loader->loads_success++;
  }

  return result;
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
