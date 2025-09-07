#include "cache.h"
#include "security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// HTTP cache implementation using a simple hash table with LRU eviction
typedef struct CacheNode {
  JSRT_HttpCacheEntry entry;
  struct CacheNode* next;
  struct CacheNode* prev;
  time_t last_accessed;
} CacheNode;

struct JSRT_HttpCache {
  CacheNode** buckets;
  size_t bucket_count;
  size_t max_entries;
  size_t current_entries;
  size_t total_size_bytes;
  size_t hits;
  size_t misses;
  
  // LRU list pointers
  CacheNode* lru_head;
  CacheNode* lru_tail;
  
  time_t default_ttl;  // Default time to live in seconds
};

// Simple hash function for URLs
static size_t hash_url(const char* url, size_t bucket_count) {
  size_t hash = 5381;
  const unsigned char* str = (const unsigned char*)url;
  int c;

  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }

  return hash % bucket_count;
}

// Move node to front of LRU list (most recently used)
static void move_to_front(JSRT_HttpCache* cache, CacheNode* node) {
  if (!node || node == cache->lru_head) {
    return;  // Already at front or invalid
  }

  // Remove from current position
  if (node->prev) {
    node->prev->next = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  }
  if (node == cache->lru_tail) {
    cache->lru_tail = node->prev;
  }

  // Insert at front
  node->prev = NULL;
  node->next = cache->lru_head;
  if (cache->lru_head) {
    cache->lru_head->prev = node;
  }
  cache->lru_head = node;
  
  if (!cache->lru_tail) {
    cache->lru_tail = node;
  }
  
  node->last_accessed = time(NULL);
}

// Remove least recently used entry
static void evict_lru(JSRT_HttpCache* cache) {
  if (!cache->lru_tail) {
    return;
  }

  CacheNode* lru_node = cache->lru_tail;
  
  // Remove from LRU list
  if (lru_node->prev) {
    lru_node->prev->next = NULL;
    cache->lru_tail = lru_node->prev;
  } else {
    cache->lru_head = cache->lru_tail = NULL;
  }

  // Remove from hash table
  size_t bucket = hash_url(lru_node->entry.url, cache->bucket_count);
  CacheNode** current = &cache->buckets[bucket];
  
  while (*current) {
    if (*current == lru_node) {
      *current = lru_node->next;
      break;
    }
    current = &(*current)->next;
  }

  // Free memory
  cache->total_size_bytes -= lru_node->entry.size;
  cache->current_entries--;
  
  free(lru_node->entry.url);
  free(lru_node->entry.data);
  free(lru_node->entry.etag);
  free(lru_node->entry.last_modified);
  free(lru_node);
}

JSRT_HttpCache* jsrt_http_cache_create(size_t max_entries) {
  if (max_entries == 0) {
    max_entries = 100;  // Default size
  }

  JSRT_HttpCache* cache = malloc(sizeof(JSRT_HttpCache));
  if (!cache) {
    return NULL;
  }

  // Use a bucket count that's a prime number roughly equal to max_entries
  cache->bucket_count = max_entries > 100 ? 101 : 53;
  cache->buckets = calloc(cache->bucket_count, sizeof(CacheNode*));
  if (!cache->buckets) {
    free(cache);
    return NULL;
  }

  cache->max_entries = max_entries;
  cache->current_entries = 0;
  cache->total_size_bytes = 0;
  cache->hits = 0;
  cache->misses = 0;
  cache->lru_head = NULL;
  cache->lru_tail = NULL;
  
  // Get default TTL from config
  JSRT_HttpConfig* config = jsrt_http_config_init();
  cache->default_ttl = config ? 3600 : 3600;  // 1 hour default
  
  const char* ttl_env = getenv("JSRT_HTTP_MODULES_CACHE_TTL");
  if (ttl_env) {
    cache->default_ttl = (time_t)atol(ttl_env);
  }

  return cache;
}

JSRT_HttpCacheEntry* jsrt_http_cache_get(JSRT_HttpCache* cache, const char* url) {
  if (!cache || !url) {
    return NULL;
  }

  size_t bucket = hash_url(url, cache->bucket_count);
  CacheNode* current = cache->buckets[bucket];

  while (current) {
    if (strcmp(current->entry.url, url) == 0) {
      // Check if expired
      if (jsrt_http_cache_is_expired(&current->entry)) {
        jsrt_http_cache_remove(cache, url);
        cache->misses++;
        return NULL;
      }
      
      // Move to front of LRU list
      move_to_front(cache, current);
      cache->hits++;
      return &current->entry;
    }
    current = current->next;
  }

  cache->misses++;
  return NULL;
}

