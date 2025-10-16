/**
 * @file http_internal.h
 * @brief Internal HTTP module implementation for jsrt
 *
 * This file defines the core data structures and function prototypes for the
 * Node.js-compatible HTTP module implementation in jsrt. The HTTP module provides
 * both server-side (http.Server, IncomingMessage, ServerResponse) and client-side
 * (ClientRequest) functionality.
 *
 * ## Architecture Overview
 *
 * The HTTP module is built on several key components:
 *
 * 1. **Server Layer** (JSHttpServer)
 *    - Wraps net.Server for TCP connection handling
 *    - Manages default timeouts and header limits
 *    - Emits 'request' events with IncomingMessage/ServerResponse pairs
 *
 * 2. **Request Processing** (JSHttpRequest/IncomingMessage)
 *    - Implements Readable stream interface
 *    - Represents incoming HTTP requests (server-side)
 *    - Provides access to request method, URL, headers, and body
 *    - Supports per-request timeout handling
 *
 * 3. **Response Generation** (JSHttpResponse/ServerResponse)
 *    - Implements Writable stream interface
 *    - Handles response headers, status codes, and body writing
 *    - Supports chunked transfer encoding
 *    - Provides cork/uncork for buffering optimization
 *
 * 4. **Client Requests** (JSHTTPClientRequest/ClientRequest)
 *    - Implements Writable stream interface
 *    - Makes HTTP requests to remote servers
 *    - Handles request timeouts and connection management
 *    - Returns IncomingMessage for response handling
 *
 * 5. **HTTP Parsing** (JSHttpConnection)
 *    - Uses llhttp for standards-compliant HTTP/1.1 parsing
 *    - Handles keep-alive connections
 *    - Manages incremental header and body parsing
 *    - Supports Expect: 100-continue and Upgrade requests
 *
 * ## Implementation Phases
 *
 * - Phase 1: Basic Server (completed)
 * - Phase 2: Client Requests (completed)
 * - Phase 3: Advanced HTTP Features (completed)
 * - Phase 4: Stream Integration (completed)
 * - Phase 5: Timeout & Advanced Features (completed)
 * - Phase 6: Integration Tests (completed)
 * - Phase 7: Documentation & Cleanup (current)
 * - Phase 8: Production Readiness (pending)
 *
 * @see docs/plan/node-http-plan.md for complete implementation plan
 */

#ifndef JSRT_NODE_HTTP_INTERNAL_H
#define JSRT_NODE_HTTP_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../../deps/llhttp/build/llhttp.h"
#include "../../runtime.h"
#include "../node_modules.h"
#include "../stream/stream_internal.h"  // Phase 4: Stream integration

/**
 * @brief Forward declaration for querystring parsing initialization
 */
extern JSValue JSRT_InitNodeQueryString(JSContext* ctx);

/**
 * @defgroup http_class_ids HTTP Class IDs
 * @brief QuickJS class IDs for HTTP object types
 * @{
 */

/** @brief Class ID for http.Server objects */
extern JSClassID js_http_server_class_id;

/** @brief Class ID for http.IncomingMessage objects */
extern JSClassID js_http_request_class_id;

/** @brief Class ID for http.ServerResponse objects */
extern JSClassID js_http_response_class_id;

/** @brief Class ID for http.ClientRequest objects */
extern JSClassID js_http_client_request_class_id;

/** @} */

/**
 * @defgroup http_type_tags HTTP Type Tags
 * @brief Magic numbers to identify struct types during cleanup
 *
 * These tags are used in finalizer callbacks to safely distinguish between
 * different struct types that may share similar memory layouts.
 * @{
 */

/** @brief Type tag for JSHttpServer ('HTPS' in ASCII) */
#define HTTP_TYPE_SERVER 0x48545053

/** @brief Type tag for JSHttpRequest ('HTRQ' in ASCII) */
#define HTTP_TYPE_REQUEST 0x48545251

/** @brief Type tag for JSHttpResponse ('HTRP' in ASCII) */
#define HTTP_TYPE_RESPONSE 0x48545250

