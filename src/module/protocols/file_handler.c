/**
 * File Protocol Handler Implementation
 */

#include "file_handler.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "../util/module_debug.h"
#include "protocol_registry.h"

// Forward declaration
static JSRT_ReadFileResult file_handler_load_impl(const char* url, void* user_data);

/**
 * Convert hex character to integer
 */
static int hex_to_int(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

/**
 * URL decode a string in place
 *
 * Converts %XX sequences to their character equivalents.
 * Returns the new length of the string.
 */
static size_t url_decode(char* str) {
  if (!str)
    return 0;

  char* src = str;
  char* dst = str;

  while (*src) {
    if (*src == '%' && src[1] && src[2]) {
      int hi = hex_to_int(src[1]);
      int lo = hex_to_int(src[2]);

      if (hi >= 0 && lo >= 0) {
        *dst++ = (char)((hi << 4) | lo);
        src += 3;
        continue;
      }
    }

    *dst++ = *src++;
  }

  *dst = '\0';
  return dst - str;
}

/**
 * Parse file:// URL to filesystem path
 *
 * Handles:
 *   - file:///absolute/path (standard - 3 slashes)
 *   - file://path (non-standard - 2 slashes)
 *   - URL decoding
 *
 * Returns allocated string with filesystem path, or NULL on error.
 * Caller must free the returned string.
 */
static char* parse_file_url(const char* url) {
  if (!url) {
    MODULE_Debug_Error("NULL URL provided to parse_file_url");
    return NULL;
  }

  // Check for file:// prefix
  if (strncmp(url, "file://", 7) != 0) {
    MODULE_Debug_Error("Invalid file URL (missing file:// prefix): %s", url);
    return NULL;
  }

  const char* path_start = url + 7;  // Skip "file://"

  // Handle file:///absolute/path (3 slashes total)
  // The path_start now points after "file://", so if it starts with "/",
  // we have "file:///" which is the standard format for absolute paths
  if (*path_start == '/') {
    // Standard format: file:///path -> /path
    path_start++;  // Skip the extra slash
  }
  // For file://path (2 slashes), path_start already points to "path"

  // Allocate buffer for decoded path
  size_t path_len = strlen(path_start);
  char* path = malloc(path_len + 1);
  if (!path) {
    MODULE_Debug_Error("Failed to allocate memory for file path");
    return NULL;
  }

  // Copy and decode URL
  strcpy(path, path_start);
  url_decode(path);

  MODULE_Debug_Protocol("Parsed file URL: %s -> %s", url, path);

  return path;
}

/**
 * Load function for file:// protocol
 */
JSRT_ReadFileResult jsrt_file_handler_load(const char* url, void* user_data) {
  (void)user_data;  // Unused

  MODULE_Debug_Protocol("Loading from file URL: %s", url);

  JSRT_ReadFileResult result = JSRT_ReadFileResultDefault();

  // Parse URL to filesystem path
  char* path = parse_file_url(url);
  if (!path) {
    result.error = JSRT_READ_FILE_ERROR_FILE_NOT_FOUND;
    MODULE_Debug_Error("Failed to parse file URL: %s", url);
    return result;
  }

  // Read file using existing file I/O
  result = JSRT_ReadFile(path);

  if (result.error != JSRT_READ_FILE_OK) {
    MODULE_Debug_Error("Failed to read file from URL %s (path: %s): %s", url, path,
                       JSRT_ReadFileErrorToString(result.error));
  } else {
    MODULE_Debug_Protocol("Successfully loaded file from URL: %s (%zu bytes)", url, result.size);
  }

  free(path);
  return result;
}

/**
 * Initialize file protocol handler
 */
void jsrt_file_handler_init(void) {
  MODULE_Debug_Protocol("Initializing file:// protocol handler");

  JSRT_ProtocolHandler handler = {
      .protocol_name = "file", .load = jsrt_file_handler_load, .cleanup = NULL, .user_data = NULL};

  if (!jsrt_register_protocol_handler("file", &handler)) {
    MODULE_Debug_Error("Failed to register file:// protocol handler");
    return;
  }

  MODULE_Debug_Protocol("file:// protocol handler registered successfully");
}

/**
 * Cleanup file protocol handler
 */
void jsrt_file_handler_cleanup(void) {
  MODULE_Debug_Protocol("Cleaning up file:// protocol handler");
  jsrt_unregister_protocol_handler("file");
}
