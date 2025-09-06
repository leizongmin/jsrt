#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "node_modules.h"

// Class IDs for HTTP classes
static JSClassID js_http_server_class_id;
static JSClassID js_http_request_class_id;
static JSClassID js_http_response_class_id;
static JSClassID js_http_client_request_class_id;

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

// Helper function to add EventEmitter methods
static void add_event_emitter_methods(JSContext* ctx, JSValue obj) {
  JSValue events_module = JSRT_LoadNodeModuleCommonJS(ctx, "events");
  if (!JS_IsException(events_module)) {
    JSValue event_emitter = JS_GetPropertyStr(ctx, events_module, "EventEmitter");
    if (!JS_IsException(event_emitter)) {
      JSValue prototype = JS_GetPropertyStr(ctx, event_emitter, "prototype");
      if (!JS_IsException(prototype)) {
        const char* methods[] = {"on", "emit", "once", "removeListener", "removeAllListeners", "listenerCount", NULL};
        for (int i = 0; methods[i]; i++) {
          JSValue method = JS_GetPropertyStr(ctx, prototype, methods[i]);
          if (JS_IsFunction(ctx, method)) {
            JS_SetPropertyStr(ctx, obj, methods[i], JS_DupValue(ctx, method));
          }
          JS_FreeValue(ctx, method);
        }
        JS_SetPropertyStr(ctx, obj, "_events", JS_NewObject(ctx));
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

    char status_line[256];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", res->status_code, res->status_message);

    // For simplicity, just mark headers as sent
    res->headers_sent = true;
  }

  if (argc > 0) {
    const char* data = JS_ToCString(ctx, argv[0]);
    if (data) {
      // In a real implementation, you'd write to the socket
      // For now, just mark as handled
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

  // End the response
  if (!res->headers_sent) {
    res->headers_sent = true;
  }

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
  add_event_emitter_methods(ctx, obj);

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

  // Export individual functions
  JS_SetModuleExport(ctx, m, "createServer", JS_GetPropertyStr(ctx, http_module, "createServer"));
  JS_SetModuleExport(ctx, m, "request", JS_GetPropertyStr(ctx, http_module, "request"));
  JS_SetModuleExport(ctx, m, "Agent", JS_GetPropertyStr(ctx, http_module, "Agent"));
  JS_SetModuleExport(ctx, m, "globalAgent", JS_GetPropertyStr(ctx, http_module, "globalAgent"));
  JS_SetModuleExport(ctx, m, "Server", JS_GetPropertyStr(ctx, http_module, "Server"));
  JS_SetModuleExport(ctx, m, "ServerResponse", JS_GetPropertyStr(ctx, http_module, "ServerResponse"));
  JS_SetModuleExport(ctx, m, "IncomingMessage", JS_GetPropertyStr(ctx, http_module, "IncomingMessage"));
  JS_SetModuleExport(ctx, m, "METHODS", JS_GetPropertyStr(ctx, http_module, "METHODS"));
  JS_SetModuleExport(ctx, m, "STATUS_CODES", JS_GetPropertyStr(ctx, http_module, "STATUS_CODES"));

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", http_module);

  return 0;
}