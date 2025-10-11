#include "../../url/url.h"
#include "../../util/user_agent.h"
#include "http_internal.h"

// Server class implementation
// This file contains the HTTP Server class constructor, methods, and finalizer

// Timer close callback - called when timer handle is safely closed
static void http_listen_timer_close_cb(uv_handle_t* handle) {
  JSHttpListenAsync* async_op = (JSHttpListenAsync*)handle->data;
  if (async_op) {
    if (async_op->ctx) {
      JS_FreeValue(async_op->ctx, async_op->http_server);
      JS_FreeValue(async_op->ctx, async_op->net_server);

      // Free copied arguments
      if (async_op->argv_copy) {
        for (int i = 0; i < async_op->argc; i++) {
          JS_FreeValue(async_op->ctx, async_op->argv_copy[i]);
        }
        free(async_op->argv_copy);
      }
    }
    // Now it's safe to free the async_op structure
    free(async_op);
  }
}

// Async listen cleanup - properly close timer before freeing
void http_listen_async_cleanup(JSHttpListenAsync* async_op) {
  if (async_op) {
    // CRITICAL FIX #1.1: Must use uv_close() instead of just uv_timer_stop()
    // uv_close() is async, so actual cleanup happens in close callback
    uv_close((uv_handle_t*)&async_op->timer, http_listen_timer_close_cb);
  }
}

// Async listen timer callback
void http_listen_timer_callback(uv_timer_t* timer) {
  JSHttpListenAsync* async_op = (JSHttpListenAsync*)timer->data;
  if (!async_op || !async_op->ctx) {
    return;
  }

  JSContext* ctx = async_op->ctx;

  // Call the net server's listen method asynchronously
  JSValue listen_method = JS_GetPropertyStr(ctx, async_op->net_server, "listen");
  if (JS_IsFunction(ctx, listen_method)) {
    JSValue result = JS_Call(ctx, listen_method, async_op->net_server, async_op->argc, async_op->argv_copy);
    JS_FreeValue(ctx, result);
  }
  JS_FreeValue(ctx, listen_method);

  // Cleanup
  http_listen_async_cleanup(async_op);
}

// Server listen method
JSValue js_http_server_listen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpServer* server = JS_GetOpaque(this_val, js_http_server_class_id);
  if (!server) {
    return JS_ThrowTypeError(ctx, "Invalid server object");
  }

  // Parse arguments to match Node.js http.Server.listen() overloads:
  // listen(port[, host][, backlog][, callback])
  // listen(port[, callback])
  // The callback can be at any position after port

  JSValue net_args[3] = {JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED};  // port, host, callback
  int net_argc = 0;

  if (argc > 0) {
    net_args[0] = argv[0];  // port
    net_argc = 1;

    // Check remaining arguments for host (string) and callback (function)
    for (int i = 1; i < argc && i < 3; i++) {
      if (JS_IsFunction(ctx, argv[i])) {
        // Found callback - always goes to position 2 (third arg)
        net_args[2] = argv[i];
        net_argc = 3;
      } else if (JS_IsString(argv[i]) && JS_IsUndefined(net_args[1])) {
        // Found host string - goes to position 1 (second arg)
        net_args[1] = argv[i];
        if (net_argc < 2)
          net_argc = 2;
      }
    }
  }

  // Call net.Server.listen with properly arranged arguments
  JSValue listen_method = JS_GetPropertyStr(ctx, server->net_server, "listen");
  if (JS_IsFunction(ctx, listen_method)) {
    JSValue result = JS_Call(ctx, listen_method, server->net_server, net_argc, net_args);
    JS_FreeValue(ctx, listen_method);

    // Check for errors
    if (JS_IsException(result)) {
      return result;
    }
    JS_FreeValue(ctx, result);
  } else {
    JS_FreeValue(ctx, listen_method);
    return JS_ThrowTypeError(ctx, "net.Server.listen is not a function");
  }

  // Return the HTTP server object for chaining (like Node.js does)
  return JS_DupValue(ctx, this_val);
}

// Server close method
JSValue js_http_server_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpServer* server = JS_GetOpaque(this_val, js_http_server_class_id);
  if (!server) {
    return JS_UNDEFINED;
  }

  if (!server->destroyed) {
    JSValue close_method = JS_GetPropertyStr(ctx, server->net_server, "close");
    JS_Call(ctx, close_method, server->net_server, argc, argv);
    JS_FreeValue(ctx, close_method);
    server->destroyed = true;
  }

  return JS_UNDEFINED;
}

