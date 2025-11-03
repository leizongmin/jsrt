#include "module_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../module/core/module_cache.h"
#include "../../module/core/module_context.h"
#include "../../module/core/module_loader.h"
#include "../../module/loaders/commonjs_loader.h"
#include "../../module/resolver/path_resolver.h"
#include "../../module/resolver/path_util.h"
#include "../../runtime.h"
#include "../../util/debug.h"
#include "../../util/file.h"
#include "../node_modules.h"
#include "compile_cache.h"
#include "hooks.h"
#include "sourcemap.h"

// Module class ID for opaque data
static JSClassID jsrt_module_class_id;

// Forward declarations
static void jsrt_module_finalizer(JSRuntime* rt, JSValue val);
static JSClassDef jsrt_module_class = {
    .class_name = "Module",
    .finalizer = jsrt_module_finalizer,
};

// Helper function to duplicate a string
static char* jsrt_strdup(const char* str) {
  if (!str)
    return NULL;
  size_t len = strlen(str);
  char* dup = malloc(len + 1);
  if (dup) {
    memcpy(dup, str, len + 1);
  }
  return dup;
}

static JSRT_CompileCacheConfig* jsrt_module_get_compile_cache(JSContext* ctx) {
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt) {
    return NULL;
  }
  return rt->compile_cache;
}

static const char* jsrt_compile_cache_status_message(JSRT_CompileCacheStatus status) {
  switch (status) {
    case JSRT_COMPILE_CACHE_ENABLED:
      return "enabled";
    case JSRT_COMPILE_CACHE_ALREADY_ENABLED:
      return "already_enabled";
    case JSRT_COMPILE_CACHE_FAILED:
      return "failed";
    case JSRT_COMPILE_CACHE_DISABLED:
      return "disabled";
    default:
      return "unknown";
  }
}

static JSValue jsrt_module_build_compile_cache_result(JSContext* ctx, JSRT_CompileCacheStatus status,
                                                      JSRT_CompileCacheConfig* config) {
  JSValue result = JS_NewObject(ctx);
  if (JS_IsException(result)) {
    return result;
  }

  if (JS_SetPropertyStr(ctx, result, "status", JS_NewInt32(ctx, status)) < 0) {
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
  }

  JSValue message = JS_NewString(ctx, jsrt_compile_cache_status_message(status));
  if (JS_IsException(message)) {
    JS_FreeValue(ctx, result);
    return message;
  }
  if (JS_SetPropertyStr(ctx, result, "message", message) < 0) {
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
  }

  if (config && jsrt_compile_cache_is_enabled(config)) {
    const char* directory = jsrt_compile_cache_get_directory(config);
    if (directory) {
      if (JS_SetPropertyStr(ctx, result, "directory", JS_NewString(ctx, directory)) < 0) {
        JS_FreeValue(ctx, result);
        return JS_EXCEPTION;
      }
    }
    if (JS_SetPropertyStr(ctx, result, "portable", JS_NewBool(ctx, config->portable)) < 0) {
      JS_FreeValue(ctx, result);
      return JS_EXCEPTION;
    }
  }

  return result;
}

/**
 * Module class finalizer - cleanup module data
 */
static void jsrt_module_finalizer(JSRuntime* rt, JSValue val) {
  JSRT_ModuleData* data = JS_GetOpaque(val, jsrt_module_class_id);
  if (data) {
    jsrt_module_free_data(rt, data);
  }
}

/**
 * Get module data from Module instance
 */
JSRT_ModuleData* jsrt_module_get_data(JSContext* ctx, JSValueConst obj) {
  return JS_GetOpaque(obj, jsrt_module_class_id);
}

/**
 * Free module data
 */
void jsrt_module_free_data(JSRuntime* rt, JSRT_ModuleData* data) {
  if (!data)
    return;

  JS_FreeValueRT(rt, data->exports);
  JS_FreeValueRT(rt, data->require);
  JS_FreeValueRT(rt, data->parent);
  JS_FreeValueRT(rt, data->children);
  JS_FreeValueRT(rt, data->paths);

  free(data->id);
  free(data->filename);
  free(data->path);
  free(data);
}

/**
 * module.registerHooks(options) - Register module resolution and loading hooks
 */
JSValue jsrt_module_register_hooks(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "module.registerHooks() requires an options object");
  }

  if (!JS_IsObject(argv[0])) {
    return JS_ThrowTypeError(ctx, "module.registerHooks() options must be an object");
  }

  // Get runtime to access hook registry
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->hook_registry) {
    return JS_ThrowInternalError(ctx, "Module hook registry not initialized");
  }

  JSValue options = argv[0];
  JSValue resolve_fn = JS_NULL;
  JSValue load_fn = JS_NULL;

  // Extract resolve function
  JSValue resolve_val = JS_GetPropertyStr(ctx, options, "resolve");
  if (JS_IsException(resolve_val)) {
    return JS_EXCEPTION;
  }
  if (!JS_IsUndefined(resolve_val) && !JS_IsNull(resolve_val)) {
    if (!JS_IsFunction(ctx, resolve_val)) {
      JS_FreeValue(ctx, resolve_val);
      return JS_ThrowTypeError(ctx, "module.registerHooks() resolve option must be a function");
    }
    resolve_fn = resolve_val;
  } else {
    JS_FreeValue(ctx, resolve_val);
  }

  // Extract load function
  JSValue load_val = JS_GetPropertyStr(ctx, options, "load");
  if (JS_IsException(load_val)) {
    if (!JS_IsNull(resolve_fn)) {
      JS_FreeValue(ctx, resolve_fn);
    }
    return JS_EXCEPTION;
  }
  if (!JS_IsUndefined(load_val) && !JS_IsNull(load_val)) {
    if (!JS_IsFunction(ctx, load_val)) {
      if (!JS_IsNull(resolve_fn)) {
        JS_FreeValue(ctx, resolve_fn);
      }
      JS_FreeValue(ctx, load_val);
      return JS_ThrowTypeError(ctx, "module.registerHooks() load option must be a function");
    }
    load_fn = load_val;
  } else {
    JS_FreeValue(ctx, load_val);
  }

  // Validate that at least one function is provided
  if (JS_IsNull(resolve_fn) && JS_IsNull(load_fn)) {
    return JS_ThrowTypeError(ctx, "module.registerHooks() requires at least resolve or load function");
  }

  // Register the hook
  int result = jsrt_hook_register(rt->hook_registry, resolve_fn, load_fn);

  // Clean up local references (hook registry takes ownership)
  if (!JS_IsNull(resolve_fn)) {
    JS_FreeValue(ctx, resolve_fn);
  }
  if (!JS_IsNull(load_fn)) {
    JS_FreeValue(ctx, load_fn);
  }

  if (result != 0) {
    return JS_ThrowInternalError(ctx, "Failed to register module hooks");
  }

  // Return a handle/identifier for the registered hooks
  // For now, we return a simple handle object with hook count
  JSValue handle = JS_NewObject(ctx);
  if (JS_IsException(handle)) {
    return JS_EXCEPTION;
  }

  // Add metadata to the handle
  JS_SetPropertyStr(ctx, handle, "id", JS_NewInt32(ctx, rt->hook_registry->hook_count));
  JS_SetPropertyStr(ctx, handle, "resolve", JS_IsNull(resolve_fn) ? JS_FALSE : JS_TRUE);
  JS_SetPropertyStr(ctx, handle, "load", JS_IsNull(load_fn) ? JS_FALSE : JS_TRUE);

  return handle;
}

/**
 * module.builtinModules - Array of built-in module names
 */
JSValue jsrt_module_builtinModules(JSContext* ctx) {
  JSValue arr = JS_NewArray(ctx);
  if (JS_IsException(arr))
    return arr;

  int count = JSRT_GetNodeModuleCount();
  int arr_index = 0;

  // Add both unprefixed and node: prefixed forms
  for (int i = 0; i < count; i++) {
    const char* name = JSRT_GetNodeModuleName(i);
    if (!name)
      continue;

    // Add unprefixed name (e.g., "fs")
    JSValue name_val = JS_NewString(ctx, name);
    JS_SetPropertyUint32(ctx, arr, arr_index++, name_val);

    // Add node: prefixed name (e.g., "node:fs")
    size_t prefixed_len = strlen("node:") + strlen(name) + 1;
    char* prefixed = malloc(prefixed_len);
    if (prefixed) {
      snprintf(prefixed, prefixed_len, "node:%s", name);
      JSValue prefixed_val = JS_NewString(ctx, prefixed);
      JS_SetPropertyUint32(ctx, arr, arr_index++, prefixed_val);
      free(prefixed);
    }
  }

  // Freeze the array to make it read-only
  JSValue freeze_fn = JS_GetPropertyStr(ctx, JS_GetGlobalObject(ctx), "Object");
  if (!JS_IsException(freeze_fn)) {
    JSValue freeze_method = JS_GetPropertyStr(ctx, freeze_fn, "freeze");
    if (!JS_IsException(freeze_method)) {
      JS_Call(ctx, freeze_method, JS_UNDEFINED, 1, &arr);
      JS_FreeValue(ctx, freeze_method);
    }
    JS_FreeValue(ctx, freeze_fn);
  }

  return arr;
}

/**
 * module.isBuiltin(moduleName) - Check if module is built-in
 */
JSValue jsrt_module_isBuiltin(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing module name argument");
  }

  if (!JS_IsString(argv[0])) {
    return JS_FALSE;
  }

  const char* module_name = JS_ToCString(ctx, argv[0]);
  if (!module_name) {
    return JS_FALSE;
  }

  // Strip "node:" prefix if present
  const char* name_to_check = module_name;
  if (strncmp(module_name, "node:", 5) == 0) {
    name_to_check = module_name + 5;
  }

  // Check if it's a built-in module
  bool is_builtin = JSRT_IsNodeModule(name_to_check);

  JS_FreeCString(ctx, module_name);

  return JS_NewBool(ctx, is_builtin);
}

/**
 * module.createRequire(filename) - Create require function for ESM
 */
