#include "sourcemap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/debug.h"

// FNV-1a hash function for string keys
static uint32_t hash_string(const char* str) {
  uint32_t hash = 2166136261u;
  while (*str) {
    hash ^= (uint8_t)(*str++);
    hash *= 16777619u;
  }
  return hash;
}

// ============================================================================
// Source Map Lifecycle
// ============================================================================

JSRT_SourceMap* jsrt_source_map_new(JSContext* ctx) {
  JSRT_SourceMap* map = js_mallocz(ctx, sizeof(JSRT_SourceMap));
  if (!map) {
    return NULL;
  }

  // Initialize all fields to NULL/0
  map->version = NULL;
  map->file = NULL;
  map->source_root = NULL;
  map->sources = NULL;
  map->sources_count = 0;
  map->sources_content = NULL;
  map->sources_content_count = 0;
  map->names = NULL;
  map->names_count = 0;
  map->mappings = NULL;
  map->decoded_mappings = NULL;
  map->decoded_mappings_count = 0;
  map->payload = JS_UNDEFINED;

  return map;
}

void jsrt_source_map_free(JSRuntime* rt, JSRT_SourceMap* map) {
  if (!map) {
    return;
  }

  // Free string fields
  if (map->version) {
    js_free_rt(rt, map->version);
  }
  if (map->file) {
    js_free_rt(rt, map->file);
  }
  if (map->source_root) {
    js_free_rt(rt, map->source_root);
  }
  if (map->mappings) {
    js_free_rt(rt, map->mappings);
  }

  // Free sources array
  if (map->sources) {
    for (size_t i = 0; i < map->sources_count; i++) {
      if (map->sources[i]) {
        js_free_rt(rt, map->sources[i]);
      }
    }
    js_free_rt(rt, map->sources);
  }

  // Free sources_content array
  if (map->sources_content) {
    for (size_t i = 0; i < map->sources_content_count; i++) {
      if (map->sources_content[i]) {
        js_free_rt(rt, map->sources_content[i]);
      }
    }
    js_free_rt(rt, map->sources_content);
  }

  // Free names array
  if (map->names) {
    for (size_t i = 0; i < map->names_count; i++) {
      if (map->names[i]) {
        js_free_rt(rt, map->names[i]);
      }
    }
    js_free_rt(rt, map->names);
  }

  // Free decoded mappings
  if (map->decoded_mappings) {
    js_free_rt(rt, map->decoded_mappings);
  }

  // Free JS payload
  if (!JS_IsUndefined(map->payload)) {
    JS_FreeValueRT(rt, map->payload);
  }

  // Free the map itself
  js_free_rt(rt, map);
}

// ============================================================================
// Source Map Cache Management
// ============================================================================

JSRT_SourceMapCache* jsrt_source_map_cache_init(JSContext* ctx, size_t capacity) {
  if (capacity == 0) {
    capacity = 16;  // Default capacity
  }

  JSRT_SourceMapCache* cache = js_mallocz(ctx, sizeof(JSRT_SourceMapCache));
  if (!cache) {
    return NULL;
  }

  cache->buckets = js_mallocz(ctx, sizeof(JSRT_SourceMapCacheEntry*) * capacity);
  if (!cache->buckets) {
    js_free(ctx, cache);
    return NULL;
  }

  cache->capacity = capacity;
  cache->size = 0;
  cache->enabled = true;  // Enabled by default

  return cache;
}

void jsrt_source_map_cache_free(JSRuntime* rt, JSRT_SourceMapCache* cache) {
  if (!cache) {
    return;
  }

  // Free all entries
  for (size_t i = 0; i < cache->capacity; i++) {
    JSRT_SourceMapCacheEntry* entry = cache->buckets[i];
    while (entry) {
      JSRT_SourceMapCacheEntry* next = entry->next;

      // Free entry resources
      if (entry->path) {
        js_free_rt(rt, entry->path);
      }
      if (entry->source_map) {
        jsrt_source_map_free(rt, entry->source_map);
      }
      js_free_rt(rt, entry);

      entry = next;
    }
  }

  // Free buckets array
  js_free_rt(rt, cache->buckets);

  // Free cache itself
  js_free_rt(rt, cache);
}

