#include <stdlib.h>
#include <string.h>
#include "../url.h"

// Parse URL authority with empty userinfo (no credentials)
// This is a simplified version for authorities without userinfo
int parse_empty_userinfo_authority(JSRT_URL* parsed, const char* authority) {
  if (!authority || !parsed)
    return -1;

  char* input = strdup(authority);
  if (!input)
    return -1;

  // Split on ':' to separate hostname and port
  char* colon_pos = strchr(input, ':');
  char* host_part = input;
  char* port_part = NULL;

  if (colon_pos) {
    *colon_pos = '\0';
    port_part = colon_pos + 1;
  }

  // Process hostname
  if (strlen(host_part) == 0) {
    free(parsed->hostname);
    parsed->hostname = strdup("");
    if (!parsed->hostname) {
      free(input);
      return -1;
    }
    free(parsed->host);
    parsed->host = strdup("");
    if (!parsed->host) {
      free(input);
      return -1;
    }
  } else {
    free(parsed->hostname);
    parsed->hostname = strdup(host_part);
    if (!parsed->hostname) {
      free(input);
      return -1;
    }
    free(parsed->host);
    parsed->host = strdup(host_part);
    if (!parsed->host) {
      free(input);
      return -1;
    }
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