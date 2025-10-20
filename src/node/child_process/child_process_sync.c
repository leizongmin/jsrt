#include "../../util/debug.h"
#include "child_process_internal.h"

// Forward declarations
static JSValue create_buffer_from_data(JSContext* ctx, const char* data, size_t len);
static char** js_array_to_string_array(JSContext* ctx, JSValue arr);

// Synchronous process state
typedef struct {
  // Output buffers
  char* stdout_buffer;
  char* stderr_buffer;
  size_t stdout_size;
  size_t stderr_size;
  size_t stdout_capacity;
  size_t stderr_capacity;

  // Process state
  bool finished;
  int exit_code;
  int term_signal;
  bool timeout_expired;
  pid_t pid;

  // libuv handles (owned by this struct)
  uv_process_t process;
  uv_pipe_t stdout_pipe;
  uv_pipe_t stderr_pipe;
  uv_timer_t timeout_timer;

  // Configuration
  size_t max_buffer;
  bool has_timeout;
  uint64_t timeout_ms;

  // Error tracking
  bool buffer_exceeded;
  int spawn_error;
} SyncState;

// Append data to buffer
static int append_to_sync_buffer(char** buffer, size_t* size, size_t* capacity, const char* data, size_t data_len,
                                 size_t max_buffer) {
  if (*size + data_len > max_buffer) {
    return -1;  // maxBuffer exceeded
  }

  if (*size + data_len > *capacity) {
    size_t new_capacity = (*capacity == 0) ? 4096 : *capacity * 2;
    while (new_capacity < *size + data_len) {
      new_capacity *= 2;
    }

    char* new_buffer = realloc(*buffer, new_capacity);
    if (!new_buffer) {
      return -1;  // Out of memory
    }

    *buffer = new_buffer;
    *capacity = new_capacity;
  }

  memcpy(*buffer + *size, data, data_len);
  *size += data_len;

  return 0;
}

// Allocation callback for stdout
static void sync_stdout_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

// Read callback for stdout
static void sync_stdout_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  SyncState* state = (SyncState*)stream->data;

  if (nread > 0) {
    if (!state->buffer_exceeded) {
      if (append_to_sync_buffer(&state->stdout_buffer, &state->stdout_size, &state->stdout_capacity, buf->base, nread,
                                state->max_buffer) < 0) {
        state->buffer_exceeded = true;
        JSRT_Debug("stdout maxBuffer exceeded");
      }
    }
  } else if (nread < 0) {
    // EOF or error
    if (nread != UV_EOF) {
      JSRT_Debug("stdout read error: %s", uv_strerror(nread));
    }
    uv_read_stop(stream);
  }

  if (buf->base) {
    free(buf->base);
  }
}

// Allocation callback for stderr
static void sync_stderr_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

// Read callback for stderr
static void sync_stderr_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  SyncState* state = (SyncState*)stream->data;

  if (nread > 0) {
    if (!state->buffer_exceeded) {
      if (append_to_sync_buffer(&state->stderr_buffer, &state->stderr_size, &state->stderr_capacity, buf->base, nread,
                                state->max_buffer) < 0) {
        state->buffer_exceeded = true;
        JSRT_Debug("stderr maxBuffer exceeded");
      }
    }
  } else if (nread < 0) {
    // EOF or error
    if (nread != UV_EOF) {
      JSRT_Debug("stderr read error: %s", uv_strerror(nread));
    }
    uv_read_stop(stream);
  }

  if (buf->base) {
    free(buf->base);
  }
}

// Process exit callback
static void sync_exit_cb(uv_process_t* process, int64_t exit_status, int term_signal) {
  SyncState* state = (SyncState*)process->data;

  JSRT_Debug("Sync process %d exited with status %ld, signal %d", (int)state->pid, (long)exit_status, term_signal);

  state->finished = true;
  state->exit_code = (int)exit_status;
  state->term_signal = term_signal;
}

// Timeout callback
static void sync_timeout_cb(uv_timer_t* timer) {
  SyncState* state = (SyncState*)timer->data;

  JSRT_Debug("Sync process timeout expired");

  state->timeout_expired = true;

  // Kill the process
  uv_process_kill(&state->process, SIGTERM);
}

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
  char** result = malloc(sizeof(char*) * (length + 1));
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
        free(result[j]);
      }
      free(result);
      return NULL;
    }

    result[i] = strdup(str);
    JS_FreeCString(ctx, str);
  }

  result[length] = NULL;
  return result;
}

