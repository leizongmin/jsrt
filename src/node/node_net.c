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
  uv_timer_t timeout_timer;
  char* host;
  int port;
  bool connected;
  bool destroyed;
  bool connecting;
  bool paused;
  bool in_callback;  // Prevent finalization during callback execution
  bool timeout_enabled;
  unsigned int timeout_ms;
  size_t bytes_read;
  size_t bytes_written;
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
  int connection_count;       // Track number of active connections
  JSValue listen_callback;    // Store callback for async execution
  uv_timer_t callback_timer;  // Timer for async callback
} JSNetServer;

// Forward declarations
static JSValue js_net_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_net_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
static JSValue js_socket_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
static JSValue js_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
static void on_listen_callback_timer(uv_timer_t* timer);
static void on_socket_timeout(uv_timer_t* timer);

// Data read callback
static void on_socket_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  JSNetConnection* conn = (JSNetConnection*)stream->data;
  if (!conn || !conn->ctx || conn->destroyed) {
    goto cleanup;
  }

  // Mark that we're in a callback to prevent finalization
  conn->in_callback = true;

  JSContext* ctx = conn->ctx;

  // Check if socket object is still valid (not freed by GC)
  if (JS_IsUndefined(conn->socket_obj) || JS_IsNull(conn->socket_obj)) {
    conn->in_callback = false;
    goto cleanup;
  }

  if (nread < 0) {
    if (nread == UV_EOF) {
      // Emit 'end' event when connection closes gracefully
      JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
      if (JS_IsFunction(ctx, emit)) {
        JSValue args[] = {JS_NewString(ctx, "end")};
        JS_Call(ctx, emit, conn->socket_obj, 1, args);
        JS_FreeValue(ctx, args[0]);
      }
      JS_FreeValue(ctx, emit);
    } else {
      // Emit error event for actual errors
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
    // Increment bytes read counter
    conn->bytes_read += nread;

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

  // Clear callback flag
  if (conn) {
    conn->in_callback = false;
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

    // Increment connection count
    server_data->connection_count++;

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
  if (!conn || !conn->ctx || conn->destroyed) {
    return;
  }

  // Mark that we're in a callback to prevent finalization
  conn->in_callback = true;

  JSContext* ctx = conn->ctx;

  // Check if socket object is still valid (not freed by GC)
  if (JS_IsUndefined(conn->socket_obj) || JS_IsNull(conn->socket_obj)) {
    conn->in_callback = false;
    return;
  }

  if (status == 0) {
    conn->connected = true;
    conn->connecting = false;

    // Emit 'connect' event
    JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue args[] = {JS_NewString(ctx, "connect")};
      JS_Call(ctx, emit, conn->socket_obj, 1, args);
      JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, emit);

    // Emit 'ready' event (socket is ready for writing)
    emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue args[] = {JS_NewString(ctx, "ready")};
      JS_Call(ctx, emit, conn->socket_obj, 1, args);
      JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, emit);
  } else {
    conn->connecting = false;

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

  // Clear callback flag
  conn->in_callback = false;
}

// Timeout callback for socket
static void on_socket_timeout(uv_timer_t* timer) {
  JSNetConnection* conn = (JSNetConnection*)timer->data;
  if (!conn || !conn->ctx || conn->destroyed) {
    return;
  }

  JSContext* ctx = conn->ctx;

  // Check if socket object is still valid
  if (JS_IsUndefined(conn->socket_obj) || JS_IsNull(conn->socket_obj)) {
    return;
  }

  // Emit 'timeout' event
  JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "timeout")};
    JS_Call(ctx, emit, conn->socket_obj, 1, args);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);
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

  // Set connecting state
  conn->connecting = true;

  // Resolve address and connect
  // NOTE: uv_ip4_addr expects a numeric IPv4 address, not a hostname
  // TODO: Implement proper hostname resolution with uv_getaddrinfo for full Node.js compatibility
  struct sockaddr_in addr;
  uv_ip4_addr(conn->host, conn->port, &addr);

  int result = uv_tcp_connect(&conn->connect_req, &conn->handle, (const struct sockaddr*)&addr, on_connect);

  if (result < 0) {
    conn->connecting = false;
    uv_close((uv_handle_t*)&conn->handle, NULL);
    return JS_ThrowInternalError(ctx, "Failed to connect: %s", uv_strerror(result));
  }

  return this_val;  // Return this for chaining
}

