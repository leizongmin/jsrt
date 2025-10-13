/**
 * Module Format Detector - Implementation
 */

#include "format_detector.h"
#include <stdlib.h>
#include <string.h>
#include "../resolver/package_json.h"
#include "../util/module_debug.h"
#include "../util/module_errors.h"
#include "content_analyzer.h"

/**
 * Get file extension from path
 * Returns pointer to extension (including dot) or NULL if no extension
 */
static const char* get_file_extension(const char* path) {
  if (!path) {
    return NULL;
  }

  // Find last dot and last path separator
  const char* last_dot = strrchr(path, '.');
  const char* last_sep = strrchr(path, '/');

#ifdef _WIN32
  const char* last_backslash = strrchr(path, '\\');
  if (last_backslash && (!last_sep || last_backslash > last_sep)) {
    last_sep = last_backslash;
  }
#endif

  // Extension must be after path separator (if any)
  if (last_dot && (!last_sep || last_dot > last_sep)) {
    return last_dot;
  }

  return NULL;
}

/**
 * Detect format based on file extension
 */
JSRT_ModuleFormat jsrt_detect_format_by_extension(const char* path) {
  if (!path) {
    MODULE_DEBUG_DETECTOR("Path is NULL");
    return JSRT_MODULE_FORMAT_UNKNOWN;
  }

  const char* ext = get_file_extension(path);
  if (!ext) {
    MODULE_DEBUG_DETECTOR("No extension found in path: %s", path);
    return JSRT_MODULE_FORMAT_UNKNOWN;
  }

  MODULE_DEBUG_DETECTOR("Extension detected: %s for path: %s", ext, path);

  // Check known extensions
  if (strcmp(ext, ".cjs") == 0) {
    MODULE_DEBUG_DETECTOR("Format: CommonJS (.cjs)");
    return JSRT_MODULE_FORMAT_COMMONJS;
  }

  if (strcmp(ext, ".mjs") == 0) {
    MODULE_DEBUG_DETECTOR("Format: ESM (.mjs)");
    return JSRT_MODULE_FORMAT_ESM;
  }

  if (strcmp(ext, ".json") == 0) {
    MODULE_DEBUG_DETECTOR("Format: JSON (.json)");
    return JSRT_MODULE_FORMAT_JSON;
  }

  if (strcmp(ext, ".js") == 0) {
    MODULE_DEBUG_DETECTOR("Format: Unknown (.js - needs further detection)");
    return JSRT_MODULE_FORMAT_UNKNOWN;
  }

  // Unknown extension
  MODULE_DEBUG_DETECTOR("Format: Unknown (unrecognized extension: %s)", ext);
  return JSRT_MODULE_FORMAT_UNKNOWN;
}

/**
 * Get directory path from file path
 * Returns newly allocated string (caller must free)
 */
static char* get_directory_path(const char* path) {
  if (!path) {
    return NULL;
  }

  // Find last path separator
  const char* last_sep = strrchr(path, '/');

#ifdef _WIN32
  const char* last_backslash = strrchr(path, '\\');
  if (last_backslash && (!last_sep || last_backslash > last_sep)) {
    last_sep = last_backslash;
  }
#endif

  if (!last_sep) {
    // No separator, use current directory
    return strdup(".");
  }

  // Extract directory part
  size_t dir_len = last_sep - path;
  if (dir_len == 0) {
    // Path starts with separator (e.g., "/file.js")
    return strdup("/");
  }

  char* dir = malloc(dir_len + 1);
  if (!dir) {
    return NULL;
  }

  memcpy(dir, path, dir_len);
  dir[dir_len] = '\0';
  return dir;
}

/**
 * Detect format using package.json "type" field
 */
