#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "zlib_internal.h"

// Simple object pool for ZlibContext to reduce allocation overhead
#define POOL_SIZE 8

typedef struct {
  ZlibContext* pool[POOL_SIZE];
  int count;
  pthread_mutex_t lock;
} ZlibContextPool;

static ZlibContextPool context_pool = {{NULL}, 0, PTHREAD_MUTEX_INITIALIZER};

// Get a context from pool or allocate new one
ZlibContext* zlib_context_acquire(JSContext* ctx) {
  ZlibContext* zctx = NULL;

  pthread_mutex_lock(&context_pool.lock);
  if (context_pool.count > 0) {
    zctx = context_pool.pool[--context_pool.count];
  }
  pthread_mutex_unlock(&context_pool.lock);

  if (zctx) {
    // Reset the context for reuse
    memset(&zctx->strm, 0, sizeof(z_stream));
    zlib_options_init_defaults(&zctx->opts);
    zctx->initialized = false;
    zctx->is_deflate = false;
    if (zctx->output_buffer) {
      free(zctx->output_buffer);
      zctx->output_buffer = NULL;
    }
    zctx->output_capacity = 0;
    zctx->output_size = 0;
    return zctx;
  }

  // Pool empty, allocate new
  return zlib_context_new(ctx);
}

// Return context to pool or free it
void zlib_context_release(ZlibContext* zctx) {
  if (!zctx) {
    return;
  }

  // Clean up zlib state
  if (zctx->initialized) {
    zlib_cleanup(zctx);
  }

  // Clean up output buffer
  if (zctx->output_buffer) {
    free(zctx->output_buffer);
    zctx->output_buffer = NULL;
  }

  zlib_options_cleanup(&zctx->opts);

  // Try to return to pool
  pthread_mutex_lock(&context_pool.lock);
  if (context_pool.count < POOL_SIZE) {
    context_pool.pool[context_pool.count++] = zctx;
    pthread_mutex_unlock(&context_pool.lock);
    return;
  }
  pthread_mutex_unlock(&context_pool.lock);

  // Pool full, free it
  free(zctx);
}

// Buffer pool for common chunk sizes
#define BUFFER_POOL_SIZES 4
static const size_t buffer_sizes[BUFFER_POOL_SIZES] = {
    4 * 1024,   // 4KB
    16 * 1024,  // 16KB - default chunk size
    64 * 1024,  // 64KB
    256 * 1024  // 256KB
};

typedef struct {
  uint8_t* buffers[POOL_SIZE];
  int count;
  pthread_mutex_t lock;
} BufferPool;

static BufferPool buffer_pools[BUFFER_POOL_SIZES] = {{{NULL}, 0, PTHREAD_MUTEX_INITIALIZER},
                                                     {{NULL}, 0, PTHREAD_MUTEX_INITIALIZER},
                                                     {{NULL}, 0, PTHREAD_MUTEX_INITIALIZER},
                                                     {{NULL}, 0, PTHREAD_MUTEX_INITIALIZER}};

// Find appropriate pool index for size
static int find_pool_index(size_t size) {
  for (int i = 0; i < BUFFER_POOL_SIZES; i++) {
    if (size <= buffer_sizes[i]) {
      return i;
    }
  }
  return -1;  // Too large for pooling
}

// Acquire buffer from pool or allocate
uint8_t* zlib_buffer_acquire(size_t size, size_t* actual_size) {
  int pool_idx = find_pool_index(size);

  if (pool_idx >= 0) {
    BufferPool* pool = &buffer_pools[pool_idx];
    uint8_t* buffer = NULL;

    pthread_mutex_lock(&pool->lock);
    if (pool->count > 0) {
      buffer = pool->buffers[--pool->count];
    }
    pthread_mutex_unlock(&pool->lock);

    if (buffer) {
      *actual_size = buffer_sizes[pool_idx];
      return buffer;
    }

    // Pool empty, allocate with pooled size
    *actual_size = buffer_sizes[pool_idx];
    return (uint8_t*)malloc(buffer_sizes[pool_idx]);
  }

  // Too large for pooling, allocate exact size
  *actual_size = size;
  return (uint8_t*)malloc(size);
}

// Release buffer back to pool or free
void zlib_buffer_release(uint8_t* buffer, size_t size) {
  if (!buffer) {
    return;
  }

  int pool_idx = find_pool_index(size);

  if (pool_idx >= 0 && size == buffer_sizes[pool_idx]) {
    BufferPool* pool = &buffer_pools[pool_idx];

    pthread_mutex_lock(&pool->lock);
    if (pool->count < POOL_SIZE) {
      pool->buffers[pool->count++] = buffer;
      pthread_mutex_unlock(&pool->lock);
      return;
    }
    pthread_mutex_unlock(&pool->lock);
  }

  // Not pooled or pool full
  free(buffer);
}

// Clean up pools on shutdown (optional, for testing)
void zlib_pools_cleanup(void) {
  // Clean context pool
  pthread_mutex_lock(&context_pool.lock);
  for (int i = 0; i < context_pool.count; i++) {
    free(context_pool.pool[i]);
  }
  context_pool.count = 0;
  pthread_mutex_unlock(&context_pool.lock);

  // Clean buffer pools
  for (int p = 0; p < BUFFER_POOL_SIZES; p++) {
    BufferPool* pool = &buffer_pools[p];
    pthread_mutex_lock(&pool->lock);
    for (int i = 0; i < pool->count; i++) {
      free(pool->buffers[i]);
    }
    pool->count = 0;
    pthread_mutex_unlock(&pool->lock);
  }
}
