#include <unistd.h>
#include "../../util/debug.h"
#include "child_process_internal.h"

// ChildProcess.prototype.send(message[, sendHandle][, options][, callback])
JSValue js_child_process_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "send() requires at least 1 argument");
  }

  // Get ChildProcess opaque data
  JSChildProcess* child = JS_GetOpaque(this_val, js_child_process_class_id);
  if (!child) {
    return JS_ThrowTypeError(ctx, "Invalid ChildProcess object");
  }

  // Check if IPC channel exists
  if (!child->ipc_channel || !child->connected) {
    return JS_ThrowInternalError(ctx, "Channel closed");
  }

  JSValue message = argv[0];
  JSValue callback = JS_UNDEFINED;

  // Extract callback from various positions:
  // send(message, callback)
  // send(message, sendHandle, callback)
  // send(message, sendHandle, options, callback)
  if (argc >= 2 && JS_IsFunction(ctx, argv[argc - 1])) {
    callback = argv[argc - 1];
  }

  // TODO: sendHandle support (file descriptors, sockets) - Phase 7
  // For now, we only support JSON-serializable messages

  // Send the message
  int result = send_ipc_message(child->ipc_channel, message, callback);

  if (result < 0) {
    return JS_FALSE;
  }

  return JS_TRUE;
}

// ChildProcess.prototype.disconnect()
JSValue js_child_process_disconnect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSChildProcess* child = JS_GetOpaque(this_val, js_child_process_class_id);
  if (!child) {
    return JS_ThrowTypeError(ctx, "Invalid ChildProcess object");
  }

  // Disconnect IPC channel if connected
  if (child->ipc_channel && child->connected) {
    disconnect_ipc_channel(child->ipc_channel);
    child->connected = false;

    // Update the connected property on the JS object
    JS_SetPropertyStr(ctx, this_val, "connected", JS_FALSE);

    // Emit 'disconnect' event (no additional arguments)
    emit_event(ctx, child->child_obj, "disconnect", 0, NULL);
  }

  return JS_UNDEFINED;
}

// fork(modulePath[, args][, options])
JSValue js_child_process_fork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "fork() requires at least 1 argument (modulePath)");
  }

  // Get module path
  const char* module_path = JS_ToCString(ctx, argv[0]);
  if (!module_path) {
    return JS_EXCEPTION;
  }

  // Get args array (optional)
  JSValue args_val = (argc >= 2 && JS_IsArray(ctx, argv[1])) ? argv[1] : JS_UNDEFINED;

  // Get options (optional)
  JSValue options_val = JS_UNDEFINED;
  if (argc >= 3 && JS_IsObject(argv[2])) {
    options_val = argv[2];
  } else if (argc >= 2 && JS_IsObject(argv[1]) && !JS_IsArray(ctx, argv[1])) {
    // fork(modulePath, options) - no args
    options_val = argv[1];
    args_val = JS_UNDEFINED;
  }

  // Get process.execPath (jsrt binary path)
  JSRT_Runtime* rt = (JSRT_Runtime*)JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  if (!rt) {
    JS_FreeCString(ctx, module_path);
    return JS_ThrowInternalError(ctx, "Runtime not available");
  }

  // Build fork options by merging user options with IPC setup
  JSValue fork_options = JS_NewObject(ctx);

  // Copy user options if provided
  if (!JS_IsUndefined(options_val)) {
    // Copy common options
    JSValue cwd = JS_GetPropertyStr(ctx, options_val, "cwd");
    if (!JS_IsUndefined(cwd)) {
      JS_SetPropertyStr(ctx, fork_options, "cwd", cwd);
    } else {
      JS_FreeValue(ctx, cwd);
    }

    JSValue env = JS_GetPropertyStr(ctx, options_val, "env");
    if (!JS_IsUndefined(env)) {
      JS_SetPropertyStr(ctx, fork_options, "env", env);
    } else {
      JS_FreeValue(ctx, env);
    }

    JSValue silent = JS_GetPropertyStr(ctx, options_val, "silent");
    if (!JS_IsUndefined(silent)) {
      JS_SetPropertyStr(ctx, fork_options, "silent", silent);
    } else {
      JS_FreeValue(ctx, silent);
    }

    // execArgv - Node.js V8 flags, ignore for jsrt
  }

  // Configure stdio for IPC
  // stdio: ['pipe', 'pipe', 'pipe', 'ipc'] or ['inherit', 'inherit', 'inherit', 'ipc'] if silent
  JSValue silent_val = JS_GetPropertyStr(ctx, fork_options, "silent");
  bool silent = JS_ToBool(ctx, silent_val);
  JS_FreeValue(ctx, silent_val);

  JSValue stdio = JS_NewArray(ctx);
  if (silent) {
    JS_SetPropertyUint32(ctx, stdio, 0, JS_NewString(ctx, "pipe"));
    JS_SetPropertyUint32(ctx, stdio, 1, JS_NewString(ctx, "pipe"));
    JS_SetPropertyUint32(ctx, stdio, 2, JS_NewString(ctx, "pipe"));
  } else {
    JS_SetPropertyUint32(ctx, stdio, 0, JS_NewString(ctx, "inherit"));
    JS_SetPropertyUint32(ctx, stdio, 1, JS_NewString(ctx, "inherit"));
    JS_SetPropertyUint32(ctx, stdio, 2, JS_NewString(ctx, "inherit"));
  }
  JS_SetPropertyUint32(ctx, stdio, 3, JS_NewString(ctx, "ipc"));
  JS_SetPropertyStr(ctx, fork_options, "stdio", stdio);

  // Build command: use jsrt binary path
  // Try to read /proc/self/exe on Linux
  char exec_path_buf[4096];
  const char* exec_path = "jsrt";  // Fallback

