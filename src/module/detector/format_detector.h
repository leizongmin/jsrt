/**
 * Module Format Detector
 *
 * Detects the format of JavaScript modules (CommonJS, ESM, JSON)
 * based on file extension, package.json, and content analysis.
 */

#ifndef JSRT_MODULE_FORMAT_DETECTOR_H
#define JSRT_MODULE_FORMAT_DETECTOR_H

#include <quickjs.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Module format types
 */
typedef enum {
  JSRT_MODULE_FORMAT_UNKNOWN = 0,   // Format cannot be determined
  JSRT_MODULE_FORMAT_COMMONJS = 1,  // CommonJS (require/module.exports)
  JSRT_MODULE_FORMAT_ESM = 2,       // ES Modules (import/export)
  JSRT_MODULE_FORMAT_JSON = 3       // JSON module
} JSRT_ModuleFormat;

/**
 * Detect format based on file extension
 *
 * @param path File path to check
 * @return Format based on extension:
 *         - .cjs → COMMONJS
 *         - .mjs → ESM
 *         - .json → JSON
 *         - .js → UNKNOWN (needs further detection)
 *         - no extension → UNKNOWN
 *         - other → UNKNOWN
 */
JSRT_ModuleFormat jsrt_detect_format_by_extension(const char* path);

/**
 * Detect format using package.json "type" field
 * Walks up directory tree to find nearest package.json
 *
 * @param ctx JavaScript context
 * @param path File path to check
 * @return Format based on package.json:
 *         - type: "module" → ESM
 *         - type: "commonjs" → COMMONJS
 *         - no type field → UNKNOWN
 *         - no package.json → UNKNOWN
 */
JSRT_ModuleFormat jsrt_detect_format_by_package(JSContext* ctx, const char* path);

/**
 * Main format detection function
 * Combines extension, package.json, and content analysis
 *
 * Detection priority (Node.js compatible):
 * 1. Extension first: .cjs/.mjs/.json → return immediately
 * 2. Package.json second: .js files check package.json "type"
 * 3. Content analysis last: analyze file content for hints
 * 4. Default: COMMONJS (Node.js default for .js)
 *
 * @param ctx JavaScript context
 * @param path File path to check
 * @param content File content (can be NULL, will be loaded if needed)
 * @param content_length Content length (0 if content is NULL)
 * @return Detected module format
 */
JSRT_ModuleFormat jsrt_detect_module_format(JSContext* ctx, const char* path, const char* content,
                                            size_t content_length);

/**
 * Convert format enum to human-readable string
 *
 * @param format Module format
 * @return String representation ("unknown", "commonjs", "esm", "json")
 */
const char* jsrt_module_format_to_string(JSRT_ModuleFormat format);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_FORMAT_DETECTOR_H
