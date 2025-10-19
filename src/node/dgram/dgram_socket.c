#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include "../../util/debug.h"
#include "dgram_internal.h"

JSClassID js_dgram_socket_class_id;

// Property getter: destroyed
JSValue js_dgram_socket_get_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }
  return JS_NewBool(ctx, socket->destroyed);
}

// Socket constructor (based on net module's js_socket_constructor)
JSValue js_dgram_socket_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_dgram_socket_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  // Allocate socket structure
  JSDgramSocket* socket = js_mallocz(ctx, sizeof(JSDgramSocket));
  if (!socket) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize socket structure
  socket->type_tag = DGRAM_TYPE_SOCKET;
  socket->ctx = ctx;
  socket->socket_obj = JS_DupValue(ctx, obj);
  socket->bound = false;
  socket->connected = false;
  socket->destroyed = false;
  socket->receiving = false;
  socket->in_callback = false;
  socket->close_count = 0;
  socket->multicast_interface = NULL;
  socket->family = AF_INET;  // Default to IPv4
  socket->bytes_sent = 0;
  socket->bytes_received = 0;
  socket->messages_sent = 0;
  socket->messages_received = 0;

  // Parse options if provided
  if (argc > 0 && JS_IsObject(argv[0])) {
    JSValue type_val = JS_GetPropertyStr(ctx, argv[0], "type");
    if (!JS_IsUndefined(type_val)) {
      const char* type = JS_ToCString(ctx, type_val);
      if (type) {
        if (strcmp(type, "udp6") == 0) {
          socket->family = AF_INET6;
        }
        JS_FreeCString(ctx, type);
      }
    }
    JS_FreeValue(ctx, type_val);
  }

  // Initialize libuv UDP handle
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  int result = uv_udp_init(rt->uv_loop, &socket->handle);

  if (result < 0) {
    js_free(ctx, socket);
    JS_FreeValue(ctx, obj);
    return JS_ThrowInternalError(ctx, "Failed to initialize UDP socket: %s", uv_strerror(result));
  }

  socket->handle.data = socket;

  // Add EventEmitter methods
  add_event_emitter_methods(ctx, obj);

  // Add 'destroyed' property
  JS_DefinePropertyGetSet(ctx, obj, JS_NewAtom(ctx, "destroyed"),
                          JS_NewCFunction(ctx, js_dgram_socket_get_destroyed, "get_destroyed", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);

  JS_SetOpaque(obj, socket);

  return obj;
}

// createSocket(type) or createSocket(options [, callback])
JSValue js_dgram_create_socket(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "Missing required argument: type or options");
  }

  JSValue socket_obj;
  JSValue callback = JS_UNDEFINED;

  // Handle createSocket(type) or createSocket(type, callback)
  if (JS_IsString(argv[0])) {
    const char* type = JS_ToCString(ctx, argv[0]);
    if (!type) {
      return JS_EXCEPTION;
    }

    if (strcmp(type, "udp4") != 0 && strcmp(type, "udp6") != 0) {
      JS_FreeCString(ctx, type);
      return JS_ThrowTypeError(ctx, "Invalid socket type: must be 'udp4' or 'udp6'");
    }

    // Create options object
    JSValue options = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, options, "type", JS_NewString(ctx, type));
    JS_FreeCString(ctx, type);

    JSValue ctor_argv[] = {options};
    socket_obj = js_dgram_socket_constructor(ctx, JS_UNDEFINED, 1, ctor_argv);
    JS_FreeValue(ctx, options);

    if (argc > 1 && JS_IsFunction(ctx, argv[1])) {
      callback = argv[1];
    }
  }
  // Handle createSocket(options [, callback])
  else if (JS_IsObject(argv[0])) {
    JSValue ctor_argv[] = {argv[0]};
    socket_obj = js_dgram_socket_constructor(ctx, JS_UNDEFINED, 1, ctor_argv);

    if (argc > 1 && JS_IsFunction(ctx, argv[1])) {
      callback = argv[1];
    }
  } else {
    return JS_ThrowTypeError(ctx, "First argument must be string or object");
  }

  if (JS_IsException(socket_obj)) {
    return socket_obj;
  }

  // If callback provided, register it as 'message' listener
  if (!JS_IsUndefined(callback)) {
    JSValue on_func = JS_GetPropertyStr(ctx, socket_obj, "on");
    if (JS_IsFunction(ctx, on_func)) {
      JSValue argv_on[] = {JS_NewString(ctx, "message"), callback};
      JSValue result = JS_Call(ctx, on_func, socket_obj, 2, argv_on);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, argv_on[0]);
    }
    JS_FreeValue(ctx, on_func);
  }

  return socket_obj;
}

