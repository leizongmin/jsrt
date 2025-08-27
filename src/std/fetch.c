#include "fetch.h"

#include <ctype.h>
#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "../util/debug.h"
#include "../util/jsutils.h"

// Forward declare class IDs
static JSClassID JSRT_HeadersClassID;
static JSClassID JSRT_RequestClassID;
static JSClassID JSRT_ResponseClassID;

// Headers structure
typedef struct JSRT_HeaderItem {
  char *name;
  char *value;
  struct JSRT_HeaderItem *next;
} JSRT_HeaderItem;

typedef struct {
  JSRT_HeaderItem *first;
} JSRT_Headers;

// Request structure
typedef struct {
  char *method;
  char *url;
  JSRT_Headers *headers;
  JSValue body;
} JSRT_Request;

// Response structure
typedef struct {
  int status;
  char *status_text;
  JSRT_Headers *headers;
  JSValue body;
  bool ok;
} JSRT_Response;

// HTTP request context for async operations
typedef struct {
  JSRT_Runtime *rt;
  uv_tcp_t tcp_handle;
  uv_connect_t connect_req;
  uv_write_t write_req;
  char *host;
  int port;
  char *path;
  char *method;
  JSRT_Headers *request_headers;

  // Response data
  char *response_buffer;
  size_t response_size;
  size_t response_capacity;

  // Promise resolvers
  JSValue resolve_func;
  JSValue reject_func;
} JSRT_FetchContext;

// Helper functions
static JSRT_Headers *JSRT_HeadersNew();
static void JSRT_HeadersFree(JSRT_Headers *headers);
static void JSRT_HeadersSet(JSRT_Headers *headers, const char *name, const char *value);
static const char *JSRT_HeadersGet(JSRT_Headers *headers, const char *name);
static bool JSRT_HeadersHas(JSRT_Headers *headers, const char *name);
static void JSRT_HeadersDelete(JSRT_Headers *headers, const char *name);

// Headers class implementation
static void JSRT_HeadersFinalize(JSRuntime *rt, JSValue val) {
  JSRT_Headers *headers = JS_GetOpaque(val, JSRT_HeadersClassID);
  if (headers) {
    JSRT_HeadersFree(headers);
  }
}

static JSClassDef JSRT_HeadersClass = {
    .class_name = "Headers",
    .finalizer = JSRT_HeadersFinalize,
};

static JSValue JSRT_HeadersConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JSRT_HeadersNew();

  JSValue obj = JS_NewObjectClass(ctx, JSRT_HeadersClassID);
  JS_SetOpaque(obj, headers);
  return obj;
}

static JSValue JSRT_HeadersGetMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JS_GetOpaque2(ctx, this_val, JSRT_HeadersClassID);
  if (!headers) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing name parameter");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  const char *value = JSRT_HeadersGet(headers, name);
  JS_FreeCString(ctx, name);

  if (value) {
    return JS_NewString(ctx, value);
  } else {
    return JS_NULL;
  }
}

static JSValue JSRT_HeadersSetMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JS_GetOpaque2(ctx, this_val, JSRT_HeadersClassID);
  if (!headers) {
    return JS_EXCEPTION;
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Missing name or value parameter");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  const char *value = JS_ToCString(ctx, argv[1]);
  if (!name || !value) {
    if (name) JS_FreeCString(ctx, name);
    if (value) JS_FreeCString(ctx, value);
    return JS_EXCEPTION;
  }

  JSRT_HeadersSet(headers, name, value);
  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);

  return JS_UNDEFINED;
}

static JSValue JSRT_HeadersHasMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JS_GetOpaque2(ctx, this_val, JSRT_HeadersClassID);
  if (!headers) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing name parameter");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  bool has = JSRT_HeadersHas(headers, name);
  JS_FreeCString(ctx, name);

  return JS_NewBool(ctx, has);
}

static JSValue JSRT_HeadersDeleteMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Headers *headers = JS_GetOpaque2(ctx, this_val, JSRT_HeadersClassID);
  if (!headers) {
    return JS_EXCEPTION;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing name parameter");
  }

  const char *name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  JSRT_HeadersDelete(headers, name);
  JS_FreeCString(ctx, name);

  return JS_UNDEFINED;
}

// Request class implementation
static void JSRT_RequestFinalize(JSRuntime *rt, JSValue val) {
  JSRT_Request *request = JS_GetOpaque(val, JSRT_RequestClassID);
  if (request) {
    free(request->method);
    free(request->url);
    JSRT_HeadersFree(request->headers);
    JS_FreeValueRT(rt, request->body);
    free(request);
  }
}