// Async write completion callback
static void on_socket_write_complete(uv_write_t* req, int status) {
  // Get connection for updating bytes_written
  JSNetConnection* conn = (JSNetConnection*)req->handle->data;

  // Free the write request and buffer data
  if (req->data) {
    free(req->data);  // Free the buffer data
  }
  free(req);

  // Emit 'drain' event if write queue is now empty
  if (conn && conn->ctx && !conn->destroyed) {
    size_t queue_size = uv_stream_get_write_queue_size((uv_stream_t*)&conn->handle);
    if (queue_size == 0 && !JS_IsUndefined(conn->socket_obj)) {
      JSContext* ctx = conn->ctx;
      JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
      if (JS_IsFunction(ctx, emit)) {
        JSValue args[] = {JS_NewString(ctx, "drain")};
        JS_Call(ctx, emit, conn->socket_obj, 1, args);
        JS_FreeValue(ctx, args[0]);
      }
      JS_FreeValue(ctx, emit);
    }
  }
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

  // Increment bytes written counter
  conn->bytes_written += len;

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

static JSValue js_socket_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  if (!conn->paused && conn->connected) {
    // Stop reading from the socket
    uv_read_stop((uv_stream_t*)&conn->handle);
    conn->paused = true;
  }

  return this_val;
}

static JSValue js_socket_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  if (conn->paused && conn->connected) {
    // Resume reading from the socket
    uv_read_start((uv_stream_t*)&conn->handle, on_socket_alloc, on_socket_read);
    conn->paused = false;
  }

  return this_val;
}

static JSValue js_socket_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "setTimeout requires timeout argument");
  }

  int timeout;
  if (JS_ToInt32(ctx, &timeout, argv[0])) {
    return JS_EXCEPTION;
  }

  if (timeout == 0) {
    // Disable timeout
    if (conn->timeout_enabled) {
      uv_timer_stop(&conn->timeout_timer);
      conn->timeout_enabled = false;
    }
  } else {
    conn->timeout_ms = timeout;
    conn->timeout_enabled = true;

    // Initialize timer if not already initialized
    if (!uv_is_active((uv_handle_t*)&conn->timeout_timer)) {
      JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
      uv_timer_init(rt->uv_loop, &conn->timeout_timer);
      conn->timeout_timer.data = conn;
    }

    // Start/restart the timeout timer
    uv_timer_start(&conn->timeout_timer, on_socket_timeout, timeout, 0);
  }

  return this_val;
}

static JSValue js_socket_set_keep_alive(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  bool enable = true;
  unsigned int delay = 0;

  if (argc > 0) {
    enable = JS_ToBool(ctx, argv[0]);
  }

  if (argc > 1 && enable) {
    int delay_val;
    if (JS_ToInt32(ctx, &delay_val, argv[1]) == 0) {
      delay = (unsigned int)delay_val / 1000;  // Convert ms to seconds for libuv
    }
  }

  // Set TCP keepalive
  int result = uv_tcp_keepalive(&conn->handle, enable ? 1 : 0, delay);
  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to set keepalive: %s", uv_strerror(result));
  }

  return this_val;
}

static JSValue js_socket_set_no_delay(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  bool enable = true;
  if (argc > 0) {
    enable = JS_ToBool(ctx, argv[0]);
  }

  // Set TCP_NODELAY (Nagle's algorithm)
  int result = uv_tcp_nodelay(&conn->handle, enable ? 1 : 0);
  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to set nodelay: %s", uv_strerror(result));
  }

  return this_val;
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
    uv_close((uv_handle_t*)&server->handle, NULL);
    return JS_ThrowInternalError(ctx, "Bind failed: %s", uv_strerror(result));
  }

  result = uv_listen((uv_stream_t*)&server->handle, 128, on_connection);
  if (result < 0) {
    uv_close((uv_handle_t*)&server->handle, NULL);
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

static JSValue js_server_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || !server->listening) {
    return JS_NULL;
  }

  struct sockaddr_storage addr;
  int addrlen = sizeof(addr);
  int r = uv_tcp_getsockname(&server->handle, (struct sockaddr*)&addr, &addrlen);
  if (r != 0) {
    return JS_NULL;
  }

  // Create address object { address: string, family: string, port: number }
  JSValue obj = JS_NewObject(ctx);

  char ip[INET6_ADDRSTRLEN];
  int port = 0;
  const char* family = NULL;

  if (addr.ss_family == AF_INET) {
    struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr;
    uv_ip4_name(addr4, ip, sizeof(ip));
    port = ntohs(addr4->sin_port);
    family = "IPv4";
  } else if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr;
    uv_ip6_name(addr6, ip, sizeof(ip));
    port = ntohs(addr6->sin6_port);
    family = "IPv6";
  } else {
    JS_FreeValue(ctx, obj);
    return JS_NULL;
  }

  JS_SetPropertyStr(ctx, obj, "address", JS_NewString(ctx, ip));
  JS_SetPropertyStr(ctx, obj, "family", JS_NewString(ctx, family));
  JS_SetPropertyStr(ctx, obj, "port", JS_NewInt32(ctx, port));

  return obj;
}

