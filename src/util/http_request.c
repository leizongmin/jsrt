#include "http_request.h"
#include "user_agent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

char* jsrt_http_build_request(const char* method, const char* path, const char* host, int port, const char* body,
                              size_t body_len, jsrt_http_header_entry_t* headers) {
  if (!method || !path || !host) {
    return NULL;
  }

  // Calculate initial buffer size
  size_t request_size = strlen(method) + strlen(path) + strlen(host) + body_len + 1024;

  // Calculate additional space needed for custom headers
  jsrt_http_header_entry_t* header = headers;
  while (header) {
    request_size += strlen(header->name) + strlen(header->value) + 4;  // ": \r\n"
    header = header->next;
  }

  char* request = malloc(request_size);
  if (!request)
    return NULL;

  // Build request line and Host header
  int len;
  if (port == 80 || port == 443) {
    // Default ports - don't include in Host header
    len = snprintf(request, request_size,
                   "%s %s HTTP/1.1\r\n"
                   "Host: %s\r\n",
                   method, path, host);
  } else {
    len = snprintf(request, request_size,
                   "%s %s HTTP/1.1\r\n"
                   "Host: %s:%d\r\n",
                   method, path, host, port);
  }

  if (len >= (int)request_size) {
    free(request);
    return NULL;
  }

  // Add custom headers, tracking which standard headers are provided
  int has_user_agent = 0;
  int has_content_type = 0;
  int has_connection = 0;

  header = headers;
  while (header) {
    len += snprintf(request + len, request_size - len, "%s: %s\r\n", header->name, header->value);

    if (strcasecmp(header->name, "User-Agent") == 0)
      has_user_agent = 1;
    if (strcasecmp(header->name, "Content-Type") == 0)
      has_content_type = 1;
    if (strcasecmp(header->name, "Connection") == 0)
      has_connection = 1;

    header = header->next;
  }

  // Add default User-Agent if not provided
  if (!has_user_agent) {
    const char* user_agent = jsrt_get_static_user_agent();
    len += snprintf(request + len, request_size - len, "User-Agent: %s\r\n", user_agent);
  }

  // Add default Connection header if not provided
  if (!has_connection) {
    len += snprintf(request + len, request_size - len, "Connection: close\r\n");
  }

  // Add Content-Length if body is present
  if (body && body_len > 0) {
    len += snprintf(request + len, request_size - len, "Content-Length: %zu\r\n", body_len);
  }

  // End of headers
  len += snprintf(request + len, request_size - len, "\r\n");

  // Append body if present
  if (body && body_len > 0) {
    if (len + body_len < request_size) {
      memcpy(request + len, body, body_len);
      len += body_len;
    } else {
      free(request);
      return NULL;
    }
  }

  return request;
}

void jsrt_http_free_headers(jsrt_http_header_entry_t* headers) {
  while (headers) {
    jsrt_http_header_entry_t* next = headers->next;
    free(headers->name);
    free(headers->value);
    free(headers);
    headers = next;
  }
}

jsrt_http_header_entry_t* jsrt_http_header_create(const char* name, const char* value) {
  if (!name || !value) {
    return NULL;
  }

  jsrt_http_header_entry_t* header = malloc(sizeof(jsrt_http_header_entry_t));
  if (!header) {
    return NULL;
  }

  header->name = strdup(name);
  header->value = strdup(value);
  header->next = NULL;

  if (!header->name || !header->value) {
    free(header->name);
    free(header->value);
    free(header);
    return NULL;
  }

  return header;
}

int jsrt_http_header_add(jsrt_http_header_entry_t** head, const char* name, const char* value) {
  jsrt_http_header_entry_t* new_header = jsrt_http_header_create(name, value);
  if (!new_header) {
    return -1;
  }

  if (*head == NULL) {
    *head = new_header;
  } else {
    jsrt_http_header_entry_t* current = *head;
    while (current->next) {
      current = current->next;
    }
    current->next = new_header;
  }

  return 0;
}
