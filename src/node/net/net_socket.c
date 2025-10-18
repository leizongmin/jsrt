#include "../../util/debug.h"
#include "net_internal.h"

bool js_net_connection_queue_write(JSNetConnection* conn, const char* data, size_t len) {
  if (!conn || data == NULL) {
    return false;
  }

  // Allocate node for pending write
  JSPendingWrite* item = malloc(sizeof(JSPendingWrite));
  if (!item) {
    return false;
  }

  // Ensure we always allocate at least one byte to avoid malloc(0)
  size_t alloc_len = len > 0 ? len : 1;
  char* copy = malloc(alloc_len);
  if (!copy) {
    free(item);
    return false;
  }

  if (len > 0) {
    memcpy(copy, data, len);
  } else {
    copy[0] = '\0';
  }

  item->data = copy;
  item->len = len;
  item->next = NULL;

  if (!conn->pending_writes_head) {
    conn->pending_writes_head = item;
    conn->pending_writes_tail = item;
  } else {
    conn->pending_writes_tail->next = item;
    conn->pending_writes_tail = item;
  }

  return true;
}

JSPendingWrite* js_net_connection_detach_pending_writes(JSNetConnection* conn) {
  if (!conn) {
    return NULL;
  }

  JSPendingWrite* head = conn->pending_writes_head;
  conn->pending_writes_head = NULL;
  conn->pending_writes_tail = NULL;
  return head;
}

void js_net_connection_free_pending_list(JSPendingWrite* head) {
  JSPendingWrite* current = head;
  while (current) {
    JSPendingWrite* next = current->next;
    if (current->data) {
      free(current->data);
    }
    free(current);
    current = next;
  }
}

void js_net_connection_clear_pending_writes(JSNetConnection* conn) {
  if (!conn) {
    return;
  }

  JSPendingWrite* head = js_net_connection_detach_pending_writes(conn);
  js_net_connection_free_pending_list(head);
}

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
  if (!conn->host) {
    JS_FreeCString(ctx, host);
    return JS_ThrowOutOfMemory(ctx);
  }
  JS_FreeCString(ctx, host);

  // Map common host aliases to concrete addresses for immediate resolution
  const char* connect_host = conn->host;
  if (strcmp(conn->host, "localhost") == 0) {
    connect_host = "127.0.0.1";
  } else if (strcmp(conn->host, "ip6-localhost") == 0) {
    connect_host = "::1";
  }

  // Get runtime for uv_loop
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);

  // DON'T call uv_tcp_init() again - the handle is already initialized in the constructor
  // Calling it twice corrupts the handle state
  conn->handle.data = conn;
  conn->connect_req.data = conn;

  // Set connecting state
  conn->connecting = true;
  conn->connected = false;
  conn->end_after_connect = false;

  // Keep socket alive during connection by storing as global property
  // This prevents premature GC while connecting
  // Use a unique property name based on connection pointer
  char prop_name[64];
  snprintf(prop_name, sizeof(prop_name), "__active_socket_%p__", (void*)conn);
  JSValue global = JS_GetGlobalObject(ctx);
  JS_SetPropertyStr(ctx, global, prop_name, JS_DupValue(ctx, conn->socket_obj));
  JS_FreeValue(ctx, global);

  // Clear any pending data from previous attempts
  js_net_connection_clear_pending_writes(conn);

  // Resolve address and connect
  // Try IPv4 first, then IPv6, then use DNS resolution for hostnames
  struct sockaddr_storage addr_storage;
  struct sockaddr* addr = (struct sockaddr*)&addr_storage;
  int result;

  // Try IPv4 first
  struct sockaddr_in addr4;
  if (uv_ip4_addr(connect_host, conn->port, &addr4) == 0) {
    // IPv4 address
    JSRT_Debug("js_socket_connect: connecting to IPv4 address %s:%d", connect_host, conn->port);
    memcpy(&addr_storage, &addr4, sizeof(addr4));
    result = uv_tcp_connect(&conn->connect_req, &conn->handle, addr, on_connect);
    JSRT_Debug("js_socket_connect: uv_tcp_connect returned %d", result);
  } else {
    // Try IPv6
    struct sockaddr_in6 addr6;
    if (uv_ip6_addr(connect_host, conn->port, &addr6) == 0) {
      // IPv6 address
      memcpy(&addr_storage, &addr6, sizeof(addr6));
      result = uv_tcp_connect(&conn->connect_req, &conn->handle, addr, on_connect);
    } else {
      // Not an IP address - use DNS resolution
      struct addrinfo hints;
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_UNSPEC;      // Allow IPv4 or IPv6
      hints.ai_socktype = SOCK_STREAM;  // TCP socket
      hints.ai_protocol = IPPROTO_TCP;

      conn->getaddrinfo_req.data = conn;
      result = uv_getaddrinfo(rt->uv_loop, &conn->getaddrinfo_req, on_getaddrinfo, connect_host, NULL, &hints);

      if (result < 0) {
        conn->connecting = false;
        // Free resources before error return
        js_net_connection_clear_pending_writes(conn);
        char* error_host = conn->host;
        conn->host = NULL;
        JSValue error = JS_ThrowInternalError(ctx, "DNS lookup failed for %s: %s", error_host, uv_strerror(result));
        free(error_host);
        if (conn->encoding) {
          free(conn->encoding);
          conn->encoding = NULL;
        }
        uv_close((uv_handle_t*)&conn->handle, NULL);
        return error;
      }

      // DNS resolution started, return immediately
      return this_val;
    }
  }

  if (result < 0) {
    conn->connecting = false;
    // Free resources before error return
    js_net_connection_clear_pending_writes(conn);
    if (conn->host) {
      free(conn->host);
      conn->host = NULL;
    }
    if (conn->encoding) {
      free(conn->encoding);
      conn->encoding = NULL;
    }
    uv_close((uv_handle_t*)&conn->handle, NULL);
    return JS_ThrowInternalError(ctx, "Failed to connect: %s", uv_strerror(result));
  }

  return this_val;  // Return this for chaining
}