// Helper to create Buffer from C data
static JSValue create_buffer_from_data(JSContext* ctx, const char* data, size_t len) {
  JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
  if (JS_IsException(buffer_module)) {
    return JS_NULL;
  }

  JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
  if (JS_IsException(buffer_class)) {
    JS_FreeValue(ctx, buffer_module);
    return JS_NULL;
  }

  JSValue result = JS_NULL;

  if (!data || len == 0) {
    // Create empty Buffer
    JSValue alloc_func = JS_GetPropertyStr(ctx, buffer_class, "alloc");
    if (JS_IsFunction(ctx, alloc_func)) {
      JSValue argv[] = {JS_NewInt32(ctx, 0)};
      result = JS_Call(ctx, alloc_func, buffer_class, 1, argv);
      JS_FreeValue(ctx, argv[0]);
    }
    JS_FreeValue(ctx, alloc_func);
  } else {
    // Create Buffer from data
    JSValue from_func = JS_GetPropertyStr(ctx, buffer_class, "from");
    if (JS_IsFunction(ctx, from_func)) {
      JSValue array_buffer = JS_NewArrayBufferCopy(ctx, (const uint8_t*)data, len);
      JSValue argv[] = {array_buffer};
      result = JS_Call(ctx, from_func, buffer_class, 1, argv);
      JS_FreeValue(ctx, array_buffer);
    }
    JS_FreeValue(ctx, from_func);
  }

  JS_FreeValue(ctx, buffer_class);
  JS_FreeValue(ctx, buffer_module);

  return result;
}

// spawnSync(command[, args][, options])
JSValue js_child_process_spawn_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("child_process.spawnSync() called with %d args", argc);

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "spawnSync() requires at least a command argument");
  }

  // Parse command
  const char* command = JS_ToCString(ctx, argv[0]);
  if (!command) {
    return JS_ThrowTypeError(ctx, "command must be a string");
  }

  // Parse args array (optional)
  char** args = NULL;
  if (argc > 1 && JS_IsArray(ctx, argv[1])) {
    args = js_array_to_string_array(ctx, argv[1]);
    if (!args) {
      JS_FreeCString(ctx, command);
      return JS_ThrowOutOfMemory(ctx);
    }
  }

  // Parse options (optional)
  JSChildProcessOptions options;
  memset(&options, 0, sizeof(options));
  options.uid = -1;
  options.gid = -1;
  options.stdio_count = 3;
  options.max_buffer = 1024 * 1024;  // Default 1MB

  if (argc > 2 && JS_IsObject(argv[2])) {
    if (parse_spawn_options(ctx, argv[2], &options) < 0) {
      JS_FreeCString(ctx, command);
      if (args)
        free_string_array(args);
      return JS_EXCEPTION;
    }

    // Parse maxBuffer
    JSValue max_buffer_val = JS_GetPropertyStr(ctx, argv[2], "maxBuffer");
    if (!JS_IsUndefined(max_buffer_val)) {
      int64_t max_buffer;
      if (!JS_ToInt64(ctx, &max_buffer, max_buffer_val)) {
        options.max_buffer = max_buffer;
      }
    }
    JS_FreeValue(ctx, max_buffer_val);

    // Parse timeout
    JSValue timeout_val = JS_GetPropertyStr(ctx, argv[2], "timeout");
    if (!JS_IsUndefined(timeout_val)) {
      int64_t timeout;
      if (!JS_ToInt64(ctx, &timeout, timeout_val)) {
        options.timeout = timeout;
      }
    }
    JS_FreeValue(ctx, timeout_val);
  }

  // Initialize sync state
  SyncState state;
  memset(&state, 0, sizeof(state));
  state.max_buffer = options.max_buffer;
  state.has_timeout = (options.timeout > 0);
  state.timeout_ms = options.timeout;

  // Create isolated event loop
  uv_loop_t loop;
  int result = uv_loop_init(&loop);
  if (result < 0) {
    JS_FreeCString(ctx, command);
    if (args)
      free_string_array(args);
    free_spawn_options(&options);
    return JS_ThrowInternalError(ctx, "Failed to create event loop");
  }

  // Initialize stdio pipes
  uv_pipe_init(&loop, &state.stdout_pipe, 0);
  uv_pipe_init(&loop, &state.stderr_pipe, 0);
  state.stdout_pipe.data = &state;
  state.stderr_pipe.data = &state;

  // Build uv_process_options_t
  uv_process_options_t uv_options;
  memset(&uv_options, 0, sizeof(uv_options));
  uv_options.exit_cb = sync_exit_cb;
  uv_options.file = command;

  // Build args array: [command, ...args, NULL]
  int arg_count = 1;  // command
  if (args) {
    for (char** p = args; *p; p++)
      arg_count++;
  }
  char** uv_args = malloc(sizeof(char*) * (arg_count + 1));
  if (!uv_args) {
    uv_loop_close(&loop);
    JS_FreeCString(ctx, command);
    if (args)
      free_string_array(args);
    free_spawn_options(&options);
    return JS_ThrowOutOfMemory(ctx);
  }
  uv_args[0] = (char*)command;
  if (args) {
    for (int i = 0; args[i]; i++) {
      uv_args[i + 1] = args[i];
    }
  }
  uv_args[arg_count] = NULL;

  uv_options.args = uv_args;
  uv_options.env = options.env;
  uv_options.cwd = options.cwd;

  // Setup stdio for sync mode: inherit stdin, pipe stdout/stderr
  uv_stdio_container_t stdio[3];
  stdio[0].flags = UV_INHERIT_FD;
  stdio[0].data.fd = 0;  // stdin
  stdio[1].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  stdio[1].data.stream = (uv_stream_t*)&state.stdout_pipe;
  stdio[2].flags = UV_CREATE_PIPE | UV_WRITABLE_PIPE;
  stdio[2].data.stream = (uv_stream_t*)&state.stderr_pipe;

  uv_options.stdio_count = 3;
  uv_options.stdio = stdio;
  uv_options.flags = 0;

  // Set uid/gid on POSIX
