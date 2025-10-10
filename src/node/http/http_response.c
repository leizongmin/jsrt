#include "../../util/user_agent.h"
#include "http_internal.h"

// ServerResponse class implementation
// This file contains the HTTP ServerResponse class

// Response writeHead method
JSValue js_http_response_write_head(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || res->headers_sent) {
    return JS_ThrowTypeError(ctx, "Headers already sent");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "writeHead requires status code");
  }

  JS_ToInt32(ctx, &res->status_code, argv[0]);

  if (argc > 1 && !JS_IsUndefined(argv[1])) {
    const char* message = JS_ToCString(ctx, argv[1]);
    if (message) {
      free(res->status_message);
      res->status_message = strdup(message);
      JS_FreeCString(ctx, message);
    }
  }

  if (argc > 2 && JS_IsObject(argv[2])) {
    // Set headers from object
    res->headers = JS_DupValue(ctx, argv[2]);
  }

  return JS_UNDEFINED;
}

// Response write method
JSValue js_http_response_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  if (!res->headers_sent) {
    // Send headers first
    if (res->status_code == 0)
      res->status_code = 200;
    if (!res->status_message)
      res->status_message = strdup("OK");

    char status_line[1024];
    // Use dynamic user-agent for Server header
    char* server_header = jsrt_generate_user_agent(ctx);
    snprintf(status_line, sizeof(status_line),
             "HTTP/1.1 %d %s\r\nContent-Type: text/plain\r\nConnection: close\r\nServer: %s\r\n\r\n", res->status_code,
             res->status_message, server_header);
    free(server_header);

    // Write headers to socket
    if (!JS_IsUndefined(res->socket)) {
      JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
      if (JS_IsFunction(ctx, write_method)) {
        JSValue header_data = JS_NewString(ctx, status_line);
        JS_Call(ctx, write_method, res->socket, 1, &header_data);
        JS_FreeValue(ctx, header_data);
      }
      JS_FreeValue(ctx, write_method);
    }

    res->headers_sent = true;
  }

  if (argc > 0) {
    const char* data = JS_ToCString(ctx, argv[0]);
    if (data && !JS_IsUndefined(res->socket)) {
      // Write data to socket
      JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
      if (JS_IsFunction(ctx, write_method)) {
        JSValue body_data = JS_NewString(ctx, data);
        JS_Call(ctx, write_method, res->socket, 1, &body_data);
        JS_FreeValue(ctx, body_data);
      }
      JS_FreeValue(ctx, write_method);
      JS_FreeCString(ctx, data);
    }
  }

  return JS_NewBool(ctx, true);
}

// Response end method
JSValue js_http_response_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  if (argc > 0) {
    // Write final data
    js_http_response_write(ctx, this_val, argc, argv);
  }

  // Ensure headers are sent even for empty response
  if (!res->headers_sent) {
    js_http_response_write(ctx, this_val, 0, NULL);
  }

  // End the response by closing the connection
  if (!JS_IsUndefined(res->socket)) {
    JSValue end_method = JS_GetPropertyStr(ctx, res->socket, "end");
    if (JS_IsFunction(ctx, end_method)) {
      JS_Call(ctx, end_method, res->socket, 0, NULL);
    }
    JS_FreeValue(ctx, end_method);
  }

  return JS_UNDEFINED;
}

// Response setHeader method
JSValue js_http_response_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || res->headers_sent) {
    return JS_ThrowTypeError(ctx, "Headers already sent");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "setHeader requires name and value");
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  const char* value = JS_ToCString(ctx, argv[1]);

  if (name && value) {
    JS_SetPropertyStr(ctx, res->headers, name, JS_NewString(ctx, value));
  }

  if (name)
    JS_FreeCString(ctx, name);
  if (value)
    JS_FreeCString(ctx, value);

  return JS_UNDEFINED;
}

// ServerResponse constructor
JSValue js_http_response_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_http_response_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSHttpResponse* res = calloc(1, sizeof(JSHttpResponse));
  if (!res) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  res->ctx = ctx;
  res->response_obj = JS_DupValue(ctx, obj);
  res->headers_sent = false;
  res->status_code = 0;
  res->headers = JS_NewObject(ctx);

  JS_SetOpaque(obj, res);

  // Add response methods
  JS_SetPropertyStr(ctx, obj, "writeHead", JS_NewCFunction(ctx, js_http_response_write_head, "writeHead", 3));
  JS_SetPropertyStr(ctx, obj, "write", JS_NewCFunction(ctx, js_http_response_write, "write", 1));
  JS_SetPropertyStr(ctx, obj, "end", JS_NewCFunction(ctx, js_http_response_end, "end", 1));
  JS_SetPropertyStr(ctx, obj, "setHeader", JS_NewCFunction(ctx, js_http_response_set_header, "setHeader", 2));

  // Add properties
  JS_SetPropertyStr(ctx, obj, "statusCode", JS_NewInt32(ctx, 200));
  JS_SetPropertyStr(ctx, obj, "statusMessage", JS_NewString(ctx, "OK"));
  JS_SetPropertyStr(ctx, obj, "headersSent", JS_NewBool(ctx, false));

  return obj;
}

// ServerResponse finalizer
void js_http_response_finalizer(JSRuntime* rt, JSValue val) {
  JSHttpResponse* res = JS_GetOpaque(val, js_http_response_class_id);
  if (res) {
    free(res->status_message);
    JS_FreeValueRT(rt, res->headers);
    JS_FreeValueRT(rt, res->socket);
    free(res);
  }
}