static JSClassDef JSRT_RequestClass = {
    .class_name = "Request",
    .finalizer = JSRT_RequestFinalize,
};

static JSValue JSRT_RequestConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing input parameter");
  }

  const char *input = JS_ToCString(ctx, argv[0]);
  if (!input) {
    return JS_EXCEPTION;
  }

  JSRT_Request *request = malloc(sizeof(JSRT_Request));
  request->method = strdup("GET");
  request->url = strdup(input);
  request->headers = JSRT_HeadersNew();
  request->body = JS_UNDEFINED;

  JS_FreeCString(ctx, input);

  // Handle options parameter
  if (argc > 1 && JS_IsObject(argv[1])) {
    JSValue method_val = JS_GetPropertyStr(ctx, argv[1], "method");
    if (JS_IsString(method_val)) {
      const char *method = JS_ToCString(ctx, method_val);
      if (method) {
        free(request->method);
        request->method = strdup(method);
        JS_FreeCString(ctx, method);
      }
    }
    JS_FreeValue(ctx, method_val);
  }

  JSValue obj = JS_NewObjectClass(ctx, JSRT_RequestClassID);
  JS_SetOpaque(obj, request);
  return obj;
}

static JSValue JSRT_RequestGetMethod(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Request *request = JS_GetOpaque2(ctx, this_val, JSRT_RequestClassID);
  if (!request) {
    return JS_EXCEPTION;
  }
  return JS_NewString(ctx, request->method);
}

static JSValue JSRT_RequestGetUrl(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Request *request = JS_GetOpaque2(ctx, this_val, JSRT_RequestClassID);
  if (!request) {
    return JS_EXCEPTION;
  }
  return JS_NewString(ctx, request->url);
}

// Response class implementation
static void JSRT_ResponseFinalize(JSRuntime *rt, JSValue val) {
  JSRT_Response *response = JS_GetOpaque(val, JSRT_ResponseClassID);
  if (response) {
    free(response->status_text);
    JSRT_HeadersFree(response->headers);
    JS_FreeValueRT(rt, response->body);
    free(response);
  }
}

static JSClassDef JSRT_ResponseClass = {
    .class_name = "Response",
    .finalizer = JSRT_ResponseFinalize,
};

static JSValue JSRT_ResponseConstructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
  JSRT_Response *response = malloc(sizeof(JSRT_Response));
  response->status = 200;
  response->status_text = strdup("OK");
  response->headers = JSRT_HeadersNew();
  response->body = JS_UNDEFINED;
  response->ok = true;

  JSValue obj = JS_NewObjectClass(ctx, JSRT_ResponseClassID);
  JS_SetOpaque(obj, response);
  return obj;
}

static JSValue JSRT_ResponseGetStatus(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Response *response = JS_GetOpaque2(ctx, this_val, JSRT_ResponseClassID);
  if (!response) {
    return JS_EXCEPTION;
  }
  return JS_NewInt32(ctx, response->status);
}

static JSValue JSRT_ResponseGetOk(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Response *response = JS_GetOpaque2(ctx, this_val, JSRT_ResponseClassID);
  if (!response) {
    return JS_EXCEPTION;
  }
  return JS_NewBool(ctx, response->ok);
}

static JSValue JSRT_ResponseText(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Response *response = JS_GetOpaque2(ctx, this_val, JSRT_ResponseClassID);
  if (!response) {
    return JS_EXCEPTION;
  }

  // For now, just return the body as is if it's a string
  return JS_DupValue(ctx, response->body);
}

static JSValue JSRT_ResponseJson(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  JSRT_Response *response = JS_GetOpaque2(ctx, this_val, JSRT_ResponseClassID);
  if (!response) {
    return JS_EXCEPTION;
  }

  // For now, just parse the body as JSON
  if (JS_IsString(response->body)) {
    const char *json_str = JS_ToCString(ctx, response->body);
    if (!json_str) {
      return JS_EXCEPTION;
    }

    JSValue result = JS_ParseJSON(ctx, json_str, strlen(json_str), "<response>");
    JS_FreeCString(ctx, json_str);
    return result;
  }

  return JS_UNDEFINED;
}

