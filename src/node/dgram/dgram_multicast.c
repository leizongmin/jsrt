#include "../../util/debug.h"
#include "dgram_internal.h"

// socket.addMembership(multicastAddress [, multicastInterface])
JSValue js_dgram_socket_add_membership(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing required argument: multicastAddress");
  }

  const char* multicast_addr = JS_ToCString(ctx, argv[0]);
  if (!multicast_addr) {
    return JS_EXCEPTION;
  }

  const char* interface_addr = NULL;
  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    interface_addr = JS_ToCString(ctx, argv[1]);
  }

  int result = uv_udp_set_membership(&socket->handle, multicast_addr, interface_addr, UV_JOIN_GROUP);

  JS_FreeCString(ctx, multicast_addr);
  if (interface_addr) {
    JS_FreeCString(ctx, interface_addr);
  }

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to join multicast group: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// socket.dropMembership(multicastAddress [, multicastInterface])
JSValue js_dgram_socket_drop_membership(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing required argument: multicastAddress");
  }

  const char* multicast_addr = JS_ToCString(ctx, argv[0]);
  if (!multicast_addr) {
    return JS_EXCEPTION;
  }

  const char* interface_addr = NULL;
  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    interface_addr = JS_ToCString(ctx, argv[1]);
  }

  int result = uv_udp_set_membership(&socket->handle, multicast_addr, interface_addr, UV_LEAVE_GROUP);

  JS_FreeCString(ctx, multicast_addr);
  if (interface_addr) {
    JS_FreeCString(ctx, interface_addr);
  }

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to leave multicast group: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// socket.setMulticastTTL(ttl)
JSValue js_dgram_socket_set_multicast_ttl(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
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

  if (ttl < 0 || ttl > 255) {
    return JS_ThrowRangeError(ctx, "TTL must be between 0 and 255");
  }

  int result = uv_udp_set_multicast_ttl(&socket->handle, ttl);
  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to set multicast TTL: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// socket.setMulticastInterface(multicastInterface)
JSValue js_dgram_socket_set_multicast_interface(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing required argument: multicastInterface");
  }

  const char* interface_addr = JS_ToCString(ctx, argv[0]);
  if (!interface_addr) {
    return JS_EXCEPTION;
  }

  int result = uv_udp_set_multicast_interface(&socket->handle, interface_addr);

  // Store interface for later use
  if (result == 0) {
    if (socket->multicast_interface) {
      js_free(ctx, socket->multicast_interface);
    }
    socket->multicast_interface = js_strdup(ctx, interface_addr);
  }

  JS_FreeCString(ctx, interface_addr);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to set multicast interface: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// socket.setMulticastLoopback(flag)
JSValue js_dgram_socket_set_multicast_loopback(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing required argument: flag");
  }

  int flag = JS_ToBool(ctx, argv[0]);
  int result = uv_udp_set_multicast_loop(&socket->handle, flag);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to set multicast loopback: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// socket.addSourceSpecificMembership(sourceAddress, groupAddress [, multicastInterface])
JSValue js_dgram_socket_add_source_specific_membership(JSContext* ctx, JSValueConst this_val, int argc,
                                                       JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Missing required arguments: sourceAddress and groupAddress");
  }

  const char* source_addr = JS_ToCString(ctx, argv[0]);
  if (!source_addr) {
    return JS_EXCEPTION;
  }

  const char* group_addr = JS_ToCString(ctx, argv[1]);
  if (!group_addr) {
    JS_FreeCString(ctx, source_addr);
    return JS_EXCEPTION;
  }

  const char* interface_addr = NULL;
  if (argc > 2 && !JS_IsUndefined(argv[2])) {
    interface_addr = JS_ToCString(ctx, argv[2]);
  }

  int result = uv_udp_set_source_membership(&socket->handle, group_addr, interface_addr, source_addr, UV_JOIN_GROUP);

  JS_FreeCString(ctx, source_addr);
  JS_FreeCString(ctx, group_addr);
  if (interface_addr) {
    JS_FreeCString(ctx, interface_addr);
  }

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to join source-specific multicast group: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}

// socket.dropSourceSpecificMembership(sourceAddress, groupAddress [, multicastInterface])
JSValue js_dgram_socket_drop_source_specific_membership(JSContext* ctx, JSValueConst this_val, int argc,
                                                        JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Missing required arguments: sourceAddress and groupAddress");
  }

  const char* source_addr = JS_ToCString(ctx, argv[0]);
  if (!source_addr) {
    return JS_EXCEPTION;
  }

  const char* group_addr = JS_ToCString(ctx, argv[1]);
  if (!group_addr) {
    JS_FreeCString(ctx, source_addr);
    return JS_EXCEPTION;
  }

  const char* interface_addr = NULL;
  if (argc > 2 && !JS_IsUndefined(argv[2])) {
    interface_addr = JS_ToCString(ctx, argv[2]);
  }

  int result = uv_udp_set_source_membership(&socket->handle, group_addr, interface_addr, source_addr, UV_LEAVE_GROUP);

  JS_FreeCString(ctx, source_addr);
  JS_FreeCString(ctx, group_addr);
  if (interface_addr) {
    JS_FreeCString(ctx, interface_addr);
  }

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to leave source-specific multicast group: %s", uv_strerror(result));
  }

  return JS_UNDEFINED;
}