static JSValue js_server_get_connections(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server) {
    return JS_UNDEFINED;
  }

  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "getConnections requires a callback function");
  }

  // Call callback with (err, count)
  JSValue callback = argv[0];
  JSValue args[2];
  args[0] = JS_NULL;  // No error
  args[1] = JS_NewInt32(ctx, server->connection_count);

  JSValue result = JS_Call(ctx, callback, this_val, 2, args);
  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, result);

  return JS_UNDEFINED;
}

static JSValue js_server_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || server->destroyed) {
    return this_val;
  }

  if (server->listening) {
    uv_ref((uv_handle_t*)&server->handle);
  }

  return this_val;
}

static JSValue js_server_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || server->destroyed) {
    return this_val;
  }

  if (server->listening) {
    uv_unref((uv_handle_t*)&server->handle);
  }

  return this_val;
}

static JSValue js_socket_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  if (conn->connected) {
    uv_ref((uv_handle_t*)&conn->handle);
  }

  return this_val;
}

static JSValue js_socket_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  if (conn->connected) {
    uv_unref((uv_handle_t*)&conn->handle);
  }

  return this_val;
}

// Socket property getters
static JSValue js_socket_get_local_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || !conn->connected) {
    return JS_NULL;
  }

  struct sockaddr_storage addr;
  int addrlen = sizeof(addr);
  int r = uv_tcp_getsockname(&conn->handle, (struct sockaddr*)&addr, &addrlen);
  if (r != 0) {
    return JS_NULL;
  }

  char ip[INET6_ADDRSTRLEN];
  if (addr.ss_family == AF_INET) {
    struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr;
    uv_ip4_name(addr4, ip, sizeof(ip));
  } else if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr;
    uv_ip6_name(addr6, ip, sizeof(ip));
  } else {
    return JS_NULL;
  }

  return JS_NewString(ctx, ip);
}

static JSValue js_socket_get_local_port(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || !conn->connected) {
    return JS_NULL;
  }

  struct sockaddr_storage addr;
  int addrlen = sizeof(addr);
  int r = uv_tcp_getsockname(&conn->handle, (struct sockaddr*)&addr, &addrlen);
  if (r != 0) {
    return JS_NULL;
  }

  int port = 0;
  if (addr.ss_family == AF_INET) {
    struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr;
    port = ntohs(addr4->sin_port);
  } else if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr;
    port = ntohs(addr6->sin6_port);
  }

  return JS_NewInt32(ctx, port);
}

static JSValue js_socket_get_local_family(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || !conn->connected) {
    return JS_NULL;
  }

  struct sockaddr_storage addr;
  int addrlen = sizeof(addr);
  int r = uv_tcp_getsockname(&conn->handle, (struct sockaddr*)&addr, &addrlen);
  if (r != 0) {
    return JS_NULL;
  }

  if (addr.ss_family == AF_INET) {
    return JS_NewString(ctx, "IPv4");
  } else if (addr.ss_family == AF_INET6) {
    return JS_NewString(ctx, "IPv6");
  }

  return JS_NULL;
}

static JSValue js_socket_get_remote_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || !conn->connected) {
    return JS_NULL;
  }

  struct sockaddr_storage addr;
  int addrlen = sizeof(addr);
  int r = uv_tcp_getpeername(&conn->handle, (struct sockaddr*)&addr, &addrlen);
  if (r != 0) {
    return JS_NULL;
  }

  char ip[INET6_ADDRSTRLEN];
  if (addr.ss_family == AF_INET) {
    struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr;
    uv_ip4_name(addr4, ip, sizeof(ip));
  } else if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr;
    uv_ip6_name(addr6, ip, sizeof(ip));
  } else {
    return JS_NULL;
  }

  return JS_NewString(ctx, ip);
}

static JSValue js_socket_get_remote_port(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || !conn->connected) {
    return JS_NULL;
  }

  struct sockaddr_storage addr;
  int addrlen = sizeof(addr);
  int r = uv_tcp_getpeername(&conn->handle, (struct sockaddr*)&addr, &addrlen);
  if (r != 0) {
    return JS_NULL;
  }

  int port = 0;
  if (addr.ss_family == AF_INET) {
    struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr;
    port = ntohs(addr4->sin_port);
  } else if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr;
    port = ntohs(addr6->sin6_port);
  }

  return JS_NewInt32(ctx, port);
}

