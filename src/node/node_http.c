#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../deps/llhttp/build/llhttp.h"
#include "../std/url.h"
#include "node_modules.h"

// Forward declarations for URL and querystring parsing
extern JSValue JSRT_InitNodeQueryString(JSContext* ctx);

// Class IDs for HTTP classes
static JSClassID js_http_server_class_id;
static JSClassID js_http_request_class_id;
static JSClassID js_http_response_class_id;
static JSClassID js_http_client_request_class_id;

// Global HTTP server reference for connection handler workaround
static JSValue g_current_http_server;
static JSContext* g_current_http_server_ctx = NULL;
static bool g_http_server_initialized = false;

// HTTP Connection state for parsing
typedef struct {
  JSContext* ctx;
  JSValue server;
  JSValue socket;
  llhttp_t parser;
  llhttp_settings_t settings;
  JSValue current_request;
  JSValue current_response;
  bool request_complete;
} JSHttpConnection;

// HTTP Server state
typedef struct {
  JSContext* ctx;
  JSValue server_obj;
  JSValue net_server;  // Underlying net.Server
  bool destroyed;
} JSHttpServer;

// HTTP Request state
typedef struct {
  JSContext* ctx;
  JSValue request_obj;
  char* method;
  char* url;
  char* http_version;
  JSValue headers;
  JSValue socket;
} JSHttpRequest;

// HTTP Response state
typedef struct {
  JSContext* ctx;
  JSValue response_obj;
  JSValue socket;
  bool headers_sent;
  int status_code;
  char* status_message;
  JSValue headers;
} JSHttpResponse;

// Helper function to add EventEmitter methods and proper inheritance
static void setup_event_emitter_inheritance(JSContext* ctx, JSValue obj) {
  JSValue events_module = JSRT_LoadNodeModuleCommonJS(ctx, "events");
  if (!JS_IsException(events_module)) {
    JSValue event_emitter = JS_GetPropertyStr(ctx, events_module, "EventEmitter");
    if (!JS_IsException(event_emitter)) {
      JSValue prototype = JS_GetPropertyStr(ctx, event_emitter, "prototype");
      if (!JS_IsException(prototype)) {
        // Set up proper prototype chain
        JS_SetPrototype(ctx, obj, prototype);

        // Copy EventEmitter methods
        const char* methods[] = {"on", "emit", "once", "removeListener", "removeAllListeners", "listenerCount", NULL};
        for (int i = 0; methods[i]; i++) {
          JSValue method = JS_GetPropertyStr(ctx, prototype, methods[i]);
          if (JS_IsFunction(ctx, method)) {
            JS_SetPropertyStr(ctx, obj, methods[i], JS_DupValue(ctx, method));
          }
          JS_FreeValue(ctx, method);
        }

        // Initialize EventEmitter state
        JS_SetPropertyStr(ctx, obj, "_events", JS_NewObject(ctx));
        JS_SetPropertyStr(ctx, obj, "_eventsCount", JS_NewInt32(ctx, 0));
        JS_SetPropertyStr(ctx, obj, "_maxListeners", JS_NewInt32(ctx, 10));
      }
      JS_FreeValue(ctx, prototype);
    }
    JS_FreeValue(ctx, event_emitter);
    JS_FreeValue(ctx, events_module);
  }
}

// Mock HTTP parsing - in a real implementation, you'd use a proper HTTP parser
static JSValue parse_http_request(JSContext* ctx, const char* data) {
  JSValue req_obj = JS_NewObject(ctx);

  // Simple HTTP request parsing (mock implementation)
  char* line = strdup(data);
  char* method = strtok(line, " ");
  char* url = strtok(NULL, " ");
  char* version = strtok(NULL, "\r\n");

  if (method)
    JS_SetPropertyStr(ctx, req_obj, "method", JS_NewString(ctx, method));
  if (url)
    JS_SetPropertyStr(ctx, req_obj, "url", JS_NewString(ctx, url));
  if (version)
    JS_SetPropertyStr(ctx, req_obj, "httpVersion", JS_NewString(ctx, version + 5));  // Skip "HTTP/"

  // Mock headers
  JSValue headers = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, headers, "host", JS_NewString(ctx, "localhost"));
  JS_SetPropertyStr(ctx, headers, "user-agent", JS_NewString(ctx, "jsrt"));
  JS_SetPropertyStr(ctx, req_obj, "headers", headers);

  free(line);
  return req_obj;
}

