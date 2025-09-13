#include "url_parser.h"
#include "debug.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declare internal URL structure from url/url_core.c
typedef struct {
  char* href;
  char* protocol;
  char* username;
  char* password;
  char* host;
  char* hostname;
  char* port;
  char* pathname;
  char* search;
  char* hash;
  char* origin;
} internal_url_t;

// Helper functions from url/url_core.c (simplified versions for internal use)
static char* strip_url_whitespace(const char* url) {
  if (!url)
    return NULL;

  // Find start (skip leading whitespace)
  const char* start = url;
  while (*start &&
         (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' || *start == '\f' || *start == 0x0B)) {
    start++;
  }

  // Find end (skip trailing whitespace)
  const char* end = url + strlen(url) - 1;
  while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == 0x0B)) {
    end--;
  }

  // Create trimmed string
  size_t len = end - start + 1;
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  strncpy(result, start, len);
  result[len] = '\0';
  return result;
}

static char* remove_all_ascii_whitespace(const char* url) {
  if (!url)
    return NULL;

  size_t len = strlen(url);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  size_t j = 0;
  for (size_t i = 0; i < len; i++) {
    char c = url[i];
    // Skip ASCII whitespace characters
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != 0x0B) {
      result[j++] = c;
    }
  }
  result[j] = '\0';
  return result;
}

static char* normalize_url_backslashes(const char* url) {
  if (!url)
    return NULL;

  size_t len = strlen(url);
  char* result = malloc(len + 1);
  if (!result)
    return NULL;

  // Convert backslashes to forward slashes (Windows compatibility)
  for (size_t i = 0; i <= len; i++) {
    result[i] = (url[i] == '\\') ? '/' : url[i];
  }

  return result;
}

static int validate_url_characters(const char* url) {
  if (!url)
    return 0;

  // Basic validation - reject obviously invalid characters
  for (const char* p = url; *p; p++) {
    unsigned char c = (unsigned char)*p;
    // Reject control characters except tab
    if (c < 0x20 && c != 0x09) {
      return 0;
    }
  }
  return 1;
}

// Simple URL parser adapted from url/url_core.c logic
static internal_url_t* parse_url_internal(const char* url) {
  char* trimmed_url = strip_url_whitespace(url);
  if (!trimmed_url)
    return NULL;

  char* cleaned_url = remove_all_ascii_whitespace(trimmed_url);
  free(trimmed_url);
  if (!cleaned_url)
    return NULL;

  char* normalized_url = normalize_url_backslashes(cleaned_url);
  free(cleaned_url);
  if (!normalized_url)
    return NULL;

  if (!validate_url_characters(normalized_url)) {
    free(normalized_url);
    return NULL;
  }

  internal_url_t* parsed = calloc(1, sizeof(internal_url_t));
  if (!parsed) {
    free(normalized_url);
    return NULL;
  }

  // Find scheme
  char* colon = strchr(normalized_url, ':');
  if (!colon || colon == normalized_url) {
    free(normalized_url);
    free(parsed);
    return NULL;
  }

  // Extract protocol
  size_t proto_len = colon - normalized_url;
  parsed->protocol = malloc(proto_len + 2);  // +1 for ':' +1 for null
  if (!parsed->protocol) {
    free(normalized_url);
    free(parsed);
    return NULL;
  }
  strncpy(parsed->protocol, normalized_url, proto_len);
  parsed->protocol[proto_len] = ':';
  parsed->protocol[proto_len + 1] = '\0';

  // Check for "//" after scheme
  if (strncmp(colon + 1, "//", 2) != 0) {
    free(normalized_url);
    free(parsed->protocol);
    free(parsed);
    return NULL;
  }

  // Parse authority section
  const char* authority_start = colon + 3;  // Skip "://"
  const char* path_start = strchr(authority_start, '/');
  const char* query_start = strchr(authority_start, '?');
  const char* fragment_start = strchr(authority_start, '#');

  // Find end of authority
  const char* authority_end = authority_start + strlen(authority_start);
  if (path_start && path_start < authority_end)
    authority_end = path_start;
  if (query_start && query_start < authority_end)
    authority_end = query_start;
  if (fragment_start && fragment_start < authority_end)
    authority_end = fragment_start;

  // Extract host and port
  const char* host_start = authority_start;
  const char* port_start = strchr(host_start, ':');

  if (port_start && port_start < authority_end) {
    // Extract hostname
    size_t hostname_len = port_start - host_start;
    parsed->hostname = malloc(hostname_len + 1);
    if (parsed->hostname) {
      strncpy(parsed->hostname, host_start, hostname_len);
      parsed->hostname[hostname_len] = '\0';
    }

    // Extract port
    size_t port_len = authority_end - port_start - 1;
    parsed->port = malloc(port_len + 1);
    if (parsed->port) {
      strncpy(parsed->port, port_start + 1, port_len);
      parsed->port[port_len] = '\0';
    }

    // Host = hostname:port
    parsed->host = malloc(hostname_len + port_len + 2);
    if (parsed->host) {
      snprintf(parsed->host, hostname_len + port_len + 2, "%s:%s", parsed->hostname, parsed->port);
    }
  } else {
    // No explicit port
    size_t hostname_len = authority_end - host_start;
    parsed->hostname = malloc(hostname_len + 1);
    if (parsed->hostname) {
      strncpy(parsed->hostname, host_start, hostname_len);
      parsed->hostname[hostname_len] = '\0';
    }
    parsed->host = strdup(parsed->hostname ? parsed->hostname : "");
    parsed->port = strdup("");  // Empty port
  }

  // Extract pathname
  if (path_start) {
    const char* path_end =
        query_start ? query_start : (fragment_start ? fragment_start : normalized_url + strlen(normalized_url));
    size_t path_len = path_end - path_start;
    parsed->pathname = malloc(path_len + 1);
    if (parsed->pathname) {
      strncpy(parsed->pathname, path_start, path_len);
      parsed->pathname[path_len] = '\0';
    }
  } else {
    parsed->pathname = strdup("/");
  }

  // Extract search
  if (query_start) {
    const char* search_end = fragment_start ? fragment_start : normalized_url + strlen(normalized_url);
    size_t search_len = search_end - query_start;
    parsed->search = malloc(search_len + 1);
    if (parsed->search) {
      strncpy(parsed->search, query_start, search_len);
      parsed->search[search_len] = '\0';
    }
  } else {
    parsed->search = strdup("");
  }

  // Extract hash
  if (fragment_start) {
    parsed->hash = strdup(fragment_start);
  } else {
    parsed->hash = strdup("");
  }

  // Build href
  parsed->href = strdup(normalized_url);

  free(normalized_url);
  return parsed;
}

