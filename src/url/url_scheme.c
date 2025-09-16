#include <ctype.h>
#include "url.h"

// Handle special scheme formats like "http:example.com/" or "http:foo.com"
// Returns 0 on success, -1 on error
int parse_special_scheme_without_slashes(JSRT_URL* parsed, char** ptr) {
  if (!parsed || !ptr || !*ptr) {
    return -1;
  }

  char* input_ptr = *ptr;

  // Check if this starts with userinfo pattern (:...@)
  if (input_ptr[0] == ':' && strchr(input_ptr, '@')) {
    // This is likely a case like "http::@c:29" where we have scheme::userinfo@host:port
    // Set flag to indicate this special pattern for origin calculation
    parsed->double_colon_at_pattern = 1;

    // Skip the leading colon and parse as authority
    input_ptr++;  // Skip the extra ':'

    if (parse_empty_userinfo_authority(parsed, input_ptr) != 0) {
      return -1;
    }

    // Set pointer to end since we consumed all the authority
    *ptr = input_ptr + strlen(input_ptr);
    return 0;
  }

  // Special case for file URLs with dot normalization pattern like "file:.//p"
  if (strcmp(parsed->protocol, "file:") == 0 && input_ptr[0] == '.' && input_ptr[1] == '/' && input_ptr[2] == '/') {
    // This should be treated as an opaque path, not authority
    // Normalize ".//p" to just "p"
    char* normalized_path = strdup(input_ptr + 3);  // Skip "./"
    free(parsed->pathname);
    parsed->pathname = normalized_path;

    // Clear hostname since this is an opaque path
    free(parsed->hostname);
    parsed->hostname = strdup("");
    free(parsed->host);
    parsed->host = strdup("");

    // Mark as opaque path for href building
    parsed->opaque_path = 1;

    // Move pointer to end since we consumed everything
    *ptr = input_ptr + strlen(input_ptr);
    return 0;
  }

  // Special handling for file URLs without slashes - these should be opaque paths
  if (strcmp(parsed->protocol, "file:") == 0) {
    // For file URLs like "file:test" or "file:...", treat as opaque path
    free(parsed->pathname);
    parsed->pathname = strdup(input_ptr);

    // Clear hostname since this is an opaque path
    free(parsed->hostname);
    parsed->hostname = strdup("");
    free(parsed->host);
    parsed->host = strdup("");

    // Mark as opaque path for href building
    parsed->opaque_path = 1;

    // Move pointer to end since we consumed everything
    *ptr = input_ptr + strlen(input_ptr);
    return 0;
  }

  // Regular hostname parsing for cases like "http:example.com/"
  char* slash_pos = strchr(input_ptr, '/');
  char* query_pos = strchr(input_ptr, '?');
  char* fragment_pos = strchr(input_ptr, '#');

  // Find the first delimiter
  char* first_delimiter = input_ptr + strlen(input_ptr);  // Default to end
  if (slash_pos && slash_pos < first_delimiter)
    first_delimiter = slash_pos;
  if (query_pos && query_pos < first_delimiter)
    first_delimiter = query_pos;
  if (fragment_pos && fragment_pos < first_delimiter)
    first_delimiter = fragment_pos;

  if (first_delimiter > input_ptr) {
    // Extract hostname part
    size_t hostname_len = first_delimiter - input_ptr;
    char* hostname = malloc(hostname_len + 1);
    strncpy(hostname, input_ptr, hostname_len);
    hostname[hostname_len] = '\0';

    free(parsed->hostname);
    // For file URLs, convert pipe to colon in hostname
    if (strcmp(parsed->protocol, "file:") == 0 && strchr(hostname, '|')) {
      for (size_t i = 0; i < hostname_len; i++) {
        if (hostname[i] == '|')
          hostname[i] = ':';
      }
    }
    parsed->hostname = hostname;
    free(parsed->host);
    parsed->host = strdup(hostname);

    *ptr = first_delimiter;
  }

  return 0;
}

