#include <ctype.h>
#include <errno.h>
#include "url.h"

// Parse authority section: [userinfo@]host[:port]
// Returns 0 on success, -1 on error
int parse_authority(JSRT_URL* parsed, const char* authority_str) {
  if (!authority_str || !parsed) {
    return -1;
  }

  char* authority = strdup(authority_str);
  char* at_pos = strrchr(authority, '@');  // Use rightmost @ to handle @ in userinfo
  char* host_part = authority;

  if (at_pos) {
    *at_pos = '\0';
    // Check if userinfo contains unencoded forward slashes (invalid per WHATWG)
    // If so, drop the userinfo entirely and use only the hostname part
    if (strchr(authority, '/') != NULL) {
      // Invalid userinfo containing forward slashes - drop it completely
      // Use only the hostname part after the '@'
      host_part = at_pos + 1;

      // Clear any existing userinfo since it's invalid
      free(parsed->username);
      parsed->username = strdup("");
      free(parsed->password);
      parsed->password = strdup("");
      parsed->has_password_field = 0;
    } else {
      // Valid userinfo - parse normally
      char* colon_pos = strchr(authority, ':');
      if (colon_pos) {
        *colon_pos = '\0';
        free(parsed->username);
        parsed->username = strdup(authority);
        free(parsed->password);
        parsed->password = strdup(colon_pos + 1);
        parsed->has_password_field = 1;  // Original URL had password field
      } else {
        free(parsed->username);
        parsed->username = strdup(authority);
        parsed->has_password_field = 0;  // No password field in original URL
      }
      host_part = at_pos + 1;
    }
  }

  // Parse host and port
  char* port_colon = NULL;
  int is_ipv6_with_port = 0;

  // Check if this is IPv6 with port: [ipv6]:port format
  if (host_part[0] == '[') {
    char* close_bracket = strchr(host_part, ']');
    if (close_bracket && *(close_bracket + 1) == ':') {
      // IPv6 with port: [ipv6]:port
      port_colon = close_bracket + 1;
      is_ipv6_with_port = 1;
    }
  } else {
    // Regular hostname - find rightmost colon for port
    port_colon = strrchr(host_part, ':');
  }

  // For file URLs, single letter + colon should be treated as drive letter, not port
  int is_file_drive = 0;
  if (strcmp(parsed->protocol, "file:") == 0 && port_colon && !is_ipv6_with_port && (port_colon - host_part) == 1 &&
      isalpha(host_part[0])) {
    is_file_drive = 1;
  }

  // For file URLs, only allow drive letters, not real ports
  if (strcmp(parsed->protocol, "file:") == 0 && port_colon && !is_file_drive) {
    // file URL with port that's not a drive letter is invalid
    goto cleanup_and_return_error;
  }

  if (port_colon && !is_file_drive) {  // Has port and not file drive
    *port_colon = '\0';
    free(parsed->hostname);

    // Handle IPv6 addresses - extract the part inside brackets
    if (is_ipv6_with_port) {
      // For IPv6, host_part is like "[::1]" - we need to extract just the IPv6 part
      if (host_part[0] == '[') {
        char* close_bracket = strchr(host_part, ']');
        if (close_bracket) {
          size_t ipv6_len = close_bracket - host_part - 1;  // Length without brackets
          char* ipv6_part = malloc(ipv6_len + 1);
          strncpy(ipv6_part, host_part + 1, ipv6_len);  // Skip opening bracket
          ipv6_part[ipv6_len] = '\0';

          // Canonicalize the IPv6 address
          char* canonical_ipv6 = canonicalize_ipv6(ipv6_part);
          if (canonical_ipv6) {
            // Format as [canonical_ipv6] for hostname
            parsed->hostname = malloc(strlen(canonical_ipv6) + 3);
            snprintf(parsed->hostname, strlen(canonical_ipv6) + 3, "[%s]", canonical_ipv6);
            free(canonical_ipv6);
          } else {
            // Invalid IPv6 address
            parsed->hostname = strdup(host_part);  // Keep original for error handling
          }
          free(ipv6_part);
        } else {
          parsed->hostname = strdup(host_part);
        }
      } else {
        parsed->hostname = strdup(host_part);
      }
    } else {
      // For file URLs, convert pipe to colon in hostname
      if (strcmp(parsed->protocol, "file:") == 0 && strchr(host_part, '|')) {
        size_t len = strlen(host_part);
        char* converted_host = malloc(len + 1);
        for (size_t i = 0; i < len; i++) {
          converted_host[i] = (host_part[i] == '|') ? ':' : host_part[i];
        }
        converted_host[len] = '\0';
        parsed->hostname = converted_host;
      } else {
        parsed->hostname = strdup(host_part);
      }
    }

    // Validate hostname characters (including Unicode validation)
    if (!validate_hostname_characters(parsed->hostname)) {
      goto cleanup_and_return_error;
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
      goto cleanup_and_return_error;
    } else {
      // Not an IPv4 address - check for special cases before lowercasing
      int should_lowercase = 1;

      // For file URLs, check if hostname is actually a Windows drive letter
      if (parsed->protocol && strcmp(parsed->protocol, "file:") == 0 && parsed->hostname &&
          strlen(parsed->hostname) >= 2) {
        // Check for patterns like "C:" or "C|" that should be treated as drive letters
        if (isalpha(parsed->hostname[0]) && (parsed->hostname[1] == ':' || parsed->hostname[1] == '|') &&
            (strlen(parsed->hostname) == 2 || parsed->hostname[2] == '/' || parsed->hostname[2] == '\0')) {
          should_lowercase = 0;  // Preserve case for Windows drive letters
        }
      }

      if (should_lowercase) {
        // Normalize hostname case to lowercase (required for DNS hostnames per WHATWG URL spec)
        for (size_t i = 0; parsed->hostname[i]; i++) {
          parsed->hostname[i] = tolower(parsed->hostname[i]);
        }
      }
    }

    // Special handling for file URLs with localhost
    if (strcmp(parsed->protocol, "file:") == 0 && strcmp(parsed->hostname, "localhost") == 0) {
      // For file URLs, localhost should be normalized to empty hostname
      free(parsed->hostname);
      parsed->hostname = strdup("");
    }

    // Parse port, handling leading zeros
    char* port_str = port_colon + 1;
    if (strlen(port_str) > 0) {
      // Validate port string - must contain only digits, no spaces or invalid characters
      for (char* p = port_str; *p; p++) {
        if (!isdigit(*p)) {
          // Invalid port format - should throw error
          goto cleanup_and_return_error;
        }
      }

      // Check for port number range (0-65535)
      char* endptr;
      errno = 0;
      long port_long = strtol(port_str, &endptr, 10);

      // Check for conversion errors or out of range
      if (errno == ERANGE || port_long < 0 || port_long > 65535 || *endptr != '\0') {
        goto cleanup_and_return_error;
      }

      // Remove leading zeros but keep at least one digit
      while (*port_str == '0' && *(port_str + 1) != '\0') {
        port_str++;
      }

      // Convert to number and back to remove leading zeros completely
      int port_num = (int)port_long;
      char normalized_port[8];
      snprintf(normalized_port, sizeof(normalized_port), "%d", port_num);

      // Extract scheme from protocol for default port check
      char* scheme = strdup(parsed->protocol);
      char* colon = strchr(scheme, ':');
      if (colon)
        *colon = '\0';

      // Check if port is default port (port 0 should be kept as per WHATWG spec)
      if (is_default_port(scheme, normalized_port)) {
        free(parsed->port);
        parsed->port = strdup("");
      } else {
        free(parsed->port);
        parsed->port = strdup(normalized_port);
      }

      free(scheme);
    }
  } else {
    free(parsed->hostname);

    // Handle IPv6 addresses without port
    if (host_part[0] == '[' && strchr(host_part, ']')) {
      char* close_bracket = strchr(host_part, ']');
      if (close_bracket && *(close_bracket + 1) == '\0') {
        // IPv6 without port: [ipv6]
        size_t ipv6_len = close_bracket - host_part - 1;  // Length without brackets
        char* ipv6_part = malloc(ipv6_len + 1);
        strncpy(ipv6_part, host_part + 1, ipv6_len);  // Skip opening bracket
        ipv6_part[ipv6_len] = '\0';

        // Canonicalize the IPv6 address
        char* canonical_ipv6 = canonicalize_ipv6(ipv6_part);
        if (canonical_ipv6) {
          // Format as [canonical_ipv6] for hostname
          parsed->hostname = malloc(strlen(canonical_ipv6) + 3);
          snprintf(parsed->hostname, strlen(canonical_ipv6) + 3, "[%s]", canonical_ipv6);
          free(canonical_ipv6);
        } else {
          // Invalid IPv6 address
          parsed->hostname = strdup(host_part);  // Keep original for error handling
        }
        free(ipv6_part);
      } else {
        parsed->hostname = strdup(host_part);
      }
    } else if (strcmp(parsed->protocol, "file:") == 0 && strchr(host_part, '|')) {
      size_t len = strlen(host_part);
      char* converted_host = malloc(len + 1);
      for (size_t i = 0; i < len; i++) {
        converted_host[i] = (host_part[i] == '|') ? ':' : host_part[i];
      }
      converted_host[len] = '\0';
      parsed->hostname = converted_host;
    } else {
      parsed->hostname = strdup(host_part);
    }

    // Apply Unicode normalization to hostname (fullwidth -> halfwidth, case normalization)
    char* normalized_hostname = normalize_hostname_unicode(parsed->hostname);
    if (normalized_hostname) {
      free(parsed->hostname);
      parsed->hostname = normalized_hostname;
    }

    // Validate hostname characters (including Unicode validation)
    if (!validate_hostname_characters(parsed->hostname)) {
      goto cleanup_and_return_error;
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
      goto cleanup_and_return_error;
    } else {
      // Not an IPv4 address - check for special cases before lowercasing
      int should_lowercase = 1;

      // For file URLs, check if hostname is actually a Windows drive letter
      if (parsed->protocol && strcmp(parsed->protocol, "file:") == 0 && parsed->hostname &&
          strlen(parsed->hostname) >= 2) {
        // Check for patterns like "C:" or "C|" that should be treated as drive letters
        if (isalpha(parsed->hostname[0]) && (parsed->hostname[1] == ':' || parsed->hostname[1] == '|') &&
            (strlen(parsed->hostname) == 2 || parsed->hostname[2] == '/' || parsed->hostname[2] == '\0')) {
          should_lowercase = 0;  // Preserve case for Windows drive letters
        }
      }

      if (should_lowercase) {
        // Normalize hostname case to lowercase (required for DNS hostnames per WHATWG URL spec)
        for (size_t i = 0; parsed->hostname[i]; i++) {
          parsed->hostname[i] = tolower(parsed->hostname[i]);
        }
      }
    }

    // Special handling for file URLs with localhost
    if (strcmp(parsed->protocol, "file:") == 0 && strcmp(parsed->hostname, "localhost") == 0) {
      // For file URLs, localhost should be normalized to empty hostname
      free(parsed->hostname);
      parsed->hostname = strdup("");
    }
  }

  // Validate that special schemes (except file:) have non-empty hostnames
  // Per WHATWG URL spec, special schemes like http, https, ws, wss, ftp require hosts
  if (is_special_scheme(parsed->protocol) && strcmp(parsed->protocol, "file:") != 0 && strlen(parsed->hostname) == 0) {
    goto cleanup_and_return_error;  // Empty host for special scheme (not file:) is invalid
  }

  // Set host field
  free(parsed->host);
  if (strlen(parsed->port) > 0) {
    parsed->host = malloc(strlen(parsed->hostname) + strlen(parsed->port) + 2);
    snprintf(parsed->host, strlen(parsed->hostname) + strlen(parsed->port) + 2, "%s:%s", parsed->hostname,
             parsed->port);
  } else {
    parsed->host = strdup(parsed->hostname);
  }

  free(authority);
  return 0;

cleanup_and_return_error:
  free(authority);
  return -1;
}

