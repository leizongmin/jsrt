/**
 * ZIP Protocol Handler (Future Implementation)
 *
 * This is a placeholder for future zip:// protocol support.
 * The zip:// protocol will allow loading modules from ZIP archives.
 *
 * URL Format:
 *   zip:///path/to/archive.zip#/path/inside/archive
 *
 * Examples:
 *   zip:///opt/myapp/modules.zip#/lib/utils.js
 *   zip://./local.zip#/index.js
 *
 * Implementation Requirements:
 *   1. Parse ZIP URL format (archive path + internal path)
 *   2. Extract and cache internal path from ZIP archive
 *   3. Handle ZIP archive validation and security
 *   4. Support both absolute and relative archive paths
 *   5. Efficient caching to avoid repeated extraction
 *
 * Handler Interface:
 *   - load(url, user_data) -> JSRT_ReadFileResult
 *     * Parse URL to extract archive path and internal path
 *     * Open ZIP archive (with caching)
 *     * Extract requested file from archive
 *     * Return file content or error
 *   - cleanup(user_data) -> void
 *     * Close cached ZIP archives
 *     * Free extraction buffers
 *
 * Security Considerations:
 *   - Validate ZIP archive integrity
 *   - Prevent path traversal attacks (../ in internal paths)
 *   - Limit extraction size to prevent zip bombs
 *   - Consider archive signature verification
 *
 * Dependencies:
 *   - Will likely use deps/zlib for ZIP support
 *   - Or integrate minizip library
 *
 * Phase Implementation:
 *   - Phase 4: Design only (this header)
 *   - Phase 6+: Full implementation
 *
 * TODO:
 *   - [ ] Research best ZIP library for integration
 *   - [ ] Design archive caching strategy
 *   - [ ] Implement ZIP file extraction
 *   - [ ] Add security validation
 *   - [ ] Write comprehensive tests
 */

#ifndef __JSRT_MODULE_ZIP_HANDLER_H__
#define __JSRT_MODULE_ZIP_HANDLER_H__

#include "../../util/file.h"

// Forward declarations for future implementation

/**
 * Initialize ZIP protocol handler (NOT IMPLEMENTED)
 *
 * This is a placeholder for future implementation.
 * Will register the zip:// protocol handler.
 */
void jsrt_zip_handler_init(void);

/**
 * Cleanup ZIP protocol handler (NOT IMPLEMENTED)
 *
 * This is a placeholder for future implementation.
 * Will unregister the zip:// protocol handler and free resources.
 */
void jsrt_zip_handler_cleanup(void);

/**
 * Load function for zip:// protocol (NOT IMPLEMENTED)
 *
 * This is a placeholder for future implementation.
 *
 * @param url ZIP URL (format: zip:///path/to/archive.zip#/internal/path)
 * @param user_data Custom data
 * @return JSRT_ReadFileResult with extracted content or error
 */
JSRT_ReadFileResult jsrt_zip_handler_load(const char* url, void* user_data);

#endif  // __JSRT_MODULE_ZIP_HANDLER_H__