// HTTP Response methods
static JSValue js_http_response_write_head(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || res->headers_sent) {
    return JS_ThrowTypeError(ctx, "Headers already sent");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "writeHead requires status code");
  }

  JS_ToInt32(ctx, &res->status_code, argv[0]);

  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    const char* message = JS_ToCString(ctx, argv[1]);
    if (message) {
      free(res->status_message);
      res->status_message = strdup(message);
      JS_FreeCString(ctx, message);
    }
  }

  if (argc > 2 && JS_IsObject(argv[2])) {
    // Set headers from object
    res->headers = JS_DupValue(ctx, argv[2]);
  }

  return JS_UNDEFINED;
}

static JSValue js_http_response_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  if (!res->headers_sent) {
    // Send headers first
    if (res->status_code == 0)
      res->status_code = 200;
    if (!res->status_message)
      res->status_message = strdup("OK");

    char status_line[1024];
    snprintf(status_line, sizeof(status_line),
             "HTTP/1.1 %d %s\r\nContent-Type: text/plain\r\nConnection: close\r\nServer: jsrt/1.0\r\n\r\n",
             res->status_code, res->status_message);

    // Write headers to socket
    if (!JS_IsUndefined(res->socket)) {
      JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
      if (JS_IsFunction(ctx, write_method)) {
        JSValue header_data = JS_NewString(ctx, status_line);
        JS_Call(ctx, write_method, res->socket, 1, &header_data);
        JS_FreeValue(ctx, header_data);
      }
      JS_FreeValue(ctx, write_method);
    }

    res->headers_sent = true;
  }

  if (argc > 0) {
    const char* data = JS_ToCString(ctx, argv[0]);
    if (data && !JS_IsUndefined(res->socket)) {
      // Write data to socket
      JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
      if (JS_IsFunction(ctx, write_method)) {
        JSValue body_data = JS_NewString(ctx, data);
        JS_Call(ctx, write_method, res->socket, 1, &body_data);
        JS_FreeValue(ctx, body_data);
      }
      JS_FreeValue(ctx, write_method);
      JS_FreeCString(ctx, data);
    }
  }

  return JS_NewBool(ctx, true);
}

static JSValue js_http_response_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  if (argc > 0) {
    // Write final data
    js_http_response_write(ctx, this_val, argc, argv);
  }

  // Ensure headers are sent even for empty response
  if (!res->headers_sent) {
    js_http_response_write(ctx, this_val, 0, NULL);
  }

  // End the response by closing the connection
  if (!JS_IsUndefined(res->socket)) {
    JSValue end_method = JS_GetPropertyStr(ctx, res->socket, "end");
    if (JS_IsFunction(ctx, end_method)) {
      JS_Call(ctx, end_method, res->socket, 0, NULL);
    }
    JS_FreeValue(ctx, end_method);
  }

  return JS_UNDEFINED;
}

// Add setHeader method
static JSValue js_http_response_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || res->headers_sent) {
    return JS_ThrowTypeError(ctx, "Headers already sent");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "setHeader requires name and value");
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  const char* value = JS_ToCString(ctx, argv[1]);

  if (name && value) {
    JS_SetPropertyStr(ctx, res->headers, name, JS_NewString(ctx, value));
  }

  if (name)
    JS_FreeCString(ctx, name);
  if (value)
    JS_FreeCString(ctx, value);

  return JS_UNDEFINED;
}

// HTTP Server methods
static JSValue js_http_server_listen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpServer* server = JS_GetOpaque(this_val, js_http_server_class_id);
  if (!server) {
    return JS_ThrowTypeError(ctx, "Invalid server object");
  }

  // Delegate to the underlying net.Server
  JSValue listen_method = JS_GetPropertyStr(ctx, server->net_server, "listen");
  JSValue result = JS_Call(ctx, listen_method, server->net_server, argc, argv);
  JS_FreeValue(ctx, listen_method);

  return result;
}

static JSValue js_http_server_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpServer* server = JS_GetOpaque(this_val, js_http_server_class_id);
  if (!server) {
    return JS_UNDEFINED;
  }

  if (!server->destroyed) {
    JSValue close_method = JS_GetPropertyStr(ctx, server->net_server, "close");
    JS_Call(ctx, close_method, server->net_server, argc, argv);
    JS_FreeValue(ctx, close_method);
    server->destroyed = true;
  }

  return JS_UNDEFINED;
}

