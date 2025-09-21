#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "../url.h"

// Parse URL components after preprocessing - returns pointer to remaining path/query/fragment
char* parse_url_components(JSRT_URL* parsed, const char* scheme, char* ptr) {
  if (!parsed || !scheme || !ptr)
    return NULL;

  JSRT_Debug("parse_url_components: scheme='%s', ptr='%s'", scheme, ptr);

  // Check if this is a special scheme
  int is_special = is_special_scheme(scheme);

  // Special validation for blob URLs
  if (strcmp(scheme, "blob") == 0) {
    // Blob URLs are valid even when empty (blob:) per WHATWG URL spec
    // Only reject truly invalid cases

    // Blob URLs can contain various formats:
    // 1. blob:http://example.com/uuid (with inner URL)
    // 2. blob:d3958f5c-0777-0845-9dcf-2cb28783acaf (UUID only)
    // 3. blob: (empty)
    // 4. blob:about:blank, blob:ws://example.org/, etc.
    // We should accept all formats, validation is done elsewhere

    // file: URLs are not allowed as blob inner URLs per WPT tests
    if (strncmp(ptr, "file:", 5) == 0) {
      return NULL;
    }
  }

  // Handle different URL formats

  if (strncmp(ptr, "//", 2) == 0) {
    parsed->has_authority_syntax = 1;  // Mark as authority-based syntax
    return parse_authority_based_url_with_position(parsed, scheme, ptr, is_special);
  } else if (is_special && *ptr == '/' && *(ptr + 1) != '/') {
    // Special case: "http:/example.com/" should become "http://example.com/"
    if (parse_special_scheme_single_slash(parsed, &ptr) != 0) {
      return NULL;
    }
    return ptr;
  } else if (strcmp(scheme, "file") == 0 && *ptr != '/' && *ptr != '\\') {
    // File URLs without slashes should be normalized to absolute paths
    // According to WHATWG URL spec, file:path becomes file:///normalized_path

    // Set empty hostname and host for file URLs
    free(parsed->hostname);
    parsed->hostname = strdup("");
    free(parsed->host);
    parsed->host = strdup("");

    // Extract just the path portion (before ? or #) for processing
    char* query_pos = strchr(ptr, '?');
    char* fragment_pos = strchr(ptr, '#');
    char* path_end = NULL;

    if (query_pos && fragment_pos) {
      path_end = (query_pos < fragment_pos) ? query_pos : fragment_pos;
    } else if (query_pos) {
      path_end = query_pos;
    } else if (fragment_pos) {
      path_end = fragment_pos;
    }

    // Create a copy of just the path portion
    char* path_only = NULL;
    if (path_end) {
      size_t path_part_len = path_end - ptr;
      path_only = malloc(path_part_len + 1);
      if (!path_only) {
        return NULL;
      }
      strncpy(path_only, ptr, path_part_len);
      path_only[path_part_len] = '\0';
    } else {
      path_only = strdup(ptr);
    }

    // Normalize the path component
    char* normalized_path = NULL;
    if (strcmp(path_only, "") == 0 || strcmp(path_only, ".") == 0) {
      // file: or file:. becomes file:/// (root directory)
      normalized_path = strdup("/");
    } else if (strcmp(path_only, "..") == 0) {
      // file:.. becomes file:/// (root directory, can't go above root)
      normalized_path = strdup("/");
    } else if (strncmp(path_only, "..", 2) == 0 && path_only[2] != '\0') {
      // file:..something becomes file:///..something
      size_t path_len = strlen(path_only) + 2;  // +1 for leading slash, +1 for null terminator
      normalized_path = malloc(path_len);
      if (!normalized_path) {
        free(path_only);
        return NULL;
      }
      snprintf(normalized_path, path_len, "/%s", path_only);
    } else {
      // file:path becomes file:///path
      size_t path_len = strlen(path_only) + 2;  // +1 for leading slash, +1 for null terminator
      normalized_path = malloc(path_len);
      if (!normalized_path) {
        free(path_only);
        return NULL;
      }
      snprintf(normalized_path, path_len, "/%s", path_only);
    }

    free(path_only);
    free(parsed->pathname);
    parsed->pathname = normalized_path;

    // Don't mark as opaque path - this is a regular file URL with authority syntax
    parsed->opaque_path = 0;
    parsed->has_authority_syntax = 1;

    // Handle query and fragment portions directly since we've already set the pathname
    if (path_end) {
      char* remaining = path_end;
      char* query_start = NULL;
      char* fragment_start = NULL;

      if (*remaining == '?') {
        query_start = remaining;
        char* fragment_pos = strchr(remaining, '#');
        if (fragment_pos) {
          fragment_start = fragment_pos;
          // Null-terminate query portion
          *fragment_pos = '\0';
        }

        // Set search (query) component
        free(parsed->search);
        parsed->search = strdup(query_start);

        // Restore the string if we null-terminated it
        if (fragment_start) {
          *fragment_start = '#';
          // Set hash (fragment) component
          free(parsed->hash);
          parsed->hash = strdup(fragment_start);
        }
      } else if (*remaining == '#') {
        fragment_start = remaining;
        // Set hash (fragment) component
        free(parsed->hash);
        parsed->hash = strdup(fragment_start);
      }
    }

    // Return pointer to end since we've handled everything
    return ptr + strlen(ptr);
  } else if (is_special) {
    // Special schemes: handle various formats like "http:example.com/" or "http::@host:port"
    if (parse_special_scheme_without_slashes(parsed, &ptr) != 0) {
      return NULL;
    }
    return ptr;
  }

  return ptr;  // Return original pointer for non-authority URLs
}

