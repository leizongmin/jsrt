/**
 * WASI Module Header
 *
 * Node.js-compatible WASI (WebAssembly System Interface) implementation for jsrt.
 * Provides the WASI class for sandboxed WebAssembly module execution.
 */

#ifndef JSRT_NODE_WASI_H
#define JSRT_NODE_WASI_H

#include <quickjs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wasm_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Preopen directory mapping
 * Maps virtual paths (in WASM) to real filesystem paths (on host)
 */
typedef struct {
  char* virtual_path;  // Virtual path in WASM (e.g., "/sandbox")
  char* real_path;     // Real filesystem path (e.g., "/tmp/wasm")
} jsrt_wasi_preopen_t;

typedef struct jsrt_wasi_t jsrt_wasi_t;
typedef struct jsrt_wasi_options_t jsrt_wasi_options_t;

typedef struct {
  bool in_use;
  int host_fd;
  uint64_t rights_base;
  uint64_t rights_inheriting;
  uint16_t fd_flags;
  uint8_t filetype;
  const jsrt_wasi_preopen_t* preopen;  // Non-NULL for preopened directories
} jsrt_wasi_fd_entry;

/**
 * WASI options structure
 * Matches Node.js WASI constructor options
 */
typedef struct jsrt_wasi_options_t {
  // Command-line arguments
  char** args;        // NULL-terminated array of argument strings
  size_t args_count;  // Number of arguments (excluding NULL terminator)

  // Environment variables
  char** env;        // NULL-terminated array of "KEY=VALUE" strings
  size_t env_count;  // Number of environment variables

  // Preopened directories (sandboxed filesystem access)
  jsrt_wasi_preopen_t* preopens;
  size_t preopen_count;

  // Standard I/O file descriptors
  int stdin_fd;   // Default: 0
  int stdout_fd;  // Default: 1
  int stderr_fd;  // Default: 2

  // Behavior options
  bool return_on_exit;  // Return exit code instead of calling process.exit()

  // WASI version
  char* version;  // "preview1" (default) or "unstable"
} jsrt_wasi_options_t;

/**
 * WASI instance structure
 * Opaque handle to WASI instance state
 */
typedef struct jsrt_wasi_t {
  JSContext* ctx;  // JavaScript context

  // Configuration
  jsrt_wasi_options_t options;

  // WAMR integration
  // Lifetime model: wasm_instance (JS object) owns the WAMR instance.
  // We hold a strong reference (JS_DupValue) to prevent GC while WASI is alive.
  // The Instance finalizer handles WAMR cleanup when both are garbage collected.
  wasm_module_inst_t wamr_instance;  // WAMR instance extracted from wasm_instance
  wasm_exec_env_t exec_env;          // WAMR execution environment (created in start/initialize)

  // JavaScript objects
  JSValue wasm_instance;  // WebAssembly.Instance (JS object, strong reference)
  JSValue import_object;  // Cached import object from getImportObject()

  // State tracking
  bool started;           // Has start() been called?
  bool initialized;       // Has initialize() been called?
  int exit_code;          // Exit code from _start (if return_on_exit=true)
  bool exit_requested;    // proc_exit invoked during execution
  bool memory_validated;  // Default memory export present
  bool instance_failed;   // Permanent failure state after invalid attachment

  // File descriptor table
  jsrt_wasi_fd_entry* fd_table;
  size_t fd_table_capacity;
  size_t fd_table_count;
} jsrt_wasi_t;

/**
 * Create WASI instance
 * @param ctx JavaScript context
 * @param options WASI configuration options
 * @return WASI instance or NULL on error
 */
jsrt_wasi_t* jsrt_wasi_new(JSContext* ctx, const jsrt_wasi_options_t* options);

/**
 * Free WASI instance
 * @param wasi WASI instance to free
 */
void jsrt_wasi_free(jsrt_wasi_t* wasi);

/**
 * Parse WASI options from JavaScript object
 * @param ctx JavaScript context
 * @param options_obj JavaScript options object
 * @param options Output options structure
 * @return 0 on success, -1 on error (exception set)
 */
int jsrt_wasi_parse_options(JSContext* ctx, JSValue options_obj, jsrt_wasi_options_t* options);

/**
 * Free WASI options
 * @param options Options structure to free
 */
void jsrt_wasi_free_options(jsrt_wasi_options_t* options);

int jsrt_wasi_init_fd_table(jsrt_wasi_t* wasi);
jsrt_wasi_fd_entry* jsrt_wasi_get_fd(jsrt_wasi_t* wasi, uint32_t fd);
int jsrt_wasi_fd_table_alloc(jsrt_wasi_t* wasi, int host_fd, uint8_t filetype, uint64_t rights_base,
                             uint64_t rights_inheriting, uint16_t fd_flags, uint32_t* out_fd);
int jsrt_wasi_fd_table_release(jsrt_wasi_t* wasi, uint32_t fd);

/**
 * Get import object for WebAssembly.Instance
 * @param ctx JavaScript context
 * @param wasi WASI instance
 * @return Import object (JavaScript object)
 */
JSValue jsrt_wasi_get_import_object(JSContext* ctx, jsrt_wasi_t* wasi);

/**
 * Start WASI instance (call _start export)
 * @param ctx JavaScript context
 * @param wasi WASI instance
 * @param instance WebAssembly.Instance
 * @return Exit code (if return_on_exit=true) or undefined
 */
JSValue jsrt_wasi_start(JSContext* ctx, jsrt_wasi_t* wasi, JSValue instance);

/**
 * Initialize WASI instance (call _initialize export)
 * @param ctx JavaScript context
 * @param wasi WASI instance
 * @param instance WebAssembly.Instance
 * @return undefined on success, exception on error
 */
JSValue jsrt_wasi_initialize(JSContext* ctx, jsrt_wasi_t* wasi, JSValue instance);

/**
 * Initialize WASI module (register with module system)
 * @param ctx JavaScript context
 * @return WASI module exports object
 */
JSValue JSRT_InitNodeWASI(JSContext* ctx);

/**
 * Initialize WASI module (ESM variant)
 * @param ctx JavaScript context
 * @param m Module definition
 * @return 0 on success, -1 on error
 */
int js_node_wasi_init(JSContext* ctx, JSModuleDef* m);

/**
 * Check if module is WASI
 * @param name Module name (without prefix)
 * @return true if WASI module
 */
bool JSRT_IsWASIModule(const char* name);

typedef enum {
  JSRT_WASI_ERROR_INVALID_ARGUMENT = 0,
  JSRT_WASI_ERROR_INVALID_INSTANCE,
  JSRT_WASI_ERROR_MISSING_MEMORY_EXPORT,
  JSRT_WASI_ERROR_MISSING_START_EXPORT,
  JSRT_WASI_ERROR_ALREADY_STARTED,
  JSRT_WASI_ERROR_ALREADY_INITIALIZED,
  JSRT_WASI_ERROR_INTERNAL,
} jsrt_wasi_error_t;

const char* jsrt_wasi_error_to_string(jsrt_wasi_error_t code);
JSValue jsrt_wasi_throw_error(JSContext* ctx, jsrt_wasi_error_t code, const char* detail);

#ifdef __cplusplus
}
#endif

#endif /* JSRT_NODE_WASI_H */
