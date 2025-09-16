#include "fetch.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../crypto/crypto.h"
#include "../util/debug.h"
#include "../util/jsutils.h"
#include "../util/ssl_client.h"
#include "../util/url_parser.h"
#include "../util/user_agent.h"
#include "parser.h"

typedef struct jsrt_header_entry {
  char* name;
  char* value;
  struct jsrt_header_entry* next;
} jsrt_header_entry_t;

typedef struct {
  JSRT_Runtime* rt;
  uv_tcp_t tcp_handle;
  uv_connect_t connect_req;
  uv_write_t write_req;
  uv_getaddrinfo_t dns_req;

  char* host;
  int port;
  char* path;
  char* method;
  char* body;
  size_t body_len;
  int is_https;

  jsrt_header_entry_t* headers;

  jsrt_http_parser_t* parser;

  char* request_buffer;
  size_t request_len;

  char* response_buffer;
  size_t response_size;
  size_t response_capacity;

  JSValue resolve_func;
  JSValue reject_func;

  int connection_closed;

  // SSL/TLS support
  jsrt_ssl_client_t* ssl_client;
} jsrt_fetch_context_t;

static int parse_url(const char* url, char** host, int* port, char** path, int* is_https) {
  jsrt_url_t parsed;

  if (jsrt_url_parse(url, &parsed) != 0) {
    return -1;
  }

  // Copy parsed components
  *host = strdup(parsed.host);
  *port = parsed.port;
  *path = strdup(parsed.path);
  *is_https = parsed.is_secure ? 1 : 0;

  // Check for allocation failures
  if (!*host || !*path) {
    free(*host);
    free(*path);
    jsrt_url_free(&parsed);
    return -1;
  }

  jsrt_url_free(&parsed);
  return 0;
}

static char* build_http_request(const char* method, const char* path, const char* host, int port, const char* body,
                                size_t body_len, jsrt_header_entry_t* headers) {
  size_t request_size = strlen(method) + strlen(path) + strlen(host) + body_len + 1024;

  // Calculate additional space needed for custom headers
  jsrt_header_entry_t* header = headers;
  while (header) {
    request_size += strlen(header->name) + strlen(header->value) + 4;  // ": \r\n"
    header = header->next;
  }

  char* request = malloc(request_size);
  if (!request)
    return NULL;

  int len = snprintf(request, request_size,
                     "%s %s HTTP/1.1\r\n"
                     "Host: %s:%d\r\n",
                     method, path, host, port);

  // Add custom headers, with defaults if not specified
  int has_user_agent = 0;
  int has_content_type = 0;

  header = headers;
  while (header) {
    len += snprintf(request + len, request_size - len, "%s: %s\r\n", header->name, header->value);

    if (strcasecmp(header->name, "User-Agent") == 0)
      has_user_agent = 1;
    if (strcasecmp(header->name, "Content-Type") == 0)
      has_content_type = 1;

    header = header->next;
  }

  // Add default headers if not provided
  if (!has_user_agent) {
    const char* user_agent = jsrt_get_static_user_agent();
    len += snprintf(request + len, request_size - len, "User-Agent: %s\r\n", user_agent);
  }

  len += snprintf(request + len, request_size - len, "Connection: close\r\n");

  if (body && body_len > 0) {
    len += snprintf(request + len, request_size - len, "Content-Length: %zu\r\n", body_len);
  }

  len += snprintf(request + len, request_size - len, "\r\n");

  if (body && body_len > 0) {
    if (len + body_len < request_size) {
      memcpy(request + len, body, body_len);
      len += body_len;
    }
  }

  return request;
}

static void free_header_list(jsrt_header_entry_t* headers) {
  while (headers) {
    jsrt_header_entry_t* next = headers->next;
    free(headers->name);
    free(headers->value);
    free(headers);
    headers = next;
  }
}