#ifndef _WIN32
  if (options.uid >= 0) {
    uv_options.uid = options.uid;
    uv_options.flags |= UV_PROCESS_SETUID;
  }
  if (options.gid >= 0) {
    uv_options.gid = options.gid;
    uv_options.flags |= UV_PROCESS_SETGID;
  }
#endif

  // Spawn process
  state.process.data = &state;
  result = uv_spawn(&loop, &state.process, &uv_options);

  // Cleanup temporary args array
  free(uv_args);

  if (result < 0) {
    JSRT_Debug("uv_spawn failed: %s", uv_strerror(result));

    // Close loop and cleanup
    uv_close((uv_handle_t*)&state.stdout_pipe, NULL);
    uv_close((uv_handle_t*)&state.stderr_pipe, NULL);
    uv_run(&loop, UV_RUN_DEFAULT);
    uv_loop_close(&loop);

    JS_FreeCString(ctx, command);
    if (args)
      free_string_array(args);
    free_spawn_options(&options);

    // Create error result
    JSValue result_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result_obj, "pid", JS_NewInt32(ctx, 0));
    JS_SetPropertyStr(ctx, result_obj, "status", JS_NULL);
    JS_SetPropertyStr(ctx, result_obj, "signal", JS_NULL);
    JS_SetPropertyStr(ctx, result_obj, "stdout", create_buffer_from_data(ctx, NULL, 0));
    JS_SetPropertyStr(ctx, result_obj, "stderr", create_buffer_from_data(ctx, NULL, 0));

    JSValue error = create_spawn_error(ctx, result, command, "spawnSync");
    JS_SetPropertyStr(ctx, result_obj, "error", error);

    JSValue output = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, output, 0, JS_NULL);
    JS_SetPropertyUint32(ctx, output, 1, create_buffer_from_data(ctx, NULL, 0));
    JS_SetPropertyUint32(ctx, output, 2, create_buffer_from_data(ctx, NULL, 0));
    JS_SetPropertyStr(ctx, result_obj, "output", output);

    return result_obj;
  }

  state.pid = state.process.pid;
  JSRT_Debug("Sync process spawned with PID %d", (int)state.pid);

  // Start reading from pipes
  uv_read_start((uv_stream_t*)&state.stdout_pipe, sync_stdout_alloc, sync_stdout_read);
  uv_read_start((uv_stream_t*)&state.stderr_pipe, sync_stderr_alloc, sync_stderr_read);

  // Start timeout timer if specified
  if (state.has_timeout) {
    uv_timer_init(&loop, &state.timeout_timer);
    state.timeout_timer.data = &state;
    uv_timer_start(&state.timeout_timer, sync_timeout_cb, state.timeout_ms, 0);
  }

  // Run loop until process exits (blocking!)
  while (!state.finished) {
    uv_run(&loop, UV_RUN_ONCE);
  }

  // Stop reading from pipes
  uv_read_stop((uv_stream_t*)&state.stdout_pipe);
  uv_read_stop((uv_stream_t*)&state.stderr_pipe);

  // Stop timeout timer if it was started
  if (state.has_timeout) {
    uv_timer_stop(&state.timeout_timer);
  }

  // Close all handles
  if (!uv_is_closing((uv_handle_t*)&state.process)) {
    uv_close((uv_handle_t*)&state.process, NULL);
  }
  if (!uv_is_closing((uv_handle_t*)&state.stdout_pipe)) {
    uv_close((uv_handle_t*)&state.stdout_pipe, NULL);
  }
  if (!uv_is_closing((uv_handle_t*)&state.stderr_pipe)) {
    uv_close((uv_handle_t*)&state.stderr_pipe, NULL);
  }
  if (state.has_timeout && !uv_is_closing((uv_handle_t*)&state.timeout_timer)) {
    uv_close((uv_handle_t*)&state.timeout_timer, NULL);
  }

  // Drain loop to process close callbacks
  uv_run(&loop, UV_RUN_DEFAULT);

  // Close loop
  uv_loop_close(&loop);

  // Build result object
  JSValue result_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, result_obj, "pid", JS_NewInt32(ctx, state.pid));

  // Set status and signal
  if (state.term_signal != 0) {
    JS_SetPropertyStr(ctx, result_obj, "status", JS_NULL);
    const char* sig_name = signal_name(state.term_signal);
    JS_SetPropertyStr(ctx, result_obj, "signal", sig_name ? JS_NewString(ctx, sig_name) : JS_NULL);
  } else {
    JS_SetPropertyStr(ctx, result_obj, "status", JS_NewInt32(ctx, state.exit_code));
    JS_SetPropertyStr(ctx, result_obj, "signal", JS_NULL);
  }

  // Create buffers
  JSValue stdout_buffer = create_buffer_from_data(ctx, state.stdout_buffer, state.stdout_size);
  JSValue stderr_buffer = create_buffer_from_data(ctx, state.stderr_buffer, state.stderr_size);

  JS_SetPropertyStr(ctx, result_obj, "stdout", JS_DupValue(ctx, stdout_buffer));
  JS_SetPropertyStr(ctx, result_obj, "stderr", JS_DupValue(ctx, stderr_buffer));

  // Create output array
  JSValue output = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, output, 0, JS_NULL);
  JS_SetPropertyUint32(ctx, output, 1, stdout_buffer);
  JS_SetPropertyUint32(ctx, output, 2, stderr_buffer);
  JS_SetPropertyStr(ctx, result_obj, "output", output);

  // Set error if applicable
  if (state.buffer_exceeded) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "stdout maxBuffer exceeded"));
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ERR_CHILD_PROCESS_STDIO_MAXBUFFER"));
    JS_SetPropertyStr(ctx, result_obj, "error", error);
  } else if (state.timeout_expired) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Timeout expired"));
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ETIMEDOUT"));
    JS_SetPropertyStr(ctx, error, "killed", JS_NewBool(ctx, true));
    JS_SetPropertyStr(ctx, result_obj, "error", error);
  } else {
    JS_SetPropertyStr(ctx, result_obj, "error", JS_UNDEFINED);
  }

  // Cleanup
  free(state.stdout_buffer);
  free(state.stderr_buffer);
  JS_FreeCString(ctx, command);
  if (args)
    free_string_array(args);
  free_spawn_options(&options);

  return result_obj;
}