JSValue js_socket_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return JS_ThrowTypeError(ctx, "Socket is destroyed");
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
  size_t alloc_len = len > 0 ? len : 1;

  if (!conn->connected) {
    if (conn->connecting) {
      bool queued = js_net_connection_queue_write(conn, data, len);
      JS_FreeCString(ctx, data);
      if (!queued) {
        return JS_ThrowOutOfMemory(ctx);
      }
      return JS_NewBool(ctx, true);
    }

    JS_FreeCString(ctx, data);
    return JS_ThrowTypeError(ctx, "Socket is not connected");
  }

  // Allocate buffer that will persist during async write
  char* buffer = malloc(alloc_len);
  if (!buffer) {
    JS_FreeCString(ctx, data);
    return JS_ThrowOutOfMemory(ctx);
  }
  if (len > 0) {
    memcpy(buffer, data, len);
  }
  JS_FreeCString(ctx, data);

  // Create write request
  uv_write_t* write_req = malloc(sizeof(uv_write_t));
  if (!write_req) {
    free(buffer);
    return JS_ThrowOutOfMemory(ctx);
  }
  write_req->data = buffer;  // Store buffer for cleanup

  uv_buf_t buf = uv_buf_init(buffer, len);

  // Perform async write
  JSRT_Debug_Truncated("[debug] socket write len=%zu connected=%d connecting=%d\n", len, conn->connected,
                       conn->connecting);
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
    uv_shutdown(&conn->shutdown_req, (uv_stream_t*)&conn->handle, on_shutdown);
    conn->connected = false;
  } else if (conn->connecting) {
    conn->end_after_connect = true;
  }

  return JS_UNDEFINED;
}

JSValue js_socket_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    JSRT_Debug_Truncated("[debug] js_socket_destroy skipped conn=%p destroyed=%d\n", conn, conn ? conn->destroyed : -1);
    return JS_UNDEFINED;
  }

  JSRT_Debug_Truncated("[debug] js_socket_destroy conn=%p\n", conn);
  conn->destroyed = true;
  conn->connected = false;
  conn->connecting = false;
  conn->end_after_connect = false;

  // Drop any queued writes
  js_net_connection_clear_pending_writes(conn);

  // Close the underlying handle if still active
  if (!uv_is_closing((uv_handle_t*)&conn->handle)) {
    if (conn->close_count == 0) {
      conn->close_count = 1;
    }
    conn->handle.data = conn;
    uv_close((uv_handle_t*)&conn->handle, socket_close_callback);
  }

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

JSValue js_socket_set_encoding(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  // Free previous encoding if set
  if (conn->encoding) {
    free(conn->encoding);
    conn->encoding = NULL;
  }

  // Set new encoding if provided
  if (argc > 0 && !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0])) {
    const char* encoding = JS_ToCString(ctx, argv[0]);
    if (encoding) {
      conn->encoding = strdup(encoding);
      JS_FreeCString(ctx, encoding);
    }
  }

  return this_val;
}

JSValue js_socket_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  if (!uv_is_closing((uv_handle_t*)&conn->handle)) {
    uv_ref((uv_handle_t*)&conn->handle);
  }

  return this_val;
}

JSValue js_socket_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || conn->destroyed) {
    return this_val;
  }

  if (!uv_is_closing((uv_handle_t*)&conn->handle)) {
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
  JSRT_Debug_Truncated("[debug] new net.Socket conn=%p\n", conn);

  conn->type_tag = NET_TYPE_SOCKET;  // Set type tag for cleanup callback
  conn->ctx = ctx;
  conn->socket_obj = JS_DupValue(ctx, obj);
  conn->client_request_obj = JS_UNDEFINED;
  conn->connected = false;
  conn->destroyed = false;
  conn->connecting = false;
  conn->paused = false;
  conn->encoding = NULL;          // Default: binary/Buffer mode
  conn->allow_half_open = false;  // Default: don't allow half-open
  conn->in_callback = false;
  conn->timeout_enabled = false;
  conn->timeout_timer_initialized = false;
  conn->timeout_timer = NULL;
  conn->close_count = 0;
  conn->timeout_ms = 0;
  conn->bytes_read = 0;
  conn->bytes_written = 0;
  conn->had_error = false;
  conn->end_after_connect = false;
  conn->is_http_client = false;
  conn->pending_writes_head = NULL;
  conn->pending_writes_tail = NULL;

  // Parse constructor options if provided
  if (argc > 0 && JS_IsObject(argv[0])) {
    // allowHalfOpen option
    JSValue allow_half_open = JS_GetPropertyStr(ctx, argv[0], "allowHalfOpen");
    if (JS_IsBool(allow_half_open)) {
      conn->allow_half_open = JS_ToBool(ctx, allow_half_open);
    }
    JS_FreeValue(ctx, allow_half_open);
  }

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
  JS_SetPropertyStr(ctx, obj, "setEncoding", JS_NewCFunction(ctx, js_socket_set_encoding, "setEncoding", 1));
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
