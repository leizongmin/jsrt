#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "node_modules.h"

// Helper function to add EventEmitter methods to an object
static void add_event_emitter_methods(JSContext* ctx, JSValue obj) {
  // Get the node:events module to access EventEmitter methods
  JSValue events_module = JSRT_LoadNodeModuleCommonJS(ctx, "events");
  if (!JS_IsException(events_module)) {
    JSValue event_emitter = JS_GetPropertyStr(ctx, events_module, "EventEmitter");
    if (!JS_IsException(event_emitter)) {
      JSValue prototype = JS_GetPropertyStr(ctx, event_emitter, "prototype");
      if (!JS_IsException(prototype)) {
        // Copy EventEmitter methods
        const char* methods[] = {"on", "emit", "once", "removeListener", "removeAllListeners", "listenerCount", NULL};
        for (int i = 0; methods[i]; i++) {
          JSValue method = JS_GetPropertyStr(ctx, prototype, methods[i]);
          if (JS_IsFunction(ctx, method)) {
            JS_SetPropertyStr(ctx, obj, methods[i], JS_DupValue(ctx, method));
          }
          JS_FreeValue(ctx, method);
        }

        // Initialize the _events property that EventEmitter uses
        JS_SetPropertyStr(ctx, obj, "_events", JS_NewObject(ctx));
      }
      JS_FreeValue(ctx, prototype);
    }
    JS_FreeValue(ctx, event_emitter);
    JS_FreeValue(ctx, events_module);
  }
}

// Class IDs for networking classes
static JSClassID js_server_class_id;
static JSClassID js_socket_class_id;

// Connection state
typedef struct {
  JSContext* ctx;
  JSValue server_obj;
  JSValue socket_obj;
  uv_tcp_t handle;
  uv_connect_t connect_req;
  uv_shutdown_t shutdown_req;
  char* host;
  int port;
  bool connected;
  bool destroyed;
} JSNetConnection;

// Server state
typedef struct {
  JSContext* ctx;
  JSValue server_obj;
  uv_tcp_t handle;
  bool listening;
  bool destroyed;
  char* host;
  int port;
} JSNetServer;

// Forward declarations
static JSValue js_net_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_net_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_socket_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
static JSValue js_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

// Connection callbacks
static void on_connection(uv_stream_t* server, int status) {
  JSNetServer* server_data = (JSNetServer*)server->data;
  if (status < 0) {
    // TODO: Emit error event
    return;
  }

  JSContext* ctx = server_data->ctx;

  // Create new socket for the connection
  JSValue socket = js_socket_constructor(ctx, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(socket)) {
    return;
  }

  JSNetConnection* conn = JS_GetOpaque(socket, js_socket_class_id);
  if (!conn) {
    JS_FreeValue(ctx, socket);
    return;
  }

  // Accept the connection
  uv_tcp_init(uv_default_loop(), &conn->handle);
  conn->handle.data = conn;

  if (uv_accept(server, (uv_stream_t*)&conn->handle) == 0) {
    conn->connected = true;

    // Emit 'connection' event on server
    JSValue emit = JS_GetPropertyStr(ctx, server_data->server_obj, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue args[] = {JS_NewString(ctx, "connection"), socket};
      JS_Call(ctx, emit, server_data->server_obj, 2, args);
      JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, emit);
  }

  JS_FreeValue(ctx, socket);
}

static void on_connect(uv_connect_t* req, int status) {
  JSNetConnection* conn = (JSNetConnection*)req->data;
  JSContext* ctx = conn->ctx;

  if (status == 0) {
    conn->connected = true;

    // Emit 'connect' event
    JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue args[] = {JS_NewString(ctx, "connect")};
      JS_Call(ctx, emit, conn->socket_obj, 1, args);
      JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, emit);
  } else {
    // Emit 'error' event
    JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, uv_strerror(status)));
      JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(status)));

      JSValue args[] = {JS_NewString(ctx, "error"), error};
      JS_Call(ctx, emit, conn->socket_obj, 2, args);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
    }
    JS_FreeValue(ctx, emit);
  }
}

