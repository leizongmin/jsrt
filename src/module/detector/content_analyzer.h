/**
 * Module Content Analyzer
 *
 * Analyzes JavaScript source code content to detect module format
 * based on keywords and patterns (import/export for ESM, require/module.exports for CommonJS).
 */

#ifndef JSRT_MODULE_CONTENT_ANALYZER_H
#define JSRT_MODULE_CONTENT_ANALYZER_H

#include <stddef.h>
#include "format_detector.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Analyze file content to detect module format
 *
 * Performs lexical analysis to detect:
 * - ESM patterns: "import", "export" at statement level
 * - CommonJS patterns: "require(", "module.exports", "exports."
 *
 * The analyzer:
 * - Skips string literals (single/double/template quotes)
 * - Skips comments (// and /* *\/)
 * - Looks for keywords at statement boundaries
 * - Prefers ESM if both ESM and CommonJS patterns detected
 *
 * @param content File content to analyze
 * @param length Content length in bytes
 * @return Detected format:
 *         - ESM if import/export found (even if require also present)
 *         - COMMONJS if only require/module.exports found
 *         - UNKNOWN if no clear patterns found
 */
JSRT_ModuleFormat jsrt_analyze_content_format(const char* content, size_t length);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_CONTENT_ANALYZER_H
