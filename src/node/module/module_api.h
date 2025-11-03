#ifndef __JSRT_NODE_MODULE_API_H__
#define __JSRT_NODE_MODULE_API_H__

#include <quickjs.h>
#include <stdbool.h>

/**
 * @file module_api.h
 * @brief Node.js module API implementation (node:module)
 *
 * Provides programmatic access to the module system including:
 * - Module class with static/instance methods
 * - module.builtinModules - List of built-in modules
 * - module.createRequire() - Create require function for ESM
 * - module.isBuiltin() - Check if module is built-in
 * - module.syncBuiltinESMExports() - Sync CommonJS/ESM exports
 * - Source map support (findSourceMap, SourceMap class)
 * - Compilation cache management
 */

// Module class data structure
typedef struct {
  JSValue exports;   // module.exports object
  JSValue require;   // Bound require function
  char* id;          // Module identifier
  char* filename;    // Absolute file path
  bool loaded;       // Load completion flag
  JSValue parent;    // Parent module
  JSValue children;  // Array of child modules
  JSValue paths;     // Search paths array
  char* path;        // Directory name
} JSRT_ModuleData;

// Initialize the node:module API
JSValue JSRT_InitNodeModule(JSContext* ctx);
int js_node_module_init(JSContext* ctx, JSModuleDef* m);

// Core Module APIs
JSValue jsrt_module_builtinModules(JSContext* ctx);
JSValue jsrt_module_isBuiltin(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_module_createRequire(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_module_syncBuiltinESMExports(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_module_find_source_map(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_module_get_source_maps_support(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_module_set_source_maps_support(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_module_register_hooks(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Package.json utilities
JSValue jsrt_module_find_package_json(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_module_parse_package_json(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Module class constructor
JSValue jsrt_module_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

// Module class static methods
JSValue jsrt_module_load(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_module_resolve_filename(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Module instance methods
JSValue jsrt_module_require(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue jsrt_module_compile(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Helper functions
JSRT_ModuleData* jsrt_module_get_data(JSContext* ctx, JSValueConst obj);
void jsrt_module_free_data(JSRuntime* rt, JSRT_ModuleData* data);

#endif  // __JSRT_NODE_MODULE_API_H__
