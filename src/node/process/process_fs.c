#include "process_fs.h"
#include <errno.h>
#include <quickjs.h>
#include <stdlib.h>
#include <string.h>
#include "process_platform.h"

// Process current working directory function
static JSValue js_process_cwd(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  size_t path_max = jsrt_get_path_max();
  char* buf = malloc(path_max);
  if (!buf) {
    return JS_ThrowOutOfMemory(ctx);
  }

  char* result = JSRT_GETCWD(buf, path_max);
  if (!result) {
    free(buf);
    return JS_ThrowTypeError(ctx, "Failed to get current working directory: %s", strerror(errno));
  }

  JSValue ret = JS_NewString(ctx, buf);
  free(buf);
  return ret;
}

// Process change directory function
static JSValue js_process_chdir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "chdir requires a path argument");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  int result = JSRT_CHDIR(path);
  JS_FreeCString(ctx, path);

  if (result != 0) {
    return JS_ThrowTypeError(ctx, "Failed to change directory: %s", strerror(errno));
  }

  return JS_UNDEFINED;
}

// Initialize process filesystem functions
void jsrt_process_fs_init(JSContext* ctx, JSValue process_obj) {
  // Add cwd method
  JS_SetPropertyStr(ctx, process_obj, "cwd", JS_NewCFunction(ctx, js_process_cwd, "cwd", 0));

  // Add chdir method
  JS_SetPropertyStr(ctx, process_obj, "chdir", JS_NewCFunction(ctx, js_process_chdir, "chdir", 1));
}