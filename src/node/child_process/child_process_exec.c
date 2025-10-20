#include "../../util/debug.h"
#include "child_process_internal.h"

// Helper to append data to buffer
static int append_to_buffer(JSContext* ctx, char** buffer, size_t* size, size_t* capacity, const char* data,
                            size_t data_len, size_t max_buffer) {
  // Check if adding this data would exceed maxBuffer
  if (*size + data_len > max_buffer) {
    return -1;  // maxBuffer exceeded
  }

  // Resize buffer if needed
  if (*size + data_len > *capacity) {
    size_t new_capacity = (*capacity == 0) ? 4096 : *capacity * 2;
    while (new_capacity < *size + data_len) {
      new_capacity *= 2;
    }

    char* new_buffer = js_realloc(ctx, *buffer, new_capacity);
    if (!new_buffer) {
      return -1;  // Out of memory
    }

    *buffer = new_buffer;
    *capacity = new_capacity;
  }

  // Append data
  memcpy(*buffer + *size, data, data_len);
  *size += data_len;

  return 0;
}

// exec(command[, options][, callback])
JSValue js_child_process_exec(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("child_process.exec() called with %d args", argc);

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "exec() requires a command argument");
  }

  // Parse command
  const char* command = JS_ToCString(ctx, argv[0]);
  if (!command) {
    return JS_ThrowTypeError(ctx, "command must be a string");
  }

  // Parse optional arguments
  JSValue options = JS_UNDEFINED;
  JSValue callback = JS_UNDEFINED;

  if (argc >= 2) {
    if (JS_IsFunction(ctx, argv[1])) {
      callback = argv[1];
    } else {
      options = argv[1];
      if (argc >= 3 && JS_IsFunction(ctx, argv[2])) {
        callback = argv[2];
      }
    }
  }

  // Create new options object with shell: true
  JSValue exec_options = JS_NewObject(ctx);
  if (JS_IsException(exec_options)) {
    JS_FreeCString(ctx, command);
    return exec_options;
  }

  // Copy properties from options if provided
  if (!JS_IsUndefined(options) && JS_IsObject(options)) {
    JSPropertyEnum* props;
    uint32_t prop_count;

    if (!JS_GetOwnPropertyNames(ctx, &props, &prop_count, options, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
      for (uint32_t i = 0; i < prop_count; i++) {
        JSValue val = JS_GetProperty(ctx, options, props[i].atom);
        JS_SetProperty(ctx, exec_options, props[i].atom, val);
        JS_FreeAtom(ctx, props[i].atom);
      }
      js_free(ctx, props);
    }
  }

  // Set shell: true
  JS_SetPropertyStr(ctx, exec_options, "shell", JS_NewBool(ctx, true));

  // Build args array for shell: ['-c', command]
  JSValue args_array = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, args_array, 0, JS_NewString(ctx, "-c"));
  JS_SetPropertyUint32(ctx, args_array, 1, JS_NewString(ctx, command));

  // Determine shell path
#ifdef _WIN32
  const char* shell = "cmd.exe";
  // On Windows, use /c instead of -c
  JS_SetPropertyUint32(ctx, args_array, 0, JS_NewString(ctx, "/c"));
#else
  const char* shell = "/bin/sh";
#endif

  // Call spawn with shell
  JSValue spawn_argv[3] = {JS_NewString(ctx, shell), args_array, exec_options};
  JSValue child = js_child_process_spawn(ctx, this_val, 3, spawn_argv);

  JS_FreeValue(ctx, spawn_argv[0]);
  JS_FreeValue(ctx, spawn_argv[1]);
  JS_FreeValue(ctx, spawn_argv[2]);

  if (JS_IsException(child)) {
    JS_FreeCString(ctx, command);
    return child;
  }

  // Set up buffering on the ChildProcess
  JSChildProcess* child_data = JS_GetOpaque(child, js_child_process_class_id);
  if (child_data) {
    child_data->buffering = true;
    child_data->stdout_size = 0;
    child_data->stderr_size = 0;
    child_data->stdout_capacity = 0;
    child_data->stderr_capacity = 0;

    // Parse maxBuffer from options (default 1MB)
    child_data->max_buffer = 1024 * 1024;
    if (!JS_IsUndefined(options) && JS_IsObject(options)) {
      JSValue max_buffer_val = JS_GetPropertyStr(ctx, options, "maxBuffer");
      if (!JS_IsUndefined(max_buffer_val)) {
        int64_t max_buffer;
        if (!JS_ToInt64(ctx, &max_buffer, max_buffer_val)) {
          child_data->max_buffer = max_buffer;
        }
      }
      JS_FreeValue(ctx, max_buffer_val);
    }

    // Store callback if provided
    if (!JS_IsUndefined(callback)) {
      child_data->exec_callback = JS_DupValue(ctx, callback);
    } else {
      child_data->exec_callback = JS_UNDEFINED;
    }

    // Parse and start timeout if specified
    if (!JS_IsUndefined(options) && JS_IsObject(options)) {
      JSValue timeout_val = JS_GetPropertyStr(ctx, options, "timeout");
      if (!JS_IsUndefined(timeout_val)) {
        int64_t timeout;
        if (!JS_ToInt64(ctx, &timeout, timeout_val) && timeout > 0) {
          child_data->timeout_ms = timeout;

          // Allocate and start timer
          child_data->timeout_timer = js_malloc(ctx, sizeof(uv_timer_t));
          if (child_data->timeout_timer) {
            JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
            uv_timer_init(rt->uv_loop, child_data->timeout_timer);
            child_data->timeout_timer->data = child_data;
            uv_timer_start(child_data->timeout_timer, on_timeout, timeout, 0);
            JSRT_Debug("Started timeout timer for %lld ms", (long long)timeout);
          }
        }
      }
      JS_FreeValue(ctx, timeout_val);
    }

    // Store command for error messages
    if (child_data->file) {
      free(child_data->file);
    }
    child_data->file = strdup(command);
  }

  JS_FreeCString(ctx, command);
  return child;
}