// execSync(command[, options])
JSValue js_child_process_exec_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("child_process.execSync() called with %d args", argc);

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "execSync() requires a command argument");
  }

  // Parse command
  const char* command = JS_ToCString(ctx, argv[0]);
  if (!command) {
    return JS_ThrowTypeError(ctx, "command must be a string");
  }

  // Build options with shell: true
  JSValue exec_options = JS_NewObject(ctx);
  if (argc > 1 && JS_IsObject(argv[1])) {
    // Copy properties from provided options
    JSPropertyEnum* props;
    uint32_t prop_count;

    if (!JS_GetOwnPropertyNames(ctx, &props, &prop_count, argv[1], JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY)) {
      for (uint32_t i = 0; i < prop_count; i++) {
        JSValue val = JS_GetProperty(ctx, argv[1], props[i].atom);
        JS_SetProperty(ctx, exec_options, props[i].atom, val);
        JS_FreeAtom(ctx, props[i].atom);
      }
      js_free(ctx, props);
    }
  }

  // Determine shell
#ifdef _WIN32
  const char* shell = "cmd.exe";
  JSValue args_array = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, args_array, 0, JS_NewString(ctx, "/c"));
  JS_SetPropertyUint32(ctx, args_array, 1, JS_NewString(ctx, command));
