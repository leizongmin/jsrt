#include "../../util/debug.h"
#include "dgram_internal.h"

// Socket class definition
static JSClassDef js_dgram_socket_class = {
    "Socket",
    .finalizer = js_dgram_socket_finalizer,
};

// CommonJS module initialization
JSValue JSRT_InitNodeDgram(JSContext* ctx) {
  JSValue dgram = JS_NewObject(ctx);

  // Export createSocket function
  JS_SetPropertyStr(ctx, dgram, "createSocket", JS_NewCFunction(ctx, js_dgram_create_socket, "createSocket", 2));

  // Export Socket constructor
  JS_NewClassID(&js_dgram_socket_class_id);
  JS_NewClass(JS_GetRuntime(ctx), js_dgram_socket_class_id, &js_dgram_socket_class);

  JSValue socket_proto = JS_NewObject(ctx);

  // Add Socket methods
  JS_SetPropertyStr(ctx, socket_proto, "bind", JS_NewCFunction(ctx, js_dgram_socket_bind, "bind", 3));
  JS_SetPropertyStr(ctx, socket_proto, "send", JS_NewCFunction(ctx, js_dgram_socket_send, "send", 6));
  JS_SetPropertyStr(ctx, socket_proto, "close", JS_NewCFunction(ctx, js_dgram_socket_close, "close", 1));
  JS_SetPropertyStr(ctx, socket_proto, "address", JS_NewCFunction(ctx, js_dgram_socket_address, "address", 0));
  JS_SetPropertyStr(ctx, socket_proto, "ref", JS_NewCFunction(ctx, js_dgram_socket_ref, "ref", 0));
  JS_SetPropertyStr(ctx, socket_proto, "unref", JS_NewCFunction(ctx, js_dgram_socket_unref, "unref", 0));

  // Multicast methods
  JS_SetPropertyStr(ctx, socket_proto, "addMembership",
                    JS_NewCFunction(ctx, js_dgram_socket_add_membership, "addMembership", 2));
  JS_SetPropertyStr(ctx, socket_proto, "dropMembership",
                    JS_NewCFunction(ctx, js_dgram_socket_drop_membership, "dropMembership", 2));
  JS_SetPropertyStr(ctx, socket_proto, "setMulticastTTL",
                    JS_NewCFunction(ctx, js_dgram_socket_set_multicast_ttl, "setMulticastTTL", 1));
  JS_SetPropertyStr(ctx, socket_proto, "setMulticastInterface",
                    JS_NewCFunction(ctx, js_dgram_socket_set_multicast_interface, "setMulticastInterface", 1));
  JS_SetPropertyStr(ctx, socket_proto, "setMulticastLoopback",
                    JS_NewCFunction(ctx, js_dgram_socket_set_multicast_loopback, "setMulticastLoopback", 1));
  JS_SetPropertyStr(
      ctx, socket_proto, "addSourceSpecificMembership",
      JS_NewCFunction(ctx, js_dgram_socket_add_source_specific_membership, "addSourceSpecificMembership", 3));
  JS_SetPropertyStr(
      ctx, socket_proto, "dropSourceSpecificMembership",
      JS_NewCFunction(ctx, js_dgram_socket_drop_source_specific_membership, "dropSourceSpecificMembership", 3));

  // Socket option methods
  JS_SetPropertyStr(ctx, socket_proto, "setBroadcast",
                    JS_NewCFunction(ctx, js_dgram_socket_set_broadcast, "setBroadcast", 1));
  JS_SetPropertyStr(ctx, socket_proto, "setTTL", JS_NewCFunction(ctx, js_dgram_socket_set_ttl, "setTTL", 1));
  JS_SetPropertyStr(ctx, socket_proto, "getSendBufferSize",
                    JS_NewCFunction(ctx, js_dgram_socket_get_send_buffer_size, "getSendBufferSize", 0));
  JS_SetPropertyStr(ctx, socket_proto, "getRecvBufferSize",
                    JS_NewCFunction(ctx, js_dgram_socket_get_recv_buffer_size, "getRecvBufferSize", 0));
  JS_SetPropertyStr(ctx, socket_proto, "setSendBufferSize",
                    JS_NewCFunction(ctx, js_dgram_socket_set_send_buffer_size, "setSendBufferSize", 1));
  JS_SetPropertyStr(ctx, socket_proto, "setRecvBufferSize",
                    JS_NewCFunction(ctx, js_dgram_socket_set_recv_buffer_size, "setRecvBufferSize", 1));

  // Connected UDP methods (optional, stubs for now)
  JS_SetPropertyStr(ctx, socket_proto, "connect", JS_NewCFunction(ctx, js_dgram_socket_connect, "connect", 3));
  JS_SetPropertyStr(ctx, socket_proto, "disconnect", JS_NewCFunction(ctx, js_dgram_socket_disconnect, "disconnect", 0));
  JS_SetPropertyStr(ctx, socket_proto, "remoteAddress",
                    JS_NewCFunction(ctx, js_dgram_socket_remote_address, "remoteAddress", 0));

  JS_SetClassProto(ctx, js_dgram_socket_class_id, socket_proto);

  JSValue socket_constructor = JS_NewCFunction2(ctx, js_dgram_socket_constructor, "Socket", 1, JS_CFUNC_constructor, 0);
  JS_SetPropertyStr(ctx, dgram, "Socket", socket_constructor);

  return dgram;
}

// ES Module initialization
int js_node_dgram_init(JSContext* ctx, JSModuleDef* m) {
  JSValue dgram = JSRT_InitNodeDgram(ctx);

  // Set module exports
  JS_SetModuleExport(ctx, m, "default", JS_DupValue(ctx, dgram));
  JS_SetModuleExport(ctx, m, "createSocket", JS_GetPropertyStr(ctx, dgram, "createSocket"));
  JS_SetModuleExport(ctx, m, "Socket", JS_GetPropertyStr(ctx, dgram, "Socket"));

  JS_FreeValue(ctx, dgram);
  return 0;
}
