#ifndef JSRT_NODE_HTTP_INTERNAL_H
#define JSRT_NODE_HTTP_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../../deps/llhttp/build/llhttp.h"
#include "../../runtime.h"
#include "../node_modules.h"
#include "../stream/stream_internal.h"  // Phase 4: Stream integration

// Forward declarations for URL and querystring parsing
extern JSValue JSRT_InitNodeQueryString(JSContext* ctx);

// Class IDs for HTTP classes
extern JSClassID js_http_server_class_id;
extern JSClassID js_http_request_class_id;
extern JSClassID js_http_response_class_id;
extern JSClassID js_http_client_request_class_id;

// Type tags for cleanup callback to distinguish structs
#define HTTP_TYPE_SERVER 0x48545053    // 'HTPS' in hex
#define HTTP_TYPE_REQUEST 0x48545251   // 'HTRQ' in hex
#define HTTP_TYPE_RESPONSE 0x48545250  // 'HTRP' in hex

// CRITICAL FIX #1.4: Connection handler wrapper to avoid global server state
// This wrapper stores the server reference with the connection handler function
typedef struct {
  JSContext* ctx;
  JSValue server;
} JSHttpConnectionHandlerWrapper;

// HTTP Connection state for parsing
typedef struct {
  JSContext* ctx;
  JSValue server;
  JSValue socket;
  llhttp_t parser;
  llhttp_settings_t settings;
  JSValue current_request;
  JSValue current_response;
  bool request_complete;

  // Header accumulation state
  char* current_header_field;
  char* current_header_value;

  // URL accumulation
  char* url_buffer;
  size_t url_buffer_size;
  size_t url_buffer_capacity;

  // Body accumulation
  char* body_buffer;
  size_t body_size;
  size_t body_capacity;

  // Keep-alive state
  bool keep_alive;
  bool should_close;

  // Timeout handling
  uv_timer_t* timeout_timer;
  uint32_t timeout_ms;

  // Special request handling flags
  bool expect_continue;  // Expect: 100-continue
  bool is_upgrade;       // Upgrade request
} JSHttpConnection;

// HTTP Server state
typedef struct {
  JSContext* ctx;
  JSValue server_obj;
  JSValue net_server;  // Underlying net.Server
  bool destroyed;
  uint32_t timeout_ms;                           // Default connection timeout (0 = no timeout)
  JSHttpConnectionHandlerWrapper* conn_wrapper;  // CRITICAL FIX #1.4: Store wrapper for cleanup
} JSHttpServer;

// HTTP Request state (IncomingMessage)
// Phase 4: Now implements Readable stream interface
typedef struct {
  JSContext* ctx;
  JSValue request_obj;
  char* method;
  char* url;
  char* http_version;
  JSValue headers;
  JSValue socket;
  JSStreamData* stream;  // Readable stream data (Phase 4)
} JSHttpRequest;

// HTTP Response state (ServerResponse)
// Phase 4.2: Now implements Writable stream interface
typedef struct {
  JSContext* ctx;
  JSValue response_obj;
  JSValue socket;
  bool headers_sent;
  bool finished;  // end() has been called
  int status_code;
  char* status_message;
  JSValue headers;
  bool use_chunked;  // Use chunked transfer encoding
  JSStreamData* stream;  // Writable stream data (Phase 4.2)
} JSHttpResponse;

// HTTP Client Request state (ClientRequest)
typedef struct {
  JSContext* ctx;
  JSValue request_obj;
  JSValue socket;
  char* method;
  char* host;
  int port;
  char* path;
  char* protocol;  // "http:" or "https:"
  JSValue headers;
  JSValue options;  // Store original options
  bool headers_sent;
  bool finished;
  bool aborted;
  JSValue response_obj;  // IncomingMessage response
  uint32_t timeout_ms;
  uv_timer_t* timeout_timer;
  bool timeout_timer_initialized;

  // Connection state for response parsing
  llhttp_t parser;
  llhttp_settings_t settings;
  char* current_header_field;
  char* current_header_value;
  char* body_buffer;
  size_t body_size;
  size_t body_capacity;
} JSHTTPClientRequest;

