#include <ctype.h>
#include "../../util/debug.h"
#include "../net/net_internal.h"
#include "http_incoming.h"
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

  JSRT_Debug_Truncated("[debug] headers complete status=%d\n", parser->status_code);

  JSRT_Debug_Truncated("[debug] headers complete\n");

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
    JSValue chunk = JS_NewStringLen(ctx, at, length);
    JSValue args[] = {JS_NewString(ctx, "data"), chunk};
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
  JSRT_Debug_Truncated("[debug] client_on_message_complete\n");

  js_http_incoming_end(ctx, client_req->response_obj);

  if (!JS_IsUndefined(client_req->socket)) {
    JSValue socket_val = JS_DupValue(ctx, client_req->socket);
    JSRT_Debug_Truncated("[debug] client_on_message_complete socket_val tag=%d\n", JS_VALUE_GET_TAG(socket_val));
    fflush(stderr);

    JSNetConnection* socket_conn = JS_GetOpaque(socket_val, js_socket_class_id);
    if (socket_conn) {
      socket_conn->is_http_client = false;
      if (!JS_IsUndefined(socket_conn->client_request_obj)) {
        JS_FreeValue(ctx, socket_conn->client_request_obj);
        socket_conn->client_request_obj = JS_UNDEFINED;
      }
    }

    JSValue end_method = JS_GetPropertyStr(ctx, socket_val, "end");
    if (JS_IsFunction(ctx, end_method)) {
      JSValue result = JS_Call(ctx, end_method, socket_val, 0, NULL);
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, end_method);

    JSValue unref_method = JS_GetPropertyStr(ctx, socket_val, "unref");
    if (JS_IsFunction(ctx, unref_method)) {
      JSValue result = JS_Call(ctx, unref_method, socket_val, 0, NULL);
      JS_FreeValue(ctx, result);
    }
    JS_FreeValue(ctx, unref_method);

    JSValue destroy_result = js_socket_destroy(ctx, socket_val, 0, NULL);
    JS_FreeValue(ctx, destroy_result);

    client_req->socket = JS_UNDEFINED;
    JS_FreeValue(ctx, socket_val);
  }

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

// Phase 4.3: Helper to write data to socket with optional chunked encoding
static void write_to_socket(JSHTTPClientRequest* client_req, const char* data, size_t data_len) {
  if (!client_req || !client_req->ctx || JS_IsUndefined(client_req->socket)) {
    return;
  }

  JSContext* ctx = client_req->ctx;
  JSValue write_method = JS_GetPropertyStr(ctx, client_req->socket, "write");

  if (JS_IsFunction(ctx, write_method)) {
    if (client_req->use_chunked && client_req->headers_sent && data_len > 0) {
      // Write chunk size in hex + CRLF
      char chunk_header[32];
      snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", data_len);

      JSValue header_str = JS_NewString(ctx, chunk_header);
      JSValue result = JS_Call(ctx, write_method, client_req->socket, 1, &header_str);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, header_str);

      // Write chunk data
      JSValue data_str = JS_NewStringLen(ctx, data, data_len);
      result = JS_Call(ctx, write_method, client_req->socket, 1, &data_str);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, data_str);

      // Write trailing CRLF
      JSValue crlf = JS_NewString(ctx, "\r\n");
      result = JS_Call(ctx, write_method, client_req->socket, 1, &crlf);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, crlf);
    } else {
      // Write data directly (not chunked or before headers sent)
      JSValue data_str = JS_NewStringLen(ctx, data, data_len);
      JSValue result = JS_Call(ctx, write_method, client_req->socket, 1, &data_str);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, data_str);
    }
  }
  JS_FreeValue(ctx, write_method);
}

