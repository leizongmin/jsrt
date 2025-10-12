/**
 * Module Specifier Parser Implementation
 */

#include "specifier.h"

#include <stdlib.h>
#include <string.h>

#include "../util/module_debug.h"
#include "path_util.h"

// Helper to check if string starts with prefix
static bool starts_with(const char* str, const char* prefix) {
  if (!str || !prefix)
    return false;
  return strncmp(str, prefix, strlen(prefix)) == 0;
}

// Helper to extract protocol from URL (returns NULL if not a URL)
static char* extract_protocol(const char* specifier) {
  if (!specifier)
    return NULL;

  // Look for "://" pattern
  const char* protocol_end = strstr(specifier, "://");
  if (!protocol_end)
    return NULL;

  size_t protocol_len = protocol_end - specifier;
  char* protocol = (char*)malloc(protocol_len + 1);
  if (!protocol)
    return NULL;

  strncpy(protocol, specifier, protocol_len);
  protocol[protocol_len] = '\0';

  return protocol;
}

// Helper to extract package name and subpath from bare specifier
static void extract_package_parts(const char* specifier, char** package_name, char** subpath) {
  if (!specifier || !package_name || !subpath) {
    if (package_name)
      *package_name = NULL;
    if (subpath)
      *subpath = NULL;
    return;
  }

  // Handle scoped packages (@org/package or @org/package/subpath)
  if (specifier[0] == '@') {
    // Find second slash (after org name)
    const char* first_slash = strchr(specifier + 1, '/');
    if (!first_slash) {
      // Just "@org" - invalid but treat as package name
      *package_name = strdup(specifier);
      *subpath = NULL;
      return;
    }

    const char* second_slash = strchr(first_slash + 1, '/');
    if (!second_slash) {
      // "@org/package" - no subpath
      *package_name = strdup(specifier);
      *subpath = NULL;
    } else {
      // "@org/package/subpath"
      size_t pkg_len = second_slash - specifier;
      *package_name = (char*)malloc(pkg_len + 1);
      if (*package_name) {
        strncpy(*package_name, specifier, pkg_len);
        (*package_name)[pkg_len] = '\0';
      }
      *subpath = strdup(second_slash + 1);
    }
  } else {
    // Regular package (package or package/subpath)
    const char* slash = strchr(specifier, '/');
    if (!slash) {
      // Just "package" - no subpath
      *package_name = strdup(specifier);
      *subpath = NULL;
    } else {
      // "package/subpath"
      size_t pkg_len = slash - specifier;
      *package_name = (char*)malloc(pkg_len + 1);
      if (*package_name) {
        strncpy(*package_name, specifier, pkg_len);
        (*package_name)[pkg_len] = '\0';
      }
      *subpath = strdup(slash + 1);
    }
  }
}

JSRT_ModuleSpecifier* jsrt_parse_specifier(const char* specifier) {
  if (!specifier || specifier[0] == '\0') {
    MODULE_DEBUG_RESOLVER("Cannot parse NULL or empty specifier");
    return NULL;
  }

  MODULE_DEBUG_RESOLVER("Parsing specifier: '%s'", specifier);

  JSRT_ModuleSpecifier* spec = (JSRT_ModuleSpecifier*)calloc(1, sizeof(JSRT_ModuleSpecifier));
  if (!spec)
    return NULL;

  spec->value = strdup(specifier);
  if (!spec->value) {
    free(spec);
    return NULL;
  }

  // Check for package imports (#internal/utils)
  if (specifier[0] == '#') {
    spec->type = JSRT_SPECIFIER_IMPORT;
    MODULE_DEBUG_RESOLVER("Detected IMPORT specifier");
    return spec;
  }

  // Check for builtin specifiers (jsrt:, node:)
  if (starts_with(specifier, "jsrt:") || starts_with(specifier, "node:")) {
    spec->type = JSRT_SPECIFIER_BUILTIN;
    const char* colon = strchr(specifier, ':');
    if (colon) {
      size_t protocol_len = colon - specifier;
      spec->protocol = (char*)malloc(protocol_len + 1);
      if (spec->protocol) {
        strncpy(spec->protocol, specifier, protocol_len);
        spec->protocol[protocol_len] = '\0';
      }
    }
    MODULE_DEBUG_RESOLVER("Detected BUILTIN specifier with protocol '%s'", spec->protocol ? spec->protocol : "NULL");
    return spec;
  }

  // Check for URL specifiers (http://, https://, file://)
  spec->protocol = extract_protocol(specifier);
  if (spec->protocol) {
    spec->type = JSRT_SPECIFIER_URL;
    MODULE_DEBUG_RESOLVER("Detected URL specifier with protocol '%s'", spec->protocol);
    return spec;
  }

  // Check for absolute paths
  if (jsrt_is_absolute_path(specifier)) {
    spec->type = JSRT_SPECIFIER_ABSOLUTE;
    MODULE_DEBUG_RESOLVER("Detected ABSOLUTE specifier");
    return spec;
  }

  // Check for relative paths (./ or ../)
  if (jsrt_is_relative_path(specifier)) {
    spec->type = JSRT_SPECIFIER_RELATIVE;
    MODULE_DEBUG_RESOLVER("Detected RELATIVE specifier");
    return spec;
  }

  // Everything else is a bare specifier (npm package)
  spec->type = JSRT_SPECIFIER_BARE;
  extract_package_parts(specifier, &spec->package_name, &spec->subpath);
  MODULE_DEBUG_RESOLVER("Detected BARE specifier: package='%s', subpath='%s'",
                        spec->package_name ? spec->package_name : "NULL", spec->subpath ? spec->subpath : "NULL");

  return spec;
}

void jsrt_specifier_free(JSRT_ModuleSpecifier* spec) {
  if (!spec)
    return;

  free(spec->value);
  free(spec->protocol);
  free(spec->package_name);
  free(spec->subpath);
  free(spec);
}

const char* jsrt_specifier_type_to_string(JSRT_SpecifierType type) {
  switch (type) {
    case JSRT_SPECIFIER_BARE:
      return "BARE";
    case JSRT_SPECIFIER_RELATIVE:
      return "RELATIVE";
    case JSRT_SPECIFIER_ABSOLUTE:
      return "ABSOLUTE";
    case JSRT_SPECIFIER_URL:
      return "URL";
    case JSRT_SPECIFIER_BUILTIN:
      return "BUILTIN";
    case JSRT_SPECIFIER_IMPORT:
      return "IMPORT";
    default:
      return "UNKNOWN";
  }
}
