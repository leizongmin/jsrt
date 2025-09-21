#include <ctype.h>
#include <string.h>
#include "url.h"

// Handle backslash-starting relative URLs (Windows path patterns)
char* handle_backslash_relative_path(const char* url, JSRT_URL* base_url, JSRT_URL* result) {
  // According to WHATWG URL spec, these should be treated as path-only URLs

  // For special schemes (like http, https), backslashes should be normalized to forward slashes
  // For file schemes, preserve as Windows path syntax
  int is_special = is_special_scheme(base_url->protocol);

  if (is_special && strcmp(base_url->protocol, "file:") != 0) {
    // Special schemes: normalize backslashes to forward slashes and treat as absolute path
    char* normalized_path = malloc(strlen(url) + 1);
    if (!normalized_path) {
      return NULL;
    }
    strcpy(normalized_path, url);
    // Convert backslashes to forward slashes
    for (char* p = normalized_path; *p; p++) {
      if (*p == '\\') {
        *p = '/';
      }
    }

    // Parse as absolute path
    char* search_pos = strchr(normalized_path, '?');
    char* hash_pos = strchr(normalized_path, '#');

    if (hash_pos) {
      *hash_pos = '\0';
      result->hash = malloc(strlen(hash_pos + 1) + 2);
      if (!result->hash) {
        free(normalized_path);
        return NULL;
      }
      snprintf(result->hash, strlen(hash_pos + 1) + 2, "#%s", hash_pos + 1);
    } else {
      result->hash = strdup("");
    }

    if (search_pos && (!hash_pos || search_pos < hash_pos)) {
      *search_pos = '\0';
      const char* search_end = hash_pos ? hash_pos : (search_pos + strlen(search_pos + 1) + 1);
      size_t search_len = search_end - search_pos;
      result->search = malloc(search_len + 2);
      if (!result->search) {
        free(normalized_path);
        return NULL;
      }
      snprintf(result->search, search_len + 2, "?%.*s", (int)(search_len - 1), search_pos + 1);
    } else {
      result->search = strdup("");
    }

    result->pathname = url_path_encode_special(normalized_path);
    free(normalized_path);
  } else {
    // File URLs or non-special schemes: treat backslashes as normal path characters
    // Convert backslashes to forward slashes for consistency
    char* normalized_path = malloc(strlen(url) + 1);
    if (!normalized_path) {
      return NULL;
    }
    strcpy(normalized_path, url);
    // Convert backslashes to forward slashes
    for (char* p = normalized_path; *p; p++) {
      if (*p == '\\') {
        *p = '/';
      }
    }

    // Parse as absolute path
    char* search_pos = strchr(normalized_path, '?');
    char* hash_pos = strchr(normalized_path, '#');

    if (hash_pos) {
      *hash_pos = '\0';
      result->hash = malloc(strlen(hash_pos + 1) + 2);
      if (!result->hash) {
        free(normalized_path);
        return NULL;
      }
      snprintf(result->hash, strlen(hash_pos + 1) + 2, "#%s", hash_pos + 1);
    } else {
      result->hash = strdup("");
    }

    if (search_pos && (!hash_pos || search_pos < hash_pos)) {
      *search_pos = '\0';
      const char* search_end = hash_pos ? hash_pos : (search_pos + strlen(search_pos + 1) + 1);
      size_t search_len = search_end - search_pos;
      result->search = malloc(search_len + 2);
      if (!result->search) {
        free(normalized_path);
        return NULL;
      }
      snprintf(result->search, search_len + 2, "?%.*s", (int)(search_len - 1), search_pos + 1);
    } else {
      result->search = strdup("");
    }

    // Use appropriate encoding based on scheme type
    if (is_special_scheme(result->protocol)) {
      result->pathname = url_path_encode_special(normalized_path);
    } else {
      result->pathname = url_nonspecial_path_encode(normalized_path);
    }
    free(normalized_path);
  }

  return result->pathname;
}

