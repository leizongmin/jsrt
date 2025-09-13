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
  if (parsed->username && strlen(parsed->username) > 0) {
    char* encoded_username = url_userinfo_encode_with_scheme_name(parsed->username, parsed->protocol);
    if (parsed->has_password_field && parsed->password && strlen(parsed->password) > 0) {
      // Original URL had password field with non-empty password, include colon
      char* encoded_password = url_userinfo_encode_with_scheme_name(parsed->password, parsed->protocol);
      snprintf(userinfo, sizeof(userinfo), "%s:%s@", encoded_username, encoded_password);
      free(encoded_password);
    } else {
      // No password field in original URL, or password is empty - omit colon per WPT spec
      snprintf(userinfo, sizeof(userinfo), "%s@", encoded_username);
    }
    free(encoded_username);
  }

  // Use encoded values for href construction
  char* final_pathname =
      is_special ? url_component_encode(parsed->pathname) : strdup(parsed->pathname ? parsed->pathname : "");
  char* final_search = url_component_encode(parsed->search);
  char* final_hash = is_special ? url_fragment_encode(parsed->hash) : url_fragment_encode_nonspecial(parsed->hash);

  if (is_special && strlen(parsed->host) > 0) {
    snprintf(parsed->href, href_len, "%s//%s%s%s%s%s", parsed->protocol, userinfo, parsed->host, final_pathname,
             final_search, final_hash);
  } else if (strlen(parsed->host) > 0) {
    snprintf(parsed->href, href_len, "%s//%s%s%s%s%s", parsed->protocol, userinfo, parsed->host, final_pathname,
             final_search, final_hash);
  } else if (is_special && !parsed->opaque_path) {
    // Special schemes with empty host (like file:///path) should include //
    snprintf(parsed->href, href_len, "%s//%s%s%s", parsed->protocol, final_pathname, final_search, final_hash);
  } else {
    // Non-special schemes or opaque paths (no authority)
    snprintf(parsed->href, href_len, "%s%s%s%s", parsed->protocol, final_pathname, final_search, final_hash);
  }

  // Clean up encoded strings
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

  // Handle fragment first
  if (fragment_pos) {
    free(parsed->hash);
    parsed->hash = strdup(fragment_pos);  // Include the #
    *fragment_pos = '\0';
  }

  // Handle query
  if (query_pos) {
    char* search_end = fragment_pos ? fragment_pos : ptr + strlen(ptr);
    size_t search_len = search_end - query_pos;
    free(parsed->search);
    parsed->search = malloc(search_len + 1);
    strncpy(parsed->search, query_pos, search_len);
    parsed->search[search_len] = '\0';
    *query_pos = '\0';
  }

  // Handle pathname
  if (strlen(ptr) > 0) {
    // Only update pathname if it hasn't been set by special case handling above
    if (strcmp(parsed->pathname, "/") == 0 || strcmp(parsed->pathname, "") == 0) {
      free(parsed->pathname);
      parsed->pathname = strdup(ptr);
    }
  } else if (is_special_scheme(parsed->protocol)) {
    // Special schemes should have "/" as default path if empty
    if (strcmp(parsed->pathname, "") == 0) {
      free(parsed->pathname);
      parsed->pathname = strdup("/");
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

  char* url_string = malloc(total_len);
  strcpy(url_string, protocol);

  // Add authority if host exists
  if (host && strlen(host) > 0) {
    strcat(url_string, "//");

    // Add userinfo if exists
    if (username && strlen(username) > 0) {
      char* encoded_username = url_userinfo_encode(username);
      strcat(url_string, encoded_username);
      free(encoded_username);

      if (has_password_field && password && strlen(password) > 0) {
        strcat(url_string, ":");
        char* encoded_password = url_userinfo_encode(password);
        strcat(url_string, encoded_password);
        free(encoded_password);
      }
      strcat(url_string, "@");
    }

    strcat(url_string, host);
  } else if (is_special && pathname && pathname[0] == '/') {
    // Special schemes with no host but absolute path need //
    strcat(url_string, "//");
  }

  // Add pathname
  if (pathname) {
    char* encoded_pathname = is_special ? url_component_encode(pathname) : strdup(pathname);
    strcat(url_string, encoded_pathname);
    free(encoded_pathname);
  }

  // Add search
  if (search && strlen(search) > 0) {
    char* encoded_search = url_component_encode(search);
    strcat(url_string, encoded_search);
    free(encoded_search);
  }

  // Add hash
  if (hash && strlen(hash) > 0) {
    char* encoded_hash = is_special ? url_fragment_encode(hash) : url_fragment_encode_nonspecial(hash);
    strcat(url_string, encoded_hash);
    free(encoded_hash);
  }

  return url_string;
}