static void fetch_context_free(jsrt_fetch_context_t* ctx) {
  if (!ctx)
    return;

  // Clean up SSL resources
  if (ctx->ssl_client) {
    jsrt_ssl_client_free(ctx->ssl_client);
    ctx->ssl_client = NULL;
  }

  free(ctx->host);
  free(ctx->path);
  free(ctx->method);
  free(ctx->body);
  free(ctx->request_buffer);
  free(ctx->response_buffer);

  if (ctx->headers) {
    free_header_list(ctx->headers);
  }

  if (ctx->parser) {
    jsrt_http_parser_destroy(ctx->parser);
  }

  if (ctx->rt && ctx->rt->ctx) {
    if (!JS_IsUndefined(ctx->resolve_func)) {
      JS_FreeValue(ctx->rt->ctx, ctx->resolve_func);
    }
    if (!JS_IsUndefined(ctx->reject_func)) {
      JS_FreeValue(ctx->rt->ctx, ctx->reject_func);
    }
  }

  free(ctx);
}

static void on_close(uv_handle_t* handle) {
  jsrt_fetch_context_t* ctx = (jsrt_fetch_context_t*)handle->data;
  if (ctx) {
    fetch_context_free(ctx);
  }
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  jsrt_fetch_context_t* ctx = (jsrt_fetch_context_t*)stream->data;

  if (!ctx || !ctx->rt || !ctx->rt->ctx) {
    if (buf->base)
      free(buf->base);
    return;
  }

  if (nread < 0) {
    if (nread == UV_EOF) {
      // Force complete parsing on EOF
      if (ctx->parser && ctx->parser->current_message) {
        // Try to finish parsing with empty data to trigger completion
        jsrt_http_parser_execute(ctx->parser, "", 0);

        if (ctx->parser->current_message->status_code > 0) {
          // We have a valid response, mark as complete if not already
          if (!ctx->parser->current_message->complete) {
            ctx->parser->current_message->complete = 1;
          }
          JSValue response_obj = jsrt_http_message_to_js(ctx->rt->ctx, ctx->parser->current_message);
          JS_Call(ctx->rt->ctx, ctx->resolve_func, JS_UNDEFINED, 1, &response_obj);
          JS_FreeValue(ctx->rt->ctx, response_obj);
        } else {
          JSValue error = JS_NewError(ctx->rt->ctx);
          JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "Incomplete HTTP response"));
          JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
          JS_FreeValue(ctx->rt->ctx, error);
        }
      } else {
        JSValue error = JS_NewError(ctx->rt->ctx);
        JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "No HTTP response received"));
        JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
        JS_FreeValue(ctx->rt->ctx, error);
      }
    } else {
      JSValue error = JS_NewError(ctx->rt->ctx);
      char error_msg[256];
      snprintf(error_msg, sizeof(error_msg), "Read error: %s", uv_strerror(nread));
      JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
      JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx->rt->ctx, error);
    }

    uv_close((uv_handle_t*)&ctx->tcp_handle, on_close);
    if (buf->base)
      free(buf->base);
    return;
  }

  if (nread > 0) {
    jsrt_http_error_t result = jsrt_http_parser_execute(ctx->parser, buf->base, nread);

    if (result != JSRT_HTTP_OK && result != JSRT_HTTP_ERROR_INCOMPLETE) {
      JSValue error = JS_NewError(ctx->rt->ctx);
      JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "HTTP parsing error"));
      JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx->rt->ctx, error);
      uv_close((uv_handle_t*)&ctx->tcp_handle, on_close);
    } else if (ctx->parser->current_message && ctx->parser->current_message->complete) {
      JSValue response_obj = jsrt_http_message_to_js(ctx->rt->ctx, ctx->parser->current_message);
      JS_Call(ctx->rt->ctx, ctx->resolve_func, JS_UNDEFINED, 1, &response_obj);
      JS_FreeValue(ctx->rt->ctx, response_obj);
      uv_close((uv_handle_t*)&ctx->tcp_handle, on_close);
    }
  }

  if (buf->base)
    free(buf->base);
}

static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

static void on_write(uv_write_t* req, int status) {
  if (status != 0) {
    jsrt_fetch_context_t* ctx = (jsrt_fetch_context_t*)req->handle->data;
    if (ctx && ctx->rt && ctx->rt->ctx) {
      JSValue error = JS_NewError(ctx->rt->ctx);
      char error_msg[256];
      snprintf(error_msg, sizeof(error_msg), "Write failed: %s", uv_strerror(status));
      JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
      JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx->rt->ctx, error);
      uv_close((uv_handle_t*)&ctx->tcp_handle, on_close);
    }
  }
}

