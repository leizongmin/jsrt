#include <ctype.h>
#include "http_internal.h"

// Helper function to normalize header names to lowercase
static char* normalize_header_name(const char* name) {
  if (!name)
    return NULL;

  size_t len = strlen(name);
  char* lower = malloc(len + 1);
  if (!lower)
    return NULL;

  for (size_t i = 0; i < len; i++) {
    lower[i] = (char)tolower((unsigned char)name[i]);
  }
  lower[len] = '\0';
  return lower;
}

// Client response parser callbacks (llhttp in HTTP_RESPONSE mode)
int client_on_message_begin(llhttp_t* parser) {
  JSHTTPClientRequest* client_req = (JSHTTPClientRequest*)parser->data;
  if (!client_req || !client_req->ctx) {
    return -1;
  }

  // Reset body buffer for new message
  if (client_req->body_buffer) {
    free(client_req->body_buffer);
    client_req->body_buffer = NULL;
    client_req->body_size = 0;
    client_req->body_capacity = 0;
  }

  return 0;
}

int client_on_status(llhttp_t* parser, const char* at, size_t length) {
  JSHTTPClientRequest* client_req = (JSHTTPClientRequest*)parser->data;
  if (!client_req || !client_req->ctx) {
    return -1;
  }

  // Store status message (will be used when creating IncomingMessage)
  // Status code is available in parser->status_code
  return 0;
}

int client_on_header_field(llhttp_t* parser, const char* at, size_t length) {
  JSHTTPClientRequest* client_req = (JSHTTPClientRequest*)parser->data;
  if (!client_req || !client_req->ctx) {
    return -1;
  }

  // Save previous header if complete
  if (client_req->current_header_field && client_req->current_header_value) {
    JSValue headers = JS_GetPropertyStr(client_req->ctx, client_req->response_obj, "headers");
    if (JS_IsObject(headers)) {
      char* lower_name = normalize_header_name(client_req->current_header_field);
      if (lower_name) {
        // Check if header already exists (multi-value headers)
        JSValue existing = JS_GetPropertyStr(client_req->ctx, headers, lower_name);
        if (JS_IsUndefined(existing)) {
          // First value
          JS_SetPropertyStr(client_req->ctx, headers, lower_name,
                            JS_NewString(client_req->ctx, client_req->current_header_value));
        } else if (JS_IsArray(client_req->ctx, existing)) {
          // Already an array, append
          uint32_t len;
          JSValue len_val = JS_GetPropertyStr(client_req->ctx, existing, "length");
          JS_ToUint32(client_req->ctx, &len, len_val);
          JS_FreeValue(client_req->ctx, len_val);
          JS_SetPropertyUint32(client_req->ctx, existing, len,
                               JS_NewString(client_req->ctx, client_req->current_header_value));
        } else {
          // Convert to array
          JSValue arr = JS_NewArray(client_req->ctx);
          JS_SetPropertyUint32(client_req->ctx, arr, 0, existing);
          JS_SetPropertyUint32(client_req->ctx, arr, 1,
                               JS_NewString(client_req->ctx, client_req->current_header_value));
          JS_SetPropertyStr(client_req->ctx, headers, lower_name, arr);
        }
        JS_FreeValue(client_req->ctx, existing);
        free(lower_name);
      }
    }
    JS_FreeValue(client_req->ctx, headers);

    free(client_req->current_header_field);
    free(client_req->current_header_value);
    client_req->current_header_field = NULL;
    client_req->current_header_value = NULL;
  }

  // Start new header field
  client_req->current_header_field = malloc(length + 1);
  if (!client_req->current_header_field) {
    return -1;
  }
  memcpy(client_req->current_header_field, at, length);
  client_req->current_header_field[length] = '\0';

  return 0;
}

int client_on_header_value(llhttp_t* parser, const char* at, size_t length) {
  JSHTTPClientRequest* client_req = (JSHTTPClientRequest*)parser->data;
  if (!client_req || !client_req->ctx) {
    return -1;
  }

  // Store header value
  if (client_req->current_header_value) {
    free(client_req->current_header_value);
  }

  client_req->current_header_value = malloc(length + 1);
  if (!client_req->current_header_value) {
    return -1;
  }
  memcpy(client_req->current_header_value, at, length);
  client_req->current_header_value[length] = '\0';

  return 0;
}

