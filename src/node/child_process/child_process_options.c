#include "../../util/debug.h"
#include "child_process_internal.h"

// Helper to convert JS array to NULL-terminated string array
static char** js_array_to_string_array(JSContext* ctx, JSValue arr) {
  if (!JS_IsArray(ctx, arr)) {
    return NULL;
  }

  JSValue length_val = JS_GetPropertyStr(ctx, arr, "length");
  uint32_t length;
  if (JS_ToUint32(ctx, &length, length_val)) {
    JS_FreeValue(ctx, length_val);
    return NULL;
  }
  JS_FreeValue(ctx, length_val);

  // Allocate array with space for NULL terminator
  char** result = js_malloc(ctx, sizeof(char*) * (length + 1));
  if (!result) {
    return NULL;
  }

  for (uint32_t i = 0; i < length; i++) {
    JSValue item = JS_GetPropertyUint32(ctx, arr, i);
    const char* str = JS_ToCString(ctx, item);
    JS_FreeValue(ctx, item);

    if (!str) {
      // Cleanup on error
      for (uint32_t j = 0; j < i; j++) {
        js_free(ctx, result[j]);
      }
      js_free(ctx, result);
      return NULL;
    }

    result[i] = js_strdup(ctx, str);
    JS_FreeCString(ctx, str);
  }

  result[length] = NULL;
  return result;
}

// Helper to convert JS object to environment array
static char** js_object_to_env_array(JSContext* ctx, JSValue env_obj) {
  if (JS_IsUndefined(env_obj) || JS_IsNull(env_obj)) {
    return NULL;
  }

  // Get all property names
  JSPropertyEnum* props;
  uint32_t prop_count;

  if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, env_obj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
    return NULL;
  }

  // Allocate array
  char** result = js_malloc(ctx, sizeof(char*) * (prop_count + 1));
  if (!result) {
    js_free(ctx, props);
    return NULL;
  }

  for (uint32_t i = 0; i < prop_count; i++) {
    JSValue key_val = JS_AtomToString(ctx, props[i].atom);
    JSValue val = JS_GetProperty(ctx, env_obj, props[i].atom);

    const char* key = JS_ToCString(ctx, key_val);
    const char* value = JS_ToCString(ctx, val);

    if (key && value) {
      size_t len = strlen(key) + strlen(value) + 2;  // "KEY=VALUE\0"
      result[i] = js_malloc(ctx, len);
      snprintf(result[i], len, "%s=%s", key, value);
    } else {
      result[i] = NULL;
    }

    if (key)
      JS_FreeCString(ctx, key);
    if (value)
      JS_FreeCString(ctx, value);
    JS_FreeValue(ctx, key_val);
    JS_FreeValue(ctx, val);
  }

  result[prop_count] = NULL;

  for (uint32_t i = 0; i < prop_count; i++) {
    JS_FreeAtom(ctx, props[i].atom);
  }
  js_free(ctx, props);

  return result;
}