static void on_connect(uv_connect_t* req, int status) {
  jsrt_fetch_context_t* ctx = (jsrt_fetch_context_t*)req->data;

  if (!ctx || !ctx->rt || !ctx->rt->ctx) {
    if (ctx)
      fetch_context_free(ctx);
    return;
  }

  if (status != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Connection failed: %s", uv_strerror(status));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    fetch_context_free(ctx);
    return;
  }

  // Handle SSL/TLS for HTTPS connections
  if (ctx->is_https) {
    // Initialize SSL global functions first
    if (!jsrt_ssl_global_init()) {
      JSValue error = JS_NewError(ctx->rt->ctx);
      JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "SSL/TLS functions not available"));
      JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx->rt->ctx, error);
      fetch_context_free(ctx);
      return;
    }

    // Create SSL client
    ctx->ssl_client = jsrt_ssl_client_new();
    if (!ctx->ssl_client) {
      JSValue error = JS_NewError(ctx->rt->ctx);
      JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "Failed to create SSL client"));
      JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx->rt->ctx, error);
      fetch_context_free(ctx);
      return;
    }

    // Get the file descriptor from the TCP handle
    int fd;
    int ret = uv_fileno((uv_handle_t*)&ctx->tcp_handle, (uv_os_fd_t*)&fd);
    if (ret != 0) {
      JSValue error = JS_NewError(ctx->rt->ctx);
      char error_msg[256];
      snprintf(error_msg, sizeof(error_msg), "Failed to get socket descriptor: %s", uv_strerror(ret));
      JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
      JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx->rt->ctx, error);
      fetch_context_free(ctx);
      return;
    }

    // Setup SSL client for this connection
    if (jsrt_ssl_client_setup(ctx->ssl_client, fd, ctx->host) != 0) {
      JSValue error = JS_NewError(ctx->rt->ctx);
      JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "Failed to setup SSL client"));
      JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx->rt->ctx, error);
      fetch_context_free(ctx);
      return;
    }

    // Perform SSL handshake
    int handshake_ret = jsrt_ssl_client_handshake(ctx->ssl_client);
    if (handshake_ret != 1) {
      JSValue error = JS_NewError(ctx->rt->ctx);
      JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "SSL handshake failed"));
      JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
      JS_FreeValue(ctx->rt->ctx, error);
      fetch_context_free(ctx);
      return;
    }

    JSRT_Debug("JSRT_Fetch: SSL handshake successful for %s:%d", ctx->host, ctx->port);
  }

  int ret = uv_read_start((uv_stream_t*)&ctx->tcp_handle, alloc_buffer, on_read);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Read start failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    fetch_context_free(ctx);
    return;
  }

  ctx->request_buffer =
      build_http_request(ctx->method, ctx->path, ctx->host, ctx->port, ctx->body, ctx->body_len, ctx->headers);
  if (!ctx->request_buffer) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, "Failed to build HTTP request"));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    fetch_context_free(ctx);
    return;
  }

  ctx->request_len = strlen(ctx->request_buffer);
  uv_buf_t write_buf = uv_buf_init(ctx->request_buffer, ctx->request_len);

  ret = uv_write(&ctx->write_req, (uv_stream_t*)&ctx->tcp_handle, &write_buf, 1, on_write);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "Write failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    fetch_context_free(ctx);
  }
}