int client_on_headers_complete(llhttp_t* parser) {
  JSHTTPClientRequest* client_req = (JSHTTPClientRequest*)parser->data;
  if (!client_req || !client_req->ctx) {
    return -1;
  }

  JSContext* ctx = client_req->ctx;

  // Save last header
  if (client_req->current_header_field && client_req->current_header_value) {
    JSValue headers = JS_GetPropertyStr(ctx, client_req->response_obj, "headers");
    if (JS_IsObject(headers)) {
      char* lower_name = normalize_header_name(client_req->current_header_field);
      if (lower_name) {
        JSValue existing = JS_GetPropertyStr(ctx, headers, lower_name);
        if (JS_IsUndefined(existing)) {
          JS_SetPropertyStr(ctx, headers, lower_name, JS_NewString(ctx, client_req->current_header_value));
        } else if (JS_IsArray(ctx, existing)) {
          uint32_t len;
          JSValue len_val = JS_GetPropertyStr(ctx, existing, "length");
          JS_ToUint32(ctx, &len, len_val);
          JS_FreeValue(ctx, len_val);
          JS_SetPropertyUint32(ctx, existing, len, JS_NewString(ctx, client_req->current_header_value));
        } else {
          JSValue arr = JS_NewArray(ctx);
          JS_SetPropertyUint32(ctx, arr, 0, existing);
          JS_SetPropertyUint32(ctx, arr, 1, JS_NewString(ctx, client_req->current_header_value));
          JS_SetPropertyStr(ctx, headers, lower_name, arr);
        }
        JS_FreeValue(ctx, existing);
        free(lower_name);
      }
    }
    JS_FreeValue(ctx, headers);

    free(client_req->current_header_field);
    free(client_req->current_header_value);
    client_req->current_header_field = NULL;
    client_req->current_header_value = NULL;
  }

  // Set status code and HTTP version on response
  JS_SetPropertyStr(ctx, client_req->response_obj, "statusCode", JS_NewInt32(ctx, parser->status_code));

  char version[16];
  snprintf(version, sizeof(version), "%d.%d", parser->http_major, parser->http_minor);
  JS_SetPropertyStr(ctx, client_req->response_obj, "httpVersion", JS_NewString(ctx, version));

  // Emit 'response' event on ClientRequest
  JSValue emit = JS_GetPropertyStr(ctx, client_req->request_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "response"), JS_DupValue(ctx, client_req->response_obj)};
    JSValue result = JS_Call(ctx, emit, client_req->request_obj, 2, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
  }
  JS_FreeValue(ctx, emit);

  return 0;
}

int client_on_body(llhttp_t* parser, const char* at, size_t length) {
  JSHTTPClientRequest* client_req = (JSHTTPClientRequest*)parser->data;
  if (!client_req || !client_req->ctx) {
    return -1;
  }

  JSContext* ctx = client_req->ctx;

  // Emit 'data' event on IncomingMessage (response)
  JSValue emit = JS_GetPropertyStr(ctx, client_req->response_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    // Create Buffer from data
    JSValue buffer_data = JS_NewArrayBufferCopy(ctx, (const uint8_t*)at, length);
    JSValue args[] = {JS_NewString(ctx, "data"), buffer_data};
    JSValue result = JS_Call(ctx, emit, client_req->response_obj, 2, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
  }
  JS_FreeValue(ctx, emit);

  return 0;
}

int client_on_message_complete(llhttp_t* parser) {
  JSHTTPClientRequest* client_req = (JSHTTPClientRequest*)parser->data;
  if (!client_req || !client_req->ctx) {
    return -1;
  }

  JSContext* ctx = client_req->ctx;

  // Emit 'end' event on IncomingMessage (response)
  JSValue emit = JS_GetPropertyStr(ctx, client_req->response_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "end")};
    JSValue result = JS_Call(ctx, emit, client_req->response_obj, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);

  return 0;
}

// ClientRequest methods
JSValue js_http_client_request_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  if (client_req->headers_sent) {
    return JS_ThrowTypeError(ctx, "Cannot set headers after they are sent");
  }

  if (argc < 2) {
    return JS_ThrowTypeError(ctx, "setHeader requires name and value");
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  const char* value = JS_ToCString(ctx, argv[1]);

  if (!name || !value) {
    if (name)
      JS_FreeCString(ctx, name);
    if (value)
      JS_FreeCString(ctx, value);
    return JS_EXCEPTION;
  }

  // Normalize header name to lowercase
  char* lower_name = normalize_header_name(name);
  if (lower_name) {
    JS_SetPropertyStr(ctx, client_req->headers, lower_name, JS_NewString(ctx, value));
    free(lower_name);
  }

  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);

  return JS_UNDEFINED;
}