// Class finalizers
static void js_http_server_finalizer(JSRuntime* rt, JSValue val) {
  JSHttpServer* server = JS_GetOpaque(val, js_http_server_class_id);
  if (server) {
    JS_FreeValueRT(rt, server->net_server);
    free(server);
  }
}

static void js_http_response_finalizer(JSRuntime* rt, JSValue val) {
  JSHttpResponse* res = JS_GetOpaque(val, js_http_response_class_id);
  if (res) {
    free(res->status_message);
    JS_FreeValueRT(rt, res->headers);
    JS_FreeValueRT(rt, res->socket);
    free(res);
  }
}

static void js_http_request_finalizer(JSRuntime* rt, JSValue val) {
  JSHttpRequest* req = JS_GetOpaque(val, js_http_request_class_id);
  if (req) {
    free(req->method);
    free(req->url);
    free(req->http_version);
    JS_FreeValueRT(rt, req->headers);
    JS_FreeValueRT(rt, req->socket);
    free(req);
  }
}

// Class definitions
static JSClassDef js_http_server_class = {
    "Server",
    .finalizer = js_http_server_finalizer,
};

static JSClassDef js_http_response_class = {
    "ServerResponse",
    .finalizer = js_http_response_finalizer,
};

static JSClassDef js_http_request_class = {
    "IncomingMessage",
    .finalizer = js_http_request_finalizer,
};

// Constructor functions
static JSValue js_http_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_http_server_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSHttpServer* server = calloc(1, sizeof(JSHttpServer));
  if (!server) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  server->ctx = ctx;
  server->server_obj = JS_DupValue(ctx, obj);
  server->destroyed = false;

  // Create underlying net.Server
  JSValue net_module = JSRT_LoadNodeModuleCommonJS(ctx, "net");
  if (JS_IsException(net_module)) {
    free(server);
    JS_FreeValue(ctx, obj);
    return net_module;
  }

  JSValue create_server = JS_GetPropertyStr(ctx, net_module, "createServer");
  server->net_server = JS_Call(ctx, create_server, JS_UNDEFINED, 0, NULL);
  JS_FreeValue(ctx, create_server);
  JS_FreeValue(ctx, net_module);

  if (JS_IsException(server->net_server)) {
    free(server);
    JS_FreeValue(ctx, obj);
    return server->net_server;
  }

  JS_SetOpaque(obj, server);

  // Add HTTP server methods
  JS_SetPropertyStr(ctx, obj, "listen", JS_NewCFunction(ctx, js_http_server_listen, "listen", 3));
  JS_SetPropertyStr(ctx, obj, "close", JS_NewCFunction(ctx, js_http_server_close, "close", 0));

  // Add EventEmitter functionality
  setup_event_emitter_inheritance(ctx, obj);

  return obj;
}

static JSValue js_http_response_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_http_response_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSHttpResponse* res = calloc(1, sizeof(JSHttpResponse));
  if (!res) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  res->ctx = ctx;
  res->response_obj = JS_DupValue(ctx, obj);
  res->headers_sent = false;
  res->status_code = 0;
  res->headers = JS_NewObject(ctx);

  JS_SetOpaque(obj, res);

  // Add response methods
  JS_SetPropertyStr(ctx, obj, "writeHead", JS_NewCFunction(ctx, js_http_response_write_head, "writeHead", 3));
  JS_SetPropertyStr(ctx, obj, "write", JS_NewCFunction(ctx, js_http_response_write, "write", 1));
  JS_SetPropertyStr(ctx, obj, "end", JS_NewCFunction(ctx, js_http_response_end, "end", 1));
  JS_SetPropertyStr(ctx, obj, "setHeader", JS_NewCFunction(ctx, js_http_response_set_header, "setHeader", 2));

  // Add properties
  JS_SetPropertyStr(ctx, obj, "statusCode", JS_NewInt32(ctx, 200));
  JS_SetPropertyStr(ctx, obj, "statusMessage", JS_NewString(ctx, "OK"));
  JS_SetPropertyStr(ctx, obj, "headersSent", JS_NewBool(ctx, false));

  return obj;
}

