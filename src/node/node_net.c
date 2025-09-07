#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../runtime.h"
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
  JSValue listen_callback;    // Store callback for async execution
  uv_timer_t callback_timer;  // Timer for async callback
} JSNetServer;

// Forward declarations
static JSValue js_net_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_net_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_socket_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
static JSValue js_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
static void on_listen_callback_timer(uv_timer_t* timer);

// Data read callback
static void on_socket_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  JSNetConnection* conn = (JSNetConnection*)stream->data;
  if (!conn || !conn->ctx) {
    goto cleanup;
  }

  JSContext* ctx = conn->ctx;

  // Check if socket object is still valid (not freed by GC)
  if (JS_IsUndefined(conn->socket_obj) || JS_IsNull(conn->socket_obj)) {
    goto cleanup;
  }

  if (nread < 0) {
    if (nread != UV_EOF) {
      // Emit error event
      JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
      if (JS_IsFunction(ctx, emit)) {
        JSValue error = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, uv_strerror(nread)));
        JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(nread)));

        JSValue args[] = {JS_NewString(ctx, "error"), error};
        JS_Call(ctx, emit, conn->socket_obj, 2, args);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
      }
      JS_FreeValue(ctx, emit);
    }

    // Close the connection
    uv_close((uv_handle_t*)stream, NULL);
    conn->connected = false;
  } else if (nread > 0) {
    // Emit 'data' event with the buffer content
    JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
    if (JS_IsFunction(ctx, emit)) {
      // Create a string from the buffer data
      JSValue data = JS_NewStringLen(ctx, buf->base, nread);
      JSValue args[] = {JS_NewString(ctx, "data"), data};
      JSValue result = JS_Call(ctx, emit, conn->socket_obj, 2, args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
    }
    JS_FreeValue(ctx, emit);
  }

cleanup:

  // Always free the buffer allocated in on_socket_alloc
  if (buf->base) {
    free(buf->base);
  }
}

// Allocation callback for socket reads
static void on_socket_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

