// Static OpenSSL build - HTTP client implementation using static OpenSSL
#include "../util/debug.h"
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

// Direct OpenSSL includes for static linking
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

// Initialize SSL/TLS support (call once at startup)
static int ssl_initialized = 0;

static void init_ssl() {
  if (!ssl_initialized) {
    SSL_library_init();
    SSL_load_error_strings();
    ssl_initialized = 1;
  }
}

// Parse URL into components
typedef struct {
  char* scheme;
  char* host;
  int port;
  char* path;
} ParsedURL;

static ParsedURL parse_url(const char* url) {
  ParsedURL result = {0};

  // Default values
  result.port = 80;
  result.path = strdup("/");

  if (!url)
    return result;

  // Find scheme
  const char* scheme_end = strstr(url, "://");
  if (scheme_end) {
    size_t scheme_len = scheme_end - url;
    result.scheme = malloc(scheme_len + 1);
    strncpy(result.scheme, url, scheme_len);
    result.scheme[scheme_len] = '\0';

    if (strcmp(result.scheme, "https") == 0) {
      result.port = 443;
    }

    url = scheme_end + 3;  // Skip "://"
  } else {
    result.scheme = strdup("http");
  }

  // Find path
  const char* path_start = strchr(url, '/');
  if (path_start) {
    free(result.path);
    result.path = strdup(path_start);
  }

  // Extract host and port
  size_t host_len = path_start ? (path_start - url) : strlen(url);
  const char* colon = strchr(url, ':');
  if (colon && colon < url + host_len) {
    size_t host_only_len = colon - url;
    result.host = malloc(host_only_len + 1);
    strncpy(result.host, url, host_only_len);
    result.host[host_only_len] = '\0';
    result.port = atoi(colon + 1);
  } else {
    result.host = malloc(host_len + 1);
    strncpy(result.host, url, host_len);
    result.host[host_len] = '\0';
  }

  return result;
}

static void free_parsed_url(ParsedURL* url) {
  if (url) {
    free(url->scheme);
    free(url->host);
    free(url->path);
    memset(url, 0, sizeof(ParsedURL));
  }
}

// Perform HTTP GET request
JSRT_HttpResponse JSRT_HttpGet(const char* url) {
  return JSRT_HttpGetWithOptions(url, "jsrt/1.0", 30000);
}