// Handle absolute path resolution (URLs starting with '/')
int handle_absolute_path(const char* url, JSRT_URL* base_url, JSRT_URL* result) {
  char* path_copy = strdup(url);
  char* search_pos = strchr(path_copy, '?');
  char* hash_pos = strchr(path_copy, '#');

  if (hash_pos) {
    *hash_pos = '\0';
    result->hash = malloc(strlen(hash_pos + 1) + 2);  // +1 for '#', +1 for '\0'
    if (!result->hash) {
      free(path_copy);
      return 0;
    }
    snprintf(result->hash, strlen(hash_pos + 1) + 2, "#%s", hash_pos + 1);
  } else {
    result->hash = strdup("");
    if (!result->hash) {
      free(path_copy);
      return 0;
    }
  }

  if (search_pos && (!hash_pos || search_pos < hash_pos)) {
    *search_pos = '\0';
    const char* search_end = hash_pos ? (hash_pos) : (search_pos + strlen(search_pos + 1) + 1);
    size_t search_len = search_end - search_pos;
    result->search = malloc(search_len + 2);  // +1 for '?', +1 for '\0'
    if (!result->search) {
      free(path_copy);
      return 0;
    }
    snprintf(result->search, search_len + 2, "?%.*s", (int)(search_len - 1), search_pos + 1);
  } else {
    result->search = strdup("");
    if (!result->search) {
      free(path_copy);
      return 0;
    }
  }

  // Special handling for file: scheme Windows drive preservation
  // Per WHATWG URL spec, "/" against "file:///C:/path" should resolve to "file:///C:/"
  if (strcmp(result->protocol, "file:") == 0 && strcmp(path_copy, "/") == 0) {
    // Check if base URL has a Windows drive letter pattern (/C:/ or /C|/)
    const char* base_path = base_url->pathname;
    if (base_path && strlen(base_path) >= 3 && base_path[0] == '/' && isalpha(base_path[1]) &&
        (base_path[2] == ':' || base_path[2] == '|')) {
      // Preserve the drive letter: "/" -> "/C:/"
      char* drive_path = malloc(5);  // "/C:/\0"
      if (drive_path) {
        snprintf(drive_path, 5, "/%c:/", base_path[1]);
        result->pathname = url_path_encode_special(drive_path);
        free(drive_path);
      } else {
        result->pathname = url_path_encode_special(path_copy);
      }
    } else {
      // Regular absolute path
      result->pathname = url_path_encode_special(path_copy);
    }
  } else {
    // Encode the absolute pathname according to the scheme type
    if (is_special_scheme(result->protocol)) {
      result->pathname = url_path_encode_special(path_copy);
    } else {
      result->pathname = url_nonspecial_path_encode(path_copy);
    }
  }
  free(path_copy);
  return 1;
}

// Handle Windows drive letter cases in relative URLs for file scheme
int handle_windows_drive_relative(const char* path_copy, JSRT_URL* result, int is_file_scheme) {
  if (!is_file_scheme || strlen(path_copy) < 2 || !isalpha(path_copy[0]) || path_copy[1] != '|') {
    return 0;  // Not a Windows drive case
  }

  // Handle "C|" (drive letter only) case
  if (strlen(path_copy) == 2) {
    // Convert "C|" -> "/C:/"
    char* absolute_path = malloc(5);  // "/C:/\0"
    if (!absolute_path) {
      return -1;  // Error
    }
    snprintf(absolute_path, 5, "/%c:/", path_copy[0]);
    result->pathname = absolute_path;
    return 1;  // Handled
  }
  // Handle "C|/foo/bar" case
  else if (path_copy[2] == '/') {
    // Convert Windows drive letter to absolute file path
    // "C|/foo/bar" -> "/C:/foo/bar"
    size_t new_len = strlen(path_copy) + 2;  // +1 for leading '/', +1 for null terminator
    char* absolute_path = malloc(new_len);
    if (!absolute_path) {
      return -1;  // Error
    }
    snprintf(absolute_path, new_len, "/%c:%s", path_copy[0], path_copy + 2);  // Convert | to :

    result->pathname = absolute_path;
    return 1;  // Handled
  }

  return 0;  // Not handled
}

