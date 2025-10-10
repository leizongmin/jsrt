#include <ctype.h>
#include "http_internal.h"

// HTTP Parser implementation with full llhttp integration
// Adapted from src/http/parser.c with node:http specific enhancements

// Utility: String duplication helpers
static char* jsrt_strdup(const char* str) {
  if (!str)
    return NULL;
  size_t len = strlen(str);
  char* copy = malloc(len + 1);
  if (copy) {
    memcpy(copy, str, len + 1);
  }
  return copy;
}

static char* jsrt_strndup(const char* str, size_t n) {
  if (!str)
    return NULL;
  char* copy = malloc(n + 1);
  if (copy) {
    memcpy(copy, str, n);
    copy[n] = '\0';
  }
  return copy;
}

// Utility: Convert string to lowercase for case-insensitive header storage
static char* str_to_lower(const char* str) {
  if (!str)
    return NULL;
  size_t len = strlen(str);
  char* lower = malloc(len + 1);
  if (!lower)
    return NULL;
  for (size_t i = 0; i < len; i++) {
    lower[i] = tolower((unsigned char)str[i]);
  }
  lower[len] = '\0';
  return lower;
}

// Utility: Append to dynamic buffer
static int buffer_append(char** buffer, size_t* size, size_t* capacity, const char* data, size_t len) {
  if (!buffer || !data || len == 0)
    return 0;

  if (*size + len > *capacity) {
    size_t new_capacity = *capacity == 0 ? 4096 : *capacity * 2;
    while (new_capacity < *size + len) {
      new_capacity *= 2;
    }

    char* new_buffer = realloc(*buffer, new_capacity);
    if (!new_buffer)
      return -1;

    *buffer = new_buffer;
    *capacity = new_capacity;
  }

  memcpy(*buffer + *size, data, len);
  *size += len;

  return 0;
}

// llhttp callback: message begin
int on_message_begin(llhttp_t* parser) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;
  JSContext* ctx = conn->ctx;

  // Free previous request/response if they exist (keep-alive reuse)
  if (!JS_IsUndefined(conn->current_request)) {
    JS_FreeValue(ctx, conn->current_request);
    conn->current_request = JS_UNDEFINED;
  }
  if (!JS_IsUndefined(conn->current_response)) {
    JS_FreeValue(ctx, conn->current_response);
    conn->current_response = JS_UNDEFINED;
  }

  // Reset parsing state
  free(conn->current_header_field);
  conn->current_header_field = NULL;
  free(conn->current_header_value);
  conn->current_header_value = NULL;
  free(conn->url_buffer);
  conn->url_buffer = NULL;
  conn->url_buffer_size = 0;
  conn->url_buffer_capacity = 0;
  free(conn->body_buffer);
  conn->body_buffer = NULL;
  conn->body_size = 0;
  conn->body_capacity = 0;

  // Create new request and response objects
  conn->current_request = js_http_request_constructor(ctx, JS_UNDEFINED, 0, NULL);
  conn->current_response = js_http_response_constructor(ctx, JS_UNDEFINED, 0, NULL);

  if (JS_IsException(conn->current_request) || JS_IsException(conn->current_response)) {
    return -1;
  }

  // Set up response with socket reference
  JSHttpResponse* res = JS_GetOpaque(conn->current_response, js_http_response_class_id);
  if (res) {
    JS_FreeValue(ctx, res->socket);
    res->socket = JS_DupValue(ctx, conn->socket);
  }

  // Set up request with socket reference
  JSHttpRequest* req = JS_GetOpaque(conn->current_request, js_http_request_class_id);
  if (req) {
    JS_FreeValue(ctx, req->socket);
    req->socket = JS_DupValue(ctx, conn->socket);
  }

  conn->request_complete = false;
  return 0;
}

// llhttp callback: URL accumulation (may be called multiple times)
int on_url(llhttp_t* parser, const char* at, size_t length) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;

  // Accumulate URL data (llhttp may call this multiple times for a single URL)
  if (buffer_append(&conn->url_buffer, &conn->url_buffer_size, &conn->url_buffer_capacity, at, length) != 0) {
    return -1;
  }

  return 0;
}

// llhttp callback: Status message (for responses - not used in server mode)
int on_status(llhttp_t* parser, const char* at, size_t length) {
  // Not used in HTTP server parsing (only for client responses)
  return 0;
}

