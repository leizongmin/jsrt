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
  char* port_colon = strrchr(host_part, ':');
  // For file URLs, single letter + colon should be treated as drive letter, not port
  int is_file_drive = 0;
  if (strcmp(parsed->protocol, "file:") == 0 && port_colon && (port_colon - host_part) == 1 && isalpha(host_part[0])) {
    is_file_drive = 1;
  }

  if (port_colon && strchr(host_part, '[') == NULL && !is_file_drive) {  // Not IPv6 and not file drive
    *port_colon = '\0';
    free(parsed->hostname);
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

    // Validate hostname characters (including Unicode validation)
    if (!validate_hostname_characters(parsed->hostname)) {
      goto cleanup_and_return_error;
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

    // Validate hostname characters (including Unicode validation)
    if (!validate_hostname_characters(parsed->hostname)) {
      goto cleanup_and_return_error;
    }
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