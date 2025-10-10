#ifndef JSRT_NODE_HTTP_PARSER_H
#define JSRT_NODE_HTTP_PARSER_H

#include "http_internal.h"

// Parser callback functions
int on_message_begin(llhttp_t* parser);
int on_url(llhttp_t* parser, const char* at, size_t length);
int on_message_complete(llhttp_t* parser);

// HTTP request parsing
void parse_enhanced_http_request(JSContext* ctx, const char* data, char* method, char* url, char* version,
                                 JSValue request);

// Connection handling
void js_http_connection_handler(JSContext* ctx, JSValue server, JSValue socket);
JSValue js_http_net_connection_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_simple_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

#endif  // JSRT_NODE_HTTP_PARSER_H