// llhttp callback: Header field
int on_header_field(llhttp_t* parser, const char* at, size_t length) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;
  JSContext* ctx = conn->ctx;

  // If we have a complete previous header (field + value), store it
  if (conn->current_header_field && conn->current_header_value) {
    JSHttpRequest* req = JS_GetOpaque(conn->current_request, js_http_request_class_id);
    if (req) {
      // Normalize header name to lowercase
      char* lower_name = str_to_lower(conn->current_header_field);
      if (lower_name) {
        // Check if header already exists (multi-value header)
        JSValue existing = JS_GetPropertyStr(ctx, req->headers, lower_name);
        if (!JS_IsUndefined(existing)) {
          // Multi-value header: convert to array or append to array
          if (JS_IsArray(ctx, existing)) {
            // Already an array, append
            JSValue new_value = JS_NewString(ctx, conn->current_header_value);
            JSValue push = JS_GetPropertyStr(ctx, existing, "push");
            JSValue args[] = {new_value};
            JS_Call(ctx, push, existing, 1, args);
            JS_FreeValue(ctx, push);
            JS_FreeValue(ctx, new_value);
          } else {
            // Convert to array
            JSValue array = JS_NewArray(ctx);
            JSValue old_value = existing;  // Don't free, we're moving it
            JS_SetPropertyUint32(ctx, array, 0, old_value);
            JSValue new_value = JS_NewString(ctx, conn->current_header_value);
            JS_SetPropertyUint32(ctx, array, 1, new_value);
            JS_SetPropertyStr(ctx, req->headers, lower_name, array);
          }
        } else {
          // First occurrence of this header
          JS_SetPropertyStr(ctx, req->headers, lower_name, JS_NewString(ctx, conn->current_header_value));
        }
        JS_FreeValue(ctx, existing);
        free(lower_name);
      }
    }

    free(conn->current_header_field);
    free(conn->current_header_value);
    conn->current_header_field = NULL;
    conn->current_header_value = NULL;
  }

  // Start accumulating new header field
  conn->current_header_field = jsrt_strndup(at, length);
  return conn->current_header_field ? 0 : -1;
}

// llhttp callback: Header value
int on_header_value(llhttp_t* parser, const char* at, size_t length) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;

  // Accumulate header value (may be called multiple times)
  if (!conn->current_header_value) {
    conn->current_header_value = jsrt_strndup(at, length);
    return conn->current_header_value ? 0 : -1;
  } else {
    // Append to existing value
    size_t old_len = strlen(conn->current_header_value);
    char* new_value = realloc(conn->current_header_value, old_len + length + 1);
    if (!new_value)
      return -1;
    memcpy(new_value + old_len, at, length);
    new_value[old_len + length] = '\0';
    conn->current_header_value = new_value;
    return 0;
  }
}

