#include "../../util/debug.h"
#include "dgram_internal.h"

// Close callback for UDP socket handle (based on net module's socket_close_callback)
void dgram_socket_close_callback(uv_handle_t* handle) {
  JSDgramSocket* socket = (JSDgramSocket*)handle->data;

  // Verify type tag for safety
  if (!socket || socket->type_tag != DGRAM_TYPE_SOCKET) {
    JSRT_Debug("dgram_socket_close_callback: Invalid socket or type tag");
    return;
  }

  JSRT_Debug("dgram_socket_close_callback: Decrementing close_count from %d", socket->close_count);

  // Decrement close count
  if (socket->close_count > 0) {
    socket->close_count--;
  }

  // If all handles are closed, free the socket structure
  if (socket->close_count == 0) {
    JSRT_Debug("dgram_socket_close_callback: All handles closed, freeing socket");

    // Free multicast interface if allocated
    if (socket->multicast_interface) {
      js_free(socket->ctx, socket->multicast_interface);
      socket->multicast_interface = NULL;
    }

    // Free the socket object reference
    if (!JS_IsUndefined(socket->socket_obj)) {
      JS_FreeValue(socket->ctx, socket->socket_obj);
      socket->socket_obj = JS_UNDEFINED;
    }

    // Free the socket structure itself
    js_free(socket->ctx, socket);
  }
}

// Finalizer for dgram Socket objects (based on net module's js_socket_finalizer)
void js_dgram_socket_finalizer(JSRuntime* rt, JSValue val) {
  JSDgramSocket* socket = JS_GetOpaque(val, js_dgram_socket_class_id);

  if (!socket) {
    return;
  }

  // Verify type tag for safety
  if (socket->type_tag != DGRAM_TYPE_SOCKET) {
    JSRT_Debug("js_dgram_socket_finalizer: Invalid type tag");
    return;
  }

  JSRT_Debug("js_dgram_socket_finalizer: Finalizing socket, destroyed=%d, in_callback=%d", socket->destroyed,
             socket->in_callback);

  // If we're in a callback, defer cleanup
  if (socket->in_callback) {
    JSRT_Debug("js_dgram_socket_finalizer: Deferring cleanup (in callback)");
    return;
  }

  // Mark as destroyed
  socket->destroyed = true;

  // Stop receiving if active
  if (socket->receiving && !uv_is_closing((uv_handle_t*)&socket->handle)) {
    uv_udp_recv_stop(&socket->handle);
    socket->receiving = false;
  }

  // Close the UDP handle if not already closing
  if (!uv_is_closing((uv_handle_t*)&socket->handle)) {
    if (socket->close_count == 0) {
      socket->close_count = 1;  // Will be decremented in close callback
    }
    socket->handle.data = socket;
    uv_close((uv_handle_t*)&socket->handle, dgram_socket_close_callback);
  }
}