// Forward declarations for HTTP implementation
static void JSRT_FetchContextFree(JSRT_FetchContext *ctx);
static void JSRT_FetchOnConnect(uv_connect_t *req, int status);
static void JSRT_FetchOnWrite(uv_write_t *req, int status);
static void JSRT_FetchOnRead(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
static void JSRT_FetchAllocBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
static void JSRT_FetchOnGetAddrInfo(uv_getaddrinfo_t *req, int status, struct addrinfo *res);
static void JSRT_FetchOnClose(uv_handle_t *handle);
static int JSRT_FetchParseURL(const char *url, char **host, int *port, char **path);
static char *JSRT_FetchBuildHttpRequest(const char *method, const char *path, const char *host, int port,
                                        JSRT_Headers *headers);
static JSRT_Response *JSRT_FetchParseHttpResponse(const char *response_data, size_t response_size);

// Real fetch function implementation using libuv
static JSValue JSRT_Fetch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing input parameter");
  }

  const char *input = JS_ToCString(ctx, argv[0]);
  if (!input) {
    return JS_EXCEPTION;
  }

  // Create Promise capability
  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, input);
    return JS_EXCEPTION;
  }

  // Create fetch context
  JSRT_FetchContext *fetch_ctx = malloc(sizeof(JSRT_FetchContext));
  if (!fetch_ctx) {
    JS_FreeCString(ctx, input);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return JS_ThrowOutOfMemory(ctx);
  }

  memset(fetch_ctx, 0, sizeof(JSRT_FetchContext));
  fetch_ctx->rt = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
  fetch_ctx->resolve_func = JS_DupValue(ctx, resolving_funcs[0]);
  fetch_ctx->reject_func = JS_DupValue(ctx, resolving_funcs[1]);

  // Parse URL
  if (JSRT_FetchParseURL(input, &fetch_ctx->host, &fetch_ctx->port, &fetch_ctx->path) != 0) {
    JS_FreeCString(ctx, input);
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid URL"));
    JS_Call(ctx, fetch_ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JSRT_FetchContextFree(fetch_ctx);
    goto cleanup;
  }

  fetch_ctx->method = strdup("GET");  // Default method
  fetch_ctx->request_headers = JSRT_HeadersNew();

  // Process options if provided
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue options = argv[1];

    // Extract method
    JSValue method_val = JS_GetPropertyStr(ctx, options, "method");
    if (JS_IsString(method_val)) {
      const char *method_str = JS_ToCString(ctx, method_val);
      if (method_str) {
        free(fetch_ctx->method);
        fetch_ctx->method = strdup(method_str);
        JS_FreeCString(ctx, method_str);
      }
    }
    JS_FreeValue(ctx, method_val);

    // Extract headers
    JSValue headers_val = JS_GetPropertyStr(ctx, options, "headers");
    if (JS_IsObject(headers_val)) {
      // Try to get as Headers object first
      JSRT_Headers *custom_headers = JS_GetOpaque2(ctx, headers_val, JSRT_HeadersClassID);
      if (custom_headers) {
        // Copy headers from Headers object
        JSRT_HeaderItem *item = custom_headers->first;
        while (item) {
          JSRT_HeadersSet(fetch_ctx->request_headers, item->name, item->value);
          item = item->next;
        }
      } else {
        // Try to iterate as plain object
        JSPropertyEnum *props;
        uint32_t prop_count;
        if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, headers_val, JS_GPN_STRING_MASK) == 0) {
          for (uint32_t i = 0; i < prop_count; i++) {
            JSValue key = JS_AtomToString(ctx, props[i].atom);
            JSValue value = JS_GetProperty(ctx, headers_val, props[i].atom);
            if (JS_IsString(key) && JS_IsString(value)) {
              const char *key_str = JS_ToCString(ctx, key);
              const char *value_str = JS_ToCString(ctx, value);
              if (key_str && value_str) {
                JSRT_HeadersSet(fetch_ctx->request_headers, key_str, value_str);
              }
              if (key_str) JS_FreeCString(ctx, key_str);
              if (value_str) JS_FreeCString(ctx, value_str);
            }
            JS_FreeValue(ctx, key);
            JS_FreeValue(ctx, value);
          }
          for (uint32_t i = 0; i < prop_count; i++) {
            JS_FreeAtom(ctx, props[i].atom);
          }
          js_free(ctx, props);
        }
      }
    }
    JS_FreeValue(ctx, headers_val);

    // TODO: Add body support for POST requests
    // JSValue body_val = JS_GetPropertyStr(ctx, options, "body");
    // JS_FreeValue(ctx, body_val);
  }

  // Set default headers
  if (!JSRT_HeadersGet(fetch_ctx->request_headers, "user-agent")) {
    JSRT_HeadersSet(fetch_ctx->request_headers, "user-agent", "jsrt/1.0");
  }
  if (!JSRT_HeadersGet(fetch_ctx->request_headers, "connection")) {
    JSRT_HeadersSet(fetch_ctx->request_headers, "connection", "close");
  }

  // Start DNS resolution
  uv_getaddrinfo_t *getaddrinfo_req = malloc(sizeof(uv_getaddrinfo_t));
  if (!getaddrinfo_req) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Memory allocation failed"));
    JS_Call(ctx, fetch_ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    JSRT_FetchContextFree(fetch_ctx);
    goto cleanup;
  }

  getaddrinfo_req->data = fetch_ctx;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = 2;    // AF_INET equivalent - IPv4
  hints.ai_socktype = 1;  // SOCK_STREAM equivalent - TCP

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", fetch_ctx->port);

  int ret = uv_getaddrinfo(fetch_ctx->rt->uv_loop, getaddrinfo_req, JSRT_FetchOnGetAddrInfo, fetch_ctx->host, port_str,
                           &hints);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "DNS resolution failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, error_msg));
    JS_Call(ctx, fetch_ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    free(getaddrinfo_req);
    JSRT_FetchContextFree(fetch_ctx);
    goto cleanup;
  }

