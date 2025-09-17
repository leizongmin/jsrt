#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "url.h"

// Detect URL scheme and return scheme and remainder
int detect_url_scheme(const char* url, char** scheme, char** remainder) {
  if (!url || !scheme || !remainder)
    return -1;

  *scheme = NULL;
  *remainder = NULL;

  // Check for scheme and handle special cases
  char* scheme_colon = strchr(url, ':');

  // URL schemes must start with a letter (per RFC 3986)
  // Paths starting with / should never be treated as having a scheme
  if (scheme_colon && scheme_colon > url && url[0] != '/' &&
      ((url[0] >= 'a' && url[0] <= 'z') || (url[0] >= 'A' && url[0] <= 'Z'))) {
    size_t scheme_len = scheme_colon - url;
    *scheme = malloc(scheme_len + 1);
    strncpy(*scheme, url, scheme_len);
    (*scheme)[scheme_len] = '\0';
    *remainder = scheme_colon + 1;
    return 0;
  }

  return -1;  // No scheme found
}

// Parse URL components after preprocessing - returns pointer to remaining path/query/fragment
char* parse_url_components(JSRT_URL* parsed, const char* scheme, char* ptr) {
  if (!parsed || !scheme || !ptr)
    return NULL;

  // Check if this is a special scheme
  int is_special = is_special_scheme(scheme);

  // Special validation for blob URLs
  if (strcmp(scheme, "blob") == 0) {
    // blob URLs must have a valid inner URL after "blob:"
    if (strlen(ptr) == 0 || strcmp(ptr, "blob:") == 0) {
      return NULL;
    }

    // The inner URL should be an absolute URL with a valid scheme
    if (!(strncmp(ptr, "http://", 7) == 0 || strncmp(ptr, "https://", 8) == 0 || strncmp(ptr, "ftp://", 6) == 0 ||
          strncmp(ptr, "ws://", 5) == 0 || strncmp(ptr, "wss://", 6) == 0)) {
      return NULL;
    }

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
    // File URLs without slashes should be treated as opaque paths
    // Set the pathname to the entire remaining content
    free(parsed->pathname);
    parsed->pathname = strdup(ptr);

    // Clear hostname since this is an opaque path
    free(parsed->hostname);
    parsed->hostname = strdup("");
    free(parsed->host);
    parsed->host = strdup("");

    // Mark as opaque path for href building
    parsed->opaque_path = 1;

    // Return pointer to end since we consumed everything
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
  // Special case for file URLs: "file:///path" should have empty hostname and path="/path"
  if (strcmp(scheme, "file") == 0) {
    // Set empty hostname and host
    free(parsed->hostname);
    parsed->hostname = strdup("");
    free(parsed->host);
    parsed->host = strdup("");
    // The remaining part (starting with /) is the path
    // ptr currently points to "/path"
    return 0;
  } else {
    // Special case for URLs like "///test" which should become "http://test/"
    return parse_empty_authority_with_path(parsed, ptr);
  }
}

// Parse URLs with standard authority section
int parse_standard_authority_url(JSRT_URL* parsed, char** ptr) {
  // Special case: for double colon @ pattern (::@...@...), handle directly
  if ((*ptr)[0] == ':' && (*ptr)[1] == ':' && strchr(*ptr, '@')) {
    return parse_double_colon_at_pattern(parsed, ptr);
  } else {
    return parse_normal_authority(parsed, ptr);
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
      free(authority);
      return -1;
    }

    free(authority);
    *ptr = authority_end;
  }

  return 0;
}

// Create and initialize a new URL structure
JSRT_URL* create_url_structure(void) {
  JSRT_URL* parsed = malloc(sizeof(JSRT_URL));
  if (!parsed)
    return NULL;

  memset(parsed, 0, sizeof(JSRT_URL));

  // Initialize with empty strings to prevent NULL dereference
  parsed->href = strdup("");
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
  parsed->has_password_field = 0;
  parsed->double_colon_at_pattern = 0;
  parsed->opaque_path = 0;
  parsed->has_authority_syntax = 0;

  return parsed;
}