// Connection callbacks
static void on_connection(uv_stream_t* server, int status) {
  JSNetServer* server_data = (JSNetServer*)server->data;
  if (status < 0) {
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

  // Accept the connection - use the same event loop as the server
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  uv_tcp_init(rt->uv_loop, &conn->handle);
  conn->handle.data = conn;

  if (uv_accept(server, (uv_stream_t*)&conn->handle) == 0) {
    conn->connected = true;

    // Start reading from the socket to enable data events
    uv_read_start((uv_stream_t*)&conn->handle, on_socket_alloc, on_socket_read);

    // Emit 'connection' event on server
    JSValue emit = JS_GetPropertyStr(ctx, server_data->server_obj, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue args[] = {JS_NewString(ctx, "connection"), socket};
      JSValue result = JS_Call(ctx, emit, server_data->server_obj, 2, args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, emit);
  }

  JS_FreeValue(ctx, socket);
}

static void on_connect(uv_connect_t* req, int status) {
  JSNetConnection* conn = (JSNetConnection*)req->data;
  if (!conn || !conn->ctx) {
    return;
  }

  JSContext* ctx = conn->ctx;

  // Check if socket object is still valid (not freed by GC)
  if (JS_IsUndefined(conn->socket_obj) || JS_IsNull(conn->socket_obj)) {
    return;
  }

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

// Async callback timer for listen() callback
static void on_listen_callback_timer(uv_timer_t* timer) {
  JSNetServer* server = (JSNetServer*)timer->data;
  if (!server || JS_IsUndefined(server->listen_callback)) {
    return;
  }

  JSContext* ctx = server->ctx;
  JSValue callback = server->listen_callback;

  // Call the callback asynchronously
  JSValue result = JS_Call(ctx, callback, JS_UNDEFINED, 0, NULL);
  if (JS_IsException(result)) {
    // Handle exception but don't crash
    JSValue exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
  }
  JS_FreeValue(ctx, result);

  // Clean up callback and timer
  JS_FreeValue(ctx, server->listen_callback);
  server->listen_callback = JS_UNDEFINED;
  uv_timer_stop(&server->callback_timer);
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
  if (port < 0 || port > 65535) {
    return JS_ThrowRangeError(ctx, "Port must be between 0 and 65535");
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

  // Initialize TCP handle with the correct event loop
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  uv_tcp_init(rt->uv_loop, &conn->handle);
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

// Async write completion callback
static void on_socket_write_complete(uv_write_t* req, int status) {
  // Free the write request and buffer data
  if (req->data) {
    free(req->data);  // Free the buffer data
  }
  free(req);
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

  size_t len = strlen(data);

  // Allocate buffer that will persist during async write
  char* buffer = malloc(len);
  memcpy(buffer, data, len);
  JS_FreeCString(ctx, data);

  // Create write request
  uv_write_t* write_req = malloc(sizeof(uv_write_t));
  write_req->data = buffer;  // Store buffer for cleanup

  uv_buf_t buf = uv_buf_init(buffer, len);

  // Perform async write
  int result = uv_write(write_req, (uv_stream_t*)&conn->handle, &buf, 1, on_socket_write_complete);

  if (result < 0) {
    free(buffer);
    free(write_req);
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
  if (port < 0 || port > 65535) {
    return JS_ThrowRangeError(ctx, "Port must be between 0 and 65535");
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

  // Initialize TCP server with the correct event loop
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  uv_tcp_init(rt->uv_loop, &server->handle);
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

  // Schedule callback for async execution if provided
  if (argc > 2 && JS_IsFunction(ctx, argv[2])) {
    // Store callback for async execution
    server->listen_callback = JS_DupValue(ctx, argv[2]);

    // Initialize timer for next tick callback execution
    JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
    uv_timer_init(rt->uv_loop, &server->callback_timer);
    server->callback_timer.data = server;

    // Start timer with 0 delay for next tick execution
    uv_timer_start(&server->callback_timer, on_listen_callback_timer, 0, 0);
  } else {
    server->listen_callback = JS_UNDEFINED;
  }

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

// Close callback for socket cleanup
static void socket_close_callback(uv_handle_t* handle) {
  // Do not free memory here to avoid race conditions with uv_walk
  // Memory will be freed during runtime cleanup
  (void)handle;
}

// Class finalizers
static void js_socket_finalizer(JSRuntime* rt, JSValue val) {
  JSNetConnection* conn = JS_GetOpaque(val, js_socket_class_id);
  if (conn) {
    // Mark socket object as invalid to prevent use-after-free in callbacks
    conn->socket_obj = JS_UNDEFINED;

    // Only close if not already closing - memory will be freed during runtime cleanup
    if (!uv_is_closing((uv_handle_t*)&conn->handle)) {
      uv_close((uv_handle_t*)&conn->handle, socket_close_callback);
    }
    // Do not free memory here - it will be freed during runtime cleanup
  }
}

// Close callback for server cleanup
static void server_close_callback(uv_handle_t* handle) {
  // Do not free memory here to avoid race conditions with uv_walk
  // Memory will be freed during runtime cleanup
  (void)handle;
}

static void js_server_finalizer(JSRuntime* rt, JSValue val) {
  JSNetServer* server = JS_GetOpaque(val, js_server_class_id);
  if (server) {
    // Clean up callback if it exists
    if (!JS_IsUndefined(server->listen_callback)) {
      JS_FreeValueRT(rt, server->listen_callback);
      uv_timer_stop(&server->callback_timer);
    }

    if (server->listening && !uv_is_closing((uv_handle_t*)&server->handle)) {
      // Use proper close callback - memory will be freed during runtime cleanup
      uv_close((uv_handle_t*)&server->handle, server_close_callback);
    }
    // Do not free memory here - it will be freed during runtime cleanup
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

  // Initialize libuv handle - CRITICAL for memory safety
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  uv_tcp_init(rt->uv_loop, &conn->handle);
  conn->handle.data = conn;

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
  server->listen_callback = JS_UNDEFINED;

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

  // Export individual functions with proper memory management
  JSValue createServer = JS_GetPropertyStr(ctx, net_module, "createServer");
  JS_SetModuleExport(ctx, m, "createServer", JS_DupValue(ctx, createServer));
  JS_FreeValue(ctx, createServer);

  JSValue connect = JS_GetPropertyStr(ctx, net_module, "connect");
  JS_SetModuleExport(ctx, m, "connect", JS_DupValue(ctx, connect));
  JS_FreeValue(ctx, connect);

  JSValue Socket = JS_GetPropertyStr(ctx, net_module, "Socket");
  JS_SetModuleExport(ctx, m, "Socket", JS_DupValue(ctx, Socket));
  JS_FreeValue(ctx, Socket);

  JSValue Server = JS_GetPropertyStr(ctx, net_module, "Server");
  JS_SetModuleExport(ctx, m, "Server", JS_DupValue(ctx, Server));
  JS_FreeValue(ctx, Server);

  // Also export the whole module as default
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, net_module));

  JS_FreeValue(ctx, net_module);
  return 0;
}