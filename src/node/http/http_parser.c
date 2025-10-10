#include "http_internal.h"

// HTTP Parser implementation
// This file contains llhttp integration and parsing callbacks

// llhttp callback: message begin
int on_message_begin(llhttp_t* parser) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;

  // Create new request and response objects
  conn->current_request = js_http_request_constructor(conn->ctx, JS_UNDEFINED, 0, NULL);
  conn->current_response = js_http_response_constructor(conn->ctx, JS_UNDEFINED, 0, NULL);

  if (JS_IsException(conn->current_request) || JS_IsException(conn->current_response)) {
    return -1;
  }

  // Set up response with socket reference
  JSHttpResponse* res = JS_GetOpaque(conn->current_response, js_http_response_class_id);
  if (res) {
    res->socket = JS_DupValue(conn->ctx, conn->socket);
  }

  return 0;
}

// llhttp callback: URL
int on_url(llhttp_t* parser, const char* at, size_t length) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;
  if (!JS_IsUndefined(conn->current_request)) {
    char* url = malloc(length + 1);
    if (url) {
      memcpy(url, at, length);
      url[length] = '\0';
      JS_SetPropertyStr(conn->ctx, conn->current_request, "url", JS_NewString(conn->ctx, url));
      free(url);
    }
  }
  return 0;
}

// llhttp callback: message complete
int on_message_complete(llhttp_t* parser) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;
  JSContext* ctx = conn->ctx;

  // Set request method
  const char* method = llhttp_method_name(llhttp_get_method(parser));
  JS_SetPropertyStr(ctx, conn->current_request, "method", JS_NewString(ctx, method));

  // Set HTTP version
  char version[8];
  snprintf(version, sizeof(version), "%d.%d", parser->http_major, parser->http_minor);
  JS_SetPropertyStr(ctx, conn->current_request, "httpVersion", JS_NewString(ctx, version));

  // Emit 'request' event on server
  JSValue emit = JS_GetPropertyStr(ctx, conn->server, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "request"), JS_DupValue(ctx, conn->current_request),
                      JS_DupValue(ctx, conn->current_response)};
    JS_Call(ctx, emit, conn->server, 3, args);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, args[2]);
  }
  JS_FreeValue(ctx, emit);

  conn->request_complete = true;
  return 0;
}

// Enhanced HTTP request line parser with URL parsing
void parse_enhanced_http_request(JSContext* ctx, const char* data, char* method, char* url, char* version,
                                 JSValue request) {
  // Simple parser for "METHOD /path HTTP/1.1"
  const char* start = data;
  const char* space1 = strchr(start, ' ');
  if (!space1)
    return;

  // Extract method
  int method_len = space1 - start;
  if (method_len < 16) {
    strncpy(method, start, method_len);
    method[method_len] = '\0';
  }

  // Extract URL
  start = space1 + 1;
  const char* space2 = strchr(start, ' ');
  if (!space2)
    return;

  int url_len = space2 - start;
  if (url_len < 1024) {
    strncpy(url, start, url_len);
    url[url_len] = '\0';
  }

  // Extract version
  start = space2 + 1;
  const char* crlf = strstr(start, "\r\n");
  if (crlf) {
    int version_len = crlf - start;
    if (version_len < 16) {
      strncpy(version, start, version_len);
      version[version_len] = '\0';
    }
  }

  // Parse URL using standard URL parser if it contains more than just a path
  if (url[0] == '/') {
    // This is a path-only URL, parse it properly
    char* path_part = url;
    char* query_part = strchr(url, '?');

    if (query_part) {
      *query_part = '\0';  // Temporarily null-terminate the path
      query_part++;        // Move to start of query string

      // Set pathname
      JS_SetPropertyStr(ctx, request, "pathname", JS_NewString(ctx, path_part));

      // Parse query string using node:querystring
      JSValue querystring_module = JSRT_InitNodeQueryString(ctx);
      JSValue parse_func = JS_GetPropertyStr(ctx, querystring_module, "parse");

      if (!JS_IsUndefined(parse_func)) {
        JSValue query_str_val = JS_NewString(ctx, query_part);
        JSValue parsed_query = JS_Call(ctx, parse_func, JS_UNDEFINED, 1, &query_str_val);
        JS_SetPropertyStr(ctx, request, "query", parsed_query);
        JS_SetPropertyStr(ctx, request, "search", JS_NewString(ctx, query_part));
        JS_FreeValue(ctx, query_str_val);
        JS_FreeValue(ctx, parsed_query);
      }

      JS_FreeValue(ctx, parse_func);
      JS_FreeValue(ctx, querystring_module);

      // Restore the '?' character
      *(query_part - 1) = '?';
    } else {
      // No query string, just set pathname
      JS_SetPropertyStr(ctx, request, "pathname", JS_NewString(ctx, path_part));
      JS_SetPropertyStr(ctx, request, "query", JS_NewObject(ctx));
      JS_SetPropertyStr(ctx, request, "search", JS_NewString(ctx, ""));
    }
  }
}

