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

// Default values
#define WASI_DEFAULT_VERSION "preview1"
#define WASI_DEFAULT_STDIN 0
#define WASI_DEFAULT_STDOUT 1
#define WASI_DEFAULT_STDERR 2

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

  // Allocate preopens array
  jsrt_wasi_preopen_t* preopens = calloc(count, sizeof(jsrt_wasi_preopen_t));
  if (!preopens) {
    js_free(ctx, props);
    JS_ThrowOutOfMemory(ctx);
    return -1;
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
    }
  }

  // Initialize state
  wasi->wasm_instance = JS_UNDEFINED;
  wasi->import_object = JS_UNDEFINED;
  wasi->started = false;
  wasi->initialized = false;
  wasi->exit_code = 0;
  wasi->wamr_instance = NULL;
  wasi->exec_env = NULL;

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

  // Note: wamr_instance is owned by JavaScript WebAssembly.Instance,
  // so we don't free it here

  free(wasi);
}
