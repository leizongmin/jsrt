/**
 * WASI Core Implementation
 *
 * Core functionality for WASI instances:
 * - Instance creation and destruction
 * - Options parsing and validation
 * - Memory management
 */

#include "../../util/debug.h"
#include "wasi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../../../deps/wamr/core/shared/platform/include/platform_wasi_types.h"

// Default values
#define WASI_DEFAULT_VERSION "preview1"
#define WASI_DEFAULT_STDIN 0
#define WASI_DEFAULT_STDOUT 1
#define WASI_DEFAULT_STDERR 2
#define WASI_FD_TABLE_INITIAL_CAPACITY 8

static int jsrt_wasi_fd_table_set(jsrt_wasi_t* wasi, uint32_t fd, int host_fd, uint8_t filetype, uint32_t rights_base,
                                  uint32_t rights_inheriting, uint16_t fd_flags, const jsrt_wasi_preopen_t* preopen);

/**
 * Helper: Duplicate string or return NULL
 */
static char* safe_strdup(const char* str) {
  return str ? strdup(str) : NULL;
}

/**
 * Helper: Parse JavaScript array of strings
 */
static int parse_string_array(JSContext* ctx, JSValue array_val, char*** out_array, size_t* out_count) {
  if (!JS_IsArray(ctx, array_val)) {
    JS_ThrowTypeError(ctx, "Expected array of strings");
    return -1;
  }

  // Get array length
  JSValue len_val = JS_GetPropertyStr(ctx, array_val, "length");
  uint32_t count;
  if (JS_ToUint32(ctx, &count, len_val) < 0) {
    JS_FreeValue(ctx, len_val);
    return -1;
  }
  JS_FreeValue(ctx, len_val);

  // Allocate array (+1 for NULL terminator)
  char** array = calloc(count + 1, sizeof(char*));
  if (!array) {
    JS_ThrowOutOfMemory(ctx);
    return -1;
  }

  // Parse each element
  for (uint32_t i = 0; i < count; i++) {
    JSValue item = JS_GetPropertyUint32(ctx, array_val, i);

    const char* str = JS_ToCString(ctx, item);
    JS_FreeValue(ctx, item);

    if (!str) {
      // Cleanup and return error
      for (uint32_t j = 0; j < i; j++) {
        free(array[j]);
      }
      free(array);
      return -1;
    }

    array[i] = strdup(str);
    JS_FreeCString(ctx, str);

    if (!array[i]) {
      // Cleanup and return error
      for (uint32_t j = 0; j < i; j++) {
        free(array[j]);
      }
      free(array);
      JS_ThrowOutOfMemory(ctx);
      return -1;
    }
  }

  array[count] = NULL;  // NULL terminator
  *out_array = array;
  *out_count = count;
  return 0;
}

/**
 * Helper: Parse JavaScript object into environment variables array
 */
static int parse_env_object(JSContext* ctx, JSValue env_obj, char*** out_env, size_t* out_count) {
  // Get property names
  JSPropertyEnum* props;
  uint32_t count;
  if (JS_GetOwnPropertyNames(ctx, &props, &count, env_obj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
    return -1;
  }

  // Allocate env array (+1 for NULL terminator)
  char** env = calloc(count + 1, sizeof(char*));
  if (!env) {
    js_free(ctx, props);
    JS_ThrowOutOfMemory(ctx);
    return -1;
  }

  // Parse each entry
  for (uint32_t i = 0; i < count; i++) {
    // Get key (environment variable name)
    JSValue key = JS_AtomToString(ctx, props[i].atom);
    const char* name = JS_ToCString(ctx, key);
    JS_FreeValue(ctx, key);

    // Get value
    JSValue value = JS_GetProperty(ctx, env_obj, props[i].atom);
    const char* val = JS_ToCString(ctx, value);
    JS_FreeValue(ctx, value);

    if (!name || !val) {
      // Cleanup
      JS_FreeCString(ctx, name);
      JS_FreeCString(ctx, val);
      for (uint32_t j = 0; j < i; j++) {
        free(env[j]);
      }
      free(env);
      js_free(ctx, props);
      return -1;
    }

    // Validate: no '=' in name
    if (strchr(name, '=')) {
      JS_FreeCString(ctx, name);
      JS_FreeCString(ctx, val);
      for (uint32_t j = 0; j < i; j++) {
        free(env[j]);
      }
      free(env);
      js_free(ctx, props);
      JS_ThrowTypeError(ctx, "Environment variable name cannot contain '='");
      return -1;
    }

    // Format as "KEY=VALUE"
    size_t len = strlen(name) + 1 + strlen(val) + 1;
    env[i] = malloc(len);
    if (!env[i]) {
      JS_FreeCString(ctx, name);
      JS_FreeCString(ctx, val);
      for (uint32_t j = 0; j < i; j++) {
        free(env[j]);
      }
      free(env);
      js_free(ctx, props);
      JS_ThrowOutOfMemory(ctx);
      return -1;
    }
    snprintf(env[i], len, "%s=%s", name, val);

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, val);
  }

  env[count] = NULL;  // NULL terminator
  js_free(ctx, props);

  *out_env = env;
  *out_count = count;
  return 0;
}

