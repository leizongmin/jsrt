#ifndef JSRT_NODE_NET_INTERNAL_H
#define JSRT_NODE_NET_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../runtime.h"
#include "../node_modules.h"

// Class IDs for networking classes
extern JSClassID js_server_class_id;
extern JSClassID js_socket_class_id;

// Connection state
typedef struct {
  JSContext* ctx;
  JSValue server_obj;
  JSValue socket_obj;
  uv_tcp_t handle;
  uv_connect_t connect_req;
  uv_shutdown_t shutdown_req;
  uv_timer_t* timeout_timer;  // Allocated pointer instead of embedded handle
  char* host;
  int port;
  bool connected;
  bool destroyed;
  bool connecting;
  bool paused;
  bool in_callback;  // Prevent finalization during callback execution
  bool timeout_enabled;
  bool timeout_timer_initialized;  // Track if timer was allocated and initialized
  int close_count;                 // Number of handles that need to close before freeing
  unsigned int timeout_ms;
  size_t bytes_read;
  size_t bytes_written;
  bool had_error;  // Track error state for close event
} JSNetConnection;

// Server state
typedef struct {
  JSContext* ctx;
  JSValue server_obj;
  uv_tcp_t handle;
  bool listening;
  bool destroyed;
  bool in_callback;        // Flag to prevent double-free during callback
  bool timer_initialized;  // Track if timer was initialized
  char* host;
  int port;
  JSValue listen_callback;     // Store callback for async execution
  uv_timer_t* callback_timer;  // Allocated pointer instead of embedded handle
} JSNetServer;

// Helper function to add EventEmitter methods to an object
void add_event_emitter_methods(JSContext* ctx, JSValue obj);

// Callback functions (from net_callbacks.c)
void on_socket_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void on_socket_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
void on_connection(uv_stream_t* server, int status);
void on_connect(uv_connect_t* req, int status);
void on_socket_timeout(uv_timer_t* timer);
void on_listen_callback_timer(uv_timer_t* timer);
void on_socket_write_complete(uv_write_t* req, int status);

// Timer cleanup helpers (from net_finalizers.c)
void socket_timeout_timer_close_callback(uv_handle_t* handle);
void server_callback_timer_close_callback(uv_handle_t* handle);

// Socket methods (from net_socket.c)
JSValue js_socket_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_set_keep_alive(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_set_no_delay(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

// Server methods (from net_server.c)
JSValue js_server_listen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_server_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_server_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_server_get_connections(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_server_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_server_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

// Property getters (from net_properties.c)
JSValue js_socket_get_local_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_local_port(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_local_family(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_remote_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_remote_port(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_remote_family(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_bytes_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_bytes_written(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_connecting(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_pending(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_ready_state(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_socket_get_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Finalizers (from net_finalizers.c)
void socket_close_callback(uv_handle_t* handle);
void js_socket_finalizer(JSRuntime* rt, JSValue val);
void server_close_callback(uv_handle_t* handle);
void server_timer_close_callback(uv_handle_t* handle);
void js_server_finalizer(JSRuntime* rt, JSValue val);

// Module functions (from net_module.c)
JSValue js_net_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_net_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

#endif  // JSRT_NODE_NET_INTERNAL_H
