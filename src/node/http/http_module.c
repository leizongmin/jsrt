#include "http_internal.h"

// Global variables
JSClassID js_http_server_class_id;
JSClassID js_http_request_class_id;
JSClassID js_http_response_class_id;
JSClassID js_http_client_request_class_id;

JSValue g_current_http_server;
JSContext* g_current_http_server_ctx = NULL;
bool g_http_server_initialized = false;

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

// HTTP request function (basic implementation)
JSValue js_http_request(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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