static JSValue js_http_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_http_request_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSHttpRequest* req = calloc(1, sizeof(JSHttpRequest));
  if (!req) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  req->ctx = ctx;
  req->request_obj = JS_DupValue(ctx, obj);
  req->headers = JS_NewObject(ctx);

  JS_SetOpaque(obj, req);

  // Add default properties
  JS_SetPropertyStr(ctx, obj, "method", JS_NewString(ctx, "GET"));
  JS_SetPropertyStr(ctx, obj, "url", JS_NewString(ctx, "/"));
  JS_SetPropertyStr(ctx, obj, "httpVersion", JS_NewString(ctx, "1.1"));
  JS_SetPropertyStr(ctx, obj, "headers", JS_DupValue(ctx, req->headers));

  return obj;
}

// llhttp callback functions
static int on_message_begin(llhttp_t* parser) {
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

static int on_url(llhttp_t* parser, const char* at, size_t length) {
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

static int on_message_complete(llhttp_t* parser) {
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

// Forward declaration
static void parse_enhanced_http_request(JSContext* ctx, const char* data, char* method, char* url, char* version,
                                        JSValue request);

// Simple HTTP data handler
static JSValue js_http_simple_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1)
    return JS_UNDEFINED;

  typedef struct {
    JSValue server;
    JSValue request;
    JSValue response;
    JSContext* ctx;
  } HTTPHandlerData;

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
    JS_Call(ctx, emit, data->server, 3, args);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, args[2]);
  }
  JS_FreeValue(ctx, emit);

  JS_FreeCString(ctx, request_data);
  return JS_UNDEFINED;
}

// Enhanced HTTP request line parser with URL parsing
static void parse_enhanced_http_request(JSContext* ctx, const char* data, char* method, char* url, char* version,
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

// Simple HTTP connection handler
static void js_http_connection_handler(JSContext* ctx, JSValue server, JSValue socket) {
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

    // Store the HTTP objects for the handler - use a simple struct
    typedef struct {
      JSValue server;
      JSValue request;
      JSValue response;
      JSContext* ctx;
    } HTTPHandlerData;

    HTTPHandlerData* handler_data = malloc(sizeof(HTTPHandlerData));
    if (handler_data) {
      handler_data->server = JS_DupValue(ctx, server);
      handler_data->request = JS_DupValue(ctx, request);
      handler_data->response = JS_DupValue(ctx, response);
      handler_data->ctx = ctx;
      JS_SetOpaque(data_handler, handler_data);
    }

    JSValue args[] = {JS_NewString(ctx, "data"), data_handler};
    JS_Call(ctx, on_method, socket, 2, args);
    JS_FreeValue(ctx, args[0]);
    // Don't free data_handler - stored in event system
  }
  JS_FreeValue(ctx, on_method);

  // Clean up our references (duplicates are stored in the handler)
  JS_FreeValue(ctx, request);
  JS_FreeValue(ctx, response);
}

// C function wrapper for connection handling
static JSValue js_http_net_connection_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc > 0) {
    // Use global HTTP server reference (workaround for event system property loss)
    if (g_http_server_initialized && g_current_http_server_ctx == ctx) {
      js_http_connection_handler(ctx, g_current_http_server, argv[0]);
    }
  }
  return JS_UNDEFINED;
}

// Main module functions
static JSValue js_http_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue server = js_http_server_constructor(ctx, JS_UNDEFINED, 0, NULL);

  if (argc > 0 && JS_IsFunction(ctx, argv[0])) {
    // Add request listener
    JSValue on_method = JS_GetPropertyStr(ctx, server, "on");
    if (JS_IsFunction(ctx, on_method)) {
      JSValue args[] = {JS_NewString(ctx, "request"), JS_DupValue(ctx, argv[0])};
      JS_Call(ctx, on_method, server, 2, args);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
    }
    JS_FreeValue(ctx, on_method);
  }

  // Set up connection handler for the underlying net server
  JSHttpServer* http_server = JS_GetOpaque(server, js_http_server_class_id);
  if (http_server) {
    JSValue net_on_method = JS_GetPropertyStr(ctx, http_server->net_server, "on");
    if (JS_IsFunction(ctx, net_on_method)) {
      // Create a connection handler function that processes HTTP requests
      JSValue connection_handler = JS_NewCFunction(ctx, js_http_net_connection_handler, "connectionHandler", 1);

      // Store reference to HTTP server in global variable (workaround for event system)
      if (g_http_server_initialized) {
        JS_FreeValue(g_current_http_server_ctx, g_current_http_server);
      }
      g_current_http_server = JS_DupValue(ctx, server);
      g_current_http_server_ctx = ctx;
      g_http_server_initialized = true;

      JSValue args[] = {JS_NewString(ctx, "connection"), connection_handler};
      JSValue result = JS_Call(ctx, net_on_method, http_server->net_server, 2, args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
      // DO NOT free connection_handler (args[1]) - it needs to persist for event callbacks
      // The event system will manage its lifecycle
    }
    JS_FreeValue(ctx, net_on_method);
  }
  return server;
}

