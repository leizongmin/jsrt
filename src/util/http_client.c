#include "http_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define close closesocket
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// Internal function to parse URLs
static int parse_url(const char* url, char** host, int* port, char** path) {
  // Simple URL parser for http://host:port/path format
  if (strncmp(url, "http://", 7) != 0) {
    return -1;  // Only support HTTP for now (HTTPS would require SSL/TLS)
  }

  const char* start = url + 7;  // Skip "http://"

  // Find the end of hostname (either : or / or end of string)
  const char* host_end = start;
  while (*host_end && *host_end != ':' && *host_end != '/') {
    host_end++;
  }

  // Extract hostname
  size_t host_len = host_end - start;
  *host = malloc(host_len + 1);
  if (!*host)
    return -1;
  strncpy(*host, start, host_len);
  (*host)[host_len] = '\0';

  // Default port
  *port = 80;

  // Parse port if present
  if (*host_end == ':') {
    *port = atoi(host_end + 1);
    // Find start of path
    while (*host_end && *host_end != '/') {
      host_end++;
    }
  }

  // Extract path
  if (*host_end == '/') {
    *path = strdup(host_end);
  } else {
    *path = strdup("/");
  }

  if (!*path) {
    free(*host);
    return -1;
  }

  return 0;
}

// Internal function to build HTTP request
static char* build_http_request(const char* method, const char* path, const char* host, int port) {
  size_t request_size = strlen(method) + strlen(path) + strlen(host) + 200;
  char* request = malloc(request_size);
  if (!request)
    return NULL;

  int len;
  if (port == 80) {
    len = snprintf(request, request_size,
                   "%s %s HTTP/1.1\r\n"
                   "Host: %s\r\n"
                   "Connection: close\r\n"
                   "User-Agent: jsrt/1.0\r\n"
                   "\r\n",
                   method, path, host);
  } else {
    len = snprintf(request, request_size,
                   "%s %s HTTP/1.1\r\n"
                   "Host: %s:%d\r\n"
                   "Connection: close\r\n"
                   "User-Agent: jsrt/1.0\r\n"
                   "\r\n",
                   method, path, host, port);
  }

  if (len >= (int)request_size) {
    free(request);
    return NULL;
  }

  return request;
}

// Internal function to parse HTTP response
static JSRT_HttpResponse parse_http_response(const char* response_data, size_t response_size) {
  JSRT_HttpResponse response = {0};

  if (!response_data || response_size == 0) {
    response.error = JSRT_HTTP_ERROR_NETWORK;
    return response;
  }

  // Find end of headers
  const char* headers_end = strstr(response_data, "\r\n\r\n");
  if (!headers_end) {
    headers_end = strstr(response_data, "\n\n");
    if (!headers_end) {
      response.error = JSRT_HTTP_ERROR_HTTP_ERROR;
      return response;
    }
    headers_end += 2;
  } else {
    headers_end += 4;
  }

  // Parse status line
  int major, minor;
  char status_text_buffer[256] = {0};
  if (sscanf(response_data, "HTTP/%d.%d %d %255[^\r\n]", &major, &minor, &response.status, status_text_buffer) >= 3) {
    if (strlen(status_text_buffer) > 0) {
      response.status_text = strdup(status_text_buffer);
    } else {
      response.status_text = strdup("OK");
    }
  } else {
    response.status = 500;
    response.status_text = strdup("Parse Error");
  }

  // Extract body
  size_t body_size = response_size - (headers_end - response_data);
  if (body_size > 0) {
    response.body = malloc(body_size + 1);
    if (response.body) {
      memcpy(response.body, headers_end, body_size);
      response.body[body_size] = '\0';
      response.body_size = body_size;
    }
  }

  response.error = JSRT_HTTP_OK;
  return response;
}

JSRT_HttpResponse JSRT_HttpGet(const char* url) {
  JSRT_HttpResponse response = {0};
  char* host = NULL;
  char* path = NULL;
  int port = 80;
#ifdef _WIN32
  SOCKET sockfd = INVALID_SOCKET;
#else
  int sockfd = -1;
#endif
  struct sockaddr_in serv_addr;
  char* http_request = NULL;
  char* response_buffer = NULL;
  size_t response_capacity = 4096;
  size_t response_size = 0;

#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    response.error = JSRT_HTTP_ERROR_NETWORK;
    return response;
  }