// Parse authority-based URLs (scheme://authority/path) - returns position after authority
char* parse_authority_based_url_with_position(JSRT_URL* parsed, const char* scheme, char* ptr, int is_special) {
  // Check if this is a non-special scheme with empty authority (like "foo://")
  if (!is_special && (*(ptr + 2) == '\0' || *(ptr + 2) == '?' || *(ptr + 2) == '#')) {
    // For non-special schemes like "foo://", set empty host and empty pathname
    free(parsed->hostname);
    parsed->hostname = strdup("");
    free(parsed->host);
    parsed->host = strdup("");
    free(parsed->pathname);
    parsed->pathname = strdup("");     // Keep pathname empty for foo://
    parsed->has_authority_syntax = 1;  // Mark as authority-based syntax

    // Return pointer after "//" - no further path parsing needed
    return ptr + 2;  // Skip "//" and return position for path parsing
  }

  // Special case for file URLs: "file:.//p" should be treated as path, not authority
  if (is_special && (strcmp(scheme, "file") == 0 || strcmp(scheme, "file:") == 0) &&
      (strncmp(ptr, "//.", 3) == 0 || (ptr[0] == '.' && strncmp(ptr + 1, "//", 2) == 0))) {
    // For file URLs like "file:.//p", treat ".//p" as a path that needs normalization
    return ptr;  // Return original position to be parsed as pathname
  }

  // Standard format: scheme://authority/path
  ptr += 2;

  // Handle empty authority case: if next character is '/', parse next segment as hostname
  if (*ptr == '/') {
    if (parse_empty_authority_url(parsed, scheme, &ptr) != 0) {
      return NULL;
    }
    return ptr;
  } else {
    if (parse_standard_authority_url(parsed, &ptr) != 0) {
      return NULL;
    }
    return ptr;
  }
}

// Parse authority-based URLs (scheme://authority/path) - legacy interface
int parse_authority_based_url(JSRT_URL* parsed, const char* scheme, char* ptr, int is_special) {
  char* result = parse_authority_based_url_with_position(parsed, scheme, ptr, is_special);
  return result ? 0 : -1;
}

