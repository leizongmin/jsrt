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

// Close callback for socket cleanup
// NOTE: DO NOT free the connection struct here!
// It will be freed by JSRT_RuntimeCleanupWalkCallback after the loop closes
// Freeing here causes use-after-free during uv_walk in shutdown
void socket_close_callback(uv_handle_t* handle) {
  // Do nothing - cleanup happens in JSRT_RuntimeCleanupWalkCallback
}

// Class finalizers
void js_socket_finalizer(JSRuntime* rt, JSValue val) {
  JSNetConnection* conn = JS_GetOpaque(val, js_socket_class_id);
  if (!conn) {
    return;
  }

  // Prevent double-free: clear opaque pointer immediately
  JS_SetOpaque(val, NULL);

  // Mark socket object as invalid to prevent use-after-free in callbacks
  conn->socket_obj = JS_UNDEFINED;

  // Close timeout timer if it's initialized
  if (conn->timeout_timer_initialized && conn->timeout_timer) {
    uv_timer_stop(conn->timeout_timer);
    if (!uv_is_closing((uv_handle_t*)conn->timeout_timer)) {
      conn->timeout_timer->data = conn;
      uv_close((uv_handle_t*)conn->timeout_timer, socket_timeout_timer_close_callback);
    }
    conn->timeout_enabled = false;
  }

  // Close socket handle - callback will free the struct
  if (!uv_is_closing((uv_handle_t*)&conn->handle)) {
    conn->handle.data = conn;
    uv_close((uv_handle_t*)&conn->handle, socket_close_callback);
  } else {
    // Handle already closing or closed, free immediately
    if (conn->host) {
      free(conn->host);
    }
    free(conn);
  }
}

// Timer close callback for server - frees timer memory and decrements close count
void server_callback_timer_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetServer* server = (JSNetServer*)handle->data;
    handle->data = NULL;

    // Free the timer itself
    free(server->callback_timer);
    server->callback_timer = NULL;
    server->timer_initialized = false;

    server->close_count--;
    if (server->close_count == 0) {
      // All handles closed, safe to free
      if (server->host) {
        free(server->host);
      }
      free(server);
    }
  }
}

// Close callback for server cleanup
// NOTE: DO NOT free the server struct here!
// It will be freed by JSRT_RuntimeCleanupWalkCallback after the loop closes
// Freeing here causes use-after-free during uv_walk in shutdown
void server_close_callback(uv_handle_t* handle) {
  // Do nothing - cleanup happens in JSRT_RuntimeCleanupWalkCallback
}

void js_server_finalizer(JSRuntime* rt, JSValue val) {
  JSNetServer* server = JS_GetOpaque(val, js_server_class_id);
  if (server) {
    // Mark server object as invalid to prevent use-after-free in callbacks
    server->server_obj = JS_UNDEFINED;

    // Set close count to number of handles we're closing
    server->close_count = 0;

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
      server->close_count++;
      server->callback_timer->data = server;
      uv_close((uv_handle_t*)server->callback_timer, server_callback_timer_close_callback);
    }

    // Close server handle
    if (!uv_is_closing((uv_handle_t*)&server->handle)) {
      server->close_count++;
      server->handle.data = server;
      uv_close((uv_handle_t*)&server->handle, server_close_callback);
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
