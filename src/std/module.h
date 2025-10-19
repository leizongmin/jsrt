#ifndef __JSRT_STD_MODULE_H__
#define __JSRT_STD_MODULE_H__

#include <quickjs.h>

#include "../runtime.h"

// Initialize module loading support
void JSRT_StdModuleInit(JSRT_Runtime* rt);

// Module loader function for QuickJS (legacy, renamed to avoid conflict)
JSModuleDef* JSRT_StdModuleLoader(JSContext* ctx, const char* module_name, void* opaque);

// Module normalize function for QuickJS (legacy)
char* JSRT_StdModuleNormalize(JSContext* ctx, const char* module_base_name, const char* module_name, void* opaque);

// Initialize CommonJS support (require, module.exports, exports)
void JSRT_StdCommonJSInit(JSRT_Runtime* rt);

// Set the entry module path for CommonJS require stack traces
void JSRT_StdCommonJSSetEntryPath(const char* path);

// Build message/stack strings for module-not-found errors (caller frees)
void JSRT_StdModuleBuildNotFoundStrings(const char* module_display, const char* require_display,
                                        bool include_require_section, char** message_out, char** stack_out);

// Cleanup module system with context
void JSRT_StdModuleCleanup(JSContext* ctx);

#endif
