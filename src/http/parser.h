#ifndef JSRT_HTTP_PARSER_H
#define JSRT_HTTP_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include "llhttp.h"
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  JSRT_HTTP_OK = 0,
  JSRT_HTTP_ERROR_INVALID_DATA,
  JSRT_HTTP_ERROR_MEMORY,
  JSRT_HTTP_ERROR_NETWORK,
  JSRT_HTTP_ERROR_TIMEOUT,
  JSRT_HTTP_ERROR_PROTOCOL,
  JSRT_HTTP_ERROR_INCOMPLETE
} jsrt_http_error_t;

typedef enum { JSRT_HTTP_REQUEST, JSRT_HTTP_RESPONSE } jsrt_http_type_t;

typedef struct jsrt_http_header {
  char* name;
  char* value;
  struct jsrt_http_header* next;
} jsrt_http_header_t;

typedef struct {
  jsrt_http_header_t* first;
  jsrt_http_header_t* last;
  int count;
} jsrt_http_headers_t;

typedef struct {
  char* data;
  size_t size;
  size_t capacity;
} jsrt_buffer_t;

typedef struct {
  int major_version;
  int minor_version;
  int status_code;
  char* status_message;
  char* method;
  char* url;

  jsrt_http_headers_t headers;
  jsrt_buffer_t body;

  int complete;
  int error;

  char* _current_header_field;
  char* _current_header_value;
} jsrt_http_message_t;

typedef struct {
  llhttp_t parser;
  llhttp_settings_t settings;
  jsrt_http_message_t* current_message;
  JSContext* ctx;
  void* user_data;
} jsrt_http_parser_t;

jsrt_http_parser_t* jsrt_http_parser_create(JSContext* ctx, jsrt_http_type_t type);
void jsrt_http_parser_destroy(jsrt_http_parser_t* parser);
jsrt_http_error_t jsrt_http_parser_execute(jsrt_http_parser_t* parser, const char* data, size_t len);

jsrt_http_message_t* jsrt_http_message_create(void);
void jsrt_http_message_destroy(jsrt_http_message_t* message);

jsrt_http_headers_t* jsrt_http_headers_create(void);
void jsrt_http_headers_destroy(jsrt_http_headers_t* headers);
void jsrt_http_headers_add(jsrt_http_headers_t* headers, const char* name, const char* value);
const char* jsrt_http_headers_get(jsrt_http_headers_t* headers, const char* name);

jsrt_buffer_t* jsrt_buffer_create(size_t initial_capacity);
void jsrt_buffer_destroy(jsrt_buffer_t* buffer);
jsrt_http_error_t jsrt_buffer_append(jsrt_buffer_t* buffer, const char* data, size_t len);

JSValue jsrt_http_message_to_js(JSContext* ctx, jsrt_http_message_t* message);

#ifdef __cplusplus
}
#endif

#endif