JSValue jsrt_module_createRequire(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing filename argument");
  }

  // Get filename argument (can be string path or file:// URL)
  const char* filename_str = JS_ToCString(ctx, argv[0]);
  if (!filename_str) {
    return JS_EXCEPTION;
  }

  // Handle file:// URLs by stripping the protocol
  const char* path_str = filename_str;
  if (strncmp(filename_str, "file://", 7) == 0) {
    path_str = filename_str + 7;
// On Windows, file:///C:/... should become C:/...
#ifdef _WIN32
    if (path_str[0] == '/' && path_str[2] == ':') {
      path_str++;  // Skip leading /
    }
#endif
  }

  // Get runtime to access module loader
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->module_loader) {
    JS_FreeCString(ctx, filename_str);
    return JS_ThrowInternalError(ctx, "Module loader not initialized");
  }

  // Create require function bound to the given path
  // Note: We use the existing jsrt_create_require_function which already handles this
  JSValue require_fn = jsrt_create_require_function(ctx, rt->module_loader, path_str);

  JS_FreeCString(ctx, filename_str);

  if (JS_IsException(require_fn)) {
    return require_fn;
  }

  // Add require.resolve, require.cache, require.extensions, require.main properties
  // These are expected to be present on the require function

  // require.resolve - use Module._resolveFilename
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue module_ns = JS_GetPropertyStr(ctx, global, "module");
  JSValue module_ctor = JS_GetPropertyStr(ctx, module_ns, "Module");

  if (!JS_IsUndefined(module_ctor)) {
    JSValue resolve_fn = JS_GetPropertyStr(ctx, module_ctor, "_resolveFilename");
    if (!JS_IsUndefined(resolve_fn)) {
      JS_SetPropertyStr(ctx, require_fn, "resolve", resolve_fn);
    } else {
      JS_FreeValue(ctx, resolve_fn);
    }

    // require.cache - reference to Module._cache
    JSValue cache = JS_GetPropertyStr(ctx, module_ctor, "_cache");
    if (!JS_IsUndefined(cache)) {
      JS_SetPropertyStr(ctx, require_fn, "cache", cache);
    } else {
      JS_FreeValue(ctx, cache);
    }

    // require.extensions - reference to Module._extensions
    JSValue extensions = JS_GetPropertyStr(ctx, module_ctor, "_extensions");
    if (!JS_IsUndefined(extensions)) {
      JS_SetPropertyStr(ctx, require_fn, "extensions", extensions);
    } else {
      JS_FreeValue(ctx, extensions);
    }

    // require.main - undefined for now (set when main module runs)
    JS_SetPropertyStr(ctx, require_fn, "main", JS_UNDEFINED);
  }

  JS_FreeValue(ctx, module_ctor);
  JS_FreeValue(ctx, module_ns);
  JS_FreeValue(ctx, global);

  return require_fn;
}

/**
 * module.syncBuiltinESMExports() - Sync CommonJS/ESM exports
 */
JSValue jsrt_module_syncBuiltinESMExports(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("module.syncBuiltinESMExports() called");

  // Get runtime to access module loader
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->module_loader) {
    JSRT_Debug("No runtime or module loader available");
    return JS_UNDEFINED;
  }

  // Iterate through all builtin modules and sync their exports
  // For now, we'll focus on node: modules that have both CJS and ESM versions

  // Get the global object to access require/imported modules
  JSValue global = JS_GetGlobalObject(ctx);

  // List of builtin modules that may need syncing
  const char* builtin_modules[] = {"fs",   "path",  "os",     "util", "events", "buffer",      "stream", "net",
                                   "http", "https", "crypto", "zlib", "url",    "querystring", NULL};

  int synced_count = 0;

  for (int i = 0; builtin_modules[i]; i++) {
    const char* module_name = builtin_modules[i];

    // Try to get CommonJS version (require)
    char cjs_require_code[256];
    snprintf(cjs_require_code, sizeof(cjs_require_code),
             "(function() { try { return require('%s'); } catch(e) { return undefined; } })()", module_name);

    JSValue cjs_module =
        JS_Eval(ctx, cjs_require_code, strlen(cjs_require_code), "<sync_builtin_esm>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(cjs_module)) {
      JS_FreeValue(ctx, cjs_module);
      continue;  // Skip modules that don't exist in CJS
    }

    // Try to get ESM version (import)
    char esm_import_code[256];
    snprintf(
        esm_import_code, sizeof(esm_import_code),
        "(function() { try { return globalThis.node && globalThis.node['%s']; } catch(e) { return undefined; } })()",
        module_name);

    JSValue esm_module =
        JS_Eval(ctx, esm_import_code, strlen(esm_import_code), "<sync_builtin_esm>", JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(esm_module)) {
      JS_FreeValue(ctx, cjs_module);
      JS_FreeValue(ctx, esm_module);
      continue;  // Skip modules that don't exist in ESM
    }

    // If both exist and are objects, sync properties from CJS to ESM
    if (!JS_IsUndefined(cjs_module) && !JS_IsUndefined(esm_module) && JS_IsObject(cjs_module) &&
        JS_IsObject(esm_module)) {
      // Get property names of CJS module
      JSPropertyEnum* props;
      uint32_t prop_count;

      int ret = JS_GetOwnPropertyNames(ctx, &props, &prop_count, cjs_module, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);

      if (ret >= 0) {
        int synced_props = 0;

        // Copy each property from CJS to ESM
        for (uint32_t j = 0; j < prop_count; j++) {
          JSValue prop_name = JS_AtomToString(ctx, props[j].atom);

          if (!JS_IsException(prop_name)) {
            const char* prop_str = JS_ToCString(ctx, prop_name);

            if (prop_str) {
              // Skip properties that already exist in ESM module
              JSValue existing_prop = JS_GetPropertyStr(ctx, esm_module, prop_str);
              bool has_existing = !JS_IsUndefined(existing_prop) && !JS_IsException(existing_prop);
              JS_FreeValue(ctx, existing_prop);

              if (!has_existing) {
                // Copy property from CJS to ESM
                JSValue prop_value = JS_GetPropertyStr(ctx, cjs_module, prop_str);

                if (!JS_IsException(prop_value)) {
                  if (JS_SetPropertyStr(ctx, esm_module, prop_str, JS_DupValue(ctx, prop_value)) >= 0) {
                    synced_props++;
                  }
                  JS_FreeValue(ctx, prop_value);
                }
              }

              JS_FreeCString(ctx, prop_str);
            }

            JS_FreeValue(ctx, prop_name);
          }
        }

        // Free property enumeration
        for (uint32_t j = 0; j < prop_count; j++) {
          JS_FreeAtom(ctx, props[j].atom);
        }
        js_free_rt(JS_GetRuntime(ctx), props);

        if (synced_props > 0) {
          JSRT_Debug("Synced %d properties from CommonJS to ESM for module '%s'", synced_props, module_name);
          synced_count++;
        }
      }
    }

    JS_FreeValue(ctx, cjs_module);
    JS_FreeValue(ctx, esm_module);
  }

  JS_FreeValue(ctx, global);

  JSRT_Debug("module.syncBuiltinESMExports() completed, synced %d modules", synced_count);
  return JS_UNDEFINED;
}

/**
 * module.findSourceMap(path) - Find source map for file
 */
JSValue jsrt_module_find_source_map(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing path argument");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Get runtime to access source map cache
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->source_map_cache) {
    JS_FreeCString(ctx, path);
    return JS_UNDEFINED;
  }

  // Find source map (returns SourceMap instance or undefined)
  JSValue result = jsrt_find_source_map(ctx, rt->source_map_cache, path);
  JS_FreeCString(ctx, path);

  return result;
}

/**
 * module.getSourceMapsSupport() - Get source map configuration
 */
JSValue jsrt_module_get_source_maps_support(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Get runtime to access source map cache
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->source_map_cache) {
    // Return default configuration if cache not available
    JSValue result = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, result, "enabled", JS_FALSE);
    JS_SetPropertyStr(ctx, result, "nodeModules", JS_FALSE);
    JS_SetPropertyStr(ctx, result, "generatedCode", JS_FALSE);
    return result;
  }

  // Get current configuration
  bool enabled, node_modules, generated_code;
  jsrt_source_map_cache_get_config(rt->source_map_cache, &enabled, &node_modules, &generated_code);

  // Build result object
  JSValue result = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, result, "enabled", JS_NewBool(ctx, enabled));
  JS_SetPropertyStr(ctx, result, "nodeModules", JS_NewBool(ctx, node_modules));
  JS_SetPropertyStr(ctx, result, "generatedCode", JS_NewBool(ctx, generated_code));

  return result;
}

/**
 * module.setSourceMapsSupport(enabled[, options]) - Set source map configuration
 */
JSValue jsrt_module_set_source_maps_support(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing enabled argument");
  }

  // Get runtime to access source map cache
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->source_map_cache) {
    return JS_UNDEFINED;
  }

  // Get enabled flag
  bool enabled = JS_ToBool(ctx, argv[0]);

  // Get optional configuration
  bool node_modules = false;
  bool generated_code = false;

  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue options = argv[1];

    // Get nodeModules flag
    JSValue node_modules_val = JS_GetPropertyStr(ctx, options, "nodeModules");
    if (JS_IsBool(node_modules_val)) {
      node_modules = JS_ToBool(ctx, node_modules_val);
    }
    JS_FreeValue(ctx, node_modules_val);

    // Get generatedCode flag
    JSValue generated_code_val = JS_GetPropertyStr(ctx, options, "generatedCode");
    if (JS_IsBool(generated_code_val)) {
      generated_code = JS_ToBool(ctx, generated_code_val);
    }
    JS_FreeValue(ctx, generated_code_val);
  }

  // Set configuration
  jsrt_source_map_cache_set_config(rt->source_map_cache, enabled, node_modules, generated_code);

  return JS_UNDEFINED;
}

/**
 * Property getters/setters
 */

// module.id getter
static JSValue jsrt_module_get_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_ModuleData* data = jsrt_module_get_data(ctx, this_val);
  if (!data)
    return JS_UNDEFINED;
  return JS_NewString(ctx, data->id ? data->id : "");
}

// module.id setter
static JSValue jsrt_module_set_id(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1)
    return JS_UNDEFINED;

  JSRT_ModuleData* data = jsrt_module_get_data(ctx, this_val);
  if (!data)
    return JS_UNDEFINED;

  const char* new_id = JS_ToCString(ctx, argv[0]);
  if (new_id) {
    free(data->id);
    data->id = strdup(new_id);
    JS_FreeCString(ctx, new_id);
  }
  return JS_UNDEFINED;
}