/** @} */

/**
 * @brief HTTP connection state for server-side request parsing
 *
 * This structure maintains the state for an individual HTTP connection on the server side.
 * It uses llhttp for incremental HTTP/1.1 parsing and manages the lifecycle of request/response
 * pairs on the connection.
 *
 * ## Lifecycle
 *
 * 1. Created when net.Server emits 'connection' event
 * 2. Attached to socket's 'data' event handler
 * 3. Parses incoming data incrementally via llhttp callbacks
 * 4. Emits 'request' event on server with IncomingMessage/ServerResponse pair
 * 5. Supports keep-alive for multiple requests on same connection
 * 6. Cleaned up when socket closes or timeout occurs
 *
 * ## llhttp Integration
 *
 * The parser and settings fields integrate with llhttp library:
 * - parser: Maintains parsing state between data chunks
 * - settings: Callback function pointers for parsing events
 * - Header/URL/body accumulation buffers store partial data between callbacks
 *
 * ## Memory Management
 *
 * All dynamically allocated fields (buffers, timers) must be freed in the socket
 * close handler. The connection struct itself is freed when the socket closes.
 *
 * @see js_http_connection_handler() for connection setup
 * @see js_http_llhttp_data_handler() for data processing
 * @see js_http_close_handler() for cleanup
 */
typedef struct {
  JSContext* ctx;             /**< QuickJS context for this connection */
  JSValue server;             /**< Reference to http.Server object */
  JSValue socket;             /**< Reference to net.Socket object */
  llhttp_t parser;            /**< llhttp parser state */
  llhttp_settings_t settings; /**< llhttp callback settings */
  JSValue current_request;    /**< Current IncomingMessage being parsed */
  JSValue current_response;   /**< Current ServerResponse for this request */
  bool request_complete;      /**< Whether current request parsing is complete */

  /** @name Header Accumulation
   * Buffers for incremental header parsing
   * @{
   */
  char* current_header_field; /**< Current header field name being accumulated */
  char* current_header_value; /**< Current header value being accumulated */
  /** @} */

  /** @name URL Accumulation
   * Buffer for incremental URL parsing
   * @{
   */
  char* url_buffer;           /**< URL accumulation buffer */
  size_t url_buffer_size;     /**< Current URL buffer length */
  size_t url_buffer_capacity; /**< URL buffer allocated capacity */
  /** @} */

  /** @name Body Accumulation
   * Buffer for request body data
   * @{
   */
  char* body_buffer;    /**< Request body accumulation buffer */
  size_t body_size;     /**< Current body length */
  size_t body_capacity; /**< Body buffer allocated capacity */
  /** @} */

  /** @name Connection Management
   * Keep-alive and connection state
   * @{
   */
  bool keep_alive;   /**< Whether connection supports keep-alive */
  bool should_close; /**< Whether connection should close after response */
  /** @} */

  /** @name Timeout Handling
   * Server-side connection timeout (Phase 5.1.1)
   * @{
   */
  uv_timer_t* timeout_timer; /**< libuv timer for connection timeout */
  uint32_t timeout_ms;       /**< Timeout duration in milliseconds (0 = no timeout) */
  /** @} */

  /** @name Special Request Flags
   * Flags for special HTTP request handling
   * @{
   */
  bool expect_continue; /**< Client sent 'Expect: 100-continue' header */
  bool is_upgrade;      /**< Client requesting protocol upgrade (WebSocket, etc.) */
  bool request_emitted; /**< Whether 'request' event has been emitted for current request */
  /** @} */

  /** @name Deferred Event Emission (Bug fix for POST body)
   * Timer to defer 'end' event emission until after 'request' event handlers run
   * @{
   */
  uv_timer_t* defer_end_timer; /**< Timer to defer 'end' event emission */
  /** @} */
} JSHttpConnection;

