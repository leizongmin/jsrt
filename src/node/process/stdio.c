/**
 * Process stdio implementation (stdout, stderr, stdin)
 */

#include <stdio.h>
#include "process.h"

// stdout.write() implementation
static JSValue js_stdout_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write() requires at least 1 argument");
  }

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_EXCEPTION;
  }

  // Write to stdout without newline
  fputs(str, stdout);
  fflush(stdout);
  JS_FreeCString(ctx, str);

  // Return true to indicate success (Node.js behavior)
  return JS_NewBool(ctx, 1);
}

// stderr.write() implementation
static JSValue js_stderr_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write() requires at least 1 argument");
  }

  const char* str = JS_ToCString(ctx, argv[0]);
  if (!str) {
    return JS_EXCEPTION;
  }

  // Write to stderr without newline
  fputs(str, stderr);
  fflush(stderr);
  JS_FreeCString(ctx, str);

  // Return true to indicate success (Node.js behavior)
  return JS_NewBool(ctx, 1);
}

// Create stdout object
JSValue jsrt_create_stdout(JSContext* ctx) {
  JSValue stdout_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, stdout_obj, "write", JS_NewCFunction(ctx, js_stdout_write, "write", 1));
  JS_SetPropertyStr(ctx, stdout_obj, "isTTY", JS_NewBool(ctx, 1));  // Simplified: assume TTY
  return stdout_obj;
}

// Create stderr object
JSValue jsrt_create_stderr(JSContext* ctx) {
  JSValue stderr_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, stderr_obj, "write", JS_NewCFunction(ctx, js_stderr_write, "write", 1));
  JS_SetPropertyStr(ctx, stderr_obj, "isTTY", JS_NewBool(ctx, 1));  // Simplified: assume TTY
  return stderr_obj;
}

// Create stdin object (placeholder for now)
JSValue jsrt_create_stdin(JSContext* ctx) {
  JSValue stdin_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, stdin_obj, "isTTY", JS_NewBool(ctx, 1));  // Simplified: assume TTY
  // TODO: Add read/readline functionality if needed
  return stdin_obj;
}