JSRT_SourceMap* jsrt_source_map_cache_lookup(JSRT_SourceMapCache* cache, const char* path) {
  if (!cache || !path || !cache->enabled) {
    return NULL;
  }

  uint32_t hash = hash_string(path);
  size_t bucket_index = hash % cache->capacity;

  JSRT_SourceMapCacheEntry* entry = cache->buckets[bucket_index];
  while (entry) {
    if (strcmp(entry->path, path) == 0) {
      return entry->source_map;
    }
    entry = entry->next;
  }

  return NULL;
}

bool jsrt_source_map_cache_store(JSContext* ctx, JSRT_SourceMapCache* cache, const char* path, JSRT_SourceMap* map) {
  if (!cache || !path || !map) {
    return false;
  }

  // Check if already exists (update case)
  uint32_t hash = hash_string(path);
  size_t bucket_index = hash % cache->capacity;

  JSRT_SourceMapCacheEntry* entry = cache->buckets[bucket_index];
  while (entry) {
    if (strcmp(entry->path, path) == 0) {
      // Update existing entry
      jsrt_source_map_free(JS_GetRuntime(ctx), entry->source_map);
      entry->source_map = map;
      return true;
    }
    entry = entry->next;
  }

  // Create new entry
  JSRT_SourceMapCacheEntry* new_entry = js_mallocz(ctx, sizeof(JSRT_SourceMapCacheEntry));
  if (!new_entry) {
    return false;
  }

  new_entry->path = js_strdup(ctx, path);
  if (!new_entry->path) {
    js_free(ctx, new_entry);
    return false;
  }

  new_entry->source_map = map;
  new_entry->next = cache->buckets[bucket_index];
  cache->buckets[bucket_index] = new_entry;
  cache->size++;

  return true;
}

void jsrt_source_map_cache_set_enabled(JSRT_SourceMapCache* cache, bool enabled) {
  if (cache) {
    cache->enabled = enabled;
  }
}

bool jsrt_source_map_cache_is_enabled(JSRT_SourceMapCache* cache) {
  return cache ? cache->enabled : false;
}

// ============================================================================
// Source Map Parsing (Stub - to be implemented in Task 2.2)
// ============================================================================

JSRT_SourceMap* jsrt_source_map_parse(JSContext* ctx, JSValue payload) {
  // TODO: Implement in Task 2.2 (Source Map Parsing)
  // This will include:
  // - JSON validation
  // - VLQ decoding
  // - Building decoded_mappings array
  JSRT_Debug("jsrt_source_map_parse: TODO - implement in Task 2.2");
  return NULL;
}

// ============================================================================
// SourceMap JavaScript Class (Stub - to be implemented in Task 2.3)
// ============================================================================

bool jsrt_source_map_class_init(JSContext* ctx, JSValue module_obj) {
  // TODO: Implement in Task 2.3 (SourceMap Class)
  // This will register the SourceMap class with:
  // - Constructor
  // - payload getter
  // - findEntry() method
  // - findOrigin() method
  JSRT_Debug("jsrt_source_map_class_init: TODO - implement in Task 2.3");
  return true;  // Temporary success for now
}

JSValue jsrt_source_map_create_instance(JSContext* ctx, JSRT_SourceMap* map) {
  // TODO: Implement in Task 2.3 (SourceMap Class)
  JSRT_Debug("jsrt_source_map_create_instance: TODO - implement in Task 2.3");
  return JS_UNDEFINED;
}

// ============================================================================
// Source Map Lookup (Stub - to be implemented in Task 2.6)
// ============================================================================

JSValue jsrt_find_source_map(JSContext* ctx, JSRT_SourceMapCache* cache, const char* path) {
  // TODO: Implement in Task 2.6 (module.findSourceMap)
  // This will:
  // 1. Check cache
  // 2. Look for inline source maps
  // 3. Look for external .map files
  // 4. Parse and cache if found
  JSRT_Debug("jsrt_find_source_map: TODO - implement in Task 2.6");
  return JS_UNDEFINED;
}
