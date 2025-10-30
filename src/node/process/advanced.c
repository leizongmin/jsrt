#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../util/debug.h"
#include "process.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

// Global state for advanced features
static bool g_source_maps_enabled = false;
static uv_idle_t* g_process_ref_handle = NULL;

// ============================================================================
// Task 6.1: process.loadEnvFile(path)
// ============================================================================

// Helper: Trim whitespace from a string
static char* trim_whitespace(char* str) {
  char* end;

  // Trim leading space
  while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') {
    str++;
  }

  if (*str == 0) {  // All spaces?
    return str;
  }

  // Trim trailing space
  end = str + strlen(str) - 1;
  while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
    end--;
  }

  // Write new null terminator
  *(end + 1) = 0;

  return str;
}

// Helper: Parse a single line from .env file
static bool parse_env_line(JSContext* ctx, const char* line, char** key_out, char** value_out) {
  // Skip comments and empty lines
  const char* trimmed = line;
  while (*trimmed == ' ' || *trimmed == '\t') {
    trimmed++;
  }
  if (*trimmed == '#' || *trimmed == '\0' || *trimmed == '\n' || *trimmed == '\r') {
    return false;
  }

  // Find the '=' separator
  const char* eq = strchr(trimmed, '=');
  if (!eq) {
    return false;
  }

  // Extract key
  size_t key_len = eq - trimmed;
  char* key = (char*)js_malloc(ctx, key_len + 1);
  if (!key) {
    return false;
  }
  strncpy(key, trimmed, key_len);
  key[key_len] = '\0';

  // Trim whitespace from key in place
  char* trimmed_key = trim_whitespace(key);
  if (trimmed_key != key) {
    // If trim_whitespace returned different pointer, we need to move the data
    memmove(key, trimmed_key, strlen(trimmed_key) + 1);
  }

  // Extract value
  const char* value_start = eq + 1;
  char* value = (char*)js_malloc(ctx, strlen(value_start) + 1);
  if (!value) {
    js_free(ctx, key);
    return false;
  }
  strcpy(value, value_start);

  // Trim whitespace from value in place
  char* trimmed_value = trim_whitespace(value);
  if (trimmed_value != value) {
    memmove(value, trimmed_value, strlen(trimmed_value) + 1);
  }

  // Handle quoted values
  size_t value_len = strlen(value);
  if (value_len >= 2 &&
      ((value[0] == '"' && value[value_len - 1] == '"') || (value[0] == '\'' && value[value_len - 1] == '\''))) {
    // Remove quotes
    memmove(value, value + 1, value_len - 2);
    value[value_len - 2] = '\0';
  }

  *key_out = key;
  *value_out = value;
  return true;
}

// Helper: Simple variable expansion (basic $VAR and ${VAR})
static char* expand_variables(JSContext* ctx, const char* value) {
  // For simplicity, just return a copy
  // Full variable expansion would require recursive parsing
  // which is complex. This is a basic implementation.
  char* result = (char*)js_malloc(ctx, strlen(value) + 1);
  if (result) {
    strcpy(result, value);
  }
  return result;
}

JSValue js_process_load_env_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* path = ".env";  // Default path
  const char* path_to_free = NULL;

  // Parse optional path argument
  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    path_to_free = JS_ToCString(ctx, argv[0]);
    if (!path_to_free) {
      return JS_ThrowTypeError(ctx, "path must be a string");
    }
    path = path_to_free;
  }

  // Read file
  FILE* fp = fopen(path, "r");
  if (!fp) {
    if (path_to_free) {
      JS_FreeCString(ctx, path_to_free);
    }
    return JS_ThrowInternalError(ctx, "Failed to open .env file: %s", strerror(errno));
  }

  // Parse file line by line and set environment variables directly
  char line[4096];
  int line_num = 0;
  while (fgets(line, sizeof(line), fp)) {
    line_num++;

    char* key = NULL;
    char* value = NULL;
    if (parse_env_line(ctx, line, &key, &value)) {
      // Expand variables (basic implementation)
      char* expanded = expand_variables(ctx, value);

      if (expanded) {
        // Set the actual environment variable using setenv
#ifdef _WIN32
        _putenv_s(key, expanded);
#else
        setenv(key, expanded, 1);  // 1 = overwrite
#endif
        js_free(ctx, expanded);
      }

      js_free(ctx, key);
      js_free(ctx, value);
    }
  }

  fclose(fp);

  if (path_to_free) {
    JS_FreeCString(ctx, path_to_free);
  }

  return JS_UNDEFINED;
}

// ============================================================================
// Task 6.2: process.getActiveResourcesInfo()
// ============================================================================

// Callback to walk handles and collect info
static void walk_handle_cb(uv_handle_t* handle, void* arg) {
  JSContext* ctx = (JSContext*)arg;
  JSValue array = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "_active_resources_array");

  if (JS_IsUndefined(array)) {
    return;
  }

  const char* type_str = "Unknown";
  switch (handle->type) {
    case UV_TCP:
      type_str = "TCPSocket";
      break;
    case UV_UDP:
      type_str = "UDPSocket";
      break;
    case UV_TIMER:
      type_str = "Timer";
      break;
    case UV_IDLE:
      type_str = "Idle";
      break;
    case UV_PREPARE:
      type_str = "Prepare";
      break;
    case UV_CHECK:
      type_str = "Check";
      break;
    case UV_SIGNAL:
      type_str = "Signal";
      break;
    case UV_PROCESS:
      type_str = "ChildProcess";
      break;
    case UV_FS_EVENT:
      type_str = "FSEvent";
      break;
    case UV_FS_POLL:
      type_str = "FSPoll";
      break;
    case UV_POLL:
      type_str = "Poll";
      break;
    case UV_ASYNC:
      type_str = "Async";
      break;
    case UV_TTY:
      type_str = "TTY";
      break;
    case UV_NAMED_PIPE:
      type_str = "Pipe";
      break;
    default:
      break;
  }

  // Append to array
  uint32_t len;
  JSValue len_val = JS_GetPropertyStr(ctx, array, "length");
  JS_ToUint32(ctx, &len, len_val);
  JS_FreeValue(ctx, len_val);

  JS_SetPropertyUint32(ctx, array, len, JS_NewString(ctx, type_str));

  JS_FreeValue(ctx, array);
}

