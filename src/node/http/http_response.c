#include <ctype.h>
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

  // Check if response is already finished
  if (res->finished) {
    return JS_ThrowTypeError(ctx, "Cannot write after end");
  }

  if (!res->headers_sent) {
    // Send headers first (implicit writeHead)
    if (res->status_code == 0)
      res->status_code = 200;
    if (!res->status_message)
      res->status_message = strdup("OK");

    // CRITICAL FIX #1.3: Build headers with dynamic allocation to prevent buffer overflow
    size_t buffer_capacity = 4096;
    size_t buffer_size = 0;
    char* header_buffer = malloc(buffer_capacity);
    if (!header_buffer) {
      return JS_ThrowOutOfMemory(ctx);
    }

    // Helper macro for safe append
#define APPEND_HEADER(fmt, ...)                                                                            \
  do {                                                                                                     \
    int needed = snprintf(NULL, 0, fmt, __VA_ARGS__);                                                      \
    if (needed < 0) {                                                                                      \
      free(header_buffer);                                                                                 \
      return JS_ThrowInternalError(ctx, "Header formatting error");                                        \
    }                                                                                                      \
    if (buffer_size + needed + 1 > buffer_capacity) {                                                      \
      size_t new_capacity = (buffer_size + needed + 1) * 2;                                                \
      char* new_buffer = realloc(header_buffer, new_capacity);                                             \
      if (!new_buffer) {                                                                                   \
        free(header_buffer);                                                                               \
        return JS_ThrowOutOfMemory(ctx);                                                                   \
      }                                                                                                    \
      header_buffer = new_buffer;                                                                          \
      buffer_capacity = new_capacity;                                                                      \
    }                                                                                                      \
    buffer_size += snprintf(header_buffer + buffer_size, buffer_capacity - buffer_size, fmt, __VA_ARGS__); \
  } while (0)

    // Status line
    APPEND_HEADER("HTTP/1.1 %d %s\r\n", res->status_code, res->status_message);

    // Check if we need chunked encoding (no Content-Length set)
    JSValue content_length = JS_GetPropertyStr(ctx, res->headers, "content-length");
    if (JS_IsUndefined(content_length)) {
      res->use_chunked = true;
    }
    JS_FreeValue(ctx, content_length);

    // Write custom headers
    JSPropertyEnum* props;
    uint32_t prop_count;
    if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, res->headers, JS_GPN_STRING_MASK) == 0) {
      for (uint32_t i = 0; i < prop_count; i++) {
        JSValue key = JS_AtomToString(ctx, props[i].atom);
        JSValue val = JS_GetProperty(ctx, res->headers, props[i].atom);

        const char* key_str = JS_ToCString(ctx, key);
        const char* val_str = JS_ToCString(ctx, val);

        if (key_str && val_str) {
          // Capitalize header name (first letter + after hyphens)
          char header_name[256];
          size_t j = 0;
          bool capitalize = true;
          while (key_str[j] && j < sizeof(header_name) - 1) {
            if (capitalize) {
              header_name[j] = toupper((unsigned char)key_str[j]);
              capitalize = false;
            } else {
              header_name[j] = key_str[j];
            }
            if (key_str[j] == '-')
              capitalize = true;
            j++;
          }
          header_name[j] = '\0';

          APPEND_HEADER("%s: %s\r\n", header_name, val_str);
        }

        if (key_str)
          JS_FreeCString(ctx, key_str);
        if (val_str)
          JS_FreeCString(ctx, val_str);
        JS_FreeValue(ctx, key);
        JS_FreeValue(ctx, val);
      }
      js_free(ctx, props);
    }

    // Add chunked encoding header if needed
    if (res->use_chunked) {
      APPEND_HEADER("%s", "Transfer-Encoding: chunked\r\n");
    }

    // Add Server header and finalize
    char* server_header = jsrt_generate_user_agent(ctx);
    APPEND_HEADER("Server: %s\r\n\r\n", server_header);
    free(server_header);