/**
 * Helper: Validate path exists and is a directory
 */
static bool validate_preopen_path(const char* path) {
  if (!path) {
    return false;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    return false;  // Path doesn't exist
  }

  return S_ISDIR(st.st_mode);  // Must be a directory
}

/**
 * Helper: Parse preopens object
 */
static int parse_preopens_object(JSContext* ctx, JSValue preopens_obj, jsrt_wasi_preopen_t** out_preopens,
                                 size_t* out_count) {
  // Get property names
  JSPropertyEnum* props;
  uint32_t count;
  if (JS_GetOwnPropertyNames(ctx, &props, &count, preopens_obj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) < 0) {
    return -1;
  }

  jsrt_wasi_preopen_t* preopens = NULL;
  if (count > 0) {
    // Allocate preopens array
    preopens = calloc(count, sizeof(jsrt_wasi_preopen_t));
    if (!preopens) {
      js_free(ctx, props);
      JS_ThrowOutOfMemory(ctx);
      return -1;
    }
  }

  // Parse each entry
  for (uint32_t i = 0; i < count; i++) {
    // Get virtual path (key)
    JSValue key = JS_AtomToString(ctx, props[i].atom);
    const char* virtual_path = JS_ToCString(ctx, key);
    JS_FreeValue(ctx, key);

    // Get real path (value)
    JSValue value = JS_GetProperty(ctx, preopens_obj, props[i].atom);
    const char* real_path = JS_ToCString(ctx, value);
    JS_FreeValue(ctx, value);

    if (!virtual_path || !real_path) {
      // Cleanup
      JS_FreeCString(ctx, virtual_path);
      JS_FreeCString(ctx, real_path);
      for (uint32_t j = 0; j < i; j++) {
        free(preopens[j].virtual_path);
        free(preopens[j].real_path);
      }
      free(preopens);
      js_free(ctx, props);
      return -1;
    }

    // Validate real path
    if (!validate_preopen_path(real_path)) {
      JS_ThrowTypeError(ctx, "Preopen path does not exist or is not a directory: %s", real_path);
      JS_FreeCString(ctx, virtual_path);
      JS_FreeCString(ctx, real_path);
      for (uint32_t j = 0; j < i; j++) {
        free(preopens[j].virtual_path);
        free(preopens[j].real_path);
      }
      free(preopens);
      js_free(ctx, props);
      return -1;
    }

    // Store preopen
    preopens[i].virtual_path = strdup(virtual_path);
    preopens[i].real_path = strdup(real_path);

    JS_FreeCString(ctx, virtual_path);
    JS_FreeCString(ctx, real_path);

    if (!preopens[i].virtual_path || !preopens[i].real_path) {
      for (uint32_t j = 0; j <= i; j++) {
        free(preopens[j].virtual_path);
        free(preopens[j].real_path);
      }
      free(preopens);
      js_free(ctx, props);
      JS_ThrowOutOfMemory(ctx);
      return -1;
    }
  }

  js_free(ctx, props);

  *out_preopens = preopens;
  *out_count = count;
  return 0;
}

/**
 * Parse WASI options from JavaScript object
 */
