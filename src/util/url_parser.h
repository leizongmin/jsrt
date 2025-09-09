#ifndef __JSRT_UTIL_URL_PARSER_H__
#define __JSRT_UTIL_URL_PARSER_H__

#include <stdbool.h>

// URL components structure
typedef struct {
  char* scheme;    // "http" or "https"
  char* host;      // hostname or IP address
  int port;        // port number (80 for http, 443 for https by default)
  char* path;      // path component (includes query and fragment)
  bool is_secure;  // true for https, false for http
} jsrt_url_t;

// Parse URL string into components
// Returns: 0 on success, -1 on failure
// Note: Allocates memory for components, must be freed with jsrt_url_free()
int jsrt_url_parse(const char* url, jsrt_url_t* parsed);

// Free memory allocated by jsrt_url_parse()
void jsrt_url_free(jsrt_url_t* url);

// Get default port for a scheme
// Returns: default port number, or -1 if scheme is unknown
int jsrt_url_default_port(const char* scheme);

// Check if URL uses secure protocol (https)
bool jsrt_url_is_secure(const char* url);

#endif