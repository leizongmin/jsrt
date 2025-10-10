#include "../../url/url.h"
#include "../../util/user_agent.h"
#include "http_internal.h"

// Server class implementation
// This file contains the HTTP Server class constructor, methods, and finalizer

// Async listen cleanup
void http_listen_async_cleanup(JSHttpListenAsync* async_op) {
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
    uv_timer_stop(&async_op->timer);
    free(async_op);
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

  // Create async operation to prevent blocking
  JSHttpListenAsync* async_op = calloc(1, sizeof(JSHttpListenAsync));
  if (!async_op) {
    return JS_ThrowOutOfMemory(ctx);
  }

  // Store operation data
  async_op->ctx = ctx;
  async_op->http_server = JS_DupValue(ctx, this_val);
  async_op->net_server = JS_DupValue(ctx, server->net_server);
  async_op->argc = argc;

  // Copy arguments to avoid stack reference issues
  if (argc > 0) {
    async_op->argv_copy = malloc(sizeof(JSValue) * argc);
    if (!async_op->argv_copy) {
      http_listen_async_cleanup(async_op);
      return JS_ThrowOutOfMemory(ctx);
    }
    for (int i = 0; i < argc; i++) {
      async_op->argv_copy[i] = JS_DupValue(ctx, argv[i]);
    }
  }

  // Initialize timer for immediate async execution (next tick)
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  uv_timer_init(rt->uv_loop, &async_op->timer);
  async_op->timer.data = async_op;

  // Start timer with 0 delay for next tick execution
  uv_timer_start(&async_op->timer, http_listen_timer_callback, 0, 0);

  // Return the server object immediately for chaining (like Node.js does)
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

// Server finalizer
void js_http_server_finalizer(JSRuntime* rt, JSValue val) {
  JSHttpServer* server = JS_GetOpaque(val, js_http_server_class_id);
  if (server) {
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

  // Add EventEmitter functionality
  setup_event_emitter_inheritance(ctx, obj);

  return obj;
}
