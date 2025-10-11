#include "http_client.h"
#include "http_internal.h"

// Global variables
JSClassID js_http_server_class_id;
JSClassID js_http_request_class_id;
JSClassID js_http_response_class_id;
JSClassID js_http_client_request_class_id;
JSClassID js_http_agent_class_id;

// Helper function to add EventEmitter methods and proper inheritance
void setup_event_emitter_inheritance(JSContext* ctx, JSValue obj) {
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

// Main HTTP createServer function
JSValue js_http_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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
      // CRITICAL FIX #1.4: Use wrapper to store server reference instead of global variable
      // Create wrapper that holds server reference
      JSHttpConnectionHandlerWrapper* wrapper = malloc(sizeof(JSHttpConnectionHandlerWrapper));
      if (wrapper) {
        wrapper->ctx = ctx;
        wrapper->server = JS_DupValue(ctx, server);

        // Store wrapper in server for cleanup
        http_server->conn_wrapper = wrapper;

        // Create connection handler with wrapper as opaque data
        JSValue connection_handler = JS_NewCFunction(ctx, js_http_net_connection_handler, "connectionHandler", 1);
        JS_SetOpaque(connection_handler, wrapper);

        JSValue args[] = {JS_NewString(ctx, "connection"), connection_handler};
        JSValue result = JS_Call(ctx, net_on_method, http_server->net_server, 2, args);
        JS_FreeValue(ctx, result);
        JS_FreeValue(ctx, args[0]);
        // DO NOT free connection_handler (args[1]) - it needs to persist for event callbacks
        // The event system will manage its lifecycle
      }
    }
    JS_FreeValue(ctx, net_on_method);
  }
  return server;
}

// Helper to parse URL string into components
static int parse_url_components(JSContext* ctx, const char* url_str, char** host, int* port, char** path,
                                char** protocol) {
  // Basic URL parsing: http://host:port/path
  // For simplicity, only support http:// for now
  const char* p = url_str;

  // Check protocol
  if (strncmp(p, "http://", 7) == 0) {
    *protocol = strdup("http:");
    p += 7;
    *port = 80;
  } else if (strncmp(p, "https://", 8) == 0) {
    *protocol = strdup("https:");
    p += 8;
    *port = 443;
  } else {
    // Assume http if no protocol
    *protocol = strdup("http:");
    *port = 80;
  }

  // Extract host and optional port
  const char* path_start = strchr(p, '/');
  const char* port_start = strchr(p, ':');

  // Determine host length
  size_t host_len;
  if (port_start && (!path_start || port_start < path_start)) {
    // Port specified
    host_len = port_start - p;
    port_start++;  // Skip ':'
    *port = atoi(port_start);
  } else if (path_start) {
    host_len = path_start - p;
  } else {
    host_len = strlen(p);
  }

  *host = malloc(host_len + 1);
  if (!*host) {
    free(*protocol);
    return -1;
  }
  memcpy(*host, p, host_len);
  (*host)[host_len] = '\0';

  // Extract path
  if (path_start) {
    *path = strdup(path_start);
  } else {
    *path = strdup("/");
  }

  return 0;
}

// Socket data handler for client response parsing
static JSValue http_client_socket_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Get the client request from the socket's opaque data
  JSValue client_req_val = JS_GetPropertyStr(ctx, this_val, "_clientRequest");
  if (JS_IsUndefined(client_req_val)) {
    JS_FreeValue(ctx, client_req_val);
    return JS_UNDEFINED;
  }

  JSHTTPClientRequest* client_req = JS_GetOpaque(client_req_val, js_http_client_request_class_id);
  if (!client_req || argc < 1) {
    JS_FreeValue(ctx, client_req_val);
    return JS_UNDEFINED;
  }

  // Parse received data with llhttp
  size_t data_len;
  uint8_t* data = JS_GetArrayBuffer(ctx, &data_len, argv[0]);
  if (data) {
    llhttp_execute(&client_req->parser, (const char*)data, data_len);
  } else {
    // Try to get string data
    const char* str_data = JS_ToCString(ctx, argv[0]);
    if (str_data) {
      llhttp_execute(&client_req->parser, str_data, strlen(str_data));
      JS_FreeCString(ctx, str_data);
    }
  }

  JS_FreeValue(ctx, client_req_val);
  return JS_UNDEFINED;
}