int jsrt_wasi_parse_options(JSContext* ctx, JSValue options_obj, jsrt_wasi_options_t* options) {
  if (!options) {
    return -1;
  }

  // Initialize with defaults
  memset(options, 0, sizeof(jsrt_wasi_options_t));
  options->stdin_fd = WASI_DEFAULT_STDIN;
  options->stdout_fd = WASI_DEFAULT_STDOUT;
  options->stderr_fd = WASI_DEFAULT_STDERR;
  options->return_on_exit = false;
  options->version = strdup(WASI_DEFAULT_VERSION);

  // If no options provided, use defaults
  if (JS_IsUndefined(options_obj) || JS_IsNull(options_obj)) {
    return 0;
  }

  if (!JS_IsObject(options_obj)) {
    JS_ThrowTypeError(ctx, "WASI options must be an object");
    free(options->version);
    return -1;
  }

  // Parse args (array of strings)
  JSValue args_val = JS_GetPropertyStr(ctx, options_obj, "args");
  if (!JS_IsUndefined(args_val)) {
    if (parse_string_array(ctx, args_val, &options->args, &options->args_count) < 0) {
      JS_FreeValue(ctx, args_val);
      free(options->version);
      return -1;
    }
  }
  JS_FreeValue(ctx, args_val);

  // Parse env (object)
  JSValue env_val = JS_GetPropertyStr(ctx, options_obj, "env");
  if (!JS_IsUndefined(env_val)) {
    if (!JS_IsObject(env_val)) {
      JS_ThrowTypeError(ctx, "env must be an object");
      JS_FreeValue(ctx, env_val);
      jsrt_wasi_free_options(options);
      return -1;
    }
    if (parse_env_object(ctx, env_val, &options->env, &options->env_count) < 0) {
      JS_FreeValue(ctx, env_val);
      jsrt_wasi_free_options(options);
      return -1;
    }
  }
  JS_FreeValue(ctx, env_val);

  // Parse preopens (object)
  JSValue preopens_val = JS_GetPropertyStr(ctx, options_obj, "preopens");
  if (!JS_IsUndefined(preopens_val)) {
    if (!JS_IsObject(preopens_val)) {
      JS_ThrowTypeError(ctx, "preopens must be an object");
      JS_FreeValue(ctx, preopens_val);
      jsrt_wasi_free_options(options);
      return -1;
    }
    if (parse_preopens_object(ctx, preopens_val, &options->preopens, &options->preopen_count) < 0) {
      JS_FreeValue(ctx, preopens_val);
      jsrt_wasi_free_options(options);
      return -1;
    }
  }
  JS_FreeValue(ctx, preopens_val);

  // Parse stdin (number)
  JSValue stdin_val = JS_GetPropertyStr(ctx, options_obj, "stdin");
  if (!JS_IsUndefined(stdin_val)) {
    int32_t fd;
    if (JS_ToInt32(ctx, &fd, stdin_val) < 0) {
      JS_FreeValue(ctx, stdin_val);
      jsrt_wasi_free_options(options);
      return -1;
    }
    // Validate FD is non-negative
    if (fd < 0) {
      JS_FreeValue(ctx, stdin_val);
      jsrt_wasi_free_options(options);
      JS_ThrowTypeError(ctx, "stdin file descriptor must be non-negative");
      return -1;
    }
    options->stdin_fd = fd;
  }
  JS_FreeValue(ctx, stdin_val);

  // Parse stdout (number)
  JSValue stdout_val = JS_GetPropertyStr(ctx, options_obj, "stdout");
  if (!JS_IsUndefined(stdout_val)) {
    int32_t fd;
    if (JS_ToInt32(ctx, &fd, stdout_val) < 0) {
      JS_FreeValue(ctx, stdout_val);
      jsrt_wasi_free_options(options);
      return -1;
    }
    // Validate FD is non-negative
    if (fd < 0) {
      JS_FreeValue(ctx, stdout_val);
      jsrt_wasi_free_options(options);
      JS_ThrowTypeError(ctx, "stdout file descriptor must be non-negative");
      return -1;
    }
    options->stdout_fd = fd;
  }
  JS_FreeValue(ctx, stdout_val);

  // Parse stderr (number)
  JSValue stderr_val = JS_GetPropertyStr(ctx, options_obj, "stderr");
  if (!JS_IsUndefined(stderr_val)) {
    int32_t fd;
    if (JS_ToInt32(ctx, &fd, stderr_val) < 0) {
      JS_FreeValue(ctx, stderr_val);
      jsrt_wasi_free_options(options);
      return -1;
    }
    // Validate FD is non-negative
    if (fd < 0) {
      JS_FreeValue(ctx, stderr_val);
      jsrt_wasi_free_options(options);
      JS_ThrowTypeError(ctx, "stderr file descriptor must be non-negative");
      return -1;
    }
    options->stderr_fd = fd;
  }
  JS_FreeValue(ctx, stderr_val);

  // Parse returnOnExit (boolean)
  JSValue return_on_exit_val = JS_GetPropertyStr(ctx, options_obj, "returnOnExit");
  if (!JS_IsUndefined(return_on_exit_val)) {
    options->return_on_exit = JS_ToBool(ctx, return_on_exit_val);
  }
  JS_FreeValue(ctx, return_on_exit_val);

  // Parse version (string)
  JSValue version_val = JS_GetPropertyStr(ctx, options_obj, "version");
  if (!JS_IsUndefined(version_val)) {
    const char* version = JS_ToCString(ctx, version_val);
    if (version) {
      free(options->version);
      options->version = strdup(version);
      JS_FreeCString(ctx, version);

      // Validate version
      if (strcmp(options->version, "preview1") != 0 && strcmp(options->version, "unstable") != 0) {
        JS_ThrowTypeError(ctx, "WASI version must be 'preview1' or 'unstable'");
        JS_FreeValue(ctx, version_val);
        jsrt_wasi_free_options(options);
        return -1;
      }
    }
  }
  JS_FreeValue(ctx, version_val);

  return 0;
}

