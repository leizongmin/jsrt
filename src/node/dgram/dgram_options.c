#include "../../util/debug.h"
#include "dgram_internal.h"

// socket.setBroadcast(flag)
JSValue js_dgram_socket_set_broadcast(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing required argument: flag");
  }

  int flag = JS_ToBool(ctx, argv[0]);
  int result = uv_udp_set_broadcast(&socket->handle, flag);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to set broadcast: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// socket.setTTL(ttl)
JSValue js_dgram_socket_set_ttl(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing required argument: ttl");
  }

  int ttl;
  if (JS_ToInt32(ctx, &ttl, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  if (ttl < 1 || ttl > 255) {
    return JS_ThrowRangeError(ctx, "TTL must be between 1 and 255");
  }

  int result = uv_udp_set_ttl(&socket->handle, ttl);
  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to set TTL: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// socket.getSendBufferSize()
JSValue js_dgram_socket_get_send_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  int size = 0;
  int result = uv_send_buffer_size((uv_handle_t*)&socket->handle, &size);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to get send buffer size: %s", uv_strerror(result));
  }

  return JS_NewInt32(ctx, size);
}

// socket.getRecvBufferSize()
JSValue js_dgram_socket_get_recv_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  int size = 0;
  int result = uv_recv_buffer_size((uv_handle_t*)&socket->handle, &size);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to get receive buffer size: %s", uv_strerror(result));
  }

  return JS_NewInt32(ctx, size);
}

// socket.setSendBufferSize(size)
JSValue js_dgram_socket_set_send_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing required argument: size");
  }

  int size;
  if (JS_ToInt32(ctx, &size, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  if (size < 0) {
    return JS_ThrowRangeError(ctx, "Size must be a positive number");
  }

  int result = uv_send_buffer_size((uv_handle_t*)&socket->handle, &size);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to set send buffer size: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// socket.setRecvBufferSize(size)
JSValue js_dgram_socket_set_recv_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing required argument: size");
  }

  int size;
  if (JS_ToInt32(ctx, &size, argv[0]) < 0) {
    return JS_EXCEPTION;
  }

  if (size < 0) {
    return JS_ThrowRangeError(ctx, "Size must be a positive number");
  }

  int result = uv_recv_buffer_size((uv_handle_t*)&socket->handle, &size);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to set receive buffer size: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// Connected UDP operations (optional, stubs for now)
JSValue js_dgram_socket_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "Connected UDP is not yet implemented");
}

JSValue js_dgram_socket_disconnect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "Connected UDP is not yet implemented");
}

JSValue js_dgram_socket_remote_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  return JS_ThrowInternalError(ctx, "Connected UDP is not yet implemented");
}