// HTTP handler data structure
typedef struct {
  JSValue server;
  JSValue request;
  JSValue response;
  JSContext* ctx;
} HTTPHandlerData;

// Structure for async HTTP listen operation
typedef struct {
  JSContext* ctx;
  JSValue http_server;
  JSValue net_server;
  JSValue* argv_copy;  // Dynamically allocated copy of arguments
  int argc;
  uv_timer_t timer;
} JSHttpListenAsync;

// Helper function to add EventEmitter methods and proper inheritance
void setup_event_emitter_inheritance(JSContext* ctx, JSValue obj);

// Server methods (from http_server.c)
JSValue js_http_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_http_server_listen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_http_server_finalizer(JSRuntime* rt, JSValue val);

// IncomingMessage methods (from http_incoming.c)
JSValue js_http_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
void js_http_request_finalizer(JSRuntime* rt, JSValue val);

// ServerResponse methods (from http_response.c)
JSValue js_http_response_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_http_response_write_head(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_get_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_remove_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_get_headers(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_write_continue(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_http_response_finalizer(JSRuntime* rt, JSValue val);

// Phase 4.2: Writable stream methods for ServerResponse
JSValue js_http_response_cork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_uncork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_writable(JSContext* ctx, JSValueConst this_val);
JSValue js_http_response_writable_ended(JSContext* ctx, JSValueConst this_val);
JSValue js_http_response_writable_finished(JSContext* ctx, JSValueConst this_val);

// Parser functions (from http_parser.c) - Full llhttp callback suite
int on_message_begin(llhttp_t* parser);
int on_url(llhttp_t* parser, const char* at, size_t length);
int on_status(llhttp_t* parser, const char* at, size_t length);
int on_header_field(llhttp_t* parser, const char* at, size_t length);
int on_header_value(llhttp_t* parser, const char* at, size_t length);
int on_headers_complete(llhttp_t* parser);
int on_body(llhttp_t* parser, const char* at, size_t length);
int on_message_complete(llhttp_t* parser);
int on_chunk_header(llhttp_t* parser);
int on_chunk_complete(llhttp_t* parser);

// Connection handling
void js_http_connection_handler(JSContext* ctx, JSValue server, JSValue socket);
JSValue js_http_net_connection_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_simple_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_llhttp_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_close_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void parse_enhanced_http_request(JSContext* ctx, const char* data, char* method, char* url, char* version,
                                 JSValue request);

// Async listen helpers
void http_listen_async_cleanup(JSHttpListenAsync* async_op);
void http_listen_timer_callback(uv_timer_t* timer);

// Module functions (from http_module.c)
JSValue js_http_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_request(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_agent_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

// ClientRequest methods (from http_client.c)
JSValue js_http_client_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_http_client_request_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_get_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_remove_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_abort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_set_no_delay(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_set_socket_keep_alive(JSContext* ctx, JSValueConst this_val, int argc,
                                                     JSValueConst* argv);
JSValue js_http_client_request_flush_headers(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void js_http_client_request_finalizer(JSRuntime* rt, JSValue val);

// Client response parsing (from http_client.c)
int client_on_message_begin(llhttp_t* parser);
int client_on_status(llhttp_t* parser, const char* at, size_t length);
int client_on_header_field(llhttp_t* parser, const char* at, size_t length);
int client_on_header_value(llhttp_t* parser, const char* at, size_t length);
int client_on_headers_complete(llhttp_t* parser);
int client_on_body(llhttp_t* parser, const char* at, size_t length);
int client_on_message_complete(llhttp_t* parser);

#endif  // JSRT_NODE_HTTP_INTERNAL_H
