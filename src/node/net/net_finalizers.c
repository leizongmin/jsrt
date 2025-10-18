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
      JSRT_Debug("add_deferred_cleanup: ptr=%p already in list, skipping", ptr);
      return;
    }
    current = current->next;
  }

  // Not in list, add it
  JSRT_Debug("add_deferred_cleanup: adding ptr=%p string1=%p string2=%p", ptr, string1, string2);
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
    JSRT_Debug("jsrt_net_cleanup_deferred: freeing ptr=%p string1=%p string2=%p", current->ptr, current->string1,
               current->string2);
    if (current->string1) {
      JSRT_Debug("jsrt_net_cleanup_deferred: freeing string1=%p", current->string1);
      free(current->string1);
    }
    if (current->string2) {
      JSRT_Debug("jsrt_net_cleanup_deferred: freeing string2=%p", current->string2);
      free(current->string2);
    }
    JSRT_Debug("jsrt_net_cleanup_deferred: freeing ptr=%p", current->ptr);
    free(current->ptr);
    JSRT_Debug("jsrt_net_cleanup_deferred: freeing node=%p", current);
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
      // Mark as -1 to indicate cleanup has been deferred
      conn->close_count = -1;
    }
  }
}

// Close callback for socket cleanup
void socket_close_callback(uv_handle_t* handle) {
  if (handle->data) {
    JSNetConnection* conn = (JSNetConnection*)handle->data;

    JSRT_Debug("socket_close_callback: conn=%p close_count=%d", conn, conn->close_count);

    conn->close_count--;
    if (conn->close_count == 0) {
      // Defer freeing until after loop closes to avoid use-after-free in uv_walk
      JSRT_Debug("socket_close_callback: deferring cleanup for conn=%p", conn);
      add_deferred_cleanup(conn, conn->host, conn->encoding);
      // Mark as -1 to indicate cleanup has been deferred
      // This prevents finalizer from trying to defer cleanup again
      conn->close_count = -1;
    }
  } else {
    JSRT_Debug("socket_close_callback: handle->data is NULL");
  }
}

// Helper to remove socket from global properties (prevents GC)
void jsrt_net_remove_active_socket_ref(JSContext* ctx, JSNetConnection* conn) {
  if (!ctx || !conn) {
    return;
  }

  // Remove the global property that keeps the socket alive
  char prop_name[64];
  snprintf(prop_name, sizeof(prop_name), "__active_socket_%p__", (void*)conn);
  JSValue global = JS_GetGlobalObject(ctx);
  JSAtom atom = JS_NewAtom(ctx, prop_name);
  JSRT_Debug("jsrt_net_remove_active_socket_ref: removing global property '%s' for conn=%p", prop_name, conn);
  JS_DeleteProperty(ctx, global, atom, 0);
  JS_FreeAtom(ctx, atom);
  JS_FreeValue(ctx, global);
}