// Parse URLs with empty authority (like scheme:///path)
int parse_empty_authority_url(JSRT_URL* parsed, const char* scheme, char** ptr) {
  // For both file URLs and non-special schemes: "scheme:///path" should have empty hostname and path="/path"
  if (strcmp(scheme, "file") == 0 || !is_special_scheme(scheme)) {
    // Set empty hostname and host
    free(parsed->hostname);
    parsed->hostname = strdup("");
    free(parsed->host);
    parsed->host = strdup("");
    // The remaining part (starting with /) is the path
    // ptr currently points to "/path"
    return 0;
  } else {
    // Special schemes only: URLs like "http:///test" should become "http://test/"
    return parse_empty_authority_with_path(parsed, ptr);
  }
}

// Parse URLs with standard authority section
int parse_standard_authority_url(JSRT_URL* parsed, char** ptr) {
  JSRT_Debug("parse_standard_authority_url: ptr='%s'", *ptr);
  // Special case: for double colon @ pattern (::@...@...), handle directly
  if ((*ptr)[0] == ':' && (*ptr)[1] == ':' && strchr(*ptr, '@')) {
    return parse_double_colon_at_pattern(parsed, ptr);
  } else {
    int result = parse_normal_authority(parsed, ptr);
    if (result != 0) {
      JSRT_Debug("parse_standard_authority_url: parse_normal_authority failed");
    }
    return result;
  }
}

// Parse double colon @ pattern (::@host@host)
int parse_double_colon_at_pattern(JSRT_URL* parsed, char** ptr) {
  // Set flag to indicate this special pattern for origin calculation
  parsed->double_colon_at_pattern = 1;

  // For patterns like ::@c@d:2, use last @ to separate userinfo from host per WPT spec
  char* last_at = NULL;
  char* temp_ptr = *ptr;
  while (*temp_ptr) {
    if (*temp_ptr == '@') {
      last_at = temp_ptr;
    }
    temp_ptr++;
  }

  if (!last_at) {
    return -1;  // No @ found, should not happen
  }

  char* authority_end = find_authority_end(*ptr, last_at);

  // Parse userinfo (everything before last @)
  size_t userinfo_len = last_at - *ptr;
  char* userinfo = malloc(userinfo_len + 1);
  if (!userinfo) {
    return -1;
  }
  strncpy(userinfo, *ptr, userinfo_len);
  userinfo[userinfo_len] = '\0';

  // Parse userinfo as username:password
  char* colon_in_userinfo = strchr(userinfo, ':');
  if (colon_in_userinfo) {
    *colon_in_userinfo = '\0';
    free(parsed->username);
    parsed->username = strdup(userinfo);
    free(parsed->password);
    parsed->password = strdup(colon_in_userinfo + 1);
    parsed->has_password_field = 1;
  } else {
    free(parsed->username);
    parsed->username = strdup(userinfo);
    parsed->has_password_field = 0;
  }
  free(userinfo);

  // Parse host part (everything after last @, before authority_end)
  char* host_start = last_at + 1;
  size_t host_len = authority_end - host_start;
  char* host_part = malloc(host_len + 1);
  if (!host_part) {
    // userinfo was already freed at line 260, don't double-free
    return -1;
  }
  strncpy(host_part, host_start, host_len);
  host_part[host_len] = '\0';

  // Parse host:port (find rightmost colon for port)
  char* port_colon = strrchr(host_part, ':');
  if (port_colon) {
    *port_colon = '\0';
    free(parsed->hostname);
    parsed->hostname = strdup(host_part);

    // Validate hostname with @ characters allowed for double colon pattern
    if (!validate_hostname_characters_allow_at(parsed->hostname, 1)) {
      free(host_part);
      return -1;
    }

    // Try to canonicalize as IPv4 address if it looks like one
    char* ipv4_canonical = canonicalize_ipv4_address(parsed->hostname);
    if (ipv4_canonical) {
      // Replace hostname with canonical IPv4 form
      free(parsed->hostname);
      parsed->hostname = ipv4_canonical;
    } else if (looks_like_ipv4_address(parsed->hostname)) {
      // Hostname looks like IPv4 but failed canonicalization - this means invalid IPv4
      // According to WHATWG URL spec, this should fail URL parsing
      free(host_part);
      return -1;
    }

    // Special handling for file URLs with localhost
    if (strcmp(parsed->protocol, "file:") == 0 && strcmp(parsed->hostname, "localhost") == 0) {
      // For file URLs, localhost should be normalized to empty hostname
      free(parsed->hostname);
      parsed->hostname = strdup("");
    }

    // Parse and normalize port
    char* port_str = port_colon + 1;
    char* normalized_port = normalize_port(port_str, parsed->protocol);
    if (normalized_port) {
      free(parsed->port);
      parsed->port = normalized_port;

      // Set host field
      free(parsed->host);
      if (strlen(normalized_port) > 0) {
        size_t full_host_len = strlen(parsed->hostname) + strlen(normalized_port) + 2;
        parsed->host = malloc(full_host_len);
        if (!parsed->host) {
          free(host_part);
          return -1;
        }
        snprintf(parsed->host, full_host_len, "%s:%s", parsed->hostname, normalized_port);
      } else {
        parsed->host = strdup(parsed->hostname);
      }
    } else {
      free(host_part);
      return -1;
    }
  } else {
    free(parsed->hostname);
    parsed->hostname = strdup(host_part);

    // Validate hostname with @ characters allowed for double colon pattern
    if (!validate_hostname_characters_allow_at(parsed->hostname, 1)) {
      free(host_part);
      return -1;
    }

    // Try to canonicalize as IPv4 address if it looks like one
    char* ipv4_canonical = canonicalize_ipv4_address(parsed->hostname);
    if (ipv4_canonical) {
      // Replace hostname with canonical IPv4 form
      free(parsed->hostname);
      parsed->hostname = ipv4_canonical;
    } else if (looks_like_ipv4_address(parsed->hostname)) {
      // Hostname looks like IPv4 but failed canonicalization - this means invalid IPv4
      // According to WHATWG URL spec, this should fail URL parsing
      free(host_part);
      return -1;
    }

    // Special handling for file URLs with localhost
    if (strcmp(parsed->protocol, "file:") == 0 && strcmp(parsed->hostname, "localhost") == 0) {
      // For file URLs, localhost should be normalized to empty hostname
      free(parsed->hostname);
      parsed->hostname = strdup("");
    }

    free(parsed->host);
    parsed->host = strdup(parsed->hostname);
  }
  free(host_part);

  *ptr = authority_end;
  return 0;
}

