#include <stdlib.h>
#include <string.h>
#include "process.h"

// External global variables for command line arguments (defined in process.c)
extern int jsrt_argc;
extern char** jsrt_argv;

// Process ID getter
JSValue js_process_get_pid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewInt32(ctx, jsrt_process_getpid());
}

// Parent process ID getter
JSValue js_process_get_ppid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewInt32(ctx, jsrt_process_getppid());
}

// Command line arguments getter
JSValue js_process_get_argv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue argv_array = JS_NewArray(ctx);

  if (JS_IsException(argv_array)) {
    return JS_EXCEPTION;
  }

  for (int i = 0; i < jsrt_argc; i++) {
    JSValue arg = JS_NewString(ctx, jsrt_argv[i]);
    if (JS_IsException(arg)) {
      JS_FreeValue(ctx, argv_array);
      return JS_EXCEPTION;
    }

    if (JS_SetPropertyUint32(ctx, argv_array, i, arg) < 0) {
      JS_FreeValue(ctx, arg);
      JS_FreeValue(ctx, argv_array);
      return JS_EXCEPTION;
    }
  }

  return argv_array;
}

// First command line argument (executable path) getter
JSValue js_process_get_argv0(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (jsrt_argc > 0 && jsrt_argv && jsrt_argv[0]) {
    return JS_NewString(ctx, jsrt_argv[0]);
  }
  return JS_NewString(ctx, "");
}

// Platform name getter
JSValue js_process_get_platform(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewString(ctx, jsrt_process_platform());
}

// Architecture name getter
JSValue js_process_get_arch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewString(ctx, jsrt_process_arch());
}

void jsrt_process_init_basic(void) {
  // Basic initialization if needed
  // Currently no initialization required
}