void jsrt_http_cache_put(JSRT_HttpCache* cache, const char* url, const char* data, 
                         size_t size, const char* etag, const char* last_modified) {
  if (!cache || !url || !data) {
    return;
  }

  // Check if entry already exists
  JSRT_HttpCacheEntry* existing = jsrt_http_cache_get(cache, url);
  if (existing) {
    // Update existing entry
    free(existing->data);
    free(existing->etag);
    free(existing->last_modified);
    
    existing->data = malloc(size + 1);
    if (existing->data) {
      memcpy(existing->data, data, size);
      existing->data[size] = '\0';
      existing->size = size;
    }
    
    existing->etag = etag ? strdup(etag) : NULL;
    existing->last_modified = last_modified ? strdup(last_modified) : NULL;
    existing->cached_at = time(NULL);
    existing->expires_at = existing->cached_at + cache->default_ttl;
    
    return;
  }

  // Evict entries if we're at capacity
  while (cache->current_entries >= cache->max_entries) {
    evict_lru(cache);
  }

  // Create new node
  CacheNode* node = malloc(sizeof(CacheNode));
  if (!node) {
    return;
  }

  // Initialize entry
  node->entry.url = strdup(url);
  node->entry.data = malloc(size + 1);
  if (!node->entry.data) {
    free(node->entry.url);
    free(node);
    return;
  }
  
  memcpy(node->entry.data, data, size);
  node->entry.data[size] = '\0';
  node->entry.size = size;
  node->entry.etag = etag ? strdup(etag) : NULL;
  node->entry.last_modified = last_modified ? strdup(last_modified) : NULL;
  node->entry.cached_at = time(NULL);
  node->entry.expires_at = node->entry.cached_at + cache->default_ttl;
  
  node->last_accessed = node->entry.cached_at;

  // Insert into hash table
  size_t bucket = hash_url(url, cache->bucket_count);
  node->next = cache->buckets[bucket];
  cache->buckets[bucket] = node;

  // Insert at front of LRU list
  node->prev = NULL;
  if (cache->lru_head) {
    cache->lru_head->prev = node;
    node->next = cache->lru_head;
  } else {
    cache->lru_tail = node;
    node->next = NULL;
  }
  cache->lru_head = node;

  cache->current_entries++;
  cache->total_size_bytes += size;
}

bool jsrt_http_cache_is_expired(JSRT_HttpCacheEntry* entry) {
  if (!entry) {
    return true;
  }

  time_t now = time(NULL);
  return now > entry->expires_at;
}

void jsrt_http_cache_remove(JSRT_HttpCache* cache, const char* url) {
  if (!cache || !url) {
    return;
  }

  size_t bucket = hash_url(url, cache->bucket_count);
  CacheNode** current = &cache->buckets[bucket];

  while (*current) {
    if (strcmp((*current)->entry.url, url) == 0) {
      CacheNode* to_remove = *current;
      
      // Remove from hash table
      *current = to_remove->next;
      
      // Remove from LRU list
      if (to_remove->prev) {
        to_remove->prev->next = to_remove->next;
      } else {
        cache->lru_head = to_remove->next;
      }
      
      if (to_remove->next) {
        to_remove->next->prev = to_remove->prev;
      } else {
        cache->lru_tail = to_remove->prev;
      }

      // Free memory
      cache->total_size_bytes -= to_remove->entry.size;
      cache->current_entries--;
      
      free(to_remove->entry.url);
      free(to_remove->entry.data);
      free(to_remove->entry.etag);
      free(to_remove->entry.last_modified);
      free(to_remove);
      
      return;
    }
    current = &(*current)->next;
  }
}

void jsrt_http_cache_clear(JSRT_HttpCache* cache) {
  if (!cache) {
    return;
  }

  for (size_t i = 0; i < cache->bucket_count; i++) {
    CacheNode* current = cache->buckets[i];
    while (current) {
      CacheNode* next = current->next;
      
      free(current->entry.url);
      free(current->entry.data);
      free(current->entry.etag);
      free(current->entry.last_modified);
      free(current);
      
      current = next;
    }
    cache->buckets[i] = NULL;
  }

  cache->current_entries = 0;
  cache->total_size_bytes = 0;
  cache->lru_head = NULL;
  cache->lru_tail = NULL;
}

void jsrt_http_cache_free(JSRT_HttpCache* cache) {
  if (!cache) {
    return;
  }

  jsrt_http_cache_clear(cache);
  free(cache->buckets);
  free(cache);
}

JSRT_HttpCacheStats jsrt_http_cache_get_stats(JSRT_HttpCache* cache) {
  JSRT_HttpCacheStats stats = {0};
  
  if (cache) {
    stats.total_entries = cache->current_entries;
    stats.max_entries = cache->max_entries;
    stats.total_size_bytes = cache->total_size_bytes;
    stats.hits = cache->hits;
    stats.misses = cache->misses;
  }
  
  return stats;
}