// Handle special scheme with single slash: "http:/example.com/"
// Returns 0 on success, -1 on error
int parse_special_scheme_single_slash(JSRT_URL* parsed, char** ptr) {
  if (!parsed || !ptr || !*ptr) {
    return -1;
  }

  // Skip the single slash
  (*ptr)++;

  char* input_ptr = *ptr;
  char* authority_end = find_authority_end(input_ptr, strchr(input_ptr, '@'));

  if (authority_end > input_ptr) {
    size_t authority_len = authority_end - input_ptr;
    char* authority = malloc(authority_len + 1);
    strncpy(authority, input_ptr, authority_len);
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

// Handle triple slash case: "///test" -> "http://test/"
// Returns 0 on success, -1 on error
int parse_empty_authority_with_path(JSRT_URL* parsed, char** ptr) {
  if (!parsed || !ptr || !*ptr) {
    return -1;
  }

  char* input_ptr = *ptr;

  // Skip the extra slash
  input_ptr++;  // Skip the third slash

  // Find end of hostname (until next slash, query, or fragment)
  char* hostname_end = input_ptr;
  while (*hostname_end && *hostname_end != '/' && *hostname_end != '?' && *hostname_end != '#') {
    hostname_end++;
  }

  if (hostname_end > input_ptr) {
    // Extract hostname
    size_t hostname_len = hostname_end - input_ptr;
    char* hostname = malloc(hostname_len + 1);
    strncpy(hostname, input_ptr, hostname_len);
    hostname[hostname_len] = '\0';

    free(parsed->hostname);
    parsed->hostname = hostname;
    free(parsed->host);
    parsed->host = strdup(hostname);

    *ptr = hostname_end;  // Move to start of path
  }

  return 0;
}

// Ensure special schemes have "/" as default path
void ensure_special_scheme_default_path(JSRT_URL* parsed, char** ptr) {
  if (!parsed || !ptr || !*ptr) {
    return;
  }

  char* input_ptr = *ptr;

  // Always ensure "/" path for special schemes when no path is specified
  if (*input_ptr == '\0') {
    free(parsed->pathname);
    parsed->pathname = strdup("/");
  } else if (*input_ptr == '/' && *(input_ptr + 1) == '\0') {
    // For a single trailing slash, preserve it in the pathname
    free(parsed->pathname);
    parsed->pathname = strdup("/");
    (*ptr)++;  // Skip the trailing slash to avoid double processing
  }
}

// Handle file URL Windows drive letter conversion
void handle_file_url_drive_letters(JSRT_URL* parsed) {
  if (!parsed || strcmp(parsed->protocol, "file:") != 0) {
    return;
  }

  // Special handling for file URLs with Windows drive letters
  // If hostname looks like a Windows drive (single letter + colon or pipe), move it to pathname
  if (strlen(parsed->hostname) == 2 && isalpha(parsed->hostname[0]) &&
      (parsed->hostname[1] == ':' || parsed->hostname[1] == '|')) {
    // Move drive letter from hostname to pathname
    // Convert pipe to colon if needed
    char drive_letter[3];
    drive_letter[0] = parsed->hostname[0];
    drive_letter[1] = (parsed->hostname[1] == '|') ? ':' : parsed->hostname[1];
    drive_letter[2] = '\0';

    size_t new_pathname_len = strlen(parsed->pathname) + 4;  // "/" + drive_letter + existing path + null terminator
    char* new_pathname = malloc(new_pathname_len);
    snprintf(new_pathname, new_pathname_len, "/%s%s", drive_letter, parsed->pathname);

    free(parsed->pathname);
    parsed->pathname = new_pathname;

    // Clear hostname and host
    free(parsed->hostname);
    parsed->hostname = strdup("");
    free(parsed->host);
    parsed->host = strdup("");
  }
}