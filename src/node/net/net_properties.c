#include "net_internal.h"

// Socket property getters
JSValue js_socket_get_local_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_get_local_port(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_get_local_family(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_get_remote_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_get_remote_port(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_get_remote_family(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_get_bytes_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewInt64(ctx, 0);
  }
  return JS_NewInt64(ctx, conn->bytes_read);
}

JSValue js_socket_get_bytes_written(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewInt64(ctx, 0);
  }
  return JS_NewInt64(ctx, conn->bytes_written);
}

JSValue js_socket_get_connecting(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewBool(ctx, false);
  }
  return JS_NewBool(ctx, conn->connecting);
}

JSValue js_socket_get_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewBool(ctx, true);
  }
  return JS_NewBool(ctx, conn->destroyed);
}

JSValue js_socket_get_pending(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn) {
    return JS_NewBool(ctx, false);
  }
  // pending means not yet connected (connecting or not started)
  return JS_NewBool(ctx, !conn->connected && !conn->destroyed);
}

JSValue js_socket_get_ready_state(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

JSValue js_socket_get_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetConnection* conn = JS_GetOpaque(this_val, js_socket_class_id);
  if (!conn || !conn->connected) {
    return JS_NewInt32(ctx, 0);
  }

  // Get the write queue size from libuv
  size_t size = uv_stream_get_write_queue_size((uv_stream_t*)&conn->handle);
  return JS_NewInt64(ctx, size);
}