// llhttp callback: Headers complete
int on_headers_complete(llhttp_t* parser) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;
  JSContext* ctx = conn->ctx;

  // Store final header if present
  if (conn->current_header_field && conn->current_header_value) {
    JSHttpRequest* req = JS_GetOpaque(conn->current_request, js_http_request_class_id);
    if (req) {
      char* lower_name = str_to_lower(conn->current_header_field);
      if (lower_name) {
        JSValue existing = JS_GetPropertyStr(ctx, req->headers, lower_name);
        if (!JS_IsUndefined(existing)) {
          if (JS_IsArray(ctx, existing)) {
            JSValue new_value = JS_NewString(ctx, conn->current_header_value);
            JSValue push = JS_GetPropertyStr(ctx, existing, "push");
            JSValue args[] = {new_value};
            JS_Call(ctx, push, existing, 1, args);
            JS_FreeValue(ctx, push);
            JS_FreeValue(ctx, new_value);
          } else {
            JSValue array = JS_NewArray(ctx);
            JS_SetPropertyUint32(ctx, array, 0, existing);
            JSValue new_value = JS_NewString(ctx, conn->current_header_value);
            JS_SetPropertyUint32(ctx, array, 1, new_value);
            JS_SetPropertyStr(ctx, req->headers, lower_name, array);
          }
        } else {
          JS_SetPropertyStr(ctx, req->headers, lower_name, JS_NewString(ctx, conn->current_header_value));
        }
        JS_FreeValue(ctx, existing);
        free(lower_name);
      }
    }

    free(conn->current_header_field);
    free(conn->current_header_value);
    conn->current_header_field = NULL;
    conn->current_header_value = NULL;
  }

  // Set request metadata
  JSHttpRequest* req = JS_GetOpaque(conn->current_request, js_http_request_class_id);
  if (req) {
    // Set HTTP version
    char version[8];
    snprintf(version, sizeof(version), "%d.%d", parser->http_major, parser->http_minor);
    free(req->http_version);
    req->http_version = jsrt_strdup(version);

    // Set method
    if (parser->method != 0) {
      free(req->method);
      req->method = jsrt_strdup(llhttp_method_name(parser->method));
    }

    // Set URL (accumulated from on_url callbacks)
    if (conn->url_buffer && conn->url_buffer_size > 0) {
      free(req->url);
      // Null-terminate the accumulated URL
      char* url = malloc(conn->url_buffer_size + 1);
      if (url) {
        memcpy(url, conn->url_buffer, conn->url_buffer_size);
        url[conn->url_buffer_size] = '\0';
        req->url = url;
      }
    }

    // Update JS object properties
    if (req->method) {
      JS_SetPropertyStr(ctx, conn->current_request, "method", JS_NewString(ctx, req->method));
    }
    if (req->url) {
      JS_SetPropertyStr(ctx, conn->current_request, "url", JS_NewString(ctx, req->url));

      // Parse URL for pathname and query
      parse_enhanced_http_request(ctx, NULL, NULL, req->url, NULL, conn->current_request);
    }
    if (req->http_version) {
      JS_SetPropertyStr(ctx, conn->current_request, "httpVersion", JS_NewString(ctx, req->http_version));
    }

    // Check Connection header for keep-alive
    JSValue conn_header = JS_GetPropertyStr(ctx, req->headers, "connection");
    const char* conn_str = JS_ToCString(ctx, conn_header);
    if (conn_str) {
      conn->keep_alive = (strcasecmp(conn_str, "keep-alive") == 0);
      conn->should_close = (strcasecmp(conn_str, "close") == 0);
      JS_FreeCString(ctx, conn_str);
    } else {
      // HTTP/1.1 defaults to keep-alive, HTTP/1.0 defaults to close
      conn->keep_alive = (parser->http_major == 1 && parser->http_minor == 1);
      conn->should_close = !conn->keep_alive;
    }
    JS_FreeValue(ctx, conn_header);
  }

  return 0;
}

// llhttp callback: Body chunk
int on_body(llhttp_t* parser, const char* at, size_t length) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;

  // Accumulate body data
  if (buffer_append(&conn->body_buffer, &conn->body_size, &conn->body_capacity, at, length) != 0) {
    return -1;
  }

  // TODO Phase 4: Emit 'data' events for streaming instead of buffering

  return 0;
}

// llhttp callback: Message complete
int on_message_complete(llhttp_t* parser) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;
  JSContext* ctx = conn->ctx;

  // Set body on request if any
  if (conn->body_buffer && conn->body_size > 0) {
    // Store body as internal property for now (Phase 4 will make it a stream)
    JSValue body_str = JS_NewStringLen(ctx, conn->body_buffer, conn->body_size);
    JS_SetPropertyStr(ctx, conn->current_request, "_body", body_str);
  }

  // Emit 'request' event on server
  JSValue emit = JS_GetPropertyStr(ctx, conn->server, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "request"), JS_DupValue(ctx, conn->current_request),
                      JS_DupValue(ctx, conn->current_response)};
    JSValue result = JS_Call(ctx, emit, conn->server, 3, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, args[2]);
  }
  JS_FreeValue(ctx, emit);

  conn->request_complete = true;

  // For keep-alive, reset parser for next request
  if (conn->keep_alive && !conn->should_close) {
    llhttp_init(&conn->parser, HTTP_REQUEST, &conn->settings);
    conn->parser.data = conn;
  }

  return 0;
}

// llhttp callback: Chunk header (for chunked transfer encoding)
int on_chunk_header(llhttp_t* parser) {
  // Called when a chunk header is parsed
  // Chunk size available in parser->content_length
  return 0;
}

// llhttp callback: Chunk complete (for chunked transfer encoding)
int on_chunk_complete(llhttp_t* parser) {
  // Called when a chunk is complete
  return 0;
}

