/**
 * Module Cache Implementation
 *
 * Simple hash map with chaining for collision resolution.
 * Uses FNV-1a hash function for good distribution.
 */

#include "module_cache.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../util/module_debug.h"

// FNV-1a hash constants
#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

/**
 * FNV-1a hash function
 */
static uint64_t hash_string(const char* str) {
  uint64_t hash = FNV_OFFSET_BASIS;
  while (*str) {
    hash ^= (uint64_t)(unsigned char)(*str++);
    hash *= FNV_PRIME;
  }
  return hash;
}

/**
 * Get current timestamp in microseconds
 */
static uint64_t get_timestamp(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/**
 * Create a new module cache
 */
JSRT_ModuleCache* jsrt_module_cache_create(JSContext* ctx, size_t capacity) {
  if (!ctx || capacity == 0) {
    MODULE_DEBUG_ERROR("Invalid arguments to jsrt_module_cache_create");
    return NULL;
  }

// Validate capacity is reasonable and won't cause overflow
#define MAX_CACHE_CAPACITY 100000
  if (capacity > MAX_CACHE_CAPACITY) {
    MODULE_DEBUG_ERROR("Cache capacity too large: %zu (max: %d)", capacity, MAX_CACHE_CAPACITY);
    return NULL;
  }

  // Check for integer overflow in bucket allocation
  if (capacity > SIZE_MAX / sizeof(JSRT_ModuleCacheEntry*)) {
    MODULE_DEBUG_ERROR("Cache capacity would cause integer overflow: %zu", capacity);
    return NULL;
  }

  MODULE_DEBUG_CACHE("Creating module cache with capacity %zu", capacity);

  // Allocate cache structure
  JSRT_ModuleCache* cache = (JSRT_ModuleCache*)js_malloc(ctx, sizeof(JSRT_ModuleCache));
  if (!cache) {
    MODULE_DEBUG_ERROR("Failed to allocate cache: out of memory");
    return NULL;
  }

  // Initialize structure
  memset(cache, 0, sizeof(JSRT_ModuleCache));
  cache->ctx = ctx;
  cache->capacity = capacity;
  cache->max_size = capacity;  // Allow up to capacity entries

  // Allocate buckets (overflow check already done above)
  size_t buckets_size = capacity * sizeof(JSRT_ModuleCacheEntry*);
  cache->buckets = (JSRT_ModuleCacheEntry**)js_malloc(ctx, buckets_size);
  if (!cache->buckets) {
    MODULE_DEBUG_ERROR("Failed to allocate cache buckets: out of memory");
    js_free(ctx, cache);
    return NULL;
  }
  memset(cache->buckets, 0, buckets_size);

  // Initialize statistics
  cache->hits = 0;
  cache->misses = 0;
  cache->evictions = 0;
  cache->collisions = 0;

  // Initialize memory tracking
  cache->memory_used = sizeof(JSRT_ModuleCache) + buckets_size;

  MODULE_DEBUG_CACHE("Cache created successfully: %p (capacity: %zu)", (void*)cache, capacity);

  return cache;
}

/**
 * Find entry in cache (internal helper)
 */
static JSRT_ModuleCacheEntry* find_entry(JSRT_ModuleCache* cache, const char* key, size_t* bucket_idx) {
  if (!cache || !key) {
    return NULL;
  }

  uint64_t hash = hash_string(key);
  size_t idx = hash % cache->capacity;

  if (bucket_idx) {
    *bucket_idx = idx;
  }

  JSRT_ModuleCacheEntry* entry = cache->buckets[idx];
  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      return entry;
    }
    entry = entry->next;
  }

  return NULL;
}

/**
 * Get module exports from cache
 */
