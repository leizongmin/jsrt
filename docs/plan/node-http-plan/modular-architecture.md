# Modular Architecture Design for node:http

**Created**: 2025-10-10
**Status**: Phase 0 - Research Complete

## Executive Summary

This document defines the modular file structure for refactoring the single 992-line `node_http.c` into 8-10 focused files, following proven patterns from `net` (7 files) and `dgram` (8 files) modules.

## Current State

**node_http.c** (992 lines) - Single monolithic file:
- HTTP Server class
- IncomingMessage (request) class
- ServerResponse class
- HTTP Agent class
- Mock client implementation
- EventEmitter integration
- Parser integration (incomplete)

**Issues**:
- ❌ Poor maintainability (single large file)
- ❌ Difficult to test individual components
- ❌ No clear separation of concerns
- ❌ Hard to extend

## Target Modular Structure

### File Organization (10 files)

```
src/node/http/
├── http_internal.h          # Shared definitions (150 lines)
├── http_module.c            # Module registration (100 lines)
├── http_server.c            # Server implementation (250 lines)
├── http_server.h            # Server API exports (50 lines)
├── http_client.c            # Client implementation (300 lines)
├── http_client.h            # Client API exports (50 lines)
├── http_incoming.c          # IncomingMessage class (200 lines)
├── http_response.c          # ServerResponse class (250 lines)
├── http_parser.c            # llhttp integration (400 lines)
├── http_callbacks.c         # libuv callbacks (200 lines)
├── http_finalizers.c        # Memory cleanup (150 lines)
└── http_agent.c             # Agent & connection pooling (200 lines)
```

**Total**: ~2,300 lines (vs 992 currently) - growth due to completeness

## Detailed File Specifications

### 1. http_internal.h - Shared Definitions

**Purpose**: Central header with all type definitions, constants, and function prototypes

