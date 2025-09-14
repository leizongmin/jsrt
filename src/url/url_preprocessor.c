#include <ctype.h>
#include <string.h>
#include "url.h"

// Apply file URL specific normalizations
char* preprocess_file_urls(const char* cleaned_url) {
  if (!cleaned_url)
    return NULL;

  char* preprocessed_url = (char*)cleaned_url;  // Default: no preprocessing needed
  char* url_to_free = NULL;

  // Handle "file:.//path" -> "file:path" normalization
  if (strncmp(cleaned_url, "file:.//", 8) == 0) {
    size_t new_len = strlen(cleaned_url) - 3;  // Remove the ".//"
    char* normalized = malloc(new_len + 1);
    strcpy(normalized, "file:");
    strcat(normalized, cleaned_url + 8);  // Skip "file:.//"
    preprocessed_url = normalized;
    url_to_free = normalized;
  }
  // Handle "file:./path" -> "file:path" normalization
  else if (strncmp(cleaned_url, "file:./", 7) == 0) {
    size_t new_len = strlen(cleaned_url) - 2;  // Remove the "./"
    char* normalized = malloc(new_len + 1);
    strcpy(normalized, "file:");
    strcat(normalized, cleaned_url + 7);  // Skip "file:./"
    preprocessed_url = normalized;
    url_to_free = normalized;
  }
  // Handle "file:/.//path" -> "file:///path" normalization
  // Note: After backslash normalization, "file:/.//p" becomes "file:/./p"
  else if (strncmp(cleaned_url, "file:/./", 8) == 0) {
    size_t new_len = strlen(cleaned_url) + 2;  // Add two extra "/" to make "///"
    char* normalized = malloc(new_len + 1);
    strcpy(normalized, "file:///");
    strcat(normalized, cleaned_url + 8);  // Skip "file:/./"
    preprocessed_url = normalized;
    url_to_free = normalized;
  }
  // Handle Windows drive letter: "file:C:/path" -> "file:///C:/path"
  else if (strncmp(cleaned_url, "file:", 5) == 0 && strlen(cleaned_url) > 7 && isalpha(cleaned_url[5]) &&
           cleaned_url[6] == ':' && cleaned_url[7] == '/') {
    size_t new_len = strlen(cleaned_url) + 3;  // Add "///"
    char* normalized = malloc(new_len + 1);
    strcpy(normalized, "file:///");
    strcat(normalized, cleaned_url + 5);  // Skip "file:"
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
    strcpy(full_url, "file:");
    strcat(full_url, cleaned_url);

    JSRT_URL* result = JSRT_ParseURL(full_url, NULL);
    free(full_url);
    return result;
  }
}

// Handle empty URL string resolution
JSRT_URL* handle_empty_url(const char* base) {
  if (base) {
    return JSRT_ParseURL(base, NULL);  // Parse base URL as absolute
  } else {
    return NULL;  // Empty URL with no base is invalid
  }
}

// Main URL preprocessing coordinator
char* preprocess_url_string(const char* url, const char* base) {
  if (!url)
    return NULL;

  // Strip leading and trailing ASCII whitespace, then remove all internal ASCII whitespace
  char* trimmed_url = strip_url_whitespace(url);
  if (!trimmed_url) {
    return NULL;
  }

  // Check for null bytes in the URL - this is invalid per WHATWG URL spec
  if (strstr(trimmed_url, "%00")) {
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
    return NULL;
  }

  // Normalize backslashes for special schemes
  char* normalized_url = normalize_url_backslashes(cleaned_url);
  free(cleaned_url);
  if (!normalized_url) {
    return NULL;
  }

  return normalized_url;
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

          // Per WHATWG URL spec: URLs with scheme are always absolute
          // Special schemes like "http:foo.com" should be parsed as absolute URLs
          free(scheme);
          return 0;  // Any URL with a scheme is absolute
        } else if (*p == '/' || *p == '?' || *p == '#') {
          // No scheme found before path/query/fragment
          break;
        }
      }
    }

    // Any other URL with base is relative if it doesn't have scheme+authority
    return 1;
  }

  // Without base URL, only absolute URLs with scheme+authority are not relative
  return 0;
}