// Parse normal authority section
int parse_normal_authority(JSRT_URL* parsed, char** ptr) {
  // First find the authority boundary (before first '/', '?', or '#')
  char* authority_boundary = *ptr;
  while (*authority_boundary && *authority_boundary != '/' && *authority_boundary != '?' &&
         *authority_boundary != '#') {
    authority_boundary++;
  }

  // Scan only within the authority section to find the rightmost '@' symbol
  char* rightmost_at = NULL;
  char* temp_ptr = *ptr;
  while (temp_ptr < authority_boundary) {
    if (*temp_ptr == '@') {
      rightmost_at = temp_ptr;
    }
    temp_ptr++;
  }

  char* authority_end = find_authority_end(*ptr, rightmost_at);

  if (authority_end > *ptr) {
    size_t authority_len = authority_end - *ptr;
    char* authority = malloc(authority_len + 1);
    strncpy(authority, *ptr, authority_len);
    authority[authority_len] = '\0';

    if (parse_authority(parsed, authority) != 0) {
      JSRT_Debug("parse_normal_authority: parse_authority failed for '%s'", authority);
      free(authority);
      return -1;
    }
    free(authority);
  } else {
    // For empty authority, we need to validate whether this is allowed
    // Special schemes (except file:) must have non-empty hostnames
    if (parsed->protocol && is_special_scheme(parsed->protocol) && strcmp(parsed->protocol, "file:") != 0) {
      return -1;  // Empty authority for special scheme (not file:) is invalid
    }

    // For non-special schemes or file:, set empty hostname and host
    free(parsed->hostname);
    parsed->hostname = strdup("");
    free(parsed->host);
    parsed->host = strdup("");
    free(parsed->port);
    parsed->port = strdup("");
  }

  *ptr = authority_end;
  return 0;
}