cleanup:
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeCString(ctx, input);
  return promise;
}

// HTTP implementation helper functions
static void JSRT_FetchContextFree(JSRT_FetchContext *ctx) {
  if (!ctx) return;

  if (ctx->host) free(ctx->host);
  if (ctx->path) free(ctx->path);
  if (ctx->method) free(ctx->method);
  if (ctx->response_buffer) free(ctx->response_buffer);
  if (ctx->request_headers) JSRT_HeadersFree(ctx->request_headers);

  // Free JavaScript values safely - only if runtime context is still valid
  if (ctx->rt && ctx->rt->ctx) {
    if (!JS_IsUndefined(ctx->resolve_func)) {
      JS_FreeValue(ctx->rt->ctx, ctx->resolve_func);
      ctx->resolve_func = JS_UNDEFINED;
    }
    if (!JS_IsUndefined(ctx->reject_func)) {
      JS_FreeValue(ctx->rt->ctx, ctx->reject_func);
      ctx->reject_func = JS_UNDEFINED;
    }
  }

  free(ctx);
}

static int JSRT_FetchParseURL(const char *url, char **host, int *port, char **path) {
  // Simple URL parser for http://host:port/path format
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
    return -1;  // Only support HTTP/HTTPS
  }

  const char *start = url;
  bool is_https = strncmp(url, "https://", 8) == 0;
  start += is_https ? 8 : 7;

  // Find the end of hostname (either : or / or end of string)
  const char *host_end = start;
  while (*host_end && *host_end != ':' && *host_end != '/') {
    host_end++;
  }

  // Extract hostname
  size_t host_len = host_end - start;
  *host = malloc(host_len + 1);
  if (!*host) return -1;
  strncpy(*host, start, host_len);
  (*host)[host_len] = '\0';

  // Default ports
  *port = is_https ? 443 : 80;

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

static char *JSRT_FetchBuildHttpRequest(const char *method, const char *path, const char *host, int port,
                                        JSRT_Headers *headers) {
  // Calculate required buffer size more accurately
  size_t base_size = strlen(method) + strlen(path) + strlen(host) + 100;  // Base request line + host header

  // Calculate headers size
  size_t headers_size = 0;
  if (headers) {
    JSRT_HeaderItem *item = headers->first;
    while (item) {
      headers_size += strlen(item->name) + strlen(item->value) + 4;  // ": \r\n"
      item = item->next;
    }
  }

  size_t request_size = base_size + headers_size + 10;  // Extra buffer for safety
  if (request_size < 1024) request_size = 1024;         // Minimum size

  char *request = malloc(request_size);
  if (!request) return NULL;

  // Build request line
  int len = snprintf(request, request_size, "%s %s HTTP/1.1\r\n", method, path);
  if (len >= (int)request_size) {
    free(request);
    return NULL;
  }

  // Add Host header - handle default ports properly
  if (port == 80) {
    len += snprintf(request + len, request_size - len, "Host: %s\r\n", host);
  } else {
    len += snprintf(request + len, request_size - len, "Host: %s:%d\r\n", host, port);
  }

  if (len >= (int)request_size) {
    free(request);
    return NULL;
  }

  // Add custom headers
  if (headers) {
    JSRT_HeaderItem *item = headers->first;
    while (item && len < (int)request_size - 10) {
      int header_len = snprintf(request + len, request_size - len, "%s: %s\r\n", item->name, item->value);
      if (len + header_len >= (int)request_size) {
        // Need more space - reallocate
        request_size *= 2;
        char *new_request = realloc(request, request_size);
        if (!new_request) {
          free(request);
          return NULL;
        }
        request = new_request;
        header_len = snprintf(request + len, request_size - len, "%s: %s\r\n", item->name, item->value);
      }
      len += header_len;
      item = item->next;
    }
  }

  // End headers
  if (len + 3 < (int)request_size) {
    len += snprintf(request + len, request_size - len, "\r\n");
  } else {
    free(request);
    return NULL;
  }

  return request;
}