#undef APPEND_HEADER

    // Write headers to socket
    if (!JS_IsUndefined(res->socket)) {
      JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
      if (JS_IsFunction(ctx, write_method)) {
        JSValue header_data = JS_NewStringLen(ctx, header_buffer, buffer_size);
        JS_Call(ctx, write_method, res->socket, 1, &header_data);
        JS_FreeValue(ctx, header_data);
      }
      JS_FreeValue(ctx, write_method);
    }

    free(header_buffer);
    res->headers_sent = true;
  }

  // Write body data
  bool can_write_more = true;  // Phase 4.2: Track back-pressure
  size_t bytes_written = 0;

  if (argc > 0) {
    const char* data = JS_ToCString(ctx, argv[0]);
    if (data && !JS_IsUndefined(res->socket)) {
      size_t data_len = strlen(data);
      bytes_written = data_len;

      // Phase 4.2: Check if corked - if so, we're buffering
      if (res->stream && res->stream->writable_corked > 0) {
        // Corked - just buffer the write, return true
        // Actual write will happen on uncork()
        JS_FreeCString(ctx, data);
        return JS_NewBool(ctx, true);
      }

      JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
      if (JS_IsFunction(ctx, write_method)) {
        if (res->use_chunked) {
          // Write as chunked data: {size_hex}\r\n{data}\r\n
          if (data_len > 0) {
            char chunk_header[32];
            snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", data_len);

            JSValue chunk_start = JS_NewString(ctx, chunk_header);
            JS_Call(ctx, write_method, res->socket, 1, &chunk_start);
            JS_FreeValue(ctx, chunk_start);

            JSValue body_data = JS_NewString(ctx, data);
            JS_Call(ctx, write_method, res->socket, 1, &body_data);
            JS_FreeValue(ctx, body_data);

            JSValue chunk_end = JS_NewString(ctx, "\r\n");
            JS_Call(ctx, write_method, res->socket, 1, &chunk_end);
            JS_FreeValue(ctx, chunk_end);
          }
        } else {
          // Write directly (with Content-Length)
          JSValue body_data = JS_NewString(ctx, data);
          JS_Call(ctx, write_method, res->socket, 1, &body_data);
          JS_FreeValue(ctx, body_data);
        }
      }
      JS_FreeValue(ctx, write_method);
      JS_FreeCString(ctx, data);

      // Phase 4.2: Check for back-pressure
      if (res->stream && bytes_written > res->stream->options.highWaterMark) {
        can_write_more = false;
        res->stream->need_drain = true;
      }
    }
  }

  // Phase 4.2: Return false if back-pressure detected, true otherwise
  return JS_NewBool(ctx, can_write_more);
}

// Response end method
JSValue js_http_response_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  // Check if already finished
  if (res->finished) {
    return JS_ThrowTypeError(ctx, "Response already ended");
  }

  if (argc > 0) {
    // Write final data
    js_http_response_write(ctx, this_val, argc, argv);
  }

  // Ensure headers are sent even for empty response
  if (!res->headers_sent) {
    js_http_response_write(ctx, this_val, 0, NULL);
  }

  // Send chunked terminator if using chunked encoding
  if (res->use_chunked && !JS_IsUndefined(res->socket)) {
    JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
    if (JS_IsFunction(ctx, write_method)) {
      JSValue terminator = JS_NewString(ctx, "0\r\n\r\n");
      JS_Call(ctx, write_method, res->socket, 1, &terminator);
      JS_FreeValue(ctx, terminator);
    }
    JS_FreeValue(ctx, write_method);
  }

  // Mark as finished
  res->finished = true;

  // Phase 4.2: Update Writable stream state
  if (res->stream) {
    res->stream->writable = false;
    res->stream->writable_ended = true;
    res->stream->writable_finished = true;

    // Update properties
    JS_SetPropertyStr(ctx, this_val, "writable", JS_FALSE);
    JS_SetPropertyStr(ctx, this_val, "writableEnded", JS_TRUE);
    JS_SetPropertyStr(ctx, this_val, "writableFinished", JS_TRUE);
  }

  // Emit 'finish' event
  JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue event_name = JS_NewString(ctx, "finish");
    JSValue result = JS_Call(ctx, emit, this_val, 1, &event_name);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, event_name);
  }
  JS_FreeValue(ctx, emit);

  // Close or keep-alive based on Connection header
  // For now, always close (keep-alive will be enhanced in connection management)
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
    // Store header with lowercase key for case-insensitive access
    char* lower_name = malloc(strlen(name) + 1);
    if (lower_name) {
      for (size_t i = 0; name[i]; i++) {
        lower_name[i] = tolower((unsigned char)name[i]);
      }
      lower_name[strlen(name)] = '\0';
      JS_SetPropertyStr(ctx, res->headers, lower_name, JS_NewString(ctx, value));
      free(lower_name);
    }
  }

  if (name)
    JS_FreeCString(ctx, name);
  if (value)
    JS_FreeCString(ctx, value);

  return JS_UNDEFINED;
}