// socket.bind([port] [, address] [, callback]) or socket.bind(options [, callback])
JSValue js_dgram_socket_bind(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (socket->bound) {
    return JS_ThrowTypeError(ctx, "Socket is already bound");
  }

  if (socket->destroyed) {
    return JS_ThrowTypeError(ctx, "Socket is destroyed");
  }

  int port = 0;
  const char* address = NULL;
  bool exclusive = true;  // Default to exclusive binding
  JSValue callback = JS_UNDEFINED;

  // Parse arguments
  if (argc > 0) {
    if (JS_IsObject(argv[0]) && !JS_IsFunction(ctx, argv[0])) {
      // bind(options [, callback])
      JSValue port_val = JS_GetPropertyStr(ctx, argv[0], "port");
      if (!JS_IsUndefined(port_val)) {
        JS_ToInt32(ctx, &port, port_val);
      }
      JS_FreeValue(ctx, port_val);

      JSValue addr_val = JS_GetPropertyStr(ctx, argv[0], "address");
      if (!JS_IsUndefined(addr_val)) {
        address = JS_ToCString(ctx, addr_val);
      }
      JS_FreeValue(ctx, addr_val);

      JSValue excl_val = JS_GetPropertyStr(ctx, argv[0], "exclusive");
      if (!JS_IsUndefined(excl_val)) {
        exclusive = JS_ToBool(ctx, excl_val);
      }
      JS_FreeValue(ctx, excl_val);

      if (argc > 1 && JS_IsFunction(ctx, argv[1])) {
        callback = argv[1];
      }
    } else {
      // bind([port] [, address] [, callback])
      int arg_idx = 0;

      if (arg_idx < argc && JS_IsNumber(argv[arg_idx])) {
        JS_ToInt32(ctx, &port, argv[arg_idx]);
        arg_idx++;
      }

      if (arg_idx < argc && JS_IsString(argv[arg_idx])) {
        address = JS_ToCString(ctx, argv[arg_idx]);
        arg_idx++;
      }

      if (arg_idx < argc && JS_IsFunction(ctx, argv[arg_idx])) {
        callback = argv[arg_idx];
      }
    }
  }

  // Default address based on socket family
  if (!address) {
    address = (socket->family == AF_INET6) ? "::" : "0.0.0.0";
  }

  // Parse address and bind
  struct sockaddr_storage addr_storage;
  int result;

  if (socket->family == AF_INET) {
    struct sockaddr_in* addr = (struct sockaddr_in*)&addr_storage;
    result = uv_ip4_addr(address, port, addr);
  } else {
    struct sockaddr_in6* addr = (struct sockaddr_in6*)&addr_storage;
    result = uv_ip6_addr(address, port, addr);
  }

  if (result < 0) {
    if (address && strcmp(address, "0.0.0.0") != 0 && strcmp(address, "::") != 0) {
      JS_FreeCString(ctx, address);
    }
    return JS_ThrowInternalError(ctx, "Invalid address: %s", uv_strerror(result));
  }

  // Bind socket
  unsigned int flags = exclusive ? 0 : UV_UDP_REUSEADDR;
  result = uv_udp_bind(&socket->handle, (struct sockaddr*)&addr_storage, flags);

  if (result < 0) {
    if (address && strcmp(address, "0.0.0.0") != 0 && strcmp(address, "::") != 0) {
      JS_FreeCString(ctx, address);
    }

    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, uv_strerror(result)));
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(result)));
    JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, "bind"));

    JSValue argv_emit[] = {JS_NewString(ctx, "error"), error};
    JSValue emit_func = JS_GetPropertyStr(ctx, this_val, "emit");
    if (JS_IsFunction(ctx, emit_func)) {
      JS_Call(ctx, emit_func, this_val, 2, argv_emit);
    }
    JS_FreeValue(ctx, emit_func);
    JS_FreeValue(ctx, argv_emit[0]);
    JS_FreeValue(ctx, argv_emit[1]);

    return JS_UNDEFINED;
  }

  if (address && strcmp(address, "0.0.0.0") != 0 && strcmp(address, "::") != 0) {
    JS_FreeCString(ctx, address);
  }

  socket->bound = true;

  // Start receiving
  result = uv_udp_recv_start(&socket->handle, on_dgram_alloc, on_dgram_recv);
  if (result == 0) {
    socket->receiving = true;
  }

  // Emit 'listening' event
  JSValue argv_emit[] = {JS_NewString(ctx, "listening")};
  JSValue emit_func = JS_GetPropertyStr(ctx, this_val, "emit");
  if (JS_IsFunction(ctx, emit_func)) {
    JS_Call(ctx, emit_func, this_val, 1, argv_emit);
  }
  JS_FreeValue(ctx, emit_func);
  JS_FreeValue(ctx, argv_emit[0]);

  // Call callback if provided
  if (!JS_IsUndefined(callback) && JS_IsFunction(ctx, callback)) {
    JSValue result_cb = JS_Call(ctx, callback, this_val, 0, NULL);
    JS_FreeValue(ctx, result_cb);
  }

  return JS_UNDEFINED;
}