#ifdef __linux__
  ssize_t len = readlink("/proc/self/exe", exec_path_buf, sizeof(exec_path_buf) - 1);
  if (len > 0) {
    exec_path_buf[len] = '\0';
    exec_path = exec_path_buf;
  }
#endif

  // Build args: [module_path, ...user_args]
  JSValue spawn_args = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, spawn_args, 0, JS_NewString(ctx, module_path));

  if (!JS_IsUndefined(args_val)) {
    JSValue length_val = JS_GetPropertyStr(ctx, args_val, "length");
    uint32_t length;
    if (!JS_ToUint32(ctx, &length, length_val)) {
      for (uint32_t i = 0; i < length; i++) {
        JSValue arg = JS_GetPropertyUint32(ctx, args_val, i);
        JS_SetPropertyUint32(ctx, spawn_args, i + 1, arg);
      }
    }
    JS_FreeValue(ctx, length_val);
  }

  // Call spawn() with fork configuration
  JSValue spawn_argv[3] = {JS_NewString(ctx, exec_path), spawn_args, fork_options};

  JSValue child = js_child_process_spawn(ctx, this_val, 3, spawn_argv);

  // Cleanup
  JS_FreeValue(ctx, spawn_argv[0]);
  JS_FreeValue(ctx, spawn_argv[1]);
  JS_FreeValue(ctx, spawn_argv[2]);
  JS_FreeCString(ctx, module_path);

  if (JS_IsException(child)) {
    return child;
  }

  // Setup IPC channel on the child object
  JSChildProcess* child_data = JS_GetOpaque(child, js_child_process_class_id);
  if (!child_data) {
    return child;  // Return the child even if IPC setup failed
  }

  // The IPC channel is created in spawn() when stdio[3] = 'ipc'
  // Just verify it exists
  if (child_data->ipc_channel) {
    child_data->connected = true;

    // Set connected property
    JS_SetPropertyStr(ctx, child, "connected", JS_TRUE);

    // Start reading from IPC channel
    start_ipc_reading(child_data->ipc_channel);
  }

  return child;
}