/**
 * @brief HTTP Server state (http.Server)
 *
 * Represents an HTTP server that wraps a net.Server to handle HTTP/1.1 connections.
 * The server manages default timeout values, header limits, and emits 'request'
 * events for each incoming HTTP request.
 *
 * ## Usage Pattern
 *
 * ```javascript
 * const http = require('node:http');
 * const server = http.createServer((req, res) => {
 *   res.writeHead(200, { 'Content-Type': 'text/plain' });
 *   res.end('Hello World\n');
 * });
 * server.setTimeout(5000); // 5 second default timeout
 * server.listen(3000);
 * ```
 *
 * ## Relationship to net.Server
 *
 * - http.Server wraps net.Server (stored in net_server field)
 * - net.Server handles TCP socket management
 * - http.Server adds HTTP parsing layer via JSHttpConnection
 * - Connection timeout is applied to each new socket via net_server
 *
 * @see js_http_server_constructor() for server creation
 * @see js_http_server_listen() for server startup
 * @see js_http_connection_handler() for per-connection setup
 */
typedef struct {
  JSContext* ctx;             /**< QuickJS context */
  JSValue server_obj;         /**< JavaScript http.Server object */
  JSValue net_server;         /**< Underlying net.Server object */
  bool destroyed;             /**< Whether server has been destroyed */
  uint32_t timeout_ms;        /**< Default connection timeout in ms (0 = no timeout) */
  uint32_t max_headers_count; /**< Max header count (default: 2000, 0 = unlimited) */
  uint32_t max_header_size;   /**< Max header size in bytes (default: 8192, 0 = unlimited) */
} JSHttpServer;

/**
 * @brief HTTP Request state (http.IncomingMessage)
 *
 * Represents an incoming HTTP request on the server side. Implements the Readable
 * stream interface for reading request body data.
 *
 * ## Usage Pattern
 *
 * ```javascript
 * server.on('request', (req, res) => {
 *   console.log(`${req.method} ${req.url}`);
 *   console.log('Headers:', req.headers);
 *
 *   req.setTimeout(5000); // Per-request timeout
 *   req.on('data', (chunk) => {
 *     // Process body data
 *   });
 *   req.on('end', () => {
 *     res.end('Request complete');
 *   });
 * });
 * ```
 *
 * ## Stream Interface (Phase 4)
 *
 * IncomingMessage implements Readable stream:
 * - 'data' events for body chunks
 * - 'end' event when body complete
 * - pause()/resume() for flow control
 * - pipe() for streaming to Writable
 *
 * ## Timeout Handling (Phase 5.1.2)
 *
 * Each request can have an individual timeout via setTimeout():
 * - Emits 'timeout' event when timer fires
 * - Does NOT auto-destroy the request
 * - Application must handle timeout (e.g., req.destroy())
 *
 * @see js_http_request_constructor() for object creation
 * @see js_http_incoming_set_timeout() for timeout management
 */
typedef struct {
  JSContext* ctx;       /**< QuickJS context */
  JSValue request_obj;  /**< JavaScript IncomingMessage object */
  char* method;         /**< HTTP method (GET, POST, etc.) */
  char* url;            /**< Request URL path */
  char* http_version;   /**< HTTP version string (e.g., "1.1") */
  JSValue headers;      /**< Request headers object */
  JSValue socket;       /**< Associated net.Socket object */
  JSStreamData* stream; /**< Readable stream implementation (Phase 4) */

  /** @name Timeout Handling (Phase 5.1.2)
   * Per-request timeout support
   * @{
   */
  uv_timer_t* timeout_timer; /**< libuv timer for request timeout */
  uint32_t timeout_ms;       /**< Timeout duration in ms (0 = no timeout) */
  /** @} */
} JSHttpRequest;