// socket.address()
JSValue js_dgram_socket_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (!socket->bound) {
    return JS_ThrowTypeError(ctx, "Socket is not bound");
  }

  struct sockaddr_storage addr_storage;
  int addr_len = sizeof(addr_storage);
  int result = uv_udp_getsockname(&socket->handle, (struct sockaddr*)&addr_storage, &addr_len);

  if (result < 0) {
    return JS_ThrowInternalError(ctx, "Failed to get socket name: %s", uv_strerror(result));
  }

  JSValue addr_obj = JS_NewObject(ctx);
  char addr_str[INET6_ADDRSTRLEN];
  int port = 0;
  const char* family = "IPv4";

  if (addr_storage.ss_family == AF_INET) {
    struct sockaddr_in* addr = (struct sockaddr_in*)&addr_storage;
    inet_ntop(AF_INET, &addr->sin_addr, addr_str, sizeof(addr_str));
    port = ntohs(addr->sin_port);
    family = "IPv4";
  } else if (addr_storage.ss_family == AF_INET6) {
    struct sockaddr_in6* addr = (struct sockaddr_in6*)&addr_storage;
    inet_ntop(AF_INET6, &addr->sin6_addr, addr_str, sizeof(addr_str));
    port = ntohs(addr->sin6_port);
    family = "IPv6";
  }

  JS_SetPropertyStr(ctx, addr_obj, "address", JS_NewString(ctx, addr_str));
  JS_SetPropertyStr(ctx, addr_obj, "port", JS_NewInt32(ctx, port));
  JS_SetPropertyStr(ctx, addr_obj, "family", JS_NewString(ctx, family));

  return addr_obj;
}

// socket.close([callback])
JSValue js_dgram_socket_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (socket->destroyed) {
    return JS_UNDEFINED;
  }

  JSValue callback = (argc > 0 && JS_IsFunction(ctx, argv[0])) ? argv[0] : JS_UNDEFINED;

  // Register close callback if provided
  if (!JS_IsUndefined(callback)) {
    JSValue once_func = JS_GetPropertyStr(ctx, this_val, "once");
    if (JS_IsFunction(ctx, once_func)) {
      JSValue argv_once[] = {JS_NewString(ctx, "close"), callback};
      JSValue result = JS_Call(ctx, once_func, this_val, 2, argv_once);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, argv_once[0]);
    }
    JS_FreeValue(ctx, once_func);
  }

  // Stop receiving
  if (socket->receiving && !uv_is_closing((uv_handle_t*)&socket->handle)) {
    uv_udp_recv_stop(&socket->handle);
    socket->receiving = false;
  }

  // Close handle
  if (!uv_is_closing((uv_handle_t*)&socket->handle)) {
    if (socket->close_count == 0) {
      socket->close_count = 1;
    }
    socket->destroyed = true;
    socket->handle.data = socket;
    uv_close((uv_handle_t*)&socket->handle, dgram_socket_close_callback);

    // Emit 'close' event after handle is closed
    // (this will be done in the close callback, but we emit it now for consistency)
    JSValue argv_emit[] = {JS_NewString(ctx, "close")};
    JSValue emit_func = JS_GetPropertyStr(ctx, this_val, "emit");
    if (JS_IsFunction(ctx, emit_func)) {
      JS_Call(ctx, emit_func, this_val, 1, argv_emit);
    }
    JS_FreeValue(ctx, emit_func);
    JS_FreeValue(ctx, argv_emit[0]);
  }

  return JS_UNDEFINED;
}

// socket.ref()
JSValue js_dgram_socket_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  uv_ref((uv_handle_t*)&socket->handle);
  return this_val;
}

// socket.unref()
JSValue js_dgram_socket_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  uv_unref((uv_handle_t*)&socket->handle);
  return this_val;
}