// Enhanced HTTP request URL parser (called from on_headers_complete)
void parse_enhanced_http_request(JSContext* ctx, const char* data, char* method, char* url, char* version,
                                 JSValue request) {
  // If url is provided directly, parse it
  if (!url)
    return;

  // Parse URL for pathname and query
  if (url[0] == '/') {
    // This is a path-only URL
    char* path_part = url;
    char* query_part = strchr(url, '?');

    if (query_part) {
      // Temporarily null-terminate the path
      size_t path_len = query_part - path_part;
      char* pathname = jsrt_strndup(path_part, path_len);
      query_part++;  // Move to start of query string

      if (pathname) {
        JS_SetPropertyStr(ctx, request, "pathname", JS_NewString(ctx, pathname));
        free(pathname);
      }

      // Parse query string using node:querystring
      JSValue querystring_module = JSRT_InitNodeQueryString(ctx);
      JSValue parse_func = JS_GetPropertyStr(ctx, querystring_module, "parse");

      if (!JS_IsUndefined(parse_func)) {
        JSValue query_str_val = JS_NewString(ctx, query_part);
        JSValue parsed_query = JS_Call(ctx, parse_func, JS_UNDEFINED, 1, &query_str_val);
        JS_SetPropertyStr(ctx, request, "query", parsed_query);
        JS_SetPropertyStr(ctx, request, "search", JS_NewString(ctx, query_part));
        JS_FreeValue(ctx, query_str_val);
        // Don't free parsed_query, it's stored in request
      }

      JS_FreeValue(ctx, parse_func);
      JS_FreeValue(ctx, querystring_module);
    } else {
      // No query string, just set pathname
      JS_SetPropertyStr(ctx, request, "pathname", JS_NewString(ctx, path_part));
      JS_SetPropertyStr(ctx, request, "query", JS_NewObject(ctx));
      JS_SetPropertyStr(ctx, request, "search", JS_NewString(ctx, ""));
    }
  }
}

// Simple HTTP data handler (legacy, now integrated with full llhttp)
JSValue js_http_simple_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1)
    return JS_UNDEFINED;

  HTTPHandlerData* data = JS_GetOpaque(this_val, 0);
  if (!data)
    return JS_UNDEFINED;

  const char* request_data = JS_ToCString(ctx, argv[0]);
  if (!request_data)
    return JS_UNDEFINED;

  // Parse HTTP request line (simplified for legacy compatibility)
  char method[16] = "GET";
  char url[1024] = "/";
  char version[16] = "HTTP/1.1";

  const char* start = request_data;
  const char* space1 = strchr(start, ' ');
  if (space1) {
    int method_len = space1 - start;
    if (method_len < 16) {
      strncpy(method, start, method_len);
      method[method_len] = '\0';
    }

    start = space1 + 1;
    const char* space2 = strchr(start, ' ');
    if (space2) {
      int url_len = space2 - start;
      if (url_len < 1024) {
        strncpy(url, start, url_len);
        url[url_len] = '\0';
      }

      start = space2 + 1;
      const char* crlf = strstr(start, "\r\n");
      if (crlf) {
        int version_len = crlf - start;
        if (version_len < 16) {
          strncpy(version, start, version_len);
          version[version_len] = '\0';
        }
      }
    }
  }

  parse_enhanced_http_request(ctx, NULL, method, url, version, data->request);

  // Update request object with parsed data
  JS_SetPropertyStr(ctx, data->request, "method", JS_NewString(ctx, method));
  JS_SetPropertyStr(ctx, data->request, "url", JS_NewString(ctx, url));
  JS_SetPropertyStr(ctx, data->request, "httpVersion", JS_NewString(ctx, version));

  // Emit 'request' event on server
  JSValue emit = JS_GetPropertyStr(ctx, data->server, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "request"), JS_DupValue(ctx, data->request), JS_DupValue(ctx, data->response)};
    JSValue result = JS_Call(ctx, emit, data->server, 3, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, args[2]);
  }
  JS_FreeValue(ctx, emit);

  JS_FreeCString(ctx, request_data);
  return JS_UNDEFINED;
}