/**
 * @brief HTTP Response state (http.ServerResponse)
 *
 * Represents an outgoing HTTP response on the server side. Implements the Writable
 * stream interface for sending response body data.
 *
 * ## Usage Pattern
 *
 * ```javascript
 * server.on('request', (req, res) => {
 *   res.setHeader('Content-Type', 'application/json');
 *   res.writeHead(200, { 'X-Custom': 'header' });
 *   res.write('{"status":');
 *   res.write('"ok"}');
 *   res.end();
 * });
 * ```
 *
 * ## Header Management
 *
 * - Headers can be set before writeHead() or passed to writeHead()
 * - Once headers_sent is true, cannot modify headers
 * - setHeader(), getHeader(), removeHeader() for header manipulation
 * - getHeaders() returns all headers as object
 *
 * ## Stream Interface (Phase 4.2)
 *
 * ServerResponse implements Writable stream:
 * - write() for sending body chunks
 * - end() to complete response
 * - cork()/uncork() for buffering optimization
 * - 'drain' event for backpressure handling
 *
 * ## Chunked Encoding
 *
 * Automatically uses chunked transfer encoding when:
 * - No Content-Length header is set
 * - Response body size is unknown
 * - Controlled by use_chunked flag
 *
 * @see js_http_response_constructor() for object creation
 * @see js_http_response_write_head() for header finalization
 * @see js_http_response_write() for body writing
 * @see js_http_response_end() for completion
 */
typedef struct {
  JSContext* ctx;       /**< QuickJS context */
  JSValue response_obj; /**< JavaScript ServerResponse object */
  JSValue socket;       /**< Associated net.Socket object */
  bool headers_sent;    /**< Whether writeHead() has been called */
  bool finished;        /**< Whether end() has been called */
  int status_code;      /**< HTTP status code (e.g., 200, 404) */
  char* status_message; /**< HTTP status message (e.g., "OK", "Not Found") */
  JSValue headers;      /**< Response headers object */
  bool use_chunked;     /**< Use chunked transfer encoding */
  JSStreamData* stream; /**< Writable stream implementation (Phase 4.2) */
} JSHttpResponse;

/**
 * @brief HTTP Client Request state (http.ClientRequest)
 *
 * Represents an outgoing HTTP request from the client side. Implements the Writable
 * stream interface for sending request body data, and parses the incoming response.
 *
 * ## Usage Pattern
 *
 * ```javascript
 * const http = require('node:http');
 * const req = http.request({
 *   hostname: 'example.com',
 *   port: 80,
 *   path: '/api/data',
 *   method: 'POST'
 * }, (res) => {
 *   console.log(`STATUS: ${res.statusCode}`);
 *   res.on('data', (chunk) => {
 *     console.log(`BODY: ${chunk}`);
 *   });
 * });
 *
 * req.setTimeout(5000);
 * req.write('request body');
 * req.end();
 * ```
 *
 * ## Request Lifecycle
 *
 * 1. Created via http.request() or http.get()
 * 2. Connect to remote server via net.connect()
 * 3. Send request headers (on first write() or end())
 * 4. Write request body via write() calls
 * 5. Complete request with end()
 * 6. Parse response via llhttp
 * 7. Emit 'response' event with IncomingMessage
 *
 * ## Response Parsing
 *
 * ClientRequest includes its own llhttp parser for response parsing:
 * - parser: llhttp state for response parsing
 * - settings: llhttp callbacks for response events
 * - Accumulates headers and body into response_obj (IncomingMessage)
 *
 * ## Timeout Handling (Phase 5.1.3)
 *
 * - setTimeout(timeout, callback) sets request timeout
 * - Emits 'timeout' event when timer fires
 * - Does NOT auto-destroy the request
 * - Application should call req.destroy() on timeout
 *
 * ## Stream Interface (Phase 4.3)
 *
 * ClientRequest implements Writable stream:
 * - write() for sending request body chunks
 * - end() to complete request
 * - cork()/uncork() for buffering
 *
 * @see js_http_client_request_constructor() for object creation
 * @see js_http_request() for http.request()
 * @see js_http_get() for http.get()
 * @see js_http_client_request_set_timeout() for timeout management
 */