static JSRT_Response *JSRT_FetchParseHttpResponse(const char *response_data, size_t response_size) {
  if (!response_data || response_size == 0) return NULL;

  JSRT_Response *response = malloc(sizeof(JSRT_Response));
  if (!response) return NULL;

  memset(response, 0, sizeof(JSRT_Response));
  response->headers = JSRT_HeadersNew();

  // Find end of headers - try both \r\n\r\n and \n\n for compatibility
  const char *headers_end = strstr(response_data, "\r\n\r\n");
  if (!headers_end) {
    headers_end = strstr(response_data, "\n\n");
    if (!headers_end) {
      JSRT_HeadersFree(response->headers);
      free(response);
      return NULL;
    }
  }

  // Parse status line - more robust parsing
  int major, minor;
  char status_text_buffer[256] = {0};
  if (sscanf(response_data, "HTTP/%d.%d %d %255[^\r\n]", &major, &minor, &response->status, status_text_buffer) >= 3) {
    // Successfully parsed status line
    if (strlen(status_text_buffer) > 0) {
      response->status_text = strdup(status_text_buffer);
    } else {
      // Use default status text based on status code
      if (response->status >= 200 && response->status < 300) {
        response->status_text = strdup("OK");
      } else if (response->status >= 400 && response->status < 500) {
        response->status_text = strdup("Client Error");
      } else if (response->status >= 500) {
        response->status_text = strdup("Server Error");
      } else {
        response->status_text = strdup("Unknown");
      }
    }
  } else {
    // Fallback parsing
    response->status = 500;
    response->status_text = strdup("Parse Error");
  }

  response->ok = (response->status >= 200 && response->status < 300);

  // Parse headers - improved robustness
  const char *header_start = strchr(response_data, '\n');
  if (header_start) {
    header_start++;  // Skip status line
    while (header_start < headers_end) {
      const char *line_end = strchr(header_start, '\n');
      if (!line_end) break;

      // Handle \r\n line endings
      const char *actual_line_end = line_end;
      if (line_end > header_start && *(line_end - 1) == '\r') {
        actual_line_end = line_end - 1;
      }

      const char *colon = strchr(header_start, ':');
      if (colon && colon < actual_line_end) {
        // Calculate lengths
        size_t name_len = colon - header_start;
        size_t value_len = actual_line_end - colon - 1;

        // Skip whitespace after colon
        const char *value_start = colon + 1;
        while (value_start < actual_line_end && (*value_start == ' ' || *value_start == '\t')) {
          value_start++;
          value_len--;
        }

        if (name_len > 0 && value_len > 0) {
          char *name = malloc(name_len + 1);
          char *value = malloc(value_len + 1);
          if (name && value) {
            strncpy(name, header_start, name_len);
            name[name_len] = '\0';
            strncpy(value, value_start, value_len);
            value[value_len] = '\0';
            JSRT_HeadersSet(response->headers, name, value);
          }
          if (name) free(name);
          if (value) free(value);
        }
      }

      header_start = line_end + 1;
    }
  }

  // Body will be set later in the calling function
  response->body = JS_UNDEFINED;

  return response;
}