#endif

  // Parse URL
  if (parse_url(url, &host, &port, &path) != 0) {
    response.error = JSRT_HTTP_ERROR_INVALID_URL;
    goto cleanup;
  }

  // Build HTTP request
  http_request = build_http_request("GET", path, host, port);
  if (!http_request) {
    response.error = JSRT_HTTP_ERROR_OUT_OF_MEMORY;
    goto cleanup;
  }

  // Create socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
  if (sockfd == INVALID_SOCKET) {
#else
  if (sockfd < 0) {
#endif
    response.error = JSRT_HTTP_ERROR_NETWORK;
    goto cleanup;
  }

  // Setup server address
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  // Resolve hostname
#ifdef _WIN32
  struct addrinfo hints, *result = NULL;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(host, NULL, &hints, &result) != 0 || !result) {
    response.error = JSRT_HTTP_ERROR_NETWORK;
    goto cleanup;
  }

  struct sockaddr_in* addr_in = (struct sockaddr_in*)result->ai_addr;
  serv_addr.sin_addr = addr_in->sin_addr;
  freeaddrinfo(result);
#else
  struct hostent* server = gethostbyname(host);
  if (!server) {
    response.error = JSRT_HTTP_ERROR_NETWORK;
    goto cleanup;
  }
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
#endif

  // Connect to server
  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    response.error = JSRT_HTTP_ERROR_NETWORK;
    goto cleanup;
  }

  // Send HTTP request
  size_t request_len = strlen(http_request);
  ssize_t sent = send(sockfd, http_request, request_len, 0);
  if (sent < 0 || (size_t)sent != request_len) {
    response.error = JSRT_HTTP_ERROR_NETWORK;
    goto cleanup;
  }

  // Read response
  response_buffer = malloc(response_capacity);
  if (!response_buffer) {
    response.error = JSRT_HTTP_ERROR_OUT_OF_MEMORY;
    goto cleanup;
  }

  while (1) {
    ssize_t received = recv(sockfd, response_buffer + response_size, response_capacity - response_size - 1, 0);
    if (received <= 0)
      break;

    response_size += received;

    // Expand buffer if needed
    if (response_size >= response_capacity - 1) {
      response_capacity *= 2;
      char* new_buffer = realloc(response_buffer, response_capacity);
      if (!new_buffer) {
        response.error = JSRT_HTTP_ERROR_OUT_OF_MEMORY;
        goto cleanup;
      }
      response_buffer = new_buffer;
    }
  }

  response_buffer[response_size] = '\0';

  // Parse HTTP response
  response = parse_http_response(response_buffer, response_size);

cleanup:
  if (host)
    free(host);
  if (path)
    free(path);
  if (http_request)
    free(http_request);
  if (response_buffer)
    free(response_buffer);
#ifdef _WIN32
  if (sockfd != INVALID_SOCKET)
    closesocket(sockfd);
  WSACleanup();
#else
  if (sockfd >= 0)
    close(sockfd);
#endif

  return response;
}

// Enhanced HTTP GET with custom user agent and timeout
JSRT_HttpResponse JSRT_HttpGetWithOptions(const char* url, const char* user_agent, int timeout_ms) {
  // For now, just use the existing JSRT_HttpGet function
  // TODO: Implement proper timeout and user agent support
  // This is a minimal implementation to get HTTP module loading working
  return JSRT_HttpGet(url);
}

void JSRT_HttpResponseFree(JSRT_HttpResponse* response) {
  if (!response)
    return;

  if (response->status_text) {
    free(response->status_text);
    response->status_text = NULL;
  }

  if (response->body) {
    free(response->body);
    response->body = NULL;
  }

  if (response->content_type) {
    free(response->content_type);
    response->content_type = NULL;
  }

  if (response->etag) {
    free(response->etag);
    response->etag = NULL;
  }

  if (response->last_modified) {
    free(response->last_modified);
    response->last_modified = NULL;
  }

  response->body_size = 0;
  response->status = 0;
  response->error = 0;
}