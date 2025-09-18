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
      if (!result->hash) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
      snprintf(result->hash, strlen(hash_pos + 1) + 2, "#%s", hash_pos + 1);
    } else {
      result->hash = strdup("");
      if (!result->hash) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
      if (!result->hash) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
      if (!result->hash) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
    }

    if (search_pos && (!hash_pos || search_pos < hash_pos)) {
      *search_pos = '\0';
      const char* search_end = hash_pos ? (hash_pos) : (search_pos + strlen(search_pos + 1) + 1);
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
      if (!result->search) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
      if (!result->search) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
    }

    // Encode the absolute pathname according to the scheme type
    if (is_special_scheme(result->protocol)) {
      result->pathname = url_path_encode_special(path_copy);
    } else {
      result->pathname = url_nonspecial_path_encode(path_copy);
    }
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
    // Handle relative URLs like "C|/foo/bar" when base is a file: URL
    // These should be treated as absolute paths, not relative paths
    if (is_file_scheme && strlen(path_copy) >= 3 && isalpha(path_copy[0]) && path_copy[1] == '|' &&
        path_copy[2] == '/') {
      // Convert Windows drive letter to absolute file path
      // "C|/foo/bar" -> "/C:/foo/bar"
      size_t new_len = strlen(path_copy) + 2;  // +1 for leading '/', +1 for null terminator
      char* absolute_path = malloc(new_len);
      if (!absolute_path) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
      snprintf(absolute_path, new_len, "/%c:%s", path_copy[0], path_copy + 2);  // Convert | to :

      free(result->pathname);
      result->pathname = absolute_path;
      free(path_copy);
      goto cleanup_and_normalize;
    }

    // Special case: empty URL should resolve to base URL unchanged
    if (strlen(path_copy) == 0) {
      // Empty relative URL: preserve base pathname and search, clear hash (unless already set)
      result->pathname = strdup(base_url->pathname);
      if (!result->pathname) {
        free(path_copy);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }
      if (strlen(result->search) == 0) {
        free(result->search);
        result->search = strdup(base_url->search ? base_url->search : "");
        if (!result->search) {
          free(path_copy);
          JSRT_FreeURL(base_url);
          JSRT_FreeURL(result);
          return NULL;
        }
      }
    }
    // For non-special schemes, handle relative paths differently
    else if (!is_special) {
      // Non-special schemes: simple concatenation without directory resolution
      // According to WHATWG URL spec, non-special schemes preserve relative paths as-is
      char* encoded_path_copy = url_nonspecial_path_encode(path_copy);
      result->pathname = encoded_path_copy;
    } else {
      // Special schemes: traditional directory-based resolution
      const char* base_pathname = base_url->pathname;
      char* last_slash = strrchr(base_pathname, '/');

      if (last_slash == NULL || last_slash == base_pathname) {
        // No directory or root directory
        char* temp_pathname = malloc(strlen(path_copy) + 2);
        if (!temp_pathname) {
          free(path_copy);
          JSRT_FreeURL(base_url);
          JSRT_FreeURL(result);
          return NULL;
        }
        snprintf(temp_pathname, strlen(path_copy) + 2, "/%s", path_copy);
        result->pathname = url_path_encode_special(temp_pathname);
        free(temp_pathname);
      } else {
        // Copy base directory and append relative path
        size_t dir_len = last_slash - base_pathname;         // Length up to but not including last slash
        size_t total_len = dir_len + strlen(path_copy) + 3;  // dir + '/' + relative_path + '\0'
        char* temp_pathname = malloc(total_len);
        if (!temp_pathname) {
          free(path_copy);
          JSRT_FreeURL(base_url);
          JSRT_FreeURL(result);
          return NULL;
        }
        strncpy(temp_pathname, base_pathname, dir_len);
        temp_pathname[dir_len] = '/';
        snprintf(temp_pathname + dir_len + 1, total_len - dir_len - 1, "%s", path_copy);
        result->pathname = url_path_encode_special(temp_pathname);
        free(temp_pathname);
      }
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

  // Pathname is already percent-encoded when stored in the URL object
  // Just use it as-is for href construction
  char* encoded_pathname = strdup(result->pathname ? result->pathname : "");
  if (!encoded_pathname) {
    JSRT_FreeURL(base_url);
    JSRT_FreeURL(result);
    return NULL;
  }
  char* encoded_search = url_component_encode(result->search);
  if (!encoded_search) {
    free(encoded_pathname);
    JSRT_FreeURL(base_url);
    JSRT_FreeURL(result);
    return NULL;
  }
  // Use scheme-appropriate fragment encoding
  char* encoded_hash = is_special_scheme(result->protocol) ? url_fragment_encode(result->hash)
                                                           : url_fragment_encode_nonspecial(result->hash);
  if (!encoded_hash) {
    free(encoded_pathname);
    free(encoded_search);
    JSRT_FreeURL(base_url);
    JSRT_FreeURL(result);
    return NULL;
  }

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
      if (!protocol_and_slashes_temp) {
        // Cleanup and bail out (do not free result->origin; keep URL object consistent)
        free(encoded_pathname);
        free(encoded_search);
        free(encoded_hash);
        if (result->href) {
          free(result->href);
          result->href = NULL;
        }
        return result;
      }
      snprintf(protocol_and_slashes_temp, strlen(result->protocol) + 3, "%s//", result->protocol);
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
    if (!result->href) {
      free(encoded_pathname);
      free(encoded_search);
      free(encoded_hash);
      JSRT_FreeURL(base_url);
      JSRT_FreeURL(result);
      return NULL;
    }

    // Build href with user info if present
    if ((result->username && strlen(result->username) > 0) || (result->password && strlen(result->password) > 0)) {
      // Insert user info before the host part in the origin
      // Origin format is "protocol://host" - we need "protocol://user:pass@host"
      char* protocol_and_slashes = malloc(strlen(result->protocol) + 3);
      if (!protocol_and_slashes) {
        if (result->href) {
          free(result->href);
          result->href = NULL;
        }
        free(encoded_pathname);
        free(encoded_search);
        free(encoded_hash);
        return result;
      }
      snprintf(protocol_and_slashes, strlen(result->protocol) + 3, "%s//", result->protocol);

      size_t written = snprintf(result->href, href_len, "%s", protocol_and_slashes);

      if (result->username && strlen(result->username) > 0) {
        written += snprintf(result->href + written, href_len - written, "%s", result->username);
      }
      if (result->has_password_field) {
        written += snprintf(result->href + written, href_len - written, ":");
        if (result->password) {
          written += snprintf(result->href + written, href_len - written, "%s", result->password);
        }
      }
      written += snprintf(result->href + written, href_len - written, "@");

      // Add the host part (origin without "protocol://")
      size_t prefix_len = strlen(protocol_and_slashes);
      if (strlen(result->origin) > prefix_len) {
        const char* host_part = result->origin + prefix_len;
        written += snprintf(result->href + written, href_len - written, "%s", host_part);
      }

      free(protocol_and_slashes);
    } else {
      // Special handling for file URLs which have null origin
      if (strcmp(result->protocol, "file:") == 0) {
        // File URLs: build as file:// + pathname + search + hash
        size_t written = snprintf(result->href, href_len, "%s//", result->protocol);
        written += snprintf(result->href + written, href_len - written, "%s%s%s", encoded_pathname, encoded_search,
                            encoded_hash);
      } else {
        size_t written = snprintf(result->href, href_len, "%s", result->origin);
        written += snprintf(result->href + written, href_len - written, "%s%s%s", encoded_pathname, encoded_search,
                            encoded_hash);
      }
    }
  } else {
    // Non-special schemes: check if it has authority (host)
    if (result->host && strlen(result->host) > 0) {
      // Has authority: use protocol + "//" + host + pathname + search + hash
      size_t href_len = strlen(result->protocol) + 2 + strlen(result->host) + strlen(encoded_pathname) +
                        strlen(encoded_search) + strlen(encoded_hash) + 1;
      result->href = malloc(href_len);
      if (!result->href) {
        free(encoded_pathname);
        free(encoded_search);
        free(encoded_hash);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }

      snprintf(result->href, href_len, "%s//%s%s%s%s", result->protocol, result->host, encoded_pathname, encoded_search,
               encoded_hash);
    } else {
      // No authority: use direct protocol + pathname + search + hash construction (opaque)
      size_t href_len =
          strlen(result->protocol) + strlen(encoded_pathname) + strlen(encoded_search) + strlen(encoded_hash) + 1;
      result->href = malloc(href_len);
      if (!result->href) {
        free(encoded_pathname);
        free(encoded_search);
        free(encoded_hash);
        JSRT_FreeURL(base_url);
        JSRT_FreeURL(result);
        return NULL;
      }

      snprintf(result->href, href_len, "%s%s%s%s", result->protocol, encoded_pathname, encoded_search, encoded_hash);
    }
  }

  free(encoded_pathname);
  free(encoded_search);
  free(encoded_hash);

  JSRT_FreeURL(base_url);
  return result;
}