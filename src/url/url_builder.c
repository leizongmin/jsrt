#include <ctype.h>
#include <string.h>
#include "url.h"

// Build final href string
void build_href(JSRT_URL* parsed) {
  if (!parsed)
    return;

  int is_special = is_special_scheme(parsed->protocol);

  // Calculate href length including userinfo
  size_t userinfo_len = 0;
  if (parsed->username && strlen(parsed->username) > 0) {
    userinfo_len += strlen(parsed->username);
    if (parsed->password && strlen(parsed->password) > 0) {
      userinfo_len += 1 + strlen(parsed->password);  // +1 for ':'
    }
    userinfo_len += 1;  // +1 for '@'
  }

  size_t href_len = strlen(parsed->protocol) + userinfo_len + strlen(parsed->host) + strlen(parsed->pathname) +
                    strlen(parsed->search) + strlen(parsed->hash) + 20;
  parsed->href = malloc(href_len);

  // Build userinfo part with proper encoding
  char userinfo[512] = "";
  if ((parsed->username && strlen(parsed->username) > 0) ||
      (parsed->has_password_field && parsed->password && strlen(parsed->password) > 0)) {
    // Either username exists or password exists
    char* encoded_username =
        url_userinfo_encode_with_scheme_name(parsed->username ? parsed->username : "", parsed->protocol);

    if (parsed->has_password_field && parsed->password && strlen(parsed->password) > 0) {
      // Password exists, include colon and password
      char* encoded_password = url_userinfo_encode_with_scheme_name(parsed->password, parsed->protocol);
      snprintf(userinfo, sizeof(userinfo), "%s:%s@", encoded_username, encoded_password);
      free(encoded_password);
    } else {
      // No password or password is empty - omit colon per WPT spec
      snprintf(userinfo, sizeof(userinfo), "%s@", encoded_username);
    }
    free(encoded_username);
  }

  // Build final host with normalized port
  char* final_host;
  if (parsed->hostname && strlen(parsed->hostname) > 0) {
    char* normalized_port = normalize_port(parsed->port, parsed->protocol);
    if (normalized_port && strlen(normalized_port) > 0) {
      // Host includes port
      size_t host_len = strlen(parsed->hostname) + strlen(normalized_port) + 2;
      final_host = malloc(host_len);
      snprintf(final_host, host_len, "%s:%s", parsed->hostname, normalized_port);
    } else {
      // Host without port
      final_host = strdup(parsed->hostname);
    }
    free(normalized_port);
  } else {
    final_host = strdup(parsed->host ? parsed->host : "");
  }

  // Pathname is already percent-encoded when stored in the URL object
  // Just use it as-is for href construction (no additional encoding needed)
  char* final_pathname = strdup(parsed->pathname ? parsed->pathname : "");
  char* final_search = url_component_encode(parsed->search);
  char* final_hash = is_special ? url_fragment_encode(parsed->hash) : url_fragment_encode_nonspecial(parsed->hash);

  if (is_special && strlen(final_host) > 0) {
    snprintf(parsed->href, href_len, "%s//%s%s%s%s%s", parsed->protocol, userinfo, final_host, final_pathname,
             final_search, final_hash);
  } else if (strlen(final_host) > 0) {
    snprintf(parsed->href, href_len, "%s//%s%s%s%s%s", parsed->protocol, userinfo, final_host, final_pathname,
             final_search, final_hash);
  } else if (is_special && !parsed->opaque_path) {
    // Special schemes with empty host (like file:///path) should include //
    // For file URLs, ensure proper slash handling to avoid file:////
    if (strcmp(parsed->protocol, "file:") == 0) {
      // File URLs: build as file:// + pathname (which already starts with /)
      // This ensures file:/// not file:////
      snprintf(parsed->href, href_len, "%s//%s%s%s", parsed->protocol, final_pathname, final_search, final_hash);
    } else {
      snprintf(parsed->href, href_len, "%s//%s%s%s", parsed->protocol, final_pathname, final_search, final_hash);
    }
  } else if (strcmp(parsed->protocol, "file:") == 0 && parsed->opaque_path) {
    // Opaque file URLs need special formatting
    // But for cases like "//" -> "file:///" we shouldn't double the slashes
    if (final_pathname && final_pathname[0] == '/') {
      // pathname already starts with slash, use file:// format
      snprintf(parsed->href, href_len, "%s//%s%s%s", parsed->protocol, final_pathname, final_search, final_hash);
    } else {
      // pathname doesn't start with slash, use file:/// format
      snprintf(parsed->href, href_len, "%s///%s%s%s", parsed->protocol, final_pathname, final_search, final_hash);
    }
  } else if (parsed->has_authority_syntax) {
    // Non-special schemes that started with // should preserve authority syntax
    // For empty pathname (like foo://), don't add extra slash
    if (strlen(final_pathname) == 0) {
      snprintf(parsed->href, href_len, "%s//%s%s", parsed->protocol, final_search, final_hash);
    } else {
      snprintf(parsed->href, href_len, "%s//%s%s%s", parsed->protocol, final_pathname, final_search, final_hash);
    }
  } else {
    // Non-special schemes or opaque paths (no authority)
    snprintf(parsed->href, href_len, "%s%s%s%s", parsed->protocol, final_pathname, final_search, final_hash);
  }

  // Clean up encoded strings
  free(final_host);
  free(final_pathname);
  free(final_search);
  free(final_hash);
}