JSValue jsrt_module_cache_get(JSRT_ModuleCache* cache, const char* key) {
  if (!cache || !key) {
    MODULE_DEBUG_ERROR("Invalid arguments to jsrt_module_cache_get");
    return JS_UNDEFINED;
  }

  MODULE_DEBUG_CACHE("Cache lookup: %s", key);

  JSRT_ModuleCacheEntry* entry = find_entry(cache, key, NULL);
  if (entry) {
    cache->hits++;
    entry->access_count++;
    entry->last_access = get_timestamp();

    MODULE_DEBUG_CACHE("Cache HIT: %s (access_count: %llu)", key, (unsigned long long)entry->access_count);
    return JS_DupValue(cache->ctx, entry->exports);
  }

  cache->misses++;
  MODULE_DEBUG_CACHE("Cache MISS: %s", key);
  return JS_UNDEFINED;
}

/**
 * Store module exports in cache
 */
int jsrt_module_cache_put(JSRT_ModuleCache* cache, const char* key, JSValue exports) {
  if (!cache || !key) {
    MODULE_DEBUG_ERROR("Invalid arguments to jsrt_module_cache_put");
    return -1;
  }

  if (JS_IsUndefined(exports)) {
    MODULE_DEBUG_ERROR("Cannot cache undefined exports for: %s", key);
    return -1;
  }

  MODULE_DEBUG_CACHE("Cache put: %s", key);

  // Check if entry already exists
  size_t bucket_idx;
  JSRT_ModuleCacheEntry* existing = find_entry(cache, key, &bucket_idx);
  if (existing) {
    MODULE_DEBUG_CACHE("Updating existing cache entry: %s", key);
    // Free old value and replace with new one
    JS_FreeValue(cache->ctx, existing->exports);
    existing->exports = JS_DupValue(cache->ctx, exports);
    existing->load_time = get_timestamp();
    existing->access_count = 0;
    existing->last_access = existing->load_time;
    return 0;
  }

  // Check if cache is full
  if (cache->size >= cache->max_size) {
    MODULE_DEBUG_ERROR("Cache is full (size: %zu, max: %zu)", cache->size, cache->max_size);
    // TODO: Implement LRU eviction policy in future phases
    return -1;
  }

  // Create new entry
  JSRT_ModuleCacheEntry* entry = (JSRT_ModuleCacheEntry*)js_malloc(cache->ctx, sizeof(JSRT_ModuleCacheEntry));
  if (!entry) {
    MODULE_DEBUG_ERROR("Failed to allocate cache entry: out of memory");
    return -1;
  }

  // Initialize entry
  size_t key_len = strlen(key) + 1;
  entry->key = (char*)js_malloc(cache->ctx, key_len);
  if (!entry->key) {
    MODULE_DEBUG_ERROR("Failed to allocate key string: out of memory");
    js_free(cache->ctx, entry);
    return -1;
  }
  memcpy(entry->key, key, key_len);

  entry->exports = JS_DupValue(cache->ctx, exports);
  entry->load_time = get_timestamp();
  entry->access_count = 0;
  entry->last_access = entry->load_time;
  entry->next = NULL;

  // Insert at head of bucket chain
  uint64_t hash = hash_string(key);
  bucket_idx = hash % cache->capacity;

  if (cache->buckets[bucket_idx] != NULL) {
    cache->collisions++;
    MODULE_DEBUG_CACHE("Hash collision in bucket %zu", bucket_idx);
  }

  entry->next = cache->buckets[bucket_idx];
  cache->buckets[bucket_idx] = entry;

  cache->size++;
  cache->memory_used += sizeof(JSRT_ModuleCacheEntry) + key_len;

  MODULE_DEBUG_CACHE("Cache entry added: %s (size: %zu/%zu)", key, cache->size, cache->max_size);

  return 0;
}

/**
 * Check if a module exists in cache
 */
int jsrt_module_cache_has(JSRT_ModuleCache* cache, const char* key) {
  if (!cache || !key) {
    return 0;
  }

  return find_entry(cache, key, NULL) != NULL;
}

/**
 * Remove a module from cache
 */
