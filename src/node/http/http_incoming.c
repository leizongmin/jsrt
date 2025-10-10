#include "http_internal.h"

// IncomingMessage class implementation
// This file contains the HTTP IncomingMessage (request) class

// IncomingMessage constructor
JSValue js_http_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_http_request_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSHttpRequest* req = calloc(1, sizeof(JSHttpRequest));
  if (!req) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  req->ctx = ctx;
  req->request_obj = JS_DupValue(ctx, obj);
  req->headers = JS_NewObject(ctx);

  JS_SetOpaque(obj, req);

  // Add default properties
  JS_SetPropertyStr(ctx, obj, "method", JS_NewString(ctx, "GET"));
  JS_SetPropertyStr(ctx, obj, "url", JS_NewString(ctx, "/"));
  JS_SetPropertyStr(ctx, obj, "httpVersion", JS_NewString(ctx, "1.1"));
  JS_SetPropertyStr(ctx, obj, "headers", JS_DupValue(ctx, req->headers));

  return obj;
}

// IncomingMessage finalizer
void js_http_request_finalizer(JSRuntime* rt, JSValue val) {
  JSHttpRequest* req = JS_GetOpaque(val, js_http_request_class_id);
  if (req) {
    free(req->method);
    free(req->url);
    free(req->http_version);
    JS_FreeValueRT(rt, req->headers);
    JS_FreeValueRT(rt, req->socket);
    free(req);
  }
}