// module.filename getter
static JSValue jsrt_module_get_filename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_ModuleData* data = jsrt_module_get_data(ctx, this_val);
  if (!data)
    return JS_UNDEFINED;
  return data->filename ? JS_NewString(ctx, data->filename) : JS_UNDEFINED;
}

// module.filename setter
static JSValue jsrt_module_set_filename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1)
    return JS_UNDEFINED;

  JSRT_ModuleData* data = jsrt_module_get_data(ctx, this_val);
  if (!data)
    return JS_UNDEFINED;

  const char* new_filename = JS_ToCString(ctx, argv[0]);
  if (new_filename) {
    free(data->filename);
    data->filename = strdup(new_filename);
    JS_FreeCString(ctx, new_filename);

    // Update path property (directory name)
    free(data->path);
    data->path = NULL;

    // Extract directory from filename
    const char* last_sep = strrchr(data->filename, '/');
    if (!last_sep) {
      last_sep = strrchr(data->filename, '\\');
    }
    if (last_sep) {
      size_t len = last_sep - data->filename;
      data->path = malloc(len + 1);
      if (data->path) {
        memcpy(data->path, data->filename, len);
        data->path[len] = '\0';
      }
    }
  }
  return JS_UNDEFINED;
}

// module.path getter
static JSValue jsrt_module_get_path(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_ModuleData* data = jsrt_module_get_data(ctx, this_val);
  if (!data)
    return JS_UNDEFINED;
  return data->path ? JS_NewString(ctx, data->path) : JS_UNDEFINED;
}

// module.loaded getter
static JSValue jsrt_module_get_loaded(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_ModuleData* data = jsrt_module_get_data(ctx, this_val);
  if (!data)
    return JS_UNDEFINED;
  return JS_NewBool(ctx, data->loaded);
}

// module.loaded setter
static JSValue jsrt_module_set_loaded(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1)
    return JS_UNDEFINED;

  JSRT_ModuleData* data = jsrt_module_get_data(ctx, this_val);
  if (!data)
    return JS_UNDEFINED;
  data->loaded = JS_ToBool(ctx, argv[0]);
  return JS_UNDEFINED;
}

/**
 * Module constructor - new Module(id, parent)
 */
JSValue jsrt_module_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue proto, obj;

  // Get the prototype from new.target or the constructor
  if (!JS_IsUndefined(new_target)) {
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
      return JS_EXCEPTION;
  } else {
    proto = JS_UNDEFINED;
  }

  obj = JS_NewObjectProtoClass(ctx, proto, jsrt_module_class_id);
  JS_FreeValue(ctx, proto);
  if (JS_IsException(obj))
    return obj;

  // Allocate module data
  JSRT_ModuleData* data = calloc(1, sizeof(JSRT_ModuleData));
  if (!data) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize module data
  data->exports = JS_NewObject(ctx);
  data->require = JS_UNDEFINED;
  data->parent = JS_UNDEFINED;
  data->children = JS_NewArray(ctx);
  data->paths = JS_NewArray(ctx);
  data->loaded = false;
  data->id = NULL;
  data->filename = NULL;
  data->path = NULL;

  // Get id from arguments
  if (argc > 0 && JS_IsString(argv[0])) {
    const char* id = JS_ToCString(ctx, argv[0]);
    if (id) {
      data->id = strdup(id);
      JS_FreeCString(ctx, id);
    }
  }

  // Get parent from arguments
  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    data->parent = JS_DupValue(ctx, argv[1]);
  }

  JS_SetOpaque(obj, data);

  // Define properties with getters/setters for proper synchronization
  JSValue getter, setter;

  // id property (getter/setter)
  getter = JS_NewCFunction2(ctx, jsrt_module_get_id, "get id", 0, JS_CFUNC_generic, 0);
  setter = JS_NewCFunction2(ctx, jsrt_module_set_id, "set id", 1, JS_CFUNC_generic, 0);
  JS_DefineProperty(ctx, obj, JS_NewAtom(ctx, "id"), JS_UNDEFINED, getter, setter,
                    JS_PROP_C_W_E | JS_PROP_HAS_GET | JS_PROP_HAS_SET);

  // filename property (getter/setter)
  getter = JS_NewCFunction2(ctx, jsrt_module_get_filename, "get filename", 0, JS_CFUNC_generic, 0);
  setter = JS_NewCFunction2(ctx, jsrt_module_set_filename, "set filename", 1, JS_CFUNC_generic, 0);
  JS_DefineProperty(ctx, obj, JS_NewAtom(ctx, "filename"), JS_UNDEFINED, getter, setter,
                    JS_PROP_C_W_E | JS_PROP_HAS_GET | JS_PROP_HAS_SET);

  // path property (getter only - auto-extracted from filename)
  getter = JS_NewCFunction2(ctx, jsrt_module_get_path, "get path", 0, JS_CFUNC_generic, 0);
  JS_DefineProperty(ctx, obj, JS_NewAtom(ctx, "path"), JS_UNDEFINED, getter, JS_UNDEFINED,
                    JS_PROP_C_W_E | JS_PROP_HAS_GET);

  // loaded property (getter/setter)
  getter = JS_NewCFunction2(ctx, jsrt_module_get_loaded, "get loaded", 0, JS_CFUNC_generic, 0);
  setter = JS_NewCFunction2(ctx, jsrt_module_set_loaded, "set loaded", 1, JS_CFUNC_generic, 0);
  JS_DefineProperty(ctx, obj, JS_NewAtom(ctx, "loaded"), JS_UNDEFINED, getter, setter,
                    JS_PROP_C_W_E | JS_PROP_HAS_GET | JS_PROP_HAS_SET);

  // exports, parent, children, paths - direct values
  JS_DefinePropertyValueStr(ctx, obj, "exports", JS_DupValue(ctx, data->exports), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, obj, "parent", JS_DupValue(ctx, data->parent), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, obj, "children", JS_DupValue(ctx, data->children), JS_PROP_C_W_E);
  JS_DefinePropertyValueStr(ctx, obj, "paths", JS_DupValue(ctx, data->paths), JS_PROP_C_W_E);
  // Note: require and _compile are inherited from prototype, not set on instance

  return obj;
}

/**
 * Helper: Add module to Module._cache
 * Called by Module._load after successfully loading a module
 */
static int jsrt_module_cache_add_entry(JSContext* ctx, const char* filename, JSValue module) {
  // Get Module constructor from global scope
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue module_ns = JS_GetPropertyStr(ctx, global, "module");
  JSValue module_ctor = JS_GetPropertyStr(ctx, module_ns, "Module");

  if (JS_IsUndefined(module_ctor)) {
    JS_FreeValue(ctx, module_ns);
    JS_FreeValue(ctx, global);
    return -1;
  }

  // Get Module._cache
  JSValue cache = JS_GetPropertyStr(ctx, module_ctor, "_cache");

  if (!JS_IsUndefined(cache) && !JS_IsNull(cache)) {
    // Add module to cache with filename as key
    JS_SetPropertyStr(ctx, cache, filename, JS_DupValue(ctx, module));
  }

  JS_FreeValue(ctx, cache);
  JS_FreeValue(ctx, module_ctor);
  JS_FreeValue(ctx, module_ns);
  JS_FreeValue(ctx, global);

  return 0;
}

/**
 * Helper: Remove module from Module._cache
 */
static int jsrt_module_cache_remove_entry(JSContext* ctx, const char* filename) {
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue module_ns = JS_GetPropertyStr(ctx, global, "module");
  JSValue module_ctor = JS_GetPropertyStr(ctx, module_ns, "Module");

  if (JS_IsUndefined(module_ctor)) {
    JS_FreeValue(ctx, module_ns);
    JS_FreeValue(ctx, global);
    return -1;
  }

  JSValue cache = JS_GetPropertyStr(ctx, module_ctor, "_cache");

  if (!JS_IsUndefined(cache) && !JS_IsNull(cache)) {
    JSAtom atom = JS_NewAtom(ctx, filename);
    JS_DeleteProperty(ctx, cache, atom, 0);
    JS_FreeAtom(ctx, atom);
  }

  JS_FreeValue(ctx, cache);
  JS_FreeValue(ctx, module_ctor);
  JS_FreeValue(ctx, module_ns);
  JS_FreeValue(ctx, global);

  return 0;
}

/**
 * Module._load(request, parent, isMain) - Load module
 */
