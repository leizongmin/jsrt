#ifndef JSRT_NODE_HTTP_INCOMING_H
#define JSRT_NODE_HTTP_INCOMING_H

#include "http_internal.h"

// IncomingMessage class functions
JSValue js_http_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
void js_http_request_finalizer(JSRuntime* rt, JSValue val);

#endif  // JSRT_NODE_HTTP_INCOMING_H