// Resolve complex relative paths against base directory
int resolve_complex_relative_path(const char* path_copy, JSRT_URL* base_url, JSRT_URL* result, int is_special) {
  // Special case: empty URL should resolve to base URL unchanged
  if (strlen(path_copy) == 0) {
    // Empty relative URL: preserve base pathname and search, clear hash (unless already set)
    result->pathname = strdup(base_url->pathname);
    if (!result->pathname) {
      return 0;
    }
    if (strlen(result->search) == 0) {
      free(result->search);
      result->search = strdup(base_url->search ? base_url->search : "");
      if (!result->search) {
        return 0;
      }
    }
    return 1;
  }

  // For non-special schemes, handle relative paths differently
  if (!is_special) {
    // Non-special schemes still need proper relative path resolution
    // According to WHATWG URL spec, relative resolution works for all schemes
    const char* base_pathname = base_url->pathname;
    char* last_slash = strrchr(base_pathname, '/');

    if (last_slash == NULL || last_slash == base_pathname) {
      // No directory or root directory - treat as root-level relative
      char* temp_pathname = malloc(strlen(path_copy) + 2);
      if (!temp_pathname) {
        return 0;
      }
      snprintf(temp_pathname, strlen(path_copy) + 2, "/%s", path_copy);
      result->pathname = url_nonspecial_path_encode(temp_pathname);
      free(temp_pathname);
    } else {
      // Copy base directory and append relative path
      size_t dir_len = last_slash - base_pathname;         // Length up to but not including last slash
      size_t total_len = dir_len + strlen(path_copy) + 3;  // dir + '/' + relative_path + '\0'
      char* temp_pathname = malloc(total_len);
      if (!temp_pathname) {
        return 0;
      }
      strncpy(temp_pathname, base_pathname, dir_len);
      temp_pathname[dir_len] = '/';
      snprintf(temp_pathname + dir_len + 1, total_len - dir_len - 1, "%s", path_copy);
      result->pathname = url_nonspecial_path_encode(temp_pathname);
      free(temp_pathname);
    }
  } else {
    // Special schemes: traditional directory-based resolution
    const char* base_pathname = base_url->pathname;
    char* last_slash = strrchr(base_pathname, '/');

    if (last_slash == NULL || last_slash == base_pathname) {
      // No directory or root directory
      char* temp_pathname = malloc(strlen(path_copy) + 2);
      if (!temp_pathname) {
        return 0;
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
        return 0;
      }
      strncpy(temp_pathname, base_pathname, dir_len);
      temp_pathname[dir_len] = '/';
      snprintf(temp_pathname + dir_len + 1, total_len - dir_len - 1, "%s", path_copy);
      result->pathname = url_path_encode_special(temp_pathname);
      free(temp_pathname);
    }
  }

  return 1;
}

// Build href string for resolved URL - different logic for special vs non-special schemes
int build_resolved_href(JSRT_URL* result) {
  // Pathname is already percent-encoded when stored in the URL object
  // Just use it as-is for href construction
  char* encoded_pathname = strdup(result->pathname ? result->pathname : "");
  if (!encoded_pathname) {
    return 0;
  }
  char* encoded_search = url_query_encode_with_scheme(result->search, result->protocol);
  if (!encoded_search) {
    free(encoded_pathname);
    return 0;
  }
  // Use scheme-appropriate fragment encoding
  char* encoded_hash = is_special_scheme(result->protocol) ? url_fragment_encode(result->hash)
                                                           : url_fragment_encode_nonspecial(result->hash);
  if (!encoded_hash) {
    free(encoded_pathname);
    free(encoded_search);
    return 0;
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
        return 0;
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
      return 0;
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
        return 0;
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
        return 0;
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
        return 0;
      }

      snprintf(result->href, href_len, "%s%s%s%s", result->protocol, encoded_pathname, encoded_search, encoded_hash);
    }
  }

  free(encoded_pathname);
  free(encoded_search);
  free(encoded_hash);
  return 1;
}