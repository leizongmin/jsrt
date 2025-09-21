#include <ctype.h>
#include <string.h>
#include "url.h"

// Forward declarations for helper functions
extern char* handle_backslash_relative_path(const char* url, JSRT_URL* base_url, JSRT_URL* result);
extern int handle_absolute_path(const char* url, JSRT_URL* base_url, JSRT_URL* result);
extern int handle_windows_drive_relative(const char* path_copy, JSRT_URL* result, int is_file_scheme);
extern int resolve_complex_relative_path(const char* path_copy, JSRT_URL* base_url, JSRT_URL* result, int is_special);
extern int build_resolved_href(JSRT_URL* result);

// Resolve relative URL against base URL
JSRT_URL* resolve_relative_url(const char* url, const char* base) {
  // Check if this is a protocol-relative URL (starts with //)
  if (strncmp(url, "//", 2) == 0) {
    return handle_protocol_relative(url, base);
  }

  // Parse base URL first
  JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
  if (!base_url || strlen(base_url->protocol) == 0) {
    if (base_url)
      JSRT_FreeURL(base_url);
    return NULL;  // Invalid base URL
  }

  // For non-special schemes (like test:, mailto:, data:), empty host is valid (opaque URLs)
  // Only require host for special schemes (except file: which can have empty host)
  int is_special = is_special_scheme(base_url->protocol);
  int is_file_scheme = (strcmp(base_url->protocol, "file:") == 0);
  if (is_special && strlen(base_url->host) == 0 && !is_file_scheme) {
    JSRT_FreeURL(base_url);
    return NULL;  // Special schemes require host (except file:)
  }

  // Check if base URL is opaque and relative URL is incompatible
  // An opaque URL is a non-special scheme without authority and without hierarchical path
  // Opaque means pathname doesn't start with "/" (no hierarchical structure)
  int is_opaque = !is_special && (strlen(base_url->host) == 0) && (base_url->pathname && base_url->pathname[0] != '/');
  if (is_opaque) {
    // For opaque base URLs, only fragment URLs (#...) are allowed
    // According to WHATWG URL spec, paths and queries should fail for opaque bases
    if (url[0] != '\0' && url[0] != '#') {
      // Non-fragment relative URLs should fail for opaque bases
      JSRT_FreeURL(base_url);
      return NULL;  // Cannot resolve relative URL against opaque base
    }
  }

  JSRT_URL* result = malloc(sizeof(JSRT_URL));
  if (!result) {
    JSRT_FreeURL(base_url);
    return NULL;
  }
  memset(result, 0, sizeof(JSRT_URL));

  // Initialize result with base URL components
  result->protocol = strdup(base_url->protocol);
  result->username = strdup(base_url->username ? base_url->username : "");
  result->password = strdup(base_url->password ? base_url->password : "");
  result->host = strdup(base_url->host);
  result->hostname = strdup(base_url->hostname);
  result->port = strdup(base_url->port);
  result->search_params = JS_UNDEFINED;
  result->ctx = NULL;
  result->has_password_field = base_url->has_password_field;
  result->double_colon_at_pattern = base_url->double_colon_at_pattern;
  result->opaque_path = base_url->opaque_path;
  result->has_authority_syntax = base_url->has_authority_syntax;

  // Check for allocation failures
  if (!result->protocol || !result->username || !result->password || !result->host || !result->hostname ||
      !result->port) {
    JSRT_FreeURL(base_url);
    JSRT_FreeURL(result);
    return NULL;
  }

  // Handle scheme-only relative URLs like "http:foo.com"
  // Only special schemes should be treated as relative paths with the scheme stripped
  char* colon_pos = strchr(url, ':');
  if (colon_pos && colon_pos > url && ((url[0] >= 'a' && url[0] <= 'z') || (url[0] >= 'A' && url[0] <= 'Z')) &&
      strstr(url, "://") == NULL) {
    // Extract scheme to check if it's special
    size_t scheme_len = colon_pos - url;
    char* scheme = malloc(scheme_len + 2);  // +1 for ':', +1 for '\0'
    if (!scheme) {
      JSRT_FreeURL(base_url);
      JSRT_FreeURL(result);
      return NULL;
    }
    strncpy(scheme, url, scheme_len);
    scheme[scheme_len] = ':';
    scheme[scheme_len + 1] = '\0';

    // Special case: file URLs against non-file base URLs should be treated as absolute URLs
    if (strcmp(scheme, "file:") == 0 && strcmp(base_url->protocol, "file:") != 0) {
      free(scheme);
      JSRT_FreeURL(base_url);
      // Parse as absolute file URL by calling main parser without base
      return JSRT_ParseURL(url, NULL);
    }

    // Only strip scheme for special schemes
    if (is_special_scheme(scheme)) {
      url = colon_pos + 1;  // Skip the scheme and colon
    }
    free(scheme);
  }

  // Handle URLs that start with a fragment (e.g., "#fragment" or ":#")
  char* fragment_pos = strchr(url, '#');
  if (fragment_pos != NULL) {
    // Check if it's a fragment-only URL (starts with #) or has a fragment part
    if (url[0] == '#') {
      // Fragment-only URL: preserve base pathname and search, replace hash
      result->pathname = strdup(base_url->pathname);
      result->search = strdup(base_url->search ? base_url->search : "");
      result->hash = strdup(url);  // Include the '#'
    } else {
      // URL has both path and fragment components - will be handled below
      goto handle_complex_relative_path;
    }
  } else if (url[0] == '?') {
    // Query-only URL: preserve base pathname, replace search and clear hash
    result->pathname = strdup(base_url->pathname);
    result->search = strdup(url);  // Include the '?'
    result->hash = strdup("");
  } else if (url[0] == '\\') {
    // Handle backslash-starting relative URLs (Windows path patterns)
    if (!handle_backslash_relative_path(url, base_url, result)) {
      JSRT_FreeURL(base_url);
      JSRT_FreeURL(result);
      return NULL;
    }
  } else if (url[0] == '/') {
    // Absolute path - parse the path to separate pathname, search, and hash
    if (!handle_absolute_path(url, base_url, result)) {
      JSRT_FreeURL(base_url);
      JSRT_FreeURL(result);
      return NULL;
    }
  } else {
  handle_complex_relative_path:
    // Relative path - resolve against base directory
    // First, parse the relative path to separate path, search, and hash components
    char* path_copy = strdup(url);
    char* search_pos = strchr(path_copy, '?');
    char* hash_pos = strchr(path_copy, '#');

    // Extract hash component
    if (hash_pos) {
      *hash_pos = '\0';
      result->hash = malloc(strlen(hash_pos + 1) + 2);  // +1 for '#', +1 for '\0'
      if (!result->hash) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
      snprintf(result->hash, strlen(hash_pos + 1) + 2, "#%s", hash_pos + 1);
    } else {
      result->hash = strdup("");
    }

    // Extract search component
    if (search_pos && (!hash_pos || search_pos < hash_pos)) {
      *search_pos = '\0';
      const char* search_end = hash_pos ? hash_pos : (search_pos + strlen(search_pos + 1) + 1);
      size_t search_len = search_end - search_pos;
      result->search = malloc(search_len + 2);  // +1 for '?', +1 for '\0'
      if (!result->search) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
      snprintf(result->search, search_len + 2, "?%.*s", (int)(search_len - 1), search_pos + 1);
    } else {
      result->search = strdup("");
    }

    // Special case: Windows drive letter in relative URL for file scheme
    int drive_result = handle_windows_drive_relative(path_copy, result, is_file_scheme);
    if (drive_result == -1) {
      // Error occurred
      free(path_copy);
      JSRT_FreeURL(base_url);
      JSRT_FreeURL(result);
      return NULL;
    } else if (drive_result == 1) {
      // Handled - goto cleanup
      free(path_copy);
      goto cleanup_and_normalize;
    }

    // Handle regular relative paths
    if (!resolve_complex_relative_path(path_copy, base_url, result, is_special)) {
      free(path_copy);
      JSRT_FreeURL(base_url);
      JSRT_FreeURL(result);
      return NULL;
    }

    free(path_copy);
  }

cleanup_and_normalize:
  // Normalize dot segments in the pathname for ALL schemes
  // According to WHATWG URL spec, dot segment normalization applies to all schemes
  if (result->pathname) {
    char* normalized_pathname = normalize_dot_segments_with_percent_decoding(result->pathname);
    if (normalized_pathname) {
      free(result->pathname);
      result->pathname = normalized_pathname;
    }
  }

  // Normalize Windows drive letters in file URL pathnames
  if (result->protocol && strcmp(result->protocol, "file:") == 0 && result->pathname) {
    char* normalized_drive = normalize_windows_drive_letters(result->pathname);
    if (normalized_drive == NULL) {
      // Invalid drive letter pattern (e.g., double pipes)
      JSRT_FreeURL(base_url);
      JSRT_FreeURL(result);
      return NULL;
    }
    if (normalized_drive != result->pathname) {  // Check if it was actually changed
      free(result->pathname);
      result->pathname = normalized_drive;
    }
  }

  // Build origin using compute_origin function to handle all scheme types correctly
  result->origin = compute_origin_with_pathname(result->protocol, result->hostname, result->port,
                                                result->double_colon_at_pattern, result->pathname);

  // Build href using helper function
  if (!build_resolved_href(result)) {
    JSRT_FreeURL(base_url);
    JSRT_FreeURL(result);
    return NULL;
  }

  JSRT_FreeURL(base_url);

  // Build the final href string for the resolved URL
  build_href(result);

  return result;
}