// Response getHeader method
JSValue js_http_response_get_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "getHeader requires name");
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_UNDEFINED;
  }

  // Convert to lowercase for case-insensitive lookup
  char* lower_name = malloc(strlen(name) + 1);
  JSValue result = JS_UNDEFINED;

  if (lower_name) {
    for (size_t i = 0; name[i]; i++) {
      lower_name[i] = tolower((unsigned char)name[i]);
    }
    lower_name[strlen(name)] = '\0';

    result = JS_GetPropertyStr(ctx, res->headers, lower_name);
    free(lower_name);
  }

  JS_FreeCString(ctx, name);
  return result;
}

// Response removeHeader method
JSValue js_http_response_remove_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || res->headers_sent) {
    return JS_ThrowTypeError(ctx, "Headers already sent");
  }

  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "removeHeader requires name");
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) {
    return JS_UNDEFINED;
  }

  // Convert to lowercase for case-insensitive removal
  char* lower_name = malloc(strlen(name) + 1);
  if (lower_name) {
    for (size_t i = 0; name[i]; i++) {
      lower_name[i] = tolower((unsigned char)name[i]);
    }
    lower_name[strlen(name)] = '\0';

    JSAtom atom = JS_NewAtom(ctx, lower_name);
    JS_DeleteProperty(ctx, res->headers, atom, 0);
    JS_FreeAtom(ctx, atom);
    free(lower_name);
  }

  JS_FreeCString(ctx, name);
  return JS_UNDEFINED;
}

// Response getHeaders method
JSValue js_http_response_get_headers(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  // Return a copy of the headers object
  return JS_DupValue(ctx, res->headers);
}

// Response writeContinue method - sends 100 Continue
JSValue js_http_response_write_continue(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  // Send 100 Continue status line
  const char* continue_response = "HTTP/1.1 100 Continue\r\n\r\n";

  if (!JS_IsUndefined(res->socket)) {
    JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
    if (JS_IsFunction(ctx, write_method)) {
      JSValue data = JS_NewString(ctx, continue_response);
      JS_Call(ctx, write_method, res->socket, 1, &data);
      JS_FreeValue(ctx, data);
    }
    JS_FreeValue(ctx, write_method);
  }

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
  res->finished = false;
  res->status_code = 0;
  res->use_chunked = false;
  res->headers = JS_NewObject(ctx);

  // Phase 4.2: Initialize Writable stream data
  res->stream = calloc(1, sizeof(JSStreamData));
  if (!res->stream) {
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, res->headers);
    free(res);
    return JS_ThrowOutOfMemory(ctx);
  }

  // Initialize Writable stream state
  res->stream->writable = true;
  res->stream->writable_ended = false;
  res->stream->writable_finished = false;
  res->stream->writable_corked = 0;
  res->stream->need_drain = false;
  res->stream->options.highWaterMark = 16384;  // 16KB default

  // Initialize write callbacks array
  res->stream->write_callback_capacity = 4;
  res->stream->write_callbacks = malloc(sizeof(WriteCallback) * res->stream->write_callback_capacity);
  if (!res->stream->write_callbacks) {
    free(res->stream);
    JS_FreeValue(ctx, obj);
    JS_FreeValue(ctx, res->headers);
    free(res);
    return JS_ThrowOutOfMemory(ctx);
  }
  res->stream->write_callback_count = 0;

  JS_SetOpaque(obj, res);

  // Add response methods
  JS_SetPropertyStr(ctx, obj, "writeHead", JS_NewCFunction(ctx, js_http_response_write_head, "writeHead", 3));
  JS_SetPropertyStr(ctx, obj, "write", JS_NewCFunction(ctx, js_http_response_write, "write", 1));
  JS_SetPropertyStr(ctx, obj, "end", JS_NewCFunction(ctx, js_http_response_end, "end", 1));
  JS_SetPropertyStr(ctx, obj, "setHeader", JS_NewCFunction(ctx, js_http_response_set_header, "setHeader", 2));
  JS_SetPropertyStr(ctx, obj, "getHeader", JS_NewCFunction(ctx, js_http_response_get_header, "getHeader", 1));
  JS_SetPropertyStr(ctx, obj, "removeHeader", JS_NewCFunction(ctx, js_http_response_remove_header, "removeHeader", 1));
  JS_SetPropertyStr(ctx, obj, "getHeaders", JS_NewCFunction(ctx, js_http_response_get_headers, "getHeaders", 0));
  JS_SetPropertyStr(ctx, obj, "writeContinue",
                    JS_NewCFunction(ctx, js_http_response_write_continue, "writeContinue", 0));

  // Phase 4.2: Add Writable stream methods
  JS_SetPropertyStr(ctx, obj, "cork", JS_NewCFunction(ctx, js_http_response_cork, "cork", 0));
  JS_SetPropertyStr(ctx, obj, "uncork", JS_NewCFunction(ctx, js_http_response_uncork, "uncork", 0));

  // Add properties
  JS_SetPropertyStr(ctx, obj, "statusCode", JS_NewInt32(ctx, 200));
  JS_SetPropertyStr(ctx, obj, "statusMessage", JS_NewString(ctx, "OK"));
  JS_SetPropertyStr(ctx, obj, "headersSent", JS_NewBool(ctx, false));

  // Phase 4.2: Add Writable stream properties
  JS_SetPropertyStr(ctx, obj, "writable", JS_NewBool(ctx, true));
  JS_SetPropertyStr(ctx, obj, "writableEnded", JS_NewBool(ctx, false));
  JS_SetPropertyStr(ctx, obj, "writableFinished", JS_NewBool(ctx, false));

  return obj;
}

