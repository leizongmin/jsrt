#include "../../util/debug.h"
#include "child_process_internal.h"

// Create a spawn error
JSValue create_spawn_error(JSContext* ctx, int uv_error, const char* path, const char* syscall) {
  JSValue error = JS_NewError(ctx);

  const char* code_str = uv_err_name(uv_error);
  const char* message_str = uv_strerror(uv_error);

  JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, code_str));
  JS_SetPropertyStr(ctx, error, "errno", JS_NewInt32(ctx, uv_error));
  JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, syscall));

  if (path) {
    JS_SetPropertyStr(ctx, error, "path", JS_NewString(ctx, path));

    char full_message[512];
    snprintf(full_message, sizeof(full_message), "%s %s %s", syscall, path, message_str);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, full_message));
  } else {
    char full_message[256];
    snprintf(full_message, sizeof(full_message), "%s %s", syscall, message_str);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, full_message));
  }

  return error;
}

// Create an exec error (for timeout or non-zero exit)
JSValue create_exec_error(JSContext* ctx, int exit_code, const char* signal, const char* cmd) {
  JSValue error = JS_NewError(ctx);

  char message[512];
  if (signal) {
    snprintf(message, sizeof(message), "Command failed: %s (killed by signal %s)", cmd, signal);
    JS_SetPropertyStr(ctx, error, "signal", JS_NewString(ctx, signal));
  } else {
    snprintf(message, sizeof(message), "Command failed: %s (exit code %d)", cmd, exit_code);
  }

  JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));
  JS_SetPropertyStr(ctx, error, "code", JS_NewInt32(ctx, exit_code));
  JS_SetPropertyStr(ctx, error, "cmd", JS_NewString(ctx, cmd));

  return error;
}