JSValue jsrt_module_load(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing request argument");
  }

  const char* request = JS_ToCString(ctx, argv[0]);
  if (!request) {
    return JS_EXCEPTION;
  }

  // Get parent module (argv[1])
  JSValue parent = (argc > 1) ? argv[1] : JS_UNDEFINED;

  // Get isMain flag (argv[2])
  bool is_main = (argc > 2) ? JS_ToBool(ctx, argv[2]) : false;

  // Step 1: Resolve filename using Module._resolveFilename
  JSValue resolved_args[2] = {argv[0], parent};
  JSValue resolved_filename = jsrt_module_resolve_filename(ctx, this_val, 2, resolved_args);

  if (JS_IsException(resolved_filename)) {
    JS_FreeCString(ctx, request);
    return JS_EXCEPTION;
  }

  const char* filename = JS_ToCString(ctx, resolved_filename);
  JS_FreeValue(ctx, resolved_filename);

  if (!filename) {
    JS_FreeCString(ctx, request);
    return JS_EXCEPTION;
  }

  // Step 2: Check Module._cache for existing module
  JSValue global = JS_GetGlobalObject(ctx);
  JSValue module_ns = JS_GetPropertyStr(ctx, global, "module");
  JSValue module_ctor = JS_GetPropertyStr(ctx, module_ns, "Module");
  JSValue cache = JS_GetPropertyStr(ctx, module_ctor, "_cache");

  JSValue cached_module = JS_GetPropertyStr(ctx, cache, filename);

  if (!JS_IsUndefined(cached_module) && !JS_IsNull(cached_module)) {
    // Module found in cache - return its exports
    JSValue exports = JS_GetPropertyStr(ctx, cached_module, "exports");
    JS_FreeValue(ctx, cached_module);
    JS_FreeValue(ctx, cache);
    JS_FreeValue(ctx, module_ctor);
    JS_FreeValue(ctx, module_ns);
    JS_FreeValue(ctx, global);
    JS_FreeCString(ctx, request);
    JS_FreeCString(ctx, filename);
    return exports;
  }

  JS_FreeValue(ctx, cached_module);

  // Step 3: Create new Module instance
  JSValue new_module_args[2] = {JS_NewString(ctx, filename), parent};
  JSValue new_module = JS_CallConstructor(ctx, module_ctor, 2, new_module_args);
  JS_FreeValue(ctx, new_module_args[0]);

  if (JS_IsException(new_module)) {
    JS_FreeValue(ctx, cache);
    JS_FreeValue(ctx, module_ctor);
    JS_FreeValue(ctx, module_ns);
    JS_FreeValue(ctx, global);
    JS_FreeCString(ctx, request);
    JS_FreeCString(ctx, filename);
    return JS_EXCEPTION;
  }

  // Set module.filename
  JS_SetPropertyStr(ctx, new_module, "filename", JS_NewString(ctx, filename));

  // Step 4: Add to cache BEFORE loading (for circular dependency support)
  JS_SetPropertyStr(ctx, cache, filename, JS_DupValue(ctx, new_module));

  // Step 5: Set require.main if this is the main module
  if (is_main) {
    JSValue require_prop = JS_GetPropertyStr(ctx, global, "require");
    if (!JS_IsUndefined(require_prop)) {
      JS_SetPropertyStr(ctx, require_prop, "main", JS_DupValue(ctx, new_module));
    }
    JS_FreeValue(ctx, require_prop);
  }

  // Step 6: Load the module using existing module loader
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->module_loader) {
    JS_FreeValue(ctx, cache);
    JS_FreeValue(ctx, module_ctor);
    JS_FreeValue(ctx, module_ns);
    JS_FreeValue(ctx, global);
    JS_FreeCString(ctx, request);
    JS_FreeCString(ctx, filename);
    JS_FreeValue(ctx, new_module);
    return JS_ThrowInternalError(ctx, "Module loader not initialized");
  }

  // For builtin modules, use the module loader directly
  if (JSRT_IsNodeModule(request) || strncmp(request, "node:", 5) == 0) {
    JSValue builtin_exports = jsrt_load_module(rt->module_loader, request, NULL);

    if (JS_IsException(builtin_exports)) {
      // Remove from cache on failure
      JSAtom atom = JS_NewAtom(ctx, filename);
      JS_DeleteProperty(ctx, cache, atom, 0);
      JS_FreeAtom(ctx, atom);
      JS_FreeValue(ctx, cache);
      JS_FreeValue(ctx, module_ctor);
      JS_FreeValue(ctx, module_ns);
      JS_FreeValue(ctx, global);
      JS_FreeCString(ctx, request);
      JS_FreeCString(ctx, filename);
      JS_FreeValue(ctx, new_module);
      return builtin_exports;
    }

    // Set module.exports to the builtin exports
    JS_SetPropertyStr(ctx, new_module, "exports", JS_DupValue(ctx, builtin_exports));

    // Set module.loaded = true
    JSRT_ModuleData* data = jsrt_module_get_data(ctx, new_module);
    if (data) {
      data->loaded = true;
    }

    JS_FreeValue(ctx, cache);
    JS_FreeValue(ctx, module_ctor);
    JS_FreeValue(ctx, module_ns);
    JS_FreeValue(ctx, global);
    JS_FreeCString(ctx, request);
    JS_FreeCString(ctx, filename);
    JS_FreeValue(ctx, new_module);

    return builtin_exports;
  }

  // For file modules, load using the module loader
  JSValue loaded_exports = jsrt_load_module(rt->module_loader, filename, NULL);

  if (JS_IsException(loaded_exports)) {
    // Remove from cache on failure
    JSAtom atom = JS_NewAtom(ctx, filename);
    JS_DeleteProperty(ctx, cache, atom, 0);
    JS_FreeAtom(ctx, atom);
    JS_FreeValue(ctx, cache);
    JS_FreeValue(ctx, module_ctor);
    JS_FreeValue(ctx, module_ns);
    JS_FreeValue(ctx, global);
    JS_FreeCString(ctx, request);
    JS_FreeCString(ctx, filename);
    JS_FreeValue(ctx, new_module);
    return loaded_exports;
  }

  // Update the cached module's exports
  JS_SetPropertyStr(ctx, new_module, "exports", JS_DupValue(ctx, loaded_exports));

  // Set module.loaded = true
  JSRT_ModuleData* data = jsrt_module_get_data(ctx, new_module);
  if (data) {
    data->loaded = true;
  }

  // Step 7: Return module.exports
  // Note: We return loaded_exports which is already a copy
  JS_FreeValue(ctx, cache);
  JS_FreeValue(ctx, module_ctor);
  JS_FreeValue(ctx, module_ns);
  JS_FreeValue(ctx, global);
  JS_FreeCString(ctx, request);
  JS_FreeCString(ctx, filename);
  JS_FreeValue(ctx, new_module);

  return loaded_exports;
}

/**
 * Module._resolveFilename(request, parent, isMain, options) - Resolve module path
 */
JSValue jsrt_module_resolve_filename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing request argument");
  }

  const char* request = JS_ToCString(ctx, argv[0]);
  if (!request) {
    return JS_EXCEPTION;
  }

  // Check if it's a builtin module (return as-is)
  if (JSRT_IsNodeModule(request)) {
    JSValue result = JS_NewString(ctx, request);
    JS_FreeCString(ctx, request);
    return result;
  }

  // Handle node: prefixed builtins
  if (strncmp(request, "node:", 5) == 0) {
    const char* name = request + 5;
    if (JSRT_IsNodeModule(name)) {
      JSValue result = JS_NewString(ctx, request);
      JS_FreeCString(ctx, request);
      return result;
    }
  }

  // Get base_path from parent module
  const char* base_path = NULL;
  char* base_path_allocated = NULL;

  if (argc > 1 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
    // Parent is a Module instance - get its filename
    JSValue parent_filename = JS_GetPropertyStr(ctx, argv[1], "filename");
    if (!JS_IsUndefined(parent_filename) && JS_IsString(parent_filename)) {
      const char* parent_filename_str = JS_ToCString(ctx, parent_filename);
      if (parent_filename_str) {
        // Extract directory from parent filename
        const char* last_sep = strrchr(parent_filename_str, '/');
        if (!last_sep) {
          last_sep = strrchr(parent_filename_str, '\\');
        }
        if (last_sep) {
          size_t len = last_sep - parent_filename_str;
          base_path_allocated = malloc(len + 1);
          if (base_path_allocated) {
            memcpy(base_path_allocated, parent_filename_str, len);
            base_path_allocated[len] = '\0';
            base_path = base_path_allocated;
          }
        }
        JS_FreeCString(ctx, parent_filename_str);
      }
    }
    JS_FreeValue(ctx, parent_filename);
  }

  // Get runtime to access module loader
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->module_loader) {
    JS_FreeCString(ctx, request);
    free(base_path_allocated);
    return JS_ThrowInternalError(ctx, "Module loader not initialized");
  }

  // Use path resolver to resolve the module path
  JSRT_ResolvedPath* resolved = jsrt_resolve_path(ctx, request, base_path, false);

  JS_FreeCString(ctx, request);
  free(base_path_allocated);

  if (!resolved) {
    // Throw MODULE_NOT_FOUND error
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "MODULE_NOT_FOUND"));
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Cannot find module"));
    return JS_Throw(ctx, error);
  }

  // Return resolved path
  JSValue result = JS_NewString(ctx, resolved->resolved_path);
  jsrt_resolved_path_free(resolved);

  return result;
}

/**
 * module.require(id) - Require a module
 */
JSValue jsrt_module_require(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // TODO: Implementation will call Module._load()
  return JS_UNDEFINED;
}

/**
 * module._compile(content, filename) - Compile module code
 */
JSValue jsrt_module_compile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Missing content or filename argument");
  }

  // Get content and filename
  const char* content = JS_ToCString(ctx, argv[0]);
  if (!content) {
    return JS_EXCEPTION;
  }

  const char* filename = JS_ToCString(ctx, argv[1]);
  if (!filename) {
    JS_FreeCString(ctx, content);
    return JS_EXCEPTION;
  }

  // Get module object (this)
  JSRT_ModuleData* data = jsrt_module_get_data(ctx, this_val);
  if (!data) {
    JS_FreeCString(ctx, content);
    JS_FreeCString(ctx, filename);
    return JS_ThrowTypeError(ctx, "module._compile must be called on a Module instance");
  }

  // Wrap the code with CommonJS wrapper manually
  // Build the wrapper: (function(exports, require, module, __filename, __dirname) { <code> })
  const char* prefix = "(function (exports, require, module, __filename, __dirname) { ";
  const char* suffix = "\n});";

  size_t prefix_len = strlen(prefix);
  size_t content_len = strlen(content);
  size_t suffix_len = strlen(suffix);
  size_t total_len = prefix_len + content_len + suffix_len;

  char* wrapped_str = malloc(total_len + 1);
  if (!wrapped_str) {
    JS_FreeCString(ctx, content);
    JS_FreeCString(ctx, filename);
    return JS_ThrowOutOfMemory(ctx);
  }

  memcpy(wrapped_str, prefix, prefix_len);
  memcpy(wrapped_str + prefix_len, content, content_len);
  memcpy(wrapped_str + prefix_len + content_len, suffix, suffix_len);
  wrapped_str[total_len] = '\0';

  // Evaluate the wrapped code to get the function
  // The wrapper is: "(function(exports, require, module, __filename, __dirname) { ... })"
  // So evaluating it returns the function itself
  JSValue compiled_fn = JS_Eval(ctx, wrapped_str, strlen(wrapped_str), filename, JS_EVAL_TYPE_GLOBAL);
  free(wrapped_str);  // Free the malloc'd string

  if (JS_IsException(compiled_fn)) {
    JS_FreeCString(ctx, content);
    JS_FreeCString(ctx, filename);
    return JS_EXCEPTION;
  }

  if (!JS_IsFunction(ctx, compiled_fn)) {
    JS_FreeValue(ctx, compiled_fn);
    JS_FreeCString(ctx, content);
    JS_FreeCString(ctx, filename);
    return JS_ThrowTypeError(ctx, "Wrapped code did not produce a function");
  }

  // Prepare arguments: (exports, require, module, __filename, __dirname)
  JSValue args[5];

  // exports
  args[0] = JS_GetPropertyStr(ctx, this_val, "exports");

  // require - get or create require function
  JSValue require_val = JS_GetPropertyStr(ctx, this_val, "require");
  if (JS_IsUndefined(require_val)) {
    // Create require function for this module
    JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
    if (rt && rt->module_loader) {
      require_val = jsrt_create_require_function(ctx, rt->module_loader, filename);
      // Store it on the module for future use
      JS_SetPropertyStr(ctx, this_val, "require", JS_DupValue(ctx, require_val));
    } else {
      require_val = JS_UNDEFINED;
    }
  }
  args[1] = require_val;

  // module
  args[2] = JS_DupValue(ctx, this_val);

  // __filename
  args[3] = JS_NewString(ctx, filename);

  // __dirname - extract directory from filename
  const char* last_sep = strrchr(filename, '/');
  if (!last_sep) {
    last_sep = strrchr(filename, '\\');
  }
  if (last_sep) {
    size_t len = last_sep - filename;
    char* dirname = malloc(len + 1);
    if (dirname) {
      memcpy(dirname, filename, len);
      dirname[len] = '\0';
      args[4] = JS_NewString(ctx, dirname);
      free(dirname);
    } else {
      args[4] = JS_NewString(ctx, ".");
    }
  } else {
    args[4] = JS_NewString(ctx, ".");
  }

  JS_FreeCString(ctx, content);
  JS_FreeCString(ctx, filename);

  // Execute the compiled function
  JSValue result = JS_Call(ctx, compiled_fn, JS_UNDEFINED, 5, args);

  // Free arguments
  for (int i = 0; i < 5; i++) {
    JS_FreeValue(ctx, args[i]);
  }
  JS_FreeValue(ctx, compiled_fn);

  if (JS_IsException(result)) {
    return JS_EXCEPTION;
  }

  JS_FreeValue(ctx, result);

  // Set module.loaded = true
  data->loaded = true;

  return JS_UNDEFINED;
}

