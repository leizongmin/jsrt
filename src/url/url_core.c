#include <ctype.h>
#include <errno.h>
#include "url.h"

// The modular components are compiled separately - no need to include .c files

// Free URL structure
void JSRT_FreeURL(JSRT_URL* url) {
  if (url) {
    if (url->ctx && !JS_IsUndefined(url->search_params)) {
      JS_FreeValue(url->ctx, url->search_params);
    }
    free(url->href);
    free(url->protocol);
    free(url->username);
    free(url->password);
    free(url->host);
    free(url->hostname);
    free(url->port);
    free(url->pathname);
    free(url->search);
    free(url->hash);
    free(url->origin);
    free(url);
  }
}

// Main URL parsing function - now delegates to the modular components
JSRT_URL* JSRT_ParseURL(const char* url, const char* base) {
  if (!url)
    return NULL;

  JSRT_Debug("JSRT_ParseURL: url='%s', base='%s'", url, base ? base : "(null)");

  // Handle empty URL string - per WHATWG URL spec, empty string should resolve to base
  if (strlen(url) == 0) {
    return handle_empty_url(base);
  }

  // Check if URL should be treated as relative
  char* preprocessed_url = preprocess_url_string(url, base);
  if (!preprocessed_url)
    return NULL;

  // Handle protocol-relative URLs (starting with "//") - check AFTER normalization
  if (strncmp(preprocessed_url, "//", 2) == 0) {
    JSRT_URL* result = handle_protocol_relative(preprocessed_url, base);
    free(preprocessed_url);
    return result;
  }

  if (is_relative_url(preprocessed_url, base)) {
    JSRT_URL* result = resolve_relative_url(preprocessed_url, base);
    free(preprocessed_url);
    return result;
  }

  // Check for Windows drive letter patterns and file URLs that need scheme normalization
  if (strlen(preprocessed_url) >= 3) {
    // Case 1: Bare Windows drive letter like "C:/foo"
    // According to WHATWG URL spec, this should be treated as scheme "C:" with path "/foo"
    // ONLY convert to file URL if we have no base or if base is a file URL
    if (isalpha(preprocessed_url[0]) && (preprocessed_url[1] == '|' || preprocessed_url[1] == ':') &&
        preprocessed_url[2] == '/') {
      // Check if this already has a scheme (looks for colon before the drive letter)
      // A colon at position 1 with a letter at position 0 indicates a valid URL scheme, not a Windows drive letter
      char* scheme_colon = strchr(preprocessed_url, ':');
      if (!scheme_colon || scheme_colon != &preprocessed_url[1]) {
        // Only convert to file URL if we have no base, or base is a file URL
        if (!base || strncmp(base, "file:", 5) == 0) {
          // No base or file base - add file: prefix
          size_t file_url_len = strlen("file:///") + strlen(preprocessed_url) + 1;
          char* file_url = malloc(file_url_len);
          if (!file_url) {
            free(preprocessed_url);
            return NULL;
          }
          snprintf(file_url, file_url_len, "file:///%s", preprocessed_url);
          free(preprocessed_url);
          preprocessed_url = file_url;
        }
        // Otherwise, let it be parsed as scheme "c:" with path "/foo"
      }
    }
    // Case 2: File URLs with Windows paths like "file:c:\foo" -> normalize to "file:///c:/"
    else if (strncmp(preprocessed_url, "file:", 5) == 0 && strlen(preprocessed_url) > 7 &&
             isalpha(preprocessed_url[5]) && preprocessed_url[6] == ':') {
      // This is a file URL with Windows drive letter - ensure it's absolute
      size_t file_url_len = strlen("file:///") + strlen(preprocessed_url + 5) + 1;
      char* file_url = malloc(file_url_len);
      if (!file_url) {
        free(preprocessed_url);
        return NULL;
      }
      snprintf(file_url, file_url_len, "file:///%s", preprocessed_url + 5);  // Skip "file:" prefix
      free(preprocessed_url);
      preprocessed_url = file_url;
    }
  }

  // Apply file URL specific preprocessing
  char* file_preprocessed_url = preprocess_file_urls(preprocessed_url);
  free(preprocessed_url);
  if (!file_preprocessed_url)
    return NULL;

  // Parse as absolute URL using the new parser module
  JSRT_URL* result = parse_absolute_url(file_preprocessed_url);
  free(file_preprocessed_url);

  return result;
}