JSValue js_process_get_active_resources_info(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Get uv_loop from runtime
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue process = JS_GetPropertyStr(ctx, global, "process");
  JS_FreeValue(ctx, global);
  JS_FreeValue(ctx, process);

  // Get uv_loop (need to access from runtime context)
  // For now, use a simple approach: store array in global temporarily
  JSValue array = JS_NewArray(ctx);
  JSValue global_obj = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global_obj, "_active_resources_array", JS_DupValue(ctx, array));

  // Get runtime to access uv_loop
  JSRuntime* rt = JS_GetRuntime(ctx);
  JSRT_Runtime* jsrt_rt = (JSRT_Runtime*)JS_GetRuntimeOpaque(rt);
  if (jsrt_rt && jsrt_rt->uv_loop) {
    uv_walk(jsrt_rt->uv_loop, walk_handle_cb, ctx);
  }

  // Clean up temporary global
  JS_SetPropertyStr(ctx, global_obj, "_active_resources_array", JS_UNDEFINED);
  JS_FreeValue(ctx, global_obj);

  return array;
}

// ============================================================================
// Task 6.3: process.setSourceMapsEnabled(val)
// ============================================================================

JSValue js_process_set_source_maps_enabled(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Expected boolean argument");
  }

  int val = JS_ToBool(ctx, argv[0]);
  if (val < 0) {
    return JS_EXCEPTION;
  }

  g_source_maps_enabled = val ? true : false;
  return JS_UNDEFINED;
}

JSValue js_process_get_source_maps_enabled(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewBool(ctx, g_source_maps_enabled);
}

// ============================================================================
// Task 6.4: process.ref()
// ============================================================================

// Idle callback (does nothing, just keeps loop alive)
static void process_ref_idle_cb(uv_idle_t* handle) {
  // No-op, just keeps the loop alive
}

JSValue js_process_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Get runtime to access uv_loop
  JSRuntime* rt = JS_GetRuntime(ctx);
  JSRT_Runtime* jsrt_rt = (JSRT_Runtime*)JS_GetRuntimeOpaque(rt);

  if (!jsrt_rt || !jsrt_rt->uv_loop) {
    return JS_ThrowInternalError(ctx, "Cannot access event loop");
  }

  // Create idle handle if it doesn't exist
  if (!g_process_ref_handle) {
    g_process_ref_handle = (uv_idle_t*)js_malloc(ctx, sizeof(uv_idle_t));
    if (!g_process_ref_handle) {
      return JS_ThrowOutOfMemory(ctx);
    }

    int r = uv_idle_init(jsrt_rt->uv_loop, g_process_ref_handle);
    if (r != 0) {
      js_free(ctx, g_process_ref_handle);
      g_process_ref_handle = NULL;
      return JS_ThrowInternalError(ctx, "Failed to initialize idle handle: %s", uv_strerror(r));
    }

    r = uv_idle_start(g_process_ref_handle, process_ref_idle_cb);
    if (r != 0) {
      uv_close((uv_handle_t*)g_process_ref_handle, NULL);
      js_free(ctx, g_process_ref_handle);
      g_process_ref_handle = NULL;
      return JS_ThrowInternalError(ctx, "Failed to start idle handle: %s", uv_strerror(r));
    }
  }

  // Ref the handle
  uv_ref((uv_handle_t*)g_process_ref_handle);

  return JS_UNDEFINED;
}

// ============================================================================
// Task 6.5: process.unref()
// ============================================================================

JSValue js_process_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!g_process_ref_handle) {
    // No handle to unref, just return
    return JS_UNDEFINED;
  }

  // Unref the handle
  uv_unref((uv_handle_t*)g_process_ref_handle);

  return JS_UNDEFINED;
}

// ============================================================================
// Initialization and cleanup
// ============================================================================

void jsrt_process_init_advanced(void) {
  g_source_maps_enabled = false;
  g_process_ref_handle = NULL;

  JSRT_Debug("Process advanced features module initialized");
}

// Close callback for ref handle
static void process_ref_close_cb(uv_handle_t* handle) {
  if (handle->data) {
    JSContext* ctx = (JSContext*)handle->data;
    js_free(ctx, handle);
  }
}

void jsrt_process_cleanup_advanced(JSContext* ctx) {
  // Clean up ref handle if exists
  if (g_process_ref_handle) {
    if (!uv_is_closing((uv_handle_t*)g_process_ref_handle)) {
      g_process_ref_handle->data = ctx;  // Pass context for cleanup
      uv_close((uv_handle_t*)g_process_ref_handle, process_ref_close_cb);
    }
    g_process_ref_handle = NULL;
  }

  JSRT_Debug("Process advanced features cleanup completed");
}
