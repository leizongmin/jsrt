#ifndef JSRT_NODE_HTTP_INCOMING_H
#define JSRT_NODE_HTTP_INCOMING_H

#include "http_internal.h"

// IncomingMessage class functions
JSValue js_http_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
void js_http_request_finalizer(JSRuntime* rt, JSValue val);

// Phase 4: Readable stream methods
JSValue js_http_incoming_pause(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_resume(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_is_paused(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_pipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_unpipe(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_read(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_incoming_set_encoding(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Helper functions for stream integration
void js_http_incoming_push_data(JSContext* ctx, JSValue incoming_msg, const char* data, size_t length);
void js_http_incoming_end(JSContext* ctx, JSValue incoming_msg);

#endif  // JSRT_NODE_HTTP_INCOMING_H
