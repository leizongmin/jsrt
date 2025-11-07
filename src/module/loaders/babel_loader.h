/**
 * Babel Loader Header
 */

#ifndef JSRT_BABEL_LOADER_H
#define JSRT_BABEL_LOADER_H

#include "../../runtime.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if this is a babel package that needs special handling
 */
bool jsrt_is_babel_package(const char* resolved_path);

/**
 * Create enhanced wrapper code that handles babel packages specially
 */
char* jsrt_create_enhanced_wrapper_code(const char* content, const char* resolved_path);

/**
 * Load babel package with special handling
 */
JSValue jsrt_load_babel_module(JSContext* ctx, JSRT_ModuleLoader* loader, const char* resolved_path,
                               const char* specifier);

#ifdef __cplusplus
}
#endif

#endif /* JSRT_BABEL_LOADER_H */