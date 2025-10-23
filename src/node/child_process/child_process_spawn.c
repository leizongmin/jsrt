#include <sys/stat.h>
#include "../../util/debug.h"
#include "child_process_internal.h"

// External environment variable (POSIX)
#ifndef _WIN32
extern char** environ;
#endif

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

// ChildProcess.killed getter
static JSValue js_child_process_get_killed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSChildProcess* child = JS_GetOpaque(this_val, js_child_process_class_id);
  if (!child) {
    return JS_ThrowTypeError(ctx, "Not a ChildProcess instance");
  }
  return JS_NewBool(ctx, child->killed);
}

// spawn(command, args, options)
JSValue js_child_process_spawn(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("child_process.spawn() called with %d args", argc);

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "spawn() requires at least a command argument");
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
  if (argc > 2) {
    if (parse_spawn_options(ctx, argv[2], &options) < 0) {
      JS_FreeCString(ctx, command);
      if (args)
        free_string_array(args);
      return JS_EXCEPTION;
    }
  } else {
    memset(&options, 0, sizeof(options));
    options.uid = -1;
    options.gid = -1;
    options.stdio_count = 3;
    // Default stdio to pipe (Node.js default for spawn)
    for (int i = 0; i < 3; i++) {
      options.stdio[i].flags = UV_CREATE_PIPE;
    }
  }

  // Create ChildProcess instance
  JSValue child_obj = JS_NewObjectClass(ctx, js_child_process_class_id);
  if (JS_IsException(child_obj)) {
    JS_FreeCString(ctx, command);
    if (args)
      free_string_array(args);
    free_spawn_options(&options);
    return child_obj;
  }

  // Allocate ChildProcess state
  JSChildProcess* child = js_mallocz(ctx, sizeof(JSChildProcess));
  if (!child) {
    JS_FreeValue(ctx, child_obj);
    JS_FreeCString(ctx, command);
    if (args)
      free_string_array(args);
    free_spawn_options(&options);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize ChildProcess state
  child->type_tag = CHILD_PROCESS_TYPE_TAG;
  child->ctx = ctx;
  child->child_obj = JS_DupValue(ctx, child_obj);
  child->spawned = false;
  child->exited = false;
  child->killed = false;
  child->connected = false;
  child->in_callback = false;
  child->pid = 0;
  child->exit_code = 0;
  child->signal_code = 0;
  child->stdin_pipe = NULL;
  child->stdout_pipe = NULL;
  child->stderr_pipe = NULL;
  child->ipc_channel = NULL;
  child->stdin_stream = JS_UNDEFINED;
  child->stdout_stream = JS_UNDEFINED;
  child->stderr_stream = JS_UNDEFINED;
  child->close_count = 0;
  child->handles_to_close = 0;
  child->buffering = false;
  child->stdout_buffer = NULL;
  child->stderr_buffer = NULL;
  child->stdout_size = 0;
  child->stderr_size = 0;
  child->stdout_capacity = 0;
  child->stderr_capacity = 0;
  child->max_buffer = 0;
  child->exec_callback = JS_UNDEFINED;
  child->timeout_timer = NULL;
  child->timeout_ms = 0;
  child->file = strdup(command);
  child->args = args;
  child->cwd = options.cwd ? strdup(options.cwd) : NULL;
  child->env = options.env;  // Take ownership
  child->uid = options.uid;
  child->gid = options.gid;

  // Setup stdio pipes
  if (setup_stdio_pipes(ctx, child, &options) < 0) {
    JSRT_Debug("Failed to setup stdio pipes");
    js_free(ctx, child);
    JS_FreeValue(ctx, child_obj);
    JS_FreeCString(ctx, command);
    free_spawn_options(&options);
    return JS_ThrowInternalError(ctx, "Failed to setup stdio pipes");
  }

  // Build uv_process_options_t
  uv_process_options_t uv_options;
  memset(&uv_options, 0, sizeof(uv_options));
  uv_options.exit_cb = on_process_exit;
  uv_options.file = command;

  // Build args array: [command, ...args, NULL]
  int arg_count = 1;  // command
  if (args) {
    for (char** p = args; *p; p++)
      arg_count++;
  }
  char** uv_args = malloc(sizeof(char*) * (arg_count + 1));
  if (!uv_args) {
    js_free(ctx, child);
    JS_FreeValue(ctx, child_obj);
    JS_FreeCString(ctx, command);
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
  // If no env specified, inherit parent environment
#ifndef _WIN32
  uv_options.env = child->env ? child->env : environ;
#else
  // On Windows, NULL means inherit
  uv_options.env = child->env;
#endif
  uv_options.cwd = child->cwd;
  uv_options.stdio_count = options.stdio_count;
  uv_options.stdio = options.stdio;

  // Set process flags
  uv_options.flags = 0;
  if (options.detached) {
    uv_options.flags |= UV_PROCESS_DETACHED;
  }
#ifdef _WIN32
  if (options.windows_hide) {
    uv_options.flags |= UV_PROCESS_WINDOWS_HIDE;
  }
#endif

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

  // Validate working directory if specified
  if (child->cwd) {
    struct stat st;
    if (stat(child->cwd, &st) != 0) {
      // Directory doesn't exist
      JSRT_Debug("cwd validation failed: %s does not exist", child->cwd);

      // Cleanup before error
      free(uv_args);
      JS_FreeCString(ctx, command);
      free_spawn_options(&options);

      // Create error and emit 'error' event
      JSValue error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ENOENT"));
      JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, -2));
      JS_SetPropertyStr(ctx, error, "path", JS_NewString(ctx, child->cwd));
      JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, "spawn"));

      char msg[512];
      snprintf(msg, sizeof(msg), "spawn %s ENOENT: no such directory '%s'", child->file, child->cwd);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, msg));

      // Add EventEmitter methods before emitting
      add_event_emitter_methods(ctx, child_obj);

      // Set opaque before emitting
      JS_SetOpaque(child_obj, child);

      // Emit error event asynchronously
      JSValue argv_emit[] = {error};
      emit_event_async(ctx, child_obj, "error", 1, argv_emit);

      return child_obj;
    }

    if (!S_ISDIR(st.st_mode)) {
      // Path exists but is not a directory
      JSRT_Debug("cwd validation failed: %s is not a directory", child->cwd);

      // Cleanup before error
      free(uv_args);
      JS_FreeCString(ctx, command);
      free_spawn_options(&options);

      // Create error and emit 'error' event
      JSValue error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ENOTDIR"));
      JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, -20));
      JS_SetPropertyStr(ctx, error, "path", JS_NewString(ctx, child->cwd));
      JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, "spawn"));

      char msg[512];
      snprintf(msg, sizeof(msg), "spawn %s ENOTDIR: not a directory '%s'", child->file, child->cwd);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, msg));

      // Add EventEmitter methods before emitting
      add_event_emitter_methods(ctx, child_obj);

      // Set opaque before emitting
      JS_SetOpaque(child_obj, child);

      // Emit error event asynchronously
      JSValue argv_emit[] = {error};
      emit_event_async(ctx, child_obj, "error", 1, argv_emit);

      return child_obj;
    }
  }

  // Spawn process
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  child->handle.data = child;
  int result = uv_spawn(rt->uv_loop, &child->handle, &uv_options);

  // Cleanup temporary args array
  free(uv_args);
  JS_FreeCString(ctx, command);
  free_spawn_options(&options);

  if (result < 0) {
    JSRT_Debug("uv_spawn failed: %s", uv_strerror(result));

    // Create error and emit 'error' event asynchronously
    JSValue error = create_spawn_error(ctx, result, child->file, "spawn");

    // Add EventEmitter methods before emitting
    add_event_emitter_methods(ctx, child_obj);

    // Set opaque before emitting
    JS_SetOpaque(child_obj, child);

    // Emit error event asynchronously
    JSValue argv_emit[] = {error};
    emit_event_async(ctx, child_obj, "error", 1, argv_emit);

    return child_obj;
  }

  // Process spawned successfully
  child->spawned = true;
  child->pid = child->handle.pid;

  JSRT_Debug("Process spawned with PID %d", child->pid);

  // Create stdio streams
  if (child->stdin_pipe) {
    child->stdin_stream = create_stdin_stream(ctx, child->stdin_pipe);
  }
  if (child->stdout_pipe) {
    child->stdout_stream = create_stdout_stream(ctx, child->stdout_pipe);
  }
  if (child->stderr_pipe) {
    child->stderr_stream = create_stderr_stream(ctx, child->stderr_pipe);
  }

  // Start reading from stdout/stderr
  if (start_stdio_reading(ctx, child) < 0) {
    JSRT_Debug("Failed to start stdio reading");
  }

  // Add EventEmitter methods
  add_event_emitter_methods(ctx, child_obj);

  // Set pid property
  JS_SetPropertyStr(ctx, child_obj, "pid", JS_NewInt32(ctx, child->pid));

  // Set killed property as getter
  JS_DefinePropertyGetSet(ctx, child_obj, JS_NewAtom(ctx, "killed"),
                          JS_NewCFunction(ctx, js_child_process_get_killed, "get killed", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  // Set stdin/stdout/stderr properties
  if (!JS_IsUndefined(child->stdin_stream)) {
    JS_SetPropertyStr(ctx, child_obj, "stdin", JS_DupValue(ctx, child->stdin_stream));
  }
  if (!JS_IsUndefined(child->stdout_stream)) {
    JS_SetPropertyStr(ctx, child_obj, "stdout", JS_DupValue(ctx, child->stdout_stream));
  }
  if (!JS_IsUndefined(child->stderr_stream)) {
    JS_SetPropertyStr(ctx, child_obj, "stderr", JS_DupValue(ctx, child->stderr_stream));
  }

  // Set opaque
  JS_SetOpaque(child_obj, child);

  // Emit 'spawn' event asynchronously (using nextTick pattern)
  JSValue argv_emit[] = {JS_NewString(ctx, "spawn")};
  emit_event(ctx, child_obj, "spawn", 1, argv_emit);
  JS_FreeValue(ctx, argv_emit[0]);

  return child_obj;
}

// ChildProcess.kill([signal])
JSValue js_child_process_kill(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSChildProcess* child = JS_GetOpaque(this_val, js_child_process_class_id);
  if (!child) {
    return JS_ThrowTypeError(ctx, "Not a ChildProcess instance");
  }

  if (child->exited) {
    return JS_NewBool(ctx, false);
  }

  // Parse signal (default SIGTERM)
  int signal = SIGTERM;
  if (argc > 0 && JS_IsString(argv[0])) {
    const char* signal_name_str = JS_ToCString(ctx, argv[0]);
    if (signal_name_str) {
      int parsed_signal = signal_from_name(signal_name_str);
      if (parsed_signal >= 0) {
        signal = parsed_signal;
      }
      JS_FreeCString(ctx, signal_name_str);
    }
  } else if (argc > 0 && JS_IsNumber(argv[0])) {
    int32_t signal_num;
    if (!JS_ToInt32(ctx, &signal_num, argv[0])) {
      signal = signal_num;
    }
  }

  int result = uv_process_kill(&child->handle, signal);
  if (result < 0) {
    return JS_NewBool(ctx, false);
  }

  child->killed = true;
  return JS_NewBool(ctx, true);
}

// ChildProcess.ref()
JSValue js_child_process_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSChildProcess* child = JS_GetOpaque(this_val, js_child_process_class_id);
  if (!child) {
    return JS_ThrowTypeError(ctx, "Not a ChildProcess instance");
  }

  if (child->spawned && !child->exited) {
    uv_ref((uv_handle_t*)&child->handle);
  }

  return this_val;
}

// ChildProcess.unref()
JSValue js_child_process_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSChildProcess* child = JS_GetOpaque(this_val, js_child_process_class_id);
  if (!child) {
    return JS_ThrowTypeError(ctx, "Not a ChildProcess instance");
  }

  if (child->spawned && !child->exited) {
    uv_unref((uv_handle_t*)&child->handle);
  }

  return this_val;
}
