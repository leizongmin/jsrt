#include <ctype.h>
#include "url.h"

// Forward declaration for relative URL resolution
static JSRT_URL* resolve_relative_url(const char* url, const char* base);

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

// Resolve relative URL against base URL
static JSRT_URL* resolve_relative_url(const char* url, const char* base) {
  // Parse base URL first
  JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
  if (!base_url || strlen(base_url->protocol) == 0) {
    if (base_url)
      JSRT_FreeURL(base_url);
    return NULL;  // Invalid base URL
  }

  // For non-special schemes (like test:, mailto:, data:), empty host is valid (opaque URLs)
  // Only require host for special schemes
  int is_special = is_special_scheme(base_url->protocol);
  if (is_special && strlen(base_url->host) == 0) {
    JSRT_FreeURL(base_url);
    return NULL;  // Special schemes require host
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
  } else if (url[0] == '/') {
    // Absolute path - parse the path to separate pathname, search, and hash
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

    // Now resolve the path component against the base
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

    free(path_copy);
  }

  // Normalize dot segments in the pathname
  char* normalized_pathname = normalize_dot_segments(result->pathname);
  if (normalized_pathname) {
    free(result->pathname);
    result->pathname = normalized_pathname;
  }

  // Build origin using compute_origin function to handle all scheme types correctly
  result->origin = compute_origin(result->protocol, result->hostname, result->port);

  // For non-special schemes, pathname should not be encoded (spaces preserved)
  // For special schemes, pathname should be encoded
  char* encoded_pathname;
  if (is_special_scheme(result->protocol)) {
    encoded_pathname = url_component_encode(result->pathname);
  } else {
    // Non-special schemes: preserve spaces and other characters in pathname
    encoded_pathname = strdup(result->pathname ? result->pathname : "");
  }
  char* encoded_search = url_component_encode(result->search);
  char* encoded_hash = url_fragment_encode(result->hash);

  // Build href - different logic for special vs non-special schemes
  if (is_special_scheme(result->protocol)) {
    // Special schemes: use origin-based construction (for authority-based URLs)
    size_t href_len =
        strlen(result->origin) + strlen(encoded_pathname) + strlen(encoded_search) + strlen(encoded_hash) + 1;

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
      if (result->password && strlen(result->password) > 0) {
        strcat(result->href, ":");
        strcat(result->href, result->password);
      }
      strcat(result->href, "@");

      // Add the host part (origin without "protocol://")
      const char* host_part = result->origin + strlen(protocol_and_slashes);
      strcat(result->href, host_part);

      free(protocol_and_slashes);
    } else {
      strcpy(result->href, result->origin);
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

// Main URL parsing function
JSRT_URL* JSRT_ParseURL(const char* url, const char* base) {
#ifdef DEBUG_URL_NORMALIZATION
  printf("DEBUG: JSRT_ParseURL called with URL: %s\n", url);
#endif

  // Strip leading and trailing ASCII whitespace, then remove all internal ASCII whitespace per WHATWG URL spec
  char* trimmed_url = strip_url_whitespace(url);
  if (!trimmed_url) {
#ifdef DEBUG_URL_NORMALIZATION
    printf("DEBUG: strip_url_whitespace failed\n");
#endif
    return NULL;
  }

#ifdef DEBUG_URL_NORMALIZATION
  printf("DEBUG: After strip_url_whitespace: %s\n", trimmed_url);
#endif

  // Check for null bytes in the URL - this is invalid per WHATWG URL spec
  if (strstr(trimmed_url, "%00")) {
    free(trimmed_url);
    return NULL;
  }

  // Determine if this URL has a special scheme first
  // We need to check this before removing internal whitespace
  char* initial_colon_pos = strchr(trimmed_url, ':');
  int has_special_scheme = 0;

  if (initial_colon_pos && initial_colon_pos > trimmed_url) {
    // Extract potential scheme
    size_t scheme_len = initial_colon_pos - trimmed_url;
    char* scheme = malloc(scheme_len + 1);
    strncpy(scheme, trimmed_url, scheme_len);
    scheme[scheme_len] = '\0';
    has_special_scheme = is_valid_scheme(scheme) && is_special_scheme(scheme);
    free(scheme);
  }

  // Remove internal ASCII whitespace (tab, LF, CR) for all schemes per WHATWG URL spec
  // Spaces are preserved but other whitespace is always removed
  char* cleaned_url = remove_all_ascii_whitespace(trimmed_url);
  free(trimmed_url);  // Free the intermediate result
  if (!cleaned_url)
    return NULL;

  // Handle empty URL string - per WHATWG URL spec, empty string should resolve to base
  if (strlen(cleaned_url) == 0) {
    if (base) {
      free(cleaned_url);
      return JSRT_ParseURL(base, NULL);  // Parse base URL as absolute
    } else {
      free(cleaned_url);
      return NULL;  // Empty URL with no base is invalid
    }
  }

// Validate URL characters first (but skip ASCII whitespace check since we already removed them)
#ifdef DEBUG_URL_NORMALIZATION
  printf("DEBUG: About to validate URL characters: %s\n", cleaned_url);
#endif
  if (!validate_url_characters(cleaned_url)) {
#ifdef DEBUG_URL_NORMALIZATION
    printf("DEBUG: validate_url_characters FAILED\n");
#endif
    free(cleaned_url);
    return NULL;  // Invalid characters detected
  }

  if (base && !validate_url_characters(base)) {
    free(cleaned_url);
    return NULL;  // Invalid characters in base URL
  }

  // Check if URL has a scheme (protocol)
  // A URL has a scheme if it contains ':' before any '/', '?', or '#'
  // AND the first character is a letter (per WHATWG URL Standard)
  int has_scheme = 0;
  char* colon_pos = NULL;

  // First character must be a letter for valid scheme
  if (cleaned_url[0] &&
      ((cleaned_url[0] >= 'a' && cleaned_url[0] <= 'z') || (cleaned_url[0] >= 'A' && cleaned_url[0] <= 'Z'))) {
    // Look for "://" pattern to find the correct scheme boundary
    char* scheme_end = strstr(cleaned_url, "://");
    if (scheme_end) {
      // Found "://" - extract scheme and validate it
      colon_pos = scheme_end;  // Points to the ':' in "://"
      has_scheme = 1;
    } else {
      // No "://" found, look for single colon
      for (const char* p = cleaned_url; *p; p++) {
        if (*p == ':') {
          // Found colon - always accept scheme if it follows basic format rules
          colon_pos = (char*)p;
          has_scheme = 1;
          break;
        } else if (*p == '/' || *p == '?' || *p == '#') {
          break;  // No colon found before path/query/fragment
        }
      }
    }
  }

  // Handle protocol-relative URLs (starting with "//")
  if (strncmp(cleaned_url, "//", 2) == 0) {
    if (base) {
      // Protocol-relative URL: inherit protocol from base, parse authority from URL
      JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
      if (!base_url) {
        free(cleaned_url);
        return NULL;
      }

      // Create a full URL by combining base protocol with the protocol-relative URL
      size_t full_url_len = strlen(base_url->protocol) + strlen(cleaned_url) + 2;
      char* full_url = malloc(full_url_len);
      if (!full_url) {
        JSRT_FreeURL(base_url);
        free(cleaned_url);
        return NULL;
      }
      snprintf(full_url, full_url_len, "%s%s", base_url->protocol, cleaned_url);
      JSRT_FreeURL(base_url);
      free(cleaned_url);

      // Parse the full URL recursively (without base to avoid infinite recursion)
      JSRT_URL* result = JSRT_ParseURL(full_url, NULL);
      free(full_url);
      return result;
    } else {
      // No base URL - treat as absolute URL with default scheme
      char* full_url = malloc(strlen("file:") + strlen(cleaned_url) + 1);
      if (!full_url) {
        free(cleaned_url);
        return NULL;
      }
      strcpy(full_url, "file:");
      strcat(full_url, cleaned_url);

      free(cleaned_url);
      JSRT_URL* result = JSRT_ParseURL(full_url, NULL);
      free(full_url);
      return result;
    }
  }

  // Handle other relative URLs with base
  if (base && (cleaned_url[0] == '/' || (!has_scheme && cleaned_url[0] != '\0'))) {
    JSRT_URL* result = resolve_relative_url(cleaned_url, base);
    free(cleaned_url);
    return result;
  }

  // If we reach here, we need to parse as absolute URL
  JSRT_URL* parsed = malloc(sizeof(JSRT_URL));
  memset(parsed, 0, sizeof(JSRT_URL));

  // Initialize with empty strings to prevent NULL dereference
  parsed->href = strdup(cleaned_url);
  parsed->protocol = strdup("");
  parsed->username = strdup("");
  parsed->password = strdup("");
  parsed->host = strdup("");
  parsed->hostname = strdup("");
  parsed->port = strdup("");
  parsed->pathname = strdup("");
  parsed->search = strdup("");
  parsed->hash = strdup("");
  parsed->origin = strdup("");
  parsed->search_params = JS_UNDEFINED;
  parsed->ctx = NULL;

  // Parse the URL components
  char* url_copy = strdup(cleaned_url);
  char* ptr = url_copy;

  // Parse protocol (scheme)
  char* scheme_colon = strchr(ptr, ':');
  if (scheme_colon && scheme_colon > ptr) {
    size_t protocol_len = scheme_colon - ptr + 1;  // Include the colon
    free(parsed->protocol);
    parsed->protocol = malloc(protocol_len + 1);
    strncpy(parsed->protocol, ptr, protocol_len);
    parsed->protocol[protocol_len] = '\0';

    // Move past protocol
    ptr = scheme_colon + 1;

    // Skip "//" if present
    if (strncmp(ptr, "//", 2) == 0) {
      ptr += 2;

      // Parse hostname, port, username, password
      char* authority_end = ptr;
      // Find end of authority (start of path, query, or fragment)
      while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#') {
        authority_end++;
      }

      // Extract authority part
      size_t authority_len = authority_end - ptr;
      char* authority = malloc(authority_len + 1);
      strncpy(authority, ptr, authority_len);
      authority[authority_len] = '\0';

      // Parse username:password@hostname:port
      char* at_pos = strchr(authority, '@');
      char* host_part = authority;

      if (at_pos) {
        // Has credentials
        *at_pos = '\0';
        char* cred_colon = strchr(authority, ':');
        if (cred_colon) {
          *cred_colon = '\0';
          free(parsed->username);
          parsed->username = strdup(authority);
          free(parsed->password);
          parsed->password = strdup(cred_colon + 1);
        } else {
          free(parsed->username);
          parsed->username = strdup(authority);
        }
        host_part = at_pos + 1;
      }

      // Parse hostname:port
      char* port_colon = strrchr(host_part, ':');
      if (port_colon && strchr(host_part, '[') == NULL) {  // Not IPv6
        *port_colon = '\0';
        free(parsed->hostname);
        parsed->hostname = strdup(host_part);
        free(parsed->port);
        parsed->port = strdup(port_colon + 1);

        // Set host (hostname:port format)
        free(parsed->host);
        if (strlen(parsed->port) > 0) {
          parsed->host = malloc(strlen(parsed->hostname) + strlen(parsed->port) + 2);
          sprintf(parsed->host, "%s:%s", parsed->hostname, parsed->port);
        } else {
          parsed->host = strdup(parsed->hostname);
        }
      } else {
        free(parsed->hostname);
        parsed->hostname = strdup(host_part);
        free(parsed->host);
        parsed->host = strdup(host_part);
      }

      free(authority);
      ptr = authority_end;
    }
  }

  // Parse path, query, fragment
  char* query_pos = strchr(ptr, '?');
  char* fragment_pos = strchr(ptr, '#');

  // Handle fragment
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

  // Remaining is pathname
  if (strlen(ptr) > 0) {
    free(parsed->pathname);
    parsed->pathname = strdup(ptr);
  } else {
    free(parsed->pathname);
    parsed->pathname = strdup("/");
  }

  // Compute origin
  free(parsed->origin);
  parsed->origin = compute_origin(parsed->protocol, parsed->hostname, parsed->port);

  // Build href
  free(parsed->href);
  size_t href_len = strlen(parsed->protocol) + 2 + strlen(parsed->host) + strlen(parsed->pathname) +
                    strlen(parsed->search) + strlen(parsed->hash) + 10;
  parsed->href = malloc(href_len);
  snprintf(parsed->href, href_len, "%s//%s%s%s%s", parsed->protocol, parsed->host, parsed->pathname, parsed->search,
           parsed->hash);

  free(url_copy);
  free(cleaned_url);
  return parsed;
}