JSValue js_http_client_request_get_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  if (argc < 1) {
    return JS_UNDEFINED;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  char* lower_name = normalize_header_name(name);
  JSValue result = JS_UNDEFINED;

  if (lower_name) {
    result = JS_GetPropertyStr(ctx, client_req->headers, lower_name);
    free(lower_name);
  }

  JS_FreeCString(ctx, name);
  return result;
}

JSValue js_http_client_request_remove_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  if (client_req->headers_sent) {
    return JS_ThrowTypeError(ctx, "Cannot remove headers after they are sent");
  }

  if (argc < 1) {
    return JS_UNDEFINED;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_EXCEPTION;
  }

  char* lower_name = normalize_header_name(name);
  if (lower_name) {
    JSAtom atom = JS_NewAtom(ctx, lower_name);
    JS_DeleteProperty(ctx, client_req->headers, atom, 0);
    JS_FreeAtom(ctx, atom);
    free(lower_name);
  }

  JS_FreeCString(ctx, name);
  return JS_UNDEFINED;
}

// Helper to send request line and headers
static void send_headers(JSHTTPClientRequest* client_req) {
  if (client_req->headers_sent || !client_req->ctx) {
    return;
  }

  JSContext* ctx = client_req->ctx;

  // Build request line: "METHOD /path HTTP/1.1\r\n"
  char request_line[1024];
  snprintf(request_line, sizeof(request_line), "%s %s HTTP/1.1\r\n", client_req->method ? client_req->method : "GET",
           client_req->path ? client_req->path : "/");

  // Write request line to socket
  JSValue write_method = JS_GetPropertyStr(ctx, client_req->socket, "write");
  if (JS_IsFunction(ctx, write_method)) {
    JSValue args[] = {JS_NewString(ctx, request_line)};
    JSValue result = JS_Call(ctx, write_method, client_req->socket, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, write_method);

  // Write headers
  JSPropertyEnum* tab;
  uint32_t len;
  if (JS_GetOwnPropertyNames(ctx, &tab, &len, client_req->headers, JS_GPN_STRING_MASK) == 0) {
    for (uint32_t i = 0; i < len; i++) {
      JSValue key = JS_AtomToString(ctx, tab[i].atom);
      JSValue value = JS_GetProperty(ctx, client_req->headers, tab[i].atom);

      const char* key_str = JS_ToCString(ctx, key);
      const char* value_str = JS_ToCString(ctx, value);

      if (key_str && value_str) {
        char header_line[2048];
        snprintf(header_line, sizeof(header_line), "%s: %s\r\n", key_str, value_str);

        write_method = JS_GetPropertyStr(ctx, client_req->socket, "write");
        if (JS_IsFunction(ctx, write_method)) {
          JSValue args[] = {JS_NewString(ctx, header_line)};
          JSValue result = JS_Call(ctx, write_method, client_req->socket, 1, args);
          JS_FreeValue(ctx, result);
          JS_FreeValue(ctx, args[0]);
        }
        JS_FreeValue(ctx, write_method);
      }

      if (key_str)
        JS_FreeCString(ctx, key_str);
      if (value_str)
        JS_FreeCString(ctx, value_str);

      JS_FreeValue(ctx, key);
      JS_FreeValue(ctx, value);
      JS_FreeAtom(ctx, tab[i].atom);
    }
    js_free(ctx, tab);
  }

  // End headers with blank line
  write_method = JS_GetPropertyStr(ctx, client_req->socket, "write");
  if (JS_IsFunction(ctx, write_method)) {
    JSValue args[] = {JS_NewString(ctx, "\r\n")};
    JSValue result = JS_Call(ctx, write_method, client_req->socket, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, write_method);

  client_req->headers_sent = true;
}

JSValue js_http_client_request_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  if (client_req->finished) {
    return JS_ThrowTypeError(ctx, "Request already finished");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "write requires data");
  }

  // Send headers on first write if not already sent
  if (!client_req->headers_sent) {
    send_headers(client_req);
  }

  // Write data to socket
  JSValue write_method = JS_GetPropertyStr(ctx, client_req->socket, "write");
  if (JS_IsFunction(ctx, write_method)) {
    JSValue result = JS_Call(ctx, write_method, client_req->socket, argc, argv);
    JS_FreeValue(ctx, write_method);
    return result;
  }
  JS_FreeValue(ctx, write_method);

  return JS_NewBool(ctx, true);
}

