#ifndef JSRT_NODE_HTTP_RESPONSE_H
#define JSRT_NODE_HTTP_RESPONSE_H

#include "http_internal.h"

// ServerResponse class functions
JSValue js_http_response_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_http_response_write_head(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_http_response_finalizer(JSRuntime* rt, JSValue val);

#endif  // JSRT_NODE_HTTP_RESPONSE_H
