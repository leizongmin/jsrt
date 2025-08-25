#ifndef __JSRT_STD_MODULE_H__
#define __JSRT_STD_MODULE_H__

#include <quickjs.h>

#include "../runtime.h"

// Initialize module loading support
void JSRT_StdModuleInit(JSRT_Runtime *rt);

// Module loader function for QuickJS
JSModuleDef *JSRT_ModuleLoader(JSContext *ctx, const char *module_name, void *opaque);

// Module normalize function for QuickJS
char *JSRT_ModuleNormalize(JSContext *ctx, const char *module_base_name, const char *module_name, void *opaque);

// Initialize CommonJS support (require, module.exports, exports)
void JSRT_StdCommonJSInit(JSRT_Runtime *rt);

// Cleanup module system
void JSRT_StdModuleCleanup();

#endif