int jsrt_module_cache_remove(JSRT_ModuleCache* cache, const char* key) {
  if (!cache || !key) {
    MODULE_DEBUG_ERROR("Invalid arguments to jsrt_module_cache_remove");
    return -1;
  }

  MODULE_DEBUG_CACHE("Cache remove: %s", key);

  uint64_t hash = hash_string(key);
  size_t bucket_idx = hash % cache->capacity;

  JSRT_ModuleCacheEntry* entry = cache->buckets[bucket_idx];
  JSRT_ModuleCacheEntry* prev = NULL;

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      // Found entry - remove it
      if (prev) {
        prev->next = entry->next;
      } else {
        cache->buckets[bucket_idx] = entry->next;
      }

      // Free entry resources
      size_t key_len = strlen(entry->key) + 1;
      js_free(cache->ctx, entry->key);
      JS_FreeValue(cache->ctx, entry->exports);
      js_free(cache->ctx, entry);

      cache->size--;
      cache->memory_used -= sizeof(JSRT_ModuleCacheEntry) + key_len;

      MODULE_DEBUG_CACHE("Cache entry removed: %s (size: %zu)", key, cache->size);
      return 0;
    }

    prev = entry;
    entry = entry->next;
  }

  MODULE_DEBUG_CACHE("Cache entry not found: %s", key);
  return -1;
}

/**
 * Clear all entries from cache
 */
void jsrt_module_cache_clear(JSRT_ModuleCache* cache) {
  if (!cache) {
    return;
  }

  MODULE_DEBUG_CACHE("Clearing cache (current size: %zu)", cache->size);

  for (size_t i = 0; i < cache->capacity; i++) {
    JSRT_ModuleCacheEntry* entry = cache->buckets[i];
    while (entry) {
      JSRT_ModuleCacheEntry* next = entry->next;

      // Free entry resources
      js_free(cache->ctx, entry->key);
      JS_FreeValue(cache->ctx, entry->exports);
      js_free(cache->ctx, entry);

      entry = next;
    }
    cache->buckets[i] = NULL;
  }

  cache->size = 0;
  cache->memory_used = sizeof(JSRT_ModuleCache) + (cache->capacity * sizeof(JSRT_ModuleCacheEntry*));

  MODULE_DEBUG_CACHE("Cache cleared successfully");
}

/**
 * Free cache and all entries
 */
void jsrt_module_cache_free(JSRT_ModuleCache* cache) {
  if (!cache) {
    return;
  }

  MODULE_DEBUG_CACHE("Freeing cache: %p", (void*)cache);
  MODULE_DEBUG_CACHE("  - Total entries: %zu", cache->size);
  MODULE_DEBUG_CACHE("  - Hits: %llu", (unsigned long long)cache->hits);
  MODULE_DEBUG_CACHE("  - Misses: %llu", (unsigned long long)cache->misses);
  MODULE_DEBUG_CACHE("  - Collisions: %llu", (unsigned long long)cache->collisions);
  MODULE_DEBUG_CACHE("  - Memory used: %zu bytes", cache->memory_used);

  JSContext* ctx = cache->ctx;

  // Clear all entries
  jsrt_module_cache_clear(cache);

  // Free buckets
  if (cache->buckets) {
    js_free(ctx, cache->buckets);
  }

  // Free cache structure
  js_free(ctx, cache);

  MODULE_DEBUG_CACHE("Cache freed successfully");
}

/**
 * Get cache statistics
 */
void jsrt_module_cache_get_stats(JSRT_ModuleCache* cache, uint64_t* hits, uint64_t* misses, size_t* size,
                                 size_t* memory_used) {
  if (!cache) {
    return;
  }

  if (hits)
    *hits = cache->hits;
  if (misses)
    *misses = cache->misses;
  if (size)
    *size = cache->size;
  if (memory_used)
    *memory_used = cache->memory_used;
}

/**
 * Reset cache statistics
 */
void jsrt_module_cache_reset_stats(JSRT_ModuleCache* cache) {
  if (!cache) {
    return;
  }

  MODULE_DEBUG_CACHE("Resetting cache statistics");

  cache->hits = 0;
  cache->misses = 0;
  cache->evictions = 0;
  cache->collisions = 0;
}
