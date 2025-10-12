/**
 * File Protocol Handler
 *
 * Handles loading modules from file:// URLs.
 * Supports both absolute and relative file paths.
 */

#ifndef __JSRT_MODULE_FILE_HANDLER_H__
#define __JSRT_MODULE_FILE_HANDLER_H__

#include "../../util/file.h"

/**
 * Initialize file protocol handler
 *
 * Registers the file:// protocol handler with the protocol registry.
 * Must be called after jsrt_init_protocol_handlers().
 */
void jsrt_file_handler_init(void);

/**
 * Cleanup file protocol handler
 *
 * Unregisters the file:// protocol handler.
 * Called automatically by jsrt_cleanup_protocol_handlers().
 */
void jsrt_file_handler_cleanup(void);

/**
 * Load function for file:// protocol
 *
 * Handles file:// URLs and converts them to filesystem paths.
 * Supports:
 *   - file:///absolute/path (3 slashes - standard)
 *   - file://path (2 slashes - non-standard but handled)
 *   - Proper URL decoding
 *
 * @param url Full file:// URL
 * @param user_data Custom data (unused for file handler)
 * @return JSRT_ReadFileResult with file content or error
 */
JSRT_ReadFileResult jsrt_file_handler_load(const char* url, void* user_data);

#endif  // __JSRT_MODULE_FILE_HANDLER_H__