/**
 * Free WASI options
 */
void jsrt_wasi_free_options(jsrt_wasi_options_t* options) {
  if (!options) {
    return;
  }

  // Free args
  if (options->args) {
    for (size_t i = 0; i < options->args_count; i++) {
      free(options->args[i]);
    }
    free(options->args);
  }

  // Free env
  if (options->env) {
    for (size_t i = 0; i < options->env_count; i++) {
      free(options->env[i]);
    }
    free(options->env);
  }

  // Free preopens
  if (options->preopens) {
    for (size_t i = 0; i < options->preopen_count; i++) {
      free(options->preopens[i].virtual_path);
      free(options->preopens[i].real_path);
    }
    free(options->preopens);
  }

  // Free version
  free(options->version);

  // Clear structure
  memset(options, 0, sizeof(jsrt_wasi_options_t));
}

static int jsrt_wasi_fd_table_ensure(jsrt_wasi_t* wasi, uint32_t fd) {
  if (fd < wasi->fd_table_capacity) {
    return 0;
  }

  size_t new_capacity = wasi->fd_table_capacity ? wasi->fd_table_capacity : WASI_FD_TABLE_INITIAL_CAPACITY;
  while (fd >= new_capacity) {
    new_capacity *= 2;
  }

  jsrt_wasi_fd_entry* new_table = realloc(wasi->fd_table, new_capacity * sizeof(jsrt_wasi_fd_entry));
  if (!new_table) {
    return -1;
  }

  // Zero initialise new slots
  if (new_capacity > wasi->fd_table_capacity) {
    size_t old_count = wasi->fd_table_capacity;
    memset(new_table + old_count, 0, (new_capacity - old_count) * sizeof(jsrt_wasi_fd_entry));
  }

  wasi->fd_table = new_table;
  wasi->fd_table_capacity = new_capacity;
  return 0;
}

static int jsrt_wasi_fd_table_set(jsrt_wasi_t* wasi, uint32_t fd, int host_fd, uint8_t filetype, uint32_t rights_base,
                                  uint32_t rights_inheriting, uint16_t fd_flags, const jsrt_wasi_preopen_t* preopen) {
  if (jsrt_wasi_fd_table_ensure(wasi, fd) < 0) {
    return -1;
  }

  jsrt_wasi_fd_entry* entry = &wasi->fd_table[fd];
  entry->in_use = true;
  entry->host_fd = host_fd;
  entry->filetype = filetype;
  entry->rights_base = rights_base;
  entry->rights_inheriting = rights_inheriting;
  entry->fd_flags = fd_flags;
  entry->preopen = preopen;

  if (fd >= wasi->fd_table_count) {
    wasi->fd_table_count = fd + 1;
  }

  return 0;
}