static void JSRT_FetchOnGetAddrInfo(uv_getaddrinfo_t *req, int status, struct addrinfo *res) {
  JSRT_FetchContext *ctx = (JSRT_FetchContext *)req->data;

  // Safety check - ensure runtime context is still valid
  if (!ctx || !ctx->rt || !ctx->rt->ctx) {
    if (ctx) JSRT_FetchContextFree(ctx);
    goto cleanup;
  }

  if (status != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "DNS resolution failed: %s", uv_strerror(status));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    JSRT_FetchContextFree(ctx);
    goto cleanup;
  }

  // Initialize TCP handle
  int ret = uv_tcp_init(ctx->rt->uv_loop, &ctx->tcp_handle);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "TCP initialization failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    JSRT_FetchContextFree(ctx);
    goto cleanup;
  }

  ctx->tcp_handle.data = ctx;
  ctx->connect_req.data = ctx;

  // Connect to the resolved address
  ret = uv_tcp_connect(&ctx->connect_req, &ctx->tcp_handle, (const struct sockaddr *)res->ai_addr, JSRT_FetchOnConnect);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Connection failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    JSRT_FetchContextFree(ctx);
  }

cleanup:
  if (res) uv_freeaddrinfo(res);
  free(req);
}

static void JSRT_FetchOnConnect(uv_connect_t *req, int status) {
  JSRT_FetchContext *ctx = (JSRT_FetchContext *)req->data;

  // Safety check - ensure runtime context is still valid
  if (!ctx || !ctx->rt || !ctx->rt->ctx) {
    if (ctx) JSRT_FetchContextFree(ctx);
    return;
  }

  if (status != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Connection failed: %s", uv_strerror(status));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    JSRT_FetchContextFree(ctx);
    return;
  }

  // Start reading responses
  int ret = uv_read_start((uv_stream_t *)&ctx->tcp_handle, JSRT_FetchAllocBuffer, JSRT_FetchOnRead);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Read start failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    JSRT_FetchContextFree(ctx);
    return;
  }

  // Build and send HTTP request
  char *http_request = JSRT_FetchBuildHttpRequest(ctx->method, ctx->path, ctx->host, ctx->port, ctx->request_headers);
  if (!http_request) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "Failed to build HTTP request"));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    JSRT_FetchContextFree(ctx);
    return;
  }

  uv_buf_t write_buf = uv_buf_init(http_request, strlen(http_request));
  ctx->write_req.data = http_request;  // Store pointer to free later

  ret = uv_write(&ctx->write_req, (uv_stream_t *)&ctx->tcp_handle, &write_buf, 1, JSRT_FetchOnWrite);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Write failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    free(http_request);
    JSRT_FetchContextFree(ctx);
  }
}

static void JSRT_FetchOnWrite(uv_write_t *req, int status) {
  char *http_request = (char *)req->data;
  free(http_request);  // Free the request buffer

  if (status != 0) {
    JSRT_FetchContext *ctx = (JSRT_FetchContext *)req->handle->data;

    // Safety check - ensure runtime context is still valid
    if (!ctx || !ctx->rt || !ctx->rt->ctx) {
      if (ctx) JSRT_FetchContextFree(ctx);
      return;
    }

    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Write failed: %s", uv_strerror(status));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    JSRT_FetchContextFree(ctx);
  }
  // Request sent successfully, now wait for response data in JSRT_FetchOnRead
}

static void JSRT_FetchAllocBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

