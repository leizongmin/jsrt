#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* jsrt_strdup(const char* str) {
  if (!str)
    return NULL;
  size_t len = strlen(str);
  char* copy = malloc(len + 1);
  if (copy) {
    memcpy(copy, str, len + 1);
  }
  return copy;
}

static char* jsrt_strndup(const char* str, size_t n) {
  if (!str)
    return NULL;
  char* copy = malloc(n + 1);
  if (copy) {
    memcpy(copy, str, n);
    copy[n] = '\0';
  }
  return copy;
}

jsrt_buffer_t* jsrt_buffer_create(size_t initial_capacity) {
  jsrt_buffer_t* buffer = malloc(sizeof(jsrt_buffer_t));
  if (!buffer)
    return NULL;

  buffer->size = 0;
  buffer->capacity = initial_capacity > 0 ? initial_capacity : 4096;
  buffer->data = malloc(buffer->capacity);
  if (!buffer->data) {
    free(buffer);
    return NULL;
  }

  return buffer;
}

void jsrt_buffer_destroy(jsrt_buffer_t* buffer) {
  if (!buffer)
    return;
  free(buffer->data);
  free(buffer);
}

jsrt_http_error_t jsrt_buffer_append(jsrt_buffer_t* buffer, const char* data, size_t len) {
  if (!buffer || !data || len == 0)
    return JSRT_HTTP_OK;

  if (buffer->size + len > buffer->capacity) {
    size_t new_capacity = buffer->capacity * 2;
    while (new_capacity < buffer->size + len) {
      new_capacity *= 2;
    }

    char* new_data = realloc(buffer->data, new_capacity);
    if (!new_data)
      return JSRT_HTTP_ERROR_MEMORY;

    buffer->data = new_data;
    buffer->capacity = new_capacity;
  }

  memcpy(buffer->data + buffer->size, data, len);
  buffer->size += len;

  return JSRT_HTTP_OK;
}

jsrt_http_headers_t* jsrt_http_headers_create(void) {
  jsrt_http_headers_t* headers = malloc(sizeof(jsrt_http_headers_t));
  if (!headers)
    return NULL;

  headers->first = NULL;
  headers->last = NULL;
  headers->count = 0;

  return headers;
}

void jsrt_http_headers_destroy(jsrt_http_headers_t* headers) {
  if (!headers)
    return;

  jsrt_http_header_t* current = headers->first;
  while (current) {
    jsrt_http_header_t* next = current->next;
    free(current->name);
    free(current->value);
    free(current);
    current = next;
  }

  free(headers);
}

void jsrt_http_headers_add(jsrt_http_headers_t* headers, const char* name, const char* value) {
  if (!headers || !name || !value)
    return;

  jsrt_http_header_t* header = malloc(sizeof(jsrt_http_header_t));
  if (!header)
    return;

  header->name = jsrt_strdup(name);
  header->value = jsrt_strdup(value);
  header->next = NULL;

  if (!header->name || !header->value) {
    free(header->name);
    free(header->value);
    free(header);
    return;
  }

  if (!headers->first) {
    headers->first = headers->last = header;
  } else {
    headers->last->next = header;
    headers->last = header;
  }

  headers->count++;
}

const char* jsrt_http_headers_get(jsrt_http_headers_t* headers, const char* name) {
  if (!headers || !name)
    return NULL;

  jsrt_http_header_t* current = headers->first;
  while (current) {
    if (strcasecmp(current->name, name) == 0) {
      return current->value;
    }
    current = current->next;
  }

  return NULL;
}

jsrt_http_message_t* jsrt_http_message_create(void) {
  jsrt_http_message_t* message = malloc(sizeof(jsrt_http_message_t));
  if (!message)
    return NULL;

  memset(message, 0, sizeof(jsrt_http_message_t));

  message->headers.first = NULL;
  message->headers.last = NULL;
  message->headers.count = 0;

  message->body.data = NULL;
  message->body.size = 0;
  message->body.capacity = 0;

  return message;
}

void jsrt_http_message_destroy(jsrt_http_message_t* message) {
  if (!message)
    return;

  free(message->status_message);
  free(message->method);
  free(message->url);
  free(message->_current_header_field);
  free(message->_current_header_value);

  // Free headers manually
  jsrt_http_header_t* current = message->headers.first;
  while (current) {
    jsrt_http_header_t* next = current->next;
    free(current->name);
    free(current->value);
    free(current);
    current = next;
  }

  // Free body data directly
  free(message->body.data);

  free(message);
}