// execFile(file[, args][, options][, callback])
JSValue js_child_process_exec_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("child_process.execFile() called with %d args", argc);

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "execFile() requires a file argument");
  }

  // Parse file
  const char* file = JS_ToCString(ctx, argv[0]);
  if (!file) {
    return JS_ThrowTypeError(ctx, "file must be a string");
  }

  // Parse optional arguments: args, options, callback
  JSValue args = JS_UNDEFINED;
  JSValue options = JS_UNDEFINED;
  JSValue callback = JS_UNDEFINED;

  int arg_idx = 1;

  // Next arg could be args array
  if (arg_idx < argc && JS_IsArray(ctx, argv[arg_idx])) {
    args = argv[arg_idx++];
  }

  // Next arg could be options object or callback
  if (arg_idx < argc) {
    if (JS_IsFunction(ctx, argv[arg_idx])) {
      callback = argv[arg_idx++];
    } else if (JS_IsObject(argv[arg_idx])) {
      options = argv[arg_idx++];
      // Final arg could be callback
      if (arg_idx < argc && JS_IsFunction(ctx, argv[arg_idx])) {
        callback = argv[arg_idx++];
      }
    }
  }

  // If no args array provided, create empty one
  if (JS_IsUndefined(args)) {
    args = JS_NewArray(ctx);
  }

  // Call spawn with file (no shell)
  JSValue spawn_argv[3] = {JS_NewString(ctx, file), JS_DupValue(ctx, args),
                           JS_IsUndefined(options) ? JS_NewObject(ctx) : JS_DupValue(ctx, options)};

  JSValue child = js_child_process_spawn(ctx, this_val, 3, spawn_argv);

  JS_FreeValue(ctx, spawn_argv[0]);
  JS_FreeValue(ctx, spawn_argv[1]);
  JS_FreeValue(ctx, spawn_argv[2]);

  if (JS_IsException(child)) {
    JS_FreeCString(ctx, file);
    return child;
  }

  // Set up buffering on the ChildProcess
  JSChildProcess* child_data = JS_GetOpaque(child, js_child_process_class_id);
  if (child_data) {
    child_data->buffering = true;
    child_data->stdout_size = 0;
    child_data->stderr_size = 0;
    child_data->stdout_capacity = 0;
    child_data->stderr_capacity = 0;

    // Parse maxBuffer from options (default 1MB)
    child_data->max_buffer = 1024 * 1024;
    if (!JS_IsUndefined(options) && JS_IsObject(options)) {
      JSValue max_buffer_val = JS_GetPropertyStr(ctx, options, "maxBuffer");
      if (!JS_IsUndefined(max_buffer_val)) {
        int64_t max_buffer;
        if (!JS_ToInt64(ctx, &max_buffer, max_buffer_val)) {
          child_data->max_buffer = max_buffer;
        }
      }
      JS_FreeValue(ctx, max_buffer_val);
    }

    // Store callback if provided
    if (!JS_IsUndefined(callback)) {
      child_data->exec_callback = JS_DupValue(ctx, callback);
    } else {
      child_data->exec_callback = JS_UNDEFINED;
    }

    // Parse and start timeout if specified
    if (!JS_IsUndefined(options) && JS_IsObject(options)) {
      JSValue timeout_val = JS_GetPropertyStr(ctx, options, "timeout");
      if (!JS_IsUndefined(timeout_val)) {
        int64_t timeout;
        if (!JS_ToInt64(ctx, &timeout, timeout_val) && timeout > 0) {
          child_data->timeout_ms = timeout;

          // Allocate and start timer
          child_data->timeout_timer = js_malloc(ctx, sizeof(uv_timer_t));
          if (child_data->timeout_timer) {
            JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
            uv_timer_init(rt->uv_loop, child_data->timeout_timer);
            child_data->timeout_timer->data = child_data;
            uv_timer_start(child_data->timeout_timer, on_timeout, timeout, 0);
            JSRT_Debug("Started timeout timer for %lld ms", (long long)timeout);
          }
        }
      }
      JS_FreeValue(ctx, timeout_val);
    }

    // Store file for error messages
    if (child_data->file) {
      free(child_data->file);
    }
    child_data->file = strdup(file);
  }

  JS_FreeCString(ctx, file);
  return child;
}

// Helper to call exec callback with results
void call_exec_callback(JSContext* ctx, JSChildProcess* child, JSValue error, JSValue stdout_val, JSValue stderr_val) {
  if (JS_IsUndefined(child->exec_callback)) {
    // No callback, just free values
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, stdout_val);
    JS_FreeValue(ctx, stderr_val);
    return;
  }

  JSRT_Debug("Calling exec callback");

  JSValue argv[3] = {error, stdout_val, stderr_val};
  JSValue result = JS_Call(ctx, child->exec_callback, JS_UNDEFINED, 3, argv);

  if (JS_IsException(result)) {
    JSRT_Debug("exec callback threw exception");
    // Log error but don't propagate
    JSValue exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
  }

  JS_FreeValue(ctx, result);
  JS_FreeValue(ctx, error);
  JS_FreeValue(ctx, stdout_val);
  JS_FreeValue(ctx, stderr_val);
  JS_FreeValue(ctx, child->exec_callback);
  child->exec_callback = JS_UNDEFINED;
}