/**
 * Package.json utilities
 */

// Cache for package.json lookups to avoid repeated filesystem access
typedef struct {
  char* path;          // Directory path
  char* package_json;  // Path to package.json file (if found)
  time_t mtime;        // Modification time for cache invalidation
} JSRT_PackageJSONCache;

static JSRT_PackageJSONCache* package_json_cache = NULL;
static size_t package_json_cache_size = 0;
static size_t package_json_cache_capacity = 32;

/**
 * Find the nearest package.json file by searching upward from a given path.
 *
 * @param ctx JavaScript context
 * @param this_val The 'this' value (not used)
 * @param argc Number of arguments
 * @param argv Arguments: [specifier, base]
 * @return JSValue: Path to package.json file or undefined
 */
JSValue jsrt_module_find_package_json(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Parse arguments
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "findPackageJSON: at least 1 argument required (specifier)");
  }

  const char* specifier = JS_ToCString(ctx, argv[0]);
  if (!specifier) {
    return JS_ThrowTypeError(ctx, "findPackageJSON: specifier must be a string");
  }

  // Get base path if provided, otherwise use current working directory
  char* base_path = NULL;
  if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
    const char* base_str = JS_ToCString(ctx, argv[1]);
    if (!base_str) {
      JS_FreeCString(ctx, specifier);
      return JS_ThrowTypeError(ctx, "findPackageJSON: base must be a string");
    }
    base_path = jsrt_strdup(base_str);
    JS_FreeCString(ctx, base_str);
  } else {
    // Use current working directory as base
    base_path = getcwd(NULL, 0);
  }

  if (!base_path) {
    JS_FreeCString(ctx, specifier);
    return JS_UNDEFINED;
  }

  // Resolve specifier to absolute path
  char* resolved_path = NULL;
  if (jsrt_is_absolute_path(specifier)) {
    resolved_path = jsrt_strdup(specifier);
  } else if (jsrt_is_relative_path(specifier)) {
    // For relative paths, join with base directory
    resolved_path = jsrt_path_join(base_path, specifier);
  } else {
    // Treat as file path relative to base
    resolved_path = jsrt_path_join(base_path, specifier);
  }

  JS_FreeCString(ctx, specifier);
  free(base_path);

  if (!resolved_path) {
    return JS_UNDEFINED;
  }

  // If resolved path looks like a file (has extension or doesn't end with /), get its directory
  char* search_dir = resolved_path;

  // Check if the resolved path looks like a file by checking if it has an extension or doesn't end with a separator
  const char* last_sep = jsrt_find_last_separator(resolved_path);
  const char* last_dot = strrchr(resolved_path, '.');
  bool looks_like_file = (last_sep && last_dot && last_dot > last_sep);

  // Try to check if it's actually a file
  JSRT_ReadFileResult file_check = JSRT_ReadFile(resolved_path);
  if (file_check.error == JSRT_READ_FILE_OK || looks_like_file) {
    // It's a file or looks like one, get parent directory
    char* parent_dir = jsrt_get_parent_directory(resolved_path);
    free(resolved_path);
    search_dir = parent_dir;
    JSRT_ReadFileResultFree(&file_check);
  } else {
    JSRT_ReadFileResultFree(&file_check);
  }

  if (!search_dir) {
    free(search_dir);
    return JS_UNDEFINED;
  }

  // Search upward for package.json
  char* package_json_path = NULL;
  char* current_dir = jsrt_strdup(search_dir);
  int max_iterations = 50;  // Prevent infinite loops
  int iteration = 0;

  while (current_dir && iteration < max_iterations) {
    iteration++;
    // Check cache first
    bool cache_hit = false;
    for (size_t i = 0; i < package_json_cache_size; i++) {
      if (strcmp(package_json_cache[i].path, current_dir) == 0) {
        // Check if cache entry is still valid
        struct stat st;
        if (stat(current_dir, &st) == 0 && st.st_mtime == package_json_cache[i].mtime) {
          if (package_json_cache[i].package_json) {
            package_json_path = jsrt_strdup(package_json_cache[i].package_json);
          }
          cache_hit = true;
          break;
        } else {
          // Cache entry is stale, remove it
          free(package_json_cache[i].path);
          free(package_json_cache[i].package_json);
          // Move remaining entries
          for (size_t j = i; j < package_json_cache_size - 1; j++) {
            package_json_cache[j] = package_json_cache[j + 1];
          }
          package_json_cache_size--;
          i--;  // Adjust index since we removed an entry
        }
      }
    }

    if (cache_hit) {
      break;
    }

    // Construct path to package.json in current directory
    char* candidate_path = jsrt_path_join(current_dir, "package.json");
    if (!candidate_path) {
      break;
    }

    // Check if package.json exists
    JSRT_ReadFileResult result = JSRT_ReadFile(candidate_path);
    bool found = (result.error == JSRT_READ_FILE_OK);
    JSRT_ReadFileResultFree(&result);

    if (found) {
      package_json_path = candidate_path;

      // Cache the result
      // Ensure cache has capacity
      if (package_json_cache_size >= package_json_cache_capacity) {
        // Remove oldest entry (simple FIFO)
        if (package_json_cache_size > 0) {
          free(package_json_cache[0].path);
          free(package_json_cache[0].package_json);
          // Move remaining entries
          for (size_t i = 0; i < package_json_cache_size - 1; i++) {
            package_json_cache[i] = package_json_cache[i + 1];
          }
          package_json_cache_size--;
        }
      }

      // Add new cache entry
      struct stat st;
      if (stat(current_dir, &st) == 0) {
        package_json_cache[package_json_cache_size].path = jsrt_strdup(current_dir);
        package_json_cache[package_json_cache_size].package_json = jsrt_strdup(candidate_path);
        package_json_cache[package_json_cache_size].mtime = st.st_mtime;
        package_json_cache_size++;
      }
      break;
    }

    free(candidate_path);

    // Move to parent directory
    char* parent_dir = jsrt_get_parent_directory(current_dir);
    char* old_dir = current_dir;
    current_dir = parent_dir;
    free(old_dir);

    // Stop if we've reached the root or can't go higher
    if (!current_dir || strcmp(current_dir, ".") == 0 || strcmp(current_dir, "..") == 0) {
      break;
    }
  }

  // current_dir is already freed in the loop, but handle the case where loop didn't run
  if (current_dir) {
    free(current_dir);
  }
  free(search_dir);

  JSValue result;
  if (package_json_path) {
    // Remove './' prefix if present for cleaner output
    char* clean_path = package_json_path;
    if (strstr(package_json_path, "/./") != NULL) {
      // Replace "/./" with "/" in the path
      size_t len = strlen(package_json_path);
      char* temp = malloc(len + 1);
      if (temp) {
        char* src = (char*)package_json_path;
        char* dst = temp;
        while (*src) {
          if (strncmp(src, "/./", 3) == 0) {
            *dst++ = '/';
            src += 3;
          } else {
            *dst++ = *src++;
          }
        }
        *dst = '\0';
        free(package_json_path);
        clean_path = temp;
      }
    }
    result = JS_NewString(ctx, clean_path);
    free(clean_path);
  } else {
    result = JS_UNDEFINED;
  }

  return result;
}

/**
 * Parse a package.json file and return its contents as a JavaScript object.
 *
 * @param ctx JavaScript context
 * @param this_val The 'this' value (not used)
 * @param argc Number of arguments
 * @param argv Arguments: [path]
 * @return JSValue: Parsed package.json object or throw error
 */
JSValue jsrt_module_parse_package_json(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "parsePackageJSON: 1 argument required (path)");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_ThrowTypeError(ctx, "parsePackageJSON: path must be a string");
  }

  // Read the file
  JSRT_ReadFileResult result = JSRT_ReadFile(path);
  if (result.error != JSRT_READ_FILE_OK) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "parsePackageJSON: failed to read file '%s': %s", path,
                             JSRT_ReadFileErrorToString(result.error));
  }

  // Parse JSON
  JSValue json_obj = JS_ParseJSON(ctx, result.data, result.size, "package.json");

  JSRT_ReadFileResultFree(&result);
  JS_FreeCString(ctx, path);

  return json_obj;
}

