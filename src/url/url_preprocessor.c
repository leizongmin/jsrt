#include <ctype.h>
#include <string.h>
#include "url.h"

// Apply file URL specific normalizations
char* preprocess_file_urls(const char* cleaned_url) {
  if (!cleaned_url)
    return NULL;

  // Check for invalid patterns in file URLs before processing
  if (strncmp(cleaned_url, "file://", 7) == 0) {
    // Look for pattern like file://%XX| which should be rejected per WPT tests
    const char* authority_start = cleaned_url + 7;
    const char* authority_end = strchr(authority_start, '/');
    if (!authority_end) {
      authority_end = cleaned_url + strlen(cleaned_url);
    }

    // Check for percent-encoded character followed by pipe in authority section
    for (const char* p = authority_start; p < authority_end - 2; p++) {
      if (*p == '%' && hex_to_int(p[1]) >= 0 && hex_to_int(p[2]) >= 0 && p[3] == '|') {
        // Pattern %XX| found in file URL authority - this should be rejected
        return NULL;
      }
    }
  }

  char* preprocessed_url = (char*)cleaned_url;  // Default: no preprocessing needed
  char* url_to_free = NULL;

  // Handle "file:.//path" -> "file:path" normalization
  if (strncmp(cleaned_url, "file:.//", 8) == 0) {
    size_t new_len = strlen(cleaned_url) - 3;  // Remove the "./"
    char* normalized = malloc(new_len + 1);
    snprintf(normalized, new_len + 1, "file:%s", cleaned_url + 8);  // Skip "file:./"
    preprocessed_url = normalized;
    url_to_free = normalized;
  }
  // Handle "file:./path" -> "file:path" normalization
  else if (strncmp(cleaned_url, "file:./", 7) == 0) {
    size_t new_len = strlen(cleaned_url) - 2;  // Remove the "./"
    char* normalized = malloc(new_len + 1);
    snprintf(normalized, new_len + 1, "file:%s", cleaned_url + 7);  // Skip "file:./"
    preprocessed_url = normalized;
    url_to_free = normalized;
  }
  // Handle "file:/.//path" -> "file:///path" normalization
  // Note: After backslash normalization, "file:/.//p" becomes "file:/./p"
  else if (strncmp(cleaned_url, "file:/./", 8) == 0) {
    size_t new_len = strlen(cleaned_url) + 2;  // Add two extra "/" to make "///"
    char* normalized = malloc(new_len + 1);
    snprintf(normalized, new_len + 1, "file:///%s", cleaned_url + 8);  // Skip "file:/./"
    preprocessed_url = normalized;
    url_to_free = normalized;
  }
  // Handle Windows drive letter: "file:C:/path" -> "file:///C:/path" (case-insensitive)
  else if (strlen(cleaned_url) > 7 &&
           (strncmp(cleaned_url, "file:", 5) == 0 || strncmp(cleaned_url, "File:", 5) == 0) &&
           isalpha(cleaned_url[5]) && cleaned_url[6] == ':' && cleaned_url[7] == '/') {
    size_t new_len = strlen(cleaned_url) + 3;  // Add "///"
    char* normalized = malloc(new_len + 1);
    snprintf(normalized, new_len + 1, "file:///%s", cleaned_url + 5);  // Skip "file:"
    preprocessed_url = normalized;
    url_to_free = normalized;
  }
  // Handle Windows drive letter with pipe: "file:C|/path" -> "file:///C:/path"
  // Also handles cases like "file:C|////path" with multiple slashes (case-insensitive)
  // But NOT double pipes like "file:C||/path" - only first pipe becomes colon
  else if (strlen(cleaned_url) > 7 &&
           (strncmp(cleaned_url, "file:", 5) == 0 || strncmp(cleaned_url, "File:", 5) == 0) &&
           isalpha(cleaned_url[5]) && cleaned_url[6] == '|' &&
           (cleaned_url[7] == '/' || cleaned_url[7] == '\\' || cleaned_url[7] == '\0')) {
    // Find the start of the path after the drive letter and any slashes/backslashes
    size_t path_start = 7;
    while (path_start < strlen(cleaned_url) && (cleaned_url[path_start] == '/' || cleaned_url[path_start] == '\\')) {
      path_start++;
    }

    size_t input_len = strlen(cleaned_url);
    char* normalized = malloc(input_len + 10);  // Extra space for normalization
    size_t written = snprintf(normalized, input_len + 10, "file:///");

    // Add drive letter and colon (preserve original case)
    size_t pos = 8;
    normalized[pos++] = cleaned_url[5];  // Drive letter (preserve case)
    normalized[pos++] = ':';             // Convert | to :

    // Add a single slash before the path content (not before the remaining slashes)
    if (path_start < input_len) {
      normalized[pos++] = '/';

      // Copy and normalize the rest of the path, converting backslashes to forward slashes
      for (size_t i = path_start; i < input_len; i++) {
        if (cleaned_url[i] == '\\') {
          normalized[pos++] = '/';
        } else {
          normalized[pos++] = cleaned_url[i];
        }
      }
    } else {
      // No path content after drive letter - just add a trailing slash
      normalized[pos++] = '/';
    }
    normalized[pos] = '\0';

    preprocessed_url = normalized;
    url_to_free = normalized;
  }
  // Handle Windows drive letter with double pipe: "file:C||/path" -> "file:///C||/path"
  // For double pipes, NO conversion happens - both pipes remain as pipes
  else if (strlen(cleaned_url) > 8 &&
           (strncmp(cleaned_url, "file:", 5) == 0 || strncmp(cleaned_url, "File:", 5) == 0) &&
           isalpha(cleaned_url[5]) && cleaned_url[6] == '|' && cleaned_url[7] == '|') {
    // This is a double pipe case like file:C||/m/ -> file:///C||/m/
    // NO pipe-to-colon conversion for double pipes
    size_t input_len = strlen(cleaned_url);
    char* normalized = malloc(input_len + 10);  // Extra space for normalization
    size_t written = snprintf(normalized, input_len + 10, "file:///");

    // Copy everything from position 5 onwards (drive letter + pipes + path)
    size_t pos = 8;
    for (size_t i = 5; i < input_len; i++) {
      if (cleaned_url[i] == '\\') {
        normalized[pos++] = '/';
      } else if (i == 5) {
        normalized[pos++] = cleaned_url[i];  // Preserve drive letter case
      } else {
        normalized[pos++] = cleaned_url[i];  // Keep everything else as-is
      }
    }
    normalized[pos] = '\0';

    preprocessed_url = normalized;
    url_to_free = normalized;
  }
  // Handle Windows drive letter paths with backslashes that should NOT be resolved against base
  // "file:c:\foo\bar.html" should become "file:///c:/foo/bar.html", not resolved against base (case-insensitive)
  else if (strlen(cleaned_url) > 7 &&
           (strncmp(cleaned_url, "file:", 5) == 0 || strncmp(cleaned_url, "File:", 5) == 0) &&
           isalpha(cleaned_url[5]) && (cleaned_url[6] == ':' || cleaned_url[6] == '|') && cleaned_url[7] == '\\') {
    // This is a Windows drive letter with backslash - normalize it
    size_t input_len = strlen(cleaned_url);
    char* normalized = malloc(input_len + 10);  // Extra space for normalization
    size_t written = snprintf(normalized, input_len + 10, "file:///");

    // Add drive letter and colon (preserve original case)
    size_t pos = 8;
    normalized[pos++] = cleaned_url[5];  // Drive letter (preserve case)
    normalized[pos++] = ':';             // Convert | or : to :

    // Copy and normalize the rest of the path (starting from position 7, which is the backslash)
    for (size_t i = 7; i < input_len; i++) {
      if (cleaned_url[i] == '\\') {
        normalized[pos++] = '/';
      } else {
        normalized[pos++] = cleaned_url[i];
      }
    }
    normalized[pos] = '\0';

    preprocessed_url = normalized;
    url_to_free = normalized;
  }

  return url_to_free ? url_to_free : strdup(cleaned_url);
}

