#include "net_internal.h"

// Timer close callback - frees timer memory and decrements close count
void socket_timeout_timer_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetConnection* conn = (JSNetConnection*)handle->data;
    handle->data = NULL;

    // Free the timer itself
    free(conn->timeout_timer);
    conn->timeout_timer = NULL;
    conn->timeout_timer_initialized = false;

    conn->close_count--;
    if (conn->close_count == 0) {
      // All handles closed, safe to free
      if (conn->host) {
        free(conn->host);
      }
      free(conn);
    }
  }
}

// Close callback for socket cleanup - decrement close count
void socket_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetConnection* conn = (JSNetConnection*)handle->data;
    handle->data = NULL;

    conn->close_count--;
    if (conn->close_count == 0) {
      // All handles closed, safe to free
      if (conn->host) {
        free(conn->host);
      }
      free(conn);
    }
  }
}

// Class finalizers
void js_socket_finalizer(JSRuntime* rt, JSValue val) {
  JSNetConnection* conn = JS_GetOpaque(val, js_socket_class_id);
  if (conn) {
    // Mark socket object as invalid to prevent use-after-free in callbacks
    conn->socket_obj = JS_UNDEFINED;

    // Set close count to number of handles we're closing
    conn->close_count = 0;

    // Close timeout timer if it's initialized
    if (conn->timeout_timer_initialized && conn->timeout_timer) {
      uv_timer_stop(conn->timeout_timer);
      if (!uv_is_closing((uv_handle_t*)conn->timeout_timer)) {
        conn->close_count++;
        conn->timeout_timer->data = conn;
        uv_close((uv_handle_t*)conn->timeout_timer, socket_timeout_timer_close_callback);
      }
      conn->timeout_enabled = false;
    }

    // Close socket handle
    if (!uv_is_closing((uv_handle_t*)&conn->handle)) {
      conn->close_count++;
      conn->handle.data = conn;
      uv_close((uv_handle_t*)&conn->handle, socket_close_callback);
    }

    // If no handles to close, free immediately
    if (conn->close_count == 0) {
      if (conn->host) {
        free(conn->host);
      }
      free(conn);
    }
  }
}

// Timer close callback for server - frees timer memory
void server_callback_timer_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetServer* server = (JSNetServer*)handle->data;
    // Free the timer itself
    free(server->callback_timer);
    server->callback_timer = NULL;
    server->timer_initialized = false;
  }
}

// Close callback for server cleanup
void server_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetServer* server = (JSNetServer*)handle->data;
    handle->data = NULL;

    // Free server resources
    if (server->host) {
      free(server->host);
    }

    // Now safe to free - timer is allocated separately
    free(server);
  }
}

void js_server_finalizer(JSRuntime* rt, JSValue val) {
  JSNetServer* server = JS_GetOpaque(val, js_server_class_id);
  if (server) {
    // Stop timer if it was initialized
    if (server->timer_initialized && server->callback_timer) {
      uv_timer_stop(server->callback_timer);
    }

    // Only free callback if we're NOT in the timer callback
    // (timer callback will free it when done)
    if (!server->in_callback && !JS_IsUndefined(server->listen_callback)) {
      JS_FreeValueRT(rt, server->listen_callback);
      server->listen_callback = JS_UNDEFINED;
    }

    // Close timer only if it was initialized
    if (server->timer_initialized && server->callback_timer && !uv_is_closing((uv_handle_t*)server->callback_timer)) {
      server->callback_timer->data = server;
      uv_close((uv_handle_t*)server->callback_timer, server_callback_timer_close_callback);
    }

    // Close server handle - memory will be freed in close callback
    if (!uv_is_closing((uv_handle_t*)&server->handle)) {
      server->handle.data = server;
      uv_close((uv_handle_t*)&server->handle, server_close_callback);
    } else if (server->handle.data) {
      // Handle already closing, will be freed by close callback
    } else {
      // Handle already closed and freed, or never initialized
      // This shouldn't happen, but handle it safely
    }
  }
}
