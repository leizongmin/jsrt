/**
 * WASI Import Object Implementation
 *
 * Creates the import object for WebAssembly.Instance with WASI functions.
 * Phase 3: Full WASI syscall implementation.
 */

#include "../../util/debug.h"
#include "wasi.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wasm_export.h>

/**
 * WASI syscall implementations
 */

// WASI errno codes
#define WASI_ESUCCESS 0   // Success
#define WASI_EBADF 8      // Bad file descriptor
#define WASI_EINVAL 28    // Invalid argument
#define WASI_ENOSYS 52    // Function not implemented
#define WASI_EFAULT 21    // Bad address (pointer out of bounds)
#define WASI_EIO 29       // I/O error
#define WASI_ESPIPE 29    // Illegal seek (same as EIO in WASI)

/**
 * Helper: Get WASI instance from function's opaque data
 * Syscalls use JS_NewCFunctionData to capture WASI instance pointer
 */
static inline jsrt_wasi_t* get_wasi_instance(JSValue* func_data) {
  if (!func_data) {
    return NULL;
  }
  // func_data[0] contains raw pointer encoded as int64
  int64_t ptr_val;
  if (JS_ToInt64(NULL, &ptr_val, func_data[0])) {
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
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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
  uint8_t* argv_array = get_wasm_memory(wasi, argv_ptr, wasi->options.args_count * 4);  // Array of pointers (4 bytes each)
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
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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
  uint32_t buf;   // Pointer to buffer
  uint32_t len;   // Buffer length
} wasi_iovec_t;

// fd_write(fd: fd, iovs: ptr, iovs_len: size, nwritten: ptr) -> errno
static JSValue wasi_fd_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                              JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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
  int host_fd = -1;
  if (fd == 1) {
    host_fd = wasi->options.stdout_fd;
  } else if (fd == 2) {
    host_fd = wasi->options.stderr_fd;
  } else {
    JSRT_Debug("WASI syscall: fd_write - unsupported fd %u", fd);
    return JS_NewInt32(ctx, WASI_EBADF);
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
    uint32_t buf_ptr = iovs_mem[i * 8 + 0] | (iovs_mem[i * 8 + 1] << 8) | (iovs_mem[i * 8 + 2] << 16) |
                       (iovs_mem[i * 8 + 3] << 24);
    uint32_t buf_len = iovs_mem[i * 8 + 4] | (iovs_mem[i * 8 + 5] << 8) | (iovs_mem[i * 8 + 6] << 16) |
                       (iovs_mem[i * 8 + 7] << 24);

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
    ssize_t written = write(host_fd, buf, buf_len);
    if (written < 0) {
      JSRT_Debug("WASI syscall: fd_write - write failed");
      return JS_NewInt32(ctx, WASI_EIO);
    }
    total_written += written;
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
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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
  int host_fd = -1;
  if (fd == 0) {
    host_fd = wasi->options.stdin_fd;
  } else {
    JSRT_Debug("WASI syscall: fd_read - unsupported fd %u", fd);
    return JS_NewInt32(ctx, WASI_EBADF);
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
    uint32_t buf_ptr = iovs_mem[i * 8 + 0] | (iovs_mem[i * 8 + 1] << 8) | (iovs_mem[i * 8 + 2] << 16) |
                       (iovs_mem[i * 8 + 3] << 24);
    uint32_t buf_len = iovs_mem[i * 8 + 4] | (iovs_mem[i * 8 + 5] << 8) | (iovs_mem[i * 8 + 6] << 16) |
                       (iovs_mem[i * 8 + 7] << 24);

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
      JSRT_Debug("WASI syscall: fd_read - read failed");
      return JS_NewInt32(ctx, WASI_EIO);
    }
    total_read += bytes_read;

    // If we got less than requested, stop (EOF or would block)
    if ((size_t)bytes_read < buf_len) {
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
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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

  // We don't actually close stdin/stdout/stderr
  // For preopened directories (fd >= 3), we also don't manage real file descriptors yet
  // This is a no-op that returns success
  // TODO: Implement actual file descriptor management in future phases

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// WASI whence values for fd_seek
#define WASI_WHENCE_SET 0  // Seek from beginning
#define WASI_WHENCE_CUR 1  // Seek from current position
#define WASI_WHENCE_END 2  // Seek from end

// fd_seek(fd: fd, offset: filedelta, whence: whence, newoffset: ptr) -> errno
static JSValue wasi_fd_seek(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                             JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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

  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToInt64(ctx, &offset, argv[1]) ||
      JS_ToUint32(ctx, &whence, argv[2]) || JS_ToUint32(ctx, &newoffset_ptr, argv[3])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_seek(fd=%u, offset=%lld, whence=%u, newoffset=%u)",
             fd, (long long)offset, whence, newoffset_ptr);

  // stdin/stdout/stderr are not seekable
  if (fd <= 2) {
    JSRT_Debug("WASI syscall: fd_seek - cannot seek on stdin/stdout/stderr");
    return JS_NewInt32(ctx, WASI_ESPIPE);  // Illegal seek
  }

  // For now, we don't support seeking on preopened directories
  // This would require actual file descriptor management
  // Return ENOSYS for unsupported file descriptors
  JSRT_Debug("WASI syscall: fd_seek - not implemented for fd %u", fd);
  return JS_NewInt32(ctx, WASI_ENOSYS);
}

// WASI prestat structure type
#define WASI_PREOPENTYPE_DIR 0

// fd_prestat_get(fd: fd, buf: ptr) -> errno
// Returns prestat structure: { type: u8, name_len: u32 }
static JSValue wasi_fd_prestat_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                    JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_prestat_get - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 2) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd, buf_ptr;
  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToUint32(ctx, &buf_ptr, argv[1])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_prestat_get(fd=%u, buf=%u)", fd, buf_ptr);

  // FDs 0-2 are stdin/stdout/stderr, not preopened directories
  if (fd < 3) {
    JSRT_Debug("WASI syscall: fd_prestat_get - fd %u is not a preopened directory", fd);
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  // Calculate preopen index (fd - 3, since 0-2 are stdio)
  uint32_t preopen_idx = fd - 3;

  // Check if this FD is a valid preopen
  if (preopen_idx >= wasi->options.preopen_count) {
    JSRT_Debug("WASI syscall: fd_prestat_get - fd %u out of range (only %zu preopens)",
               fd, wasi->options.preopen_count);
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  // Get the preopen
  jsrt_wasi_preopen_t* preopen = &wasi->options.preopens[preopen_idx];

  // Get buffer from WASM memory (prestat structure is 8 bytes: u8 type + 3 bytes padding + u32 name_len)
  uint8_t* buf = get_wasm_memory(wasi, buf_ptr, 8);
  if (!buf) {
    JSRT_Debug("WASI syscall: fd_prestat_get - invalid memory pointer");
    return JS_NewInt32(ctx, WASI_EFAULT);
  }

  // Calculate virtual path length
  size_t name_len = strlen(preopen->virtual_path);

  // Write prestat structure (little-endian)
  buf[0] = WASI_PREOPENTYPE_DIR;  // type: directory
  buf[1] = 0;  // padding
  buf[2] = 0;  // padding
  buf[3] = 0;  // padding
  buf[4] = (name_len >> 0) & 0xFF;  // name_len (u32)
  buf[5] = (name_len >> 8) & 0xFF;
  buf[6] = (name_len >> 16) & 0xFF;
  buf[7] = (name_len >> 24) & 0xFF;

  JSRT_Debug("WASI syscall: fd_prestat_get - fd %u is preopen '%s' (len=%zu)",
             fd, preopen->virtual_path, name_len);

  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// fd_prestat_dir_name(fd: fd, path: ptr, path_len: size) -> errno
static JSValue wasi_fd_prestat_dir_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                         JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
  if (!wasi || !wasi->wamr_instance) {
    JSRT_Debug("WASI syscall: fd_prestat_dir_name - no WAMR instance");
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  // Get arguments from JS
  if (argc != 3) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  uint32_t fd, path_ptr, path_len;
  if (JS_ToUint32(ctx, &fd, argv[0]) || JS_ToUint32(ctx, &path_ptr, argv[1]) ||
      JS_ToUint32(ctx, &path_len, argv[2])) {
    return JS_NewInt32(ctx, WASI_EINVAL);
  }

  JSRT_Debug("WASI syscall: fd_prestat_dir_name(fd=%u, path=%u, path_len=%u)", fd, path_ptr, path_len);

  // FDs 0-2 are stdin/stdout/stderr, not preopened directories
  if (fd < 3) {
    JSRT_Debug("WASI syscall: fd_prestat_dir_name - fd %u is not a preopened directory", fd);
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  // Calculate preopen index
  uint32_t preopen_idx = fd - 3;

  // Check if this FD is a valid preopen
  if (preopen_idx >= wasi->options.preopen_count) {
    JSRT_Debug("WASI syscall: fd_prestat_dir_name - fd %u out of range", fd);
    return JS_NewInt32(ctx, WASI_EBADF);
  }

  // Get the preopen
  jsrt_wasi_preopen_t* preopen = &wasi->options.preopens[preopen_idx];
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

// proc_exit(rval: exitcode)
// Note: proc_exit doesn't return a value - it terminates the process
static JSValue wasi_proc_exit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                               JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);

  // Get exit code argument
  uint32_t exit_code = 0;
  if (argc >= 1) {
    JS_ToUint32(ctx, &exit_code, argv[0]);
  }

  JSRT_Debug("WASI syscall: proc_exit(exitcode=%u)", exit_code);

  // Store exit code in WASI instance
  if (wasi) {
    wasi->exit_code = (int)exit_code;
  }

  // Throw an exception to terminate WASM execution
  // This mimics the behavior of proc_exit without calling exit()
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
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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

  // Write timestamp (little-endian, 64-bit)
  time_mem[0] = (timestamp >> 0) & 0xFF;
  time_mem[1] = (timestamp >> 8) & 0xFF;
  time_mem[2] = (timestamp >> 16) & 0xFF;
  time_mem[3] = (timestamp >> 24) & 0xFF;
  time_mem[4] = (timestamp >> 32) & 0xFF;
  time_mem[5] = (timestamp >> 40) & 0xFF;
  time_mem[6] = (timestamp >> 48) & 0xFF;
  time_mem[7] = (timestamp >> 56) & 0xFF;

  JSRT_Debug("WASI syscall: clock_time_get - returned time %llu ns", (unsigned long long)timestamp);
  return JS_NewInt32(ctx, WASI_ESUCCESS);
}

// random_get(buf: ptr, buf_len: size) -> errno
static JSValue wasi_random_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                JSValue* func_data) {
  jsrt_wasi_t* wasi = get_wasi_instance(func_data);
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
  JS_SetPropertyStr(ctx, wasi_ns, "fd_prestat_get", JS_NewCFunctionData(ctx, wasi_fd_prestat_get, 2, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_prestat_dir_name",
                    JS_NewCFunctionData(ctx, wasi_fd_prestat_dir_name, 3, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "proc_exit", JS_NewCFunctionData(ctx, wasi_proc_exit, 1, 0, 1, wasi_data));
  JS_SetPropertyStr(ctx, wasi_ns, "clock_time_get", JS_NewCFunctionData(ctx, wasi_clock_time_get, 3, 0, 1, wasi_data));
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