static JSValue js_http_request(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Basic HTTP request implementation (mock for now)
  JSValue client_req = js_http_request_constructor(ctx, JS_UNDEFINED, 0, NULL);

  if (argc > 0) {
    // Parse URL/options
    if (JS_IsString(argv[0])) {
      const char* url = JS_ToCString(ctx, argv[0]);
      if (url) {
        JS_SetPropertyStr(ctx, client_req, "url", JS_NewString(ctx, url));
        JS_FreeCString(ctx, url);
      }
    }
  }

  // Add client request methods
  JS_SetPropertyStr(ctx, client_req, "write", JS_NewCFunction(ctx, NULL, "write", 1));
  JS_SetPropertyStr(ctx, client_req, "end", JS_NewCFunction(ctx, NULL, "end", 1));

  return client_req;
}

// HTTP Agent constructor for connection pooling
static JSValue js_http_agent_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue agent = JS_NewObject(ctx);

  // Set default properties
  JS_SetPropertyStr(ctx, agent, "maxSockets", JS_NewInt32(ctx, 5));
  JS_SetPropertyStr(ctx, agent, "maxFreeSockets", JS_NewInt32(ctx, 256));
  JS_SetPropertyStr(ctx, agent, "timeout", JS_NewInt32(ctx, 30000));  // 30 seconds
  JS_SetPropertyStr(ctx, agent, "keepAlive", JS_TRUE);
  JS_SetPropertyStr(ctx, agent, "protocol", JS_NewString(ctx, "http:"));

  // Parse options if provided
  if (argc > 0 && JS_IsObject(argv[0])) {
    JSValue maxSockets = JS_GetPropertyStr(ctx, argv[0], "maxSockets");
    if (JS_IsNumber(maxSockets)) {
      JS_SetPropertyStr(ctx, agent, "maxSockets", JS_DupValue(ctx, maxSockets));
    }
    JS_FreeValue(ctx, maxSockets);

    JSValue keepAlive = JS_GetPropertyStr(ctx, argv[0], "keepAlive");
    if (JS_IsBool(keepAlive)) {
      JS_SetPropertyStr(ctx, agent, "keepAlive", JS_DupValue(ctx, keepAlive));
    }
    JS_FreeValue(ctx, keepAlive);
  }

  return agent;
}