static JSValue jsrt_module_enable_compile_cache(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_CompileCacheConfig* config = jsrt_module_get_compile_cache(ctx);
  if (!config) {
    return jsrt_module_build_compile_cache_result(ctx, JSRT_COMPILE_CACHE_FAILED, NULL);
  }

  const char* directory_arg = NULL;
  const char* directory_cstr = NULL;
  bool portable = config->portable;

  if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
    if (JS_IsString(argv[0])) {
      directory_cstr = JS_ToCString(ctx, argv[0]);
      if (!directory_cstr) {
        return JS_EXCEPTION;
      }
      directory_arg = directory_cstr;
    } else if (JS_IsObject(argv[0])) {
      JSValue dir_val = JS_GetPropertyStr(ctx, argv[0], "directory");
      if (JS_IsException(dir_val)) {
        return dir_val;
      }
      if (JS_IsString(dir_val)) {
        directory_cstr = JS_ToCString(ctx, dir_val);
        if (!directory_cstr) {
          JS_FreeValue(ctx, dir_val);
          return JS_EXCEPTION;
        }
        directory_arg = directory_cstr;
      }
      JS_FreeValue(ctx, dir_val);

      JSValue portable_val = JS_GetPropertyStr(ctx, argv[0], "portable");
      if (JS_IsException(portable_val)) {
        if (directory_cstr) {
          JS_FreeCString(ctx, directory_cstr);
        }
        return portable_val;
      }
      if (!JS_IsUndefined(portable_val)) {
        portable = JS_ToBool(ctx, portable_val);
      }
      JS_FreeValue(ctx, portable_val);
    } else {
      return JS_ThrowTypeError(ctx, "enableCompileCache expects a string path or options object");
    }
  }

  JSRT_CompileCacheStatus status = jsrt_compile_cache_enable(ctx, config, directory_arg, portable);

  if (directory_cstr) {
    JS_FreeCString(ctx, directory_cstr);
  }

  return jsrt_module_build_compile_cache_result(ctx, status, config);
}

static JSValue jsrt_module_get_compile_cache_dir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_CompileCacheConfig* config = jsrt_module_get_compile_cache(ctx);
  if (!config || !jsrt_compile_cache_is_enabled(config)) {
    return JS_UNDEFINED;
  }

  const char* directory = jsrt_compile_cache_get_directory(config);
  if (!directory) {
    return JS_UNDEFINED;
  }

  return JS_NewString(ctx, directory);
}

static JSValue jsrt_module_flush_compile_cache(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_CompileCacheConfig* config = jsrt_module_get_compile_cache(ctx);
  if (!config || !jsrt_compile_cache_is_enabled(config)) {
    return JS_NewInt32(ctx, JSRT_COMPILE_CACHE_DISABLED);
  }

  int rc = jsrt_compile_cache_flush(config);
  return JS_NewInt32(ctx, rc);
}

static JSValue jsrt_module_clear_compile_cache(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_CompileCacheConfig* config = jsrt_module_get_compile_cache(ctx);
  if (!config || !jsrt_compile_cache_is_enabled(config)) {
    return JS_NewInt32(ctx, 0);
  }

  int removed_count = jsrt_compile_cache_clear(config);
  return JS_NewInt32(ctx, removed_count);
}

static JSValue jsrt_module_get_compile_cache_stats(JSContext* ctx, JSValueConst this_val, int argc,
                                                   JSValueConst* argv) {
  JSRT_CompileCacheConfig* config = jsrt_module_get_compile_cache(ctx);
  if (!config || !jsrt_compile_cache_is_enabled(config)) {
    return JS_UNDEFINED;
  }

  uint64_t hits = 0, misses = 0, writes = 0, errors = 0, evictions = 0;
  size_t current_size = 0, size_limit = 0;

  jsrt_compile_cache_get_stats(config, &hits, &misses, &writes, &errors, &evictions, &current_size, &size_limit);

  JSValue result = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, result, "hits", JS_NewInt64(ctx, hits));
  JS_SetPropertyStr(ctx, result, "misses", JS_NewInt64(ctx, misses));
  JS_SetPropertyStr(ctx, result, "writes", JS_NewInt64(ctx, writes));
  JS_SetPropertyStr(ctx, result, "errors", JS_NewInt64(ctx, errors));
  JS_SetPropertyStr(ctx, result, "evictions", JS_NewInt64(ctx, evictions));
  JS_SetPropertyStr(ctx, result, "currentSize", JS_NewInt64(ctx, current_size));
  JS_SetPropertyStr(ctx, result, "sizeLimit", JS_NewInt64(ctx, size_limit));

  // Calculate hit rate
  uint64_t total_requests = hits + misses;
  double hit_rate = total_requests > 0 ? (double)hits / total_requests * 100.0 : 0.0;
  JS_SetPropertyStr(ctx, result, "hitRate", JS_NewFloat64(ctx, hit_rate));

  // Calculate size utilization
  double utilization = size_limit > 0 ? (double)current_size / size_limit * 100.0 : 0.0;
  JS_SetPropertyStr(ctx, result, "utilization", JS_NewFloat64(ctx, utilization));

  return result;
}

/**
 * Module.getStatistics() - Get module loading statistics
 */
static JSValue jsrt_module_get_statistics(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("Module.getStatistics() called");

  // Get runtime to access module loader
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->module_loader) {
    JSRT_Debug("No runtime or module loader available for statistics");
    return JS_UNDEFINED;
  }

  JSRT_ModuleLoader* loader = rt->module_loader;

  // Create statistics object
  JSValue stats = JS_NewObject(ctx);
  if (JS_IsException(stats)) {
    return stats;
  }

  // Basic loading statistics
  JS_SetPropertyStr(ctx, stats, "loadsTotal", JS_NewInt64(ctx, loader->loads_total));
  JS_SetPropertyStr(ctx, stats, "loadsSuccess", JS_NewInt64(ctx, loader->loads_success));
  JS_SetPropertyStr(ctx, stats, "loadsFailed", JS_NewInt64(ctx, loader->loads_failed));

  // Calculate success rate
  double success_rate = loader->loads_total > 0 ? (double)loader->loads_success / loader->loads_total * 100.0 : 0.0;
  JS_SetPropertyStr(ctx, stats, "successRate", JS_NewFloat64(ctx, success_rate));

  // Cache statistics
  JS_SetPropertyStr(ctx, stats, "cacheHits", JS_NewInt64(ctx, loader->cache_hits));
  JS_SetPropertyStr(ctx, stats, "cacheMisses", JS_NewInt64(ctx, loader->cache_misses));

  // Calculate cache hit rate
  uint64_t total_cache_requests = loader->cache_hits + loader->cache_misses;
  double cache_hit_rate = total_cache_requests > 0 ? (double)loader->cache_hits / total_cache_requests * 100.0 : 0.0;
  JS_SetPropertyStr(ctx, stats, "cacheHitRate", JS_NewFloat64(ctx, cache_hit_rate));

  // Memory usage
  JS_SetPropertyStr(ctx, stats, "memoryUsed", JS_NewInt64(ctx, loader->memory_used));

  // Get detailed cache statistics if available
  if (loader->cache) {
    uint64_t cache_hits = 0, cache_misses = 0;
    size_t cache_size = 0, cache_memory_used = 0;

    jsrt_module_cache_get_stats(loader->cache, &cache_hits, &cache_misses, &cache_size, &cache_memory_used);

    JSValue cache_stats = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, cache_stats, "hits", JS_NewInt64(ctx, cache_hits));
    JS_SetPropertyStr(ctx, cache_stats, "misses", JS_NewInt64(ctx, cache_misses));
    JS_SetPropertyStr(ctx, cache_stats, "size", JS_NewInt64(ctx, cache_size));
    JS_SetPropertyStr(ctx, cache_stats, "maxSize", JS_NewInt64(ctx, loader->max_cache_size));

    // Calculate cache utilization
    double cache_utilization = loader->max_cache_size > 0 ? (double)cache_size / loader->max_cache_size * 100.0 : 0.0;
    JS_SetPropertyStr(ctx, cache_stats, "utilization", JS_NewFloat64(ctx, cache_utilization));

    JS_SetPropertyStr(ctx, stats, "moduleCache", cache_stats);
  }

  // Get compile cache statistics if available
  JSRT_CompileCacheConfig* compile_cache = jsrt_module_get_compile_cache(ctx);
  if (compile_cache && jsrt_compile_cache_is_enabled(compile_cache)) {
    uint64_t cc_hits = 0, cc_misses = 0, cc_writes = 0, cc_errors = 0, cc_evictions = 0;
    size_t cc_current_size = 0, cc_size_limit = 0;

    jsrt_compile_cache_get_stats(compile_cache, &cc_hits, &cc_misses, &cc_writes, &cc_errors, &cc_evictions,
                                 &cc_current_size, &cc_size_limit);

    JSValue cc_stats = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, cc_stats, "hits", JS_NewInt64(ctx, cc_hits));
    JS_SetPropertyStr(ctx, cc_stats, "misses", JS_NewInt64(ctx, cc_misses));
    JS_SetPropertyStr(ctx, cc_stats, "writes", JS_NewInt64(ctx, cc_writes));
    JS_SetPropertyStr(ctx, cc_stats, "errors", JS_NewInt64(ctx, cc_errors));
    JS_SetPropertyStr(ctx, cc_stats, "evictions", JS_NewInt64(ctx, cc_evictions));
    JS_SetPropertyStr(ctx, cc_stats, "currentSize", JS_NewInt64(ctx, cc_current_size));
    JS_SetPropertyStr(ctx, cc_stats, "sizeLimit", JS_NewInt64(ctx, cc_size_limit));

    // Calculate compile cache hit rate
    uint64_t total_cc_requests = cc_hits + cc_misses;
    double cc_hit_rate = total_cc_requests > 0 ? (double)cc_hits / total_cc_requests * 100.0 : 0.0;
    JS_SetPropertyStr(ctx, cc_stats, "hitRate", JS_NewFloat64(ctx, cc_hit_rate));

    // Calculate compile cache utilization
    double cc_utilization = cc_size_limit > 0 ? (double)cc_current_size / cc_size_limit * 100.0 : 0.0;
    JS_SetPropertyStr(ctx, cc_stats, "utilization", JS_NewFloat64(ctx, cc_utilization));

    JS_SetPropertyStr(ctx, stats, "compileCache", cc_stats);
  }

  // Configuration info
  JSValue config = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, config, "cacheEnabled", JS_NewBool(ctx, loader->enable_cache));
  JS_SetPropertyStr(ctx, config, "httpImportsEnabled", JS_NewBool(ctx, loader->enable_http_imports));
  JS_SetPropertyStr(ctx, config, "nodeCompatEnabled", JS_NewBool(ctx, loader->enable_node_compat));
  JS_SetPropertyStr(ctx, config, "maxCacheSize", JS_NewInt64(ctx, loader->max_cache_size));
  JS_SetPropertyStr(ctx, stats, "configuration", config);

  return stats;
}