#else
  const char* shell = "/bin/sh";
  JSValue args_array = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, args_array, 0, JS_NewString(ctx, "-c"));
  JS_SetPropertyUint32(ctx, args_array, 1, JS_NewString(ctx, command));
#endif

  // Call spawnSync
  JSValue spawn_argv[3] = {JS_NewString(ctx, shell), args_array, exec_options};
  JSValue result = js_child_process_spawn_sync(ctx, this_val, 3, spawn_argv);

  JS_FreeValue(ctx, spawn_argv[0]);
  JS_FreeValue(ctx, spawn_argv[1]);
  JS_FreeValue(ctx, spawn_argv[2]);

  if (JS_IsException(result)) {
    JS_FreeCString(ctx, command);
    return result;
  }

  // Check for error
  JSValue error_val = JS_GetPropertyStr(ctx, result, "error");
  if (!JS_IsUndefined(error_val) && !JS_IsNull(error_val)) {
    // Attach stdout/stderr to error
    JSValue stdout = JS_GetPropertyStr(ctx, result, "stdout");
    JSValue stderr = JS_GetPropertyStr(ctx, result, "stderr");
    JSValue status = JS_GetPropertyStr(ctx, result, "status");

    JS_SetPropertyStr(ctx, error_val, "stdout", stdout);
    JS_SetPropertyStr(ctx, error_val, "stderr", stderr);
    JS_SetPropertyStr(ctx, error_val, "status", status);

    JS_FreeValue(ctx, result);
    JS_FreeCString(ctx, command);
    return JS_Throw(ctx, error_val);
  }
  JS_FreeValue(ctx, error_val);

  // Check exit code
  JSValue status = JS_GetPropertyStr(ctx, result, "status");
  int exit_code = 0;
  if (!JS_IsNull(status)) {
    JS_ToInt32(ctx, &exit_code, status);
  }
  JS_FreeValue(ctx, status);

  if (exit_code != 0) {
    // Create error
    JSValue signal = JS_GetPropertyStr(ctx, result, "signal");
    const char* signal_str = NULL;
    if (!JS_IsNull(signal)) {
      signal_str = JS_ToCString(ctx, signal);
    }
    JS_FreeValue(ctx, signal);

    JSValue error = create_exec_error(ctx, exit_code, signal_str, command);

    if (signal_str) {
      JS_FreeCString(ctx, signal_str);
    }

    // Attach stdout/stderr to error
    JSValue stdout = JS_GetPropertyStr(ctx, result, "stdout");
    JSValue stderr = JS_GetPropertyStr(ctx, result, "stderr");

    JS_SetPropertyStr(ctx, error, "stdout", stdout);
    JS_SetPropertyStr(ctx, error, "stderr", stderr);
    JS_SetPropertyStr(ctx, error, "status", JS_NewInt32(ctx, exit_code));

    JS_FreeValue(ctx, result);
    JS_FreeCString(ctx, command);
    return JS_Throw(ctx, error);
  }

  // Return stdout
  JSValue stdout = JS_GetPropertyStr(ctx, result, "stdout");
  JS_FreeValue(ctx, result);
  JS_FreeCString(ctx, command);

  return stdout;
}

