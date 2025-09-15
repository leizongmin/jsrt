#include <stdlib.h>
#include <string.h>
#include "process.h"

#ifdef _WIN32
#include <direct.h>
#define PATH_MAX 260
#else
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif

// Process exit function
JSValue js_process_exit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  int exit_code = 0;

  // Get exit code from first argument, default to 0
  if (argc > 0) {
    if (JS_ToInt32(ctx, &exit_code, argv[0]) < 0) {
      // If conversion fails, use default exit code 0
      exit_code = 0;
    }
  }

  // Ensure exit code is in valid range (0-255 on most platforms)
  if (exit_code < 0) {
    exit_code = 1;  // Convert negative codes to 1
  } else if (exit_code > 255) {
    exit_code = exit_code & 0xFF;  // Truncate to 8 bits
  }

  // Exit the process immediately
  exit(exit_code);

  // This line should never be reached, but return undefined for completeness
  return JS_UNDEFINED;
}

// Process current working directory getter
JSValue js_process_cwd(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  char* cwd_buf = malloc(PATH_MAX);
  if (cwd_buf == NULL) {
    return JS_ThrowOutOfMemory(ctx);
  }

  char* result = jsrt_process_getcwd(cwd_buf, PATH_MAX);
  if (result == NULL) {
    free(cwd_buf);
    return JS_ThrowInternalError(ctx, "Failed to get current working directory");
  }

  JSValue cwd_str = JS_NewString(ctx, cwd_buf);
  free(cwd_buf);
  return cwd_str;
}

// Process change directory function
JSValue js_process_chdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "process.chdir() requires a directory path argument");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (path == NULL) {
    return JS_EXCEPTION;
  }

  int result = jsrt_process_chdir(path);
  JS_FreeCString(ctx, path);

  if (result != 0) {
    return JS_ThrowInternalError(ctx, "Failed to change directory");
  }

  return JS_UNDEFINED;
}

void jsrt_process_init_control(void) {
  // Control initialization if needed
  // Currently no initialization required
}