static void JSRT_FetchOnRead(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
  JSRT_FetchContext *ctx = (JSRT_FetchContext *)stream->data;

  // Safety check - ensure runtime context is still valid
  if (!ctx || !ctx->rt || !ctx->rt->ctx) {
    if (ctx) {
      uv_close((uv_handle_t *)&ctx->tcp_handle, JSRT_FetchOnClose);
    }
    if (buf->base) free(buf->base);
    return;
  }

  if (nread < 0) {
    if (nread != UV_EOF) {
      JSValue error = JS_NewError(ctx->rt->ctx);
      char error_msg[256];
      snprintf(error_msg, sizeof(error_msg), "Read error: %s", uv_strerror(nread));
      JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
      JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx->rt->ctx, error);
    } else {
      // EOF - process response
      if (ctx->response_buffer && ctx->response_size > 0) {
        JSRT_Response *response = JSRT_FetchParseHttpResponse(ctx->response_buffer, ctx->response_size);
        if (response) {
          // Set the response body from the buffer
          const char *body_start = strstr(ctx->response_buffer, "\r\n\r\n");
          if (body_start) {
            body_start += 4;
            response->body = JS_NewString(ctx->rt->ctx, body_start);
          } else {
            response->body = JS_NewString(ctx->rt->ctx, "");
          }

          JSValue response_obj = JS_NewObjectClass(ctx->rt->ctx, JSRT_ResponseClassID);
          JS_SetOpaque(response_obj, response);

          // Resolve the promise with the response
          JS_Call(ctx->rt->ctx, ctx->resolve_func, JS_UNDEFINED, 1, &response_obj);
          JS_FreeValue(ctx->rt->ctx, response_obj);
        } else {
          JSValue error = JS_NewError(ctx->rt->ctx);
          JS_SetPropertyStr(ctx->rt->ctx, error, "message",
                            JS_NewString(ctx->rt->ctx, "Failed to parse HTTP response"));
          JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
          JS_FreeValue(ctx->rt->ctx, error);
        }
      } else {
        JSValue error = JS_NewError(ctx->rt->ctx);
        JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "Empty response"));
        JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
        JS_FreeValue(ctx->rt->ctx, error);
      }
    }

    uv_close((uv_handle_t *)&ctx->tcp_handle, JSRT_FetchOnClose);
    if (buf->base) free(buf->base);
    return;
  }

  if (nread > 0) {
    // Expand response buffer if needed
    if (ctx->response_size + nread >= ctx->response_capacity) {
      ctx->response_capacity = ctx->response_size + nread + 1024;
      ctx->response_buffer = realloc(ctx->response_buffer, ctx->response_capacity);
      if (!ctx->response_buffer) {
        JSValue error = JS_NewError(ctx->rt->ctx);
        JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "Out of memory"));
        JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
        JS_FreeValue(ctx->rt->ctx, error);
        uv_close((uv_handle_t *)&ctx->tcp_handle, JSRT_FetchOnClose);
        if (buf->base) free(buf->base);
        return;
      }
    }

    // Append new data
    memcpy(ctx->response_buffer + ctx->response_size, buf->base, nread);
    ctx->response_size += nread;
    ctx->response_buffer[ctx->response_size] = '\0';  // Null terminate for string operations
  }

  if (buf->base) free(buf->base);
}

// Close callback - safely free the context after handle is closed
static void JSRT_FetchOnClose(uv_handle_t *handle) {
  JSRT_FetchContext *ctx = (JSRT_FetchContext *)handle->data;
  if (ctx) {
    JSRT_FetchContextFree(ctx);
  }
}

static JSRT_Headers *JSRT_HeadersNew() {
  JSRT_Headers *headers = malloc(sizeof(JSRT_Headers));
  headers->first = NULL;
  return headers;
}

static void JSRT_HeadersFree(JSRT_Headers *headers) {
  if (!headers) return;

  JSRT_HeaderItem *current = headers->first;
  while (current) {
    JSRT_HeaderItem *next = current->next;
    free(current->name);
    free(current->value);
    free(current);
    current = next;
  }
  free(headers);
}

static void JSRT_HeadersSet(JSRT_Headers *headers, const char *name, const char *value) {
  if (!headers || !name || !value) return;

  // Convert name to lowercase
  char *lower_name = malloc(strlen(name) + 1);
  for (size_t i = 0; name[i]; i++) {
    lower_name[i] = tolower(name[i]);
  }
  lower_name[strlen(name)] = '\0';

  // Check if header already exists
  JSRT_HeaderItem *item = headers->first;
  while (item) {
    if (strcmp(item->name, lower_name) == 0) {
      free(item->value);
      item->value = strdup(value);
      free(lower_name);
      return;
    }
    item = item->next;
  }

  // Add new header
  JSRT_HeaderItem *new_item = malloc(sizeof(JSRT_HeaderItem));
  new_item->name = lower_name;
  new_item->value = strdup(value);
  new_item->next = headers->first;
  headers->first = new_item;
}

static const char *JSRT_HeadersGet(JSRT_Headers *headers, const char *name) {
  if (!headers || !name) return NULL;

  // Convert name to lowercase
  char *lower_name = malloc(strlen(name) + 1);
  for (size_t i = 0; name[i]; i++) {
    lower_name[i] = tolower(name[i]);
  }
  lower_name[strlen(name)] = '\0';

  JSRT_HeaderItem *item = headers->first;
  while (item) {
    if (strcmp(item->name, lower_name) == 0) {
      free(lower_name);
      return item->value;
    }
    item = item->next;
  }

  free(lower_name);
  return NULL;
}

static bool JSRT_HeadersHas(JSRT_Headers *headers, const char *name) { return JSRT_HeadersGet(headers, name) != NULL; }