**Contents**:
```c
#ifndef JSRT_NODE_HTTP_INTERNAL_H
#define JSRT_NODE_HTTP_INTERNAL_H

#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "../../deps/llhttp/build/llhttp.h"
#include "../../runtime.h"
#include "../node_modules.h"

// Class IDs
extern JSClassID js_http_server_class_id;
extern JSClassID js_http_client_request_class_id;
extern JSClassID js_http_incoming_message_class_id;
extern JSClassID js_http_server_response_class_id;
extern JSClassID js_http_agent_class_id;

// Type tags (for safe cleanup)
#define HTTP_TYPE_SERVER          0x48545053  // 'HTPS' (HTTP Server)
#define HTTP_TYPE_CLIENT_REQUEST  0x48544352  // 'HTCR' (HTTP Client Request)
#define HTTP_TYPE_INCOMING_MSG    0x4854494D  // 'HTIM' (HTTP Incoming Message)
#define HTTP_TYPE_SERVER_RESPONSE 0x48545352  // 'HTSR' (HTTP Server Response)
#define HTTP_TYPE_AGENT           0x48544147  // 'HTAG' (HTTP Agent)

// HTTP Server state (pattern from JSNetServer)
typedef struct {
  uint32_t type_tag;            // Must be first
  JSContext* ctx;
  JSValue server_obj;           // JavaScript Server object
  JSValue net_server;           // Underlying net.Server
  uv_timer_t* timeout_timer;    // Server-wide timeout
  bool listening;
  bool destroyed;
  bool in_callback;
  int close_count;
  unsigned int timeout_ms;      // Default timeout for connections
  unsigned int max_headers_count;  // Default 2000
  unsigned int headers_timeout;    // Headers must arrive in this time
  unsigned int request_timeout;    // Full request timeout
  unsigned int keep_alive_timeout; // Idle connection timeout
} JSHTTPServer;

// HTTP IncomingMessage state (request or response)
typedef struct {
  uint32_t type_tag;
  JSContext* ctx;
  JSValue message_obj;          // JavaScript IncomingMessage
  JSValue socket;               // Associated socket
  char* method;                 // Request method (if request)
  char* url;                    // Request URL (if request)
  char* http_version;           // "1.1" or "1.0"
  int status_code;              // Response status (if response)
  char* status_message;         // Response message (if response)
  JSValue headers;              // Headers object
  JSValue raw_headers;          // Raw headers array
  JSValue trailers;             // Trailers (if any)
  bool complete;                // Message fully received
  bool destroyed;
  llhttp_t* parser;             // Parser reference (if being parsed)
} JSHTTPIncomingMessage;

// HTTP ServerResponse state
typedef struct {
  uint32_t type_tag;
  JSContext* ctx;
  JSValue response_obj;         // JavaScript ServerResponse
  JSValue socket;               // Socket to write to
  JSValue request;              // Associated IncomingMessage
  bool headers_sent;
  bool finished;
  bool destroyed;
  int status_code;
  char* status_message;
  JSValue headers;              // Headers to send
  bool chunked_encoding;        // Using chunked encoding
  bool should_keep_alive;       // Keep connection alive
} JSHTTPServerResponse;

// HTTP ClientRequest state
typedef struct {
  uint32_t type_tag;
  JSContext* ctx;
  JSValue request_obj;          // JavaScript ClientRequest
  JSValue socket;               // Connection socket
  JSValue agent;                // HTTP Agent (or undefined)
  char* method;
  char* host;
  int port;
  char* path;
  JSValue headers;              // Request headers
  bool headers_sent;
  bool finished;
  bool destroyed;
  bool aborted;
  bool chunked_encoding;
  llhttp_t* parser;             // Response parser
  JSValue response;             // IncomingMessage (response)
  uv_timer_t* timeout_timer;
  unsigned int timeout_ms;
} JSHTTPClientRequest;

// HTTP Agent state (connection pooling)
typedef struct {
  uint32_t type_tag;
  JSContext* ctx;
  JSValue agent_obj;
  JSValue sockets;              // Object: { 'host:port': [socket1, socket2] }
  JSValue free_sockets;         // Object: { 'host:port': [socket1, socket2] }
  JSValue requests;             // Object: { 'host:port': [req1, req2] }
  int max_sockets;              // Default 5
  int max_free_sockets;         // Default 256
  unsigned int timeout_ms;      // Keep-alive timeout
  bool keep_alive;
} JSHTTPAgent;

// HTTP Connection parser state (per-connection)
typedef struct {
  llhttp_t parser;
  llhttp_settings_t settings;
  JSContext* ctx;
  JSValue server;               // Server object
  JSValue socket;               // Connection socket
  JSValue current_request;      // IncomingMessage being built
  JSValue current_response;     // ServerResponse for this request
  char* current_header_field;   // Accumulator
  char* current_header_value;   // Accumulator
  JSValue headers_array;        // Raw headers
  int header_count;
  bool headers_complete;
  bool message_complete;
} JSHTTPServerParser;

// HTTP Client parser state
typedef struct {
  llhttp_t parser;
  llhttp_settings_t settings;
  JSContext* ctx;
  JSValue client_request;       // ClientRequest object
  JSValue response;             // IncomingMessage (response)
  char* current_header_field;
  char* current_header_value;
  JSValue headers_array;
  int header_count;
  bool headers_complete;
  bool message_complete;
} JSHTTPClientParser;

// Function prototypes (organized by file)

// http_server.c
JSValue js_http_server_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_http_server_listen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_close(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_address(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// http_client.c
JSValue js_http_client_request_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_http_client_request_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_abort(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_get_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_remove_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_client_request_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// http_incoming.c
JSValue js_http_incoming_message_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_http_incoming_message_set_timeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// http_response.c
JSValue js_http_server_response_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue js_http_server_response_write_head(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_response_set_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_response_get_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_response_remove_header(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_response_write(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_server_response_end(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// http_parser.c
JSHTTPServerParser* create_server_parser(JSContext* ctx, JSValue server, JSValue socket);
void destroy_server_parser(JSHTTPServerParser* parser);
JSHTTPClientParser* create_client_parser(JSContext* ctx, JSValue client_request);
void destroy_client_parser(JSHTTPClientParser* parser);
int execute_server_parser(JSHTTPServerParser* parser, const char* data, size_t len);
int execute_client_parser(JSHTTPClientParser* parser, const char* data, size_t len);

// http_callbacks.c
void on_http_connection(uv_stream_t* server, int status);
void on_http_client_connect(uv_connect_t* req, int status);
void on_http_client_data(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
void on_http_timeout(uv_timer_t* timer);

// http_finalizers.c
void js_http_server_finalizer(JSRuntime* rt, JSValue val);
void js_http_client_request_finalizer(JSRuntime* rt, JSValue val);
void js_http_incoming_message_finalizer(JSRuntime* rt, JSValue val);
void js_http_server_response_finalizer(JSRuntime* rt, JSValue val);
void js_http_agent_finalizer(JSRuntime* rt, JSValue val);

// http_agent.c
JSValue js_http_agent_constructor(JSContext* ctx, JSValueConst new_target, int argc, JSValueConst* argv);
JSValue get_agent_socket(JSContext* ctx, JSValue agent, const char* host, int port);
void return_agent_socket(JSContext* ctx, JSValue agent, JSValue socket, const char* host, int port);

// http_module.c
JSValue js_http_create_server(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_request(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_http_get(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

// Utility functions
void add_event_emitter_methods(JSContext* ctx, JSValue obj);
const char* http_status_text(int status_code);

#endif  // JSRT_NODE_HTTP_INTERNAL_H
```

