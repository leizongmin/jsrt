#include <ctype.h>
#include <errno.h>
#include "url.h"

// Include the new modular components
#include "url_builder.c"
#include "url_parser.c"
#include "url_preprocessor.c"

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