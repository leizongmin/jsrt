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
  JSRT_Debug("WASI syscall: fd_read (stub - returning ENOSYS)");
  return JS_NewInt32(ctx, WASI_ENOSYS);  // Not implemented
}

// fd_close(fd: fd) -> errno
static JSValue wasi_fd_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                              JSValue* func_data) {
  JSRT_Debug("WASI syscall: fd_close (stub - returning ENOSYS)");
  return JS_NewInt32(ctx, WASI_ENOSYS);  // Not implemented
}

// fd_seek(fd: fd, offset: filedelta, whence: whence, newoffset: ptr) -> errno
static JSValue wasi_fd_seek(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                             JSValue* func_data) {
  JSRT_Debug("WASI syscall: fd_seek (stub - returning ENOSYS)");
  return JS_NewInt32(ctx, WASI_ENOSYS);  // Not implemented
}

// fd_prestat_get(fd: fd, buf: ptr) -> errno
static JSValue wasi_fd_prestat_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                    JSValue* func_data) {
  JSRT_Debug("WASI syscall: fd_prestat_get (stub - returning ENOSYS)");
  return JS_NewInt32(ctx, WASI_ENOSYS);  // Not implemented
}

// fd_prestat_dir_name(fd: fd, path: ptr, path_len: size) -> errno
static JSValue wasi_fd_prestat_dir_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                         JSValue* func_data) {
  JSRT_Debug("WASI syscall: fd_prestat_dir_name (stub - returning ENOSYS)");
  return JS_NewInt32(ctx, WASI_ENOSYS);  // Not implemented
}

// proc_exit(rval: exitcode)
// Note: proc_exit doesn't return a value - it terminates the process
static JSValue wasi_proc_exit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                               JSValue* func_data) {
  JSRT_Debug("WASI syscall: proc_exit (stub - no-op)");
  // proc_exit should terminate, but in stub mode we just return
  return JS_UNDEFINED;
}

// clock_time_get(id: clockid, precision: timestamp, time: ptr) -> errno
static JSValue wasi_clock_time_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                    JSValue* func_data) {
  JSRT_Debug("WASI syscall: clock_time_get (stub - returning ENOSYS)");
  return JS_NewInt32(ctx, WASI_ENOSYS);  // Not implemented
}

// random_get(buf: ptr, buf_len: size) -> errno
static JSValue wasi_random_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                JSValue* func_data) {
  JSRT_Debug("WASI syscall: random_get (stub - returning ENOSYS)");
  return JS_NewInt32(ctx, WASI_ENOSYS);  // Not implemented
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