static void on_resolve(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  jsrt_fetch_context_t* ctx = (jsrt_fetch_context_t*)req->data;

  if (!ctx || !ctx->rt || !ctx->rt->ctx) {
    if (ctx)
      fetch_context_free(ctx);
    if (res)
      uv_freeaddrinfo(res);
    return;
  }

  if (status != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "DNS resolution failed: %s", uv_strerror(status));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    fetch_context_free(ctx);
    if (res)
      uv_freeaddrinfo(res);
    return;
  }

  int ret = uv_tcp_init(ctx->rt->uv_loop, &ctx->tcp_handle);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "TCP initialization failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    fetch_context_free(ctx);
    if (res)
      uv_freeaddrinfo(res);
    return;
  }

  ctx->tcp_handle.data = ctx;
  ctx->connect_req.data = ctx;

  ret = uv_tcp_connect(&ctx->connect_req, &ctx->tcp_handle, (const struct sockaddr*)res->ai_addr, on_connect);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx->rt->ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "TCP connect failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx->rt->ctx, error, "message", JS_NewString(ctx->rt->ctx, error_msg));
    JS_Call(ctx->rt->ctx, ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx->rt->ctx, error);
    fetch_context_free(ctx);
  }

  if (res)
    uv_freeaddrinfo(res);
}

// Headers class implementation
static JSValue headers_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_UNDEFINED;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_UNDEFINED;
  }

  // Convert name to lowercase for case-insensitive lookup
  size_t name_len = strlen(name);
  char* lower_name = malloc(name_len + 1);
  for (size_t i = 0; i < name_len; i++) {
    lower_name[i] = (char)tolower((unsigned char)name[i]);
  }
  lower_name[name_len] = '\0';

  JSValue headers_map = JS_GetPropertyStr(ctx, this_val, "_headers");
  JSValue result = JS_GetPropertyStr(ctx, headers_map, lower_name);

  JS_FreeValue(ctx, headers_map);
  JS_FreeCString(ctx, name);
  free(lower_name);

  return JS_IsUndefined(result) ? JS_NULL : result;
}

static JSValue headers_set(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 2) {
    return JS_UNDEFINED;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_UNDEFINED;
  }

  // Convert name to lowercase for case-insensitive storage
  size_t name_len = strlen(name);
  char* lower_name = malloc(name_len + 1);
  for (size_t i = 0; i < name_len; i++) {
    lower_name[i] = (char)tolower((unsigned char)name[i]);
  }
  lower_name[name_len] = '\0';

  JSValue headers_map = JS_GetPropertyStr(ctx, this_val, "_headers");
  JS_SetPropertyStr(ctx, headers_map, lower_name, JS_DupValue(ctx, argv[1]));

  JS_FreeValue(ctx, headers_map);
  JS_FreeCString(ctx, name);
  free(lower_name);

  return JS_UNDEFINED;
}

static JSValue headers_has(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_FALSE;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_FALSE;
  }

  // Convert name to lowercase for case-insensitive lookup
  size_t name_len = strlen(name);
  char* lower_name = malloc(name_len + 1);
  for (size_t i = 0; i < name_len; i++) {
    lower_name[i] = (char)tolower((unsigned char)name[i]);
  }
  lower_name[name_len] = '\0';

  JSValue headers_map = JS_GetPropertyStr(ctx, this_val, "_headers");
  JSValue value = JS_GetPropertyStr(ctx, headers_map, lower_name);
  JSValue result = JS_NewBool(ctx, !JS_IsUndefined(value));

  JS_FreeValue(ctx, headers_map);
  JS_FreeValue(ctx, value);
  JS_FreeCString(ctx, name);
  free(lower_name);

  return result;
}

static JSValue headers_delete(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_UNDEFINED;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_UNDEFINED;
  }

  // Convert name to lowercase for case-insensitive deletion
  size_t name_len = strlen(name);
  char* lower_name = malloc(name_len + 1);
  for (size_t i = 0; i < name_len; i++) {
    lower_name[i] = (char)tolower((unsigned char)name[i]);
  }
  lower_name[name_len] = '\0';

  JSValue headers_map = JS_GetPropertyStr(ctx, this_val, "_headers");
  JSAtom atom = JS_NewAtom(ctx, lower_name);
  JS_DeleteProperty(ctx, headers_map, atom, 0);
  JS_FreeAtom(ctx, atom);

  JS_FreeValue(ctx, headers_map);
  JS_FreeCString(ctx, name);
  free(lower_name);

  return JS_UNDEFINED;
}