### 2. http_module.c - Module Registration

**Purpose**: Module initialization, exports, and high-level API

**Responsibilities**:
- Register class IDs
- Create class definitions
- Export `createServer()`, `request()`, `get()`
- Export constants (METHODS, STATUS_CODES)
- Export global Agent
- ES Module support

**Pattern**: Like `net_module.c` (6174 bytes)

### 3. http_server.c/.h - Server Implementation

**Purpose**: HTTP Server class and methods

**Responsibilities**:
- Server constructor
- `listen()` method (with async setup)
- `close()` method
- `address()` method
- `setTimeout()` method
- Connection handler setup
- EventEmitter integration

**Pattern**: Like `net_server.c` (8651 bytes)

### 4. http_client.c/.h - Client Implementation

**Purpose**: ClientRequest class and client API

**Responsibilities**:
- ClientRequest constructor
- `write()`, `end()`, `abort()` methods
- Header methods (`setHeader`, `getHeader`, `removeHeader`)
- `setTimeout()` method
- Socket setup and connection
- Request sending
- Response parsing integration

**Pattern**: New implementation based on fetch.c patterns

### 5. http_incoming.c - IncomingMessage Class

**Purpose**: Shared request/response message class

**Responsibilities**:
- IncomingMessage constructor
- Properties: `method`, `url`, `httpVersion`, `headers`, `statusCode`, `statusMessage`
- `setTimeout()` method
- Stream integration (Readable)
- Getter methods

**Pattern**: Extract from current node_http.c

### 6. http_response.c - ServerResponse Class

**Purpose**: ServerResponse class for server-side responses

**Responsibilities**:
- ServerResponse constructor
- `writeHead()` method
- Header methods (`setHeader`, `getHeader`, `removeHeader`, `getHeaders`)
- `write()` method
- `end()` method
- Chunked encoding support
- Stream integration (Writable)

**Pattern**: Extract and enhance from current node_http.c

### 7. http_parser.c - llhttp Integration

**Purpose**: Parser integration for server and client

