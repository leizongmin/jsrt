/**
 * WASI Import Object Implementation
 *
 * Creates the import object for WebAssembly.Instance with WASI functions.
 * Phase 3: Full WASI syscall implementation.
 */

#include "../../runtime.h"
#include "../../util/debug.h"
#include "wasi.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#define wasi_close_fd _close
#else
#include <unistd.h>
#define wasi_close_fd close
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <uv.h>
#include <wasm_export.h>
#include "../../../deps/wamr/core/shared/platform/include/platform_wasi_types.h"

/**
 * WASI syscall implementations
 */

// WASI errno codes
#define WASI_ESUCCESS __WASI_ESUCCESS
#define WASI_EBADF __WASI_EBADF
#define WASI_EINVAL __WASI_EINVAL
#define WASI_ENOSYS __WASI_ENOSYS
#define WASI_EFAULT __WASI_EFAULT
#define WASI_EIO __WASI_EIO
#define WASI_ESPIPE __WASI_ESPIPE
#define WASI_ENOENT __WASI_ENOENT
#define WASI_ENOTDIR __WASI_ENOTDIR
#define WASI_EPERM __WASI_EPERM
#define WASI_EACCES __WASI_EACCES
#define WASI_EEXIST __WASI_EEXIST
#define WASI_ENOTEMPTY __WASI_ENOTEMPTY
#define WASI_ENOTCAPABLE __WASI_ENOTCAPABLE
#define WASI_ENOMEM __WASI_ENOMEM
#define WASI_ENFILE __WASI_ENFILE
#define WASI_EMFILE __WASI_EMFILE
#define WASI_ENAMETOOLONG __WASI_ENAMETOOLONG
#define WASI_ELOOP __WASI_ELOOP
#define WASI_ENOSPC __WASI_ENOSPC
#define WASI_EBUSY __WASI_EBUSY
#define WASI_ENXIO __WASI_ENXIO
#define WASI_ENOTSUP __WASI_ENOTSUP
#define WASI_EISDIR __WASI_EISDIR

static uint8_t* get_wasm_memory(jsrt_wasi_t* wasi, uint32_t offset, uint32_t size);

static inline void write_u16_le(uint8_t* dst, uint16_t value) {
  dst[0] = (uint8_t)(value & 0xFF);
  dst[1] = (uint8_t)((value >> 8) & 0xFF);
}

static inline void write_u64_le(uint8_t* dst, uint64_t value) {
  for (int i = 0; i < 8; i++) {
    dst[i] = (uint8_t)((value >> (i * 8)) & 0xFF);
  }
}

static uv_loop_t* wasi_get_uv_loop(JSContext* ctx) {
  if (!ctx) {
    return NULL;
  }
  JSRuntime* rt = JS_GetRuntime(ctx);
  JSRT_Runtime* jsrt_rt = (JSRT_Runtime*)JS_GetRuntimeOpaque(rt);
  return jsrt_rt ? jsrt_rt->uv_loop : NULL;
}

static char* wasi_strndup(const char* src, size_t len) {
  char* out = malloc(len + 1);
  if (!out) {
    return NULL;
  }
  memcpy(out, src, len);
  out[len] = '\0';
  return out;
}

static uint64_t uv_timespec_to_ns(const uv_timespec_t* ts) {
  if (!ts) {
    return 0;
  }
  return (uint64_t)ts->tv_sec * 1000000000ULL + (uint64_t)ts->tv_nsec;
}

static uint32_t wasi_errno_from_errno(int err) {
  if (err == 0) {
    return WASI_ESUCCESS;
  }
  switch (err) {
    case EACCES:
      return WASI_EACCES;
    case EEXIST:
      return WASI_EEXIST;
    case ENOENT:
      return WASI_ENOENT;
    case ENOTDIR:
      return WASI_ENOTDIR;
    case ENOTEMPTY:
      return WASI_ENOTEMPTY;
    case EPERM:
      return WASI_EPERM;
    case EISDIR:
      return WASI_ENOTCAPABLE;  // Opening directory as file not permitted
    case ENOSPC:
      return WASI_ENOSPC;
    case ENOMEM:
      return WASI_ENOMEM;
    case ENFILE:
      return WASI_ENFILE;
    case EMFILE:
      return WASI_EMFILE;
    case ENAMETOOLONG:
      return WASI_ENAMETOOLONG;
    case ELOOP:
      return WASI_ELOOP;
    case EBUSY:
      return WASI_EBUSY;
    case ENXIO:
      return WASI_ENXIO;
#ifdef ENOTSUP
    case ENOTSUP:
      return WASI_ENOTSUP;
#endif
#ifdef EOPNOTSUPP
#if defined(ENOTSUP) && EOPNOTSUPP == ENOTSUP
#else
    case EOPNOTSUPP:
      return WASI_ENOTSUP;
#endif
#endif
    case EBADF:
      return WASI_EBADF;
    case EINVAL:
      return WASI_EINVAL;
    case EFAULT:
      return WASI_EFAULT;
    default:
      return WASI_EIO;
  }
}

static uint8_t wasi_filetype_from_mode(mode_t mode) {
  if (S_ISREG(mode)) {
    return __WASI_FILETYPE_REGULAR_FILE;
  }
  if (S_ISDIR(mode)) {
    return __WASI_FILETYPE_DIRECTORY;
  }
  if (S_ISCHR(mode)) {
    return __WASI_FILETYPE_CHARACTER_DEVICE;
  }
#ifdef S_ISBLK
  if (S_ISBLK(mode)) {
    return __WASI_FILETYPE_BLOCK_DEVICE;
  }
#endif
#ifdef S_ISSOCK
  if (S_ISSOCK(mode)) {
    return __WASI_FILETYPE_SOCKET_STREAM;
  }
#endif
#ifdef S_ISLNK
  if (S_ISLNK(mode)) {
    return __WASI_FILETYPE_SYMBOLIC_LINK;
  }
#endif
  return __WASI_FILETYPE_UNKNOWN;
}

static bool wasi_has_rights(const jsrt_wasi_fd_entry* entry, uint64_t rights) {
  if (!entry) {
    return false;
  }
  return (entry->rights_base & rights) == rights;
}

static uint32_t wasi_normalize_relative_path(const char* path, size_t len, bool allow_empty, char** out_path) {
  if (!path || !out_path) {
    return WASI_EINVAL;
  }

  if (len > 0 && path[0] == '/') {
    return WASI_ENOTCAPABLE;
  }

  size_t capacity = len > 0 ? len : 1;
  char** segments = calloc(capacity, sizeof(char*));
  if (!segments) {
    return WASI_ENOMEM;
  }

  size_t count = 0;
  size_t i = 0;
  while (i < len) {
    while (i < len && path[i] == '/') {
      i++;
    }
    size_t start = i;
    while (i < len && path[i] != '/') {
      i++;
    }
    size_t seg_len = i - start;
    if (seg_len == 0) {
      continue;
    }
    if (seg_len == 1 && path[start] == '.') {
      continue;
    }
    if (seg_len == 2 && path[start] == '.' && path[start + 1] == '.') {
      if (count == 0) {
        for (size_t j = 0; j < count; j++) {
          free(segments[j]);
        }
        free(segments);
        return WASI_ENOTCAPABLE;
      }
      free(segments[--count]);
      continue;
    }
    char* segment = wasi_strndup(path + start, seg_len);
    if (!segment) {
      for (size_t j = 0; j < count; j++) {
        free(segments[j]);
      }
      free(segments);
      return WASI_ENOMEM;
    }
    segments[count++] = segment;
  }

  char* normalized = NULL;
  if (count == 0) {
    if (!allow_empty) {
      free(segments);
      return WASI_ENOENT;
    }
    normalized = wasi_strndup("", 0);
    if (!normalized) {
      free(segments);
      return WASI_ENOMEM;
    }
  } else {
    size_t total_len = 0;
    for (size_t j = 0; j < count; j++) {
      total_len += strlen(segments[j]);
      if (j + 1 < count) {
        total_len += 1;  // separator
      }
    }
    normalized = malloc(total_len + 1);
    if (!normalized) {
      for (size_t j = 0; j < count; j++) {
        free(segments[j]);
      }
      free(segments);
      return WASI_ENOMEM;
    }
    size_t offset = 0;
    for (size_t j = 0; j < count; j++) {
      size_t seg_len = strlen(segments[j]);
      memcpy(normalized + offset, segments[j], seg_len);
      offset += seg_len;
      if (j + 1 < count) {
        normalized[offset++] = '/';
      }
    }
    normalized[offset] = '\0';
  }

  for (size_t j = 0; j < count; j++) {
    free(segments[j]);
  }
  free(segments);

  *out_path = normalized;
  return WASI_ESUCCESS;
}

