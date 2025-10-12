/**
 * Module Specifier Parser
 *
 * Parses and categorizes module specifiers (import/require strings).
 */

#ifndef JSRT_MODULE_SPECIFIER_H
#define JSRT_MODULE_SPECIFIER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Module specifier types
 */
typedef enum {
  JSRT_SPECIFIER_BARE,      // "lodash", "react" (npm package)
  JSRT_SPECIFIER_RELATIVE,  // "./module", "../utils"
  JSRT_SPECIFIER_ABSOLUTE,  // "/path/to/module"
  JSRT_SPECIFIER_URL,       // "http://...", "https://...", "file://..."
  JSRT_SPECIFIER_BUILTIN,   // "jsrt:assert", "node:fs"
  JSRT_SPECIFIER_IMPORT     // "#internal/utils" (package imports)
} JSRT_SpecifierType;

/**
 * Parsed module specifier
 */
typedef struct {
  JSRT_SpecifierType type;  // Type of specifier
  char* value;              // The full specifier string
  char* protocol;           // Protocol if URL (http, https, file, jsrt, node)
  char* package_name;       // Package name if bare (e.g., "lodash")
  char* subpath;            // Subpath within package (e.g., "array" in "lodash/array")
} JSRT_ModuleSpecifier;

/**
 * Parse a module specifier string
 * @param specifier The specifier string to parse
 * @return Parsed specifier struct (caller must free with jsrt_specifier_free)
 * @note Returns NULL if specifier is NULL or empty
 */
JSRT_ModuleSpecifier* jsrt_parse_specifier(const char* specifier);

/**
 * Free a module specifier
 * @param spec Specifier to free
 */
void jsrt_specifier_free(JSRT_ModuleSpecifier* spec);

/**
 * Get a string representation of specifier type
 * @param type Specifier type
 * @return String name of type
 */
const char* jsrt_specifier_type_to_string(JSRT_SpecifierType type);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_MODULE_SPECIFIER_H
