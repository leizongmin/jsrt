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

// HTTP error codes - mapped to http/parser.h error codes for consistency
// Note: These are kept for backward compatibility but now map to parser error codes
typedef enum {
  JSRT_HTTP_CLIENT_OK = 0,
  JSRT_HTTP_CLIENT_ERROR_INVALID_URL = 1,
  JSRT_HTTP_CLIENT_ERROR_OUT_OF_MEMORY = 2,
  JSRT_HTTP_CLIENT_ERROR_NETWORK = 3,
  JSRT_HTTP_CLIENT_ERROR_TIMEOUT = 4,
  JSRT_HTTP_CLIENT_ERROR_HTTP_ERROR = 5,
  JSRT_HTTP_CLIENT_ERROR_SSL_ERROR = 6,
  JSRT_HTTP_CLIENT_ERROR_REDIRECT_LOOP = 7
} JSRT_HttpClientError;

// Legacy macro names for backward compatibility
#define JSRT_HTTP_OK JSRT_HTTP_CLIENT_OK
#define JSRT_HTTP_ERROR_INVALID_URL JSRT_HTTP_CLIENT_ERROR_INVALID_URL
#define JSRT_HTTP_ERROR_OUT_OF_MEMORY JSRT_HTTP_CLIENT_ERROR_OUT_OF_MEMORY
#define JSRT_HTTP_ERROR_NETWORK JSRT_HTTP_CLIENT_ERROR_NETWORK
#define JSRT_HTTP_ERROR_TIMEOUT JSRT_HTTP_CLIENT_ERROR_TIMEOUT
#define JSRT_HTTP_ERROR_HTTP_ERROR JSRT_HTTP_CLIENT_ERROR_HTTP_ERROR
#define JSRT_HTTP_ERROR_SSL_ERROR JSRT_HTTP_CLIENT_ERROR_SSL_ERROR
#define JSRT_HTTP_ERROR_REDIRECT_LOOP JSRT_HTTP_CLIENT_ERROR_REDIRECT_LOOP

// Perform a synchronous GET request to the given URL
// Returns a JSRT_HttpResponse structure that must be freed with JSRT_HttpResponseFree
JSRT_HttpResponse JSRT_HttpGet(const char* url);

// Enhanced HTTP GET with custom user agent and timeout
JSRT_HttpResponse JSRT_HttpGetWithOptions(const char* url, const char* user_agent, int timeout_ms);

// Free memory allocated for an HTTP response
void JSRT_HttpResponseFree(JSRT_HttpResponse* response);

#endif