static uint32_t wasi_resolve_path(jsrt_wasi_t* wasi, jsrt_wasi_fd_entry* dir_entry, uint32_t path_ptr,
                                  uint32_t path_len, bool allow_empty, char** out_host_path) {
  if (!wasi || !dir_entry || !dir_entry->preopen || !dir_entry->preopen->real_path) {
    return WASI_ENOTCAPABLE;
  }

  uint8_t* path_mem = get_wasm_memory(wasi, path_ptr, path_len);
  if (!path_mem) {
    return WASI_EFAULT;
  }

  char* raw = wasi_strndup((const char*)path_mem, path_len);
  if (!raw) {
    return WASI_ENOMEM;
  }

  char* normalized = NULL;
  uint32_t status = wasi_normalize_relative_path(raw, path_len, allow_empty, &normalized);
  free(raw);
  if (status != WASI_ESUCCESS) {
    return status;
  }

  const char* base = dir_entry->preopen->real_path;
  size_t base_len = strlen(base);
  size_t normalized_len = strlen(normalized);
  bool needs_separator = normalized_len > 0;
  bool base_has_sep = base_len > 0 && (base[base_len - 1] == '/' || base[base_len - 1] == '\\');

  size_t total_len = base_len;
  if (needs_separator && !base_has_sep) {
    total_len += 1;
  }
  total_len += normalized_len;

  char* host_path = malloc(total_len + 1);
  if (!host_path) {
    free(normalized);
    return WASI_ENOMEM;
  }

  memcpy(host_path, base, base_len);
  size_t offset = base_len;
  if (needs_separator && !base_has_sep) {
#ifdef _WIN32
    host_path[offset++] = '\\';
#else
    host_path[offset++] = '/';
#endif
  }

#ifdef _WIN32
  for (size_t i = 0; i < normalized_len; i++) {
    char c = normalized[i] == '/' ? '\\' : normalized[i];
    host_path[offset++] = c;
  }
#else
  memcpy(host_path + offset, normalized, normalized_len);
  offset += normalized_len;
#endif

  host_path[offset] = '\0';
  free(normalized);

  *out_host_path = host_path;
  return WASI_ESUCCESS;
}

/**
 * Helper: Get WASI instance from function's opaque data
 * Syscalls use JS_NewCFunctionData to capture WASI instance pointer
 */
static inline jsrt_wasi_t* get_wasi_instance(JSContext* ctx, JSValue* func_data) {
  if (!ctx || !func_data) {
    return NULL;
  }
  // func_data[0] contains raw pointer encoded as int64
  int64_t ptr_val;
  if (JS_ToInt64(ctx, &ptr_val, func_data[0]) < 0) {
    return NULL;
  }
  return (jsrt_wasi_t*)(uintptr_t)ptr_val;
}

/**
 * Helper: Get WASM linear memory pointer
 */
static uint8_t* get_wasm_memory(jsrt_wasi_t* wasi, uint32_t offset, uint32_t size) {
  if (!wasi || !wasi->wamr_instance) {
    return NULL;
  }

  // Validate offset and size against memory bounds
  if (!wasm_runtime_validate_app_addr(wasi->wamr_instance, offset, size)) {
    return NULL;
  }

  // Convert WASM app address to native pointer
  void* native_addr = wasm_runtime_addr_app_to_native(wasi->wamr_instance, offset);
  if (!native_addr) {
    return NULL;
  }

  return (uint8_t*)native_addr;
}