**Responsibilities**:
- Server parser creation/destruction
- Client parser creation/destruction
- All 8 llhttp callbacks
- Header accumulation (case-insensitive)
- Body streaming (emit 'data' events)
- Error handling

**Pattern**: Adapt from `src/http/parser.c` (490 lines)

### 8. http_callbacks.c - libuv Callbacks

**Purpose**: Async I/O callbacks

**Responsibilities**:
- Connection callback (server)
- Connect callback (client)
- Data read callback
- Write complete callback
- Timeout callbacks
- Close callbacks

**Pattern**: Like `net_callbacks.c` (13828 bytes)

### 9. http_finalizers.c - Memory Cleanup

**Purpose**: Proper resource cleanup

**Responsibilities**:
- Server finalizer
- ClientRequest finalizer
- IncomingMessage finalizer
- ServerResponse finalizer
- Agent finalizer
- Handle close callbacks
- Deferred cleanup (close_count pattern)

**Pattern**: Like `net_finalizers.c` (4823 bytes)

### 10. http_agent.c - Connection Pooling

**Purpose**: HTTP Agent for connection reuse

**Responsibilities**:
- Agent constructor
- Socket pool management
- `get_agent_socket()` - retrieve or create socket
- `return_agent_socket()` - return socket to pool
- Keep-alive management
- Socket limits (maxSockets, maxFreeSockets)
- Request queueing

**Pattern**: Enhance existing Agent class

## Type Tag System

### Purpose
- Safe type identification in generic callbacks
- Prevent use-after-free bugs
- Enable proper cleanup

### Pattern (from net/dgram modules)

```c
#define HTTP_TYPE_SERVER 0x48545053  // 'HTPS'

typedef struct {
  uint32_t type_tag;  // MUST BE FIRST FIELD
  JSContext* ctx;
  // ... other fields
} JSHTTPServer;

// In cleanup callback:
void generic_close_callback(uv_handle_t* handle) {
  uint32_t* tag_ptr = (uint32_t*)handle->data;
  switch (*tag_ptr) {
    case HTTP_TYPE_SERVER:
      handle_server_close((JSHTTPServer*)handle->data);
      break;
    case HTTP_TYPE_CLIENT_REQUEST:
      handle_client_request_close((JSHTTPClientRequest*)handle->data);
      break;
    // ... other types
  }
}
```

## Deferred Cleanup Pattern

### Purpose
- Handle multiple libuv resources (TCP, timers, etc.)
- Prevent use-after-free during cleanup
- Ensure all handles closed before free()

### Pattern (from net module)

```c
typedef struct {
  // ... fields
  int close_count;  // Number of handles to close
} JSHTTPServer;

void start_server_cleanup(JSHTTPServer* server) {
  server->close_count = 0;

  // TCP handle
  if (uv_is_closing((uv_handle_t*)&server->handle) == 0) {
    server->close_count++;
    uv_close((uv_handle_t*)&server->handle, server_handle_close_cb);
  }

  // Timeout timer (if exists)
  if (server->timeout_timer && uv_is_closing((uv_handle_t*)server->timeout_timer) == 0) {
    server->close_count++;
    uv_close((uv_handle_t*)server->timeout_timer, server_timer_close_cb);
  }

  // If no handles to close, free immediately
  if (server->close_count == 0) {
    free_server(server);
  }
}

void server_handle_close_cb(uv_handle_t* handle) {
  JSHTTPServer* server = (JSHTTPServer*)handle->data;
  server->close_count--;
  if (server->close_count == 0) {
    free_server(server);
  }
}
```

## EventEmitter Integration

### Pattern (from net/dgram modules)

**Function**: `add_event_emitter_methods(JSContext* ctx, JSValue obj)`

