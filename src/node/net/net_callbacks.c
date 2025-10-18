#include "../../../src/util/debug.h"
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
    js_net_connection_clear_pending_writes(conn);

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
      js_net_connection_clear_pending_writes(conn);

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
  JSRT_Debug("add_event_emitter_methods: called");
  // Get the node:events module to access EventEmitter methods
  JSValue events_module = JSRT_LoadNodeModuleCommonJS(ctx, "events");
  JSRT_Debug("add_event_emitter_methods: events_module loaded, isException=%d", JS_IsException(events_module));
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

  JSContext* ctx = conn->ctx;
  bool is_http_client = conn->is_http_client;
  JSRT_Debug_Truncated("[debug] on_socket_read nread=%zd connected=%d connecting=%d http_client=%d\n", nread,
                       conn->connected, conn->connecting, is_http_client);

  // Mark that we're in a callback to prevent finalization
  conn->in_callback = true;

  // Check if socket object is still valid (not freed by GC)
  if (JS_IsUndefined(conn->socket_obj) || JS_IsNull(conn->socket_obj)) {
    conn->in_callback = false;
    goto cleanup;
  }

  if (nread < 0) {
    if (nread == UV_EOF) {
      // Emit 'end' event when connection closes gracefully
      JSRT_Debug("on_socket_read: received EOF, emitting end event");
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

    // Emit 'close' event before closing handle (while socket_obj is still valid)
    JSRT_Debug("on_socket_read: emitting close event");
    JSValue emit_close = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
    if (JS_IsFunction(ctx, emit_close)) {
      JSValue args[] = {JS_NewString(ctx, "close"), JS_NewBool(ctx, conn->had_error)};
      JS_Call(ctx, emit_close, conn->socket_obj, 2, args);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
    }
    JS_FreeValue(ctx, emit_close);

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
      // For now, always return strings to maintain compatibility
      // TODO: Implement proper Buffer support when encoding is not set
      // This requires caching the Buffer constructor to avoid module loading during callbacks
      JSValue data = JS_NewStringLen(ctx, buf->base, nread);
      JSValue args[] = {JS_NewString(ctx, "data"), data};
      JSValue result = JS_Call(ctx, emit, conn->socket_obj, 2, args);
      if (JS_IsException(result)) {
        JSValue exception = JS_GetException(ctx);
        const char* err_str = JS_ToCString(ctx, exception);
        JSRT_Debug_Truncated("[debug] emit(data) exception: %s\n", err_str);
        JS_FreeCString(ctx, err_str);
        JS_FreeValue(ctx, exception);
      }
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
  JSRT_Debug("on_connect: CALLED with status=%d", status);
  JSNetConnection* conn = (JSNetConnection*)req->data;
  if (!conn || !conn->ctx || conn->destroyed) {
    JSRT_Debug("on_connect: early return - conn=%p destroyed=%d", conn, conn ? conn->destroyed : -1);
    return;
  }

  // Check if socket object is still valid (not freed by GC) BEFORE setting in_callback
  // If GC happened between connect start and this callback, socket_obj will be JS_UNDEFINED
  if (JS_IsUndefined(conn->socket_obj) || JS_IsNull(conn->socket_obj)) {
    JSRT_Debug("on_connect: socket was garbage collected, cleaning up connection");
    conn->connecting = false;
    conn->connected = false;
    // Close the handle to free resources
    if (!uv_is_closing((uv_handle_t*)&conn->handle)) {
      if (conn->close_count == 0) {
        conn->close_count = 1;
      }
      conn->handle.data = conn;
      uv_close((uv_handle_t*)&conn->handle, socket_close_callback);
    }
    return;
  }

  // Mark that we're in a callback to prevent finalization
  conn->in_callback = true;

  JSContext* ctx = conn->ctx;

  if (status == 0) {
    JSRT_Debug("on_connect: connection established");
    conn->connected = true;
    conn->connecting = false;

    // Start reading from the socket to enable data events
    uv_read_start((uv_stream_t*)&conn->handle, on_socket_alloc, on_socket_read);
    JSRT_Debug("on_connect: uv_read_start called");

    // Flush any pending writes queued before the connection completed
    JSPendingWrite* pending = js_net_connection_detach_pending_writes(conn);
    bool flush_failed = false;
    int flush_error_code = 0;
    const char* flush_error_message = NULL;

    while (pending) {
      JSPendingWrite* next = pending->next;
      uv_write_t* write_req = malloc(sizeof(uv_write_t));
      if (!write_req) {
        flush_failed = true;
        flush_error_code = UV_ENOMEM;
        flush_error_message = uv_strerror(UV_ENOMEM);
        if (pending->data) {
          free(pending->data);
        }
      } else {
        write_req->data = pending->data;
        uv_buf_t buf = uv_buf_init(pending->data, (unsigned int)pending->len);
        int write_result = uv_write(write_req, (uv_stream_t*)&conn->handle, &buf, 1, on_socket_write_complete);
        if (write_result < 0) {
          flush_failed = true;
          flush_error_code = write_result;
          flush_error_message = uv_strerror(write_result);
          free(pending->data);
          free(write_req);
        } else {
          conn->bytes_written += pending->len;
        }
      }

      free(pending);
      pending = next;

      if (flush_failed) {
        js_net_connection_free_pending_list(pending);
        pending = NULL;
      }
    }

    if (flush_failed) {
      conn->had_error = true;
      if (!JS_IsUndefined(conn->socket_obj)) {
        JSValue emit_err = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
        if (JS_IsFunction(ctx, emit_err)) {
          JSValue error = JS_NewError(ctx);
          if (flush_error_message) {
            JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, flush_error_message));
          }
          if (flush_error_code != 0) {
            JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(flush_error_code)));
          }

          JSValue args[] = {JS_NewString(ctx, "error"), error};
          JS_Call(ctx, emit_err, conn->socket_obj, 2, args);
          JS_FreeValue(ctx, args[0]);
          JS_FreeValue(ctx, args[1]);
        }
        JS_FreeValue(ctx, emit_err);
      }
    }

    // Emit 'connect' event
    JSRT_Debug("on_connect: emitting connect event, socket_obj valid=%d",
               !JS_IsUndefined(conn->socket_obj) && !JS_IsNull(conn->socket_obj));
    JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");

    // Debug: check what emit actually is
    const char* emit_str = JS_ToCString(ctx, emit);
    JSRT_Debug("on_connect: emit value = '%s', tag=%d, isFunction=%d, isUndefined=%d, isNull=%d",
               emit_str ? emit_str : "NULL", JS_VALUE_GET_TAG(emit), JS_IsFunction(ctx, emit), JS_IsUndefined(emit),
               JS_IsNull(emit));
    if (emit_str)
      JS_FreeCString(ctx, emit_str);

    if (JS_IsFunction(ctx, emit)) {
      JSValue args[] = {JS_NewString(ctx, "connect")};
      JSValue result = JS_Call(ctx, emit, conn->socket_obj, 1, args);
      if (JS_IsException(result)) {
        JSRT_Debug("on_connect: emit connect threw exception");
      }
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
    } else {
      JSRT_Debug("on_connect: emit is not a function");
    }
    JS_FreeValue(ctx, emit);
    JSRT_Debug("on_connect: connect event emitted");

    // Emit 'ready' event (socket is ready for writing)
    emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue args[] = {JS_NewString(ctx, "ready")};
      JS_Call(ctx, emit, conn->socket_obj, 1, args);
      JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, emit);

    if (conn->end_after_connect) {
      conn->shutdown_req.data = conn;
      uv_shutdown(&conn->shutdown_req, (uv_stream_t*)&conn->handle, on_shutdown);
      conn->connected = false;
      conn->end_after_connect = false;
    }
  } else {
    conn->connecting = false;
    conn->had_error = true;
    conn->end_after_connect = false;
    js_net_connection_clear_pending_writes(conn);

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
  // Get the connection from the stream BEFORE freeing req
  JSNetConnection* conn = (JSNetConnection*)req->handle->data;

  // Free the write request
  if (req->data) {
    free(req->data);  // Free the copied buffer
  }
  free(req);
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

// Shutdown callback - called after uv_shutdown completes
void on_shutdown(uv_shutdown_t* req, int status) {
  JSNetConnection* conn = (JSNetConnection*)req->data;
  if (!conn || !conn->ctx || conn->destroyed) {
    return;
  }

  // After shutdown, the peer will receive EOF and handle close on their side
  // We don't emit events here - they will be emitted when we receive EOF
  // from the peer or when the connection is fully closed
  // Just mark that we've shut down our write side
  conn->connected = false;
}