static int on_message_begin(llhttp_t* parser) {
  jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;

  if (jsrt_parser->current_message) {
    jsrt_http_message_destroy(jsrt_parser->current_message);
  }

  jsrt_parser->current_message = jsrt_http_message_create();
  return jsrt_parser->current_message ? 0 : -1;
}

static int on_url(llhttp_t* parser, const char* at, size_t length) {
  jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;
  jsrt_http_message_t* message = jsrt_parser->current_message;

  if (!message)
    return -1;

  free(message->url);
  message->url = jsrt_strndup(at, length);
  return message->url ? 0 : -1;
}

static int on_status(llhttp_t* parser, const char* at, size_t length) {
  jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;
  jsrt_http_message_t* message = jsrt_parser->current_message;

  if (!message)
    return -1;

  free(message->status_message);
  message->status_message = jsrt_strndup(at, length);
  return message->status_message ? 0 : -1;
}

static int on_header_field(llhttp_t* parser, const char* at, size_t length) {
  jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;
  jsrt_http_message_t* message = jsrt_parser->current_message;

  if (!message)
    return -1;

  if (message->_current_header_value) {
    jsrt_http_headers_add(&message->headers, message->_current_header_field, message->_current_header_value);
    free(message->_current_header_field);
    free(message->_current_header_value);
    message->_current_header_field = NULL;
    message->_current_header_value = NULL;
  }

  message->_current_header_field = jsrt_strndup(at, length);
  return message->_current_header_field ? 0 : -1;
}

static int on_header_value(llhttp_t* parser, const char* at, size_t length) {
  jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;
  jsrt_http_message_t* message = jsrt_parser->current_message;

  if (!message)
    return -1;

  message->_current_header_value = jsrt_strndup(at, length);
  return message->_current_header_value ? 0 : -1;
}

static int on_headers_complete(llhttp_t* parser) {
  jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;
  jsrt_http_message_t* message = jsrt_parser->current_message;

  if (!message)
    return -1;

  if (message->_current_header_field && message->_current_header_value) {
    jsrt_http_headers_add(&message->headers, message->_current_header_field, message->_current_header_value);
    free(message->_current_header_field);
    free(message->_current_header_value);
    message->_current_header_field = NULL;
    message->_current_header_value = NULL;
  }

  message->major_version = parser->http_major;
  message->minor_version = parser->http_minor;
  message->status_code = parser->status_code;

  if (parser->method != 0) {
    message->method = jsrt_strdup(llhttp_method_name(parser->method));
  }

  return 0;
}

static int on_body(llhttp_t* parser, const char* at, size_t length) {
  jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;
  jsrt_http_message_t* message = jsrt_parser->current_message;

  if (!message)
    return -1;

  if (!message->body.data) {
    message->body.data = malloc(4096);
    message->body.capacity = 4096;
    message->body.size = 0;
    if (!message->body.data)
      return -1;
  }

  if (jsrt_buffer_append(&message->body, at, length) != JSRT_HTTP_OK) {
    return -1;
  }

  return 0;
}

static int on_message_complete(llhttp_t* parser) {
  jsrt_http_parser_t* jsrt_parser = (jsrt_http_parser_t*)parser->data;
  jsrt_http_message_t* message = jsrt_parser->current_message;

  if (!message)
    return -1;

  message->complete = 1;
  return 0;
}

jsrt_http_parser_t* jsrt_http_parser_create(JSContext* ctx, jsrt_http_type_t type) {
  jsrt_http_parser_t* parser = malloc(sizeof(jsrt_http_parser_t));
  if (!parser)
    return NULL;

  parser->ctx = ctx;
  parser->current_message = NULL;
  parser->user_data = NULL;

  llhttp_type_t llhttp_type = type == JSRT_HTTP_REQUEST ? HTTP_REQUEST : HTTP_RESPONSE;
  llhttp_init(&parser->parser, llhttp_type, &parser->settings);
  parser->parser.data = parser;

  llhttp_settings_init(&parser->settings);
  parser->settings.on_message_begin = on_message_begin;
  parser->settings.on_url = on_url;
  parser->settings.on_status = on_status;
  parser->settings.on_header_field = on_header_field;
  parser->settings.on_header_value = on_header_value;
  parser->settings.on_headers_complete = on_headers_complete;
  parser->settings.on_body = on_body;
  parser->settings.on_message_complete = on_message_complete;

  return parser;
}

void jsrt_http_parser_destroy(jsrt_http_parser_t* parser) {
  if (!parser)
    return;

  if (parser->current_message) {
    jsrt_http_message_destroy(parser->current_message);
  }

  free(parser);
}