int jsrt_wasi_init_fd_table(jsrt_wasi_t* wasi) {
  if (!wasi) {
    return -1;
  }

  wasi->fd_table_capacity = 0;
  wasi->fd_table_count = 0;
  wasi->fd_table = NULL;

  if (jsrt_wasi_fd_table_ensure(wasi, 2) < 0) {
    return -1;
  }

  if (jsrt_wasi_fd_table_set(wasi, 0, wasi->options.stdin_fd, __WASI_FILETYPE_CHARACTER_DEVICE,
                             __WASI_RIGHT_FD_READ | __WASI_RIGHT_FD_FDSTAT_SET_FLAGS, 0, 0, NULL) < 0) {
    return -1;
  }

  if (jsrt_wasi_fd_table_set(wasi, 1, wasi->options.stdout_fd, __WASI_FILETYPE_CHARACTER_DEVICE,
                             __WASI_RIGHT_FD_WRITE | __WASI_RIGHT_FD_FDSTAT_SET_FLAGS, 0, 0, NULL) < 0) {
    return -1;
  }

  if (jsrt_wasi_fd_table_set(wasi, 2, wasi->options.stderr_fd, __WASI_FILETYPE_CHARACTER_DEVICE,
                             __WASI_RIGHT_FD_WRITE | __WASI_RIGHT_FD_FDSTAT_SET_FLAGS, 0, 0, NULL) < 0) {
    return -1;
  }

  for (size_t i = 0; i < wasi->options.preopen_count; i++) {
    const jsrt_wasi_preopen_t* preopen = &wasi->options.preopens[i];
    uint32_t fd = 3 + (uint32_t)i;
    uint32_t rights = __WASI_RIGHT_PATH_OPEN | __WASI_RIGHT_FD_READDIR | __WASI_RIGHT_PATH_FILESTAT_GET |
                      __WASI_RIGHT_PATH_FILESTAT_SET_TIMES | __WASI_RIGHT_PATH_UNLINK_FILE |
                      __WASI_RIGHT_PATH_CREATE_DIRECTORY | __WASI_RIGHT_PATH_REMOVE_DIRECTORY;
    if (jsrt_wasi_fd_table_set(wasi, fd, -1, __WASI_FILETYPE_DIRECTORY, rights, rights, 0, preopen) < 0) {
      return -1;
    }
  }

  return 0;
}

jsrt_wasi_fd_entry* jsrt_wasi_get_fd(jsrt_wasi_t* wasi, uint32_t fd) {
  if (!wasi || fd >= wasi->fd_table_capacity) {
    return NULL;
  }

  jsrt_wasi_fd_entry* entry = &wasi->fd_table[fd];
  if (!entry->in_use) {
    return NULL;
  }

  return entry;
}

/**
 * Create WASI instance
 */