typedef struct {
  JSContext* ctx;       /**< QuickJS context */
  JSValue request_obj;  /**< JavaScript ClientRequest object */
  JSValue socket;       /**< Connected net.Socket object */
  char* method;         /**< HTTP method (GET, POST, etc.) */
  char* host;           /**< Target hostname */
  int port;             /**< Target port number */
  char* path;           /**< Request path (e.g., "/api/data") */
  char* protocol;       /**< Protocol ("http:" or "https:") */
  JSValue headers;      /**< Request headers object */
  JSValue options;      /**< Original options object */
  bool headers_sent;    /**< Whether request headers have been sent */
  bool finished;        /**< Whether end() has been called */
  bool aborted;         /**< Whether request has been aborted */
  JSValue response_obj; /**< IncomingMessage for response */

  /** @name Timeout Handling (Phase 5.1.3)
   * Client-side request timeout
   * @{
   */
  uint32_t timeout_ms;            /**< Timeout duration in ms (0 = no timeout) */
  uv_timer_t* timeout_timer;      /**< libuv timer for request timeout */
  bool timeout_timer_initialized; /**< Whether timer has been initialized */
  /** @} */

  /** @name Response Parsing
   * llhttp integration for response parsing
   * @{
   */
  llhttp_t parser;            /**< llhttp parser for response */
  llhttp_settings_t settings; /**< llhttp callback settings */
  char* current_header_field; /**< Current response header field */
  char* current_header_value; /**< Current response header value */
  char* body_buffer;          /**< Response body accumulation buffer */
  size_t body_size;           /**< Current response body length */
  size_t body_capacity;       /**< Response body buffer capacity */
  /** @} */

  /** @name Stream Interface (Phase 4.3)
   * Writable stream support for request body
   * @{
   */
  JSStreamData* stream; /**< Writable stream implementation */
  bool use_chunked;     /**< Use chunked encoding for request body */
  /** @} */
} JSHTTPClientRequest;

/**
 * @brief HTTP request handler data structure
 *
 * Temporary structure passed to user-provided request handler callback.
 * Contains references to server, request, and response objects.
 *
 * @note This structure is only used during handler invocation and is not
 *       stored long-term.
 */
typedef struct {
  JSValue server;   /**< http.Server object */
  JSValue request;  /**< IncomingMessage object */
  JSValue response; /**< ServerResponse object */
  JSContext* ctx;   /**< QuickJS context */
} HTTPHandlerData;

/**
 * @brief Async HTTP listen operation state
 *
 * Manages the asynchronous listen() operation for http.Server. Since net.Server.listen()
 * may not complete synchronously, this structure holds the state needed to retry the
 * listen operation via a timer callback.
 *
 * ## Lifecycle
 *
 * 1. Created when http.Server.listen() is called
 * 2. Timer started to retry net.Server.listen() until successful
 * 3. Cleaned up via http_listen_async_cleanup() when listen succeeds or fails
 *
 * @see js_http_server_listen() for listen initiation
 * @see http_listen_timer_callback() for retry logic
 * @see http_listen_async_cleanup() for cleanup
 */
typedef struct {
  JSContext* ctx;      /**< QuickJS context */
  JSValue http_server; /**< http.Server object */
  JSValue net_server;  /**< Underlying net.Server object */
  JSValue* argv_copy;  /**< Dynamically allocated copy of listen() arguments */
  int argc;            /**< Number of arguments in argv_copy */
  uv_timer_t timer;    /**< libuv timer for async retry */
} JSHttpListenAsync;

/**
 * @defgroup http_helpers Helper Functions
 * @{
 */

/**
 * @brief Sets up EventEmitter inheritance for HTTP objects
 *
 * Adds EventEmitter methods (on, emit, etc.) to an object by inheriting from
 * EventEmitter prototype. Required for all HTTP objects that emit events.
 *
 * @param ctx QuickJS context
 * @param obj Object to add EventEmitter methods to
 */
void setup_event_emitter_inheritance(JSContext* ctx, JSValue obj);

/** @} */

/**
 * @defgroup http_server HTTP Server Methods
 * @brief http.Server implementation (from http_server.c)
 * @{
 */

