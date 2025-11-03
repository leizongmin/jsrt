#include "module_api.h"
#include <stdlib.h>
#include <string.h>
#include "../../module/core/module_cache.h"
#include "../../module/core/module_context.h"
#include "../../module/core/module_loader.h"
#include "../../module/loaders/commonjs_loader.h"
#include "../../module/resolver/path_resolver.h"
#include "../../runtime.h"
#include "../../util/debug.h"
#include "../node_modules.h"
#include "sourcemap.h"

// Module class ID for opaque data
static JSClassID jsrt_module_class_id;

// Forward declarations
static void jsrt_module_finalizer(JSRuntime* rt, JSValue val);
static JSClassDef jsrt_module_class = {
    .class_name = "Module",
    .finalizer = jsrt_module_finalizer,
};

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
  // TODO: Implementation will sync built-in module exports
  // For now, return undefined as placeholder
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

  // Export default
  JS_SetModuleExport(ctx, m, "default", module_obj);

  return 0;
}