JSRT_ModuleFormat jsrt_detect_format_by_package(JSContext* ctx, const char* path) {
  if (!ctx || !path) {
    MODULE_DEBUG_DETECTOR("Invalid arguments (ctx=%p, path=%p)", ctx, path);
    return JSRT_MODULE_FORMAT_UNKNOWN;
  }

  // Get directory path
  char* dir_path = get_directory_path(path);
  if (!dir_path) {
    MODULE_DEBUG_DETECTOR("Failed to get directory path for: %s", path);
    return JSRT_MODULE_FORMAT_UNKNOWN;
  }

  MODULE_DEBUG_DETECTOR("Looking for package.json from: %s", dir_path);

  // Parse package.json (walks up tree automatically)
  JSRT_PackageJson* pkg = jsrt_parse_package_json(ctx, dir_path);
  free(dir_path);

  if (!pkg) {
    MODULE_DEBUG_DETECTOR("No package.json found for: %s", path);
    return JSRT_MODULE_FORMAT_UNKNOWN;
  }

  // Check type field
  JSRT_ModuleFormat format = JSRT_MODULE_FORMAT_UNKNOWN;

  if (pkg->type) {
    if (strcmp(pkg->type, "module") == 0) {
      MODULE_DEBUG_DETECTOR("package.json type: module -> ESM");
      format = JSRT_MODULE_FORMAT_ESM;
    } else if (strcmp(pkg->type, "commonjs") == 0) {
      MODULE_DEBUG_DETECTOR("package.json type: commonjs -> CommonJS");
      format = JSRT_MODULE_FORMAT_COMMONJS;
    } else {
      MODULE_DEBUG_DETECTOR("package.json type: unknown value '%s'", pkg->type);
    }
  } else {
    MODULE_DEBUG_DETECTOR("package.json has no type field");
  }

  jsrt_package_json_free(pkg);
  return format;
}

/**
 * Main format detection function
 */
JSRT_ModuleFormat jsrt_detect_module_format(JSContext* ctx, const char* path, const char* content,
                                            size_t content_length) {
  if (!path) {
    MODULE_DEBUG_ERROR("Path is NULL");
    return JSRT_MODULE_FORMAT_UNKNOWN;
  }

  MODULE_DEBUG_DETECTOR("Detecting format for: %s", path);

  // Step 1: Check file extension first
  JSRT_ModuleFormat ext_format = jsrt_detect_format_by_extension(path);

  // If extension is definitive (.cjs, .mjs, .json), return immediately
  if (ext_format == JSRT_MODULE_FORMAT_COMMONJS || ext_format == JSRT_MODULE_FORMAT_ESM ||
      ext_format == JSRT_MODULE_FORMAT_JSON) {
    MODULE_DEBUG_DETECTOR("Definitive format from extension: %s", jsrt_module_format_to_string(ext_format));
    return ext_format;
  }

  // Step 2: For .js files or extensionless, check package.json
  if (ctx) {
    JSRT_ModuleFormat pkg_format = jsrt_detect_format_by_package(ctx, path);
    if (pkg_format != JSRT_MODULE_FORMAT_UNKNOWN) {
      MODULE_DEBUG_DETECTOR("Format from package.json: %s", jsrt_module_format_to_string(pkg_format));
      return pkg_format;
    }
  }

  // Step 3: If we have content, analyze it
  if (content && content_length > 0) {
    JSRT_ModuleFormat content_format = jsrt_analyze_content_format(content, content_length);
    if (content_format != JSRT_MODULE_FORMAT_UNKNOWN) {
      MODULE_DEBUG_DETECTOR("Format from content analysis: %s", jsrt_module_format_to_string(content_format));
      return content_format;
    }
  }

  // Step 4: Default to CommonJS (Node.js behavior for .js files)
  MODULE_DEBUG_DETECTOR("No format detected, defaulting to CommonJS");
  return JSRT_MODULE_FORMAT_COMMONJS;
}

/**
 * Convert format enum to string
 */
const char* jsrt_module_format_to_string(JSRT_ModuleFormat format) {
  switch (format) {
    case JSRT_MODULE_FORMAT_UNKNOWN:
      return "unknown";
    case JSRT_MODULE_FORMAT_COMMONJS:
      return "commonjs";
    case JSRT_MODULE_FORMAT_ESM:
      return "esm";
    case JSRT_MODULE_FORMAT_JSON:
      return "json";
    default:
      return "invalid";
  }
}