JSValue js_http_client_request_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  if (client_req->finished) {
    return JS_UNDEFINED;
  }

  // Send headers if not already sent
  if (!client_req->headers_sent) {
    send_headers(client_req);
  }

  // Write final data if provided
  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    JSValue write_method = JS_GetPropertyStr(ctx, client_req->socket, "write");
    if (JS_IsFunction(ctx, write_method)) {
      JSValue result = JS_Call(ctx, write_method, client_req->socket, 1, argv);
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, write_method);
  }

  client_req->finished = true;

  // Emit 'finish' event
  JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "finish")};
    JSValue result = JS_Call(ctx, emit, this_val, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);

  return JS_UNDEFINED;
}

JSValue js_http_client_request_abort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  if (client_req->aborted) {
    return JS_UNDEFINED;
  }

  client_req->aborted = true;

  // Destroy socket immediately
  if (!JS_IsUndefined(client_req->socket)) {
    JSValue destroy_method = JS_GetPropertyStr(ctx, client_req->socket, "destroy");
    if (JS_IsFunction(ctx, destroy_method)) {
      JSValue result = JS_Call(ctx, destroy_method, client_req->socket, 0, NULL);
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, destroy_method);
  }

  // Emit 'abort' event
  JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "abort")};
    JSValue result = JS_Call(ctx, emit, this_val, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);

  return JS_UNDEFINED;
}

// Timeout callback
static void client_timeout_callback(uv_timer_t* timer) {
  JSHTTPClientRequest* client_req = (JSHTTPClientRequest*)timer->data;
  if (!client_req || !client_req->ctx) {
    return;
  }

  JSContext* ctx = client_req->ctx;

  // Emit 'timeout' event (don't auto-destroy, let user handle it)
  JSValue emit = JS_GetPropertyStr(ctx, client_req->request_obj, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "timeout")};
    JSValue result = JS_Call(ctx, emit, client_req->request_obj, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);
}

JSValue js_http_client_request_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "setTimeout requires timeout value");
  }

  int32_t timeout_ms;
  if (JS_ToInt32(ctx, &timeout_ms, argv[0]) != 0) {
    return JS_ThrowTypeError(ctx, "Invalid timeout value");
  }

  client_req->timeout_ms = timeout_ms >= 0 ? (uint32_t)timeout_ms : 0;

  // Initialize timer if needed
  if (!client_req->timeout_timer_initialized && client_req->timeout_ms > 0) {
    JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
    client_req->timeout_timer = malloc(sizeof(uv_timer_t));
    if (!client_req->timeout_timer) {
      return JS_ThrowOutOfMemory(ctx);
    }
    uv_timer_init(rt->uv_loop, client_req->timeout_timer);
    client_req->timeout_timer->data = client_req;
    client_req->timeout_timer_initialized = true;
  }

  // Start timeout timer
  if (client_req->timeout_timer_initialized && client_req->timeout_ms > 0) {
    uv_timer_start(client_req->timeout_timer, client_timeout_callback, client_req->timeout_ms, 0);
  } else if (client_req->timeout_timer_initialized) {
    uv_timer_stop(client_req->timeout_timer);
  }

  // Optional callback parameter
  if (argc > 1 && JS_IsFunction(ctx, argv[1])) {
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

  return this_val;
}

JSValue js_http_client_request_set_no_delay(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  // Forward to socket's setNoDelay
  if (!JS_IsUndefined(client_req->socket)) {
    JSValue set_no_delay = JS_GetPropertyStr(ctx, client_req->socket, "setNoDelay");
    if (JS_IsFunction(ctx, set_no_delay)) {
      JSValue result = JS_Call(ctx, set_no_delay, client_req->socket, argc, argv);
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, set_no_delay);
  }

  return this_val;
}

JSValue js_http_client_request_set_socket_keep_alive(JSContext* ctx, JSValueConst this_val, int argc,
                                                     JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  // Forward to socket's setKeepAlive
  if (!JS_IsUndefined(client_req->socket)) {
    JSValue set_keep_alive = JS_GetPropertyStr(ctx, client_req->socket, "setKeepAlive");
    if (JS_IsFunction(ctx, set_keep_alive)) {
      JSValue result = JS_Call(ctx, set_keep_alive, client_req->socket, argc, argv);
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, set_keep_alive);
  }

  return this_val;
}

JSValue js_http_client_request_flush_headers(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req) {
    return JS_ThrowTypeError(ctx, "Invalid ClientRequest object");
  }

  if (!client_req->headers_sent) {
    send_headers(client_req);
  }

  return JS_UNDEFINED;
}