// args_get(argv: ptr, argv_buf: ptr) -> errno
static JSValue wasi_args_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                             JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: args_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t argv_ptr, argv_buf_ptr;
  if (JS_ToUint32(ctx, &argv_ptr, argv[0]) || JS_ToUint32(ctx, &argv_buf_ptr, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: args_get(argv=%u, argv_buf=%u)", argv_ptr, argv_buf_ptr);

  // Calculate total buffer size needed
  size_t total_size = 0;
  for (size_t i = 0; i < wasi->options.args_count; i++) {
    total_size += strlen(wasi->options.args[i]) + 1;  // +1 for null terminator
  }

  // Get pointers to WASM memory
  uint8_t* argv_array =
      get_wasm_memory(wasi, argv_ptr, wasi->options.args_count * 4);  // Array of pointers (4 bytes each)
  uint8_t* argv_buf = get_wasm_memory(wasi, argv_buf_ptr, total_size);

  if (!argv_array || !argv_buf) {
    JSRT_Debug("WASI syscall: args_get - invalid memory pointers");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Write arguments to WASM memory
  uint32_t buf_offset = 0;
  for (size_t i = 0; i < wasi->options.args_count; i++) {
    const char* arg = wasi->options.args[i];
    size_t arg_len = strlen(arg) + 1;

    // Write pointer to argv array (little-endian)
    uint32_t arg_ptr = argv_buf_ptr + buf_offset;
    argv_array[i * 4 + 0] = (arg_ptr >> 0) & 0xFF;
    argv_array[i * 4 + 1] = (arg_ptr >> 8) & 0xFF;
    argv_array[i * 4 + 2] = (arg_ptr >> 16) & 0xFF;
    argv_array[i * 4 + 3] = (arg_ptr >> 24) & 0xFF;

    // Write string to argv_buf
    memcpy(argv_buf + buf_offset, arg, arg_len);
    buf_offset += arg_len;
  }

  JSRT_Debug("WASI syscall: args_get - wrote %zu args, %zu bytes", wasi->options.args_count, total_size);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// args_sizes_get(argc: ptr, argv_buf_size: ptr) -> errno
static JSValue wasi_args_sizes_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                   JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: args_sizes_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t argc_ptr, argv_buf_size_ptr;
  if (JS_ToUint32(ctx, &argc_ptr, argv[0]) || JS_ToUint32(ctx, &argv_buf_size_ptr, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: args_sizes_get(argc=%u, argv_buf_size=%u)", argc_ptr, argv_buf_size_ptr);

  // Calculate total buffer size needed
  size_t total_size = 0;
  for (size_t i = 0; i < wasi->options.args_count; i++) {
    total_size += strlen(wasi->options.args[i]) + 1;  // +1 for null terminator
  }

  // Get pointers to WASM memory
  uint8_t* argc_mem = get_wasm_memory(wasi, argc_ptr, 4);
  uint8_t* argv_buf_size_mem = get_wasm_memory(wasi, argv_buf_size_ptr, 4);

  if (!argc_mem || !argv_buf_size_mem) {
    JSRT_Debug("WASI syscall: args_sizes_get - invalid memory pointers");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Write argc (little-endian)
  uint32_t arg_count = wasi->options.args_count;
  argc_mem[0] = (arg_count >> 0) & 0xFF;
  argc_mem[1] = (arg_count >> 8) & 0xFF;
  argc_mem[2] = (arg_count >> 16) & 0xFF;
  argc_mem[3] = (arg_count >> 24) & 0xFF;

  // Write argv_buf_size (little-endian)
  argv_buf_size_mem[0] = (total_size >> 0) & 0xFF;
  argv_buf_size_mem[1] = (total_size >> 8) & 0xFF;
  argv_buf_size_mem[2] = (total_size >> 16) & 0xFF;
  argv_buf_size_mem[3] = (total_size >> 24) & 0xFF;

  JSRT_Debug("WASI syscall: args_sizes_get - argc=%u, buf_size=%zu", arg_count, total_size);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// environ_get(environ: ptr, environ_buf: ptr) -> errno
static JSValue wasi_environ_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: environ_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t environ_ptr, environ_buf_ptr;
  if (JS_ToUint32(ctx, &environ_ptr, argv[0]) || JS_ToUint32(ctx, &environ_buf_ptr, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: environ_get(environ=%u, environ_buf=%u)", environ_ptr, environ_buf_ptr);

  // Calculate total buffer size needed
  size_t total_size = 0;
  for (size_t i = 0; i < wasi->options.env_count; i++) {
    total_size += strlen(wasi->options.env[i]) + 1;  // +1 for null terminator
  }

  // Get pointers to WASM memory
  uint8_t* environ_array = get_wasm_memory(wasi, environ_ptr, wasi->options.env_count * 4);
  uint8_t* environ_buf = get_wasm_memory(wasi, environ_buf_ptr, total_size);

  if (!environ_array || !environ_buf) {
    JSRT_Debug("WASI syscall: environ_get - invalid memory pointers");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Write environment variables to WASM memory
  uint32_t buf_offset = 0;
  for (size_t i = 0; i < wasi->options.env_count; i++) {
    const char* env = wasi->options.env[i];
    size_t env_len = strlen(env) + 1;

    // Write pointer to environ array (little-endian)
    uint32_t env_ptr = environ_buf_ptr + buf_offset;
    environ_array[i * 4 + 0] = (env_ptr >> 0) & 0xFF;
    environ_array[i * 4 + 1] = (env_ptr >> 8) & 0xFF;
    environ_array[i * 4 + 2] = (env_ptr >> 16) & 0xFF;
    environ_array[i * 4 + 3] = (env_ptr >> 24) & 0xFF;

    // Write string to environ_buf
    memcpy(environ_buf + buf_offset, env, env_len);
    buf_offset += env_len;
  }

  JSRT_Debug("WASI syscall: environ_get - wrote %zu env vars, %zu bytes", wasi->options.env_count, total_size);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// environ_sizes_get(environc: ptr, environ_buf_size: ptr) -> errno
static JSValue wasi_environ_sizes_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                      JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: environ_sizes_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t environc_ptr, environ_buf_size_ptr;
  if (JS_ToUint32(ctx, &environc_ptr, argv[0]) || JS_ToUint32(ctx, &environ_buf_size_ptr, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: environ_sizes_get(environc=%u, environ_buf_size=%u)", environc_ptr, environ_buf_size_ptr);

  // Calculate total buffer size needed
  size_t total_size = 0;
  for (size_t i = 0; i < wasi->options.env_count; i++) {
    total_size += strlen(wasi->options.env[i]) + 1;
  }

  // Get pointers to WASM memory
  uint8_t* environc_mem = get_wasm_memory(wasi, environc_ptr, 4);
  uint8_t* environ_buf_size_mem = get_wasm_memory(wasi, environ_buf_size_ptr, 4);

  if (!environc_mem || !environ_buf_size_mem) {
    JSRT_Debug("WASI syscall: environ_sizes_get - invalid memory pointers");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Write environc (little-endian)
  uint32_t env_count = wasi->options.env_count;
  environc_mem[0] = (env_count >> 0) & 0xFF;
  environc_mem[1] = (env_count >> 8) & 0xFF;
  environc_mem[2] = (env_count >> 16) & 0xFF;
  environc_mem[3] = (env_count >> 24) & 0xFF;

  // Write environ_buf_size (little-endian)
  environ_buf_size_mem[0] = (total_size >> 0) & 0xFF;
  environ_buf_size_mem[1] = (total_size >> 8) & 0xFF;
  environ_buf_size_mem[2] = (total_size >> 16) & 0xFF;
  environ_buf_size_mem[3] = (total_size >> 24) & 0xFF;

  JSRT_Debug("WASI syscall: environ_sizes_get - environc=%u, buf_size=%zu", env_count, total_size);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// WASI iovec structure (in WASM memory)
typedef struct {
  uint32_t buf;  // Pointer to buffer
  uint32_t len;  // Buffer length
} wasi_iovec_t;

// fd_write(fd: fd, iovs: ptr, iovs_len: size, nwritten: ptr) -> errno
static JSValue wasi_fd_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                             JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_write - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 4) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd, iovs_ptr, iovs_len, nwritten_ptr;
  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToUint32(ctx, &iovs_ptr, argv[1]) || JS_ToUint32(ctx, &iovs_len, argv[2]) ||
      JS_ToUint32(ctx, &nwritten_ptr, argv[3])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_write(fd=%u, iovs=%u, iovs_len=%u, nwritten=%u)", fd, iovs_ptr, iovs_len, nwritten_ptr);

  // Check FD validity
  jsrt_wasi_fd_entry* entry = NULL;
  int host_fd = -1;
  bool is_stdio = false;
  if (fd == 1) {
    host_fd = wasi->options.stdout_fd;
    is_stdio = true;
  } else if (fd == 2) {
    host_fd = wasi->options.stderr_fd;
    is_stdio = true;
  } else {
    entry = jsrt_wasi_get_fd(wasi, fd);
    if (!entry) {
      return JS_NewInt32(ctx, WASI_EBADF);
    }
    if (entry->filetype == __WASI_FILETYPE_DIRECTORY || entry->preopen != NULL) {
      return JS_NewInt32(ctx, WASI_EISDIR);
    }
    if (!wasi_has_rights(entry, __WASI_RIGHT_FD_WRITE)) {
      return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
    }
    if (entry->host_fd < 0) {
      return JS_NewInt32(ctx, WASI_EBADF);
    }
    host_fd = entry->host_fd;
  }

  // Get iovs array from WASM memory
  uint8_t* iovs_mem = get_wasm_memory(wasi, iovs_ptr, iovs_len * 8);  // Each iovec is 8 bytes
  uint8_t* nwritten_mem = get_wasm_memory(wasi, nwritten_ptr, 4);

  if (!iovs_mem || !nwritten_mem) {
    JSRT_Debug("WASI syscall: fd_write - invalid memory pointers");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Write each iovec
  size_t total_written = 0;
  for (uint32_t i = 0; i < iovs_len; i++) {
    // Parse iovec (little-endian)
    uint32_t buf_ptr =
        iovs_mem[i * 8 + 0] | (iovs_mem[i * 8 + 1] << 8) | (iovs_mem[i * 8 + 2] << 16) | (iovs_mem[i * 8 + 3] << 24);
    uint32_t buf_len =
        iovs_mem[i * 8 + 4] | (iovs_mem[i * 8 + 5] << 8) | (iovs_mem[i * 8 + 6] << 16) | (iovs_mem[i * 8 + 7] << 24);

    if (buf_len == 0) {
      continue;
    }

    // Get buffer from WASM memory
    uint8_t* buf = get_wasm_memory(wasi, buf_ptr, buf_len);
    if (!buf) {
      JSRT_Debug("WASI syscall: fd_write - invalid buffer pointer");
      return JS_NewInt32(ctx, WASI_EFAULT);
    }

    // Write to host FD
    size_t remaining = buf_len;
    uint8_t* cursor = buf;
    while (remaining > 0) {
      ssize_t written = write(host_fd, cursor, remaining);
      if (written < 0) {
        int err_code = errno;
        JSRT_Debug("WASI syscall: fd_write - write failed (fd=%u, errno=%d)", fd, err_code);
        return JS_NewInt32(ctx, wasi_errno_from_errno(err_code));
      }
      if (written == 0) {
        break;
      }
      total_written += written;
      cursor += written;
      remaining -= (size_t)written;
      if (is_stdio) {
        break;
      }
    }
    if (!is_stdio && remaining > 0) {
      break;
    }
  }

  // Write total bytes written (little-endian)
  nwritten_mem[0] = (total_written >> 0) & 0xFF;
  nwritten_mem[1] = (total_written >> 8) & 0xFF;
  nwritten_mem[2] = (total_written >> 16) & 0xFF;
  nwritten_mem[3] = (total_written >> 24) & 0xFF;

  JSRT_Debug("WASI syscall: fd_write - wrote %zu bytes to fd %u", total_written, fd);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// fd_read(fd: fd, iovs: ptr, iovs_len: size, nread: ptr) -> errno
static JSValue wasi_fd_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                            JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_read - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 4) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd, iovs_ptr, iovs_len, nread_ptr;
  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToUint32(ctx, &iovs_ptr, argv[1]) || JS_ToUint32(ctx, &iovs_len, argv[2]) ||
      JS_ToUint32(ctx, &nread_ptr, argv[3])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_read(fd=%u, iovs=%u, iovs_len=%u, nread=%u)", fd, iovs_ptr, iovs_len, nread_ptr);

  // Check FD validity
  jsrt_wasi_fd_entry* entry = NULL;
  int host_fd = -1;
  bool is_stdio = false;
  if (fd == 0) {
    host_fd = wasi->options.stdin_fd;
    is_stdio = true;
  } else {
    entry = jsrt_wasi_get_fd(wasi, fd);
    if (!entry) {
      return JS_NewInt32(ctx, WASI_EBADF);
    }
    if (entry->filetype == __WASI_FILETYPE_DIRECTORY) {
      return JS_NewInt32(ctx, WASI_EISDIR);
    }
    if (!wasi_has_rights(entry, __WASI_RIGHT_FD_READ)) {
      return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
    }
    if (entry->host_fd < 0) {
      return JS_NewInt32(ctx, WASI_EBADF);
    }
    host_fd = entry->host_fd;
  }

  // Get iovs array from WASM memory
  uint8_t* iovs_mem = get_wasm_memory(wasi, iovs_ptr, iovs_len * 8);  // Each iovec is 8 bytes
  uint8_t* nread_mem = get_wasm_memory(wasi, nread_ptr, 4);

  if (!iovs_mem || !nread_mem) {
    JSRT_Debug("WASI syscall: fd_read - invalid memory pointers");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Read each iovec
  size_t total_read = 0;
  for (uint32_t i = 0; i < iovs_len; i++) {
    // Parse iovec (little-endian)
    uint32_t buf_ptr =
        iovs_mem[i * 8 + 0] | (iovs_mem[i * 8 + 1] << 8) | (iovs_mem[i * 8 + 2] << 16) | (iovs_mem[i * 8 + 3] << 24);
    uint32_t buf_len =
        iovs_mem[i * 8 + 4] | (iovs_mem[i * 8 + 5] << 8) | (iovs_mem[i * 8 + 6] << 16) | (iovs_mem[i * 8 + 7] << 24);

    if (buf_len == 0) {
      continue;
    }

    // Get buffer from WASM memory
    uint8_t* buf = get_wasm_memory(wasi, buf_ptr, buf_len);
    if (!buf) {
      JSRT_Debug("WASI syscall: fd_read - invalid buffer pointer");
      return JS_NewInt32(ctx, WASI_EFAULT);
    }

    // Read from host FD
    ssize_t bytes_read = read(host_fd, buf, buf_len);
    if (bytes_read < 0) {
      int err_code = errno;
      JSRT_Debug("WASI syscall: fd_read - read failed (fd=%u, errno=%d)", fd, err_code);
      return JS_NewInt32(ctx, wasi_errno_from_errno(err_code));
    }
    total_read += bytes_read;

    // If we got less than requested, stop (EOF or would block)
    if ((size_t)bytes_read < buf_len || (!is_stdio && bytes_read == 0)) {
      break;
    }
  }

  // Write total bytes read (little-endian)
  nread_mem[0] = (total_read >> 0) & 0xFF;
  nread_mem[1] = (total_read >> 8) & 0xFF;
  nread_mem[2] = (total_read >> 16) & 0xFF;
  nread_mem[3] = (total_read >> 24) & 0xFF;

  JSRT_Debug("WASI syscall: fd_read - read %zu bytes from fd %u", total_read, fd);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// fd_close(fd: fd) -> errno
static JSValue wasi_fd_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                             JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi) {
    JSRT_Debug("WASI syscall: fd_close - no WASI instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 1) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd;
  if (JS_ToUint32(ctx, &fd, argv[0])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_close(fd=%u)", fd);

  if (fd <= 2) {
    return JS_NewInt32(ctx, WASI_ESUCCESS);
  }

  jsrt_wasi_fd_entry* entry = jsrt_wasi_get_fd(wasi, fd);
  if (!entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  if (entry->preopen != NULL) {
    return JS_NewInt32(ctx, WASI_ESUCCESS);
  }

  int close_err = 0;
  if (entry->host_fd >= 0) {
    uv_loop_t* loop = wasi_get_uv_loop(wasi->ctx);
    if (loop) {
      uv_fs_t req;
      int rc = uv_fs_close(loop, &req, entry->host_fd, NULL);
      int sys_err = uv_fs_get_system_error(&req);
      uv_fs_req_cleanup(&req);
      entry->host_fd = -1;
      if (rc < 0 || sys_err != 0) {
        close_err = sys_err != 0 ? sys_err : -rc;
      }
    } else {
      if (wasi_close_fd(entry->host_fd) != 0) {
        close_err = errno;
      }
      entry->host_fd = -1;
    }
  }

  if (close_err != 0) {
    return JS_NewInt32(ctx, wasi_errno_from_errno(close_err));
  }

  jsrt_wasi_fd_table_release(wasi, fd);

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// WASI whence values for fd_seek
#define WASI_WHENCE_SET 0  // Seek from beginning
#define WASI_WHENCE_CUR 1  // Seek from current position
#define WASI_WHENCE_END 2  // Seek from end

// fd_seek(fd: fd, offset: filedelta, whence: whence, newoffset: ptr) -> errno
static JSValue wasi_fd_seek(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                            JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_seek - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 4) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd;
  int64_t offset;
  uint32_t whence, newoffset_ptr;

  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToInt64(ctx, &offset, argv[1]) || JS_ToUint32(ctx, &whence, argv[2]) ||
      JS_ToUint32(ctx, &newoffset_ptr, argv[3])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_seek(fd=%u, offset=%lld, whence=%u, newoffset=%u)", fd, (long long)offset, whence,
             newoffset_ptr);

  jsrt_wasi_fd_entry* entry = jsrt_wasi_get_fd(wasi, fd);
  if (!entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  if (entry->filetype == __WASI_FILETYPE_CHARACTER_DEVICE) {
    return JS_NewInt32(ctx, WASI_ESPIPE);
  }

  if (entry->filetype == __WASI_FILETYPE_DIRECTORY || entry->host_fd < 0) {
    return JS_NewInt32(ctx, WASI_ENOSYS);
  }

  // Seeking real file descriptors not yet supported
  return JS_NewInt32(ctx, WASI_ENOSYS);
}

// fd_tell(fd: fd, newoffset: ptr) -> errno
static JSValue wasi_fd_tell(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                            JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_tell - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd;
  uint32_t newoffset_ptr;
  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToUint32(ctx, &newoffset_ptr, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_tell(fd=%u, newoffset_ptr=%u)", fd, newoffset_ptr);

  jsrt_wasi_fd_entry* entry = jsrt_wasi_get_fd(wasi, fd);
  if (!entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  if (entry->filetype == __WASI_FILETYPE_CHARACTER_DEVICE) {
    return JS_NewInt32(ctx, WASI_ESPIPE);
  }

  if (entry->filetype == __WASI_FILETYPE_DIRECTORY || entry->host_fd < 0) {
    return JS_NewInt32(ctx, WASI_ENOSYS);
  }

  return JS_NewInt32(ctx, WASI_ENOSYS);
}

// WASI prestat structure type
#define WASI_PREOPENTYPE_DIR 0

// fd_prestat_get(fd: fd, buf: ptr) -> errno
// Returns prestat structure: { type: u8, name_len: u32 }
static JSValue wasi_fd_prestat_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                   JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_prestat_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd, buf_ptr;
  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToUint32(ctx, &buf_ptr, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_prestat_get(fd=%u, buf=%u)", fd, buf_ptr);

  jsrt_wasi_fd_entry* entry = jsrt_wasi_get_fd(wasi, fd);
  if (!entry || !entry->preopen) {
    JSRT_Debug("WASI syscall: fd_prestat_get - fd %u not a preopen", fd);
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  const jsrt_wasi_preopen_t* preopen = entry->preopen;

  uint8_t* buf = get_wasm_memory(wasi, buf_ptr, 8);
  if (!buf) {
    JSRT_Debug("WASI syscall: fd_prestat_get - invalid memory pointer");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  size_t name_len = strlen(preopen->virtual_path);
  buf[0] = WASI_PREOPENTYPE_DIR;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 0;
  buf[4] = (name_len >> 0) & 0xFF;
  buf[5] = (name_len >> 8) & 0xFF;
  buf[6] = (name_len >> 16) & 0xFF;
  buf[7] = (name_len >> 24) & 0xFF;

  JSRT_Debug("WASI syscall: fd_prestat_get - fd %u is preopen '%s' (len=%zu)", fd, preopen->virtual_path, name_len);

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// fd_prestat_dir_name(fd: fd, path: ptr, path_len: size) -> errno
static JSValue wasi_fd_prestat_dir_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                        JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_prestat_dir_name - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 3) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd, path_ptr, path_len;
  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToUint32(ctx, &path_ptr, argv[1]) || JS_ToUint32(ctx, &path_len, argv[2])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_prestat_dir_name(fd=%u, path=%u, path_len=%u)", fd, path_ptr, path_len);

  jsrt_wasi_fd_entry* entry = jsrt_wasi_get_fd(wasi, fd);
  if (!entry || !entry->preopen) {
    JSRT_Debug("WASI syscall: fd_prestat_dir_name - fd %u not a preopen", fd);
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  const jsrt_wasi_preopen_t* preopen = entry->preopen;
  size_t name_len = strlen(preopen->virtual_path);

  // Check if buffer is large enough
  if (path_len < name_len) {
    JSRT_Debug("WASI syscall: fd_prestat_dir_name - buffer too small (%u < %zu)", path_len, name_len);
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get buffer from WASM memory
  uint8_t* path_buf = get_wasm_memory(wasi, path_ptr, path_len);
  if (!path_buf) {
    JSRT_Debug("WASI syscall: fd_prestat_dir_name - invalid memory pointer");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Copy the virtual path (without null terminator - WASI doesn't include it)
  memcpy(path_buf, preopen->virtual_path, name_len);

  JSRT_Debug("WASI syscall: fd_prestat_dir_name - fd %u -> '%s'", fd, preopen->virtual_path);

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// fd_fdstat_get(fd: fd, buf: ptr) -> errno
static JSValue wasi_fd_fdstat_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                  JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_fdstat_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd;
  uint32_t fdstat_ptr;
  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToUint32(ctx, &fdstat_ptr, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_fdstat_get(fd=%u, fdstat_ptr=%u)", fd, fdstat_ptr);

  jsrt_wasi_fd_entry* entry = jsrt_wasi_get_fd(wasi, fd);
  if (!entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  uint8_t* fdstat_mem = get_wasm_memory(wasi, fdstat_ptr, sizeof(__wasi_fdstat_t));
  if (!fdstat_mem) {
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  memset(fdstat_mem, 0, sizeof(__wasi_fdstat_t));

  fdstat_mem[0] = entry->filetype;
  fdstat_mem[1] = 0;
  write_u16_le(&fdstat_mem[2], entry->fd_flags);
  memset(&fdstat_mem[4], 0, 4);
  write_u64_le(&fdstat_mem[8], entry->rights_base);
  write_u64_le(&fdstat_mem[16], entry->rights_inheriting);

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// fd_fdstat_set_flags(fd: fd, flags: fdflags) -> errno
static JSValue wasi_fd_fdstat_set_flags(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                        JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_fdstat_set_flags - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd;
  uint32_t flags;
  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToUint32(ctx, &flags, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_fdstat_set_flags(fd=%u, flags=0x%x)", fd, flags);

  jsrt_wasi_fd_entry* entry = jsrt_wasi_get_fd(wasi, fd);
  if (!entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  if (flags == entry->fd_flags) {
    return JS_NewInt32(ctx, WASI_ESUCCESS);
  }

  return JS_NewInt32(ctx, WASI_ENOSYS);
}

// path_open(dirfd, dirflags, path, path_len, oflags, rights_base, rights_inheriting, fd_flags, opened_fd) -> errno
static JSValue wasi_path_open(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                              JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 9) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t dirfd;
  uint32_t dirflags;
  uint32_t path_ptr;
  uint32_t path_len;
  uint32_t oflags;
  int64_t rights_base_i64;
  int64_t rights_inheriting_i64;
  uint32_t fd_flags;
  uint32_t opened_fd_ptr;

  if (JS_ToUint32(ctx, &dirfd, argv[0]) || JS_ToUint32(ctx, &dirflags, argv[1]) ||
      JS_ToUint32(ctx, &path_ptr, argv[2]) || JS_ToUint32(ctx, &path_len, argv[3]) ||
      JS_ToUint32(ctx, &oflags, argv[4]) || JS_ToInt64(ctx, &rights_base_i64, argv[5]) ||
      JS_ToInt64(ctx, &rights_inheriting_i64, argv[6]) || JS_ToUint32(ctx, &fd_flags, argv[7]) ||
      JS_ToUint32(ctx, &opened_fd_ptr, argv[8])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  (void)dirflags;  // Currently unused

  uint8_t* opened_fd_mem = get_wasm_memory(wasi, opened_fd_ptr, sizeof(uint32_t));
  if (!opened_fd_mem) {
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  jsrt_wasi_fd_entry* dir_entry = jsrt_wasi_get_fd(wasi, dirfd);
  if (!dir_entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  if (dir_entry->filetype != __WASI_FILETYPE_DIRECTORY) {
    return JS_NewInt32(ctx, WASI_ENOTDIR);
  }

  if (!dir_entry->preopen) {
    return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
  }

  if (!wasi_has_rights(dir_entry, __WASI_RIGHT_PATH_OPEN)) {
    return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
  }

  if ((oflags & __WASI_O_CREAT) && !wasi_has_rights(dir_entry, __WASI_RIGHT_PATH_CREATE_FILE)) {
    return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
  }

  char* host_path = NULL;
  uint32_t status = wasi_resolve_path(wasi, dir_entry, path_ptr, path_len, false, &host_path);
  if (status != WASI_ESUCCESS) {
    return JS_NewInt32(ctx, status);
  }

  JSRT_Debug("WASI path_open host path: %s", host_path);

  uv_loop_t* loop = wasi_get_uv_loop(wasi->ctx);
  if (!loop) {
    free(host_path);
    return JS_NewInt32(ctx, WASI_ENOSYS);
  }

  uint64_t rights_base = (uint64_t)rights_base_i64;
  uint64_t rights_inheriting = (uint64_t)rights_inheriting_i64;

  bool can_read = (rights_base & __WASI_RIGHT_FD_READ) != 0;
  bool can_write = (rights_base & __WASI_RIGHT_FD_WRITE) != 0;

  int flags = 0;
  if (can_read && can_write) {
    flags |= O_RDWR;
  } else if (can_write) {
    flags |= O_WRONLY;
  } else {
    flags |= O_RDONLY;
  }

  if (oflags & __WASI_O_CREAT) {
    flags |= O_CREAT;
  }
  if (oflags & __WASI_O_TRUNC) {
    flags |= O_TRUNC;
  }
  if (oflags & __WASI_O_EXCL) {
    flags |= O_EXCL;
  }
#ifdef O_DIRECTORY
  if (oflags & __WASI_O_DIRECTORY) {
    flags |= O_DIRECTORY;
  }
#endif
  if (fd_flags & __WASI_FDFLAG_APPEND) {
    flags |= O_APPEND;
  }
#ifdef O_DSYNC
  if (fd_flags & __WASI_FDFLAG_DSYNC) {
    flags |= O_DSYNC;
  }
#endif
#ifdef O_SYNC
  if (fd_flags & (__WASI_FDFLAG_RSYNC | __WASI_FDFLAG_SYNC)) {
    flags |= O_SYNC;
  }
#endif
#ifdef O_NONBLOCK
  if (fd_flags & __WASI_FDFLAG_NONBLOCK) {
    flags |= O_NONBLOCK;
  }
#endif

  uv_fs_t open_req;
  int rc = uv_fs_open(loop, &open_req, host_path, flags, 0666, NULL);
  if (rc < 0 || open_req.result < 0) {
    int err_code = rc < 0 ? uv_translate_sys_error(rc) : uv_fs_get_system_error(&open_req);
    JSRT_Debug("WASI path_open uv_fs_open failed: rc=%d, result=%lld, errno=%d", rc, (long long)open_req.result,
               err_code);
    uint32_t wasi_err = wasi_errno_from_errno(err_code);
    uv_fs_req_cleanup(&open_req);
    free(host_path);
    return JS_NewInt32(ctx, wasi_err);
  }

  int host_fd = (int)open_req.result;
  uv_fs_req_cleanup(&open_req);

  uv_fs_t stat_req;
  uint8_t filetype = __WASI_FILETYPE_UNKNOWN;
  int stat_rc = uv_fs_fstat(loop, &stat_req, host_fd, NULL);
  int stat_err = uv_fs_get_system_error(&stat_req);
  if (stat_rc == 0 && stat_req.result == 0) {
    filetype = wasi_filetype_from_mode(stat_req.statbuf.st_mode);
    uv_fs_req_cleanup(&stat_req);
    if ((oflags & __WASI_O_DIRECTORY) && filetype != __WASI_FILETYPE_DIRECTORY) {
      uv_fs_t close_req;
      uv_fs_close(loop, &close_req, host_fd, NULL);
      uv_fs_req_cleanup(&close_req);
      free(host_path);
      return JS_NewInt32(ctx, WASI_ENOTDIR);
    }
  } else {
    uv_fs_req_cleanup(&stat_req);
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, host_fd, NULL);
    uv_fs_req_cleanup(&close_req);
    free(host_path);
    return JS_NewInt32(ctx, wasi_errno_from_errno(stat_err != 0 ? stat_err : -stat_rc));
  }

  uint32_t new_fd;
  if (jsrt_wasi_fd_table_alloc(wasi, host_fd, filetype, rights_base, rights_inheriting, (uint16_t)fd_flags, &new_fd) <
      0) {
    uv_fs_t close_req;
    uv_fs_close(loop, &close_req, host_fd, NULL);
    uv_fs_req_cleanup(&close_req);
    free(host_path);
    return JS_NewInt32(ctx, WASI_ENFILE);
  }

  opened_fd_mem[0] = (uint8_t)(new_fd & 0xFF);
  opened_fd_mem[1] = (uint8_t)((new_fd >> 8) & 0xFF);
  opened_fd_mem[2] = (uint8_t)((new_fd >> 16) & 0xFF);
  opened_fd_mem[3] = (uint8_t)((new_fd >> 24) & 0xFF);

  free(host_path);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// path_filestat_get(fd, flags, path, path_len, filestat_ptr) -> errno
static JSValue wasi_path_filestat_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                      JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 5) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t dirfd;
  uint32_t flags;
  uint32_t path_ptr;
  uint32_t path_len;
  uint32_t filestat_ptr;

  if (JS_ToUint32(ctx, &dirfd, argv[0]) || JS_ToUint32(ctx, &flags, argv[1]) || JS_ToUint32(ctx, &path_ptr, argv[2]) ||
      JS_ToUint32(ctx, &path_len, argv[3]) || JS_ToUint32(ctx, &filestat_ptr, argv[4])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint8_t* filestat_mem = get_wasm_memory(wasi, filestat_ptr, sizeof(__wasi_filestat_t));
  if (!filestat_mem) {
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  jsrt_wasi_fd_entry* dir_entry = jsrt_wasi_get_fd(wasi, dirfd);
  if (!dir_entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }
  if (dir_entry->filetype != __WASI_FILETYPE_DIRECTORY || !dir_entry->preopen) {
    return JS_NewInt32(ctx, WASI_ENOTDIR);
  }
  if (!wasi_has_rights(dir_entry, __WASI_RIGHT_PATH_FILESTAT_GET)) {
    return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
  }

  char* host_path = NULL;
  uint32_t status = wasi_resolve_path(wasi, dir_entry, path_ptr, path_len, false, &host_path);
  if (status != WASI_ESUCCESS) {
    return JS_NewInt32(ctx, status);
  }

  uv_loop_t* loop = wasi_get_uv_loop(wasi->ctx);
  if (!loop) {
    free(host_path);
    return JS_NewInt32(ctx, WASI_ENOSYS);
  }

  bool follow_symlinks = (flags & __WASI_LOOKUP_SYMLINK_FOLLOW) != 0;
  uv_fs_t req;
  int rc;
  if (follow_symlinks) {
    rc = uv_fs_stat(loop, &req, host_path, NULL);
  } else {
    rc = uv_fs_lstat(loop, &req, host_path, NULL);
  }
  int sys_err = uv_fs_get_system_error(&req);
  if (rc < 0 || req.result < 0 || sys_err != 0) {
    uint32_t wasi_err = wasi_errno_from_errno(sys_err != 0 ? sys_err : -rc);
    uv_fs_req_cleanup(&req);
    free(host_path);
    return JS_NewInt32(ctx, wasi_err);
  }

  memset(filestat_mem, 0, sizeof(__wasi_filestat_t));
  uv_stat_t* st = &req.statbuf;
  write_u64_le(&filestat_mem[0], (uint64_t)st->st_dev);
  write_u64_le(&filestat_mem[8], (uint64_t)st->st_ino);
  filestat_mem[16] = wasi_filetype_from_mode(st->st_mode);
  write_u64_le(&filestat_mem[24], (uint64_t)st->st_nlink);
  write_u64_le(&filestat_mem[32], (uint64_t)st->st_size);
  write_u64_le(&filestat_mem[40], uv_timespec_to_ns(&st->st_atim));
  write_u64_le(&filestat_mem[48], uv_timespec_to_ns(&st->st_mtim));
  write_u64_le(&filestat_mem[56], uv_timespec_to_ns(&st->st_ctim));

  uv_fs_req_cleanup(&req);
  free(host_path);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// path_create_directory(fd, path, path_len) -> errno
static JSValue wasi_path_create_directory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                                          int magic, JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 3) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t dirfd;
  uint32_t path_ptr;
  uint32_t path_len;

  if (JS_ToUint32(ctx, &dirfd, argv[0]) || JS_ToUint32(ctx, &path_ptr, argv[1]) ||
      JS_ToUint32(ctx, &path_len, argv[2])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  jsrt_wasi_fd_entry* dir_entry = jsrt_wasi_get_fd(wasi, dirfd);
  if (!dir_entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }
  if (!dir_entry->preopen || dir_entry->filetype != __WASI_FILETYPE_DIRECTORY) {
    return JS_NewInt32(ctx, WASI_ENOTDIR);
  }
  if (!wasi_has_rights(dir_entry, __WASI_RIGHT_PATH_CREATE_DIRECTORY)) {
    return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
  }

  char* host_path = NULL;
  uint32_t status = wasi_resolve_path(wasi, dir_entry, path_ptr, path_len, false, &host_path);
  if (status != WASI_ESUCCESS) {
    return JS_NewInt32(ctx, status);
  }

  uv_loop_t* loop = wasi_get_uv_loop(wasi->ctx);
  if (!loop) {
    free(host_path);
    return JS_NewInt32(ctx, WASI_ENOSYS);
  }

  uv_fs_t req;
  int rc = uv_fs_mkdir(loop, &req, host_path, 0777, NULL);
  int sys_err = uv_fs_get_system_error(&req);
  uv_fs_req_cleanup(&req);
  free(host_path);
  if (rc < 0 || sys_err != 0) {
    return JS_NewInt32(ctx, wasi_errno_from_errno(sys_err != 0 ? sys_err : -rc));
  }

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// path_remove_directory(fd, path, path_len) -> errno
static JSValue wasi_path_remove_directory(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                                          int magic, JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 3) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t dirfd;
  uint32_t path_ptr;
  uint32_t path_len;

  if (JS_ToUint32(ctx, &dirfd, argv[0]) || JS_ToUint32(ctx, &path_ptr, argv[1]) ||
      JS_ToUint32(ctx, &path_len, argv[2])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  jsrt_wasi_fd_entry* dir_entry = jsrt_wasi_get_fd(wasi, dirfd);
  if (!dir_entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }
  if (!dir_entry->preopen || dir_entry->filetype != __WASI_FILETYPE_DIRECTORY) {
    return JS_NewInt32(ctx, WASI_ENOTDIR);
  }
  if (!wasi_has_rights(dir_entry, __WASI_RIGHT_PATH_REMOVE_DIRECTORY)) {
    return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
  }

  char* host_path = NULL;
  uint32_t status = wasi_resolve_path(wasi, dir_entry, path_ptr, path_len, false, &host_path);
  if (status != WASI_ESUCCESS) {
    return JS_NewInt32(ctx, status);
  }

  uv_loop_t* loop = wasi_get_uv_loop(wasi->ctx);
  if (!loop) {
    free(host_path);
    return JS_NewInt32(ctx, WASI_ENOSYS);
  }

  uv_fs_t req;
  int rc = uv_fs_rmdir(loop, &req, host_path, NULL);
  int sys_err = uv_fs_get_system_error(&req);
  uv_fs_req_cleanup(&req);
  free(host_path);

  if (rc < 0 || sys_err != 0) {
    return JS_NewInt32(ctx, wasi_errno_from_errno(sys_err != 0 ? sys_err : -rc));
  }

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// path_unlink_file(fd, path, path_len) -> errno
static JSValue wasi_path_unlink_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                     JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 3) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t dirfd;
  uint32_t path_ptr;
  uint32_t path_len;

  if (JS_ToUint32(ctx, &dirfd, argv[0]) || JS_ToUint32(ctx, &path_ptr, argv[1]) ||
      JS_ToUint32(ctx, &path_len, argv[2])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  jsrt_wasi_fd_entry* dir_entry = jsrt_wasi_get_fd(wasi, dirfd);
  if (!dir_entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }
  if (!dir_entry->preopen || dir_entry->filetype != __WASI_FILETYPE_DIRECTORY) {
    return JS_NewInt32(ctx, WASI_ENOTDIR);
  }
  if (!wasi_has_rights(dir_entry, __WASI_RIGHT_PATH_UNLINK_FILE)) {
    return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
  }

  char* host_path = NULL;
  uint32_t status = wasi_resolve_path(wasi, dir_entry, path_ptr, path_len, false, &host_path);
  if (status != WASI_ESUCCESS) {
    return JS_NewInt32(ctx, status);
  }

  uv_loop_t* loop = wasi_get_uv_loop(wasi->ctx);
  if (!loop) {
    free(host_path);
    return JS_NewInt32(ctx, WASI_ENOSYS);
  }

  uv_fs_t req;
  int rc = uv_fs_unlink(loop, &req, host_path, NULL);
  int sys_err = uv_fs_get_system_error(&req);
  uv_fs_req_cleanup(&req);
  free(host_path);

  if (rc < 0 || sys_err != 0) {
    return JS_NewInt32(ctx, wasi_errno_from_errno(sys_err != 0 ? sys_err : -rc));
  }

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// path_rename(old_fd, old_path, old_len, new_fd, new_path, new_len) -> errno
static JSValue wasi_path_rename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 6) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t old_fd;
  uint32_t old_path_ptr;
  uint32_t old_path_len;
  uint32_t new_fd;
  uint32_t new_path_ptr;
  uint32_t new_path_len;

  if (JS_ToUint32(ctx, &old_fd, argv[0]) || JS_ToUint32(ctx, &old_path_ptr, argv[1]) ||
      JS_ToUint32(ctx, &old_path_len, argv[2]) || JS_ToUint32(ctx, &new_fd, argv[3]) ||
      JS_ToUint32(ctx, &new_path_ptr, argv[4]) || JS_ToUint32(ctx, &new_path_len, argv[5])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  jsrt_wasi_fd_entry* old_entry = jsrt_wasi_get_fd(wasi, old_fd);
  jsrt_wasi_fd_entry* new_entry = jsrt_wasi_get_fd(wasi, new_fd);
  if (!old_entry || !new_entry) {
    return JS_NewInt32(ctx, WASI_EBADF);
  }
  if (!old_entry->preopen || !new_entry->preopen) {
    return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
  }
  if (!wasi_has_rights(old_entry, __WASI_RIGHT_PATH_RENAME_SOURCE) ||
      !wasi_has_rights(new_entry, __WASI_RIGHT_PATH_RENAME_TARGET)) {
    return JS_NewInt32(ctx, WASI_ENOTCAPABLE);
  }

  char* old_host_path = NULL;
  char* new_host_path = NULL;
  uint32_t status = wasi_resolve_path(wasi, old_entry, old_path_ptr, old_path_len, false, &old_host_path);
  if (status != WASI_ESUCCESS) {
    return JS_NewInt32(ctx, status);
  }
  status = wasi_resolve_path(wasi, new_entry, new_path_ptr, new_path_len, false, &new_host_path);
  if (status != WASI_ESUCCESS) {
    free(old_host_path);
    return JS_NewInt32(ctx, status);
  }

  uv_loop_t* loop = wasi_get_uv_loop(wasi->ctx);
  if (!loop) {
    free(old_host_path);
    free(new_host_path);
    return JS_NewInt32(ctx, WASI_ENOSYS);
  }

  uv_fs_t req;
  int rc = uv_fs_rename(loop, &req, old_host_path, new_host_path, NULL);
  int sys_err = uv_fs_get_system_error(&req);
  uv_fs_req_cleanup(&req);
  free(old_host_path);
  free(new_host_path);

  if (rc < 0 || sys_err != 0) {
    return JS_NewInt32(ctx, wasi_errno_from_errno(sys_err != 0 ? sys_err : -rc));
  }

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// poll_oneoff(in, out, nsubscriptions, nevents_ptr) -> errno
static JSValue wasi_poll_oneoff(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                JSValue* func_data) {
  (void)ctx;
  (void)this_val;
  (void)argv;
  (void)magic;
  (void)func_data;
  if (argc != 4) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }
  return JS_NewInt32(ctx, WASI_ENOSYS);
}

// sock_accept(fd, fdflags, newfd_ptr) -> errno
static JSValue wasi_sock_accept(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                JSValue* func_data) {
  (void)ctx;
  (void)this_val;
  (void)argv;
  (void)magic;
  (void)func_data;
  if (argc != 3) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }
  return JS_NewInt32(ctx, WASI_ENOSYS);
}

// sock_recv(fd, iovs, iovs_len, ri_flags, ro_datalen, ro_flags) -> errno
static JSValue wasi_sock_recv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                              JSValue* func_data) {
  (void)ctx;
  (void)this_val;
  (void)argv;
  (void)magic;
  (void)func_data;
  if (argc != 6) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }
  return JS_NewInt32(ctx, WASI_ENOSYS);
}

// sock_send(fd, ciovs, ciovs_len, si_flags, so_datalen_ptr) -> errno
static JSValue wasi_sock_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                              JSValue* func_data) {
  (void)ctx;
  (void)this_val;
  (void)argv;
  (void)magic;
  (void)func_data;
  if (argc != 5) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }
  return JS_NewInt32(ctx, WASI_ENOSYS);
}

// sock_shutdown(fd, how) -> errno
static JSValue wasi_sock_shutdown(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                  JSValue* func_data) {
  (void)ctx;
  (void)this_val;
  (void)argv;
  (void)magic;
  (void)func_data;
  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }
  return JS_NewInt32(ctx, WASI_ENOSYS);
}

// proc_exit(rval: exitcode)
// Note: proc_exit doesn't return a value - it terminates the process
static JSValue wasi_proc_exit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                              JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);

  // Get exit code argument
  uint32_t exit_code = 0;
  if (argc >= 1) {
    JS_ToUint32(ctx, &exit_code, argv[0]);
  }

  JSRT_Debug("WASI syscall: proc_exit(exitcode=%u)", exit_code);

  if (wasi) {
    wasi->exit_code = (int)exit_code;
    wasi->exit_requested = true;
    if (wasi->wamr_instance) {
      wasm_runtime_set_exception(wasi->wamr_instance, "WASI proc_exit");
    }

    if (!wasi->options.return_on_exit) {
      exit((int)exit_code);
    }
  }

  // Throw an exception to unwind execution; start()/initialize() will convert to exit code when requested
  return JS_ThrowInternalError(ctx, "WASI proc_exit called with code %u", exit_code);
}

// WASI clock IDs
#define WASI_CLOCK_REALTIME 0
#define WASI_CLOCK_MONOTONIC 1
#define WASI_CLOCK_PROCESS_CPUTIME 2
#define WASI_CLOCK_THREAD_CPUTIME 3

// clock_time_get(id: clockid, precision: timestamp, time: ptr) -> errno
static JSValue wasi_clock_time_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                   JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: clock_time_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 3) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t clock_id, precision_lo, precision_hi, time_ptr;
  if (JS_ToUint32(ctx, &clock_id, argv[0])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }
  // precision is a 64-bit timestamp, but we'll ignore it for now
  if (JS_ToUint32(ctx, &time_ptr, argv[2])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: clock_time_get(id=%u, time=%u)", clock_id, time_ptr);

  // Get time pointer from WASM memory
  uint8_t* time_mem = get_wasm_memory(wasi, time_ptr, 8);  // 64-bit timestamp
  if (!time_mem) {
    JSRT_Debug("WASI syscall: clock_time_get - invalid memory pointer");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Get current time based on clock ID
  struct timespec ts;
  clockid_t posix_clock_id;

  switch (clock_id) {
    case WASI_CLOCK_REALTIME:
      posix_clock_id = CLOCK_REALTIME;
      break;
    case WASI_CLOCK_MONOTONIC:
      posix_clock_id = CLOCK_MONOTONIC;
      break;
    case WASI_CLOCK_PROCESS_CPUTIME:
      posix_clock_id = CLOCK_PROCESS_CPUTIME_ID;
      break;
    case WASI_CLOCK_THREAD_CPUTIME:
      posix_clock_id = CLOCK_THREAD_CPUTIME_ID;
      break;
    default:
      JSRT_Debug("WASI syscall: clock_time_get - invalid clock_id %u", clock_id);
      return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (clock_gettime(posix_clock_id, &ts) != 0) {
    JSRT_Debug("WASI syscall: clock_time_get - clock_gettime failed");
    return JS_NewInt32(ctx, WASI_EIO);
  }

  // Convert to nanoseconds (WASI timestamp format)
  uint64_t timestamp = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;

  write_u64_le(time_mem, timestamp);

  JSRT_Debug("WASI syscall: clock_time_get - returned time %llu ns", (unsigned long long)timestamp);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// clock_res_get(id: clockid, resolution: ptr) -> errno
static JSValue wasi_clock_res_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                  JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: clock_res_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t clock_id;
  uint32_t resolution_ptr;
  if (JS_ToUint32(ctx, &clock_id, argv[0]) || JS_ToUint32(ctx, &resolution_ptr, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: clock_res_get(id=%u, resolution_ptr=%u)", clock_id, resolution_ptr);

  uint8_t* resolution_mem = get_wasm_memory(wasi, resolution_ptr, 8);
  if (!resolution_mem) {
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  uint64_t resolution_ns = 0;
  switch (clock_id) {
    case WASI_CLOCK_REALTIME:
    case WASI_CLOCK_MONOTONIC:
      resolution_ns = 1000;  // 1 microsecond approximation
      break;
    case WASI_CLOCK_PROCESS_CPUTIME:
    case WASI_CLOCK_THREAD_CPUTIME:
      return JS_NewInt32(ctx, WASI_ENOSYS);
    default:
      return JS_NewInt32(ctx, WASI_EINVAL);
  }

  write_u64_le(resolution_mem, resolution_ns);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// random_get(buf: ptr, buf_len: size) -> errno
static JSValue wasi_random_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                               JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(ctx, func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: random_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t buf_ptr, buf_len;
  if (JS_ToUint32(ctx, &buf_ptr, argv[0]) || JS_ToUint32(ctx, &buf_len, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: random_get(buf=%u, buf_len=%u)", buf_ptr, buf_len);

  // Get buffer from WASM memory
  uint8_t* buf = get_wasm_memory(wasi, buf_ptr, buf_len);
  if (!buf) {
    JSRT_Debug("WASI syscall: random_get - invalid memory pointer");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Read random bytes from /dev/urandom
  FILE* urandom = fopen("/dev/urandom", "rb");
  if (!urandom) {
    JSRT_Debug("WASI syscall: random_get - failed to open /dev/urandom");
    return JS_NewInt32(ctx, WASI_EIO);
  }

  size_t bytes_read = fread(buf, 1, buf_len, urandom);
  fclose(urandom);

  if (bytes_read != buf_len) {
    JSRT_Debug("WASI syscall: random_get - failed to read enough random bytes");
    return JS_NewInt32(ctx, WASI_EIO);
  }

  JSRT_Debug("WASI syscall: random_get - generated %u random bytes", buf_len);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

/**
 * Get WASI import object
 * Returns: { wasi_snapshot_preview1: { ...syscalls } }
 */
JSValue jsrt_wasi_get_import_object(JSContext* ctx, jsrt_wasi_t* wasi) {
  if (!wasi) {
    return JS_ThrowTypeError(ctx, "Invalid WASI instance");
  }

  // Check if already cached
  if (!JS_IsUndefined(wasi->import_object)) {
    return JS_DupValue(ctx, wasi->import_object);
  }

  // Determine namespace based on version
  const char* namespace_name = "wasi_snapshot_preview1";
  if (strcmp(wasi->options.version, "unstable") == 0) {
    namespace_name = "wasi_unstable";
  }

  JSRT_Debug("Creating WASI import object (namespace: %s)", namespace_name);

  // Create import object
  JSValue import_obj = JS_NewObject(ctx);

  // Create WASI namespace
  JSValue wasi_ns = JS_NewObject(ctx);

  // Create closure data array containing WASI instance pointer
  // This allows syscalls to access the WASI instance via get_wasi_instance()
  // We encode the pointer as int64 since QuickJS doesn't have JS_NewOpaque
  JSValue wasi_data[1];
  wasi_data[0] = JS_NewInt64(ctx, (int64_t)(uintptr_t)wasi);

  // Add WASI syscalls as closures with WASI instance data
  // Phase 3: Full syscall implementations
  JS_SetPropertyStr(ctx, wasi_ns, "args_get", JS_NewCFunctionData(ctx, wasi_args_get, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "args_sizes_get", JS_NewCFunctionData(ctx, wasi_args_sizes_get, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "environ_get", JS_NewCFunctionData(ctx, wasi_environ_get, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "environ_sizes_get",
                    JS_NewCFunctionData(ctx, wasi_environ_sizes_get, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_write", JS_NewCFunctionData(ctx, wasi_fd_write, 4, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_read", JS_NewCFunctionData(ctx, wasi_fd_read, 4, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_close", JS_NewCFunctionData(ctx, wasi_fd_close, 1, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_seek", JS_NewCFunctionData(ctx, wasi_fd_seek, 4, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_tell", JS_NewCFunctionData(ctx, wasi_fd_tell, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_prestat_get", JS_NewCFunctionData(ctx, wasi_fd_prestat_get, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_prestat_dir_name",
                    JS_NewCFunctionData(ctx, wasi_fd_prestat_dir_name, 3, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_fdstat_get", JS_NewCFunctionData(ctx, wasi_fd_fdstat_get, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_fdstat_set_flags",
                    JS_NewCFunctionData(ctx, wasi_fd_fdstat_set_flags, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "path_open", JS_NewCFunctionData(ctx, wasi_path_open, 9, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "path_filestat_get",
                    JS_NewCFunctionData(ctx, wasi_path_filestat_get, 5, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "path_create_directory",
                    JS_NewCFunctionData(ctx, wasi_path_create_directory, 3, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "path_remove_directory",
                    JS_NewCFunctionData(ctx, wasi_path_remove_directory, 3, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "path_unlink_file",
                    JS_NewCFunctionData(ctx, wasi_path_unlink_file, 3, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "path_rename", JS_NewCFunctionData(ctx, wasi_path_rename, 6, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "poll_oneoff", JS_NewCFunctionData(ctx, wasi_poll_oneoff, 4, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "sock_accept", JS_NewCFunctionData(ctx, wasi_sock_accept, 3, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "sock_recv", JS_NewCFunctionData(ctx, wasi_sock_recv, 6, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "sock_send", JS_NewCFunctionData(ctx, wasi_sock_send, 5, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "sock_shutdown", JS_NewCFunctionData(ctx, wasi_sock_shutdown, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "proc_exit", JS_NewCFunctionData(ctx, wasi_proc_exit, 1, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "clock_time_get", JS_NewCFunctionData(ctx, wasi_clock_time_get, 3, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "clock_res_get", JS_NewCFunctionData(ctx, wasi_clock_res_get, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "random_get", JS_NewCFunctionData(ctx, wasi_random_get, 2, 0, 1, wasi_data));

  // Free the wasi_data JSValue (it's been copied by JS_NewCFunctionData)
  JS_FreeValue(ctx, wasi_data[0]);

  // Add namespace to import object
  JS_SetPropertyStr(ctx, import_obj, namespace_name, wasi_ns);

  // Cache import object
  wasi->import_object = JS_DupValue(ctx, import_obj);

  JSRT_Debug("WASI import object created with Phase 3 syscall implementations");

  return import_obj;
}
