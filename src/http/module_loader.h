#ifndef __JSRT_HTTP_MODULE_LOADER_H__
#define __JSRT_HTTP_MODULE_LOADER_H__

#include "../../deps/quickjs/quickjs.h"

// Load an ES module from HTTP/HTTPS URL
JSModuleDef* jsrt_load_http_module(JSContext* ctx, const char* url);

// Load a CommonJS module from HTTP/HTTPS URL
JSValue jsrt_require_http_module(JSContext* ctx, const char* url);

// Resolve relative imports from HTTP modules
char* jsrt_resolve_http_relative_import(const char* base_url, const char* relative_path);

// Initialize HTTP module loading system
void jsrt_http_module_init(void);

// Cleanup HTTP module loading system
void jsrt_http_module_cleanup(void);

#endif