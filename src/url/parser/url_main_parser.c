#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "../url.h"

// Detect URL scheme and return scheme and remainder
int detect_url_scheme(const char* url, char** scheme, char** remainder) {
  if (!url || !scheme || !remainder)
    return -1;

  *scheme = NULL;
  *remainder = NULL;

  JSRT_Debug("detect_url_scheme: url='%s'", url);

  // Check for scheme and handle special cases
  char* scheme_colon = strchr(url, ':');

  JSRT_Debug("detect_url_scheme: scheme_colon=%p", (void*)scheme_colon);

  // URL schemes must start with a letter (per RFC 3986)
  // Paths starting with / should never be treated as having a scheme
  if (scheme_colon && scheme_colon > url && url[0] != '/' &&
      ((url[0] >= 'a' && url[0] <= 'z') || (url[0] >= 'A' && url[0] <= 'Z'))) {
    size_t scheme_len = scheme_colon - url;
    *scheme = malloc(scheme_len + 1);
    if (!*scheme) {
      return -1;
    }
    strncpy(*scheme, url, scheme_len);
    (*scheme)[scheme_len] = '\0';
    *remainder = scheme_colon + 1;
    JSRT_Debug("detect_url_scheme: scheme='%s', remainder='%s'", *scheme, *remainder);
    return 0;
  }

  JSRT_Debug("detect_url_scheme: no scheme found");
  return -1;  // No scheme found
}

// Create and initialize a new URL structure
JSRT_URL* create_url_structure(void) {
  JSRT_URL* parsed = malloc(sizeof(JSRT_URL));
  if (!parsed)
    return NULL;

  memset(parsed, 0, sizeof(JSRT_URL));

  // Initialize with empty strings to prevent NULL dereference
  // Check each allocation and clean up on failure
  parsed->href = strdup("");
  if (!parsed->href)
    goto cleanup_and_fail;

  parsed->protocol = strdup("");
  if (!parsed->protocol)
    goto cleanup_and_fail;

  parsed->username = strdup("");
  if (!parsed->username)
    goto cleanup_and_fail;

  parsed->password = strdup("");
  if (!parsed->password)
    goto cleanup_and_fail;

  parsed->host = strdup("");
  if (!parsed->host)
    goto cleanup_and_fail;

  parsed->hostname = strdup("");
  if (!parsed->hostname)
    goto cleanup_and_fail;

  parsed->port = strdup("");
  if (!parsed->port)
    goto cleanup_and_fail;

  parsed->pathname = strdup("");
  if (!parsed->pathname)
    goto cleanup_and_fail;

  parsed->search = strdup("");
  if (!parsed->search)
    goto cleanup_and_fail;

  parsed->hash = strdup("");
  if (!parsed->hash)
    goto cleanup_and_fail;

  parsed->origin = strdup("");
  if (!parsed->origin)
    goto cleanup_and_fail;

  parsed->search_params = JS_UNDEFINED;
  parsed->ctx = NULL;
  parsed->has_password_field = 0;
  parsed->double_colon_at_pattern = 0;
  parsed->opaque_path = 0;
  parsed->has_authority_syntax = 0;

  return parsed;

cleanup_and_fail:
  // Free any already allocated strings
  JSRT_FreeURL(parsed);
  return NULL;
}