// Handle protocol-relative URLs (starting with "//")
JSRT_URL* handle_protocol_relative(const char* cleaned_url, const char* base) {
  if (!cleaned_url || strncmp(cleaned_url, "//", 2) != 0) {
    return NULL;
  }

  if (base) {
    // Protocol-relative URL: inherit protocol from base, parse authority from URL
    JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
    if (!base_url) {
      return NULL;
    }

    // Create a full URL by combining base protocol with the protocol-relative URL
    size_t full_url_len = strlen(base_url->protocol) + strlen(cleaned_url) + 2;
    char* full_url = malloc(full_url_len);
    if (!full_url) {
      JSRT_FreeURL(base_url);
      return NULL;
    }
    snprintf(full_url, full_url_len, "%s%s", base_url->protocol, cleaned_url);
    JSRT_FreeURL(base_url);

    // Parse the full URL recursively (without base to avoid infinite recursion)
    JSRT_URL* result = JSRT_ParseURL(full_url, NULL);
    free(full_url);
    return result;
  } else {
    // No base URL - treat as absolute URL with default scheme
    char* full_url = malloc(strlen("file:") + strlen(cleaned_url) + 1);
    if (!full_url) {
      return NULL;
    }
    snprintf(full_url, strlen("file:") + strlen(cleaned_url) + 1, "file:%s", cleaned_url);

    JSRT_URL* result = JSRT_ParseURL(full_url, NULL);
    free(full_url);
    return result;
  }
}