static JSValue js_socket_get_remote_family(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || !conn->connected) {
    return JS_NULL;
  }

  struct sockaddr_storage addr;
  int addrlen = sizeof(addr);
  int r = uv_tcp_getpeername(&conn->handle, (struct sockaddr*)&addr, &addrlen);
  if (r != 0) {
    return JS_NULL;
  }

  if (addr.ss_family == AF_INET) {
    return JS_NewString(ctx, "IPv4");
  } else if (addr.ss_family == AF_INET6) {
    return JS_NewString(ctx, "IPv6");
  }

  return JS_NULL;
}

static JSValue js_socket_get_bytes_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewInt64(ctx, 0);
  }
  return JS_NewInt64(ctx, conn->bytes_read);
}

static JSValue js_socket_get_bytes_written(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewInt64(ctx, 0);
  }
  return JS_NewInt64(ctx, conn->bytes_written);
}

static JSValue js_socket_get_connecting(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewBool(ctx, false);
  }
  return JS_NewBool(ctx, conn->connecting);
}

static JSValue js_socket_get_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewBool(ctx, true);
  }
  return JS_NewBool(ctx, conn->destroyed);
}

static JSValue js_socket_get_pending(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewBool(ctx, false);
  }
  // pending means not yet connected (connecting or not started)
  return JS_NewBool(ctx, !conn->connected && !conn->destroyed);
}

static JSValue js_socket_get_ready_state(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewString(ctx, "closed");
  }

  if (conn->destroyed) {
    return JS_NewString(ctx, "closed");
  } else if (conn->connecting) {
    return JS_NewString(ctx, "opening");
  } else if (conn->connected) {
    return JS_NewString(ctx, "open");
  } else {
    return JS_NewString(ctx, "closed");
  }
}

static JSValue js_socket_get_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || !conn->connected) {
    return JS_NewInt32(ctx, 0);
  }

  // Get the write queue size from libuv
  size_t size = uv_stream_get_write_queue_size((uv_stream_t*)&conn->handle);
  return JS_NewInt64(ctx, size);
}

// Close callback for socket cleanup
static void socket_close_callback(uv_handle_t* handle) {
  // Free the connection struct after handle is closed
  if (handle->data) {
    JSNetConnection* conn = (JSNetConnection*)handle->data;
    if (conn->host) {
      free(conn->host);
    }
    free(conn);
    handle->data = NULL;  // Prevent double-free
  }
}

// Class finalizers
static void js_socket_finalizer(JSRuntime* rt, JSValue val) {
  JSNetConnection* conn = JS_GetOpaque(val, js_socket_class_id);
  if (conn) {
    // Mark socket object as invalid to prevent use-after-free in callbacks
    conn->socket_obj = JS_UNDEFINED;

    // Stop and close timeout timer if it's active
    if (conn->timeout_enabled) {
      uv_timer_stop(&conn->timeout_timer);
      if (!uv_is_closing((uv_handle_t*)&conn->timeout_timer)) {
        uv_close((uv_handle_t*)&conn->timeout_timer, NULL);
      }
      conn->timeout_enabled = false;
    }

    // Only close if not already closing - memory will be freed in close callback
    if (!uv_is_closing((uv_handle_t*)&conn->handle)) {
      uv_close((uv_handle_t*)&conn->handle, socket_close_callback);
    } else if (conn->handle.data) {
      // Handle already closing, but not yet freed - will be freed by close callback
      // Do nothing here
    } else {
      // Handle already closed and freed, or never initialized
      // This shouldn't happen, but handle it safely
    }
  }
}

// Close callback for server cleanup
static void server_close_callback(uv_handle_t* handle) {
  // Free the server struct after handle is closed
  if (handle->data) {
    JSNetServer* server = (JSNetServer*)handle->data;
    if (server->host) {
      free(server->host);
    }
    free(server);
    handle->data = NULL;  // Prevent double-free
  }
}