// Socket methods
static JSValue js_socket_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return JS_ThrowTypeError(ctx, "Socket is destroyed");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "connect requires port and host");
  }

  int port = 0;
  JS_ToInt32(ctx, &port, argv[0]);
  if (port <= 0 || port > 65535) {
    return JS_ThrowRangeError(ctx, "Port must be between 1 and 65535");
  }

  const char* host = JS_ToCString(ctx, argv[1]);
  if (!host) {
    return JS_EXCEPTION;
  }

  // Store connection info
  conn->port = port;
  free(conn->host);
  conn->host = strdup(host);
  JS_FreeCString(ctx, host);

  // Initialize TCP handle
  uv_tcp_init(uv_default_loop(), &conn->handle);
  conn->handle.data = conn;
  conn->connect_req.data = conn;

  // Resolve address and connect
  struct sockaddr_in addr;
  uv_ip4_addr(conn->host, conn->port, &addr);

  int result = uv_tcp_connect(&conn->connect_req, &conn->handle, (const struct sockaddr*)&addr, on_connect);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to connect: %s", uv_strerror(result));
  }

  return this_val;  // Return this for chaining
}

static JSValue js_socket_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed || !conn->connected) {
    return JS_ThrowTypeError(ctx, "Socket is not connected");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write requires data");
  }

  // Convert data to string
  const char* data = JS_ToCString(ctx, argv[0]);
  if (!data) {
    return JS_EXCEPTION;
  }

  // Create write request
  uv_write_t* write_req = malloc(sizeof(uv_write_t));
  uv_buf_t buf = uv_buf_init((char*)data, strlen(data));

  // For simplicity, not implementing full async write handling yet
  // This is a basic synchronous write
  int result = uv_try_write((uv_stream_t*)&conn->handle, &buf, 1);

  JS_FreeCString(ctx, data);
  free(write_req);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Write failed: %s", uv_strerror(result));
  }

  return JS_NewBool(ctx, true);
}

static JSValue js_socket_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return JS_ThrowTypeError(ctx, "Socket is destroyed");
  }

  if (conn->connected) {
    // Shutdown the connection
    conn->shutdown_req.data = conn;
    uv_shutdown(&conn->shutdown_req, (uv_stream_t*)&conn->handle, NULL);
    conn->connected = false;
  }

  return JS_UNDEFINED;
}

static JSValue js_socket_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return JS_UNDEFINED;
  }

  if (conn->connected) {
    uv_close((uv_handle_t*)&conn->handle, NULL);
  }

  conn->destroyed = true;
  conn->connected = false;

  return JS_UNDEFINED;
}

// Server methods
static JSValue js_server_listen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || server->destroyed) {
    return JS_ThrowTypeError(ctx, "Server is destroyed");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "listen requires a port");
  }

  int port = 0;
  JS_ToInt32(ctx, &port, argv[0]);
  if (port <= 0 || port > 65535) {
    return JS_ThrowRangeError(ctx, "Port must be between 1 and 65535");
  }

  const char* host = "0.0.0.0";
  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    host = JS_ToCString(ctx, argv[1]);
  }

  // Store server info
  server->port = port;
  free(server->host);
  server->host = strdup(host);

  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    JS_FreeCString(ctx, host);
  }

  // Initialize TCP server
  uv_tcp_init(uv_default_loop(), &server->handle);
  server->handle.data = server;

  // Bind and listen
  struct sockaddr_in addr;
  uv_ip4_addr(server->host, server->port, &addr);

  int result = uv_tcp_bind(&server->handle, (const struct sockaddr*)&addr, 0);
  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Bind failed: %s", uv_strerror(result));
  }

  result = uv_listen((uv_stream_t*)&server->handle, 128, on_connection);
  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Listen failed: %s", uv_strerror(result));
  }

  server->listening = true;

  // Emit 'listening' event
  JSValue emit = JS_GetPropertyStr(ctx, server->server_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "listening")};
    JS_Call(ctx, emit, server->server_obj, 1, args);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);

  return this_val;
}

static JSValue js_server_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || server->destroyed) {
    return JS_UNDEFINED;
  }

  if (server->listening) {
    uv_close((uv_handle_t*)&server->handle, NULL);
    server->listening = false;
  }

  server->destroyed = true;

  return JS_UNDEFINED;
}

// Class finalizers
static void js_socket_finalizer(JSRuntime* rt, JSValue val) {
  JSNetConnection* conn = JS_GetOpaque(val, js_socket_class_id);
  if (conn) {
    if (conn->connected) {
      uv_close((uv_handle_t*)&conn->handle, NULL);
    }
    free(conn->host);
    free(conn);
  }
}

static void js_server_finalizer(JSRuntime* rt, JSValue val) {
  JSNetServer* server = JS_GetOpaque(val, js_server_class_id);
  if (server) {
    if (server->listening) {
      uv_close((uv_handle_t*)&server->handle, NULL);
    }
    free(server->host);
    free(server);
  }
}

// Class definitions
static JSClassDef js_socket_class = {
    "Socket",
    .finalizer = js_socket_finalizer,
};

