#ifndef JSRT_NODE_FS_ASYNC_LIBUV_H
#define JSRT_NODE_FS_ASYNC_LIBUV_H

#include <uv.h>
#include "fs_common.h"

// Async work request structure for fs operations with libuv
typedef struct {
  uv_fs_t req;           // libuv fs request (MUST be first for casting)
  JSContext* ctx;        // QuickJS context
  JSValue callback;      // JS callback function (must be freed)
  JSValue user_buffer;   // User's buffer (for read/write operations, must be freed)
  char* path;            // Primary path (owned, must be freed)
  char* path2;           // Secondary path for operations like rename/link (owned)
  void* buffer;          // Buffer data (owned if owns_buffer=true)
  size_t buffer_size;    // Buffer size
  size_t buffer_offset;  // Offset into user's buffer (for read/write operations)
  int flags;             // Operation flags
  int mode;              // File mode
  int64_t offset;        // File offset for read/write
  int owns_buffer;       // 1 if buffer is owned and should be freed, 0 if it points to user's buffer
} fs_async_work_t;

// Work request cleanup - frees all allocated resources
void fs_async_work_free(fs_async_work_t* work);

// Generic completion callback - calls JS callback with error or result
void fs_async_generic_complete(uv_fs_t* req);

// Specific completion callbacks for different result types
void fs_async_complete_void(uv_fs_t* req);     // Operations with no result (unlink, mkdir, etc)
void fs_async_complete_fd(uv_fs_t* req);       // Operations returning fd (open)
void fs_async_complete_data(uv_fs_t* req);     // Operations returning data (read, readFile)
void fs_async_complete_stat(uv_fs_t* req);     // Operations returning stat (stat, lstat, fstat)
void fs_async_complete_string(uv_fs_t* req);   // Operations returning string (readlink, realpath)
void fs_async_complete_readdir(uv_fs_t* req);  // readdir with array result

// Helper to get uv_loop from context
uv_loop_t* fs_get_uv_loop(JSContext* ctx);

#endif  // JSRT_NODE_FS_ASYNC_LIBUV_H