static void js_server_finalizer(JSRuntime* rt, JSValue val) {
  JSNetServer* server = JS_GetOpaque(val, js_server_class_id);
  if (server) {
    // Clean up callback and timer if they exist
    if (!JS_IsUndefined(server->listen_callback)) {
      JS_FreeValueRT(rt, server->listen_callback);
      server->listen_callback = JS_UNDEFINED;
      uv_timer_stop(&server->callback_timer);
      if (!uv_is_closing((uv_handle_t*)&server->callback_timer)) {
        uv_close((uv_handle_t*)&server->callback_timer, NULL);
      }
    }

    // Close server handle if still listening - memory will be freed in close callback
    if (server->listening && !uv_is_closing((uv_handle_t*)&server->handle)) {
      uv_close((uv_handle_t*)&server->handle, server_close_callback);
    } else if (server->handle.data) {
      // Handle already closing, but not yet freed - will be freed by close callback
      // Do nothing here
    } else {
      // Handle already closed and freed, or never initialized
      // This shouldn't happen, but handle it safely
    }
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
  conn->connecting = false;
  conn->paused = false;
  conn->in_callback = false;
  conn->timeout_enabled = false;
  conn->timeout_ms = 0;
  conn->bytes_read = 0;
  conn->bytes_written = 0;

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
  JS_SetPropertyStr(ctx, obj, "pause", JS_NewCFunction(ctx, js_socket_pause, "pause", 0));
  JS_SetPropertyStr(ctx, obj, "resume", JS_NewCFunction(ctx, js_socket_resume, "resume", 0));
  JS_SetPropertyStr(ctx, obj, "setTimeout", JS_NewCFunction(ctx, js_socket_set_timeout, "setTimeout", 1));
  JS_SetPropertyStr(ctx, obj, "setKeepAlive", JS_NewCFunction(ctx, js_socket_set_keep_alive, "setKeepAlive", 2));
  JS_SetPropertyStr(ctx, obj, "setNoDelay", JS_NewCFunction(ctx, js_socket_set_no_delay, "setNoDelay", 1));
  JS_SetPropertyStr(ctx, obj, "ref", JS_NewCFunction(ctx, js_socket_ref, "ref", 0));
  JS_SetPropertyStr(ctx, obj, "unref", JS_NewCFunction(ctx, js_socket_unref, "unref", 0));

// Add property getters using JS_DefinePropertyGetSet
#define DEFINE_GETTER_PROP(name, func)                                                                        \
  do {                                                                                                        \
    JSAtom atom = JS_NewAtom(ctx, name);                                                                      \
    JSValue getter = JS_NewCFunction(ctx, func, "get " name, 0);                                              \
    JS_DefinePropertyGetSet(ctx, obj, atom, getter, JS_UNDEFINED, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE); \
    JS_FreeAtom(ctx, atom);                                                                                   \
  } while (0)

  DEFINE_GETTER_PROP("localAddress", js_socket_get_local_address);
  DEFINE_GETTER_PROP("localPort", js_socket_get_local_port);
  DEFINE_GETTER_PROP("localFamily", js_socket_get_local_family);
  DEFINE_GETTER_PROP("remoteAddress", js_socket_get_remote_address);
  DEFINE_GETTER_PROP("remotePort", js_socket_get_remote_port);
  DEFINE_GETTER_PROP("remoteFamily", js_socket_get_remote_family);
  DEFINE_GETTER_PROP("bytesRead", js_socket_get_bytes_read);
  DEFINE_GETTER_PROP("bytesWritten", js_socket_get_bytes_written);
  DEFINE_GETTER_PROP("connecting", js_socket_get_connecting);
  DEFINE_GETTER_PROP("destroyed", js_socket_get_destroyed);
  DEFINE_GETTER_PROP("pending", js_socket_get_pending);
  DEFINE_GETTER_PROP("readyState", js_socket_get_ready_state);
  DEFINE_GETTER_PROP("bufferSize", js_socket_get_buffer_size);

#undef DEFINE_GETTER_PROP

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
  server->connection_count = 0;
  server->listen_callback = JS_UNDEFINED;

  JS_SetOpaque(obj, server);

  // Add server methods
  JS_SetPropertyStr(ctx, obj, "listen", JS_NewCFunction(ctx, js_server_listen, "listen", 3));
  JS_SetPropertyStr(ctx, obj, "close", JS_NewCFunction(ctx, js_server_close, "close", 0));
  JS_SetPropertyStr(ctx, obj, "address", JS_NewCFunction(ctx, js_server_address, "address", 0));
  JS_SetPropertyStr(ctx, obj, "getConnections", JS_NewCFunction(ctx, js_server_get_connections, "getConnections", 1));
  JS_SetPropertyStr(ctx, obj, "ref", JS_NewCFunction(ctx, js_server_ref, "ref", 0));
  JS_SetPropertyStr(ctx, obj, "unref", JS_NewCFunction(ctx, js_server_unref, "unref", 0));

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