// ServerResponse finalizer
void js_http_response_finalizer(JSRuntime* rt, JSValue val) {
  JSHttpResponse* res = JS_GetOpaque(val, js_http_response_class_id);
  if (res) {
    free(res->status_message);
    JS_FreeValueRT(rt, res->headers);
    JS_FreeValueRT(rt, res->socket);

    // Phase 4.2: Clean up Writable stream data
    if (res->stream) {
      if (res->stream->write_callbacks) {
        // Free any pending callbacks
        for (size_t i = 0; i < res->stream->write_callback_count; i++) {
          JS_FreeValueRT(rt, res->stream->write_callbacks[i].callback);
        }
        free(res->stream->write_callbacks);
      }
      free(res->stream);
    }

    free(res);
  }
}

// ============================================================================
// Phase 4.2: Writable Stream Implementation
// ============================================================================

// cork() - Buffer writes
JSValue js_http_response_cork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || !res->stream) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  // Increment cork count (can be nested)
  res->stream->writable_corked++;

  return JS_UNDEFINED;
}

// uncork() - Flush buffered writes
JSValue js_http_response_uncork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || !res->stream) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  // Decrement cork count
  if (res->stream->writable_corked > 0) {
    res->stream->writable_corked--;
  }

  // If fully uncorked and we need drain, emit it
  if (res->stream->writable_corked == 0 && res->stream->need_drain) {
    res->stream->need_drain = false;

    // Emit 'drain' event
    JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue event_name = JS_NewString(ctx, "drain");
      JSValue result = JS_Call(ctx, emit, this_val, 1, &event_name);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, event_name);
    }
    JS_FreeValue(ctx, emit);
  }

  return JS_UNDEFINED;
}

// Property getters for Writable stream properties
JSValue js_http_response_writable(JSContext* ctx, JSValueConst this_val) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || !res->stream) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, res->stream->writable);
}

JSValue js_http_response_writable_ended(JSContext* ctx, JSValueConst this_val) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || !res->stream) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, res->stream->writable_ended);
}

JSValue js_http_response_writable_finished(JSContext* ctx, JSValueConst this_val) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || !res->stream) {
    return JS_FALSE;
  }
  return JS_NewBool(ctx, res->stream->writable_finished);
}