// Parse userinfo@host:port format with empty userinfo (like ::@host:port)
// Returns 0 on success, -1 on error
int parse_empty_userinfo_authority(JSRT_URL* parsed, const char* str) {
  if (!str || !parsed) {
    return -1;
  }

  char* input = strdup(str);
  char* at_pos = strchr(input, '@');
  if (!at_pos) {
    free(input);
    return -1;
  }

  *at_pos = '\0';

  // Parse userinfo (everything before @)
  char* colon_in_userinfo = strchr(input, ':');
  if (colon_in_userinfo) {
    *colon_in_userinfo = '\0';
    free(parsed->username);
    parsed->username = strdup(input);
    free(parsed->password);
    parsed->password = strdup(colon_in_userinfo + 1);
    parsed->has_password_field = 1;
  } else {
    free(parsed->username);
    parsed->username = strdup(input);
    parsed->has_password_field = 0;
  }

  // Parse host:port part after @
  char* host_part = at_pos + 1;
  char* port_colon = strchr(host_part, ':');
  if (port_colon) {
    *port_colon = '\0';
    free(parsed->hostname);
    parsed->hostname = strdup(host_part);

    // Parse port
    char* port_str = port_colon + 1;
    char* normalized_port = normalize_port(port_str, parsed->protocol);
    if (normalized_port) {
      free(parsed->port);
      parsed->port = normalized_port;

      // Set host to hostname:port if port is non-empty (non-default)
      free(parsed->host);
      if (strlen(normalized_port) > 0) {
        size_t host_len = strlen(host_part) + strlen(normalized_port) + 2;
        parsed->host = malloc(host_len);
        snprintf(parsed->host, host_len, "%s:%s", host_part, normalized_port);
      } else {
        parsed->host = strdup(host_part);
      }
    } else {
      // Invalid port
      free(input);
      return -1;
    }
  } else {
    free(parsed->hostname);
    parsed->hostname = strdup(host_part);
    free(parsed->host);
    parsed->host = strdup(host_part);
  }

  free(input);
  return 0;
}

// Find the end of authority section in a URL string
// Returns pointer to the end of authority, or end of string if no path/query/fragment
char* find_authority_end(const char* ptr, const char* rightmost_at) {
  char* authority_end;

  if (rightmost_at) {
    // Found '@' - authority extends to end of hostname after the '@'
    char* hostname_start = (char*)(rightmost_at + 1);
    authority_end = hostname_start;

    // Find end of hostname (before port, path, query, or fragment)
    while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#' &&
           *authority_end != ':') {
      authority_end++;
    }

    // Handle port if present
    if (*authority_end == ':') {
      authority_end++;  // Skip the colon
      // Skip port number
      while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#') {
        authority_end++;
      }
    }
  } else {
    // No '@' found - authority ends at first path separator
    authority_end = (char*)ptr;
    while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#') {
      authority_end++;
    }
  }

  return authority_end;
}