// HTTP connection handler with llhttp parser
void js_http_connection_handler(JSContext* ctx, JSValue server, JSValue socket) {
  // Allocate connection state
  JSHttpConnection* conn = calloc(1, sizeof(JSHttpConnection));
  if (!conn)
    return;

  conn->ctx = ctx;
  conn->server = JS_DupValue(ctx, server);
  conn->socket = JS_DupValue(ctx, socket);
  conn->current_request = JS_UNDEFINED;
  conn->current_response = JS_UNDEFINED;
  conn->request_complete = false;
  conn->keep_alive = false;
  conn->should_close = false;

  // Initialize llhttp parser
  llhttp_settings_init(&conn->settings);
  conn->settings.on_message_begin = on_message_begin;
  conn->settings.on_url = on_url;
  conn->settings.on_status = on_status;
  conn->settings.on_header_field = on_header_field;
  conn->settings.on_header_value = on_header_value;
  conn->settings.on_headers_complete = on_headers_complete;
  conn->settings.on_body = on_body;
  conn->settings.on_message_complete = on_message_complete;
  conn->settings.on_chunk_header = on_chunk_header;
  conn->settings.on_chunk_complete = on_chunk_complete;

  llhttp_init(&conn->parser, HTTP_REQUEST, &conn->settings);
  conn->parser.data = conn;

  // Set up socket 'data' listener
  JSValue on_method = JS_GetPropertyStr(ctx, socket, "on");
  if (JS_IsFunction(ctx, on_method)) {
    // Create data handler that parses with llhttp
    JSValue data_handler = JS_NewCFunction(ctx, js_http_llhttp_data_handler, "httpLLHttpDataHandler", 1);
    JS_SetOpaque(data_handler, conn);

    JSValue args[] = {JS_NewString(ctx, "data"), data_handler};
    JSValue result = JS_Call(ctx, on_method, socket, 2, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, on_method);

  // Set up socket 'close' listener for cleanup
  JSValue on_close_method = JS_GetPropertyStr(ctx, socket, "on");
  if (JS_IsFunction(ctx, on_close_method)) {
    JSValue close_handler = JS_NewCFunction(ctx, js_http_close_handler, "httpCloseHandler", 0);
    JS_SetOpaque(close_handler, conn);

    JSValue args[] = {JS_NewString(ctx, "close"), close_handler};
    JSValue result = JS_Call(ctx, on_close_method, socket, 2, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, on_close_method);
}

// llhttp data handler - parses incoming socket data
JSValue js_http_llhttp_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1)
    return JS_UNDEFINED;

  JSHttpConnection* conn = JS_GetOpaque(this_val, 0);
  if (!conn)
    return JS_UNDEFINED;

  // Get data buffer
  size_t data_len;
  const char* data;

  if (JS_IsString(argv[0])) {
    data = JS_ToCStringLen(ctx, &data_len, argv[0]);
  } else {
    // Handle Buffer/Uint8Array
    data = JS_ToCString(ctx, argv[0]);
    data_len = data ? strlen(data) : 0;
  }

  if (!data)
    return JS_UNDEFINED;

  // Parse with llhttp
  llhttp_errno_t err = llhttp_execute(&conn->parser, data, data_len);

  if (err != HPE_OK && err != HPE_PAUSED && err != HPE_PAUSED_UPGRADE) {
    // Parse error - emit error event on server
    JSValue emit = JS_GetPropertyStr(ctx, conn->server, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue error = JS_NewError(ctx);
      const char* err_msg = llhttp_errno_name(err);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, err_msg ? err_msg : "HTTP parse error"));

      JSValue args[] = {JS_NewString(ctx, "clientError"), error, JS_DupValue(ctx, conn->socket)};
      JSValue result = JS_Call(ctx, emit, conn->server, 3, args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
      JS_FreeValue(ctx, args[2]);
    }
    JS_FreeValue(ctx, emit);

    // Close connection on error
    JSValue end_method = JS_GetPropertyStr(ctx, conn->socket, "end");
    if (JS_IsFunction(ctx, end_method)) {
      JS_Call(ctx, end_method, conn->socket, 0, NULL);
    }
    JS_FreeValue(ctx, end_method);
  }

  JS_FreeCString(ctx, data);
  return JS_UNDEFINED;
}

// Socket close handler - cleanup connection state
JSValue js_http_close_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpConnection* conn = JS_GetOpaque(this_val, 0);
  if (!conn)
    return JS_UNDEFINED;

  // Free all connection resources
  JS_FreeValue(ctx, conn->server);
  JS_FreeValue(ctx, conn->socket);
  if (!JS_IsUndefined(conn->current_request)) {
    JS_FreeValue(ctx, conn->current_request);
  }
  if (!JS_IsUndefined(conn->current_response)) {
    JS_FreeValue(ctx, conn->current_response);
  }

  free(conn->current_header_field);
  free(conn->current_header_value);
  free(conn->url_buffer);
  free(conn->body_buffer);
  free(conn);

  return JS_UNDEFINED;
}

// C function wrapper for connection handling
JSValue js_http_net_connection_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc > 0) {
    // Use global HTTP server reference (workaround for event system property loss)
    if (g_http_server_initialized && g_current_http_server_ctx == ctx) {
      js_http_connection_handler(ctx, g_current_http_server, argv[0]);
    }
  }
  return JS_UNDEFINED;
}