// Simple HTTP data handler
JSValue js_http_simple_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1)
    return JS_UNDEFINED;

  HTTPHandlerData* data = JS_GetOpaque(this_val, 0);
  if (!data)
    return JS_UNDEFINED;

  const char* request_data = JS_ToCString(ctx, argv[0]);
  if (!request_data)
    return JS_UNDEFINED;

  // Parse HTTP request line
  char method[16] = "GET";
  char url[1024] = "/";
  char version[16] = "HTTP/1.1";

  parse_enhanced_http_request(ctx, request_data, method, url, version, data->request);

  // Update request object with parsed data (pathname, query, search already set in parser)
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

// Simple HTTP connection handler
void js_http_connection_handler(JSContext* ctx, JSValue server, JSValue socket) {
  // Create HTTP request and response objects
  JSValue request = js_http_request_constructor(ctx, JS_UNDEFINED, 0, NULL);
  JSValue response = js_http_response_constructor(ctx, JS_UNDEFINED, 0, NULL);

  if (JS_IsException(request) || JS_IsException(response)) {
    JS_FreeValue(ctx, request);
    JS_FreeValue(ctx, response);
    return;
  }

  // Set up response with socket reference for actual writing
  JSHttpResponse* res = JS_GetOpaque(response, js_http_response_class_id);
  if (res) {
    res->socket = JS_DupValue(ctx, socket);
  }

  // Set up request properties
  JSHttpRequest* req = JS_GetOpaque(request, js_http_request_class_id);
  if (req) {
    req->socket = JS_DupValue(ctx, socket);
  }

  // Set up simple data handler that parses HTTP and emits request event
  JSValue on_method = JS_GetPropertyStr(ctx, socket, "on");
  if (JS_IsFunction(ctx, on_method)) {
    // Create a closure that captures request, response, and server
    JSValue data_handler = JS_NewCFunction(ctx, js_http_simple_data_handler, "httpDataHandler", 1);

    // Store the HTTP objects for the handler (simplified approach - no finalizer)
    HTTPHandlerData* handler_data = malloc(sizeof(HTTPHandlerData));
    if (handler_data) {
      handler_data->server = JS_DupValue(ctx, server);
      handler_data->request = JS_DupValue(ctx, request);
      handler_data->response = JS_DupValue(ctx, response);
      handler_data->ctx = ctx;
      JS_SetOpaque(data_handler, handler_data);
    }

    JSValue args[] = {JS_NewString(ctx, "data"), data_handler};
    JSValue result = JS_Call(ctx, on_method, socket, 2, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
    // Note: data_handler is kept alive by the event system
  }
  JS_FreeValue(ctx, on_method);

  // Clean up our references (duplicates are stored in the handler)
  JS_FreeValue(ctx, request);
  JS_FreeValue(ctx, response);
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