jsrt_wasi_t* jsrt_wasi_new(JSContext* ctx, const jsrt_wasi_options_t* options) {
  if (!ctx || !options) {
    return NULL;
  }

  jsrt_wasi_t* wasi = calloc(1, sizeof(jsrt_wasi_t));
  if (!wasi) {
    JS_ThrowOutOfMemory(ctx);
    return NULL;
  }

  wasi->ctx = ctx;

  // Copy options (deep copy)
  wasi->options.stdin_fd = options->stdin_fd;
  wasi->options.stdout_fd = options->stdout_fd;
  wasi->options.stderr_fd = options->stderr_fd;
  wasi->options.return_on_exit = options->return_on_exit;
  wasi->options.version = safe_strdup(options->version);
  // Check for allocation failure
  if (!wasi->options.version && options->version) {
    jsrt_wasi_free(wasi);
    JS_ThrowOutOfMemory(ctx);
    return NULL;
  }

  // Copy args
  if (options->args_count > 0) {
    wasi->options.args = calloc(options->args_count + 1, sizeof(char*));
    if (!wasi->options.args) {
      jsrt_wasi_free(wasi);
      JS_ThrowOutOfMemory(ctx);
      return NULL;
    }
    wasi->options.args_count = options->args_count;
    for (size_t i = 0; i < options->args_count; i++) {
      wasi->options.args[i] = safe_strdup(options->args[i]);
      // Check for allocation failure (safe_strdup returns NULL if input is NULL or allocation fails)
      if (!wasi->options.args[i] && options->args[i]) {
        jsrt_wasi_free(wasi);
        JS_ThrowOutOfMemory(ctx);
        return NULL;
      }
    }
  }

  // Copy env
  if (options->env_count > 0) {
    wasi->options.env = calloc(options->env_count + 1, sizeof(char*));
    if (!wasi->options.env) {
      jsrt_wasi_free(wasi);
      JS_ThrowOutOfMemory(ctx);
      return NULL;
    }
    wasi->options.env_count = options->env_count;
    for (size_t i = 0; i < options->env_count; i++) {
      wasi->options.env[i] = safe_strdup(options->env[i]);
      // Check for allocation failure
      if (!wasi->options.env[i] && options->env[i]) {
        jsrt_wasi_free(wasi);
        JS_ThrowOutOfMemory(ctx);
        return NULL;
      }
    }
  }

  // Copy preopens
  if (options->preopen_count > 0) {
    wasi->options.preopens = calloc(options->preopen_count, sizeof(jsrt_wasi_preopen_t));
    if (!wasi->options.preopens) {
      jsrt_wasi_free(wasi);
      JS_ThrowOutOfMemory(ctx);
      return NULL;
    }
    wasi->options.preopen_count = options->preopen_count;
    for (size_t i = 0; i < options->preopen_count; i++) {
      wasi->options.preopens[i].virtual_path = safe_strdup(options->preopens[i].virtual_path);
      wasi->options.preopens[i].real_path = safe_strdup(options->preopens[i].real_path);
      // Check for allocation failure
      if ((!wasi->options.preopens[i].virtual_path && options->preopens[i].virtual_path) ||
          (!wasi->options.preopens[i].real_path && options->preopens[i].real_path)) {
        jsrt_wasi_free(wasi);
        JS_ThrowOutOfMemory(ctx);
        return NULL;
      }
    }
  }

  // Initialize state
  wasi->wasm_instance = JS_UNDEFINED;
  wasi->import_object = JS_UNDEFINED;
  wasi->started = false;
  wasi->initialized = false;
  wasi->exit_code = 0;
  wasi->exit_requested = false;
  wasi->memory_validated = false;
  wasi->wamr_instance = NULL;
  wasi->exec_env = NULL;

  if (jsrt_wasi_init_fd_table(wasi) < 0) {
    jsrt_wasi_free(wasi);
    JS_ThrowOutOfMemory(ctx);
    return NULL;
  }

  JSRT_Debug("Created WASI instance: version=%s, args=%zu, env=%zu, preopens=%zu", wasi->options.version,
             wasi->options.args_count, wasi->options.env_count, wasi->options.preopen_count);

  return wasi;
}

/**
 * Free WASI instance
 */
void jsrt_wasi_free(jsrt_wasi_t* wasi) {
  if (!wasi) {
    return;
  }

  JSRT_Debug("Freeing WASI instance");

  // Free options
  jsrt_wasi_free_options(&wasi->options);

  // Free JavaScript values
  if (wasi->ctx) {
    if (!JS_IsUndefined(wasi->wasm_instance)) {
      JS_FreeValue(wasi->ctx, wasi->wasm_instance);
    }
    if (!JS_IsUndefined(wasi->import_object)) {
      JS_FreeValue(wasi->ctx, wasi->import_object);
    }
  }

  // Free WAMR resources
  if (wasi->exec_env) {
    wasm_runtime_destroy_exec_env(wasi->exec_env);
  }

  // Note: wamr_instance is owned by JavaScript WebAssembly.Instance object.
  // We hold a strong reference via wasi->wasm_instance (JS_DupValue above),
  // which prevents the Instance from being GC'd while this WASI object is alive.
  // The Instance finalizer will clean up the WAMR instance when appropriate.
  // Therefore, we don't (and must not) free wamr_instance here.

  free(wasi->fd_table);
  free(wasi);
}