// Handle empty URL string resolution
JSRT_URL* handle_empty_url(const char* base) {
  if (base) {
    // Parse base URL first
    JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
    if (!base_url) {
      return NULL;
    }

    // Create a new URL without query and fragment per WHATWG URL spec
    // An empty URL resolves to the base URL without its query and fragment
    size_t new_url_len = strlen(base_url->protocol) + strlen(base_url->host) + strlen(base_url->pathname) +
                         strlen(base_url->username) + strlen(base_url->password) +
                         50;  // Extra safety margin for separators
    char* new_url = malloc(new_url_len);
    if (!new_url) {
      JSRT_FreeURL(base_url);
      return NULL;
    }

    // Build URL without query and fragment
    if (strlen(base_url->username) > 0 || strlen(base_url->password) > 0) {
      if (strlen(base_url->password) > 0) {
        snprintf(new_url, new_url_len, "%s//%s:%s@%s%s", base_url->protocol, base_url->username, base_url->password,
                 base_url->host, base_url->pathname);
      } else {
        snprintf(new_url, new_url_len, "%s//%s@%s%s", base_url->protocol, base_url->username, base_url->host,
                 base_url->pathname);
      }
    } else {
      snprintf(new_url, new_url_len, "%s//%s%s", base_url->protocol, base_url->host, base_url->pathname);
    }

    JSRT_FreeURL(base_url);

    // Parse the new URL without query/fragment
    JSRT_URL* result = JSRT_ParseURL(new_url, NULL);
    free(new_url);
    return result;
  } else {
    return NULL;  // Empty URL with no base is invalid
  }
}

// Main URL preprocessing coordinator
char* preprocess_url_string(const char* url, const char* base) {
  if (!url)
    return NULL;

#ifdef DEBUG
  fprintf(stderr, "[DEBUG] preprocess_url_string: url='%s'\n", url);
#endif

  // Strip leading and trailing ASCII whitespace, then remove all internal ASCII whitespace
  char* trimmed_url = strip_url_whitespace(url);
  if (!trimmed_url) {
    return NULL;
  }

  // Validate URL characters (rejects fullwidth percent, invalid Unicode, etc.)
  if (!validate_url_characters(trimmed_url)) {
    free(trimmed_url);
    return NULL;
  }

  // Check for forbidden percent-encoded characters in the URL per WHATWG URL spec
  // This includes null bytes and other control characters that should cause URL parsing to fail
  if (!validate_percent_encoded_characters(trimmed_url)) {
    free(trimmed_url);
    return NULL;
  }

  // Determine if this URL has a special scheme first
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

  // Remove internal ASCII whitespace for all schemes
  char* cleaned_url = remove_all_ascii_whitespace(trimmed_url);
  free(trimmed_url);
  if (!cleaned_url) {
    return NULL;  // URL preprocessing failed (likely memory allocation error)
  }

  // For non-special schemes, preserve spaces before query/fragment boundaries
  // They will be encoded later during path encoding
  char* space_normalized_url = cleaned_url;

  // Normalize backslashes for special schemes
  char* normalized_url = normalize_url_backslashes(space_normalized_url);
  free(space_normalized_url);
  if (!normalized_url) {
    return NULL;
  }

  // Apply single-slash normalization for absolute URLs only
  // Relative URLs with matching schemes should not be normalized here
  char* final_url = normalized_url;

  if (!base || !is_relative_url(normalized_url, base)) {
    // This is an absolute URL - apply single-slash normalization
    final_url = normalize_single_slash_schemes(normalized_url);
    free(normalized_url);
    if (!final_url) {
      return NULL;
    }
  }

  return final_url;
}

