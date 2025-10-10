#ifndef JSRT_NODE_HTTP_SERVER_H
#define JSRT_NODE_HTTP_SERVER_H

#include "http_internal.h"

// Server class functions
JSValue js_http_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_http_server_listen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_http_server_finalizer(JSRuntime* rt, JSValue val);

// Async listen helpers
void http_listen_async_cleanup(JSHttpListenAsync* async_op);
void http_listen_timer_callback(uv_timer_t* timer);

#endif  // JSRT_NODE_HTTP_SERVER_H