**Implementation**:
```c
void add_event_emitter_methods(JSContext* ctx, JSValue obj) {
  JSValue events_module = JSRT_LoadNodeModuleCommonJS(ctx, "events");
  JSValue emitter = JS_GetPropertyStr(ctx, events_module, "EventEmitter");
  JSValue prototype = JS_GetPropertyStr(ctx, emitter, "prototype");

  // Set prototype chain
  JS_SetPrototype(ctx, obj, prototype);

  // Copy methods
  const char* methods[] = {"on", "emit", "once", "removeListener", "removeAllListeners", NULL};
  for (int i = 0; methods[i]; i++) {
    JSValue method = JS_GetPropertyStr(ctx, prototype, methods[i]);
    if (JS_IsFunction(ctx, method)) {
      JS_SetPropertyStr(ctx, obj, methods[i], JS_DupValue(ctx, method));
    }
    JS_FreeValue(ctx, method);
  }

  // Initialize state
  JS_SetPropertyStr(ctx, obj, "_events", JS_NewObject(ctx));
  JS_SetPropertyStr(ctx, obj, "_eventsCount", JS_NewInt32(ctx, 0));

  JS_FreeValue(ctx, prototype);
  JS_FreeValue(ctx, emitter);
  JS_FreeValue(ctx, events_module);
}
```

## Build Integration

### Makefile Updates

**Add to HTTP_SOURCES**:
```makefile
HTTP_SOURCES = \
  src/node/http/http_module.c \
  src/node/http/http_server.c \
  src/node/http/http_client.c \
  src/node/http/http_incoming.c \
  src/node/http/http_response.c \
  src/node/http/http_parser.c \
  src/node/http/http_callbacks.c \
  src/node/http/http_finalizers.c \
  src/node/http/http_agent.c
```

**Remove**:
```makefile
src/node/node_http.c  # Delete old monolithic file
```

## Migration Strategy

### Phase 1: Create Structure
1. Create `src/node/http/` directory
2. Create `http_internal.h` with all type definitions
3. Create stub files for all modules

### Phase 2: Extract Server
1. Move Server class to `http_server.c`
2. Move server methods
3. Update references
4. Test: `make test` (server tests should pass)

### Phase 3: Extract Response
1. Move ServerResponse to `http_response.c`
2. Move response methods
3. Test: `make test`

### Phase 4: Extract IncomingMessage
1. Move IncomingMessage to `http_incoming.c`
2. Update both server and client usage
3. Test: `make test`

### Phase 5: Add Parser
1. Create `http_parser.c` with llhttp callbacks
2. Integrate with server
3. Test: enhanced parsing works

### Phase 6: Add Client
1. Implement `http_client.c`
2. Add client request methods
3. Test: client requests work

### Phase 7: Add Callbacks & Finalizers
1. Extract all libuv callbacks to `http_callbacks.c`
2. Move finalizers to `http_finalizers.c`
3. Test: no memory leaks (ASAN)

### Phase 8: Add Agent
1. Implement `http_agent.c`
2. Add connection pooling
3. Test: keep-alive works

## Success Criteria

- ✅ All files under 500 lines (except http_internal.h)
- ✅ Clear separation of concerns
- ✅ Each component independently testable
- ✅ No circular dependencies
- ✅ Follows net/dgram patterns
- ✅ All existing tests pass
- ✅ Zero memory leaks (ASAN)
- ✅ Code formatted (make format)

## Benefits of Modular Structure

1. **Maintainability**: Easy to locate and fix bugs
2. **Testability**: Each component can be unit tested
3. **Extensibility**: Add features without touching unrelated code
4. **Code Review**: Smaller, focused changes
5. **Pattern Consistency**: Follows established net/dgram patterns
6. **Team Collaboration**: Multiple developers can work in parallel

## References

- **net module**: src/node/net/ (7 files, proven pattern)
- **dgram module**: src/node/dgram/ (8 files, EventEmitter integration)
- **Current HTTP**: src/node/node_http.c (992 lines, to be split)
- **Parser foundation**: src/http/parser.c (490 lines, reusable)