static JSClassDef js_server_class = {
    "Server",
    .finalizer = js_server_finalizer,
};

// Constructor functions
static JSValue js_socket_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_socket_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSNetConnection* conn = calloc(1, sizeof(JSNetConnection));
  if (!conn) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  conn->ctx = ctx;
  conn->socket_obj = JS_DupValue(ctx, obj);
  conn->connected = false;
  conn->destroyed = false;

  JS_SetOpaque(obj, conn);

  // Add EventEmitter methods (simplified)
  JS_SetPropertyStr(ctx, obj, "connect", JS_NewCFunction(ctx, js_socket_connect, "connect", 2));
  JS_SetPropertyStr(ctx, obj, "write", JS_NewCFunction(ctx, js_socket_write, "write", 1));
  JS_SetPropertyStr(ctx, obj, "end", JS_NewCFunction(ctx, js_socket_end, "end", 0));
  JS_SetPropertyStr(ctx, obj, "destroy", JS_NewCFunction(ctx, js_socket_destroy, "destroy", 0));

  // Add EventEmitter functionality
  add_event_emitter_methods(ctx, obj);

  return obj;
}

static JSValue js_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_server_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSNetServer* server = calloc(1, sizeof(JSNetServer));
  if (!server) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  server->ctx = ctx;
  server->server_obj = JS_DupValue(ctx, obj);
  server->listening = false;
  server->destroyed = false;

  JS_SetOpaque(obj, server);

  // Add server methods
  JS_SetPropertyStr(ctx, obj, "listen", JS_NewCFunction(ctx, js_server_listen, "listen", 3));
  JS_SetPropertyStr(ctx, obj, "close", JS_NewCFunction(ctx, js_server_close, "close", 0));

  // Add EventEmitter functionality
  add_event_emitter_methods(ctx, obj);

  return obj;
}

// Main module functions
static JSValue js_net_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return js_server_constructor(ctx, JS_UNDEFINED, argc, argv);
}

static JSValue js_net_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue socket = js_socket_constructor(ctx, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(socket)) {
    return socket;
  }

  // Call connect on the socket
  JSValue connect_method = JS_GetPropertyStr(ctx, socket, "connect");
  JSValue result = JS_Call(ctx, connect_method, socket, argc, argv);
  JS_FreeValue(ctx, connect_method);

  if (JS_IsException(result)) {
    JS_FreeValue(ctx, socket);
    return result;
  }

  JS_FreeValue(ctx, result);
  return socket;
}

// Module initialization
JSValue JSRT_InitNodeNet(JSContext* ctx) {
  JSValue net_module = JS_NewObject(ctx);

  // Register class IDs
  JS_NewClassID(&js_socket_class_id);
  JS_NewClassID(&js_server_class_id);

  // Create class definitions
  JS_NewClass(JS_GetRuntime(ctx), js_socket_class_id, &js_socket_class);
  JS_NewClass(JS_GetRuntime(ctx), js_server_class_id, &js_server_class);

  // Create constructors
  JSValue socket_ctor = JS_NewCFunction2(ctx, js_socket_constructor, "Socket", 0, JS_CFUNC_constructor, 0);
  JSValue server_ctor = JS_NewCFunction2(ctx, js_server_constructor, "Server", 0, JS_CFUNC_constructor, 0);

  // Module functions
  JS_SetPropertyStr(ctx, net_module, "createServer", JS_NewCFunction(ctx, js_net_create_server, "createServer", 1));
  JS_SetPropertyStr(ctx, net_module, "connect", JS_NewCFunction(ctx, js_net_connect, "connect", 2));

  // Export constructors
  JS_SetPropertyStr(ctx, net_module, "Socket", socket_ctor);
  JS_SetPropertyStr(ctx, net_module, "Server", server_ctor);

  return net_module;
}

// ES Module support
int js_node_net_init(JSContext* ctx, JSModuleDef* m) {
  JSValue net_module = JSRT_InitNodeNet(ctx);

  // Export individual functions
  JS_SetModuleExport(ctx, m, "createServer", JS_GetPropertyStr(ctx, net_module, "createServer"));
  JS_SetModuleExport(ctx, m, "connect", JS_GetPropertyStr(ctx, net_module, "connect"));
  JS_SetModuleExport(ctx, m, "Socket", JS_GetPropertyStr(ctx, net_module, "Socket"));
  JS_SetModuleExport(ctx, m, "Server", JS_GetPropertyStr(ctx, net_module, "Server"));

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", net_module);

  return 0;
}