// Response methods for Response constructor
static JSValue response_text_stub(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue body = JS_GetPropertyStr(ctx, this_val, "_body");
  if (!JS_IsString(body)) {
    JS_FreeValue(ctx, body);
    body = JS_NewString(ctx, "");
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeValue(ctx, body);
    return promise;
  }

  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &body);
  JS_FreeValue(ctx, body);
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);

  return promise;
}

static JSValue response_json_stub(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue body = JS_GetPropertyStr(ctx, this_val, "_body");

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeValue(ctx, body);
    return promise;
  }

  if (JS_IsString(body)) {
    const char* json_str = JS_ToCString(ctx, body);
    if (json_str) {
      JSValue parsed = JS_ParseJSON(ctx, json_str, strlen(json_str), "<response>");
      if (JS_IsException(parsed)) {
        JSValue error = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid JSON"));
        JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
        JS_FreeValue(ctx, error);
        JS_FreeValue(ctx, parsed);
      } else {
        JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &parsed);
        JS_FreeValue(ctx, parsed);
      }
      JS_FreeCString(ctx, json_str);
    }
  } else {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "No response body"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
  }

  JS_FreeValue(ctx, body);
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);

  return promise;
}

// Request class implementation
static JSValue request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  if (JS_IsUndefined(new_target)) {
    return JS_ThrowTypeError(ctx, "Request constructor must be called with 'new'");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Request constructor requires a URL argument");
  }

  JSValue obj = JS_NewObject(ctx);
  if (JS_IsException(obj)) {
    return obj;
  }

  // Get URL
  const char* url = JS_ToCString(ctx, argv[0]);
  if (!url) {
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
  }
  JS_SetPropertyStr(ctx, obj, "url", JS_NewString(ctx, url));
  JS_FreeCString(ctx, url);

  // Default method is GET
  const char* method = "GET";

  // Parse options if provided
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue method_val = JS_GetPropertyStr(ctx, argv[1], "method");
    if (JS_IsString(method_val)) {
      const char* method_str = JS_ToCString(ctx, method_val);
      if (method_str) {
        JS_SetPropertyStr(ctx, obj, "method", JS_NewString(ctx, method_str));
        JS_FreeCString(ctx, method_str);
      } else {
        JS_SetPropertyStr(ctx, obj, "method", JS_NewString(ctx, method));
      }
    } else {
      JS_SetPropertyStr(ctx, obj, "method", JS_NewString(ctx, method));
    }
    JS_FreeValue(ctx, method_val);
  } else {
    JS_SetPropertyStr(ctx, obj, "method", JS_NewString(ctx, method));
  }

  return obj;
}

// Response class implementation
static JSValue response_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  if (JS_IsUndefined(new_target)) {
    return JS_ThrowTypeError(ctx, "Response constructor must be called with 'new'");
  }

  JSValue obj = JS_NewObject(ctx);
  if (JS_IsException(obj)) {
    return obj;
  }

  // Default values
  JS_SetPropertyStr(ctx, obj, "status", JS_NewInt32(ctx, 200));
  JS_SetPropertyStr(ctx, obj, "ok", JS_TRUE);
  JS_SetPropertyStr(ctx, obj, "statusText", JS_NewString(ctx, "OK"));
  JS_SetPropertyStr(ctx, obj, "_body", JS_NewString(ctx, ""));

  // Add methods
  JS_SetPropertyStr(ctx, obj, "text", JS_NewCFunction(ctx, response_text_stub, "text", 0));
  JS_SetPropertyStr(ctx, obj, "json", JS_NewCFunction(ctx, response_json_stub, "json", 0));

  // Add stub methods for arrayBuffer and blob
  JS_SetPropertyStr(ctx, obj, "arrayBuffer", JS_NewCFunction(ctx, response_text_stub, "arrayBuffer", 0));
  JS_SetPropertyStr(ctx, obj, "blob", JS_NewCFunction(ctx, response_text_stub, "blob", 0));

  return obj;
}