static void free_internal_url(internal_url_t* url) {
  if (!url)
    return;

  free(url->href);
  free(url->protocol);
  free(url->username);
  free(url->password);
  free(url->host);
  free(url->hostname);
  free(url->port);
  free(url->pathname);
  free(url->search);
  free(url->hash);
  free(url->origin);
  free(url);
}

int jsrt_url_parse(const char* url, jsrt_url_t* parsed) {
  if (!url || !parsed) {
    return -1;
  }

  // Initialize structure
  memset(parsed, 0, sizeof(jsrt_url_t));

  // Parse using WPT-tested logic
  internal_url_t* internal = parse_url_internal(url);
  if (!internal) {
    return -1;
  }

  // Extract scheme (remove trailing ':')
  if (internal->protocol) {
    size_t proto_len = strlen(internal->protocol);
    if (proto_len > 0 && internal->protocol[proto_len - 1] == ':') {
      parsed->scheme = malloc(proto_len);
      if (parsed->scheme) {
        strncpy(parsed->scheme, internal->protocol, proto_len - 1);
        parsed->scheme[proto_len - 1] = '\0';
        // Convert to lowercase
        for (size_t i = 0; i < proto_len - 1; i++) {
          parsed->scheme[i] = tolower((unsigned char)parsed->scheme[i]);
        }
      }
    }
  }

  // Copy hostname
  parsed->host = strdup(internal->hostname ? internal->hostname : "");

  // Parse port number
  if (internal->port && strlen(internal->port) > 0) {
    parsed->port = atoi(internal->port);
  } else {
    // Use default port
    parsed->port = jsrt_url_default_port(parsed->scheme);
  }

  // Build path (pathname + search + hash)
  size_t path_len = (internal->pathname ? strlen(internal->pathname) : 0) +
                    (internal->search ? strlen(internal->search) : 0) + (internal->hash ? strlen(internal->hash) : 0) +
                    1;
  parsed->path = malloc(path_len);
  if (parsed->path) {
    snprintf(parsed->path, path_len, "%s%s%s", internal->pathname ? internal->pathname : "/",
             internal->search ? internal->search : "", internal->hash ? internal->hash : "");
  }

  // Set security flag
  parsed->is_secure = (parsed->scheme && strcmp(parsed->scheme, "https") == 0);

  // Check for allocation failures
  if (!parsed->scheme || !parsed->host || !parsed->path) {
    jsrt_url_free(parsed);
    free_internal_url(internal);
    return -1;
  }

  free_internal_url(internal);

  JSRT_Debug("URL_Parser: Parsed URL - scheme: %s, host: %s, port: %d, path: %s", parsed->scheme, parsed->host,
             parsed->port, parsed->path);

  return 0;
}

void jsrt_url_free(jsrt_url_t* url) {
  if (!url) {
    return;
  }

  free(url->scheme);
  free(url->host);
  free(url->path);

  memset(url, 0, sizeof(jsrt_url_t));
}

int jsrt_url_default_port(const char* scheme) {
  if (!scheme) {
    return -1;
  }

  // Case-insensitive comparison
  if (strcasecmp(scheme, "http") == 0) {
    return 80;
  } else if (strcasecmp(scheme, "https") == 0) {
    return 443;
  } else if (strcasecmp(scheme, "ws") == 0) {
    return 80;
  } else if (strcasecmp(scheme, "wss") == 0) {
    return 443;
  } else if (strcasecmp(scheme, "ftp") == 0) {
    return 21;
  }

  return -1;  // Unknown scheme
}

bool jsrt_url_is_secure(const char* url) {
  if (!url) {
    return false;
  }

  return (strncasecmp(url, "https://", 8) == 0 || strncasecmp(url, "wss://", 6) == 0);
}