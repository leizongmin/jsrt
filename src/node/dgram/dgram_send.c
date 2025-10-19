#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
#include "../../util/debug.h"
#include "dgram_internal.h"

// socket.send(msg, [offset, length,] port [, address] [, callback])
JSValue js_dgram_socket_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSDgramSocket* socket = JS_GetOpaque(this_val, js_dgram_socket_class_id);
  if (!socket) {
    return JS_ThrowTypeError(ctx, "Not a dgram.Socket instance");
  }

  if (socket->destroyed) {
    return JS_ThrowInternalError(ctx, "Socket is destroyed");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "Missing required arguments: msg and port");
  }

  // Auto-bind if not bound
  if (!socket->bound) {
    // Bind to random port
    struct sockaddr_storage addr_storage;
    const char* bind_addr = (socket->family == AF_INET6) ? "::" : "0.0.0.0";
    int result;

    if (socket->family == AF_INET) {
      result = uv_ip4_addr(bind_addr, 0, (struct sockaddr_in*)&addr_storage);
    } else {
      result = uv_ip6_addr(bind_addr, 0, (struct sockaddr_in6*)&addr_storage);
    }

    if (result == 0) {
      result = uv_udp_bind(&socket->handle, (struct sockaddr*)&addr_storage, 0);
      if (result == 0) {
        socket->bound = true;

        // Start receiving
        result = uv_udp_recv_start(&socket->handle, on_dgram_alloc, on_dgram_recv);
        if (result == 0) {
          socket->receiving = true;
        }
      }
    }

    if (result < 0) {
      return JS_ThrowInternalError(ctx, "Failed to auto-bind socket: %s", uv_strerror(result));
    }
  }

  // Parse arguments
  JSValue msg = argv[0];
  int offset = 0;
  int length = 0;
  int port = 0;
  const char* address = NULL;
  JSValue callback = JS_UNDEFINED;
  int arg_idx = 1;

  // Check if Buffer or string
  size_t msg_len;
  uint8_t* msg_data = NULL;
  bool is_buffer = false;

  // Try to get buffer data
  JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
  if (!JS_IsException(buffer_module)) {
    JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
    if (!JS_IsException(buffer_class)) {
      JSValue is_buffer_func = JS_GetPropertyStr(ctx, buffer_class, "isBuffer");
      if (JS_IsFunction(ctx, is_buffer_func)) {
        JSValue argv_check[] = {msg};
        JSValue result = JS_Call(ctx, is_buffer_func, buffer_class, 1, argv_check);
        is_buffer = JS_ToBool(ctx, result);
        JS_FreeValue(ctx, result);
      }
      JS_FreeValue(ctx, is_buffer_func);

      if (is_buffer) {
        msg_data = JS_GetArrayBuffer(ctx, &msg_len, msg);
        if (!msg_data) {
          // Try getting from Buffer's underlying ArrayBuffer
          JSValue ab = JS_GetPropertyStr(ctx, msg, "buffer");
          if (!JS_IsException(ab)) {
            msg_data = JS_GetArrayBuffer(ctx, &msg_len, ab);
          }
          JS_FreeValue(ctx, ab);
        }
      }
    }
    JS_FreeValue(ctx, buffer_class);
    JS_FreeValue(ctx, buffer_module);
  }

  // If not buffer, try string
  if (!is_buffer) {
    const char* str = JS_ToCString(ctx, msg);
    if (!str) {
      return JS_ThrowTypeError(ctx, "Message must be a Buffer or string");
    }
    msg_len = strlen(str);
    msg_data = (uint8_t*)str;
  }

  // Parse offset and length if provided (4+ arguments means offset/length present)
  if (argc >= 4 && JS_IsNumber(argv[1]) && JS_IsNumber(argv[2])) {
    JS_ToInt32(ctx, &offset, argv[1]);
    JS_ToInt32(ctx, &length, argv[2]);
    arg_idx = 3;

    if (offset < 0 || offset >= (int)msg_len) {
      if (!is_buffer) {
        JS_FreeCString(ctx, (const char*)msg_data);
      }
      return JS_ThrowRangeError(ctx, "Offset out of range");
    }

    if (length < 0 || offset + length > (int)msg_len) {
      if (!is_buffer) {
        JS_FreeCString(ctx, (const char*)msg_data);
      }
      return JS_ThrowRangeError(ctx, "Length out of range");
    }
  } else {
    offset = 0;
    length = msg_len;
  }

  // Parse port
  if (arg_idx < argc && JS_IsNumber(argv[arg_idx])) {
    JS_ToInt32(ctx, &port, argv[arg_idx]);
    arg_idx++;
  } else {
    if (!is_buffer) {
      JS_FreeCString(ctx, (const char*)msg_data);
    }
    return JS_ThrowTypeError(ctx, "Port is required");
  }

  // Parse address (optional)
  if (arg_idx < argc && JS_IsString(argv[arg_idx])) {
    address = JS_ToCString(ctx, argv[arg_idx]);
    arg_idx++;
  }

  // Parse callback (optional)
  if (arg_idx < argc && JS_IsFunction(ctx, argv[arg_idx])) {
    callback = argv[arg_idx];
  }

  // Default address
  if (!address) {
    address = (socket->family == AF_INET6) ? "::1" : "127.0.0.1";
  }

  // Create send request
  JSDgramSendReq* send_req = malloc(sizeof(JSDgramSendReq));
  if (!send_req) {
    if (!is_buffer) {
      JS_FreeCString(ctx, (const char*)msg_data);
    }
    if (address && strcmp(address, "127.0.0.1") != 0 && strcmp(address, "::1") != 0) {
      JS_FreeCString(ctx, address);
    }
    return JS_ThrowOutOfMemory(ctx);
  }

  send_req->ctx = ctx;
  send_req->socket_obj = JS_DupValue(ctx, this_val);
  send_req->callback = JS_DupValue(ctx, callback);
  send_req->len = length;

  // Copy data
  send_req->data = malloc(length);
  if (!send_req->data) {
    JS_FreeValue(ctx, send_req->socket_obj);
    JS_FreeValue(ctx, send_req->callback);
    free(send_req);
    if (!is_buffer) {
      JS_FreeCString(ctx, (const char*)msg_data);
    }
    if (address && strcmp(address, "127.0.0.1") != 0 && strcmp(address, "::1") != 0) {
      JS_FreeCString(ctx, address);
    }
    return JS_ThrowOutOfMemory(ctx);
  }
  memcpy(send_req->data, msg_data + offset, length);

  // Parse destination address
  struct sockaddr_storage dest_addr;
  int result;

  if (socket->family == AF_INET) {
    result = uv_ip4_addr(address, port, (struct sockaddr_in*)&dest_addr);
  } else {
    result = uv_ip6_addr(address, port, (struct sockaddr_in6*)&dest_addr);
  }

  if (!is_buffer) {
    JS_FreeCString(ctx, (const char*)msg_data);
  }
  if (address && strcmp(address, "127.0.0.1") != 0 && strcmp(address, "::1") != 0) {
    JS_FreeCString(ctx, address);
  }

  if (result < 0) {
    free(send_req->data);
    JS_FreeValue(ctx, send_req->socket_obj);
    JS_FreeValue(ctx, send_req->callback);
    free(send_req);
    return JS_ThrowInternalError(ctx, "Invalid address: %s", uv_strerror(result));
  }

  // Send
  uv_buf_t buf = uv_buf_init(send_req->data, length);
  result = uv_udp_send(&send_req->req, &socket->handle, &buf, 1, (struct sockaddr*)&dest_addr, on_dgram_send);

  if (result < 0) {
    free(send_req->data);
    JS_FreeValue(ctx, send_req->socket_obj);
    JS_FreeValue(ctx, send_req->callback);
    free(send_req);

    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, uv_strerror(result)));
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(result)));
    JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, "send"));

    // Call callback with error if provided
    if (!JS_IsUndefined(callback) && JS_IsFunction(ctx, callback)) {
      JSValue argv_cb[] = {error};
      JSValue result_cb = JS_Call(ctx, callback, this_val, 1, argv_cb);
      JS_FreeValue(ctx, result_cb);
    } else {
      // Emit error event if no callback
      JSValue argv_emit[] = {JS_NewString(ctx, "error"), error};
      JSValue emit_func = JS_GetPropertyStr(ctx, this_val, "emit");
      if (JS_IsFunction(ctx, emit_func)) {
        JS_Call(ctx, emit_func, this_val, 2, argv_emit);
      }
      JS_FreeValue(ctx, emit_func);
      JS_FreeValue(ctx, argv_emit[0]);
    }

    JS_FreeValue(ctx, error);
  }

  return JS_UNDEFINED;
}