// Parse path, query, and fragment components
void parse_path_query_fragment(JSRT_URL* parsed, char* ptr) {
  if (!parsed || !ptr)
    return;

  // Parse path, query, fragment from remaining part
  char* query_pos = strchr(ptr, '?');
  char* fragment_pos = strchr(ptr, '#');

  // Calculate all positions and lengths BEFORE modifying the string
  size_t original_len = strlen(ptr);
  char* search_end = NULL;
  size_t search_len = 0;

  if (query_pos) {
    search_end = fragment_pos ? fragment_pos : ptr + original_len;
    search_len = search_end - query_pos;

    // Validate that search_len is reasonable
    if (search_len > original_len || search_end < query_pos) {
      // Invalid calculation, skip query processing
      query_pos = NULL;
    }
  }

  // Handle fragment first
  if (fragment_pos) {
    free(parsed->hash);
    parsed->hash = strdup(fragment_pos);  // Include the #
    *fragment_pos = '\0';
  }

  // Handle query
  if (query_pos && search_len > 0) {
    char* raw_search = malloc(search_len + 1);
    strncpy(raw_search, query_pos, search_len);
    raw_search[search_len] = '\0';

    free(parsed->search);
    parsed->search = url_component_encode(raw_search);
    free(raw_search);
    *query_pos = '\0';
  }

  // Handle pathname
  if (strlen(ptr) > 0) {
    // Only update pathname if it hasn't been set by special case handling above
    if (strcmp(parsed->pathname, "/") == 0 || strcmp(parsed->pathname, "") == 0) {
      free(parsed->pathname);

      // Remove BOM characters from path before encoding
      char* cleaned_path = remove_all_ascii_whitespace(ptr);

      // Encode the path properly WITHOUT decoding first
      // This preserves percent-encoded dot segments (%2e) which should NOT be normalized
      // according to WHATWG URL spec - percent-encoded dots should remain as-is
      if (strcmp(parsed->protocol, "file:") == 0) {
        parsed->pathname = url_path_encode_file(cleaned_path);
      } else if (is_special_scheme(parsed->protocol)) {
        parsed->pathname = url_path_encode_special(cleaned_path);
      } else {
        parsed->pathname = url_nonspecial_path_encode(cleaned_path);
      }

      free(cleaned_path);
    }
  } else {
    // Empty pathname handling:
    // - Special schemes should default to "/" if pathname is empty
    // - Non-special schemes with authority syntax (foo://) should keep pathname empty
    if (strcmp(parsed->pathname, "") == 0) {
      if (is_special_scheme(parsed->protocol) && !parsed->has_authority_syntax) {
        // Special schemes without authority syntax get "/" as default pathname
        free(parsed->pathname);
        parsed->pathname = strdup("/");
      }
      // Non-special schemes with authority syntax (foo://) keep empty pathname
      // Special schemes with authority syntax also keep their existing empty pathname
    }
  }
}