jsrt_http_error_t jsrt_http_parser_execute(jsrt_http_parser_t* parser, const char* data, size_t len) {
  if (!parser || !data)
    return JSRT_HTTP_ERROR_INVALID_DATA;

  llhttp_errno_t err = llhttp_execute(&parser->parser, data, len);

  if (err != HPE_OK) {
    if (parser->current_message) {
      parser->current_message->error = 1;
    }

    switch (err) {
      case HPE_PAUSED:
      case HPE_PAUSED_UPGRADE:
        return JSRT_HTTP_ERROR_INCOMPLETE;
      default:
        return JSRT_HTTP_ERROR_PROTOCOL;
    }
  }

  return JSRT_HTTP_OK;
}

static JSValue response_text_method(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue body = JS_GetPropertyStr(ctx, this_val, "_body");
  if (!JS_IsString(body)) {
    JS_FreeValue(ctx, body);
    body = JS_NewString(ctx, "");
  }

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeValue(ctx, body);
    return promise;
  }

  JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &body);
  JS_FreeValue(ctx, body);
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);

  return promise;
}

static JSValue response_json_method(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
  JSValue body = JS_GetPropertyStr(ctx, this_val, "_body");

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
  if (JS_IsException(promise)) {
    JS_FreeValue(ctx, body);
    return promise;
  }

  if (JS_IsString(body)) {
    const char* json_str = JS_ToCString(ctx, body);
    if (json_str) {
      JSValue parsed = JS_ParseJSON(ctx, json_str, strlen(json_str), "<response>");
      if (JS_IsException(parsed)) {
        JSValue error = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "Invalid JSON"));
        JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
        JS_FreeValue(ctx, error);
        JS_FreeValue(ctx, parsed);
      } else {
        JS_Call(ctx, resolving_funcs[0], JS_UNDEFINED, 1, &parsed);
        JS_FreeValue(ctx, parsed);
      }
      JS_FreeCString(ctx, json_str);
    }
  } else {
    JSValue error = JS_NewError(ctx);
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, "No response body"));
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &error);
    JS_FreeValue(ctx, error);
  }

  JS_FreeValue(ctx, body);
  JS_FreeValue(ctx, resolving_funcs[0]);
  JS_FreeValue(ctx, resolving_funcs[1]);

  return promise;
}

JSValue jsrt_http_message_to_js(JSContext* ctx, jsrt_http_message_t* message) {
  if (!ctx || !message)
    return JS_NULL;

  JSValue obj = JS_NewObject(ctx);
  if (JS_IsException(obj))
    return obj;

  JS_SetPropertyStr(
      ctx, obj, "httpVersion",
      JS_NewString(ctx, message->major_version == 1 ? (message->minor_version == 1 ? "1.1" : "1.0") : "2.0"));

  if (message->status_code > 0) {
    JS_SetPropertyStr(ctx, obj, "status", JS_NewInt32(ctx, message->status_code));
    JSValue ok = JS_NewBool(ctx, message->status_code >= 200 && message->status_code < 300);
    JS_SetPropertyStr(ctx, obj, "ok", ok);

    if (message->status_message) {
      JS_SetPropertyStr(ctx, obj, "statusText", JS_NewString(ctx, message->status_message));
    }
  }

  if (message->method) {
    JS_SetPropertyStr(ctx, obj, "method", JS_NewString(ctx, message->method));
  }

  if (message->url) {
    JS_SetPropertyStr(ctx, obj, "url", JS_NewString(ctx, message->url));
  }

  JSValue headers_obj = JS_NewObject(ctx);
  if (!JS_IsException(headers_obj)) {
    jsrt_http_header_t* header = message->headers.first;
    while (header) {
      JS_SetPropertyStr(ctx, headers_obj, header->name, JS_NewString(ctx, header->value));
      header = header->next;
    }
    JS_SetPropertyStr(ctx, obj, "headers", headers_obj);
  }

  // Store body as internal property and add methods
  if (message->body.data && message->body.size > 0) {
    JSValue body_str = JS_NewStringLen(ctx, message->body.data, message->body.size);
    JS_SetPropertyStr(ctx, obj, "_body", body_str);
  } else {
    JS_SetPropertyStr(ctx, obj, "_body", JS_NewString(ctx, ""));
  }

  // Add text() and json() methods
  JS_SetPropertyStr(ctx, obj, "text", JS_NewCFunction(ctx, response_text_method, "text", 0));
  JS_SetPropertyStr(ctx, obj, "json", JS_NewCFunction(ctx, response_json_method, "json", 0));

  return obj;
}