// Check if URL should be treated as relative
int is_relative_url(const char* cleaned_url, const char* base) {
  if (!cleaned_url)
    return 0;

  // Debug: print what we're checking
  // printf("DEBUG is_relative_url: checking '%s' with base '%s'\n", cleaned_url, base ? base : "NULL");

  // Check for scheme-relative URLs (starting with //)
  if (strncmp(cleaned_url, "//", 2) == 0) {
    // printf("DEBUG: Found scheme-relative URL (starts with //)\n");
    return 1;
  }

  // Check if URL has a scheme and authority (://) - these are absolute
  char* authority_pattern = strstr(cleaned_url, "://");
  if (authority_pattern) {
    // printf("DEBUG: Found authority pattern (://), treating as absolute\n");
    return 0;  // Absolute URL with authority
  }

  // If we have a base URL, check for special relative URL patterns
  if (base) {
    // URLs starting with "/" are always relative
    if (cleaned_url[0] == '/') {
      return 1;
    }

    // URLs starting with "?", "#" are always relative
    if (cleaned_url[0] == '?' || cleaned_url[0] == '#') {
      return 1;
    }

    // URLs starting with backslashes are relative (Windows path patterns)
    // Examples: "\x", "\\x\hello", "\\server\file"
    if (cleaned_url[0] == '\\') {
      return 1;
    }

    // Check if URL has a special scheme without authority (like "http:foo.com")
    // Only special schemes should be treated as relative according to WHATWG URL spec
    if (cleaned_url[0] &&
        ((cleaned_url[0] >= 'a' && cleaned_url[0] <= 'z') || (cleaned_url[0] >= 'A' && cleaned_url[0] <= 'Z'))) {
      for (const char* p = cleaned_url; *p; p++) {
        if (*p == ':') {
          // Found a scheme, extract it to check if it's special
          size_t scheme_len = p - cleaned_url;
          char* scheme = malloc(scheme_len + 2);  // +1 for ':', +1 for '\0'
          strncpy(scheme, cleaned_url, scheme_len);
          scheme[scheme_len] = ':';
          scheme[scheme_len + 1] = '\0';

          // Check if this is a special scheme without authority
          // For special schemes like "http:foo.com", these should be treated as relative paths
          int is_special = is_special_scheme(scheme);

          if (is_special) {
            // Check if it's a single-slash special scheme URL like "ftp:/example.com/"
            // According to WPT tests, these should only be treated as relative URLs when the scheme matches the base
            if (*(p + 1) == '/' && *(p + 2) != '/') {
              // Single slash after scheme for special schemes -> check if scheme matches base
              // Examples: "http:/example.com/" with http: base -> relative path
              //           "ftp:/example.com/" with http: base -> absolute URL
              JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
              if (base_url && strcmp(scheme, base_url->protocol) == 0) {
                // Same scheme as base -> relative URL
                JSRT_FreeURL(base_url);
                free(scheme);
                return 1;
              } else {
                // Different scheme from base -> absolute URL
                if (base_url)
                  JSRT_FreeURL(base_url);
                free(scheme);
                return 0;
              }
            } else {
              // Special schemes without authority (like "http:foo.com") - check scheme compatibility
              // Same scheme as base: relative path
              // Different scheme from base: absolute URL

              // Special case: file URLs with Windows drive letters are always absolute
              // Example: "file:c:\foo\bar.html" should be absolute even with file: base
              if (strcmp(scheme, "file:") == 0 && scheme_len + 3 < strlen(cleaned_url) &&
                  isalpha(cleaned_url[scheme_len + 1]) &&
                  (cleaned_url[scheme_len + 2] == ':' || cleaned_url[scheme_len + 2] == '|')) {
                // This is file: followed by drive letter - always absolute
                free(scheme);
                return 0;
              }

              // Special case: file URLs with relative path patterns like "file:.", "file:..", "file:..."
              // These should be treated as relative URLs according to WPT tests
              if (strcmp(scheme, "file:") == 0 && scheme_len + 1 < strlen(cleaned_url) &&
                  cleaned_url[scheme_len + 1] == '.' &&
                  (cleaned_url[scheme_len + 2] == '\0' || cleaned_url[scheme_len + 2] == '.' ||
                   cleaned_url[scheme_len + 2] == '/')) {
                // This is file: followed by relative path pattern - treat as relative
                free(scheme);
                return 1;
              }

              JSRT_URL* base_url = JSRT_ParseURL(base, NULL);
              if (base_url && strcmp(scheme, base_url->protocol) == 0) {
                // Same scheme as base -> relative URL
                JSRT_FreeURL(base_url);
                free(scheme);
                return 1;
              } else {
                // Different scheme from base -> absolute URL
                if (base_url)
                  JSRT_FreeURL(base_url);
                free(scheme);
                return 0;
              }
            }
          } else {
            // Non-special schemes with just colon are absolute (e.g., "mailto:test" → absolute)
            free(scheme);
            return 0;
          }
        } else if (*p == '/' || *p == '?' || *p == '#') {
          // No scheme found before path/query/fragment
          break;
        }
      }
    }

    // Check for Windows drive letter patterns (C|/path) which should be absolute file URLs
    // BUT ONLY if this is actually intended as a file path, not a scheme
    // According to WHATWG URL spec, "c:/foo" should be treated as scheme "c:" with path "/foo"
    // Only treat as Windows drive if we're in a file URL context
    if (strlen(cleaned_url) >= 3 && isalpha(cleaned_url[0]) && (cleaned_url[1] == '|' || cleaned_url[1] == ':') &&
        cleaned_url[2] == '/' && base && strncmp(base, "file:", 5) == 0) {
      // This is a Windows drive letter in file URL context - should be treated as absolute file URL
      return 0;
    }

    // Any other URL with base is relative if it doesn't have scheme+authority
    return 1;
  }

  // Without base URL, only absolute URLs with scheme+authority are not relative
  return 0;
}

