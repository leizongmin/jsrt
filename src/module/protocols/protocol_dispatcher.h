/**
 * Protocol Dispatcher
 *
 * Dispatches module loading requests to appropriate protocol handlers.
 * Extracts protocol from URL and invokes the registered handler.
 */

#ifndef __JSRT_MODULE_PROTOCOL_DISPATCHER_H__
#define __JSRT_MODULE_PROTOCOL_DISPATCHER_H__

#include <stdbool.h>
#include "../../util/file.h"

/**
 * Load content using appropriate protocol handler
 *
 * Extracts the protocol from the URL (part before first ':') and
 * dispatches to the registered handler for that protocol.
 *
 * Supported protocols (when handlers are registered):
 *   - file:// - Local filesystem
 *   - http:// - HTTP downloads
 *   - https:// - HTTPS downloads
 *   - zip:// - ZIP archives (future)
 *
 * @param url Full URL with protocol (e.g., "file:///path/to/file.js")
 * @return JSRT_ReadFileResult with content or error
 *
 * Error cases:
 *   - JSRT_READ_FILE_ERROR_FILE_NOT_FOUND: No handler for protocol
 *   - JSRT_READ_FILE_ERROR_READ_ERROR: Handler returned error
 *   - Other errors from specific protocol handlers
 *
 * Example:
 *   JSRT_ReadFileResult result = jsrt_load_content_by_protocol("file:///app/main.js");
 *   if (result.error == JSRT_READ_FILE_OK) {
 *     // Use result.data and result.size
 *     JSRT_ReadFileResultFree(&result);
 *   }
 */
JSRT_ReadFileResult jsrt_load_content_by_protocol(const char* url);

/**
 * Extract protocol from URL
 *
 * Returns the protocol part of a URL (before the first ':').
 * Returns NULL if URL is invalid or has no protocol.
 *
 * @param url URL to parse
 * @return Allocated string with protocol name (caller must free), or NULL
 *
 * Examples:
 *   "file:///path" -> "file"
 *   "http://example.com" -> "http"
 *   "https://example.com/file.js" -> "https"
 *   "/path/to/file" -> NULL (no protocol)
 */
char* jsrt_extract_protocol(const char* url);

/**
 * Check if URL has a protocol
 *
 * @param url URL to check
 * @return true if URL contains a protocol (has '://' pattern), false otherwise
 */
bool jsrt_has_protocol(const char* url);

// Note: Load hook utilities are only implemented in the .c file
// to avoid circular dependencies with module API headers

#endif  // __JSRT_MODULE_PROTOCOL_DISPATCHER_H__