static void JSRT_HeadersDelete(JSRT_Headers *headers, const char *name) {
  if (!headers || !name) return;

  // Convert name to lowercase
  char *lower_name = malloc(strlen(name) + 1);
  for (size_t i = 0; name[i]; i++) {
    lower_name[i] = tolower(name[i]);
  }
  lower_name[strlen(name)] = '\0';

  JSRT_HeaderItem **current = &headers->first;
  while (*current) {
    if (strcmp((*current)->name, lower_name) == 0) {
      JSRT_HeaderItem *to_delete = *current;
      *current = to_delete->next;
      free(to_delete->name);
      free(to_delete->value);
      free(to_delete);
      break;
    }
    current = &(*current)->next;
  }

  free(lower_name);
}

// Setup function
void JSRT_RuntimeSetupStdFetch(JSRT_Runtime *rt) {
  // Register Headers class
  JS_NewClassID(&JSRT_HeadersClassID);
  JS_NewClass(rt->rt, JSRT_HeadersClassID, &JSRT_HeadersClass);

  JSValue headers_proto = JS_NewObject(rt->ctx);
  JS_SetPropertyStr(rt->ctx, headers_proto, "get", JS_NewCFunction(rt->ctx, JSRT_HeadersGetMethod, "get", 1));
  JS_SetPropertyStr(rt->ctx, headers_proto, "set", JS_NewCFunction(rt->ctx, JSRT_HeadersSetMethod, "set", 2));
  JS_SetPropertyStr(rt->ctx, headers_proto, "has", JS_NewCFunction(rt->ctx, JSRT_HeadersHasMethod, "has", 1));
  JS_SetPropertyStr(rt->ctx, headers_proto, "delete", JS_NewCFunction(rt->ctx, JSRT_HeadersDeleteMethod, "delete", 1));

  JS_SetClassProto(rt->ctx, JSRT_HeadersClassID, headers_proto);

  JSValue headers_ctor = JS_NewCFunction2(rt->ctx, JSRT_HeadersConstructor, "Headers", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(rt->ctx, rt->global, "Headers", headers_ctor);

  // Register Request class
  JS_NewClassID(&JSRT_RequestClassID);
  JS_NewClass(rt->rt, JSRT_RequestClassID, &JSRT_RequestClass);

  JSValue request_proto = JS_NewObject(rt->ctx);
  JS_DefinePropertyGetSet(rt->ctx, request_proto, JS_NewAtom(rt->ctx, "method"),
                          JS_NewCFunction(rt->ctx, JSRT_RequestGetMethod, "get method", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(rt->ctx, request_proto, JS_NewAtom(rt->ctx, "url"),
                          JS_NewCFunction(rt->ctx, JSRT_RequestGetUrl, "get url", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  JS_SetClassProto(rt->ctx, JSRT_RequestClassID, request_proto);

  JSValue request_ctor = JS_NewCFunction2(rt->ctx, JSRT_RequestConstructor, "Request", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(rt->ctx, rt->global, "Request", request_ctor);

  // Register Response class
  JS_NewClassID(&JSRT_ResponseClassID);
  JS_NewClass(rt->rt, JSRT_ResponseClassID, &JSRT_ResponseClass);

  JSValue response_proto = JS_NewObject(rt->ctx);
  JS_DefinePropertyGetSet(rt->ctx, response_proto, JS_NewAtom(rt->ctx, "status"),
                          JS_NewCFunction(rt->ctx, JSRT_ResponseGetStatus, "get status", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_DefinePropertyGetSet(rt->ctx, response_proto, JS_NewAtom(rt->ctx, "ok"),
                          JS_NewCFunction(rt->ctx, JSRT_ResponseGetOk, "get ok", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);

  // Add response body methods
  JS_SetPropertyStr(rt->ctx, response_proto, "text", JS_NewCFunction(rt->ctx, JSRT_ResponseText, "text", 0));
  JS_SetPropertyStr(rt->ctx, response_proto, "json", JS_NewCFunction(rt->ctx, JSRT_ResponseJson, "json", 0));

  JS_SetClassProto(rt->ctx, JSRT_ResponseClassID, response_proto);

  JSValue response_ctor = JS_NewCFunction2(rt->ctx, JSRT_ResponseConstructor, "Response", 0, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(rt->ctx, rt->global, "Response", response_ctor);

  // Register fetch function
  JS_SetPropertyStr(rt->ctx, rt->global, "fetch", JS_NewCFunction(rt->ctx, JSRT_Fetch, "fetch", 1));

  JSRT_Debug("Fetch API setup completed");
}