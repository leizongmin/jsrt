#include "net_internal.h"

// Server methods
JSValue js_server_listen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || server->destroyed) {
    return JS_ThrowTypeError(ctx, "Server is destroyed");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "listen requires a port");
  }

  int port = 0;
  JS_ToInt32(ctx, &port, argv[0]);
  if (port < 0 || port > 65535) {
    return JS_ThrowRangeError(ctx, "Port must be between 0 and 65535");
  }

  const char* host = "0.0.0.0";
  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    host = JS_ToCString(ctx, argv[1]);
  }

  // Store server info
  server->port = port;
  free(server->host);
  server->host = strdup(host);

  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    JS_FreeCString(ctx, host);
  }

  // Initialize TCP server with the correct event loop
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  uv_tcp_init(rt->uv_loop, &server->handle);
  server->handle.data = server;

  // Bind and listen
  struct sockaddr_in addr;
  uv_ip4_addr(server->host, server->port, &addr);

  int result = uv_tcp_bind(&server->handle, (const struct sockaddr*)&addr, 0);
  if (result < 0) {
    uv_close((uv_handle_t*)&server->handle, NULL);
    return JS_ThrowInternalError(ctx, "Bind failed: %s", uv_strerror(result));
  }

  result = uv_listen((uv_stream_t*)&server->handle, 128, on_connection);
  if (result < 0) {
    uv_close((uv_handle_t*)&server->handle, NULL);
    return JS_ThrowInternalError(ctx, "Listen failed: %s", uv_strerror(result));
  }

  server->listening = true;

  // Emit 'listening' event
  JSValue emit = JS_GetPropertyStr(ctx, server->server_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "listening")};
    JS_Call(ctx, emit, server->server_obj, 1, args);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);

  // Schedule callback for async execution if provided
  if (argc > 2 && JS_IsFunction(ctx, argv[2])) {
    // Store callback for async execution
    server->listen_callback = JS_DupValue(ctx, argv[2]);

    // Initialize timer for next tick callback execution
    JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
    uv_timer_init(rt->uv_loop, &server->callback_timer);
    server->callback_timer.data = server;
    server->timer_initialized = true;

    // Start timer with 0 delay for next tick execution
    uv_timer_start(&server->callback_timer, on_listen_callback_timer, 0, 0);
  } else {
    server->listen_callback = JS_UNDEFINED;
  }

  return this_val;
}

JSValue js_server_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || server->destroyed) {
    return JS_UNDEFINED;
  }

  if (server->listening) {
    uv_close((uv_handle_t*)&server->handle, NULL);
    server->listening = false;
  }

  server->destroyed = true;

  return JS_UNDEFINED;
}

JSValue js_server_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || !server->listening) {
    return JS_NULL;
  }

  struct sockaddr_storage addr;
  int addrlen = sizeof(addr);
  int r = uv_tcp_getsockname(&server->handle, (struct sockaddr*)&addr, &addrlen);
  if (r != 0) {
    return JS_NULL;
  }

  // Create address object { address: string, family: string, port: number }
  JSValue obj = JS_NewObject(ctx);

  char ip[INET6_ADDRSTRLEN];
  int port = 0;
  const char* family = NULL;

  if (addr.ss_family == AF_INET) {
    struct sockaddr_in* addr4 = (struct sockaddr_in*)&addr;
    uv_ip4_name(addr4, ip, sizeof(ip));
    port = ntohs(addr4->sin_port);
    family = "IPv4";
  } else if (addr.ss_family == AF_INET6) {
    struct sockaddr_in6* addr6 = (struct sockaddr_in6*)&addr;
    uv_ip6_name(addr6, ip, sizeof(ip));
    port = ntohs(addr6->sin6_port);
    family = "IPv6";
  } else {
    JS_FreeValue(ctx, obj);
    return JS_NULL;
  }

  JS_SetPropertyStr(ctx, obj, "address", JS_NewString(ctx, ip));
  JS_SetPropertyStr(ctx, obj, "family", JS_NewString(ctx, family));
  JS_SetPropertyStr(ctx, obj, "port", JS_NewInt32(ctx, port));

  return obj;
}

JSValue js_server_get_connections(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server) {
    return JS_UNDEFINED;
  }

  if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
    return JS_ThrowTypeError(ctx, "getConnections requires a callback function");
  }

  // Call callback with (err, count)
  // NOTE: Connection tracking is not fully implemented yet
  // Returning 0 for now (valid behavior per Node.js docs)
  JSValue callback = argv[0];
  JSValue args[2];
  args[0] = JS_NULL;              // No error
  args[1] = JS_NewInt32(ctx, 0);  // Return 0 for now

  JSValue result = JS_Call(ctx, callback, this_val, 2, args);
  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
  JS_FreeValue(ctx, result);

  return JS_UNDEFINED;
}

JSValue js_server_ref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || server->destroyed) {
    return this_val;
  }

  if (server->listening) {
    uv_ref((uv_handle_t*)&server->handle);
  }

  return this_val;
}

JSValue js_server_unref(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSNetServer* server = JS_GetOpaque(this_val, js_server_class_id);
  if (!server || server->destroyed) {
    return this_val;
  }

  if (server->listening) {
    uv_unref((uv_handle_t*)&server->handle);
  }

  return this_val;
}

JSValue js_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_server_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSNetServer* server = calloc(1, sizeof(JSNetServer));
  if (!server) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  server->ctx = ctx;
  server->server_obj = JS_DupValue(ctx, obj);
  server->listening = false;
  server->destroyed = false;
  server->listen_callback = JS_UNDEFINED;

  JS_SetOpaque(obj, server);

  // Add server methods
  JS_SetPropertyStr(ctx, obj, "listen", JS_NewCFunction(ctx, js_server_listen, "listen", 3));
  JS_SetPropertyStr(ctx, obj, "close", JS_NewCFunction(ctx, js_server_close, "close", 0));
  JS_SetPropertyStr(ctx, obj, "address", JS_NewCFunction(ctx, js_server_address, "address", 0));
  JS_SetPropertyStr(ctx, obj, "getConnections", JS_NewCFunction(ctx, js_server_get_connections, "getConnections", 1));
  JS_SetPropertyStr(ctx, obj, "ref", JS_NewCFunction(ctx, js_server_ref, "ref", 0));
  JS_SetPropertyStr(ctx, obj, "unref", JS_NewCFunction(ctx, js_server_unref, "unref", 0));

  // Add EventEmitter functionality
  add_event_emitter_methods(ctx, obj);

  return obj;
}
