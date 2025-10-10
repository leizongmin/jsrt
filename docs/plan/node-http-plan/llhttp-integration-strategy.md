# llhttp Integration Strategy for node:http Module

**Created**: 2025-10-10
**Status**: Phase 0 - Research Complete

## Executive Summary

This document defines the strategy for integrating llhttp parser into the node:http module, leveraging existing proven patterns from `src/http/parser.c` (490 lines, 100% working).

## Existing llhttp Implementation Analysis

### src/http/parser.c - Complete Working Implementation

**Key Components**:
- ✅ Full callback suite (8 callbacks implemented)
- ✅ Header management (case-insensitive linked list)
- ✅ Buffer management (dynamic growth)
- ✅ Message structures (request/response)
- ✅ Error handling (protocol, memory, incomplete)

**Reusability**: 90% of code can be adapted for node:http

### Current node_http.c Implementation

**What Exists** (992 lines):
- Basic llhttp parser setup (callbacks: on_message_begin, on_url, on_message_complete)
- Simple HTTP request parsing
- EventEmitter integration
- Mock client implementation

**Issues**:
- ❌ Only 3 of 8 llhttp callbacks implemented
- ❌ No header parsing callbacks
- ❌ No body accumulation
- ❌ No chunked encoding support
- ❌ Single file (should be modular)

## Integration Strategy

### Phase 1: Adopt Proven Parser Structure

**Goal**: Reuse `src/http/parser.c` patterns for server-side parsing

**Key Data Structures** (from parser.c - 100% reusable):

```c
// Header management (case-insensitive)
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

// Buffer with dynamic growth
typedef struct {
  char* data;
  size_t size;
  size_t capacity;
} jsrt_buffer_t;

// Message structure
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
```

### Phase 2: Server-Side Callback Mapping

**All 8 llhttp Callbacks** (copy from parser.c):

1. **on_message_begin** → Create IncomingMessage
2. **on_url** → Extract URL (path, query string)
3. **on_status** → Response parsing (client-side only)
4. **on_header_field** → Accumulate header name
5. **on_header_value** → Accumulate header value
6. **on_headers_complete** → Finalize headers, emit 'headers' event
7. **on_body** → Accumulate body chunks, emit 'data' events
8. **on_message_complete** → Emit 'end' event, trigger 'request' event on server

**Connection-Scoped Parser**:
```c
typedef struct {
  llhttp_t parser;
  llhttp_settings_t settings;
  jsrt_http_message_t* current_message;
  JSContext* ctx;
  JSValue server;               // Server object reference
  JSValue socket;               // TCP socket reference
  JSValue current_request;      // IncomingMessage being built
  JSValue current_response;     // ServerResponse for this request
  void* user_data;
} http_server_parser_t;
```

### Phase 3: Client-Side Callback Mapping

**Response Parsing** (HTTP_RESPONSE mode):

1. **on_message_begin** → Prepare for response
2. **on_status** → Extract status code/message
3. **on_header_field/value** → Build response headers
4. **on_headers_complete** → Emit 'response' event on ClientRequest
5. **on_body** → Emit 'data' on IncomingMessage (response)
6. **on_message_complete** → Emit 'end' on response

**Client Parser Structure**:
```c
typedef struct {
  llhttp_t parser;
  llhttp_settings_t settings;
  jsrt_http_message_t* current_message;
  JSContext* ctx;
  JSValue client_request;       // ClientRequest object
  JSValue response;             // IncomingMessage (response)
  void* user_data;
} http_client_parser_t;
```

## Header Management Approach

### Case-Insensitive Header Storage

**Pattern from parser.c** (proven working):

```c
// Add header (supports multi-value via linked list)
void jsrt_http_headers_add(jsrt_http_headers_t* headers,
                           const char* name,
                           const char* value);

// Get header (case-insensitive)
const char* jsrt_http_headers_get(jsrt_http_headers_t* headers,
                                  const char* name);
```

**Implementation**:
- Linked list allows duplicate headers (e.g., Set-Cookie)
- `strcasecmp()` for case-insensitive lookup
- Preserve original case in storage
- Node.js API exposes as object (last value wins) + rawHeaders array

### Node.js API Compatibility

**headers** (object):
```javascript
{
  'content-type': 'application/json',
  'content-length': '42'
}
```

**rawHeaders** (array - preserves duplicates and case):
```javascript
['Content-Type', 'application/json', 'Content-Length', '42']
```

## Body Accumulation Strategy

### Streaming Approach

**Parser callback** → **Emit 'data'** → **User handles**

```c
static int on_body(llhttp_t* parser, const char* at, size_t length) {
  http_server_parser_t* p = (http_server_parser_t*)parser->data;

  // Emit 'data' event on IncomingMessage
  JSValue emit = JS_GetPropertyStr(p->ctx, p->current_request, "emit");
  JSValue args[] = {
    JS_NewString(p->ctx, "data"),
    JS_NewArrayBufferCopy(p->ctx, (uint8_t*)at, length)
  };
  JS_Call(p->ctx, emit, p->current_request, 2, args);
  JS_FreeValue(p->ctx, args[0]);
  JS_FreeValue(p->ctx, args[1]);
  JS_FreeValue(p->ctx, emit);

  return 0;
}
```

**Buffering (optional)**:
- For small payloads: accumulate in `jsrt_buffer_t`
- For large payloads: stream via events
- Threshold: 64KB (configurable)

### Chunked Transfer Encoding

**llhttp handles parsing automatically**:
- on_chunk_header(parser, length) → track chunk size
- on_body(parser, data, len) → called for each chunk's data
- on_chunk_complete(parser) → chunk boundary
- on_message_complete(parser) → all chunks received

