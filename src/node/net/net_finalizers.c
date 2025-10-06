#include "net_internal.h"

// Close callback for socket cleanup
void socket_close_callback(uv_handle_t* handle) {
  // Free the connection struct after handle is closed
  if (handle->data) {
    JSNetConnection* conn = (JSNetConnection*)handle->data;
    handle->data = NULL;  // Clear pointer BEFORE freeing to prevent use-after-free

    // NOTE: 'close' event emission removed to prevent use-after-free
    // The socket_obj may have been garbage collected by the time this runs

    if (conn->host) {
      free(conn->host);
    }
    // FIXME: Memory leak - cannot free socket due to embedded timeout_timer
    // Issue: timeout_timer is embedded in conn struct. Freeing here causes
    // use-after-free when libuv processes other handles in the close queue.
    // Solution needed: Allocate timer separately or use reference counting.
    // free(conn);
  }
}

// Class finalizers
void js_socket_finalizer(JSRuntime* rt, JSValue val) {
  JSNetConnection* conn = JS_GetOpaque(val, js_socket_class_id);
  if (conn) {
    // Mark socket object as invalid to prevent use-after-free in callbacks
    conn->socket_obj = JS_UNDEFINED;

    // Stop and close timeout timer if it's active
    if (conn->timeout_enabled) {
      uv_timer_stop(&conn->timeout_timer);
      if (!uv_is_closing((uv_handle_t*)&conn->timeout_timer)) {
        uv_close((uv_handle_t*)&conn->timeout_timer, NULL);
      }
      conn->timeout_enabled = false;
    }

    // Only close if not already closing - memory will be freed in close callback
    if (!uv_is_closing((uv_handle_t*)&conn->handle)) {
      uv_close((uv_handle_t*)&conn->handle, socket_close_callback);
    } else if (conn->handle.data) {
      // Handle already closing, but not yet freed - will be freed by close callback
      // Do nothing here
    } else {
      // Handle already closed and freed, or never initialized
      // This shouldn't happen, but handle it safely
    }
  }
}

// Close callback for server - decrements close count
void server_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetServer* server = (JSNetServer*)handle->data;
    handle->data = NULL;

    server->close_count--;
    if (server->close_count == 0) {
      // All handles closed, safe to free
      if (server->host) {
        free(server->host);
      }
      // FIXME: Memory leak - cannot free server due to embedded timer handle
      // Issue: callback_timer is embedded in server struct. Freeing here causes
      // use-after-free when libuv processes other handles in the close queue.
      // Solution needed: Either allocate timer separately, or ensure timer is
      // the last handle to close (complex due to libuv's queue ordering).
      // free(server);
    }
  }
}

// Timer close callback - decrements close count
void server_timer_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetServer* server = (JSNetServer*)handle->data;
    handle->data = NULL;

    server->close_count--;
    if (server->close_count == 0) {
      // All handles closed, safe to free
      if (server->host) {
        free(server->host);
      }
      // FIXME: Memory leak - cannot free server due to embedded handle
      // See comment in server_close_callback for details.
      // free(server);
    }
  }
}

void js_server_finalizer(JSRuntime* rt, JSValue val) {
  JSNetServer* server = JS_GetOpaque(val, js_server_class_id);
  if (server) {
    // Stop timer if it was initialized
    if (server->timer_initialized) {
      uv_timer_stop(&server->callback_timer);
    }

    // Only free callback if we're NOT in the timer callback
    // (timer callback will free it when done)
    if (!server->in_callback && !JS_IsUndefined(server->listen_callback)) {
      JS_FreeValueRT(rt, server->listen_callback);
      server->listen_callback = JS_UNDEFINED;
    }

    // Set close count to number of handles we're closing
    server->close_count = 0;

    // Close server handle
    if (!uv_is_closing((uv_handle_t*)&server->handle)) {
      server->close_count++;
      server->handle.data = server;
      uv_close((uv_handle_t*)&server->handle, server_close_callback);
    }

    // Close timer only if it was initialized
    if (server->timer_initialized && !uv_is_closing((uv_handle_t*)&server->callback_timer)) {
      server->close_count++;
      server->callback_timer.data = server;
      uv_close((uv_handle_t*)&server->callback_timer, server_timer_close_callback);
    }

    // If no handles to close, free immediately
    if (server->close_count == 0) {
      if (server->host) {
        free(server->host);
      }
      free(server);
    }
  }
}