// Apply URL component normalization for building
char* normalize_url_component_for_href(const char* component, int is_special_scheme) {
  if (!component)
    return strdup("");

  // Apply appropriate encoding based on scheme type
  if (is_special_scheme) {
    return url_component_encode(component);
  } else {
    return strdup(component);
  }
}

// Build URL string with specific components
char* build_url_string(const char* protocol, const char* username, const char* password, const char* host,
                       const char* pathname, const char* search, const char* hash, int has_password_field) {
  if (!protocol)
    return NULL;

  int is_special = is_special_scheme(protocol);

  // Calculate total length needed
  size_t total_len = strlen(protocol) + 20;  // Base + extra buffer

  // Add userinfo length
  if (username && strlen(username) > 0) {
    total_len += strlen(username) + 1;  // +1 for '@'
    if (has_password_field) {
      total_len += 1;  // +1 for ':'
      if (password)
        total_len += strlen(password);
    }
  }

  // Add other components
  if (host)
    total_len += strlen(host);
  if (pathname)
    total_len += strlen(pathname);
  if (search)
    total_len += strlen(search);
  if (hash)
    total_len += strlen(hash);

  // Allocate with room for NUL terminator
  char* url_string = malloc(total_len + 1);
  if (!url_string) {
    return NULL;
  }
  int written = snprintf(url_string, total_len + 1, "%s", protocol);
  if (written < 0) {
    free(url_string);
    return NULL;
  }

  // Add authority if host exists
  if (host && strlen(host) > 0) {
    written += snprintf(url_string + written, total_len + 1 - written, "//");

    // Add userinfo if exists
    if (username && strlen(username) > 0) {
      char* encoded_username = url_userinfo_encode(username);
      if (!encoded_username) {
        free(url_string);
        return NULL;
      }
      written += snprintf(url_string + written, total_len + 1 - written, "%s", encoded_username);
      free(encoded_username);

      if (has_password_field && password && strlen(password) > 0) {
        written += snprintf(url_string + written, total_len + 1 - written, ":");
        char* encoded_password = url_userinfo_encode(password);
        if (!encoded_password) {
          free(url_string);
          return NULL;
        }
        written += snprintf(url_string + written, total_len + 1 - written, "%s", encoded_password);
        free(encoded_password);
      }
      written += snprintf(url_string + written, total_len + 1 - written, "@");
    }

    written += snprintf(url_string + written, total_len + 1 - written, "%s", host);
  } else if (is_special && pathname && pathname[0] == '/') {
    // Special schemes with no host but absolute path need //
    written += snprintf(url_string + written, total_len + 1 - written, "//");
  }

  // Add pathname with scheme-specific encoding
  if (pathname) {
    char* encoded_pathname;
    if (is_special) {
      if (protocol && strcmp(protocol, "file:") == 0) {
        // File URLs use special encoding that preserves pipe characters
        encoded_pathname = url_component_encode_file_path(pathname);
      } else {
        // Other special schemes use component encoding
        encoded_pathname = url_component_encode(pathname);
      }
    } else {
      encoded_pathname = strdup(pathname);
    }
    if (!encoded_pathname) {
      free(url_string);
      return NULL;
    }
    // Replace unsafe strcat with bounded snprintf using the current write cursor
    written += snprintf(url_string + written, total_len + 1 - written, "%s", encoded_pathname);
    free(encoded_pathname);
  }

  // Add search
  if (search && strlen(search) > 0) {
    char* encoded_search = url_component_encode(search);
    if (!encoded_search) {
      free(url_string);
      return NULL;
    }
    written += snprintf(url_string + written, total_len + 1 - written, "%s", encoded_search);
    free(encoded_search);
  }

  // Add hash
  if (hash && strlen(hash) > 0) {
    char* encoded_hash = is_special ? url_fragment_encode(hash) : url_fragment_encode_nonspecial(hash);
    if (!encoded_hash) {
      free(url_string);
      return NULL;
    }
    snprintf(url_string + written, total_len + 1 - written, "%s", encoded_hash);
    free(encoded_hash);
  }

  return url_string;
}
