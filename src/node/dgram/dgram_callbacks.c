#include <arpa/inet.h>
#include "../../util/debug.h"
#include "dgram_internal.h"

// Allocation callback for receiving data (based on net module's on_socket_alloc)
void on_dgram_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

// Send callback (based on net module's on_socket_write_complete pattern)
void on_dgram_send(uv_udp_send_t* req, int status) {
  JSDgramSendReq* send_req = (JSDgramSendReq*)req;

  if (!send_req) {
    return;
  }

  JSContext* ctx = send_req->ctx;
  JSDgramSocket* socket = JS_GetOpaque(send_req->socket_obj, js_dgram_socket_class_id);

  // If callback provided, call it
  if (!JS_IsUndefined(send_req->callback) && JS_IsFunction(ctx, send_req->callback)) {
    JSValue error = JS_UNDEFINED;

    if (status < 0) {
      // Create error object
      error = JS_NewError(ctx);
      JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, uv_strerror(status)));
      JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(status)));
      JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, "send"));
    } else {
      error = JS_NULL;
    }

    JSValue argv[] = {error};
    JSValue result = JS_Call(ctx, send_req->callback, send_req->socket_obj, 1, argv);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, argv[0]);
  }

  // Update statistics on success
  if (status == 0 && socket) {
    socket->messages_sent++;
    socket->bytes_sent += send_req->len;
  }

  // Cleanup
  if (send_req->data) {
    free(send_req->data);
  }
  JS_FreeValue(ctx, send_req->callback);
  JS_FreeValue(ctx, send_req->socket_obj);
  free(send_req);
}

// Receive callback (based on net module's on_socket_read pattern but adapted for UDP)
void on_dgram_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags) {
  JSDgramSocket* socket = (JSDgramSocket*)handle->data;

  if (!socket || !socket->ctx || socket->destroyed) {
    goto cleanup;
  }

  // Mark that we're in a callback to prevent finalization
  socket->in_callback = true;

  JSContext* ctx = socket->ctx;

  // Check if socket object is still valid
  if (JS_IsUndefined(socket->socket_obj) || JS_IsNull(socket->socket_obj)) {
    socket->in_callback = false;
    goto cleanup;
  }

  // Handle errors
  if (nread < 0) {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, uv_strerror(nread)));
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, uv_err_name(nread)));
    JS_SetPropertyStr(ctx, error, "syscall", JS_NewString(ctx, "recvmsg"));

    JSValue argv[] = {JS_NewString(ctx, "error"), error};
    JSValue emit_func = JS_GetPropertyStr(ctx, socket->socket_obj, "emit");
    if (JS_IsFunction(ctx, emit_func)) {
      JS_Call(ctx, emit_func, socket->socket_obj, 2, argv);
    }
    JS_FreeValue(ctx, emit_func);
    JS_FreeValue(ctx, argv[0]);
    JS_FreeValue(ctx, argv[1]);

    socket->in_callback = false;
    goto cleanup;
  }

  // Ignore empty datagrams (nread == 0)
  if (nread == 0) {
    socket->in_callback = false;
    goto cleanup;
  }

  // Create rinfo object with sender information
  JSValue rinfo = JS_NewObject(ctx);
  char addr_str[INET6_ADDRSTRLEN];
  int port = 0;
  const char* family = "IPv4";

  if (addr) {
    if (addr->sa_family == AF_INET) {
      struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
      inet_ntop(AF_INET, &addr_in->sin_addr, addr_str, sizeof(addr_str));
      port = ntohs(addr_in->sin_port);
      family = "IPv4";
    } else if (addr->sa_family == AF_INET6) {
      struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)addr;
      inet_ntop(AF_INET6, &addr_in6->sin6_addr, addr_str, sizeof(addr_str));
      port = ntohs(addr_in6->sin6_port);
      family = "IPv6";
    }

    JS_SetPropertyStr(ctx, rinfo, "address", JS_NewString(ctx, addr_str));
    JS_SetPropertyStr(ctx, rinfo, "port", JS_NewInt32(ctx, port));
    JS_SetPropertyStr(ctx, rinfo, "family", JS_NewString(ctx, family));
    JS_SetPropertyStr(ctx, rinfo, "size", JS_NewInt64(ctx, nread));
  }

  // Update statistics
  socket->messages_received++;
  socket->bytes_received += nread;

  // Create Buffer from received data (using node:buffer module)
  JSValue buffer_module = JSRT_LoadNodeModuleCommonJS(ctx, "buffer");
  if (!JS_IsException(buffer_module)) {
    JSValue buffer_class = JS_GetPropertyStr(ctx, buffer_module, "Buffer");
    if (!JS_IsException(buffer_class)) {
      JSValue from_func = JS_GetPropertyStr(ctx, buffer_class, "from");
      if (JS_IsFunction(ctx, from_func)) {
        // Create ArrayBuffer copy from received data
        JSValue array_buffer = JS_NewArrayBufferCopy(ctx, (const uint8_t*)buf->base, nread);

        // Convert to Buffer
        JSValue argv_buf[] = {array_buffer};
        JSValue msg_buffer = JS_Call(ctx, from_func, buffer_class, 1, argv_buf);

        // Emit 'message' event
        JSValue argv[] = {JS_NewString(ctx, "message"), msg_buffer, rinfo};
        JSValue emit_func = JS_GetPropertyStr(ctx, socket->socket_obj, "emit");
        if (JS_IsFunction(ctx, emit_func)) {
          JS_Call(ctx, emit_func, socket->socket_obj, 3, argv);
        }
        JS_FreeValue(ctx, emit_func);
        JS_FreeValue(ctx, argv[0]);
        JS_FreeValue(ctx, argv[1]);
        JS_FreeValue(ctx, argv[2]);

        JS_FreeValue(ctx, array_buffer);
      }
      JS_FreeValue(ctx, from_func);
    }
    JS_FreeValue(ctx, buffer_class);
    JS_FreeValue(ctx, buffer_module);
  }

  // Clear callback flag
  socket->in_callback = false;

cleanup:
  // Always free the buffer allocated in on_dgram_alloc
  if (buf && buf->base) {
    free(buf->base);
  }
}