// Helper to send request line and headers
void send_headers(JSHTTPClientRequest* client_req) {
  if (client_req->headers_sent || !client_req->ctx) {
    return;
  }

  JSContext* ctx = client_req->ctx;

  // Phase 4.3: Check if we should use chunked encoding
  // Use chunked if no Content-Length header is set
  JSValue content_length = JS_GetPropertyStr(ctx, client_req->headers, "content-length");
  if (JS_IsUndefined(content_length)) {
    client_req->use_chunked = true;
    // Add Transfer-Encoding: chunked header
    JS_SetPropertyStr(ctx, client_req->headers, "transfer-encoding", JS_NewString(ctx, "chunked"));
  }
  JS_FreeValue(ctx, content_length);

  // Build request line: "METHOD /path HTTP/1.1\r\n"
  char request_line[1024];
  snprintf(request_line, sizeof(request_line), "%s %s HTTP/1.1\r\n", client_req->method ? client_req->method : "GET",
           client_req->path ? client_req->path : "/");

  // Write request line to socket
  write_to_socket(client_req, request_line, strlen(request_line));

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
        write_to_socket(client_req, header_line, strlen(header_line));
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
  write_to_socket(client_req, "\r\n", 2);

  client_req->headers_sent = true;
}

// Phase 4.3: Public write method delegates to stream or direct write
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

  // Get data as string
  const char* data = JS_ToCString(ctx, argv[0]);
  if (!data) {
    return JS_EXCEPTION;
  }

  size_t data_len = strlen(data);
  write_to_socket(client_req, data, data_len);
  JS_FreeCString(ctx, data);

  // Phase 4.3: Return true (no backpressure tracking yet)
  return JS_NewBool(ctx, true);
}

// Phase 4.3: Public end method with chunked terminator support
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
    const char* data = JS_ToCString(ctx, argv[0]);
    if (data) {
      size_t data_len = strlen(data);
      write_to_socket(client_req, data, data_len);
      JS_FreeCString(ctx, data);
    }
  }

  // Phase 4.3: Send chunked encoding terminator if using chunked
  if (client_req->use_chunked) {
    // Send terminator directly to bypass chunked encoding wrapper
    JSValue write_method = JS_GetPropertyStr(ctx, client_req->socket, "write");
    if (JS_IsFunction(ctx, write_method)) {
      JSValue terminator = JS_NewString(ctx, "0\r\n\r\n");
      JSValue result = JS_Call(ctx, write_method, client_req->socket, 1, &terminator);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, terminator);
    }
    JS_FreeValue(ctx, write_method);
  }

  client_req->finished = true;

  // Phase 4.3: Update stream state if stream is present
  if (client_req->stream) {
    client_req->stream->writable_ended = true;
    client_req->stream->writable_finished = true;
  }

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

// Phase 4.3: Writable stream methods for ClientRequest

JSValue js_http_client_request_cork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req || !client_req->stream) {
    return JS_UNDEFINED;
  }

  client_req->stream->writable_corked++;
  return JS_UNDEFINED;
}

JSValue js_http_client_request_uncork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req || !client_req->stream) {
    return JS_UNDEFINED;
  }

  if (client_req->stream->writable_corked > 0) {
    client_req->stream->writable_corked--;
  }
  return JS_UNDEFINED;
}

JSValue js_http_client_request_writable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req || !client_req->stream) {
    return JS_NewBool(ctx, !client_req || !client_req->finished);
  }
  return JS_NewBool(ctx, client_req->stream->writable);
}

JSValue js_http_client_request_writable_ended(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req || !client_req->stream) {
    return JS_NewBool(ctx, client_req && client_req->finished);
  }
  return JS_NewBool(ctx, client_req->stream->writable_ended);
}

JSValue js_http_client_request_writable_finished(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(this_val, js_http_client_request_class_id);
  if (!client_req || !client_req->stream) {
    return JS_NewBool(ctx, client_req && client_req->finished);
  }
  return JS_NewBool(ctx, client_req->stream->writable_finished);
}

