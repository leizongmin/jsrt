/**
 * WASI Import Object Implementation
 *
 * Creates the import object for WebAssembly.Instance with WASI functions.
 * Full WASI syscall implementation will be added in Phase 3.
 */

#include "../../util/debug.h"
#include "wasi.h"

#include <string.h>

/**
 * Stub WASI syscall implementations (Phase 3 will implement these)
 */

// args_get(argv: ptr, argv_buf: ptr) -> errno
static JSValue wasi_args_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: args_get (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// args_sizes_get(argc: ptr, argv_buf_size: ptr) -> errno
static JSValue wasi_args_sizes_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: args_sizes_get (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// environ_get(environ: ptr, environ_buf: ptr) -> errno
static JSValue wasi_environ_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: environ_get (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// environ_sizes_get(environc: ptr, environ_buf_size: ptr) -> errno
static JSValue wasi_environ_sizes_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: environ_sizes_get (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// fd_write(fd: fd, iovs: ptr, iovs_len: size, nwritten: ptr) -> errno
static JSValue wasi_fd_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: fd_write (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// fd_read(fd: fd, iovs: ptr, iovs_len: size, nread: ptr) -> errno
static JSValue wasi_fd_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: fd_read (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// fd_close(fd: fd) -> errno
static JSValue wasi_fd_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: fd_close (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// fd_seek(fd: fd, offset: filedelta, whence: whence, newoffset: ptr) -> errno
static JSValue wasi_fd_seek(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: fd_seek (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// fd_prestat_get(fd: fd, buf: ptr) -> errno
static JSValue wasi_fd_prestat_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: fd_prestat_get (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// fd_prestat_dir_name(fd: fd, path: ptr, path_len: size) -> errno
static JSValue wasi_fd_prestat_dir_name(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: fd_prestat_dir_name (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// proc_exit(rval: exitcode)
static JSValue wasi_proc_exit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: proc_exit (stub)");
  return JS_UNDEFINED;
}

// clock_time_get(id: clockid, precision: timestamp, time: ptr) -> errno
static JSValue wasi_clock_time_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: clock_time_get (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
}

// random_get(buf: ptr, buf_len: size) -> errno
static JSValue wasi_random_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("WASI syscall: random_get (stub)");
  return JS_NewInt32(ctx, 0);  // ESUCCESS
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

  // Add stub WASI syscalls
  // Phase 3 will replace these with full implementations
  JS_SetPropertyStr(ctx, wasi_ns, "args_get", JS_NewCFunction(ctx, wasi_args_get, "args_get", 2));
  JS_SetPropertyStr(ctx, wasi_ns, "args_sizes_get", JS_NewCFunction(ctx, wasi_args_sizes_get, "args_sizes_get", 2));
  JS_SetPropertyStr(ctx, wasi_ns, "environ_get", JS_NewCFunction(ctx, wasi_environ_get, "environ_get", 2));
  JS_SetPropertyStr(ctx, wasi_ns, "environ_sizes_get",
                    JS_NewCFunction(ctx, wasi_environ_sizes_get, "environ_sizes_get", 2));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_write", JS_NewCFunction(ctx, wasi_fd_write, "fd_write", 4));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_read", JS_NewCFunction(ctx, wasi_fd_read, "fd_read", 4));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_close", JS_NewCFunction(ctx, wasi_fd_close, "fd_close", 1));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_seek", JS_NewCFunction(ctx, wasi_fd_seek, "fd_seek", 4));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_prestat_get", JS_NewCFunction(ctx, wasi_fd_prestat_get, "fd_prestat_get", 2));
  JS_SetPropertyStr(ctx, wasi_ns, "fd_prestat_dir_name",
                    JS_NewCFunction(ctx, wasi_fd_prestat_dir_name, "fd_prestat_dir_name", 3));
  JS_SetPropertyStr(ctx, wasi_ns, "proc_exit", JS_NewCFunction(ctx, wasi_proc_exit, "proc_exit", 1));
  JS_SetPropertyStr(ctx, wasi_ns, "clock_time_get", JS_NewCFunction(ctx, wasi_clock_time_get, "clock_time_get", 3));
  JS_SetPropertyStr(ctx, wasi_ns, "random_get", JS_NewCFunction(ctx, wasi_random_get, "random_get", 2));

  // Add namespace to import object
  JS_SetPropertyStr(ctx, import_obj, namespace_name, wasi_ns);

  // Cache import object
  wasi->import_object = JS_DupValue(ctx, import_obj);

  JSRT_Debug("WASI import object created with stub syscalls");

  return import_obj;
}
