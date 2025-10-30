#include <stdlib.h>
#include <string.h>
#include "../../util/debug.h"
#include "process.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// Global variables for process properties
static char* g_exec_path = NULL;
static JSValue g_exec_argv = {0};  // Will be initialized to array
static int g_exit_code_set = 0;
static int g_exit_code = 0;
static char* g_process_title = NULL;

// Helper function to get executable path (platform-specific)
static const char* get_executable_path(void) {
  if (g_exec_path) {
    return g_exec_path;
  }

  static char buf[4096];
  ssize_t len = 0;

#ifdef __linux__
  // Linux: read /proc/self/exe
  len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len > 0) {
    buf[len] = '\0';
    g_exec_path = strdup(buf);
    return g_exec_path;
  }
#elif defined(__APPLE__)
  // macOS: use _NSGetExecutablePath
  uint32_t size = sizeof(buf);
  if (_NSGetExecutablePath(buf, &size) == 0) {
    g_exec_path = strdup(buf);
    return g_exec_path;
  }
#elif defined(_WIN32)
  // Windows: use GetModuleFileName
  DWORD result = GetModuleFileNameA(NULL, buf, sizeof(buf));
  if (result > 0 && result < sizeof(buf)) {
    buf[result] = '\0';
    g_exec_path = strdup(buf);
    return g_exec_path;
  }
#endif

  // Fallback to argv[0]
  extern char** jsrt_argv;
  if (jsrt_argv && jsrt_argv[0]) {
    return jsrt_argv[0];
  }

  return "jsrt";
}

// process.execPath getter
JSValue js_process_get_execPath(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* path = get_executable_path();
  return JS_NewString(ctx, path);
}

// process.execArgv getter
JSValue js_process_get_execArgv(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // For now, return empty array (jsrt doesn't have Node-specific flags yet)
  // TODO: Parse flags like --inspect, --max-old-space-size when implemented
  JSValue arr = JS_NewArray(ctx);
  return arr;
}

// process.exitCode getter
JSValue js_process_get_exitCode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (!g_exit_code_set) {
    return JS_UNDEFINED;
  }
  return JS_NewInt32(ctx, g_exit_code);
}

// process.exitCode setter
JSValue js_process_set_exitCode(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    g_exit_code_set = 0;
    return JS_UNDEFINED;
  }

  if (JS_IsUndefined(argv[0]) || JS_IsNull(argv[0])) {
    g_exit_code_set = 0;
    return JS_UNDEFINED;
  }

  int32_t code;
  if (JS_ToInt32(ctx, &code, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  g_exit_code = code;
  g_exit_code_set = 1;
  return JS_UNDEFINED;
}

// process.title getter
JSValue js_process_get_title(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (g_process_title) {
    return JS_NewString(ctx, g_process_title);
  }
  // Default to executable name
  const char* path = get_executable_path();
  return JS_NewString(ctx, path);
}

// process.title setter
JSValue js_process_set_title(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_UNDEFINED;
  }

  const char* title = JS_ToCString(ctx, argv[0]);
  if (!title) {
    return JS_EXCEPTION;
  }

  if (g_process_title) {
    free(g_process_title);
  }
  g_process_title = strdup(title);
  JS_FreeCString(ctx, title);

  // TODO: Actually set process title using prctl (Linux) or setproctitle (BSD/macOS)
  // For now, just store the value

  return JS_UNDEFINED;
}

// process.config getter (stub - returns empty object)
JSValue js_process_get_config(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue config = JS_NewObject(ctx);
  // TODO: Populate with build configuration
  // For now, return empty object
  return config;
}

// process.release getter
JSValue js_process_get_release(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue release = JS_NewObject(ctx);

  // Basic release information
  JS_SetPropertyStr(ctx, release, "name", JS_NewString(ctx, "jsrt"));

  // Get jsrt version from compile-time definition
#ifdef JSRT_VERSION
  JS_SetPropertyStr(ctx, release, "version", JS_NewString(ctx, JSRT_VERSION));
#else
  JS_SetPropertyStr(ctx, release, "version", JS_NewString(ctx, "unknown"));
#endif

  return release;
}

// process.features getter
JSValue js_process_get_features(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue features = JS_NewObject(ctx);

  // Feature detection
  JS_SetPropertyStr(ctx, features, "debug",
                    JS_NewBool(ctx,
#ifdef DEBUG
                               1
#else
                               0
#endif
                               ));

  JS_SetPropertyStr(ctx, features, "uv", JS_NewBool(ctx, 1));    // Always have libuv
  JS_SetPropertyStr(ctx, features, "ipv6", JS_NewBool(ctx, 1));  // Assume IPv6 support
  JS_SetPropertyStr(ctx, features, "tls", JS_NewBool(ctx, 1));   // Have TLS support

  return features;
}

// Get exit code for process.exit() to use
int jsrt_process_get_exit_code_internal(void) {
  return g_exit_code_set ? g_exit_code : 0;
}

// Initialization function
void jsrt_process_init_properties(void) {
  // Pre-cache executable path at startup
  get_executable_path();

  JSRT_Debug("Process properties module initialized");
}

// Cleanup function
void jsrt_process_cleanup_properties(void) {
  if (g_exec_path) {
    free(g_exec_path);
    g_exec_path = NULL;
  }

  if (g_process_title) {
    free(g_process_title);
    g_process_title = NULL;
  }

  // Note: g_exec_argv is managed by QuickJS, freed by context cleanup
}
