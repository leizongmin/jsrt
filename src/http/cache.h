#ifndef __JSRT_HTTP_CACHE_H__
#define __JSRT_HTTP_CACHE_H__

#include <stddef.h>
#include <time.h>
#include <stdbool.h>

// HTTP cache entry
typedef struct {
  char* url;
  char* data;
  size_t size;
  time_t cached_at;
  time_t expires_at;
  char* etag;
  char* last_modified;
} JSRT_HttpCacheEntry;

// HTTP cache structure
typedef struct JSRT_HttpCache JSRT_HttpCache;

// Create a new HTTP cache with maximum number of entries
JSRT_HttpCache* jsrt_http_cache_create(size_t max_entries);

// Get a cached entry (returns NULL if not found or expired)
JSRT_HttpCacheEntry* jsrt_http_cache_get(JSRT_HttpCache* cache, const char* url);

// Put an entry in the cache
void jsrt_http_cache_put(JSRT_HttpCache* cache, const char* url, const char* data, 
                         size_t size, const char* etag, const char* last_modified);

// Check if a cache entry is expired
bool jsrt_http_cache_is_expired(JSRT_HttpCacheEntry* entry);

// Remove an entry from the cache
void jsrt_http_cache_remove(JSRT_HttpCache* cache, const char* url);

// Clear all entries from the cache
void jsrt_http_cache_clear(JSRT_HttpCache* cache);

// Free the cache and all its entries
void jsrt_http_cache_free(JSRT_HttpCache* cache);

// Get cache statistics
typedef struct {
  size_t total_entries;
  size_t max_entries;
  size_t total_size_bytes;
  size_t hits;
  size_t misses;
} JSRT_HttpCacheStats;

JSRT_HttpCacheStats jsrt_http_cache_get_stats(JSRT_HttpCache* cache);

#endif