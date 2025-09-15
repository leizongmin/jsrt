#include "http_client.h"
#include "debug.h"
#include "ssl_client.h"
#include "url_parser.h"
#include "user_agent.h"

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

// Cross-platform case-insensitive string comparison
#ifdef _WIN32
#define jsrt_strncasecmp _strnicmp
#else
#ifdef __GNUC__
#define jsrt_strncasecmp strncasecmp
#else
// Fallback implementation for systems without strncasecmp
static int jsrt_strncasecmp(const char* s1, const char* s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char c1 = s1[i] >= 'A' && s1[i] <= 'Z' ? s1[i] + ('a' - 'A') : s1[i];
    char c2 = s2[i] >= 'A' && s2[i] <= 'Z' ? s2[i] + ('a' - 'A') : s2[i];
    if (c1 != c2)
      return c1 - c2;
    if (c1 == '\0')
      return 0;
  }
  return 0;
}
#endif
#endif

// Internal function to parse URLs using unified parser
static int parse_url(const char* url, char** host, int* port, char** path, int* is_https) {
  jsrt_url_t parsed_url;

  if (jsrt_url_parse(url, &parsed_url) != 0) {
    return -1;
  }

  // Copy parsed components
  *host = strdup(parsed_url.host);
  *port = parsed_url.port;
  *path = strdup(parsed_url.path);
  *is_https = parsed_url.is_secure;

  // Check for allocation failures
  if (!*host || !*path) {
    if (*host)
      free(*host);
    if (*path)
      free(*path);
    jsrt_url_free(&parsed_url);
    return -1;
  }

  jsrt_url_free(&parsed_url);
  return 0;
}

// Initialize SSL using unified SSL client
static int init_ssl_functions(void) {
  return jsrt_ssl_global_init();
}

// Extract Location header for redirect handling
static char* extract_location_header(const char* response_data, size_t response_size) {
  const char* headers_end = strstr(response_data, "\r\n\r\n");
  if (!headers_end) {
    headers_end = strstr(response_data, "\n\n");
    if (!headers_end) {
      return NULL;
    }
  }

  // Look for Location header (case insensitive)
  const char* current = response_data;
  while (current < headers_end) {
    const char* line_end = strstr(current, "\r\n");
    if (!line_end) {
      line_end = strstr(current, "\n");
      if (!line_end)
        break;
    }

    // Check if this line starts with Location:
    if ((jsrt_strncasecmp(current, "location:", 9) == 0) || (jsrt_strncasecmp(current, "Location:", 9) == 0)) {
      const char* value_start = current + 9;

      // Skip whitespace
      while (value_start < line_end && (*value_start == ' ' || *value_start == '\t')) {
        value_start++;
      }

      // Extract location value
      size_t value_len = line_end - value_start;
      if (value_len > 0) {
        char* location = malloc(value_len + 1);
        if (location) {
          strncpy(location, value_start, value_len);
          location[value_len] = '\0';
          return location;
        }
      }
    }

    current = line_end + (line_end[0] == '\r' ? 2 : 1);
  }

  return NULL;
}

