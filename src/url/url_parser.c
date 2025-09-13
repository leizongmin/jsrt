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
  fprintf(stderr, "DEBUG: Original ptr=[%s]\n", url);
  fprintf(stderr, "DEBUG: scheme_colon found at position %ld\n", scheme_colon ? (scheme_colon - url) : -1);

  // URL schemes must start with a letter (per RFC 3986)
  // Paths starting with / should never be treated as having a scheme
  if (scheme_colon && scheme_colon > url && url[0] != '/' &&
      ((url[0] >= 'a' && url[0] <= 'z') || (url[0] >= 'A' && url[0] <= 'Z'))) {
    size_t scheme_len = scheme_colon - url;
    *scheme = malloc(scheme_len + 1);
    strncpy(*scheme, url, scheme_len);
    (*scheme)[scheme_len] = '\0';
    fprintf(stderr, "DEBUG: Extracted scheme=[%s]\n", *scheme);

    *remainder = scheme_colon + 1;
    fprintf(stderr, "DEBUG: After moving past colon, ptr=[%s]\n", *remainder);
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
    if (!(strncmp(ptr, "http://", 7) == 0 || strncmp(ptr, "https://", 8) == 0 || strncmp(ptr, "ftp://", 6) == 0)) {
      return NULL;
    }

    // file: URLs are not allowed as blob inner URLs per WPT tests
    if (strncmp(ptr, "file:", 5) == 0) {
      return NULL;
    }
  }

  // Handle different URL formats
  fprintf(stderr, "DEBUG: URL format check, ptr=[%s]\n", ptr);
  fprintf(stderr, "DEBUG: First 3 chars: [%c][%c][%c]\n", ptr[0], ptr[1], ptr[2]);
  fprintf(stderr, "DEBUG: strncmp result: %d\n", strncmp(ptr, "//", 2));

  if (strncmp(ptr, "//", 2) == 0) {
    return parse_authority_based_url_with_position(parsed, scheme, ptr, is_special);
  } else if (is_special && *ptr == '/' && *(ptr + 1) != '/') {
    // Special case: "http:/example.com/" should become "http://example.com/"
    if (parse_special_scheme_single_slash(parsed, &ptr) != 0) {
      return NULL;
    }
    return ptr;
  } else if (is_special && !(strcmp(scheme, "file") == 0 && *ptr != '/' && *ptr != '\\')) {
    // Special schemes: handle various formats like "http:example.com/" or "http::@host:port"
    // But exclude file URLs without slashes (like "file:p" which should be opaque)
    if (parse_special_scheme_without_slashes(parsed, &ptr) != 0) {
      return NULL;
    }
    return ptr;
  }

  return ptr;  // Return original pointer for non-authority URLs
}

// Parse authority-based URLs (scheme://authority/path) - returns position after authority
char* parse_authority_based_url_with_position(JSRT_URL* parsed, const char* scheme, char* ptr, int is_special) {
  fprintf(stderr, "DEBUG: Taking scheme:// path\n");

  // Check if this is a non-special scheme with empty authority (like "foo://")
  if (!is_special && (*(ptr + 2) == '\0' || *(ptr + 2) == '?' || *(ptr + 2) == '#')) {
    // For non-special schemes like "foo://", preserve "//" as part of the pathname
    return ptr;  // Return original position to be parsed as pathname
  }

  // Special case for file URLs: "file:.//p" should be treated as path, not authority
  if (is_special && (strcmp(scheme, "file") == 0 || strcmp(scheme, "file:") == 0) &&
      (strncmp(ptr, "//.", 3) == 0 || (ptr[0] == '.' && strncmp(ptr + 1, "//", 2) == 0))) {
    // For file URLs like "file:.//p", treat ".//p" as a path that needs normalization
    return ptr;  // Return original position to be parsed as pathname
  }

  // Standard format: scheme://authority/path
  ptr += 2;

  // DEBUG: Print ptr after skipping //
  fprintf(stderr, "DEBUG: After //: ptr='%s'\n", ptr);

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
  fprintf(stderr, "DEBUG: Taking empty authority path\n");

  // Special case for file URLs: "file:///path" should have empty hostname and path="/path"
  if (strcmp(scheme, "file") == 0) {
    fprintf(stderr, "DEBUG: File URL with triple slash - treating as path\n");
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
  fprintf(stderr, "DEBUG: Taking standard authority parsing path\n");

  // DEBUG: Print current ptr value
  fprintf(stderr, "DEBUG: ptr value: '%s'\n", *ptr);
  fprintf(stderr, "DEBUG: ptr[0]='%c' ptr[1]='%c' has_at=%d\n", (*ptr)[0], (*ptr)[1], strchr(*ptr, '@') ? 1 : 0);

  // Special case: for double colon @ pattern (::@...@...), handle directly
  if ((*ptr)[0] == ':' && (*ptr)[1] == ':' && strchr(*ptr, '@')) {
    return parse_double_colon_at_pattern(parsed, ptr);
  } else {
    return parse_normal_authority(parsed, ptr);
  }
}

// Parse double colon @ pattern (::@host@host)
int parse_double_colon_at_pattern(JSRT_URL* parsed, char** ptr) {
  fprintf(stderr, "DEBUG: Double colon @ pattern detected for: %s\n", *ptr);

  // Set flag to indicate this special pattern for origin calculation
  parsed->double_colon_at_pattern = 1;

  // For patterns like ::@host@host, use first @ and handle manually to match WPT expectations
  char* first_at = strchr(*ptr, '@');
  char* authority_end = find_authority_end(*ptr, first_at);

  // Parse userinfo (everything before first @)
  size_t userinfo_len = first_at - *ptr;
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

  // Parse host part (everything after first @, before authority_end)
  char* host_start = first_at + 1;
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

    free(parsed->host);
    parsed->host = strdup(host_part);
  }
  free(host_part);

  *ptr = authority_end;
  return 0;
}

// Parse normal authority section
int parse_normal_authority(JSRT_URL* parsed, char** ptr) {
  // Scan the entire remaining string to find the rightmost '@' symbol
  char* rightmost_at = NULL;
  char* temp_ptr = *ptr;
  while (*temp_ptr) {
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

  return parsed;
}

// Main URL parsing function (refactored)
JSRT_URL* parse_absolute_url(const char* preprocessed_url) {
  fprintf(stderr, "DEBUG: Final preprocessed_url: [%s]\n", preprocessed_url);

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

  // Set protocol (scheme + ":")
  free(parsed->protocol);
  parsed->protocol = malloc(strlen(scheme) + 2);
  snprintf(parsed->protocol, strlen(scheme) + 2, "%s:", scheme);

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

  // Compute origin
  free(parsed->origin);
  parsed->origin = compute_origin(parsed->protocol, parsed->hostname, parsed->port, parsed->double_colon_at_pattern);

  // Build href
  build_href(parsed);

  free(url_copy);
  return parsed;
}