// Main URL parsing function (refactored)
JSRT_URL* parse_absolute_url(const char* preprocessed_url) {
  JSRT_URL* parsed = create_url_structure();
  if (!parsed)
    return NULL;

  char* url_copy = strdup(preprocessed_url);
  char* ptr = url_copy;

  // Detect scheme
  char* scheme = NULL;
  char* remainder = NULL;
  if (detect_url_scheme(ptr, &scheme, &remainder) != 0) {
    free(url_copy);
    JSRT_FreeURL(parsed);
    return NULL;
  }

  // Normalize scheme to lowercase per WHATWG URL spec
  if (scheme) {
    for (size_t i = 0; scheme[i] != '\0'; i++) {
      scheme[i] = tolower(scheme[i]);
    }
  }

  // Validate scheme according to WHATWG URL spec
  if (!is_valid_scheme(scheme)) {
    free(scheme);
    free(url_copy);
    JSRT_FreeURL(parsed);
    return NULL;
  }

  // Special validation for special schemes without proper authority
  if (is_special_scheme(scheme)) {
    // Special schemes like http, https, ftp MUST have authority (start with //)
    // Exception: file: URLs can have various formats
    if (!remainder || strlen(remainder) == 0) {
      // Case like "http:" - special scheme with no remainder at all
      free(scheme);
      free(url_copy);
      JSRT_FreeURL(parsed);
      return NULL;
    } else if (strncmp(remainder, "//", 2) != 0) {
      // Case like "http:something" without // - this should fail for special schemes
      // Exception: file: URLs allow "file:path" format
      if (strcmp(scheme, "file") != 0) {
        free(scheme);
        free(url_copy);
        JSRT_FreeURL(parsed);
        return NULL;
      }
    }
  }

  // Set protocol (scheme + ":") - normalize scheme to lowercase
  free(parsed->protocol);
  parsed->protocol = malloc(strlen(scheme) + 2);
  snprintf(parsed->protocol, strlen(scheme) + 2, "%s:", scheme);
  // Normalize scheme to lowercase per WHATWG URL spec
  for (size_t i = 0; parsed->protocol[i] != ':' && parsed->protocol[i] != '\0'; i++) {
    parsed->protocol[i] = tolower(parsed->protocol[i]);
  }

  // Parse URL components and get pointer to path/query/fragment section
  char* path_start = parse_url_components(parsed, scheme, remainder);
  if (!path_start) {
    free(scheme);
    free(url_copy);
    JSRT_FreeURL(parsed);
    return NULL;
  }

  // Parse path, query, fragment from the remaining part
  parse_path_query_fragment(parsed, path_start);

  free(scheme);

  // For file URLs without authority (like "file:p"), set opaque_path flag
  if (strcmp(parsed->protocol, "file:") == 0 && strlen(parsed->host) == 0 && parsed->pathname &&
      parsed->pathname[0] != '/') {
    parsed->opaque_path = 1;
  }

  // Handle file URL Windows drive letters
  handle_file_url_drive_letters(parsed);

  // Ensure special schemes have "/" as pathname if empty
  if (is_special_scheme(parsed->protocol) && strcmp(parsed->pathname, "") == 0) {
    free(parsed->pathname);
    parsed->pathname = strdup("/");
  }

  // Normalize dot segments in the pathname for ALL schemes
  // According to WHATWG URL spec, dot segment normalization applies to all schemes
  if (parsed->pathname) {
    char* normalized_pathname = normalize_dot_segments_with_percent_decoding(parsed->pathname);
    if (normalized_pathname) {
      free(parsed->pathname);
      parsed->pathname = normalized_pathname;
    }
  }

  // Normalize Windows drive letters in file URL pathnames
  if (parsed->protocol && strcmp(parsed->protocol, "file:") == 0 && parsed->pathname) {
    char* normalized_drive = normalize_windows_drive_letters(parsed->pathname);
    if (normalized_drive == NULL) {
      // Invalid drive letter pattern (e.g., double pipes)
      free(scheme);
      free(url_copy);
      JSRT_FreeURL(parsed);
      return NULL;
    }
    if (normalized_drive != parsed->pathname) {  // Check if it was actually changed
      free(parsed->pathname);
      parsed->pathname = normalized_drive;
    }
  }

  // Compute origin
  free(parsed->origin);
  parsed->origin = compute_origin_with_pathname(parsed->protocol, parsed->hostname, parsed->port,
                                                parsed->double_colon_at_pattern, parsed->pathname);

  // Build href
  build_href(parsed);

  free(url_copy);
  return parsed;
}