// execFileSync(file[, args][, options])
JSValue js_child_process_exec_file_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("child_process.execFileSync() called with %d args", argc);

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "execFileSync() requires a file argument");
  }

  // Parse file
  const char* file = JS_ToCString(ctx, argv[0]);
  if (!file) {
    return JS_ThrowTypeError(ctx, "file must be a string");
  }

  // Parse optional arguments
  JSValue args = JS_UNDEFINED;
  JSValue options = JS_UNDEFINED;

  int arg_idx = 1;

  // Next arg could be args array
  if (arg_idx < argc && JS_IsArray(ctx, argv[arg_idx])) {
    args = argv[arg_idx++];
  }

  // Next arg could be options
  if (arg_idx < argc && JS_IsObject(argv[arg_idx])) {
    options = argv[arg_idx++];
  }

  // If no args array provided, create empty one
  if (JS_IsUndefined(args)) {
    args = JS_NewArray(ctx);
  } else {
    args = JS_DupValue(ctx, args);
  }

  // Call spawnSync
  JSValue spawn_argv[3] = {JS_NewString(ctx, file), args,
                           JS_IsUndefined(options) ? JS_NewObject(ctx) : JS_DupValue(ctx, options)};

  JSValue result = js_child_process_spawn_sync(ctx, this_val, 3, spawn_argv);

  JS_FreeValue(ctx, spawn_argv[0]);
  JS_FreeValue(ctx, spawn_argv[1]);
  JS_FreeValue(ctx, spawn_argv[2]);

  if (JS_IsException(result)) {
    JS_FreeCString(ctx, file);
    return result;
  }

  // Check for error
  JSValue error_val = JS_GetPropertyStr(ctx, result, "error");
  if (!JS_IsUndefined(error_val) && !JS_IsNull(error_val)) {
    // Attach stdout/stderr to error
    JSValue stdout = JS_GetPropertyStr(ctx, result, "stdout");
    JSValue stderr = JS_GetPropertyStr(ctx, result, "stderr");
    JSValue status = JS_GetPropertyStr(ctx, result, "status");

    JS_SetPropertyStr(ctx, error_val, "stdout", stdout);
    JS_SetPropertyStr(ctx, error_val, "stderr", stderr);
    JS_SetPropertyStr(ctx, error_val, "status", status);

    JS_FreeValue(ctx, result);
    JS_FreeCString(ctx, file);
    return JS_Throw(ctx, error_val);
  }
  JS_FreeValue(ctx, error_val);

  // Check exit code
  JSValue status = JS_GetPropertyStr(ctx, result, "status");
  int exit_code = 0;
  if (!JS_IsNull(status)) {
    JS_ToInt32(ctx, &exit_code, status);
  }
  JS_FreeValue(ctx, status);

  if (exit_code != 0) {
    // Create error
    JSValue signal = JS_GetPropertyStr(ctx, result, "signal");
    const char* signal_str = NULL;
    if (!JS_IsNull(signal)) {
      signal_str = JS_ToCString(ctx, signal);
    }
    JS_FreeValue(ctx, signal);

    JSValue error = create_exec_error(ctx, exit_code, signal_str, file);

    if (signal_str) {
      JS_FreeCString(ctx, signal_str);
    }

    // Attach stdout/stderr to error
    JSValue stdout = JS_GetPropertyStr(ctx, result, "stdout");
    JSValue stderr = JS_GetPropertyStr(ctx, result, "stderr");

    JS_SetPropertyStr(ctx, error, "stdout", stdout);
    JS_SetPropertyStr(ctx, error, "stderr", stderr);
    JS_SetPropertyStr(ctx, error, "status", JS_NewInt32(ctx, exit_code));

    JS_FreeValue(ctx, result);
    JS_FreeCString(ctx, file);
    return JS_Throw(ctx, error);
  }

  // Return stdout
  JSValue stdout = JS_GetPropertyStr(ctx, result, "stdout");
  JS_FreeValue(ctx, result);
  JS_FreeCString(ctx, file);

  return stdout;
}
