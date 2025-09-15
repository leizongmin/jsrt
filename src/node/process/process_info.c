#include "process_info.h"
#include <quickjs.h>
#include <stdlib.h>
#include <string.h>
#include "process_platform.h"

// Global variables for process information
extern char** jsrt_argv;
extern int jsrt_argc;
extern char* jsrt_argv0;

// Get jsrt version implementation
const char* jsrt_get_version(void) {
#ifdef JSRT_VERSION
  return JSRT_VERSION;
#else
  return "1.0.0";  // Fallback version
#endif
}

// Process ID getter
static JSValue js_process_pid_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewInt32(ctx, JSRT_GETPID());
}

// Parent process ID getter
static JSValue js_process_ppid_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewInt32(ctx, JSRT_GETPPID());
}

// Process argv getter
static JSValue js_process_argv_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue arr = JS_NewArray(ctx);
  if (jsrt_argv && jsrt_argc > 0) {
    for (int i = 0; i < jsrt_argc; i++) {
      JS_SetPropertyUint32(ctx, arr, i, JS_NewString(ctx, jsrt_argv[i]));
    }
  }
  return arr;
}

// Process argv0 getter
static JSValue js_process_argv0_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (jsrt_argv0) {
    return JS_NewString(ctx, jsrt_argv0);
  }
  if (jsrt_argv && jsrt_argc > 0) {
    return JS_NewString(ctx, jsrt_argv[0]);
  }
  return JS_NewString(ctx, "jsrt");
}

// Process version getter
static JSValue js_process_version_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* version = jsrt_get_version();
  if (version) {
    return JS_NewString(ctx, version);
  }
  return JS_NewString(ctx, "unknown");
}

// Process platform getter
static JSValue js_process_platform_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewString(ctx, jsrt_get_platform());
}

// Process architecture getter
static JSValue js_process_arch_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_NewString(ctx, jsrt_get_arch());
}

// Process versions getter
static JSValue js_process_versions_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue versions = JS_NewObject(ctx);

  // Add jsrt version
  const char* jsrt_version = jsrt_get_version();
  if (jsrt_version) {
    JS_SetPropertyStr(ctx, versions, "jsrt", JS_NewString(ctx, jsrt_version));
  }

  // Add QuickJS version (if available)
  JS_SetPropertyStr(ctx, versions, "quickjs", JS_NewString(ctx, "2024-01-13"));

  // Add Node.js compatible version for compatibility
  JS_SetPropertyStr(ctx, versions, "node", JS_NewString(ctx, "20.0.0"));

  // Add V8 version for compatibility
  JS_SetPropertyStr(ctx, versions, "v8", JS_NewString(ctx, "11.3.244.8"));

  return versions;
}

// Process environment getter
static JSValue js_process_env_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue env = JS_NewObject(ctx);

  // Get environment variables
  extern char** environ;
  if (environ) {
    for (char** envp = environ; *envp; envp++) {
      char* env_str = *envp;
      char* eq = strchr(env_str, '=');
      if (eq) {
        size_t key_len = eq - env_str;
        char* key = malloc(key_len + 1);
        if (key) {
          memcpy(key, env_str, key_len);
          key[key_len] = '\0';
          JS_SetPropertyStr(ctx, env, key, JS_NewString(ctx, eq + 1));
          free(key);
        }
      }
    }
  }

  return env;
}

// Process exit function
static JSValue js_process_exit(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  int code = 0;
  if (argc > 0) {
    if (JS_ToInt32(ctx, &code, argv[0]) < 0) {
      code = 1;
    }
  }
  exit(code);
  return JS_UNDEFINED;  // Never reached
}

// Initialize process info properties
void jsrt_process_info_init(JSContext* ctx, JSValue process_obj) {
  // Define property getters
  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "pid"),
                          JS_NewCFunction(ctx, js_process_pid_get, "get pid", 0), JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "ppid"),
                          JS_NewCFunction(ctx, js_process_ppid_get, "get ppid", 0), JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "argv"),
                          JS_NewCFunction(ctx, js_process_argv_get, "get argv", 0), JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "argv0"),
                          JS_NewCFunction(ctx, js_process_argv0_get, "get argv0", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "version"),
                          JS_NewCFunction(ctx, js_process_version_get, "get version", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "platform"),
                          JS_NewCFunction(ctx, js_process_platform_get, "get platform", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "arch"),
                          JS_NewCFunction(ctx, js_process_arch_get, "get arch", 0), JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "versions"),
                          JS_NewCFunction(ctx, js_process_versions_get, "get versions", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  JS_DefinePropertyGetSet(ctx, process_obj, JS_NewAtom(ctx, "env"),
                          JS_NewCFunction(ctx, js_process_env_get, "get env", 0), JS_UNDEFINED, JS_PROP_CONFIGURABLE);

  // Add exit method
  JS_SetPropertyStr(ctx, process_obj, "exit", JS_NewCFunction(ctx, js_process_exit, "exit", 1));
}