// Server address method - delegates to net.Server
JSValue js_http_server_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpServer* server = JS_GetOpaque(this_val, js_http_server_class_id);
  if (!server) {
    return JS_NULL;
  }

  // Call net.Server's address() method
  JSValue address_method = JS_GetPropertyStr(ctx, server->net_server, "address");
  if (JS_IsFunction(ctx, address_method)) {
    JSValue result = JS_Call(ctx, address_method, server->net_server, 0, NULL);
    JS_FreeValue(ctx, address_method);
    return result;
  }

  JS_FreeValue(ctx, address_method);
  return JS_NULL;
}

// Server setTimeout method
JSValue js_http_server_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpServer* server = JS_GetOpaque(this_val, js_http_server_class_id);
  if (!server) {
    return JS_ThrowTypeError(ctx, "Invalid server object");
  }

  if (argc > 0) {
    int32_t timeout_ms;
    if (JS_ToInt32(ctx, &timeout_ms, argv[0]) != 0) {
      return JS_ThrowTypeError(ctx, "Invalid timeout value");
    }
    server->timeout_ms = timeout_ms >= 0 ? (uint32_t)timeout_ms : 0;
  }

  // Optional callback parameter (for compatibility)
  if (argc > 1 && JS_IsFunction(ctx, argv[1])) {
    // Register as 'timeout' event listener
    JSValue on_method = JS_GetPropertyStr(ctx, this_val, "on");
    if (JS_IsFunction(ctx, on_method)) {
      JSValue args[] = {JS_NewString(ctx, "timeout"), JS_DupValue(ctx, argv[1])};
      JSValue result = JS_Call(ctx, on_method, this_val, 2, args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
    }
    JS_FreeValue(ctx, on_method);
  }

  // Return server for chaining
  return JS_DupValue(ctx, this_val);
}

// Server finalizer
void js_http_server_finalizer(JSRuntime* rt, JSValue val) {
  JSHttpServer* server = JS_GetOpaque(val, js_http_server_class_id);
  if (server) {
    // CRITICAL FIX #1.4: Clean up connection handler wrapper
    if (server->conn_wrapper) {
      JS_FreeValueRT(rt, server->conn_wrapper->server);
      free(server->conn_wrapper);
      server->conn_wrapper = NULL;
    }

    JS_FreeValueRT(rt, server->net_server);
    free(server);
  }
}

// Server constructor
JSValue js_http_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_http_server_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSHttpServer* server = calloc(1, sizeof(JSHttpServer));
  if (!server) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  server->ctx = ctx;
  server->server_obj = JS_DupValue(ctx, obj);
  server->destroyed = false;
  server->timeout_ms = 0;       // No timeout by default
  server->conn_wrapper = NULL;  // CRITICAL FIX #1.4: Initialize wrapper pointer

  // Create underlying net.Server
  JSValue net_module = JSRT_LoadNodeModuleCommonJS(ctx, "net");
  if (JS_IsException(net_module)) {
    free(server);
    JS_FreeValue(ctx, obj);
    return net_module;
  }

  JSValue create_server = JS_GetPropertyStr(ctx, net_module, "createServer");
  server->net_server = JS_Call(ctx, create_server, JS_UNDEFINED, 0, NULL);
  JS_FreeValue(ctx, create_server);
  JS_FreeValue(ctx, net_module);

  if (JS_IsException(server->net_server)) {
    free(server);
    JS_FreeValue(ctx, obj);
    return server->net_server;
  }

  JS_SetOpaque(obj, server);

  // Add HTTP server methods
  JS_SetPropertyStr(ctx, obj, "listen", JS_NewCFunction(ctx, js_http_server_listen, "listen", 3));
  JS_SetPropertyStr(ctx, obj, "close", JS_NewCFunction(ctx, js_http_server_close, "close", 0));
  JS_SetPropertyStr(ctx, obj, "address", JS_NewCFunction(ctx, js_http_server_address, "address", 0));
  JS_SetPropertyStr(ctx, obj, "setTimeout", JS_NewCFunction(ctx, js_http_server_set_timeout, "setTimeout", 2));

  // Add EventEmitter functionality
  setup_event_emitter_inheritance(ctx, obj);

  return obj;
}
