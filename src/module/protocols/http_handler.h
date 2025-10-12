/**
 * HTTP/HTTPS Protocol Handler
 *
 * Handles loading modules from http:// and https:// URLs.
 * Includes security validation and content cleaning.
 */

#ifndef __JSRT_MODULE_HTTP_HANDLER_H__
#define __JSRT_MODULE_HTTP_HANDLER_H__

#include "../../util/file.h"

/**
 * Initialize HTTP/HTTPS protocol handlers
 *
 * Registers both http:// and https:// protocol handlers with the protocol registry.
 * Must be called after jsrt_init_protocol_handlers().
 *
 * Note: Uses the unified module cache from Phase 1, NOT a separate HTTP cache.
 */
void jsrt_http_handler_init(void);

/**
 * Cleanup HTTP/HTTPS protocol handlers
 *
 * Unregisters both http:// and https:// protocol handlers.
 * Called automatically by jsrt_cleanup_protocol_handlers().
 */
void jsrt_http_handler_cleanup(void);

/**
 * Load function for http:// and https:// protocols
 *
 * Downloads content from HTTP/HTTPS URLs with:
 *   - Security validation (whitelist checking)
 *   - Content cleaning (BOM removal, line ending normalization)
 *   - Proper error handling
 *
 * @param url Full http:// or https:// URL
 * @param user_data Custom data (unused for HTTP handler)
 * @return JSRT_ReadFileResult with downloaded content or error
 *
 * Note: This function performs the download and cleaning, but does NOT cache.
 * Caching should be handled by the caller using the unified module cache.
 */
JSRT_ReadFileResult jsrt_http_handler_load(const char* url, void* user_data);

#endif  // __JSRT_MODULE_HTTP_HANDLER_H__