/**
 * @brief Constructor for http.Server objects
 *
 * Creates a new http.Server instance with optional request handler.
 *
 * @param ctx QuickJS context
 * @param new_target Constructor function
 * @param argc Argument count (0 or 1)
 * @param argv Arguments: [requestListener] (optional callback)
 * @return New http.Server object or JS_EXCEPTION
 */
JSValue js_http_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

/**
 * @brief Starts HTTP server listening on specified port
 *
 * Wraps net.Server.listen() and manages async listen operation.
 *
 * @param ctx QuickJS context
 * @param this_val http.Server object
 * @param argc Argument count
 * @param argv Arguments: port [, host] [, backlog] [, callback]
 * @return this_val for chaining, or JS_EXCEPTION on error
 */
JSValue js_http_server_listen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Closes HTTP server and stops accepting connections
 *
 * @param ctx QuickJS context
 * @param this_val http.Server object
 * @param argc Argument count (0 or 1)
 * @param argv Arguments: [callback] (optional close callback)
 * @return this_val for chaining, or JS_EXCEPTION on error
 */
JSValue js_http_server_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Sets default timeout for server connections (Phase 5.1.1)
 *
 * @param ctx QuickJS context
 * @param this_val http.Server object
 * @param argc Argument count (1 or 2)
 * @param argv Arguments: timeout_ms [, callback]
 * @return this_val for chaining, or JS_EXCEPTION on error
 */
JSValue js_http_server_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Finalizer for http.Server objects
 *
 * Cleans up JSHttpServer structure and releases resources.
 *
 * @param rt QuickJS runtime
 * @param val http.Server object being finalized
 */
void js_http_server_finalizer(JSRuntime* rt, JSValue val);

/** @} */

/**
 * @defgroup http_incoming IncomingMessage Methods
 * @brief http.IncomingMessage implementation (from http_incoming.c)
 * @{
 */

/** @brief Constructor for IncomingMessage objects */
JSValue js_http_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