static JSValue headers_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj;

  if (JS_IsUndefined(new_target)) {
    // Called as function, not constructor
    return JS_ThrowTypeError(ctx, "Headers constructor must be called with 'new'");
  }

  obj = JS_NewObject(ctx);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSValue headers_map = JS_NewObject(ctx);

  // Initialize with provided headers if any
  if (argc > 0 && JS_IsObject(argv[0])) {
    // Copy properties from the input object
    JSPropertyEnum* tab;
    uint32_t len;
    if (JS_GetOwnPropertyNames(ctx, &tab, &len, argv[0], JS_GPN_STRING_MASK) == 0) {
      for (uint32_t i = 0; i < len; i++) {
        JSValue key = JS_AtomToString(ctx, tab[i].atom);
        JSValue value = JS_GetProperty(ctx, argv[0], tab[i].atom);
        const char* key_str = JS_ToCString(ctx, key);
        if (key_str) {
          // Convert key to lowercase for case-insensitive storage
          size_t key_len = strlen(key_str);
          char* lower_key = malloc(key_len + 1);
          for (size_t j = 0; j < key_len; j++) {
            lower_key[j] = (char)tolower((unsigned char)key_str[j]);
          }
          lower_key[key_len] = '\0';

          JS_SetPropertyStr(ctx, headers_map, lower_key, JS_DupValue(ctx, value));
          JS_FreeCString(ctx, key_str);
          free(lower_key);
        }
        JS_FreeValue(ctx, key);
        JS_FreeValue(ctx, value);
        JS_FreeAtom(ctx, tab[i].atom);
      }
      js_free(ctx, tab);
    }
  }

  JS_SetPropertyStr(ctx, obj, "_headers", headers_map);

  // Add methods
  JS_SetPropertyStr(ctx, obj, "get", JS_NewCFunction(ctx, headers_get, "get", 1));
  JS_SetPropertyStr(ctx, obj, "set", JS_NewCFunction(ctx, headers_set, "set", 2));
  JS_SetPropertyStr(ctx, obj, "has", JS_NewCFunction(ctx, headers_has, "has", 1));
  JS_SetPropertyStr(ctx, obj, "delete", JS_NewCFunction(ctx, headers_delete, "delete", 1));

  return obj;
}