JSRT_HttpResponse JSRT_HttpGetWithOptions(const char* url, const char* user_agent, int timeout_ms) {
  JSRT_HttpResponse response = {0};

  if (!url) {
    response.error = JSRT_HTTP_ERROR_INVALID_URL;
    response.status_text = strdup("Invalid URL");
    return response;
  }

  init_ssl();

  ParsedURL parsed = parse_url(url);
  if (!parsed.host) {
    response.error = JSRT_HTTP_ERROR_INVALID_URL;
    response.status_text = strdup("Failed to parse URL");
    free_parsed_url(&parsed);
    return response;
  }

  // Create socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    response.error = JSRT_HTTP_ERROR_NETWORK;
    response.status_text = strdup("Failed to create socket");
    free_parsed_url(&parsed);
    return response;
  }

  // Resolve hostname
  struct hostent* server = gethostbyname(parsed.host);
  if (!server) {
    close(sockfd);
    response.error = JSRT_HTTP_ERROR_NETWORK;
    response.status_text = strdup("Failed to resolve hostname");
    free_parsed_url(&parsed);
    return response;
  }

  // Connect to server
  struct sockaddr_in serv_addr = {0};
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(parsed.port);
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

  if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    close(sockfd);
    response.error = JSRT_HTTP_ERROR_NETWORK;
    response.status_text = strdup("Failed to connect");
    free_parsed_url(&parsed);
    return response;
  }

  SSL* ssl = NULL;
  SSL_CTX* ctx = NULL;

  // Setup SSL if HTTPS
  if (strcmp(parsed.scheme, "https") == 0) {
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
      close(sockfd);
      response.error = JSRT_HTTP_ERROR_SSL_ERROR;
      response.status_text = strdup("Failed to create SSL context");
      free_parsed_url(&parsed);
      return response;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    SSL_CTX_set_default_verify_paths(ctx);

    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    SSL_set_tlsext_host_name(ssl, parsed.host);

    if (SSL_connect(ssl) <= 0) {
      SSL_free(ssl);
      SSL_CTX_free(ctx);
      close(sockfd);
      response.error = JSRT_HTTP_ERROR_SSL_ERROR;
      response.status_text = strdup("SSL handshake failed");
      free_parsed_url(&parsed);
      return response;
    }
  }

  // Send HTTP request
  char request[4096];
  snprintf(request, sizeof(request),
           "GET %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "User-Agent: %s\r\n"
           "Connection: close\r\n"
           "\r\n",
           parsed.path, parsed.host, user_agent ? user_agent : "jsrt/1.0");

  int bytes_sent;
  if (ssl) {
    bytes_sent = SSL_write(ssl, request, strlen(request));
  } else {
    bytes_sent = send(sockfd, request, strlen(request), 0);
  }

  if (bytes_sent <= 0) {
    if (ssl) {
      SSL_free(ssl);
      SSL_CTX_free(ctx);
    }
    close(sockfd);
    response.error = JSRT_HTTP_ERROR_NETWORK;
    response.status_text = strdup("Failed to send request");
    free_parsed_url(&parsed);
    return response;
  }

  // Read response
  char buffer[4096];
  size_t total_received = 0;
  char* response_data = NULL;

  while (1) {
    int bytes_received;
    if (ssl) {
      bytes_received = SSL_read(ssl, buffer, sizeof(buffer));
    } else {
      bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
    }

    if (bytes_received <= 0)
      break;

    response_data = realloc(response_data, total_received + bytes_received + 1);
    if (!response_data) {
      if (ssl) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
      }
      close(sockfd);
      response.error = JSRT_HTTP_ERROR_OUT_OF_MEMORY;
      response.status_text = strdup("Out of memory");
      free_parsed_url(&parsed);
      return response;
    }

    memcpy(response_data + total_received, buffer, bytes_received);
    total_received += bytes_received;
    response_data[total_received] = '\0';
  }

  // Clean up connections
  if (ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
  }
  close(sockfd);

  if (!response_data) {
    response.error = JSRT_HTTP_ERROR_NETWORK;
    response.status_text = strdup("No response received");
    free_parsed_url(&parsed);
    return response;
  }

  // Parse HTTP response
  char* header_end = strstr(response_data, "\r\n\r\n");
  if (!header_end) {
    response.error = JSRT_HTTP_ERROR_HTTP_ERROR;
    response.status_text = strdup("Invalid HTTP response");
    free(response_data);
    free_parsed_url(&parsed);
    return response;
  }

  // Extract status code
  if (strncmp(response_data, "HTTP/", 5) == 0) {
    char* status_start = strchr(response_data, ' ');
    if (status_start) {
      response.status = atoi(status_start + 1);
    }
  }

  // Extract body
  char* body_start = header_end + 4;
  size_t body_size = total_received - (body_start - response_data);
  if (body_size > 0) {
    response.body = malloc(body_size + 1);
    memcpy(response.body, body_start, body_size);
    response.body[body_size] = '\0';
    response.body_size = body_size;
  }

  response.error = JSRT_HTTP_OK;
  response.status_text = strdup("OK");

  free(response_data);
  free_parsed_url(&parsed);
  return response;
}

void JSRT_HttpResponseFree(JSRT_HttpResponse* response) {
  if (response) {
    free(response->status_text);
    free(response->body);
    free(response->content_type);
    free(response->etag);
    free(response->last_modified);
    memset(response, 0, sizeof(JSRT_HttpResponse));
  }
}