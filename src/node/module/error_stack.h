#ifndef __JSRT_NODE_MODULE_ERROR_STACK_H__
#define __JSRT_NODE_MODULE_ERROR_STACK_H__

#include <quickjs.h>
#include <stdbool.h>

#include "sourcemap.h"

/**
 * @file error_stack.h
 * @brief Error stack trace integration with source maps
 *
 * Provides stack trace transformation using source maps to show
 * original source locations instead of generated code positions.
 *
 * Implements:
 * - Stack trace parsing and transformation
 * - Source map lookup and position mapping
 * - Configuration-aware filtering (nodeModules, generatedCode)
 * - Error.stack property override
 */

/**
 * Initialize error stack integration
 * Installs custom Error.stack getter that transforms stack traces
 * using source maps when available.
 *
 * @param ctx QuickJS context
 * @param cache Source map cache
 * @return true on success, false on error
 */
bool jsrt_error_stack_init(JSContext* ctx, JSRT_SourceMapCache* cache);

/**
 * Transform error stack trace using source maps
 * Parses the original stack trace, looks up source maps for each frame,
 * and replaces generated positions with original source positions.
 *
 * @param ctx QuickJS context
 * @param cache Source map cache
 * @param original_stack Original stack trace string
 * @return Transformed stack trace or original if source maps disabled
 */
JSValue jsrt_transform_error_stack(JSContext* ctx, JSRT_SourceMapCache* cache, const char* original_stack);

#endif  // __JSRT_NODE_MODULE_ERROR_STACK_H__
