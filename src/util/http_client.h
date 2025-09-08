#ifndef __JSRT_UTIL_HTTP_CLIENT_H__
#define __JSRT_UTIL_HTTP_CLIENT_H__

#include <stddef.h>

// HTTP response structure
typedef struct {
  int status;
  char* status_text;
  char* body;
  size_t body_size;
  char* content_type;
  char* etag;
  char* last_modified;
  int error;  // 0 = success, non-zero = error code
} JSRT_HttpResponse;

// HTTP error codes
#define JSRT_HTTP_OK 0
#define JSRT_HTTP_ERROR_INVALID_URL 1
#define JSRT_HTTP_ERROR_OUT_OF_MEMORY 2
#define JSRT_HTTP_ERROR_NETWORK 3
#define JSRT_HTTP_ERROR_TIMEOUT 4
#define JSRT_HTTP_ERROR_HTTP_ERROR 5
#define JSRT_HTTP_ERROR_SSL_ERROR 6
#define JSRT_HTTP_ERROR_REDIRECT_LOOP 7

// Perform a synchronous GET request to the given URL
// Returns a JSRT_HttpResponse structure that must be freed with JSRT_HttpResponseFree
JSRT_HttpResponse JSRT_HttpGet(const char* url);

// Enhanced HTTP GET with custom user agent and timeout
JSRT_HttpResponse JSRT_HttpGetWithOptions(const char* url, const char* user_agent, int timeout_ms);

// Free memory allocated for an HTTP response
void JSRT_HttpResponseFree(JSRT_HttpResponse* response);

#endif