// Module initialization
JSValue JSRT_InitNodeHttp(JSContext* ctx) {
  JSValue http_module = JS_NewObject(ctx);

  // Register class IDs
  JS_NewClassID(&js_http_server_class_id);
  JS_NewClassID(&js_http_response_class_id);
  JS_NewClassID(&js_http_request_class_id);

  // Create class definitions
  JS_NewClass(JS_GetRuntime(ctx), js_http_server_class_id, &js_http_server_class);
  JS_NewClass(JS_GetRuntime(ctx), js_http_response_class_id, &js_http_response_class);
  JS_NewClass(JS_GetRuntime(ctx), js_http_request_class_id, &js_http_request_class);

  // Create constructors
  JSValue server_ctor = JS_NewCFunction2(ctx, js_http_server_constructor, "Server", 0, JS_CFUNC_constructor, 0);
  JSValue response_ctor =
      JS_NewCFunction2(ctx, js_http_response_constructor, "ServerResponse", 0, JS_CFUNC_constructor, 0);
  JSValue request_ctor =
      JS_NewCFunction2(ctx, js_http_request_constructor, "IncomingMessage", 0, JS_CFUNC_constructor, 0);

  // Module functions
  JS_SetPropertyStr(ctx, http_module, "createServer", JS_NewCFunction(ctx, js_http_create_server, "createServer", 1));
  JS_SetPropertyStr(ctx, http_module, "request", JS_NewCFunction(ctx, js_http_request, "request", 2));

  // HTTP Agent class with connection pooling
  JS_SetPropertyStr(ctx, http_module, "Agent",
                    JS_NewCFunction2(ctx, js_http_agent_constructor, "Agent", 1, JS_CFUNC_constructor, 0));

  // Export constructors
  JS_SetPropertyStr(ctx, http_module, "Server", server_ctor);
  JS_SetPropertyStr(ctx, http_module, "ServerResponse", response_ctor);
  JS_SetPropertyStr(ctx, http_module, "IncomingMessage", request_ctor);

  // HTTP methods constants
  JS_SetPropertyStr(ctx, http_module, "METHODS", JS_NewArray(ctx));
  JSValue methods = JS_GetPropertyStr(ctx, http_module, "METHODS");
  const char* http_methods[] = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS", "PATCH", NULL};
  for (int i = 0; http_methods[i]; i++) {
    JS_SetPropertyUint32(ctx, methods, i, JS_NewString(ctx, http_methods[i]));
  }
  JS_FreeValue(ctx, methods);

  // Status codes
  JSValue status_codes = JS_NewObject(ctx);
  JS_SetPropertyStr(ctx, status_codes, "200", JS_NewString(ctx, "OK"));
  JS_SetPropertyStr(ctx, status_codes, "404", JS_NewString(ctx, "Not Found"));
  JS_SetPropertyStr(ctx, status_codes, "500", JS_NewString(ctx, "Internal Server Error"));
  JS_SetPropertyStr(ctx, http_module, "STATUS_CODES", status_codes);

  // Global agent with connection pooling
  JSValue globalAgent = js_http_agent_constructor(ctx, JS_UNDEFINED, 0, NULL);
  JS_SetPropertyStr(ctx, globalAgent, "maxSockets", JS_NewInt32(ctx, 5));
  JS_SetPropertyStr(ctx, globalAgent, "maxFreeSockets", JS_NewInt32(ctx, 256));
  JS_SetPropertyStr(ctx, globalAgent, "keepAlive", JS_TRUE);
  JS_SetPropertyStr(ctx, globalAgent, "protocol", JS_NewString(ctx, "http:"));
  JS_SetPropertyStr(ctx, http_module, "globalAgent", globalAgent);

  return http_module;
}

// ES Module support
int js_node_http_init(JSContext* ctx, JSModuleDef* m) {
  JSValue http_module = JSRT_InitNodeHttp(ctx);

  // Export individual functions with proper memory management
  JSValue createServer = JS_GetPropertyStr(ctx, http_module, "createServer");
  JS_SetModuleExport(ctx, m, "createServer", JS_DupValue(ctx, createServer));
  JS_FreeValue(ctx, createServer);

  JSValue request = JS_GetPropertyStr(ctx, http_module, "request");
  JS_SetModuleExport(ctx, m, "request", JS_DupValue(ctx, request));
  JS_FreeValue(ctx, request);

  JSValue Agent = JS_GetPropertyStr(ctx, http_module, "Agent");
  JS_SetModuleExport(ctx, m, "Agent", JS_DupValue(ctx, Agent));
  JS_FreeValue(ctx, Agent);

  JSValue globalAgent = JS_GetPropertyStr(ctx, http_module, "globalAgent");
  JS_SetModuleExport(ctx, m, "globalAgent", JS_DupValue(ctx, globalAgent));
  JS_FreeValue(ctx, globalAgent);

  JSValue Server = JS_GetPropertyStr(ctx, http_module, "Server");
  JS_SetModuleExport(ctx, m, "Server", JS_DupValue(ctx, Server));
  JS_FreeValue(ctx, Server);

  JSValue ServerResponse = JS_GetPropertyStr(ctx, http_module, "ServerResponse");
  JS_SetModuleExport(ctx, m, "ServerResponse", JS_DupValue(ctx, ServerResponse));
  JS_FreeValue(ctx, ServerResponse);

  JSValue IncomingMessage = JS_GetPropertyStr(ctx, http_module, "IncomingMessage");
  JS_SetModuleExport(ctx, m, "IncomingMessage", JS_DupValue(ctx, IncomingMessage));
  JS_FreeValue(ctx, IncomingMessage);

  JSValue METHODS = JS_GetPropertyStr(ctx, http_module, "METHODS");
  JS_SetModuleExport(ctx, m, "METHODS", JS_DupValue(ctx, METHODS));
  JS_FreeValue(ctx, METHODS);

  JSValue STATUS_CODES = JS_GetPropertyStr(ctx, http_module, "STATUS_CODES");
  JS_SetModuleExport(ctx, m, "STATUS_CODES", JS_DupValue(ctx, STATUS_CODES));
  JS_FreeValue(ctx, STATUS_CODES);

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, http_module));

  JS_FreeValue(ctx, http_module);
  return 0;
}