// Finalizer
void js_http_client_request_finalizer(JSRuntime* rt, JSValue val) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(val, js_http_client_request_class_id);
  if (client_req) {
    // Free timer
    if (client_req->timeout_timer_initialized && client_req->timeout_timer) {
      uv_timer_stop(client_req->timeout_timer);
      uv_close((uv_handle_t*)client_req->timeout_timer, NULL);
      free(client_req->timeout_timer);
    }

    // Free strings
    free(client_req->method);
    free(client_req->host);
    free(client_req->path);
    free(client_req->protocol);
    free(client_req->current_header_field);
    free(client_req->current_header_value);
    free(client_req->body_buffer);

    // Free JSValues
    JS_FreeValueRT(rt, client_req->socket);
    JS_FreeValueRT(rt, client_req->headers);
    JS_FreeValueRT(rt, client_req->options);
    JS_FreeValueRT(rt, client_req->response_obj);

    free(client_req);
  }
}

// Constructor
JSValue js_http_client_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv) {
  JSValue obj = JS_NewObjectClass(ctx, js_http_client_request_class_id);
  if (JS_IsException(obj)) {
    return obj;
  }

  JSHTTPClientRequest* client_req = calloc(1, sizeof(JSHTTPClientRequest));
  if (!client_req) {
    JS_FreeValue(ctx, obj);
    return JS_ThrowOutOfMemory(ctx);
  }

  client_req->ctx = ctx;
  client_req->request_obj = JS_DupValue(ctx, obj);
  client_req->socket = JS_UNDEFINED;
  client_req->headers = JS_NewObject(ctx);
  client_req->options = JS_UNDEFINED;
  client_req->response_obj = JS_UNDEFINED;
  client_req->headers_sent = false;
  client_req->finished = false;
  client_req->aborted = false;
  client_req->timeout_ms = 0;
  client_req->timeout_timer = NULL;
  client_req->timeout_timer_initialized = false;
  client_req->method = strdup("GET");
  client_req->host = NULL;
  client_req->port = 80;
  client_req->path = strdup("/");
  client_req->protocol = strdup("http:");

  // Initialize parser for response
  llhttp_settings_init(&client_req->settings);
  client_req->settings.on_message_begin = client_on_message_begin;
  client_req->settings.on_status = client_on_status;
  client_req->settings.on_header_field = client_on_header_field;
  client_req->settings.on_header_value = client_on_header_value;
  client_req->settings.on_headers_complete = client_on_headers_complete;
  client_req->settings.on_body = client_on_body;
  client_req->settings.on_message_complete = client_on_message_complete;

  llhttp_init(&client_req->parser, HTTP_RESPONSE, &client_req->settings);
  client_req->parser.data = client_req;

  JS_SetOpaque(obj, client_req);

  // Add methods
  JS_SetPropertyStr(ctx, obj, "setHeader", JS_NewCFunction(ctx, js_http_client_request_set_header, "setHeader", 2));
  JS_SetPropertyStr(ctx, obj, "getHeader", JS_NewCFunction(ctx, js_http_client_request_get_header, "getHeader", 1));
  JS_SetPropertyStr(ctx, obj, "removeHeader",
                    JS_NewCFunction(ctx, js_http_client_request_remove_header, "removeHeader", 1));
  JS_SetPropertyStr(ctx, obj, "write", JS_NewCFunction(ctx, js_http_client_request_write, "write", 1));
  JS_SetPropertyStr(ctx, obj, "end", JS_NewCFunction(ctx, js_http_client_request_end, "end", 1));
  JS_SetPropertyStr(ctx, obj, "abort", JS_NewCFunction(ctx, js_http_client_request_abort, "abort", 0));
  JS_SetPropertyStr(ctx, obj, "setTimeout", JS_NewCFunction(ctx, js_http_client_request_set_timeout, "setTimeout", 2));
  JS_SetPropertyStr(ctx, obj, "setNoDelay", JS_NewCFunction(ctx, js_http_client_request_set_no_delay, "setNoDelay", 1));
  JS_SetPropertyStr(ctx, obj, "setSocketKeepAlive",
                    JS_NewCFunction(ctx, js_http_client_request_set_socket_keep_alive, "setSocketKeepAlive", 2));
  JS_SetPropertyStr(ctx, obj, "flushHeaders",
                    JS_NewCFunction(ctx, js_http_client_request_flush_headers, "flushHeaders", 0));

  // Add EventEmitter functionality
  setup_event_emitter_inheritance(ctx, obj);

  return obj;
}