/**
 * module.reloadModule(path) - Hot reload a module
 */
static JSValue jsrt_module_reload_module(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSRT_Debug("module.reloadModule() called");

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing path argument");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  if (!path) {
    return JS_EXCEPTION;
  }

  // Get runtime to access module loader
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  if (!rt || !rt->module_loader) {
    JS_FreeCString(ctx, path);
    return JS_ThrowTypeError(ctx, "No module loader available");
  }

  JSRT_ModuleLoader* loader = rt->module_loader;

  // Create result object
  JSValue result = JS_NewObject(ctx);
  if (JS_IsException(result)) {
    JS_FreeCString(ctx, path);
    return result;
  }

  // Get initial statistics for comparison
  uint64_t initial_loads = loader->loads_total;
  uint64_t initial_success = loader->loads_success;
  uint64_t initial_failed = loader->loads_failed;

  JSRT_Debug("Attempting to reload module: %s", path);

  // First, resolve the path to ensure it's in the right format for cache lookup
  JSRT_ResolvedPath* resolved = jsrt_resolve_path(ctx, path, NULL, false);
  if (!resolved || !resolved->resolved_path) {
    // If resolution failed, it might be a builtin module
    // Try treating it as a builtin module (node: prefix)
    char builtin_path[256];
    snprintf(builtin_path, sizeof(builtin_path), "node:%s", path);

    jsrt_resolved_path_free(resolved);
    resolved = jsrt_resolve_path(ctx, builtin_path, NULL, false);

    if (!resolved || !resolved->resolved_path) {
      JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, "Failed to resolve module path"));
      JS_SetPropertyStr(ctx, result, "reloadSuccess", JS_FALSE);
      JS_FreeCString(ctx, path);
      if (resolved) {
        jsrt_resolved_path_free(resolved);
      }
      return result;
    }
  }

  // Invalidate the module in cache
  int invalidate_result = jsrt_invalidate_module(loader, resolved->resolved_path);
  bool was_cached = (invalidate_result == 0);

  // Note: Module._cache clearing is not needed since reloadModule() already
  // updates the module in our internal cache and require() will get the updated version

  // Try to reload the module
  JSValue reloaded_module = JS_UNDEFINED;
  bool reload_success = false;
  const char* error_message = NULL;

  if (!JS_IsUndefined(argv[0])) {
    // Attempt to load the module using resolved path
    reloaded_module = jsrt_load_module(loader, resolved->resolved_path, NULL);

    if (JS_IsException(reloaded_module)) {
      // Get error message
      JSValue exception = JS_GetException(ctx);
      if (!JS_IsUndefined(exception) && !JS_IsNull(exception)) {
        error_message = JS_ToCString(ctx, exception);
      }
      JS_FreeValue(ctx, exception);
      reload_success = false;
    } else {
      reload_success = true;
    }
  }

  // Calculate statistics delta
  uint64_t new_loads = loader->loads_total - initial_loads;
  uint64_t new_success = loader->loads_success - initial_success;
  uint64_t new_failed = loader->loads_failed - initial_failed;

  // Populate result object
  JS_SetPropertyStr(ctx, result, "path", JS_NewString(ctx, path));
  JS_SetPropertyStr(ctx, result, "resolvedPath", JS_NewString(ctx, resolved->resolved_path));
  JS_SetPropertyStr(ctx, result, "wasCached", JS_NewBool(ctx, was_cached));
  JS_SetPropertyStr(ctx, result, "reloadSuccess", JS_NewBool(ctx, reload_success));

  if (error_message) {
    JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, error_message));
    JS_FreeCString(ctx, error_message);
  }

  // Statistics changes
  JSValue stats_delta = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, stats_delta, "loadsAttempted", JS_NewInt64(ctx, new_loads));
  JS_SetPropertyStr(ctx, stats_delta, "loadsSuccessful", JS_NewInt64(ctx, new_success));
  JS_SetPropertyStr(ctx, stats_delta, "loadsFailed", JS_NewInt64(ctx, new_failed));
  JS_SetPropertyStr(ctx, result, "statistics", stats_delta);

  // Return the reloaded module exports if successful
  if (reload_success && !JS_IsUndefined(reloaded_module)) {
    JS_SetPropertyStr(ctx, result, "exports", JS_DupValue(ctx, reloaded_module));
  }

  JS_FreeValue(ctx, reloaded_module);
  JS_FreeCString(ctx, path);
  if (resolved) {
    jsrt_resolved_path_free(resolved);
  }

  JSRT_Debug("Module reload completed for %s: success=%s, was_cached=%s", path, reload_success ? "true" : "false",
             was_cached ? "true" : "false");

  return result;
}

/**
 * Module.wrap(script) - Wrap script in CommonJS wrapper
 */
static JSValue jsrt_module_wrap(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing script argument");
  }

  const char* script = JS_ToCString(ctx, argv[0]);
  if (!script) {
    return JS_EXCEPTION;
  }

  // CommonJS wrapper format
  const char* wrapper_prefix = "(function (exports, require, module, __filename, __dirname) { ";
  const char* wrapper_suffix = "\n});";

  size_t prefix_len = strlen(wrapper_prefix);
  size_t script_len = strlen(script);
  size_t suffix_len = strlen(wrapper_suffix);
  size_t total_len = prefix_len + script_len + suffix_len;

  char* wrapped = malloc(total_len + 1);
  if (!wrapped) {
    JS_FreeCString(ctx, script);
    return JS_ThrowOutOfMemory(ctx);
  }

  memcpy(wrapped, wrapper_prefix, prefix_len);
  memcpy(wrapped + prefix_len, script, script_len);
  memcpy(wrapped + prefix_len + script_len, wrapper_suffix, suffix_len);
  wrapped[total_len] = '\0';

  JSValue result = JS_NewString(ctx, wrapped);
  free(wrapped);
  JS_FreeCString(ctx, script);

  return result;
}

/**
 * Initialize node:module API
 */