// Timer close callback - called asynchronously by libuv
static void http_timer_close_callback(uv_handle_t* handle) {
  if (handle) {
    free(handle);
  }
}

// Finalizer
void js_http_client_request_finalizer(JSRuntime* rt, JSValue val) {
  JSHTTPClientRequest* client_req = JS_GetOpaque(val, js_http_client_request_class_id);
  if (client_req) {
    // Free timer with proper async handling
    // CRITICAL FIX #1.1: uv_close() is async, must not free() immediately
    if (client_req->timeout_timer_initialized && client_req->timeout_timer) {
      uv_timer_stop(client_req->timeout_timer);
      uv_close((uv_handle_t*)client_req->timeout_timer, http_timer_close_callback);
      client_req->timeout_timer = NULL;  // Prevent double-free
      client_req->timeout_timer_initialized = false;
    }

    // Phase 4.3: Free stream data
    if (client_req->stream) {
      if (client_req->stream->buffered_data) {
        for (size_t i = 0; i < client_req->stream->buffer_size; i++) {
          JS_FreeValueRT(rt, client_req->stream->buffered_data[i]);
        }
        free(client_req->stream->buffered_data);
      }
      if (client_req->stream->write_callbacks) {
        for (size_t i = 0; i < client_req->stream->write_callback_count; i++) {
          JS_FreeValueRT(rt, client_req->stream->write_callbacks[i].callback);
        }
        free(client_req->stream->write_callbacks);
      }
      JS_FreeValueRT(rt, client_req->stream->error_value);
      free(client_req->stream);
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

  // Phase 4.3: Initialize Writable stream data
  client_req->stream = calloc(1, sizeof(JSStreamData));
  client_req->use_chunked = false;
  if (client_req->stream) {
    client_req->stream->writable = true;
    client_req->stream->writable_ended = false;
    client_req->stream->writable_finished = false;
    client_req->stream->writable_corked = 0;
    client_req->stream->destroyed = false;
    client_req->stream->errored = false;
    client_req->stream->error_value = JS_UNDEFINED;
    client_req->stream->options.highWaterMark = 16384;  // 16KB default
    client_req->stream->buffered_data = NULL;
    client_req->stream->buffer_size = 0;
    client_req->stream->buffer_capacity = 0;
    client_req->stream->write_callbacks = NULL;
    client_req->stream->write_callback_count = 0;
    client_req->stream->write_callback_capacity = 0;
  }

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

  // Phase 4.3: Add Writable stream methods
  JS_SetPropertyStr(ctx, obj, "cork", JS_NewCFunction(ctx, js_http_client_request_cork, "cork", 0));
  JS_SetPropertyStr(ctx, obj, "uncork", JS_NewCFunction(ctx, js_http_client_request_uncork, "uncork", 0));

  // Phase 4.3: Add Writable stream property getters
  JSAtom writable_atom = JS_NewAtom(ctx, "writable");
  JS_DefinePropertyGetSet(ctx, obj, writable_atom,
                          JS_NewCFunction(ctx, js_http_client_request_writable, "get writable", 0), JS_UNDEFINED,
                          JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_atom);

  JSAtom writable_ended_atom = JS_NewAtom(ctx, "writableEnded");
  JS_DefinePropertyGetSet(ctx, obj, writable_ended_atom,
                          JS_NewCFunction(ctx, js_http_client_request_writable_ended, "get writableEnded", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_ended_atom);

  JSAtom writable_finished_atom = JS_NewAtom(ctx, "writableFinished");
  JS_DefinePropertyGetSet(ctx, obj, writable_finished_atom,
                          JS_NewCFunction(ctx, js_http_client_request_writable_finished, "get writableFinished", 0),
                          JS_UNDEFINED, JS_PROP_CONFIGURABLE);
  JS_FreeAtom(ctx, writable_finished_atom);

  // Add EventEmitter functionality
  setup_event_emitter_inheritance(ctx, obj);

  return obj;
}
