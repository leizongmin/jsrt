#include <limits.h>  // For PATH_MAX
#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>  // For malloc/free
#include <string.h>
#include <unistd.h>  // For getcwd fallback
#include "../module/resolver/npm_resolver.h"
#include "../module/resolver/path_util.h"
#include "../runtime.h"
#include "node_modules.h"
#include "process/process.h"

// Basic implementation of resolve.sync() function
// This provides the essential functionality needed by babel-eslint and other tools
static JSValue js_resolve_sync(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  const char* id = NULL;
  JSValue options_obj = JS_UNDEFINED;

  if (argc >= 1) {
    id = JS_ToCString(ctx, argv[0]);
    if (!id) {
      return JS_ThrowTypeError(ctx, "resolve: id must be a string");
    }
  }

  if (argc >= 2) {
    options_obj = argv[1];
  }

  if (!id) {
    return JS_ThrowTypeError(ctx, "resolve: id parameter is required");
  }

  // Get basedir from options if provided
  char* basedir = NULL;
  char* basedir_to_free = NULL;
  if (!JS_IsUndefined(options_obj)) {
    JSValue basedir_val = JS_GetPropertyStr(ctx, options_obj, "basedir");
    if (!JS_IsUndefined(basedir_val)) {
      basedir = (char*)JS_ToCString(ctx, basedir_val);
    }
    JS_FreeValue(ctx, basedir_val);
  }

  // If no basedir provided, use current working directory
  if (!basedir) {
    char cwd_buf[PATH_MAX];
    if (jsrt_process_getcwd(cwd_buf, sizeof(cwd_buf))) {
      basedir = cwd_buf;
    } else {
      basedir = ".";
    }
  } else {
    // We got basedir from JS_ToCString, so we need to free it later
    basedir_to_free = basedir;
  }

  JSRT_Debug("resolve.sync: id='%s', basedir='%s'", id, basedir);

  char* resolved_path = NULL;

  // Handle different resolution strategies
  if (strstr(id, "/") || strcmp(id, ".") == 0 || strcmp(id, "..") == 0) {
    // Relative or absolute path
    if (id[0] == '/') {
      resolved_path = strdup(id);
    } else {
      resolved_path = jsrt_path_join(basedir, id);
    }
  } else if (strstr(id, ".js") || strstr(id, ".json") || strstr(id, ".node")) {
    // File with extension
    if (id[0] == '/') {
      resolved_path = strdup(id);
    } else {
      resolved_path = jsrt_path_join(basedir, id);
    }
  } else {
    // Bare module specifier - use npm resolver
    resolved_path = jsrt_find_node_modules(basedir, id);

    if (!resolved_path) {
      // Try resolving as a relative path
      char* try_path = jsrt_path_join(basedir, id);
      if (try_path) {
        // Check if this looks like it could be a file or directory
        resolved_path = try_path;
      }
    }

    if (!resolved_path) {
      // Try Node.js builtin modules
      if (JSRT_IsNodeModule(id)) {
        // Return a special path for builtin modules
        char builtin_path[512];
        snprintf(builtin_path, sizeof(builtin_path), "node:%s", id);
        JS_FreeCString(ctx, id);
        if (basedir_to_free) {
          JS_FreeCString(ctx, basedir_to_free);
        }
        return JS_NewString(ctx, builtin_path);
      }
    }
  }

  if (!resolved_path) {
    JS_FreeCString(ctx, id);
    if (basedir_to_free) {
      JS_FreeCString(ctx, basedir_to_free);
    }
    return JS_ThrowReferenceError(ctx, "Cannot resolve module '%s'", id);
  }

  JS_FreeCString(ctx, id);
  if (basedir_to_free) {
    JS_FreeCString(ctx, basedir_to_free);
  }

  JSRT_Debug("resolve.sync: resolved to '%s'", resolved_path);

  JSValue result = JS_NewString(ctx, resolved_path);
  free(resolved_path);
  return result;
}

// isCore function - check if module is a Node.js core module
static JSValue js_is_core(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "isCore: module name required");
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_ThrowTypeError(ctx, "isCore: module name must be a string");
  }

  bool is_core = JSRT_IsNodeModule(name);

  JS_FreeCString(ctx, name);
  return JS_NewBool(ctx, is_core);
}

// Main resolve module initialization
JSValue JSRT_InitResolve(JSContext* ctx) {
  JSValue resolve_obj = JS_NewObject(ctx);

  // Add sync function
  JSValue sync_func = JS_NewCFunction(ctx, js_resolve_sync, "sync", 2);
  JS_SetPropertyStr(ctx, resolve_obj, "sync", sync_func);

  // Add isCore function
  JSValue is_core_func = JS_NewCFunction(ctx, js_is_core, "isCore", 1);
  JS_SetPropertyStr(ctx, resolve_obj, "isCore", is_core_func);

  JSRT_Debug("resolve module initialized with sync() and isCore() methods");

  return resolve_obj;
}

// ESM module initialization
int js_resolve_init(JSContext* ctx, JSModuleDef* m) {
  JSValue resolve_obj = JSRT_InitResolve(ctx);

  // Set exports
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, resolve_obj));
  JS_SetModuleExport(ctx, m, "sync", JS_GetPropertyStr(ctx, resolve_obj, "sync"));
  JS_SetModuleExport(ctx, m, "isCore", JS_GetPropertyStr(ctx, resolve_obj, "isCore"));

  JS_FreeValue(ctx, resolve_obj);
  return 0;
}