// Socket connect handler for client requests
static JSValue http_client_socket_connect_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue client_req_val = JS_GetPropertyStr(ctx, this_val, "_clientRequest");
  if (!JS_IsUndefined(client_req_val)) {
    // CRITICAL FIX: Send pending headers FIRST, before emitting 'socket' event
    // The socket is already connected at this point (ready event just fired)
    // If end() was called before socket connected, headers are pending
    JSHTTPClientRequest* client_req = JS_GetOpaque(client_req_val, js_http_client_request_class_id);
    if (client_req && client_req->finished && !client_req->headers_sent) {
      // Headers were not sent because socket wasn't connected
      // Send them now before user code sees the socket
      send_headers(client_req);
    }

    // Now emit 'socket' event on ClientRequest
    JSValue emit = JS_GetPropertyStr(ctx, client_req_val, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue args[] = {JS_NewString(ctx, "socket"), JS_DupValue(ctx, this_val)};
      JSValue result = JS_Call(ctx, emit, client_req_val, 2, args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
    }
    JS_FreeValue(ctx, emit);
  }
  JS_FreeValue(ctx, client_req_val);
  return JS_UNDEFINED;
}

// HTTP request function (full implementation)
JSValue js_http_request(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "http.request requires URL or options");
  }

  // Create ClientRequest object
  JSValue client_req = js_http_client_request_constructor(ctx, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(client_req)) {
    return client_req;
  }

  JSHTTPClientRequest* req_data = JS_GetOpaque(client_req, js_http_client_request_class_id);
  if (!req_data) {
    JS_FreeValue(ctx, client_req);
    return JS_ThrowTypeError(ctx, "Failed to create ClientRequest");
  }

  // Parse URL or options
  JSValue options = JS_UNDEFINED;
  if (JS_IsString(argv[0])) {
    // URL string provided
    const char* url_str = JS_ToCString(ctx, argv[0]);
    if (url_str) {
      // Store URL property on ClientRequest object
      JS_SetPropertyStr(ctx, client_req, "url", JS_NewString(ctx, url_str));

      char* host = NULL;
      char* path = NULL;
      char* protocol = NULL;
      int port = 80;

      if (parse_url_components(ctx, url_str, &host, &port, &path, &protocol) == 0) {
        // Update client request with parsed values
        free(req_data->host);
        free(req_data->path);
        free(req_data->protocol);
        req_data->host = host;
        req_data->port = port;
        req_data->path = path;
        req_data->protocol = protocol;
      }
      JS_FreeCString(ctx, url_str);
    }

    // Check for options in second argument
    if (argc > 1 && JS_IsObject(argv[1])) {
      options = JS_DupValue(ctx, argv[1]);
    }
  } else if (JS_IsObject(argv[0])) {
    // Options object provided
    options = JS_DupValue(ctx, argv[0]);

    // Extract host
    JSValue host_val = JS_GetPropertyStr(ctx, options, "host");
    if (JS_IsString(host_val)) {
      const char* host = JS_ToCString(ctx, host_val);
      if (host) {
        free(req_data->host);
        req_data->host = strdup(host);
        JS_FreeCString(ctx, host);
      }
    }
    JS_FreeValue(ctx, host_val);

    // Extract port
    JSValue port_val = JS_GetPropertyStr(ctx, options, "port");
    if (JS_IsNumber(port_val)) {
      int32_t port;
      JS_ToInt32(ctx, &port, port_val);
      req_data->port = port;
    }
    JS_FreeValue(ctx, port_val);

    // Extract path
    JSValue path_val = JS_GetPropertyStr(ctx, options, "path");
    if (JS_IsString(path_val)) {
      const char* path = JS_ToCString(ctx, path_val);
      if (path) {
        free(req_data->path);
        req_data->path = strdup(path);
        JS_FreeCString(ctx, path);
      }
    }
    JS_FreeValue(ctx, path_val);

    // Extract method
    JSValue method_val = JS_GetPropertyStr(ctx, options, "method");
    if (JS_IsString(method_val)) {
      const char* method = JS_ToCString(ctx, method_val);
      if (method) {
        free(req_data->method);
        req_data->method = strdup(method);
        JS_FreeCString(ctx, method);
      }
    }
    JS_FreeValue(ctx, method_val);

    // Extract headers
    JSValue headers_val = JS_GetPropertyStr(ctx, options, "headers");
    if (JS_IsObject(headers_val)) {
      JSPropertyEnum* tab;
      uint32_t len;
      if (JS_GetOwnPropertyNames(ctx, &tab, &len, headers_val, JS_GPN_STRING_MASK) == 0) {
        for (uint32_t i = 0; i < len; i++) {
          JSValue key = JS_AtomToString(ctx, tab[i].atom);
          JSValue value = JS_GetProperty(ctx, headers_val, tab[i].atom);

          const char* key_str = JS_ToCString(ctx, key);
          const char* value_str = JS_ToCString(ctx, value);

          if (key_str && value_str) {
            JS_SetPropertyStr(ctx, req_data->headers, key_str, JS_NewString(ctx, value_str));
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

  // Set default headers
  JSValue host_header = JS_GetPropertyStr(ctx, req_data->headers, "host");
  if (JS_IsUndefined(host_header)) {
    if (req_data->host) {
      char host_header_val[512];
      if (req_data->port == 80 || req_data->port == 443) {
        snprintf(host_header_val, sizeof(host_header_val), "%s", req_data->host);
      } else {
        snprintf(host_header_val, sizeof(host_header_val), "%s:%d", req_data->host, req_data->port);
      }
      JS_SetPropertyStr(ctx, req_data->headers, "host", JS_NewString(ctx, host_header_val));
    }
  }
  JS_FreeValue(ctx, host_header);

  // Set Connection: close by default (no keep-alive for now)
  JSValue connection_header = JS_GetPropertyStr(ctx, req_data->headers, "connection");
  if (JS_IsUndefined(connection_header)) {
    JS_SetPropertyStr(ctx, req_data->headers, "connection", JS_NewString(ctx, "close"));
  }
  JS_FreeValue(ctx, connection_header);

  // Create TCP socket and connect
  JSValue net_module = JSRT_LoadNodeModuleCommonJS(ctx, "net");
  if (JS_IsException(net_module)) {
    JS_FreeValue(ctx, client_req);
    if (!JS_IsUndefined(options))
      JS_FreeValue(ctx, options);
    return net_module;
  }

  JSValue socket_constructor = JS_GetPropertyStr(ctx, net_module, "Socket");
  JSValue socket = JS_CallConstructor(ctx, socket_constructor, 0, NULL);
  JS_FreeValue(ctx, socket_constructor);
  JS_FreeValue(ctx, net_module);

  if (JS_IsException(socket)) {
    JS_FreeValue(ctx, client_req);
    if (!JS_IsUndefined(options))
      JS_FreeValue(ctx, options);
    return socket;
  }

  // Store socket in ClientRequest
  req_data->socket = JS_DupValue(ctx, socket);

  // Create IncomingMessage for response
  req_data->response_obj = js_http_request_constructor(ctx, JS_UNDEFINED, 0, NULL);

  // Set up socket event handlers
  JSValue on_method = JS_GetPropertyStr(ctx, socket, "on");
  if (JS_IsFunction(ctx, on_method)) {
    // Store client request reference on socket for data handler
    JS_SetPropertyStr(ctx, socket, "_clientRequest", JS_DupValue(ctx, client_req));

    // on('data') - parse HTTP response
    JSValue data_handler = JS_NewCFunction(ctx, http_client_socket_data_handler, "dataHandler", 1);
    JSValue args[] = {JS_NewString(ctx, "data"), data_handler};
    JSValue result = JS_Call(ctx, on_method, socket, 2, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
    // data_handler is now owned by event system

    // on('ready') - emit 'socket' event on request (socket emits 'ready' not 'connect')
    JSValue connect_handler = JS_NewCFunction(ctx, http_client_socket_connect_handler, "connectHandler", 0);
    JSValue connect_args[] = {JS_NewString(ctx, "ready"), connect_handler};
    result = JS_Call(ctx, on_method, socket, 2, connect_args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, connect_args[0]);
  }
  JS_FreeValue(ctx, on_method);

  // Connect socket
  JSValue connect_method = JS_GetPropertyStr(ctx, socket, "connect");
  if (JS_IsFunction(ctx, connect_method)) {
    JSValue connect_args[] = {JS_NewInt32(ctx, req_data->port),
                              JS_NewString(ctx, req_data->host ? req_data->host : "localhost")};
    JSValue result = JS_Call(ctx, connect_method, socket, 2, connect_args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, connect_args[0]);
    JS_FreeValue(ctx, connect_args[1]);
  }
  JS_FreeValue(ctx, connect_method);
  JS_FreeValue(ctx, socket);

  // Register callback if provided (last argument)
  int callback_idx = argc - 1;
  if (callback_idx >= 0 && JS_IsFunction(ctx, argv[callback_idx])) {
    JSValue on_method = JS_GetPropertyStr(ctx, client_req, "on");
    if (JS_IsFunction(ctx, on_method)) {
      JSValue args[] = {JS_NewString(ctx, "response"), JS_DupValue(ctx, argv[callback_idx])};
      JSValue result = JS_Call(ctx, on_method, client_req, 2, args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
    }
    JS_FreeValue(ctx, on_method);
  }

  if (!JS_IsUndefined(options)) {
    JS_FreeValue(ctx, options);
  }

  return client_req;
}

// HTTP get function (convenience wrapper)
JSValue js_http_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  // Call http.request() with method: 'GET'
  JSValue client_req = js_http_request(ctx, this_val, argc, argv);
  if (JS_IsException(client_req)) {
    return client_req;
  }

  // Automatically call end()
  JSValue end_method = JS_GetPropertyStr(ctx, client_req, "end");
  if (JS_IsFunction(ctx, end_method)) {
    JSValue result = JS_Call(ctx, end_method, client_req, 0, NULL);
    JS_FreeValue(ctx, result);
  }
  JS_FreeValue(ctx, end_method);

  return client_req;
}

// HTTP Agent constructor for connection pooling
JSValue js_http_agent_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
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

static JSClassDef js_http_client_request_class = {
    "ClientRequest",
    .finalizer = js_http_client_request_finalizer,
};

// Module initialization
JSValue JSRT_InitNodeHttp(JSContext* ctx) {
  JSValue http_module = JS_NewObject(ctx);

  // Register class IDs
  JS_NewClassID(&js_http_server_class_id);
  JS_NewClassID(&js_http_response_class_id);
  JS_NewClassID(&js_http_request_class_id);
  JS_NewClassID(&js_http_client_request_class_id);

  // Create class definitions
  JS_NewClass(JS_GetRuntime(ctx), js_http_server_class_id, &js_http_server_class);
  JS_NewClass(JS_GetRuntime(ctx), js_http_response_class_id, &js_http_response_class);
  JS_NewClass(JS_GetRuntime(ctx), js_http_request_class_id, &js_http_request_class);
  JS_NewClass(JS_GetRuntime(ctx), js_http_client_request_class_id, &js_http_client_request_class);

  // Create constructors
  JSValue server_ctor = JS_NewCFunction2(ctx, js_http_server_constructor, "Server", 0, JS_CFUNC_constructor, 0);
  JSValue response_ctor =
      JS_NewCFunction2(ctx, js_http_response_constructor, "ServerResponse", 0, JS_CFUNC_constructor, 0);
  JSValue request_ctor =
      JS_NewCFunction2(ctx, js_http_request_constructor, "IncomingMessage", 0, JS_CFUNC_constructor, 0);

  // Create constructors for client
  JSValue client_request_ctor =
      JS_NewCFunction2(ctx, js_http_client_request_constructor, "ClientRequest", 0, JS_CFUNC_constructor, 0);

  // Module functions
  JS_SetPropertyStr(ctx, http_module, "createServer", JS_NewCFunction(ctx, js_http_create_server, "createServer", 1));
  JS_SetPropertyStr(ctx, http_module, "request", JS_NewCFunction(ctx, js_http_request, "request", 3));
  JS_SetPropertyStr(ctx, http_module, "get", JS_NewCFunction(ctx, js_http_get, "get", 3));

  // HTTP Agent class with connection pooling
  JS_SetPropertyStr(ctx, http_module, "Agent",
                    JS_NewCFunction2(ctx, js_http_agent_constructor, "Agent", 1, JS_CFUNC_constructor, 0));

  // Export constructors
  JS_SetPropertyStr(ctx, http_module, "Server", server_ctor);
  JS_SetPropertyStr(ctx, http_module, "ServerResponse", response_ctor);
  JS_SetPropertyStr(ctx, http_module, "IncomingMessage", request_ctor);
  JS_SetPropertyStr(ctx, http_module, "ClientRequest", client_request_ctor);

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

  JSValue get = JS_GetPropertyStr(ctx, http_module, "get");
  JS_SetModuleExport(ctx, m, "get", JS_DupValue(ctx, get));
  JS_FreeValue(ctx, get);

  JSValue Agent = JS_GetPropertyStr(ctx, http_module, "Agent");
  JS_SetModuleExport(ctx, m, "Agent", JS_DupValue(ctx, Agent));
  JS_FreeValue(ctx, Agent);

  JSValue ClientRequest = JS_GetPropertyStr(ctx, http_module, "ClientRequest");
  JS_SetModuleExport(ctx, m, "ClientRequest", JS_DupValue(ctx, ClientRequest));
  JS_FreeValue(ctx, ClientRequest);

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