**Generation** (for responses):
```c
// Write chunk
char chunk_header[32];
snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", data_len);
socket_write(chunk_header);
socket_write(data, data_len);
socket_write("\r\n");

// Final chunk
socket_write("0\r\n\r\n");
```

## Parser Lifecycle Management

### Server-Side Lifecycle

1. **Connection established** → Create parser
   ```c
   http_server_parser_t* parser = create_server_parser(ctx);
   llhttp_init(&parser->parser, HTTP_REQUEST, &parser->settings);
   parser->parser.data = parser;
   ```

2. **Data received** → Execute parser
   ```c
   llhttp_execute(&parser->parser, data, len);
   ```

3. **Message complete** → Reset for next request (keep-alive)
   ```c
   llhttp_reset(&parser->parser);
   parser->current_message = NULL;
   ```

4. **Connection closed** → Destroy parser
   ```c
   destroy_server_parser(parser);
   ```

### Client-Side Lifecycle

1. **Request initiated** → Create parser
   ```c
   http_client_parser_t* parser = create_client_parser(ctx);
   llhttp_init(&parser->parser, HTTP_RESPONSE, &parser->settings);
   ```

2. **Response data** → Execute parser
3. **Response complete** → Destroy parser (one request per parser)

## Error Handling Strategy

### llhttp Error Codes

**From parser.c**:
```c
typedef enum {
  JSRT_HTTP_OK = 0,
  JSRT_HTTP_ERROR_INVALID_DATA,
  JSRT_HTTP_ERROR_MEMORY,
  JSRT_HTTP_ERROR_NETWORK,
  JSRT_HTTP_ERROR_TIMEOUT,
  JSRT_HTTP_ERROR_PROTOCOL,
  JSRT_HTTP_ERROR_INCOMPLETE
} jsrt_http_error_t;
```

**Error Mapping**:
- `HPE_PAUSED` / `HPE_PAUSED_UPGRADE` → INCOMPLETE (normal)
- `HPE_INVALID_*` → PROTOCOL (400 Bad Request for server)
- `HPE_*` → PROTOCOL (emit 'error' for client)

### Node.js Error Emission

**Server** (emit 'clientError'):
```javascript
server.emit('clientError', error, socket);
// Default: send 400 Bad Request
```

**Client** (emit 'error' on request):
```javascript
request.emit('error', new Error('Parse error'));
```

## File Structure for llhttp Integration

### Modular Organization

**http_parser.c/.h** (NEW - adapted from src/http/parser.c):
- `http_parser_create_server(ctx)` → Server parser
- `http_parser_create_client(ctx)` → Client parser
- `http_parser_execute()` → Execute parsing
- `http_parser_destroy()` → Cleanup
- All 8 llhttp callbacks
- Header/buffer utilities

**http_server.c** (uses parser):
- Connection handler sets up parser
- Binds parser callbacks to emit events
- Manages parser lifecycle

**http_client.c** (uses parser):
- Request sends → awaits response
- Parser emits 'response' event
- Body streaming via parser callbacks

## Reuse Checklist

### From src/http/parser.c (90% reusable)

- [x] jsrt_http_headers_t structure
- [x] jsrt_buffer_t structure
- [x] jsrt_http_message_t structure
- [x] jsrt_http_headers_add/get functions
- [x] jsrt_buffer_append function
- [x] on_message_begin callback
- [x] on_url callback
- [x] on_status callback
- [x] on_header_field callback
- [x] on_header_value callback
- [x] on_headers_complete callback
- [x] on_body callback
- [x] on_message_complete callback

### From src/http/fetch.c (patterns)

- [x] libuv TCP connection flow
- [x] DNS resolution pattern
- [x] Write request pattern
- [x] Read response pattern
- [x] SSL/TLS integration points

## Implementation Priority

### Phase 1: Server Parser (Week 1)
1. Create http_parser.c with server-side callbacks
2. Adapt jsrt_http_message_t for server use
3. Integrate with connection handler
4. Test request parsing

### Phase 2: Header Management (Week 1)
1. Implement case-insensitive headers
2. Build rawHeaders array
3. Test multi-value headers

### Phase 3: Body Streaming (Week 1)
1. Implement on_body → emit 'data'
2. Add chunked encoding support
3. Test large payloads

### Phase 4: Client Parser (Week 2)
1. Create client-side parser
2. Response parsing callbacks
3. Integrate with ClientRequest

## Testing Strategy

### Parser Unit Tests

**test/node/http/parser/**:
- test_request_parsing.js → Various request formats
- test_response_parsing.js → Various response formats
- test_headers.js → Case-insensitive, multi-value
- test_chunked.js → Chunked encoding
- test_errors.js → Malformed HTTP

### Integration Tests

**test/node/http/integration/**:
- test_server_parsing.js → Full request cycle
- test_client_parsing.js → Full response cycle
- test_keepalive.js → Multiple requests per connection

### ASAN Validation

```bash
make jsrt_m
./target/debug/jsrt_m test/node/http/parser/test_request_parsing.js
# Check for memory leaks in parser lifecycle
```

## Success Criteria

- ✅ All 8 llhttp callbacks implemented
- ✅ Case-insensitive header access
- ✅ Chunked encoding support
- ✅ Zero memory leaks (ASAN clean)
- ✅ Handles malformed HTTP gracefully
- ✅ Supports HTTP/1.0 and HTTP/1.1
- ✅ Keep-alive connection reuse

## References

- **Existing Code**: src/http/parser.c, src/http/fetch.c
- **llhttp Docs**: deps/llhttp/README.md
- **Node.js HTTP**: https://nodejs.org/api/http.html
- **Project Plan**: docs/plan/node-http-plan.md
