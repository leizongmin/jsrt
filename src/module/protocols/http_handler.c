/**
 * HTTP/HTTPS Protocol Handler Implementation
 *
 * Extracted from src/http/module_loader.c for Phase 4 refactoring.
 * Handles downloading and cleaning content from HTTP/HTTPS URLs.
 */

#include "http_handler.h"
#include <stdlib.h>
#include <string.h>
#include "../../http/security.h"
#include "../../util/http_client.h"
#include "../util/module_debug.h"
#include "protocol_registry.h"

/**
 * Clean HTTP response content for JavaScript parsing
 *
 * Performs:
 *   - UTF-8 BOM removal
 *   - Line ending normalization (CRLF -> LF)
 *   - Null byte and control character removal
 *
 * Returns allocated string with cleaned content and sets cleaned_len.
 * Caller must free the returned string.
 */
static char* clean_js_content(const char* source, size_t source_len, size_t* cleaned_len) {
  if (!source || source_len == 0) {
    *cleaned_len = 0;
    return NULL;
  }

  const char* start = source;
  size_t len = source_len;

  // Skip UTF-8 BOM if present (0xEF 0xBB 0xBF)
  if (len >= 3 && (unsigned char)start[0] == 0xEF && (unsigned char)start[1] == 0xBB &&
      (unsigned char)start[2] == 0xBF) {
    start += 3;
    len -= 3;
    MODULE_DEBUG_PROTOCOL("Removed UTF-8 BOM from content");
  }

  // Allocate cleaned content buffer
  char* cleaned = malloc(len + 1);
  if (!cleaned) {
    MODULE_DEBUG_ERROR("Failed to allocate memory for cleaned content");
    *cleaned_len = 0;
    return NULL;
  }

  // Copy content, normalizing line endings and removing problematic characters
  size_t write_pos = 0;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)start[i];

    // Skip null bytes and other problematic control characters
    if (c == 0 || (c < 32 && c != '\t' && c != '\n' && c != '\r')) {
      continue;
    }

    // Normalize Windows line endings to Unix
    if (c == '\r' && i + 1 < len && start[i + 1] == '\n') {
      cleaned[write_pos++] = '\n';
      i++;  // Skip the \n
    } else if (c == '\r') {
      cleaned[write_pos++] = '\n';
    } else if (c >= 32 || c == '\t' || c == '\n') {
      // Only allow printable characters, tabs, and newlines
      cleaned[write_pos++] = c;
    }
  }

  cleaned[write_pos] = '\0';
  *cleaned_len = write_pos;

  MODULE_DEBUG_PROTOCOL("Cleaned content: %zu bytes -> %zu bytes", source_len, write_pos);

  return cleaned;
}

/**
 * Load function for http:// and https:// protocols
 */
JSRT_ReadFileResult jsrt_http_handler_load(const char* url, void* user_data) {
  (void)user_data;  // Unused

  MODULE_DEBUG_PROTOCOL("Loading from HTTP URL: %s", url);

  JSRT_ReadFileResult result = JSRT_ReadFileResultDefault();

  if (!url) {
    MODULE_DEBUG_ERROR("NULL URL provided to HTTP handler");
    result.error = JSRT_READ_FILE_ERROR_READ_ERROR;
    return result;
  }

  // Validate URL security
  JSRT_HttpSecurityResult security_result = jsrt_http_validate_url(url);
  if (security_result != JSRT_HTTP_SECURITY_OK) {
    const char* error_msg = "Security validation failed";
    switch (security_result) {
      case JSRT_HTTP_SECURITY_PROTOCOL_FORBIDDEN:
        error_msg = "HTTP module loading is disabled or protocol not allowed";
        break;
      case JSRT_HTTP_SECURITY_DOMAIN_NOT_ALLOWED:
        error_msg = "Domain not in allowlist";
        break;
      case JSRT_HTTP_SECURITY_INVALID_URL:
        error_msg = "Invalid URL format";
        break;
      default:
        break;
    }
    MODULE_DEBUG_ERROR("HTTP security validation failed for %s: %s", url, error_msg);
    result.error = JSRT_READ_FILE_ERROR_READ_ERROR;
    return result;
  }

  // Download content
  MODULE_DEBUG_PROTOCOL("Downloading from %s", url);
  JSRT_HttpResponse response = JSRT_HttpGetWithOptions(url, "jsrt/1.0", 30000);

  if (response.error != JSRT_HTTP_OK || response.status != 200) {
    MODULE_DEBUG_ERROR("HTTP request failed for %s: error=%d, status=%d", url, response.error, response.status);
    JSRT_HttpResponseFree(&response);
    result.error = JSRT_READ_FILE_ERROR_READ_ERROR;
    return result;
  }

  // Validate response content
  JSRT_HttpSecurityResult content_result =
      jsrt_http_validate_response_content(response.content_type, response.body_size);
  if (content_result != JSRT_HTTP_SECURITY_OK) {
    const char* error_msg = "Content validation failed";
    if (content_result == JSRT_HTTP_SECURITY_SIZE_TOO_LARGE) {
      error_msg = "Content too large";
    } else if (content_result == JSRT_HTTP_SECURITY_CONTENT_TYPE_INVALID) {
      error_msg = "Invalid content type";
    }
    MODULE_DEBUG_ERROR("HTTP content validation failed for %s: %s", url, error_msg);
    JSRT_HttpResponseFree(&response);
    result.error = JSRT_READ_FILE_ERROR_READ_ERROR;
    return result;
  }

  // Clean content
  size_t cleaned_len;
  char* cleaned_content = clean_js_content(response.body, response.body_size, &cleaned_len);

  JSRT_HttpResponseFree(&response);

  if (!cleaned_content) {
    MODULE_DEBUG_ERROR("Failed to clean HTTP content from %s", url);
    result.error = JSRT_READ_FILE_ERROR_OUT_OF_MEMORY;
    return result;
  }

  // Success
  result.error = JSRT_READ_FILE_OK;
  result.data = cleaned_content;
  result.size = cleaned_len;

  MODULE_DEBUG_PROTOCOL("Successfully loaded HTTP content from %s (%zu bytes)", url, cleaned_len);

  return result;
}

/**
 * Initialize HTTP/HTTPS protocol handlers
 */
void jsrt_http_handler_init(void) {
  MODULE_DEBUG_PROTOCOL("Initializing HTTP/HTTPS protocol handlers");

  // Register http:// handler
  JSRT_ProtocolHandler http_handler = {
      .protocol_name = "http", .load = jsrt_http_handler_load, .cleanup = NULL, .user_data = NULL};

  if (!jsrt_register_protocol_handler("http", &http_handler)) {
    MODULE_DEBUG_ERROR("Failed to register http:// protocol handler");
  } else {
    MODULE_DEBUG_PROTOCOL("http:// protocol handler registered successfully");
  }

  // Register https:// handler (uses same load function)
  JSRT_ProtocolHandler https_handler = {
      .protocol_name = "https", .load = jsrt_http_handler_load, .cleanup = NULL, .user_data = NULL};

  if (!jsrt_register_protocol_handler("https", &https_handler)) {
    MODULE_DEBUG_ERROR("Failed to register https:// protocol handler");
  } else {
    MODULE_DEBUG_PROTOCOL("https:// protocol handler registered successfully");
  }
}

/**
 * Cleanup HTTP/HTTPS protocol handlers
 */
void jsrt_http_handler_cleanup(void) {
  MODULE_DEBUG_PROTOCOL("Cleaning up HTTP/HTTPS protocol handlers");
  jsrt_unregister_protocol_handler("http");
  jsrt_unregister_protocol_handler("https");
}