JSValue JSRT_InitNodeModule(JSContext* ctx) {
  // Initialize package.json cache if not already initialized
  if (!package_json_cache) {
    package_json_cache = malloc(package_json_cache_capacity * sizeof(JSRT_PackageJSONCache));
    if (!package_json_cache) {
      JS_ThrowOutOfMemory(ctx);
      return JS_EXCEPTION;
    }
    memset(package_json_cache, 0, package_json_cache_capacity * sizeof(JSRT_PackageJSONCache));
  }

  // Create Module class
  JS_NewClassID(&jsrt_module_class_id);
  JS_NewClass(JS_GetRuntime(ctx), jsrt_module_class_id, &jsrt_module_class);

  // Create Module constructor
  JSValue ctor = JS_NewCFunction2(ctx, jsrt_module_constructor, "Module", 2, JS_CFUNC_constructor, 0);

  // Create Module prototype and add instance methods
  JSValue proto = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, proto, "require", JS_NewCFunction(ctx, jsrt_module_require, "require", 1));
  JS_SetPropertyStr(ctx, proto, "_compile", JS_NewCFunction(ctx, jsrt_module_compile, "_compile", 2));
  JS_SetPropertyStr(ctx, ctor, "prototype", proto);

  // Add static methods to Module constructor
  JS_SetPropertyStr(ctx, ctor, "builtinModules", jsrt_module_builtinModules(ctx));
  JS_SetPropertyStr(ctx, ctor, "isBuiltin", JS_NewCFunction(ctx, jsrt_module_isBuiltin, "isBuiltin", 1));
  JS_SetPropertyStr(ctx, ctor, "createRequire", JS_NewCFunction(ctx, jsrt_module_createRequire, "createRequire", 1));
  JS_SetPropertyStr(ctx, ctor, "syncBuiltinESMExports",
                    JS_NewCFunction(ctx, jsrt_module_syncBuiltinESMExports, "syncBuiltinESMExports", 0));
  JS_SetPropertyStr(ctx, ctor, "_load", JS_NewCFunction(ctx, jsrt_module_load, "_load", 3));
  JS_SetPropertyStr(ctx, ctor, "_resolveFilename",
                    JS_NewCFunction(ctx, jsrt_module_resolve_filename, "_resolveFilename", 4));
  JS_SetPropertyStr(ctx, ctor, "wrap", JS_NewCFunction(ctx, jsrt_module_wrap, "wrap", 1));
  JS_SetPropertyStr(ctx, ctor, "enableCompileCache",
                    JS_NewCFunction(ctx, jsrt_module_enable_compile_cache, "enableCompileCache", 1));
  JS_SetPropertyStr(ctx, ctor, "getCompileCacheDir",
                    JS_NewCFunction(ctx, jsrt_module_get_compile_cache_dir, "getCompileCacheDir", 0));
  JS_SetPropertyStr(ctx, ctor, "flushCompileCache",
                    JS_NewCFunction(ctx, jsrt_module_flush_compile_cache, "flushCompileCache", 0));
  JS_SetPropertyStr(ctx, ctor, "clearCompileCache",
                    JS_NewCFunction(ctx, jsrt_module_clear_compile_cache, "clearCompileCache", 0));
  JS_SetPropertyStr(ctx, ctor, "getCompileCacheStats",
                    JS_NewCFunction(ctx, jsrt_module_get_compile_cache_stats, "getCompileCacheStats", 0));
  JS_SetPropertyStr(ctx, ctor, "getStatistics", JS_NewCFunction(ctx, jsrt_module_get_statistics, "getStatistics", 0));
  JS_SetPropertyStr(ctx, ctor, "reloadModule", JS_NewCFunction(ctx, jsrt_module_reload_module, "reloadModule", 1));

  // Add Module.wrapper property (array with wrapper parts)
  JSValue wrapper = JS_NewArray(ctx);
  JS_SetPropertyUint32(ctx, wrapper, 0,
                       JS_NewString(ctx, "(function (exports, require, module, __filename, __dirname) { "));
  JS_SetPropertyUint32(ctx, wrapper, 1, JS_NewString(ctx, "\n});"));
  JS_DefinePropertyValueStr(ctx, ctor, "wrapper", wrapper, JS_PROP_ENUMERABLE);

  // Create Module._extensions object (deprecated but needed for compatibility)
  // This object maps file extensions to loader functions
  JSValue extensions = JS_NewObject(ctx);

  // Note: Extension handlers are implemented in JavaScript rather than C for flexibility
  // Each handler follows the signature: function(module, filename)
  // Default handlers will be initialized via JavaScript wrapper code

  JS_DefinePropertyValueStr(ctx, ctor, "_extensions", extensions, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  // Add Module._cache object
  // This object provides access to the internal module cache
  // Initially empty, will be populated as modules are loaded via Module._load
  JSValue cache = JS_NewObject(ctx);
  JS_DefinePropertyValueStr(ctx, ctor, "_cache", cache, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

  // Store reference to cache object in constructor for later access
  // This allows Module._load to populate it when modules are cached
  JS_SetPropertyStr(ctx, ctor, "__cacheRef", JS_DupValue(ctx, cache));

  // Create module exports object
  JSValue module_obj = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, module_obj, "Module", ctor);
  JS_SetPropertyStr(ctx, module_obj, "builtinModules", jsrt_module_builtinModules(ctx));
  JS_SetPropertyStr(ctx, module_obj, "isBuiltin", JS_NewCFunction(ctx, jsrt_module_isBuiltin, "isBuiltin", 1));
  JS_SetPropertyStr(ctx, module_obj, "createRequire",
                    JS_NewCFunction(ctx, jsrt_module_createRequire, "createRequire", 1));
  JS_SetPropertyStr(ctx, module_obj, "syncBuiltinESMExports",
                    JS_NewCFunction(ctx, jsrt_module_syncBuiltinESMExports, "syncBuiltinESMExports", 0));
  JS_SetPropertyStr(ctx, module_obj, "findSourceMap",
                    JS_NewCFunction(ctx, jsrt_module_find_source_map, "findSourceMap", 1));
  JS_SetPropertyStr(ctx, module_obj, "getSourceMapsSupport",
                    JS_NewCFunction(ctx, jsrt_module_get_source_maps_support, "getSourceMapsSupport", 0));
  JS_SetPropertyStr(ctx, module_obj, "setSourceMapsSupport",
                    JS_NewCFunction(ctx, jsrt_module_set_source_maps_support, "setSourceMapsSupport", 2));
  JS_SetPropertyStr(ctx, module_obj, "registerHooks",
                    JS_NewCFunction(ctx, jsrt_module_register_hooks, "registerHooks", 1));
  JS_SetPropertyStr(ctx, module_obj, "findPackageJSON",
                    JS_NewCFunction(ctx, jsrt_module_find_package_json, "findPackageJSON", 2));
  JS_SetPropertyStr(ctx, module_obj, "parsePackageJSON",
                    JS_NewCFunction(ctx, jsrt_module_parse_package_json, "parsePackageJSON", 1));
  JS_SetPropertyStr(ctx, module_obj, "enableCompileCache",
                    JS_NewCFunction(ctx, jsrt_module_enable_compile_cache, "enableCompileCache", 1));
  JS_SetPropertyStr(ctx, module_obj, "getCompileCacheDir",
                    JS_NewCFunction(ctx, jsrt_module_get_compile_cache_dir, "getCompileCacheDir", 0));
  JS_SetPropertyStr(ctx, module_obj, "flushCompileCache",
                    JS_NewCFunction(ctx, jsrt_module_flush_compile_cache, "flushCompileCache", 0));
  JS_SetPropertyStr(ctx, module_obj, "clearCompileCache",
                    JS_NewCFunction(ctx, jsrt_module_clear_compile_cache, "clearCompileCache", 0));
  JS_SetPropertyStr(ctx, module_obj, "getCompileCacheStats",
                    JS_NewCFunction(ctx, jsrt_module_get_compile_cache_stats, "getCompileCacheStats", 0));
  JS_SetPropertyStr(ctx, module_obj, "getStatistics",
                    JS_NewCFunction(ctx, jsrt_module_get_statistics, "getStatistics", 0));
  JS_SetPropertyStr(ctx, module_obj, "reloadModule",
                    JS_NewCFunction(ctx, jsrt_module_reload_module, "reloadModule", 1));

  JSValue compile_cache_status = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, compile_cache_status, "ENABLED", JS_NewInt32(ctx, JSRT_COMPILE_CACHE_ENABLED));
  JS_SetPropertyStr(ctx, compile_cache_status, "ALREADY_ENABLED", JS_NewInt32(ctx, JSRT_COMPILE_CACHE_ALREADY_ENABLED));
  JS_SetPropertyStr(ctx, compile_cache_status, "FAILED", JS_NewInt32(ctx, JSRT_COMPILE_CACHE_FAILED));
  JS_SetPropertyStr(ctx, compile_cache_status, "DISABLED", JS_NewInt32(ctx, JSRT_COMPILE_CACHE_DISABLED));

  JSValue constants = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, constants, "compileCacheStatus", JS_DupValue(ctx, compile_cache_status));

  JS_DefinePropertyValueStr(ctx, ctor, "constants", JS_DupValue(ctx, constants), JS_PROP_ENUMERABLE);
  JS_DefinePropertyValueStr(ctx, module_obj, "constants", constants, JS_PROP_ENUMERABLE | JS_PROP_CONFIGURABLE);
  JS_FreeValue(ctx, compile_cache_status);

  // Initialize SourceMap class (for source map support)
  if (!jsrt_source_map_class_init(ctx, module_obj)) {
    JSRT_Debug("Warning: Failed to initialize SourceMap class");
  }

  return module_obj;
}

/**
 * ES Module initialization
 */
int js_node_module_init(JSContext* ctx, JSModuleDef* m) {
  JSValue module_obj = JSRT_InitNodeModule(ctx);

  // Export Module constructor
  JSValue ctor = JS_GetPropertyStr(ctx, module_obj, "Module");
  JS_SetModuleExport(ctx, m, "Module", JS_DupValue(ctx, ctor));
  JS_FreeValue(ctx, ctor);

  // Export module functions
  JSValue builtin_modules = JS_GetPropertyStr(ctx, module_obj, "builtinModules");
  JS_SetModuleExport(ctx, m, "builtinModules", JS_DupValue(ctx, builtin_modules));
  JS_FreeValue(ctx, builtin_modules);

  JSValue is_builtin = JS_GetPropertyStr(ctx, module_obj, "isBuiltin");
  JS_SetModuleExport(ctx, m, "isBuiltin", JS_DupValue(ctx, is_builtin));
  JS_FreeValue(ctx, is_builtin);

  JSValue create_require = JS_GetPropertyStr(ctx, module_obj, "createRequire");
  JS_SetModuleExport(ctx, m, "createRequire", JS_DupValue(ctx, create_require));
  JS_FreeValue(ctx, create_require);

  JSValue sync_exports = JS_GetPropertyStr(ctx, module_obj, "syncBuiltinESMExports");
  JS_SetModuleExport(ctx, m, "syncBuiltinESMExports", JS_DupValue(ctx, sync_exports));
  JS_FreeValue(ctx, sync_exports);

  JSValue find_source_map = JS_GetPropertyStr(ctx, module_obj, "findSourceMap");
  JS_SetModuleExport(ctx, m, "findSourceMap", JS_DupValue(ctx, find_source_map));
  JS_FreeValue(ctx, find_source_map);

  JSValue get_source_maps_support = JS_GetPropertyStr(ctx, module_obj, "getSourceMapsSupport");
  JS_SetModuleExport(ctx, m, "getSourceMapsSupport", JS_DupValue(ctx, get_source_maps_support));
  JS_FreeValue(ctx, get_source_maps_support);

  JSValue set_source_maps_support = JS_GetPropertyStr(ctx, module_obj, "setSourceMapsSupport");
  JS_SetModuleExport(ctx, m, "setSourceMapsSupport", JS_DupValue(ctx, set_source_maps_support));
  JS_FreeValue(ctx, set_source_maps_support);

  JSValue register_hooks = JS_GetPropertyStr(ctx, module_obj, "registerHooks");
  JS_SetModuleExport(ctx, m, "registerHooks", JS_DupValue(ctx, register_hooks));
  JS_FreeValue(ctx, register_hooks);

  JSValue enable_compile_cache = JS_GetPropertyStr(ctx, module_obj, "enableCompileCache");
  JS_SetModuleExport(ctx, m, "enableCompileCache", JS_DupValue(ctx, enable_compile_cache));
  JS_FreeValue(ctx, enable_compile_cache);

  JSValue get_compile_cache_dir = JS_GetPropertyStr(ctx, module_obj, "getCompileCacheDir");
  JS_SetModuleExport(ctx, m, "getCompileCacheDir", JS_DupValue(ctx, get_compile_cache_dir));
  JS_FreeValue(ctx, get_compile_cache_dir);

  JSValue flush_compile_cache = JS_GetPropertyStr(ctx, module_obj, "flushCompileCache");
  JS_SetModuleExport(ctx, m, "flushCompileCache", JS_DupValue(ctx, flush_compile_cache));
  JS_FreeValue(ctx, flush_compile_cache);

  JSValue clear_compile_cache = JS_GetPropertyStr(ctx, module_obj, "clearCompileCache");
  JS_SetModuleExport(ctx, m, "clearCompileCache", JS_DupValue(ctx, clear_compile_cache));
  JS_FreeValue(ctx, clear_compile_cache);

  JSValue get_compile_cache_stats = JS_GetPropertyStr(ctx, module_obj, "getCompileCacheStats");
  JS_SetModuleExport(ctx, m, "getCompileCacheStats", JS_DupValue(ctx, get_compile_cache_stats));
  JS_FreeValue(ctx, get_compile_cache_stats);

  JSValue get_statistics = JS_GetPropertyStr(ctx, module_obj, "getStatistics");
  JS_SetModuleExport(ctx, m, "getStatistics", JS_DupValue(ctx, get_statistics));
  JS_FreeValue(ctx, get_statistics);

  JSValue reload_module = JS_GetPropertyStr(ctx, module_obj, "reloadModule");
  JS_SetModuleExport(ctx, m, "reloadModule", JS_DupValue(ctx, reload_module));
  JS_FreeValue(ctx, reload_module);

  JSValue module_constants = JS_GetPropertyStr(ctx, module_obj, "constants");
  JS_SetModuleExport(ctx, m, "constants", JS_DupValue(ctx, module_constants));
  JS_FreeValue(ctx, module_constants);

  // Export default
  JS_SetModuleExport(ctx, m, "default", module_obj);

  return 0;
}
