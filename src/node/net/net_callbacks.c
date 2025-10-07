#include "net_internal.h"

// DNS resolution callback - called after uv_getaddrinfo completes
void on_getaddrinfo(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  JSNetConnection* conn = (JSNetConnection*)req->data;

  // Check if connection is still valid BEFORE using res
  if (!conn || !conn->ctx || conn->destroyed || uv_is_closing((uv_handle_t*)&conn->handle)) {
    // Connection destroyed or being destroyed, just free result and return
    if (res) {
      uv_freeaddrinfo(res);
    }
    return;
  }

  JSContext* ctx = conn->ctx;

  if (status < 0) {
    // DNS lookup failed - emit error event
    conn->connecting = false;
    conn->had_error = true;

    if (!JS_IsUndefined(conn->socket_obj)) {
      JSValue error = JS_NewError(ctx);
      JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, uv_strerror(status)),
                                JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
      JS_DefinePropertyValueStr(ctx, error, "code", JS_NewString(ctx, "ENOTFOUND"),
                                JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
      JS_DefinePropertyValueStr(ctx, error, "syscall", JS_NewString(ctx, "getaddrinfo"),
                                JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
      JS_DefinePropertyValueStr(ctx, error, "hostname", JS_NewString(ctx, conn->host),
                                JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

      JSValue argv[] = {JS_NewString(ctx, "error"), error};
      JSValue emit_func = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
      if (JS_IsFunction(ctx, emit_func)) {
        JS_Call(ctx, emit_func, conn->socket_obj, 2, argv);
      }
      JS_FreeValue(ctx, emit_func);
      JS_FreeValue(ctx, argv[0]);
      JS_FreeValue(ctx, argv[1]);
    }

    if (res) {
      uv_freeaddrinfo(res);
    }
    return;
  }

  // DNS lookup succeeded - use first address to connect
  if (res && res->ai_addr) {
    struct sockaddr_storage addr_storage;
    if (res->ai_family == AF_INET) {
      memcpy(&addr_storage, res->ai_addr, sizeof(struct sockaddr_in));
      // Set port
      ((struct sockaddr_in*)&addr_storage)->sin_port = htons(conn->port);
    } else if (res->ai_family == AF_INET6) {
      memcpy(&addr_storage, res->ai_addr, sizeof(struct sockaddr_in6));
      // Set port
      ((struct sockaddr_in6*)&addr_storage)->sin6_port = htons(conn->port);
    }

    // Free the DNS result before attempting to connect
    uv_freeaddrinfo(res);
    res = NULL;

    int result = uv_tcp_connect(&conn->connect_req, &conn->handle, (struct sockaddr*)&addr_storage, on_connect);

    if (result < 0) {
      // Connection failed - emit error
      conn->connecting = false;
      conn->had_error = true;

      if (!JS_IsUndefined(conn->socket_obj)) {
        JSValue error = JS_NewError(ctx);
        JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, uv_strerror(result)),
                                  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);

        JSValue argv[] = {JS_NewString(ctx, "error"), error};
        JSValue emit_func = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
        if (JS_IsFunction(ctx, emit_func)) {
          JS_Call(ctx, emit_func, conn->socket_obj, 2, argv);
        }
        JS_FreeValue(ctx, emit_func);
        JS_FreeValue(ctx, argv[0]);
        JS_FreeValue(ctx, argv[1]);
      }
    }
  } else {
    // Free result if we didn't use it
    if (res) {
      uv_freeaddrinfo(res);
    }
  }
}

// Helper function to add EventEmitter methods to an object
void add_event_emitter_methods(JSContext* ctx, JSValue obj) {
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

// Data read callback
void on_socket_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
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
      conn->had_error = true;
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

    // NOTE: 'close' event emission deferred - requires more complex lifecycle management
    // to avoid use-after-free with libuv's internal queue handling

    // Close the connection - only if not already closing
    if (!uv_is_closing((uv_handle_t*)stream)) {
      if (conn->close_count == 0) {
        conn->close_count = 1;  // Only set if not already set by finalizer
      }
      conn->handle.data = conn;
      uv_close((uv_handle_t*)stream, socket_close_callback);
    }
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
void on_socket_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

// Connection callbacks
void on_connection(uv_stream_t* server, int status) {
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

void on_connect(uv_connect_t* req, int status) {
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
    conn->had_error = true;

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
void on_socket_timeout(uv_timer_t* timer) {
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
void on_listen_callback_timer(uv_timer_t* timer) {
  JSNetServer* server = (JSNetServer*)timer->data;
  if (!server || JS_IsUndefined(server->listen_callback)) {
    return;
  }

  JSContext* ctx = server->ctx;
  JSValue callback = server->listen_callback;

  // Set flag to prevent finalizer from freeing callback
  server->in_callback = true;

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
  if (server->timer_initialized && server->callback_timer) {
    uv_timer_stop(server->callback_timer);
  }

  server->in_callback = false;
}

void on_socket_write_complete(uv_write_t* req, int status) {
  // Free the write request
  if (req->data) {
    free(req->data);  // Free the copied buffer
  }
  free(req);

  // Get the connection from the stream
  JSNetConnection* conn = (JSNetConnection*)req->handle->data;
  if (!conn || !conn->ctx || conn->destroyed) {
    return;
  }

  // Check write queue size and emit 'drain' if empty
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
