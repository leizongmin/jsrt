#ifndef JSRT_NODE_DGRAM_INTERNAL_H
#define JSRT_NODE_DGRAM_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../runtime.h"
#include "../node_modules.h"

// Class ID for Socket
extern JSClassID js_dgram_socket_class_id;

// Type tag for cleanup callback identification (same pattern as net module)
#define DGRAM_TYPE_SOCKET 0x44475241  // 'DGRA' in hex

// Socket state (based on JSNetConnection pattern from net module)
typedef struct {
  uint32_t type_tag;  // Must be first field for cleanup callback
  JSContext* ctx;
  JSValue socket_obj;         // JavaScript Socket object (is EventEmitter)
  uv_udp_t handle;            // libuv UDP handle
  bool bound;                 // Socket is bound
  bool connected;             // Socket is connected (connected UDP)
  bool destroyed;             // Socket destroyed
  bool receiving;             // Currently receiving
  bool in_callback;           // Prevent finalization during callback execution
  int close_count;            // Handles that need to close before freeing
  char* multicast_interface;  // Current multicast interface
  // Socket type
  int family;  // AF_INET or AF_INET6
  // Statistics
  size_t bytes_sent;
  size_t bytes_received;
  size_t messages_sent;
  size_t messages_received;
} JSDgramSocket;

// Send request state (based on uv_udp_send_t pattern)
typedef struct {
  uv_udp_send_t req;  // libuv send request
  JSContext* ctx;
  JSValue socket_obj;  // Socket reference
  JSValue callback;    // Optional callback
  char* data;          // Buffer copy
  size_t len;
} JSDgramSendReq;

// Helper function to add EventEmitter methods to an object (from net module)
void add_event_emitter_methods(JSContext* ctx, JSValue obj);

// Socket methods (from dgram_socket.c)
JSValue js_dgram_create_socket(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_bind(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

// Send operations (from dgram_send.c)
JSValue js_dgram_socket_send(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Multicast operations (from dgram_multicast.c)
JSValue js_dgram_socket_add_membership(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_drop_membership(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_multicast_ttl(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_multicast_interface(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_multicast_loopback(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_add_source_specific_membership(JSContext* ctx, JSValueConst this_val, int argc,
                                                       JSValueConst* argv);
JSValue js_dgram_socket_drop_source_specific_membership(JSContext* ctx, JSValueConst this_val, int argc,
                                                        JSValueConst* argv);

// Socket options (from dgram_options.c)
JSValue js_dgram_socket_set_broadcast(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_ttl(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_get_send_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_get_recv_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_send_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_set_recv_buffer_size(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Connected UDP (optional, for future implementation)
JSValue js_dgram_socket_connect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_disconnect(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_dgram_socket_remote_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Callbacks (from dgram_callbacks.c)
void on_dgram_send(uv_udp_send_t* req, int status);
void on_dgram_recv(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);
void on_dgram_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

// Finalizers (from dgram_finalizers.c)
void dgram_socket_close_callback(uv_handle_t* handle);
void js_dgram_socket_finalizer(JSRuntime* rt, JSValue val);

// Property getters
JSValue js_dgram_socket_get_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

#endif  // JSRT_NODE_DGRAM_INTERNAL_H
