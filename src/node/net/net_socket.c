#include "net_internal.h"

// Socket methods
JSValue js_socket_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return JS_UNDEFINED;
  }

  // Don't manually close the handle here - let the finalizer handle cleanup
  // This avoids double-close issues and use-after-free bugs
  conn->destroyed = true;
  conn->connected = false;

  // Emit 'close' event immediately (user-initiated destroy)
  JSValue emit = JS_GetPropertyStr(ctx, conn->socket_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "close"), JS_NewBool(ctx, conn->had_error)};
    JS_Call(ctx, emit, conn->socket_obj, 2, args);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
  }
  JS_FreeValue(ctx, emit);

  return JS_UNDEFINED;
}

JSValue js_socket_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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
    if (conn->timeout_enabled && conn->timeout_timer_initialized) {
      uv_timer_stop(conn->timeout_timer);
      conn->timeout_enabled = false;
    }
  } else {
    conn->timeout_ms = timeout;
    conn->timeout_enabled = true;

    // Allocate and initialize timer if not already initialized
    if (!conn->timeout_timer_initialized) {
      JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
      conn->timeout_timer = (uv_timer_t*)malloc(sizeof(uv_timer_t));
      if (!conn->timeout_timer) {
        return JS_ThrowOutOfMemory(ctx);
      }
      uv_timer_init(rt->uv_loop, conn->timeout_timer);
      conn->timeout_timer->data = conn;
      conn->timeout_timer_initialized = true;
    }

    // Start/restart the timeout timer
    uv_timer_start(conn->timeout_timer, on_socket_timeout, timeout, 0);
  }

  return this_val;
}

JSValue js_socket_set_keep_alive(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_set_no_delay(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  if (conn->connected) {
    uv_ref((uv_handle_t*)&conn->handle);
  }

  return this_val;
}

JSValue js_socket_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  if (conn->connected) {
    uv_unref((uv_handle_t*)&conn->handle);
  }

  return this_val;
}

JSValue js_socket_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_socket_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSNetConnection* conn = calloc(1, sizeof(JSNetConnection));
  if (!conn) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  conn->type_tag = NET_TYPE_SOCKET;  // Set type tag for cleanup callback
  conn->ctx = ctx;
  conn->socket_obj = JS_DupValue(ctx, obj);
  conn->connected = false;
  conn->destroyed = false;
  conn->connecting = false;
  conn->paused = false;
  conn->in_callback = false;
  conn->timeout_enabled = false;
  conn->timeout_timer_initialized = false;
  conn->timeout_timer = NULL;
  conn->close_count = 0;
  conn->timeout_ms = 0;
  conn->bytes_read = 0;
  conn->bytes_written = 0;
  conn->had_error = false;

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
  JS_SetPropertyStr(ctx, obj, "address", JS_NewCFunction(ctx, js_socket_address, "address", 0));

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
