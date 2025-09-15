#include <ctype.h>
#include <string.h>
#include "url.h"

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

  // Handle scheme-only relative URLs like "http:foo.com"
  // Only special schemes should be treated as relative paths with the scheme stripped
  char* colon_pos = strchr(url, ':');
  if (colon_pos && colon_pos > url && ((url[0] >= 'a' && url[0] <= 'z') || (url[0] >= 'A' && url[0] <= 'Z')) &&
      strstr(url, "://") == NULL) {
    // Extract scheme to check if it's special
    size_t scheme_len = colon_pos - url;
    char* scheme = malloc(scheme_len + 2);  // +1 for ':', +1 for '\0'
    strncpy(scheme, url, scheme_len);
    scheme[scheme_len] = ':';
    scheme[scheme_len + 1] = '\0';

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
    // TODO: Windows path \\x\hello parsing - complex implementation needed
  } else if (url[0] == '/') {
    // Absolute path - parse the path to separate pathname, search, and hash
    // Note: This handles both regular absolute paths and single-slash same-scheme special URLs
    char* path_copy = strdup(url);
    char* search_pos = strchr(path_copy, '?');
    char* hash_pos = strchr(path_copy, '#');

    if (hash_pos) {
      *hash_pos = '\0';
      result->hash = malloc(strlen(hash_pos + 1) + 2);  // +1 for '#', +1 for '\0'
      sprintf(result->hash, "#%s", hash_pos + 1);
    } else {
      result->hash = strdup("");
    }

    if (search_pos && (!hash_pos || search_pos < hash_pos)) {
      *search_pos = '\0';
      const char* search_end = hash_pos ? (hash_pos) : (search_pos + strlen(search_pos + 1) + 1);
      size_t search_len = search_end - search_pos;
      result->search = malloc(search_len + 2);  // +1 for '?', +1 for '\0'
      sprintf(result->search, "?%.*s", (int)(search_len - 1), search_pos + 1);
    } else {
      result->search = strdup("");
    }

    result->pathname = strdup(path_copy);
    free(path_copy);
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
      sprintf(result->hash, "#%s", hash_pos + 1);
    } else {
      result->hash = strdup("");
    }

    // Extract search component
    if (search_pos && (!hash_pos || search_pos < hash_pos)) {
      *search_pos = '\0';
      const char* search_end = hash_pos ? hash_pos : (search_pos + strlen(search_pos + 1) + 1);
      size_t search_len = search_end - search_pos;
      result->search = malloc(search_len + 2);  // +1 for '?', +1 for '\0'
      sprintf(result->search, "?%.*s", (int)(search_len - 1), search_pos + 1);
    } else {
      result->search = strdup("");
    }

    // Special case: empty URL should resolve to base URL unchanged
    if (strlen(path_copy) == 0) {
      // Empty relative URL: preserve base pathname and search, clear hash (unless already set)
      result->pathname = strdup(base_url->pathname);
      if (strlen(result->search) == 0) {
        free(result->search);
        result->search = strdup(base_url->search ? base_url->search : "");
      }
    }
    // For non-special schemes, handle relative paths differently
    else if (!is_special) {
      // Non-special schemes: simple concatenation without directory resolution
      // According to WHATWG URL spec, non-special schemes preserve relative paths as-is
      result->pathname = strdup(path_copy);
    } else {
      // Special schemes: traditional directory-based resolution
      const char* base_pathname = base_url->pathname;
      char* last_slash = strrchr(base_pathname, '/');

      if (last_slash == NULL || last_slash == base_pathname) {
        // No directory or root directory
        result->pathname = malloc(strlen(path_copy) + 2);
        sprintf(result->pathname, "/%s", path_copy);
      } else {
        // Copy base directory and append relative path
        size_t dir_len = last_slash - base_pathname;                 // Length up to but not including last slash
        result->pathname = malloc(dir_len + strlen(path_copy) + 3);  // dir + '/' + relative_path + '\0'
        strncpy(result->pathname, base_pathname, dir_len);
        result->pathname[dir_len] = '/';
        strcpy(result->pathname + dir_len + 1, path_copy);
      }
    }

    free(path_copy);
  }

  // Normalize dot segments in the pathname (only for special schemes)
  // Non-special schemes should preserve dot segments like /.//
  if (is_special_scheme(result->protocol)) {
    char* normalized_pathname = normalize_dot_segments_with_percent_decoding(result->pathname);
    if (normalized_pathname) {
      free(result->pathname);
      result->pathname = normalized_pathname;
    }
  }

  // Build origin using compute_origin function to handle all scheme types correctly
  result->origin = compute_origin(result->protocol, result->hostname, result->port, result->double_colon_at_pattern);

  // For non-special schemes, pathname should not be encoded (spaces preserved)
  // For special schemes, pathname should be encoded (except file URLs preserve pipes)
  char* encoded_pathname;
  if (is_special_scheme(result->protocol)) {
    if (strcmp(result->protocol, "file:") == 0) {
      // File URLs preserve pipe characters in pathname
      encoded_pathname = strdup(result->pathname ? result->pathname : "");
    } else {
      // Other special schemes use component encoding
      encoded_pathname = url_component_encode(result->pathname);
    }
  } else {
    // Non-special schemes: preserve spaces and other characters in pathname
    encoded_pathname = strdup(result->pathname ? result->pathname : "");
  }
  char* encoded_search = url_component_encode(result->search);
  // Use scheme-appropriate fragment encoding
  char* encoded_hash = is_special_scheme(result->protocol) ? url_fragment_encode(result->hash)
                                                           : url_fragment_encode_nonspecial(result->hash);

  // Build href - different logic for special vs non-special schemes
  if (is_special_scheme(result->protocol)) {
    // Special schemes: use origin-based construction (for authority-based URLs)

    // Calculate the buffer size more carefully
    size_t href_len = strlen(result->protocol) + strlen(encoded_pathname) + strlen(encoded_search) +
                      strlen(encoded_hash) + 10;  // Base + safety buffer

    // Add space for "//" if needed
    href_len += 2;

    // Add space for host part (extract from origin, excluding protocol://)
    if (result->origin && strlen(result->origin) > 0) {
      char* protocol_and_slashes_temp = malloc(strlen(result->protocol) + 3);
      sprintf(protocol_and_slashes_temp, "%s//", result->protocol);
      size_t prefix_len = strlen(protocol_and_slashes_temp);
      size_t origin_len = strlen(result->origin);

      // Only add host part length if origin is longer than protocol prefix
      if (origin_len > prefix_len) {
        const char* host_part = result->origin + prefix_len;
        href_len += strlen(host_part);
      }
      free(protocol_and_slashes_temp);
    }

    // Add space for username:password@ if present
    if ((result->username && strlen(result->username) > 0) || (result->password && strlen(result->password) > 0)) {
      if (result->username && strlen(result->username) > 0) {
        href_len += strlen(result->username);
      }
      if (result->password && strlen(result->password) > 0) {
        href_len += 1 + strlen(result->password);  // +1 for colon
      }
      href_len += 1;  // +1 for @ symbol
    }

    result->href = malloc(href_len);

    // Build href with user info if present
    if ((result->username && strlen(result->username) > 0) || (result->password && strlen(result->password) > 0)) {
      // Insert user info before the host part in the origin
      // Origin format is "protocol://host" - we need "protocol://user:pass@host"
      char* protocol_and_slashes = malloc(strlen(result->protocol) + 3);
      sprintf(protocol_and_slashes, "%s//", result->protocol);

      strcpy(result->href, protocol_and_slashes);

      if (result->username && strlen(result->username) > 0) {
        strcat(result->href, result->username);
      }
      if (result->has_password_field) {
        strcat(result->href, ":");
        if (result->password) {
          strcat(result->href, result->password);
        }
      }
      strcat(result->href, "@");

      // Add the host part (origin without "protocol://")
      size_t prefix_len = strlen(protocol_and_slashes);
      if (strlen(result->origin) > prefix_len) {
        const char* host_part = result->origin + prefix_len;
        strcat(result->href, host_part);
      }

      free(protocol_and_slashes);
    } else {
      // Special handling for file URLs which have null origin
      if (strcmp(result->protocol, "file:") == 0) {
        // File URLs: build as file:// + pathname + search + hash
        strcpy(result->href, result->protocol);
        strcat(result->href, "//");
      } else {
        strcpy(result->href, result->origin);
      }
    }

    strcat(result->href, encoded_pathname);
    strcat(result->href, encoded_search);
    strcat(result->href, encoded_hash);
  } else {
    // Non-special schemes: check if it has authority (host)
    if (result->host && strlen(result->host) > 0) {
      // Has authority: use protocol + "//" + host + pathname + search + hash
      size_t href_len = strlen(result->protocol) + 2 + strlen(result->host) + strlen(encoded_pathname) +
                        strlen(encoded_search) + strlen(encoded_hash) + 1;
      result->href = malloc(href_len);

      strcpy(result->href, result->protocol);
      strcat(result->href, "//");
      strcat(result->href, result->host);
      strcat(result->href, encoded_pathname);
      strcat(result->href, encoded_search);
      strcat(result->href, encoded_hash);
    } else {
      // No authority: use direct protocol + pathname + search + hash construction (opaque)
      size_t href_len =
          strlen(result->protocol) + strlen(encoded_pathname) + strlen(encoded_search) + strlen(encoded_hash) + 1;
      result->href = malloc(href_len);

      strcpy(result->href, result->protocol);
      strcat(result->href, encoded_pathname);
      strcat(result->href, encoded_search);
      strcat(result->href, encoded_hash);
    }
  }

  free(encoded_pathname);
  free(encoded_search);
  free(encoded_hash);

  JSRT_FreeURL(base_url);
  return result;
}