/** @brief Getter for IncomingMessage.readable property */
JSValue js_http_incoming_readable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Getter for IncomingMessage.readableEnded property */
JSValue js_http_incoming_readable_ended(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Getter for IncomingMessage.readableHighWaterMark property */
JSValue js_http_incoming_readable_high_water_mark(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Getter for IncomingMessage.destroyed property */
JSValue js_http_incoming_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Destroys the IncomingMessage stream */
JSValue js_http_incoming_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Sets per-request timeout (Phase 5.1.2)
 * @param argv Arguments: timeout_ms [, callback]
 */
JSValue js_http_incoming_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Finalizer for IncomingMessage objects */
void js_http_request_finalizer(JSRuntime* rt, JSValue val);

/** @} */

/**
 * @defgroup http_response ServerResponse Methods
 * @brief http.ServerResponse implementation (from http_response.c)
 * @{
 */

/** @brief Constructor for ServerResponse objects */
JSValue js_http_response_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

/**
 * @brief Writes response status line and headers
 * @param argv Arguments: statusCode [, statusMessage] [, headers]
 */
JSValue js_http_response_write_head(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Writes response body chunk
 * @param argv Arguments: chunk [, encoding] [, callback]
 */
JSValue js_http_response_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Completes response, optionally writing final chunk
 * @param argv Arguments: [chunk] [, encoding] [, callback]
 */
JSValue js_http_response_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Sets a response header value
 * @param argv Arguments: name, value
 */
JSValue js_http_response_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Gets a response header value
 * @param argv Arguments: name
 */
JSValue js_http_response_get_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Removes a response header
 * @param argv Arguments: name
 */
JSValue js_http_response_remove_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Returns all response headers as object */
JSValue js_http_response_get_headers(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Sends HTTP 100 Continue response (Phase 3.3) */
JSValue js_http_response_write_continue(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Sends HTTP 102 Processing response (Phase 3.3) */
JSValue js_http_response_write_processing(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Sends HTTP 103 Early Hints response (Phase 3.3)
 * @param argv Arguments: hints [, callback]
 */
JSValue js_http_response_write_early_hints(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Finalizer for ServerResponse objects */
void js_http_response_finalizer(JSRuntime* rt, JSValue val);

/** @name Writable Stream Methods (Phase 4.2)
 * @{
 */
JSValue js_http_response_cork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_uncork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_writable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_writable_ended(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_writable_finished(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_writable_high_water_mark(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_response_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
/** @} */

/** @} */

/**
 * @defgroup http_parser Server-Side llhttp Callbacks
 * @brief llhttp parser callbacks for server request parsing (from http_parser.c)
 *
 * These callbacks are invoked by llhttp as it parses incoming HTTP requests.
 * All callbacks receive llhttp_t* parser with JSHttpConnection* in parser->data.
 *
 * @{
 */

/** @brief Called when a new HTTP message begins */
int on_message_begin(llhttp_t* parser);

/** @brief Called when URL data is parsed (may be called multiple times) */
int on_url(llhttp_t* parser, const char* at, size_t length);

/** @brief Called when status line is parsed (client-side only) */
int on_status(llhttp_t* parser, const char* at, size_t length);

/** @brief Called when header field name is parsed (may be called multiple times) */
int on_header_field(llhttp_t* parser, const char* at, size_t length);

/** @brief Called when header field value is parsed (may be called multiple times) */
int on_header_value(llhttp_t* parser, const char* at, size_t length);

/** @brief Called when all headers have been parsed */
int on_headers_complete(llhttp_t* parser);

/** @brief Called when request body data is parsed (may be called multiple times) */
int on_body(llhttp_t* parser, const char* at, size_t length);

/** @brief Called when HTTP message is complete */
int on_message_complete(llhttp_t* parser);

/** @brief Called when chunk header is parsed (chunked encoding only) */
int on_chunk_header(llhttp_t* parser);

/** @brief Called when chunk is complete (chunked encoding only) */
int on_chunk_complete(llhttp_t* parser);

/** @} */

/**
 * @defgroup http_connection Connection Handling
 * @brief HTTP connection setup and data handling (from http_parser.c)
 * @{
 */

/**
 * @brief Sets up HTTP parsing for a new socket connection
 *
 * Creates JSHttpConnection, initializes llhttp parser, and attaches event handlers.
 *
 * @param ctx QuickJS context
 * @param server http.Server object
 * @param socket net.Socket object for new connection
 */
void js_http_connection_handler(JSContext* ctx, JSValue server, JSValue socket);

/**
 * @brief Helper function to push data into IncomingMessage stream
 *
 * Called from llhttp parser callback (on_body) to emit 'data' events.
 *
 * @param ctx QuickJS context
 * @param incoming_msg IncomingMessage object
 * @param data Pointer to body data
 * @param length Length of body data
 */
void js_http_incoming_push_data(JSContext* ctx, JSValue incoming_msg, const char* data, size_t length);

/**
 * @brief Helper function to signal end of IncomingMessage stream
 *
 * Called from llhttp parser callback (on_message_complete) to emit 'end' event.
 * Uses deferred timer to ensure event listeners are attached before emission.
 *
 * @param ctx QuickJS context
 * @param incoming_msg IncomingMessage object
 */
void js_http_incoming_end(JSContext* ctx, JSValue incoming_msg);

/** @brief net.Server 'connection' event handler wrapper */
JSValue js_http_net_connection_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                       JSValueConst* func_data);

/** @brief Simple (legacy) socket 'data' event handler */
JSValue js_http_simple_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief llhttp-based socket 'data' event handler (current) */
JSValue js_http_llhttp_data_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                                    JSValueConst* func_data);

/** @brief Socket 'close' event handler for cleanup */
JSValue js_http_close_handler(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic,
                              JSValueConst* func_data);

/** @brief Legacy enhanced HTTP request parser (deprecated) */
void parse_enhanced_http_request(JSContext* ctx, const char* data, char* method, char* url, char* version,
                                 JSValue request);

/** @} */

/**
 * @defgroup http_async_listen Async Listen Helpers
 * @brief Async listen operation management (from http_server.c)
 * @{
 */

/** @brief Cleanup function for async listen operation */
void http_listen_async_cleanup(JSHttpListenAsync* async_op);

/** @brief Timer callback for async listen retry */
void http_listen_timer_callback(uv_timer_t* timer);

/** @} */

/**
 * @defgroup http_module HTTP Module Functions
 * @brief Top-level http module API (from http_module.c)
 * @{
 */

/**
 * @brief Creates a new http.Server
 * @param argv Arguments: [requestListener]
 */
JSValue js_http_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Makes an HTTP request
 * @param argv Arguments: url or options [, callback]
 */
JSValue js_http_request(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Makes an HTTP GET request
 * @param argv Arguments: url or options [, callback]
 */
JSValue js_http_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Constructor for http.Agent (connection pooling - stub) */
JSValue js_http_agent_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

/** @} */

/**
 * @defgroup http_client ClientRequest Methods
 * @brief http.ClientRequest implementation (from http_client.c)
 * @{
 */

/** @brief Constructor for ClientRequest objects */
JSValue js_http_client_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);

/**
 * @brief Sets a request header
 * @param argv Arguments: name, value
 */
JSValue js_http_client_request_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Gets a request header
 * @param argv Arguments: name
 */
JSValue js_http_client_request_get_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Removes a request header
 * @param argv Arguments: name
 */
JSValue js_http_client_request_remove_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Writes request body chunk
 * @param argv Arguments: chunk [, encoding] [, callback]
 */
JSValue js_http_client_request_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Completes request, optionally writing final chunk
 * @param argv Arguments: [chunk] [, encoding] [, callback]
 */
JSValue js_http_client_request_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Aborts the request */
JSValue js_http_client_request_abort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Sets request timeout (Phase 5.1.3)
 * @param argv Arguments: timeout_ms [, callback]
 */
JSValue js_http_client_request_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Disables Nagle algorithm on socket
 * @param argv Arguments: noDelay (boolean)
 */
JSValue js_http_client_request_set_no_delay(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/**
 * @brief Enables/disables socket keep-alive
 * @param argv Arguments: enable [, initialDelay]
 */
JSValue js_http_client_request_set_socket_keep_alive(JSContext* ctx, JSValueConst this_val, int argc,
                                                     JSValueConst* argv);

/** @brief Flushes request headers immediately */
JSValue js_http_client_request_flush_headers(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

/** @brief Finalizer for ClientRequest objects */
void js_http_client_request_finalizer(JSRuntime* rt, JSValue val);

/** @name Writable Stream Methods (Phase 4.3)
 * @{
 */
JSValue js_http_client_request_cork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_uncork(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_writable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_writable_ended(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_writable_finished(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_writable_high_water_mark(JSContext* ctx, JSValueConst this_val, int argc,
                                                        JSValueConst* argv);
JSValue js_http_client_request_destroyed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
/** @} */

/** @} */

/**
 * @defgroup http_client_parser Client-Side llhttp Callbacks
 * @brief llhttp parser callbacks for client response parsing (from http_client.c)
 *
 * These callbacks are invoked by llhttp as it parses HTTP responses on the client side.
 * All callbacks receive llhttp_t* parser with JSHTTPClientRequest* in parser->data.
 *
 * @{
 */

/** @brief Called when response message begins */
int client_on_message_begin(llhttp_t* parser);

/** @brief Called when status line is parsed (may be called multiple times) */
int client_on_status(llhttp_t* parser, const char* at, size_t length);

/** @brief Called when response header field is parsed */
int client_on_header_field(llhttp_t* parser, const char* at, size_t length);

/** @brief Called when response header value is parsed */
int client_on_header_value(llhttp_t* parser, const char* at, size_t length);

/** @brief Called when all response headers have been parsed */
int client_on_headers_complete(llhttp_t* parser);

/** @brief Called when response body data is parsed */
int client_on_body(llhttp_t* parser, const char* at, size_t length);

/** @brief Called when response message is complete */
int client_on_message_complete(llhttp_t* parser);

/** @} */

#endif  // JSRT_NODE_HTTP_INTERNAL_H
