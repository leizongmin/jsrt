#ifndef JSRT_HTTP_REQUEST_H
#define JSRT_HTTP_REQUEST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// HTTP header entry for linked list
typedef struct jsrt_http_header_entry {
  char* name;
  char* value;
  struct jsrt_http_header_entry* next;
} jsrt_http_header_entry_t;

/**
 * Build an HTTP request string
 *
 * @param method HTTP method (e.g., "GET", "POST")
 * @param path Request path (e.g., "/index.html")
 * @param host Hostname (e.g., "example.com")
 * @param port Port number (e.g., 80, 443)
 * @param body Request body (can be NULL)
 * @param body_len Length of request body
 * @param headers Linked list of custom headers (can be NULL)
 * @return Allocated request string, or NULL on error. Caller must free.
 */
char* jsrt_http_build_request(const char* method, const char* path, const char* host, int port, const char* body,
                              size_t body_len, jsrt_http_header_entry_t* headers);

/**
 * Free a linked list of HTTP headers
 *
 * @param headers Linked list to free
 */
void jsrt_http_free_headers(jsrt_http_header_entry_t* headers);

/**
 * Create a new HTTP header entry
 *
 * @param name Header name
 * @param value Header value
 * @return Allocated header entry, or NULL on error. Caller must free with jsrt_http_free_headers.
 */
jsrt_http_header_entry_t* jsrt_http_header_create(const char* name, const char* value);

/**
 * Add a header to the end of a linked list
 *
 * @param head Pointer to the head of the list (can point to NULL)
 * @param name Header name
 * @param value Header value
 * @return 0 on success, -1 on error
 */
int jsrt_http_header_add(jsrt_http_header_entry_t** head, const char* name, const char* value);

#ifdef __cplusplus
}
#endif

#endif  // JSRT_HTTP_REQUEST_H