// Main URL parsing function (refactored)
JSRT_URL* parse_absolute_url(const char* preprocessed_url) {
  JSRT_Debug("parse_absolute_url: preprocessed_url='%s'", preprocessed_url);

  JSRT_URL* parsed = create_url_structure();
  if (!parsed)
    return NULL;

  char* url_copy = strdup(preprocessed_url);
  char* ptr = url_copy;

  // Detect scheme
  char* scheme = NULL;
  char* remainder = NULL;
  if (detect_url_scheme(ptr, &scheme, &remainder) != 0) {
    JSRT_Debug("parse_absolute_url: detect_url_scheme failed");
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
    JSRT_Debug("parse_absolute_url: invalid scheme '%s'", scheme);
    free(scheme);
    free(url_copy);
    JSRT_FreeURL(parsed);
    return NULL;
  }

  JSRT_Debug("parse_absolute_url: valid scheme '%s', remainder='%s'", scheme, remainder);

  // Check for incomplete URLs (URLs that appear to be cut off)
  size_t url_len = strlen(preprocessed_url);
  if (url_len > 0) {
    // Check for URLs ending abruptly without proper termination
    // Examples: "https://x/", "https://x/?", "https://x/?#"
    // These should fail according to WPT if they're missing expected content
    const char* last_char = &preprocessed_url[url_len - 1];

    // Check for specific incomplete URL patterns that WPT expects to fail
    // Pattern 1: URLs ending abruptly without a closing character that appear truncated
    if (strncmp(preprocessed_url + strlen(scheme) + 1, "//", 2) == 0) {
      const char* authority_start = preprocessed_url + strlen(scheme) + 3;  // Skip "scheme://"
      const char* authority_end = strpbrk(authority_start, "/?#");
      if (!authority_end) {
        authority_end = preprocessed_url + strlen(preprocessed_url);
      }

      size_t hostname_len = authority_end - authority_start;

      // Note: Removed overly restrictive "truncated URL" check
      // URLs like "http://a" and "a://b" are perfectly valid
      // WPT expects these to parse successfully
    }

    // Special validation for URLs that look incomplete based on their ending pattern
    if (is_special_scheme(scheme)) {
      // For special schemes, check for patterns that indicate incomplete URLs
      // URLs ending with "?" or "#" followed by nothing may be incomplete
      if (*last_char == '?' || *last_char == '#') {
        // These patterns are actually valid per WHATWG - don't reject
        // "https://x/?" is valid (empty query)
        // "https://x/?#" is valid (empty query, empty fragment)
      }

      // However, URLs ending with "/" and having very short hostnames might be incomplete
      if (*last_char == '/' && url_len >= 10) {
        // Check if this looks like a truncated URL pattern
        // Look for authority section
        if (strncmp(preprocessed_url + strlen(scheme) + 1, "//", 2) == 0) {
          const char* authority_start = preprocessed_url + strlen(scheme) + 3;  // Skip "scheme://"
          const char* authority_end = strchr(authority_start, '/');
          if (authority_end) {
            size_t hostname_len = authority_end - authority_start;
            // If hostname is exactly 1 character and ends with /, it might be incomplete
            if (hostname_len == 1 && authority_start[0] >= 'a' && authority_start[0] <= 'z') {
              // This will be caught by hostname validation, but double-check here
            }
          }
        }
      }
    }

    // URLs ending with just ASCII control characters (not UTF-8 continuation bytes) should fail
    // UTF-8 continuation bytes (0x80-0xBF) are valid and should not be rejected
    unsigned char last_byte = (unsigned char)*last_char;
    if (last_byte < 0x20 && last_byte != '\0') {
      // Only reject actual ASCII control characters, not UTF-8 continuation bytes
      JSRT_Debug("parse_absolute_url: URL ends with ASCII control character 0x%02x", last_byte);
      free(scheme);
      free(url_copy);
      JSRT_FreeURL(parsed);
      return NULL;
    }
    // Note: UTF-8 continuation bytes (0x80-0xBF) and Unicode characters are allowed
  }

  // Special validation for schemes without proper authority or path
  if (is_special_scheme(scheme)) {
    // Special schemes like http, https, ftp MUST have authority (start with //)
    // Exception: file: URLs can have various formats, including scheme-only
    if (!remainder || strlen(remainder) == 0) {
      // Case like "http:" - most special schemes require authority
      // Exception: file: URLs can be scheme-only and resolve to file:///
      if (strcmp(scheme, "file") != 0) {
        free(scheme);
        free(url_copy);
        JSRT_FreeURL(parsed);
        return NULL;
      }
      // file: scheme-only is valid - will be normalized to file:/// below
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
  } else {
    // For non-special schemes, scheme-only URLs are valid
    // Per WPT tests, URLs like "sc:" should parse as valid with empty path
    // No validation needed here - empty remainder is valid for non-special schemes
  }

  // Set protocol (scheme + ":") - normalize scheme to lowercase
  free(parsed->protocol);
  parsed->protocol = malloc(strlen(scheme) + 2);
  if (!parsed->protocol) {
    free(scheme);
    free(url_copy);
    JSRT_FreeURL(parsed);
    return NULL;
  }
  snprintf(parsed->protocol, strlen(scheme) + 2, "%s:", scheme);
  // Normalize scheme to lowercase per WHATWG URL spec
  for (size_t i = 0; parsed->protocol[i] != ':' && parsed->protocol[i] != '\0'; i++) {
    parsed->protocol[i] = tolower(parsed->protocol[i]);
  }

  // Parse URL components and get pointer to path/query/fragment section
  char* path_start = parse_url_components(parsed, scheme, remainder);
  if (!path_start) {
    JSRT_Debug("parse_absolute_url: parse_url_components failed for scheme='%s', remainder='%s'", scheme, remainder);
    free(scheme);
    free(url_copy);
    JSRT_FreeURL(parsed);
    return NULL;
  }

  // Parse path, query, fragment from the remaining part BEFORE validation
  JSRT_Debug("parse_absolute_url: about to parse_path_query_fragment, path_start='%s'", path_start);
  parse_path_query_fragment(parsed, path_start);

  // Per WHATWG URL spec, single-character hostnames are valid
  // Remove the overly restrictive validation that was rejecting valid URLs

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

  // Normalize dot segments in the pathname - different rules for special vs non-special schemes
  // According to WHATWG URL spec, special schemes normalize dot segments, but non-special schemes have special rules
  // for preserving double-slash patterns after dot segment normalization
  if (parsed->pathname) {
    int is_special = is_special_scheme(parsed->protocol);

    if (is_special) {
      // Special schemes: standard dot segment normalization
      char* normalized_pathname = normalize_dot_segments_with_percent_decoding(parsed->pathname);
      if (normalized_pathname) {
        free(parsed->pathname);
        parsed->pathname = normalized_pathname;
      }
    } else {
      // Non-special schemes: apply dot segment normalization but preserve double-slash patterns
      // Per WPT tests: "/..//" -> "/.//", "/a/..//" -> "/.//" etc.
      char* normalized_pathname = normalize_dot_segments_preserve_double_slash(parsed->pathname);
      if (normalized_pathname) {
        free(parsed->pathname);
        parsed->pathname = normalized_pathname;
      }
    }
  }

  // Normalize Windows drive letters in file URL pathnames
  if (parsed->protocol && strcmp(parsed->protocol, "file:") == 0 && parsed->pathname) {
    char* normalized_drive = normalize_windows_drive_letters(parsed->pathname);
    if (normalized_drive == NULL) {
      // Invalid drive letter pattern (e.g., double pipes)
      JSRT_FreeURL(parsed);  // This frees parsed->protocol (which is independent of scheme)
      free(url_copy);
      free(scheme);  // scheme still needs to be freed separately
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