// Class finalizers
void js_socket_finalizer(JSRuntime* rt, JSValue val) {
  JSNetConnection* conn = JS_GetOpaque(val, js_socket_class_id);
  if (!conn) {
    return;
  }

  JSRT_Debug("js_socket_finalizer: called for conn=%p connecting=%d connected=%d destroyed=%d in_callback=%d", conn,
             conn->connecting, conn->connected, conn->destroyed, conn->in_callback);

  // If socket is in a callback, we MUST NOT finalize (callback is using the socket)
  // Return early and let the callback complete
  if (conn->in_callback) {
    JSRT_Debug("js_socket_finalizer: socket is in callback, skipping finalization");
    return;
  }

  // If socket is connecting or connected, we MUST NOT finalize yet
  // There are pending libuv callbacks that will try to use this socket
  // The socket should only be finalized after it's been properly closed
  if (conn->connecting || conn->connected) {
    JSRT_Debug("js_socket_finalizer: socket is connecting/connected, skipping finalization");
    return;
  }

  // If close_count is -1, cleanup has already been deferred, skip finalization
  if (conn->close_count == -1) {
    JSRT_Debug("js_socket_finalizer: cleanup already deferred for conn=%p, skipping", conn);
    return;
  }

  // Remove from active sockets global properties if present
  // Only do this if we're actually going to finalize
  if (conn->ctx) {
    jsrt_net_remove_active_socket_ref(conn->ctx, conn);
  }

  // Socket is being garbage collected
  // Mark it as unusable for any pending callbacks
  JS_SetOpaque(val, NULL);

  // Mark socket object as invalid to prevent use-after-free in callbacks
  // Callbacks will check this and skip their work
  if (!JS_IsUndefined(conn->socket_obj)) {
    JS_FreeValueRT(rt, conn->socket_obj);
    conn->socket_obj = JS_UNDEFINED;
  }
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
    JSRT_Debug("js_socket_finalizer: close_count is 0, initiating cleanup for conn=%p", conn);
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
    bool handle_is_closing = uv_is_closing((uv_handle_t*)&conn->handle);
    JSRT_Debug("js_socket_finalizer: handle is_closing=%d for conn=%p", handle_is_closing, conn);
    if (!handle_is_closing) {
      JSRT_Debug("js_socket_finalizer: closing handle for conn=%p", conn);
      conn->close_count++;
      conn->handle.data = conn;
      uv_close((uv_handle_t*)&conn->handle, socket_close_callback);
    }

    // Defer freeing even if no handles to close, to avoid use-after-free in uv_walk
    if (conn->close_count == 0) {
      JSRT_Debug("js_socket_finalizer: no handles to close, adding to deferred cleanup for conn=%p", conn);
      add_deferred_cleanup(conn, conn->host, conn->encoding);
    }
  } else {
    JSRT_Debug("js_socket_finalizer: close_count=%d, cleanup already initiated for conn=%p", conn->close_count, conn);
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
      // Mark as -1 to indicate cleanup has been deferred
      server->close_count = -1;
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
      // Mark as -1 to indicate cleanup has been deferred
      server->close_count = -1;
    }
  }
}

void js_server_finalizer(JSRuntime* rt, JSValue val) {
  JSNetServer* server = JS_GetOpaque(val, js_server_class_id);
  if (!server) {
    return;
  }

  JSRT_Debug("js_server_finalizer: called for server=%p listening=%d destroyed=%d in_callback=%d close_count=%d",
             server, server->listening, server->destroyed, server->in_callback, server->close_count);

  // If server is in a callback, we MUST NOT finalize
  if (server->in_callback) {
    JSRT_Debug("js_server_finalizer: server is in callback, skipping finalization");
    return;
  }

  // If close_count is -1, cleanup has already been deferred, skip finalization
  if (server->close_count == -1) {
    JSRT_Debug("js_server_finalizer: cleanup already deferred for server=%p, skipping", server);
    return;
  }

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

  // Close server handle if not already closing
  if (!uv_is_closing((uv_handle_t*)&server->handle)) {
    server->close_count++;
    server->handle.data = server;
    uv_close((uv_handle_t*)&server->handle, server_close_callback);
  }

  // Defer freeing even if no handles to close, to avoid use-after-free in uv_walk
  // Exception: if handle is already closing (from explicit server.close()), we can't defer
  // because the close callback won't fire our server_close_callback
  if (server->close_count == 0) {
    bool handle_already_closing = uv_is_closing((uv_handle_t*)&server->handle);
    if (handle_already_closing) {
      // Handle is closing from explicit close(), not from finalizer
      // We can't defer cleanup because server_close_callback won't be called
      // Just free immediately - the handle will complete its close with on_server_close_complete
      JSRT_Debug("js_server_finalizer: handle already closing, freeing immediately");
      if (server->host) {
        free(server->host);
      }
      free(server);
    } else {
      // Normal path: no handles to close, defer cleanup
      add_deferred_cleanup(server, server->host, NULL);
    }
  }
}
