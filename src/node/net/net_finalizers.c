#include "net_internal.h"

// Deferred cleanup list - structs to free after loop closes
typedef struct DeferredCleanup {
  void* ptr;
  char* string1;  // For host
  char* string2;  // For encoding
  struct DeferredCleanup* next;
} DeferredCleanup;

static DeferredCleanup* deferred_cleanup_head = NULL;

static void add_deferred_cleanup(void* ptr, char* string1, char* string2) {
  // Check if this pointer is already in the list to prevent double-free
  DeferredCleanup* current = deferred_cleanup_head;
  while (current) {
    if (current->ptr == ptr) {
      // Already in list, don't add again
      return;
    }
    current = current->next;
  }

  // Not in list, add it
  DeferredCleanup* node = malloc(sizeof(DeferredCleanup));
  if (node) {
    node->ptr = ptr;
    node->string1 = string1;
    node->string2 = string2;
    node->next = deferred_cleanup_head;
    deferred_cleanup_head = node;
  }
}

void jsrt_net_cleanup_deferred(void) {
  DeferredCleanup* current = deferred_cleanup_head;
  while (current) {
    DeferredCleanup* next = current->next;
    if (current->string1) {
      free(current->string1);
    }
    if (current->string2) {
      free(current->string2);
    }
    free(current->ptr);
    free(current);
    current = next;
  }
  deferred_cleanup_head = NULL;
}

// Timer close callback - frees timer memory and decrements close count
void socket_timeout_timer_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetConnection* conn = (JSNetConnection*)handle->data;

    // Free the timer itself
    free(conn->timeout_timer);
    conn->timeout_timer = NULL;
    conn->timeout_timer_initialized = false;

    conn->close_count--;
    if (conn->close_count == 0) {
      // Defer freeing until after loop closes to avoid use-after-free in uv_walk
      add_deferred_cleanup(conn, conn->host, conn->encoding);
    }
  }
}

// Close callback for socket cleanup
void socket_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetConnection* conn = (JSNetConnection*)handle->data;

    conn->close_count--;
    if (conn->close_count == 0) {
      // Defer freeing until after loop closes to avoid use-after-free in uv_walk
      add_deferred_cleanup(conn, conn->host, conn->encoding);
    }
  }
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
  if (!JS_IsUndefined(conn->client_request_obj)) {
    JS_FreeValueRT(rt, conn->client_request_obj);
    conn->client_request_obj = JS_UNDEFINED;
  }

  // DO NOT free encoding or host strings here - deferred cleanup will handle them
  // to avoid double-free when close callbacks also try to free them

  // Free any queued pending writes
  js_net_connection_clear_pending_writes(conn);

  // Initialize close_count if not already set (e.g., by destroy())
  if (conn->close_count == 0) {
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

    // Defer freeing even if no handles to close, to avoid use-after-free in uv_walk
    if (conn->close_count == 0) {
      add_deferred_cleanup(conn, conn->host, conn->encoding);
    }
  }
  // else: destroy() was called, close_count already set, cleanup will happen in callbacks
}

// Timer close callback for server - frees timer memory and decrements close count
void server_callback_timer_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetServer* server = (JSNetServer*)handle->data;

    // Free the timer itself
    free(server->callback_timer);
    server->callback_timer = NULL;
    server->timer_initialized = false;

    server->close_count--;
    if (server->close_count == 0) {
      // Defer freeing until after loop closes to avoid use-after-free in uv_walk
      add_deferred_cleanup(server, server->host, NULL);
    }
  }
}

// Close callback for server cleanup
void server_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetServer* server = (JSNetServer*)handle->data;

    server->close_count--;
    if (server->close_count == 0) {
      // Defer freeing until after loop closes to avoid use-after-free in uv_walk
      add_deferred_cleanup(server, server->host, NULL);
    }
  }
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

    // Only free callbacks if we're NOT in a callback
    // (callbacks will free themselves when done)
    if (!server->in_callback) {
      if (!JS_IsUndefined(server->listen_callback)) {
        JS_FreeValueRT(rt, server->listen_callback);
        server->listen_callback = JS_UNDEFINED;
      }
      if (!JS_IsUndefined(server->close_callback)) {
        JS_FreeValueRT(rt, server->close_callback);
        server->close_callback = JS_UNDEFINED;
      }
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

    // Defer freeing even if no handles to close, to avoid use-after-free in uv_walk
    if (server->close_count == 0) {
      add_deferred_cleanup(server, server->host, NULL);
    }
  }
}
