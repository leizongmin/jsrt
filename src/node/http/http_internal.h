#ifndef JSRT_NODE_HTTP_INTERNAL_H
#define JSRT_NODE_HTTP_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../../deps/llhttp/build/llhttp.h"
#include "../../runtime.h"
#include "../node_modules.h"

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

// Global HTTP server reference for connection handler workaround
extern JSValue g_current_http_server;
extern JSContext* g_current_http_server_ctx;
extern bool g_http_server_initialized;

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
} JSHttpConnection;

// HTTP Server state
typedef struct {
  JSContext* ctx;
  JSValue server_obj;
  JSValue net_server;  // Underlying net.Server
  bool destroyed;
} JSHttpServer;

// HTTP Request state (IncomingMessage)
typedef struct {
  JSContext* ctx;
  JSValue request_obj;
  char* method;
  char* url;
  char* http_version;
  JSValue headers;
  JSValue socket;
} JSHttpRequest;

// HTTP Response state (ServerResponse)
typedef struct {
  JSContext* ctx;
  JSValue response_obj;
  JSValue socket;
  bool headers_sent;
  int status_code;
  char* status_message;
  JSValue headers;
} JSHttpResponse;

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
void js_http_response_finalizer(JSRuntime* rt, JSValue val);

// Parser functions (from http_parser.c) - to be implemented
int on_message_begin(llhttp_t* parser);
int on_url(llhttp_t* parser, const char* at, size_t length);
int on_message_complete(llhttp_t* parser);

// Connection handling
void js_http_connection_handler(JSContext* ctx, JSValue server, JSValue socket);
JSValue js_http_net_connection_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_simple_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
void parse_enhanced_http_request(JSContext* ctx, const char* data, char* method, char* url, char* version,
                                 JSValue request);

// Async listen helpers
void http_listen_async_cleanup(JSHttpListenAsync* async_op);
void http_listen_timer_callback(uv_timer_t* timer);

// Module functions (from http_module.c)
JSValue js_http_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_request(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_agent_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

#endif  // JSRT_NODE_HTTP_INTERNAL_H