// Parse spawn options from JavaScript object
int parse_spawn_options(JSContext* ctx, JSValueConst options_obj, JSChildProcessOptions* options) {
  memset(options, 0, sizeof(*options));

  // Set defaults
  options->uid = -1;
  options->gid = -1;
  options->stdio_count = 3;           // stdin, stdout, stderr
  options->max_buffer = 1024 * 1024;  // 1MB default
  options->kill_signal = "SIGTERM";

  if (JS_IsUndefined(options_obj) || JS_IsNull(options_obj)) {
    return 0;  // No options is okay, use defaults
  }

  if (!JS_IsObject(options_obj)) {
    JS_ThrowTypeError(ctx, "options must be an object");
    return -1;
  }

  // Parse cwd
  JSValue cwd_val = JS_GetPropertyStr(ctx, options_obj, "cwd");
  if (!JS_IsUndefined(cwd_val) && !JS_IsNull(cwd_val)) {
    const char* cwd = JS_ToCString(ctx, cwd_val);
    if (cwd) {
      options->cwd = cwd;  // Store pointer, will be freed by caller
    }
  }
  JS_FreeValue(ctx, cwd_val);

  // Parse env
  JSValue env_val = JS_GetPropertyStr(ctx, options_obj, "env");
  if (!JS_IsUndefined(env_val) && !JS_IsNull(env_val)) {
    options->env = js_object_to_env_array(ctx, env_val);
  }
  JS_FreeValue(ctx, env_val);

  // Parse uid (POSIX only)
  JSValue uid_val = JS_GetPropertyStr(ctx, options_obj, "uid");
  if (!JS_IsUndefined(uid_val)) {
    int32_t uid;
    if (!JS_ToInt32(ctx, &uid, uid_val)) {
      options->uid = uid;
    }
  }
  JS_FreeValue(ctx, uid_val);

  // Parse gid (POSIX only)
  JSValue gid_val = JS_GetPropertyStr(ctx, options_obj, "gid");
  if (!JS_IsUndefined(gid_val)) {
    int32_t gid;
    if (!JS_ToInt32(ctx, &gid, gid_val)) {
      options->gid = gid;
    }
  }
  JS_FreeValue(ctx, gid_val);

  // Parse detached
  JSValue detached_val = JS_GetPropertyStr(ctx, options_obj, "detached");
  if (JS_IsBool(detached_val)) {
    options->detached = JS_ToBool(ctx, detached_val);
  }
  JS_FreeValue(ctx, detached_val);

  // Parse windowsHide
  JSValue windows_hide_val = JS_GetPropertyStr(ctx, options_obj, "windowsHide");
  if (JS_IsBool(windows_hide_val)) {
    options->windows_hide = JS_ToBool(ctx, windows_hide_val);
  }
  JS_FreeValue(ctx, windows_hide_val);

  // Parse shell
  JSValue shell_val = JS_GetPropertyStr(ctx, options_obj, "shell");
  if (JS_IsBool(shell_val)) {
    if (JS_ToBool(ctx, shell_val)) {
#ifdef _WIN32
      options->shell = "cmd.exe";
#else
      options->shell = "/bin/sh";
#endif
    }
  } else if (JS_IsString(shell_val)) {
    const char* shell = JS_ToCString(ctx, shell_val);
    if (shell) {
      options->shell = shell;
    }
  }
  JS_FreeValue(ctx, shell_val);

  // Parse timeout
  JSValue timeout_val = JS_GetPropertyStr(ctx, options_obj, "timeout");
  if (!JS_IsUndefined(timeout_val)) {
    int64_t timeout;
    if (!JS_ToInt64(ctx, &timeout, timeout_val)) {
      options->timeout = timeout;
    }
  }
  JS_FreeValue(ctx, timeout_val);

  // Parse maxBuffer
  JSValue max_buffer_val = JS_GetPropertyStr(ctx, options_obj, "maxBuffer");
  if (!JS_IsUndefined(max_buffer_val)) {
    int64_t max_buffer;
    if (!JS_ToInt64(ctx, &max_buffer, max_buffer_val)) {
      options->max_buffer = max_buffer;
    }
  }
  JS_FreeValue(ctx, max_buffer_val);

  // Parse killSignal
  JSValue kill_signal_val = JS_GetPropertyStr(ctx, options_obj, "killSignal");
  if (JS_IsString(kill_signal_val)) {
    const char* signal = JS_ToCString(ctx, kill_signal_val);
    if (signal) {
      options->kill_signal = signal;
    }
  }
  JS_FreeValue(ctx, kill_signal_val);

  return 0;
}

// Free spawn options
void free_spawn_options(JSChildProcessOptions* options) {
  if (!options) {
    return;
  }

  if (options->cwd) {
    JS_FreeCString(NULL, options->cwd);
  }

  if (options->env) {
    for (int i = 0; options->env[i]; i++) {
      free(options->env[i]);
    }
    free(options->env);
  }

  if (options->shell && options->shell != "/bin/sh" && options->shell != "cmd.exe") {
    JS_FreeCString(NULL, options->shell);
  }

  if (options->kill_signal && options->kill_signal != "SIGTERM") {
    JS_FreeCString(NULL, options->kill_signal);
  }
}
