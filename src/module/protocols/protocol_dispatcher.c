/**
 * Protocol Dispatcher Implementation
 */

#include "protocol_dispatcher.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../util/module_debug.h"
#include "protocol_registry.h"

/**
 * Extract protocol from URL
 */
char* jsrt_extract_protocol(const char* url) {
  if (!url) {
    return NULL;
  }

  // Find '://' pattern
  const char* separator = strstr(url, "://");
  if (!separator) {
    MODULE_DEBUG_PROTOCOL("No protocol found in URL: %s", url);
    return NULL;
  }

  // Calculate protocol length
  size_t protocol_len = separator - url;
  if (protocol_len == 0 || protocol_len > 16) {  // Reasonable protocol name limit
    MODULE_DEBUG_ERROR("Invalid protocol length in URL: %s", url);
    return NULL;
  }

  // Validate protocol characters (alphanumeric + dash)
  for (size_t i = 0; i < protocol_len; i++) {
    char c = url[i];
    if (!isalnum(c) && c != '-' && c != '+' && c != '.') {
      MODULE_DEBUG_ERROR("Invalid character in protocol: %c", c);
      return NULL;
    }
  }

  // Allocate and copy protocol name
  char* protocol = malloc(protocol_len + 1);
  if (!protocol) {
    MODULE_DEBUG_ERROR("Failed to allocate memory for protocol name");
    return NULL;
  }

  strncpy(protocol, url, protocol_len);
  protocol[protocol_len] = '\0';

  // Convert to lowercase for case-insensitive matching
  for (size_t i = 0; i < protocol_len; i++) {
    protocol[i] = tolower(protocol[i]);
  }

  MODULE_DEBUG_PROTOCOL("Extracted protocol: %s from URL: %s", protocol, url);

  return protocol;
}

/**
 * Check if URL has a protocol
 */
bool jsrt_has_protocol(const char* url) {
  if (!url) {
    return false;
  }

  return strstr(url, "://") != NULL;
}

/**
 * Load content using appropriate protocol handler
 */
JSRT_ReadFileResult jsrt_load_content_by_protocol(const char* url) {
  MODULE_DEBUG_PROTOCOL("Dispatching load request for URL: %s", url);

  JSRT_ReadFileResult result = JSRT_ReadFileResultDefault();

  if (!url) {
    MODULE_DEBUG_ERROR("NULL URL provided to dispatcher");
    result.error = JSRT_READ_FILE_ERROR_READ_ERROR;
    return result;
  }

  // Extract protocol
  char* protocol = jsrt_extract_protocol(url);

  // If no protocol, default to "file" for regular file paths
  if (!protocol) {
    MODULE_DEBUG_PROTOCOL("No protocol found, defaulting to file:// handler for: %s", url);
    protocol = strdup("file");
    if (!protocol) {
      result.error = JSRT_READ_FILE_ERROR_READ_ERROR;
      return result;
    }
  }

  // Get handler for protocol
  const JSRT_ProtocolHandler* handler = jsrt_get_protocol_handler(protocol);
  if (!handler) {
    MODULE_DEBUG_ERROR("No handler registered for protocol: %s (URL: %s)", protocol, url);
    free(protocol);
    result.error = JSRT_READ_FILE_ERROR_FILE_NOT_FOUND;
    return result;
  }

  MODULE_DEBUG_PROTOCOL("Dispatching to %s:// handler", protocol);
  free(protocol);

  // Call handler's load function
  result = handler->load(url, handler->user_data);

  if (result.error != JSRT_READ_FILE_OK) {
    MODULE_DEBUG_ERROR("Protocol handler failed to load URL %s: %s", url, JSRT_ReadFileErrorToString(result.error));
  } else {
    MODULE_DEBUG_PROTOCOL("Successfully loaded content via protocol handler (%zu bytes)", result.size);
  }

  return result;
}