// Normalize single-slash special schemes to double-slash format
// Examples: "ftp:/example.com/" -> "ftp://example.com/"
char* normalize_single_slash_schemes(const char* url) {
  if (!url)
    return NULL;

  size_t url_len = strlen(url);

  // First, look for pattern "scheme:hostname" (no slashes) where scheme is special
  for (size_t i = 0; i + 1 < url_len; i++) {
    if (url[i] == ':' && url[i + 1] != '/' && url[i + 1] != '\0') {
      // Found "scheme:something" pattern where something doesn't start with slash
      size_t scheme_len = i;
      char* scheme = malloc(scheme_len + 2);  // +1 for ':', +1 for '\0'
      strncpy(scheme, url, scheme_len);
      scheme[scheme_len] = ':';
      scheme[scheme_len + 1] = '\0';

      if (is_special_scheme(scheme)) {
        // Special scheme without slashes - add "//" after colon
        // EXCEPT for file URLs which should remain opaque when they have no slashes
        if (strcmp(scheme, "file:") == 0) {
          // File URLs without slashes should remain opaque - don't add "//"
          free(scheme);
          break;
        }

        size_t new_len = url_len + 2;  // +2 for "//"
        char* normalized = malloc(new_len + 1);

        // Copy scheme and colon
        strncpy(normalized, url, i + 1);
        // Add "//"
        normalized[i + 1] = '/';
        normalized[i + 2] = '/';
        // Copy the rest starting after the colon
        snprintf(normalized + i + 3, new_len - i - 2, "%s", url + i + 1);

        free(scheme);
        return normalized;
      }

      free(scheme);
      break;  // Only check first scheme found
    }
  }

  // Look for pattern "scheme:/" where scheme is special
  for (size_t i = 0; i + 2 < url_len; i++) {
    if (url[i] == ':' && url[i + 1] == '/' && url[i + 2] != '/') {
      // Found "scheme:/" pattern, check if scheme is special
      size_t scheme_len = i;
      char* scheme = malloc(scheme_len + 2);  // +1 for ':', +1 for '\0'
      strncpy(scheme, url, scheme_len);
      scheme[scheme_len] = ':';
      scheme[scheme_len + 1] = '\0';

      if (is_special_scheme(scheme)) {
        char* normalized;

        // Special scheme with single slash - normalize based on scheme type
        if (strcmp(scheme, "file:") == 0) {
          // File URLs: "file:/path" -> "file:///path" (triple slash for local files)
          size_t new_len = url_len + 2;  // +2 for two extra slashes
          normalized = malloc(new_len + 1);

          // Copy scheme and colon
          strncpy(normalized, url, i + 1);
          // Add two extra slashes to make it file:///
          normalized[i + 1] = '/';
          normalized[i + 2] = '/';
          // Copy the rest starting from the single slash
          snprintf(normalized + i + 3, new_len - i - 2, "%s", url + i + 1);
        } else {
          // Other special schemes: normalize to double slash
          size_t new_len = url_len + 1;  // +1 for extra slash
          normalized = malloc(new_len + 1);

          // Copy scheme and colon
          strncpy(normalized, url, i + 1);
          // Add extra slash
          normalized[i + 1] = '/';
          // Copy the rest starting from the single slash
          snprintf(normalized + i + 2, new_len - i - 1, "%s", url + i + 1);
        }

        free(scheme);
        return normalized;
      }

      free(scheme);
      break;  // Only check first scheme found
    }
  }

  // No normalization needed, return copy
  return strdup(url);
}