// Internal function to build HTTP request
static char* build_http_request(const char* method, const char* path, const char* host, int port) {
  size_t request_size = strlen(method) + strlen(path) + strlen(host) + 200;
  char* request = malloc(request_size);
  if (!request)
    return NULL;

  // Use static user-agent since we don't have JavaScript context here
  const char* user_agent = jsrt_get_static_user_agent();

  int len;
  if (port == 80 || port == 443) {
    len = snprintf(request, request_size,
                   "%s %s HTTP/1.1\r\n"
                   "Host: %s\r\n"
                   "Connection: close\r\n"
                   "User-Agent: %s\r\n"
                   "\r\n",
                   method, path, host, user_agent);
  } else {
    len = snprintf(request, request_size,
                   "%s %s HTTP/1.1\r\n"
                   "Host: %s:%d\r\n"
                   "Connection: close\r\n"
                   "User-Agent: %s\r\n"
                   "\r\n",
                   method, path, host, port, user_agent);
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

// Curl fallback function for SSL handshake failures
static JSRT_HttpResponse try_curl_fallback(const char* url) {
  JSRT_HttpResponse response = {0};
  response.error = JSRT_HTTP_ERROR_NETWORK;

  char temp_file[] = "/tmp/jsrt_curl_XXXXXX";
  int temp_fd = mkstemp(temp_file);
  if (temp_fd == -1) {
    JSRT_Debug("HTTP Client: curl fallback - failed to create temp file");
    return response;
  }
  close(temp_fd);

  // Build curl command with redirects, silence progress, and write to temp file
  char curl_cmd[2048];
  snprintf(curl_cmd, sizeof(curl_cmd), "curl -s -L --max-time 30 --connect-timeout 10 -o '%s' '%s' 2>/dev/null",
           temp_file, url);

  JSRT_Debug("HTTP Client: curl fallback - executing curl command");

  int curl_result = system(curl_cmd);
  if (curl_result != 0) {
    JSRT_Debug("HTTP Client: curl fallback - curl command failed with exit code %d", curl_result);
    unlink(temp_file);
    return response;
  }

  // Read the response from temp file
  FILE* temp_fp = fopen(temp_file, "rb");
  if (!temp_fp) {
    JSRT_Debug("HTTP Client: curl fallback - failed to open temp file");
    unlink(temp_file);
    return response;
  }

  // Get file size
  fseek(temp_fp, 0, SEEK_END);
  long file_size = ftell(temp_fp);
  fseek(temp_fp, 0, SEEK_SET);

  if (file_size <= 0) {
    JSRT_Debug("HTTP Client: curl fallback - temp file is empty");
    fclose(temp_fp);
    unlink(temp_file);
    return response;
  }

  // Allocate and read data
  response.body = malloc(file_size + 1);
  if (!response.body) {
    JSRT_Debug("HTTP Client: curl fallback - failed to allocate memory");
    fclose(temp_fp);
    unlink(temp_file);
    return response;
  }

  size_t bytes_read = fread(response.body, 1, file_size, temp_fp);
  fclose(temp_fp);
  unlink(temp_file);

  if ((long)bytes_read != file_size) {
    JSRT_Debug("HTTP Client: curl fallback - failed to read complete file");
    free(response.body);
    response.body = NULL;
    return response;
  }

  response.body[file_size] = '\0';
  response.body_size = file_size;
  response.status = 200;  // Assume success if curl succeeded
  response.error = JSRT_HTTP_OK;

  JSRT_Debug("HTTP Client: curl fallback - successfully read %zu bytes", response.body_size);
  return response;
}

// Internal function to perform HTTP request with SSL/redirect support
static JSRT_HttpResponse http_request_internal(const char* url, int redirect_count);

JSRT_HttpResponse JSRT_HttpGet(const char* url) {
  return http_request_internal(url, 0);
}

static JSRT_HttpResponse http_request_internal(const char* url, int redirect_count) {
  JSRT_HttpResponse response = {0};
  char* host = NULL;
  char* path = NULL;
  int port = 80;
  int is_https = 0;
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
  jsrt_ssl_client_t* ssl_client = NULL;

  // Prevent infinite redirect loops
  if (redirect_count > 10) {
    response.error = JSRT_HTTP_ERROR_REDIRECT_LOOP;
    return response;
  }

#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    response.error = JSRT_HTTP_ERROR_NETWORK;
    return response;
  }
#endif

  // Parse URL
  if (parse_url(url, &host, &port, &path, &is_https) != 0) {
    response.error = JSRT_HTTP_ERROR_INVALID_URL;
    goto cleanup;
  }

  JSRT_Debug("HTTP Client: Requesting %s://%s:%d%s", is_https ? "https" : "http", host, port, path);

  // Initialize SSL if needed
  if (is_https && !init_ssl_functions()) {
    JSRT_Debug("HTTP Client: HTTPS requested but SSL not available");
    response.error = JSRT_HTTP_ERROR_SSL_ERROR;
    goto cleanup;
  }

  // Build HTTP request
  http_request = build_http_request("GET", path, host, port);
  if (!http_request) {
    response.error = JSRT_HTTP_ERROR_OUT_OF_MEMORY;
    goto cleanup;
  }

  // Setup SSL client if needed
  if (is_https) {
    ssl_client = jsrt_ssl_client_new();
    if (!ssl_client) {
      response.error = JSRT_HTTP_ERROR_SSL_ERROR;
      goto cleanup;
    }
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

  // Setup SSL connection if needed
  if (is_https) {
    if (jsrt_ssl_client_setup(ssl_client, sockfd, host) != 0) {
      JSRT_Debug("HTTP Client: SSL setup failed");
      response.error = JSRT_HTTP_ERROR_SSL_ERROR;
      goto cleanup;
    }

    if (jsrt_ssl_client_handshake(ssl_client) != 1) {
      JSRT_Debug("HTTP Client: SSL handshake failed - trying curl fallback");

      // Try using curl as a fallback
      JSRT_HttpResponse curl_response = try_curl_fallback(url);
      if (curl_response.error == JSRT_HTTP_OK) {
        JSRT_Debug("HTTP Client: curl fallback succeeded");
        // Clean up current SSL resources first
        if (ssl_client)
          jsrt_ssl_client_free(ssl_client);
#ifdef _WIN32
        if (sockfd != INVALID_SOCKET)
          closesocket(sockfd);
        WSACleanup();
#else
        if (sockfd >= 0)
          close(sockfd);
#endif
        free(host);
        free(path);
        return curl_response;
      } else {
        JSRT_Debug("HTTP Client: curl fallback also failed");
      }

      response.error = JSRT_HTTP_ERROR_SSL_ERROR;
      goto cleanup;
    }

    JSRT_Debug("HTTP Client: SSL handshake successful");
  }

  // Send HTTP request
  size_t request_len = strlen(http_request);
  ssize_t sent;

  if (is_https) {
    sent = jsrt_ssl_client_write(ssl_client, http_request, request_len);
    if (sent <= 0) {
      response.error = JSRT_HTTP_ERROR_SSL_ERROR;
      goto cleanup;
    }
  } else {
    sent = send(sockfd, http_request, request_len, 0);
    if (sent < 0 || (size_t)sent != request_len) {
      response.error = JSRT_HTTP_ERROR_NETWORK;
      goto cleanup;
    }
  }

  // Read response
  response_buffer = malloc(response_capacity);
  if (!response_buffer) {
    response.error = JSRT_HTTP_ERROR_OUT_OF_MEMORY;
    goto cleanup;
  }

  while (1) {
    ssize_t received;

    if (is_https) {
      received =
          jsrt_ssl_client_read(ssl_client, response_buffer + response_size, response_capacity - response_size - 1);
      if (received <= 0) {
        break;  // Connection closed or error
      }
    } else {
      received = recv(sockfd, response_buffer + response_size, response_capacity - response_size - 1, 0);
      if (received <= 0)
        break;
    }

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
  JSRT_Debug("HTTP Client: Received %zu bytes", response_size);

  // Parse HTTP response
  response = parse_http_response(response_buffer, response_size);

  // Handle redirects
  if (response.status >= 300 && response.status < 400 && response.error == JSRT_HTTP_OK) {
    char* location = extract_location_header(response_buffer, response_size);
    if (location) {
      JSRT_Debug("HTTP Client: Redirecting to: %s", location);

      // Clean up current resources
      JSRT_HttpResponseFree(&response);

      // Recursive call to handle redirect
      response = http_request_internal(location, redirect_count + 1);
      free(location);

      // Clean up and return redirect result
      goto cleanup_no_response;
    }
  }

cleanup:
cleanup_no_response:
  // SSL cleanup
  if (ssl_client) {
    jsrt_ssl_client_free(ssl_client);
  }

  // Standard cleanup
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