// Static OpenSSL build - minimal HTTP client implementation
#include <stdlib.h>
#include <string.h>
#include "http_client.h"

// Minimal implementations for static builds - return error responses
JSRT_HttpResponse JSRT_HttpGet(const char* url) {
  (void)url;
  JSRT_HttpResponse response = {0};
  response.error = JSRT_HTTP_ERROR_SSL_ERROR;
  response.status_text = strdup("HTTP requests disabled in static builds");
  return response;
}

JSRT_HttpResponse JSRT_HttpGetWithOptions(const char* url, const char* user_agent, int timeout_ms) {
  (void)url;
  (void)user_agent;
  (void)timeout_ms;
  JSRT_HttpResponse response = {0};
  response.error = JSRT_HTTP_ERROR_SSL_ERROR;
  response.status_text = strdup("HTTP requests disabled in static builds");
  return response;
}

void JSRT_HttpResponseFree(JSRT_HttpResponse* response) {
  if (response) {
    if (response->status_text)
      free(response->status_text);
    if (response->body)
      free(response->body);
    if (response->content_type)
      free(response->content_type);
    if (response->etag)
      free(response->etag);
    if (response->last_modified)
      free(response->last_modified);
  }
}