JSValue jsrt_fetch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "fetch requires at least 1 argument");
  }

  const char* url = JS_ToCString(ctx, argv[0]);
  if (!url) {
    return JS_EXCEPTION;
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeCString(ctx, url);
    return JS_EXCEPTION;
  }

  jsrt_fetch_context_t* fetch_ctx = malloc(sizeof(jsrt_fetch_context_t));
  if (!fetch_ctx) {
    JS_FreeCString(ctx, url);
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return JS_ThrowOutOfMemory(ctx);
  }

  memset(fetch_ctx, 0, sizeof(jsrt_fetch_context_t));
  fetch_ctx->rt = JS_GetContextOpaque(ctx);
  fetch_ctx->resolve_func = JS_DupValue(ctx, resolving_funcs[0]);
  fetch_ctx->reject_func = JS_DupValue(ctx, resolving_funcs[1]);
  fetch_ctx->method = strdup("GET");

  if (parse_url(url, &fetch_ctx->host, &fetch_ctx->port, &fetch_ctx->path, &fetch_ctx->is_https) != 0) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid URL"));
    JS_Call(ctx, fetch_ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    fetch_context_free(fetch_ctx);
    goto cleanup;
  }

  // Check for HTTPS support if needed
  if (fetch_ctx->is_https && !openssl_handle) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message",
                      JS_NewString(ctx,
                                   "HTTPS not supported: OpenSSL library not found. "
                                   "Please install OpenSSL to use HTTPS URLs. "
                                   "当前不支持 HTTPS：未找到 OpenSSL 库。请安装 OpenSSL 以使用 HTTPS URL。"));
    JS_Call(ctx, fetch_ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    fetch_context_free(fetch_ctx);
    goto cleanup;
  }

  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue method_val = JS_GetPropertyStr(ctx, argv[1], "method");
    if (JS_IsString(method_val)) {
      const char* method_str = JS_ToCString(ctx, method_val);
      if (method_str) {
        free(fetch_ctx->method);
        fetch_ctx->method = strdup(method_str);
        JS_FreeCString(ctx, method_str);
      }
    }
    JS_FreeValue(ctx, method_val);

    JSValue body_val = JS_GetPropertyStr(ctx, argv[1], "body");
    if (JS_IsString(body_val)) {
      size_t body_len;
      const char* body_str = JS_ToCStringLen(ctx, &body_len, body_val);
      if (body_str) {
        fetch_ctx->body = malloc(body_len + 1);
        memcpy(fetch_ctx->body, body_str, body_len);
        fetch_ctx->body[body_len] = '\0';
        fetch_ctx->body_len = body_len;
        JS_FreeCString(ctx, body_str);
      }
    }
    JS_FreeValue(ctx, body_val);

    // Parse headers
    JSValue headers_val = JS_GetPropertyStr(ctx, argv[1], "headers");
    if (JS_IsObject(headers_val)) {
      JSPropertyEnum* tab;
      uint32_t len;
      if (JS_GetOwnPropertyNames(ctx, &tab, &len, headers_val, JS_GPN_STRING_MASK) == 0) {
        jsrt_header_entry_t** current_header = &fetch_ctx->headers;

        for (uint32_t i = 0; i < len; i++) {
          JSValue key = JS_AtomToString(ctx, tab[i].atom);
          JSValue value = JS_GetProperty(ctx, headers_val, tab[i].atom);

          const char* key_str = JS_ToCString(ctx, key);
          const char* value_str = JS_ToCString(ctx, value);

          if (key_str && value_str) {
            jsrt_header_entry_t* entry = malloc(sizeof(jsrt_header_entry_t));
            if (entry) {
              entry->name = strdup(key_str);
              entry->value = strdup(value_str);
              entry->next = NULL;
              *current_header = entry;
              current_header = &entry->next;
            }
          }

          if (key_str)
            JS_FreeCString(ctx, key_str);
          if (value_str)
            JS_FreeCString(ctx, value_str);

          JS_FreeValue(ctx, key);
          JS_FreeValue(ctx, value);
          JS_FreeAtom(ctx, tab[i].atom);
        }
        js_free(ctx, tab);
      }
    }
    JS_FreeValue(ctx, headers_val);
  }

  fetch_ctx->parser = jsrt_http_parser_create(ctx, JSRT_HTTP_RESPONSE);
  if (!fetch_ctx->parser) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Failed to create HTTP parser"));
    JS_Call(ctx, fetch_ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    fetch_context_free(fetch_ctx);
    goto cleanup;
  }

  fetch_ctx->dns_req.data = fetch_ctx;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", fetch_ctx->port);

  int ret = uv_getaddrinfo(fetch_ctx->rt->uv_loop, &fetch_ctx->dns_req, on_resolve, fetch_ctx->host, port_str, &hints);
  if (ret != 0) {
    JSValue error = JS_NewError(ctx);
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg), "DNS resolution start failed: %s", uv_strerror(ret));
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, error_msg));
    JS_Call(ctx, fetch_ctx->reject_func, JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
    fetch_context_free(fetch_ctx);
  }

cleanup:
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);
  JS_FreeCString(ctx, url);
  return promise;
}

void JSRT_RuntimeSetupHttpFetch(JSRT_Runtime* rt) {
  JS_SetPropertyStr(rt->ctx, rt->global, "fetch", JS_NewCFunction(rt->ctx, jsrt_fetch, "fetch", 1));
  JS_SetPropertyStr(rt->ctx, rt->global, "Headers",
                    JS_NewCFunction2(rt->ctx, headers_constructor, "Headers", 1, JS_CFUNC_constructor, 0));
  JS_SetPropertyStr(rt->ctx, rt->global, "Request",
                    JS_NewCFunction2(rt->ctx, request_constructor, "Request", 2, JS_CFUNC_constructor, 0));
  JS_SetPropertyStr(rt->ctx, rt->global, "Response",
                    JS_NewCFunction2(rt->ctx, response_constructor, "Response", 2, JS_CFUNC_constructor, 0));
  JSRT_Debug("HTTP Fetch with llhttp setup completed");
}