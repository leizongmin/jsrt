# Phase 2 Parallel Execution Strategy: Tasks 2.2-2.4

**Created:** 2025-10-10
**Status:** Planning
**Scope:** Connection handling, Request handling, Response writing enhancements
**Estimated Completion:** 22 atomic tasks across 3 waves

---

## Executive Summary

This document provides a detailed parallel execution strategy for completing Phase 2 remaining tasks (2.2-2.4) of the HTTP module implementation. The strategy maximizes parallelization while respecting hard dependencies, breaking down 22 subtasks into 3 sequential waves with internal parallelization opportunities.

### Current State
- **Phase 2.1 Complete:** Full llhttp integration with all callbacks (8/8 tasks ✅)
- **Infrastructure Ready:** JSHttpConnection structure, parser lifecycle, event emission
- **Tests Passing:** 165/165 tests, 29/32 WPT tests
- **Critical Fixes Applied:** 5/5 critical issues resolved (timer safety, parser reset, buffer overflow, global state, resource leaks)

### Execution Approach
- **3 Sequential Waves:** 2.2 (Connection) → 2.3 (Request) → 2.4 (Response)
- **Parallel Opportunities:** Within each wave, identified 60% parallelizable tasks
- **Risk Mitigation:** All high-risk tasks isolated with fallback strategies
- **Testing Strategy:** Incremental validation after each wave

---

## Table of Contents

1. [Wave 1: Connection Handling (Task 2.2)](#wave-1-connection-handling)
2. [Wave 2: Request Handling (Task 2.3)](#wave-2-request-handling)
3. [Wave 3: Response Writing (Task 2.4)](#wave-3-response-writing)
4. [Dependency Graph](#dependency-graph)
5. [File Modification Matrix](#file-modification-matrix)
6. [Testing Strategy](#testing-strategy)
7. [Risk Assessment](#risk-assessment)
8. [Execution Timeline](#execution-timeline)

---

## Wave 1: Connection Handling (Task 2.2)

**Goal:** Implement robust connection lifecycle management with keep-alive, timeouts, and proper cleanup.

**Dependencies:** Phase 2.1 complete (llhttp integration)

**Total Tasks:** 7 subtasks → **4 atomic tasks** (2 parallel groups)

### Atomic Task Breakdown

#### Group 1.A: Foundation (Sequential - MUST complete first)

##### Task 2.2.1-ATOMIC: HTTPConnection structure enhancement
**Status:** MOSTLY DONE (needs verification)
**Files:** `src/node/http/http_internal.h`
**Complexity:** TRIVIAL
**Estimated Time:** 5 min

**What exists:**
```c
typedef struct {
  JSContext* ctx;
  JSValue server;
  JSValue socket;
  llhttp_t parser;
  llhttp_settings_t settings;
  JSValue current_request;
  JSValue current_response;
  bool request_complete;
  // Header/URL/body buffers (already exist)
  bool keep_alive;       // ✅ Already exists
  bool should_close;     // ✅ Already exists
  uv_timer_t* timeout_timer;  // ✅ Already exists
  uint32_t timeout_ms;        // ✅ Already exists
  bool expect_continue;  // ✅ Already exists
  bool is_upgrade;       // ✅ Already exists
} JSHttpConnection;
```

**What to add:**
```c
// Connection lifecycle state
bool connection_active;     // Track if connection is still active
uint64_t request_count;     // Number of requests served on this connection
uint64_t last_activity_ms;  // Timestamp of last activity (for idle detection)
```

**Implementation:**
1. Verify existing fields in http_internal.h (lines 33-68)
2. Add 3 new fields for connection lifecycle tracking
3. Update js_http_connection_handler() to initialize new fields
4. Document field purposes in header comments

**Testing:**
- Compile test: `make jsrt_g`
- No runtime testing needed (structure-only change)

---

##### Task 2.2.2-ATOMIC: Connection handler registration
**Status:** MOSTLY DONE (needs enhancement)
**Files:** `src/node/http/http_parser.c` (js_http_connection_handler)
**Complexity:** SIMPLE
**Estimated Time:** 10 min
**Dependencies:** 2.2.1-ATOMIC

**What exists:**
- js_http_connection_handler() creates JSHttpConnection (line 584)
- Sets up llhttp parser and callbacks (lines 618-632)
- Registers 'data' and 'close' event handlers (lines 635-679)

**What to enhance:**
1. Initialize new connection lifecycle fields
2. Emit 'connection' event on server
3. Add connection count tracking

**Implementation:**
```c
// In js_http_connection_handler() after conn allocation:

// Initialize connection lifecycle (new fields)
conn->connection_active = true;
conn->request_count = 0;
conn->last_activity_ms = uv_now(rt->uv_loop);

// Emit 'connection' event on server (before 'data' handler setup)
JSValue emit = JS_GetPropertyStr(ctx, server, "emit");
if (JS_IsFunction(ctx, emit)) {
  JSValue args[] = {JS_NewString(ctx, "connection"), JS_DupValue(ctx, socket)};
  JSValue result = JS_Call(ctx, emit, server, 2, args);
  JS_FreeValue(ctx, result);
  JS_FreeValue(ctx, args[0]);
  JS_FreeValue(ctx, args[1]);
}
JS_FreeValue(ctx, emit);
```

**Testing:**
- Test connection event emission
- Verify connection lifecycle fields initialized

---

#### Group 1.B: Core Features (2 parallel tracks after 1.A)

##### Task 2.2.3-ATOMIC: Request/response lifecycle management
**Status:** MOSTLY DONE (needs keep-alive enhancement)
**Files:** `src/node/http/http_parser.c` (on_message_complete)
**Complexity:** MEDIUM
**Estimated Time:** 20 min
**Dependencies:** 2.2.2-ATOMIC
**Parallel With:** 2.2.5-ATOMIC (timeouts - independent)

**What exists:**
- on_message_begin() creates new request/response objects (lines 131-183)
- on_message_complete() emits 'request' event (lines 408-448)
- Keep-alive detection in on_headers_complete() (lines 360-371)
- Parser reset for keep-alive (lines 443-446)

**What to enhance:**
1. Track request count per connection
2. Enforce max requests per connection (optional security limit)
3. Update last_activity_ms on each request
4. Handle upgrade/expect-continue special cases

**Implementation:**
```c
// In on_message_complete() after emitting event:

// Update connection activity
conn->request_count++;
JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
conn->last_activity_ms = uv_now(rt->uv_loop);

// Check for connection close conditions
bool should_keep_alive = conn->keep_alive && !conn->should_close && !conn->is_upgrade;

// Optional: Enforce max requests per connection (security)
const uint64_t MAX_REQUESTS_PER_CONNECTION = 1000;  // Configurable
if (conn->request_count >= MAX_REQUESTS_PER_CONNECTION) {
  should_keep_alive = false;
  conn->should_close = true;
}

// For keep-alive, reset parser for next request
if (should_keep_alive) {
  llhttp_init(&conn->parser, HTTP_REQUEST, &conn->settings);
  conn->parser.data = conn;
} else {
  // Close connection after response completes
  // Note: Actual close happens after response.end()
  conn->connection_active = false;
}
```

**Testing:**
- Single request/response cycle
- Keep-alive with multiple requests
- Connection close after request limit

---

##### Task 2.2.5-ATOMIC: Connection timeout implementation
**Status:** FOUNDATION EXISTS (needs implementation)
**Files:** `src/node/http/http_parser.c`, `http_server.c`
**Complexity:** MEDIUM
**Estimated Time:** 25 min
**Dependencies:** 2.2.2-ATOMIC
**Parallel With:** 2.2.3-ATOMIC (lifecycle - independent)

**What exists:**
- JSHttpConnection has timeout_timer field (http_internal.h:62)
- JSHttpServer has timeout_ms field (http_internal.h:76)
- server.setTimeout() method exists (http_server.c:109-140)

**What to implement:**
1. Per-connection timeout timer setup
2. Idle timeout detection
3. Timeout event emission
4. Timer cleanup on connection close

**Implementation:**

**Step 1:** Add timeout callback function (http_parser.c)
```c
// Connection timeout callback
static void connection_timeout_callback(uv_timer_t* timer) {
  JSHttpConnection* conn = (JSHttpConnection*)timer->data;
  if (!conn || !conn->connection_active) {
    return;
  }

  JSContext* ctx = conn->ctx;

  // Emit 'timeout' event on current request/response
  if (!JS_IsUndefined(conn->current_request)) {
    JSValue emit = JS_GetPropertyStr(ctx, conn->current_request, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue args[] = {JS_NewString(ctx, "timeout")};
      JSValue result = JS_Call(ctx, emit, conn->current_request, 1, args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, emit);
  }

  // Close the connection
  conn->should_close = true;
  conn->connection_active = false;

  JSValue end_method = JS_GetPropertyStr(ctx, conn->socket, "end");
  if (JS_IsFunction(ctx, end_method)) {
    JS_Call(ctx, end_method, conn->socket, 0, NULL);
  }
  JS_FreeValue(ctx, end_method);
}
```

**Step 2:** Initialize timeout in connection handler
```c
// In js_http_connection_handler() after parser init:

// Get server timeout setting
JSHttpServer* http_server = JS_GetOpaque(server, js_http_server_class_id);
if (http_server && http_server->timeout_ms > 0) {
  conn->timeout_ms = http_server->timeout_ms;

  // Initialize timeout timer
  JSRT_Runtime* rt = JS_GetContextOpaque(ctx);
  conn->timeout_timer = malloc(sizeof(uv_timer_t));
  if (conn->timeout_timer) {
    uv_timer_init(rt->uv_loop, conn->timeout_timer);
    conn->timeout_timer->data = conn;
    uv_timer_start(conn->timeout_timer, connection_timeout_callback,
                   conn->timeout_ms, 0);  // One-shot timer
  }
}
```

**Step 3:** Reset timeout on activity
```c
// In js_http_llhttp_data_handler() after successful parse:

// Reset timeout timer on activity
if (conn->timeout_timer && conn->timeout_ms > 0) {
  uv_timer_stop(conn->timeout_timer);
  uv_timer_start(conn->timeout_timer, connection_timeout_callback,
                 conn->timeout_ms, 0);
}
```

**Step 4:** Clean up timeout in cleanup_connection()
```c
// Already exists (lines 58-62), verify proper cleanup:
if (conn->timeout_timer) {
  uv_timer_stop(conn->timeout_timer);
  uv_close((uv_handle_t*)conn->timeout_timer, NULL);  // ✅ Already correct
}
```

**Testing:**
- No timeout: verify timer not created
- With timeout: verify timer starts
- Activity reset: verify timer restarts on data
- Timeout triggers: verify event emission and connection close

---

#### Group 1.C: Cleanup (Sequential after 1.B)

##### Task 2.2.6-ATOMIC: Connection close handling
**Status:** MOSTLY DONE (needs enhancement)
**Files:** `src/node/http/http_parser.c` (cleanup_connection)
**Complexity:** SIMPLE
**Estimated Time:** 15 min
**Dependencies:** 2.2.3-ATOMIC, 2.2.5-ATOMIC

**What exists:**
- cleanup_connection() function (lines 37-65)
- Frees all connection resources
- Called from js_http_close_handler() (lines 746-754)

**What to enhance:**
1. Abort pending requests on unexpected close
2. Emit 'close' events on request/response objects
3. Update connection tracking state

**Implementation:**
```c
// In cleanup_connection() before freeing resources:

// Emit 'close' events on active request/response
if (!JS_IsUndefined(conn->current_request) && conn->connection_active) {
  JSValue emit_req = JS_GetPropertyStr(conn->ctx, conn->current_request, "emit");
  if (JS_IsFunction(conn->ctx, emit_req)) {
    JSValue args[] = {JS_NewString(conn->ctx, "close")};
    JS_Call(conn->ctx, emit_req, conn->current_request, 1, args);
    JS_FreeValue(conn->ctx, args[0]);
  }
  JS_FreeValue(conn->ctx, emit_req);
}

if (!JS_IsUndefined(conn->current_response) && conn->connection_active) {
  JSValue emit_res = JS_GetPropertyStr(conn->ctx, conn->current_response, "emit");
  if (JS_IsFunction(conn->ctx, emit_res)) {
    JSValue args[] = {JS_NewString(conn->ctx, "close")};
    JS_Call(conn->ctx, emit_res, conn->current_response, 1, args);
    JS_FreeValue(conn->ctx, args[0]);
  }
  JS_FreeValue(conn->ctx, emit_res);
}

// Mark connection as inactive
conn->connection_active = false;
```

**Testing:**
- Graceful close: verify 'close' events
- Unexpected close: verify abort handling
- Resource cleanup: ASAN validation

---

##### Task 2.2.7-ATOMIC: Integration testing for connection handling
**Status:** NEW
**Files:** `target/tmp/test_connection_lifecycle.js` (temporary test)
**Complexity:** SIMPLE
**Estimated Time:** 30 min
**Dependencies:** All of Group 1.A, 1.B, 1.C

**Test Cases:**

```javascript
// Test 1: Single request/response
const http = require('http');
const server = http.createServer((req, res) => {
  res.writeHead(200);
  res.end('OK');
});
server.listen(3000, () => {
  http.get('http://localhost:3000', (res) => {
    console.log('✅ Single request success');
    server.close();
  });
});

// Test 2: Keep-alive (multiple requests)
const server2 = http.createServer((req, res) => {
  res.setHeader('Connection', 'keep-alive');
  res.writeHead(200);
  res.end('Request ' + reqCount++);
});
let reqCount = 1;
server2.listen(3001, () => {
  // Make 3 requests on same connection
  const agent = new http.Agent({ keepAlive: true, maxSockets: 1 });
  for (let i = 0; i < 3; i++) {
    http.get({ port: 3001, agent }, (res) => {
      console.log('✅ Keep-alive request ' + (i+1));
      if (i === 2) server2.close();
    });
  }
});

// Test 3: Connection timeout
const server3 = http.createServer((req, res) => {
  // Intentionally delay response
  setTimeout(() => res.end('Late'), 2000);
});
server3.setTimeout(500);  // 500ms timeout
server3.on('timeout', (socket) => {
  console.log('✅ Timeout event fired');
  server3.close();
});
server3.listen(3002);

// Test 4: Connection close
const server4 = http.createServer((req, res) => {
  res.on('close', () => console.log('✅ Response close event'));
  res.writeHead(200);
  res.end();
});
server4.listen(3003, () => {
  http.get('http://localhost:3003', () => {
    server4.close();
  });
});
```

**Validation:**
- All 4 tests pass
- No memory leaks (ASAN)
- Connection events fire correctly
- Timeout handling works

---

### Wave 1 Summary

**Total Atomic Tasks:** 6
**Parallel Groups:** 2 (Group 1.B has 2 parallel tracks)
**Estimated Time:** 105 minutes (1h 45m)
**Risk Level:** MEDIUM (timeout handling complexity)

**Execution Order:**
1. Sequential: 2.2.1-ATOMIC, 2.2.2-ATOMIC (15 min)
2. Parallel: 2.2.3-ATOMIC || 2.2.5-ATOMIC (25 min)
3. Sequential: 2.2.6-ATOMIC (15 min)
4. Sequential: 2.2.7-ATOMIC (30 min)
5. Final validation: `make test && make wpt` (20 min)

**Critical Success Factors:**
- Timer cleanup must use async close callback (Fix #1.1 pattern)
- Connection tracking prevents resource leaks (Fix #1.5 pattern)
- Keep-alive parser reset prevents state corruption (Fix #1.2 pattern)

---

## Wave 2: Request Handling (Task 2.3)

**Goal:** Enhanced request parsing with streaming support, special headers, and robust error handling.

**Dependencies:** Wave 1 complete (connection lifecycle stable)

**Total Tasks:** 8 subtasks → **5 atomic tasks** (2 parallel groups)

### Atomic Task Breakdown

#### Group 2.A: Core Parsing (Mostly Done - Verification)

##### Task 2.3.1-ATOMIC: Request line parsing verification
**Status:** DONE (needs verification only)
**Files:** `src/node/http/http_parser.c` (on_headers_complete)
**Complexity:** TRIVIAL
**Estimated Time:** 10 min

**What exists:**
- Method extraction: `llhttp_method_name(parser->method)` (line 330)
- URL accumulation: on_url() callback (lines 186-195)
- HTTP version: `parser->http_major/http_minor` (line 323)
- Properties set on IncomingMessage (lines 346-357)

**Verification checklist:**
- [ ] Method correctly extracted for all HTTP methods
- [ ] URL properly null-terminated after accumulation
- [ ] HTTP version formatted as "1.0" or "1.1"
- [ ] Properties accessible from JavaScript

**Testing:**
```javascript
http.createServer((req, res) => {
  assert(req.method === 'GET');
  assert(req.url === '/test?foo=bar');
  assert(req.httpVersion === '1.1');
  res.end();
});
```

---

##### Task 2.3.2-ATOMIC: Header parsing verification
**Status:** DONE (needs multi-value verification)
**Files:** `src/node/http/http_parser.c` (on_header_field, on_header_value)
**Complexity:** TRIVIAL
**Estimated Time:** 10 min

**What exists:**
- Case-insensitive storage: str_to_lower() (lines 91-103)
- Multi-value header support: automatic array conversion (lines 217-235)
- Header accumulation: handles split headers (lines 260-274)

**Verification checklist:**
- [ ] Headers stored in lowercase
- [ ] Multi-value headers become arrays
- [ ] Headers split across callbacks are concatenated
- [ ] rawHeaders property exists (if implemented)

**Testing:**
```javascript
// Test multi-value headers
const req = `GET / HTTP/1.1\r
Host: example.com\r
Cookie: a=1\r
Cookie: b=2\r
\r
`;
// Expected: req.headers.cookie === ['a=1', 'b=2']
```

---

##### Task 2.3.3-ATOMIC: URL and query string parsing verification
**Status:** DONE (needs verification)
**Files:** `src/node/http/http_parser.c` (parse_enhanced_http_request)
**Complexity:** TRIVIAL
**Estimated Time:** 10 min

**What exists:**
- URL parsing: parse_enhanced_http_request() (lines 465-510)
- Query string parsing: uses node:querystring (lines 489-502)
- Properties set: pathname, query, search (lines 484, 495-496, 505-507)

**Verification checklist:**
- [ ] pathname extracted without query string
- [ ] query object parsed correctly
- [ ] search includes '?' prefix
- [ ] Handles URLs without query string

**Testing:**
```javascript
http.createServer((req, res) => {
  // URL: /path?foo=bar&baz=qux
  assert(req.pathname === '/path');
  assert(req.query.foo === 'bar');
  assert(req.query.baz === 'qux');
  assert(req.search === '?foo=bar&baz=qux');
  res.end();
});
```

---

#### Group 2.B: Advanced Features (2 parallel tracks)

##### Task 2.3.4-ATOMIC: Request body streaming (Phase 4 prep)
**Status:** PARTIAL (buffering done, streaming deferred)
**Files:** `src/node/http/http_parser.c` (on_body), `http_incoming.c`
**Complexity:** MEDIUM
**Estimated Time:** 30 min
**Dependencies:** 2.3.1, 2.3.2, 2.3.3
**Parallel With:** 2.3.5-ATOMIC (special headers - independent)

**What exists:**
- Body accumulation: buffer_append() in on_body() (lines 394-404)
- Body stored: _body property (line 416)
- Content-Length: handled by llhttp automatically
- Chunked encoding: detected by llhttp (callbacks exist)

**What to add (Phase 2 - emit events, Phase 4 - full streaming):**

**Step 1:** Emit 'data' events during parsing
```c
// In on_body() callback:
int on_body(llhttp_t* parser, const char* at, size_t length) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;
  JSContext* ctx = conn->ctx;

  // Accumulate for Phase 2 compatibility
  if (buffer_append(&conn->body_buffer, &conn->body_size,
                    &conn->body_capacity, at, length) != 0) {
    return -1;
  }

  // Emit 'data' event on request (Phase 4 prep)
  if (!JS_IsUndefined(conn->current_request)) {
    JSValue emit = JS_GetPropertyStr(ctx, conn->current_request, "emit");
    if (JS_IsFunction(ctx, emit)) {
      JSValue chunk = JS_NewStringLen(ctx, at, length);
      JSValue args[] = {JS_NewString(ctx, "data"), chunk};
      JSValue result = JS_Call(ctx, emit, conn->current_request, 2, args);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, args[0]);
      JS_FreeValue(ctx, args[1]);
    }
    JS_FreeValue(ctx, emit);
  }

  return 0;
}
```

**Step 2:** Emit 'end' event on message complete
```c
// In on_message_complete() after setting _body:

// Emit 'end' event on request
if (!JS_IsUndefined(conn->current_request)) {
  JSValue emit = JS_GetPropertyStr(ctx, conn->current_request, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "end")};
    JSValue result = JS_Call(ctx, emit, conn->current_request, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);
}
```

**Testing:**
```javascript
http.createServer((req, res) => {
  let body = '';
  req.on('data', chunk => body += chunk);
  req.on('end', () => {
    console.log('Body:', body);
    res.end('Received: ' + body.length + ' bytes');
  });
});
```

---

##### Task 2.3.5-ATOMIC: Special request handling (Expect, Upgrade)
**Status:** DETECTION DONE (needs event emission)
**Files:** `src/node/http/http_parser.c` (on_headers_complete, on_message_complete)
**Complexity:** SIMPLE
**Estimated Time:** 20 min
**Dependencies:** 2.3.1, 2.3.2
**Parallel With:** 2.3.4-ATOMIC (body streaming - independent)

**What exists:**
- Expect: 100-continue detection (lines 374-382)
- Upgrade detection (lines 384-387)
- Event emission in on_message_complete() (lines 420-438)

**What to enhance:**
1. Send 100 Continue response
2. Provide socket access for upgrade
3. Add response.writeContinue() method

**Implementation:**

**Step 1:** Add writeContinue() to ServerResponse (http_response.c)
```c
JSValue js_http_response_write_continue(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  // Send 100 Continue response
  const char* continue_response = "HTTP/1.1 100 Continue\r\n\r\n";

  JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
  if (JS_IsFunction(ctx, write_method)) {
    JSValue data = JS_NewString(ctx, continue_response);
    JSValue args[] = {data};
    JSValue result = JS_Call(ctx, write_method, res->socket, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, data);
  }
  JS_FreeValue(ctx, write_method);

  return JS_UNDEFINED;
}
```

**Step 2:** Register writeContinue() in response constructor
```c
// In js_http_response_constructor():
JS_SetPropertyStr(ctx, obj, "writeContinue",
                 JS_NewCFunction(ctx, js_http_response_write_continue,
                                "writeContinue", 0));
```

**Step 3:** Verify event emission (already exists)
```c
// In on_message_complete() - lines 420-438:
const char* event_name = "request";
if (conn->is_upgrade) {
  event_name = "upgrade";  // ✅ Already correct
} else if (conn->expect_continue) {
  event_name = "checkContinue";  // ✅ Already correct
}
// Event emitted with socket for upgrade: ✅ line 431
```

**Testing:**
```javascript
// Test Expect: 100-continue
const server = http.createServer();
server.on('checkContinue', (req, res) => {
  console.log('✅ checkContinue event');
  res.writeContinue();
  res.writeHead(200);
  res.end('OK');
});

// Test Upgrade (WebSocket handshake)
server.on('upgrade', (req, socket, head) => {
  console.log('✅ Upgrade event with socket access');
  socket.write('HTTP/1.1 101 Switching Protocols\r\n');
  socket.write('Upgrade: websocket\r\n\r\n');
});
```

---

#### Group 2.C: Error Handling (Sequential after 2.B)

##### Task 2.3.7-ATOMIC: Request error handling
**Status:** PARTIAL (parse errors done, needs HTTP error responses)
**Files:** `src/node/http/http_parser.c` (js_http_llhttp_data_handler)
**Complexity:** MEDIUM
**Estimated Time:** 25 min
**Dependencies:** 2.3.4-ATOMIC, 2.3.5-ATOMIC

**What exists:**
- Parse error handling: emits 'clientError' (lines 710-724)
- Parser reset on error (lines 726-728)
- Connection close on error (lines 733-738)

**What to add:**
1. Send proper HTTP error responses
2. Handle oversized headers (431 Request Header Fields Too Large)
3. Handle malformed requests (400 Bad Request)
4. Enforce header count limits

**Implementation:**

**Step 1:** Add error response helper
```c
static void send_http_error(JSContext* ctx, JSValue socket, int status_code,
                           const char* status_message) {
  char response[256];
  snprintf(response, sizeof(response),
           "HTTP/1.1 %d %s\r\n"
           "Connection: close\r\n"
           "Content-Length: 0\r\n"
           "\r\n",
           status_code, status_message);

  JSValue write_method = JS_GetPropertyStr(ctx, socket, "write");
  if (JS_IsFunction(ctx, write_method)) {
    JSValue data = JS_NewString(ctx, response);
    JSValue args[] = {data};
    JS_Call(ctx, write_method, socket, 1, args);
    JS_FreeValue(ctx, data);
  }
  JS_FreeValue(ctx, write_method);
}
```

**Step 2:** Add header size tracking
```c
// In JSHttpConnection structure (http_internal.h):
size_t header_bytes_received;  // Track total header size
uint32_t header_count;          // Track number of headers

// In on_header_field() before storing:
conn->header_bytes_received += length;
conn->header_count++;

// Enforce limits
const size_t MAX_HEADER_SIZE = 8192;  // 8KB (configurable)
const uint32_t MAX_HEADER_COUNT = 100;  // (configurable)

if (conn->header_bytes_received > MAX_HEADER_SIZE) {
  send_http_error(ctx, conn->socket, 431, "Request Header Fields Too Large");
  return -1;
}

if (conn->header_count > MAX_HEADER_COUNT) {
  send_http_error(ctx, conn->socket, 431, "Too Many Headers");
  return -1;
}
```

**Step 3:** Enhance error handling in data handler
```c
// In js_http_llhttp_data_handler() after llhttp_execute():
if (err != HPE_OK && err != HPE_PAUSED && err != HPE_PAUSED_UPGRADE) {
  // Send appropriate HTTP error based on error type
  int status_code = 400;
  const char* status_message = "Bad Request";

  // Classify error
  if (err == HPE_INVALID_METHOD) {
    status_code = 405;
    status_message = "Method Not Allowed";
  } else if (err == HPE_INVALID_URL) {
    status_code = 400;
    status_message = "Bad Request";
  } else if (err == HPE_HEADER_OVERFLOW) {
    status_code = 431;
    status_message = "Request Header Fields Too Large";
  }

  send_http_error(ctx, conn->socket, status_code, status_message);

  // Existing error handling continues...
}
```

**Testing:**
```javascript
// Test malformed request
const net = require('net');
const client = net.connect(3000, () => {
  client.write('INVALID REQUEST\r\n\r\n');
  client.on('data', data => {
    assert(data.toString().includes('400 Bad Request'));
    console.log('✅ Malformed request handled');
  });
});

// Test oversized headers
const hugeHeader = 'X-Large: ' + 'A'.repeat(10000) + '\r\n';
client.write('GET / HTTP/1.1\r\n' + hugeHeader + '\r\n');
// Expected: 431 Request Header Fields Too Large
```

---

##### Task 2.3.8-ATOMIC: Integration testing for request handling
**Status:** NEW
**Files:** `target/tmp/test_request_handling.js`
**Complexity:** SIMPLE
**Estimated Time:** 25 min
**Dependencies:** All of Wave 2

**Test Cases:**

```javascript
// Test 1: Request line parsing
// Test 2: Multi-value headers
// Test 3: URL and query string
// Test 4: Request body streaming
// Test 5: Expect: 100-continue
// Test 6: Upgrade handling
// Test 7: Error responses (400, 431)

// Full test suite (~150 lines)
```

**Validation:**
- All request parsing tests pass
- Special headers handled correctly
- Error responses sent properly
- No memory leaks (ASAN)

---

### Wave 2 Summary

**Total Atomic Tasks:** 5
**Parallel Groups:** 1 (Group 2.B has 2 parallel tracks)
**Estimated Time:** 130 minutes (2h 10m)
**Risk Level:** MEDIUM (body streaming complexity)

**Execution Order:**
1. Sequential: 2.3.1, 2.3.2, 2.3.3 (30 min verification)
2. Parallel: 2.3.4-ATOMIC || 2.3.5-ATOMIC (30 min)
3. Sequential: 2.3.7-ATOMIC (25 min)
4. Sequential: 2.3.8-ATOMIC (25 min)
5. Final validation: `make test && make wpt` (20 min)

**Critical Success Factors:**
- Body streaming events don't break existing buffering
- Error responses prevent connection hangs
- Header limits protect against DoS attacks

---

## Wave 3: Response Writing (Task 2.4)

**Goal:** Robust response generation with chunked encoding, header management, and proper flow control.

**Dependencies:** Wave 2 complete (request handling stable)

**Total Tasks:** 7 subtasks → **5 atomic tasks** (2 parallel groups)

### Atomic Task Breakdown

#### Group 3.A: Header Management (2 parallel tracks)

##### Task 2.4.1-ATOMIC: Enhanced writeHead() implementation
**Status:** PARTIAL (basic exists, needs enhancement)
**Files:** `src/node/http/http_response.c` (js_http_response_write_head)
**Complexity:** MEDIUM
**Estimated Time:** 25 min
**Parallel With:** 2.4.2-ATOMIC (header methods - independent)

**What exists:**
- writeHead() exists (http_response.c, referenced in http_internal.h:168)
- Status line formatting
- Header writing to socket

**What to enhance:**
1. Prevent duplicate writeHead() calls
2. Support implicit header writing
3. Handle status message parameter
4. Merge headers parameter with existing headers

**Implementation:**

**Step 1:** Read existing implementation
```bash
# First, read the current writeHead() to see what exists
```

**Step 2:** Enhance based on findings
```c
JSValue js_http_response_write_head(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_ThrowTypeError(ctx, "Invalid response object");
  }

  // Prevent duplicate writeHead()
  if (res->headers_sent) {
    return JS_ThrowError(ctx, "Headers already sent");
  }

  // Parse parameters: writeHead(statusCode[, statusMessage][, headers])
  int status_code = 200;
  const char* status_message = NULL;
  JSValue headers_obj = JS_UNDEFINED;

  if (argc >= 1) {
    JS_ToInt32(ctx, &status_code, argv[0]);
  }

  if (argc >= 2) {
    if (JS_IsString(argv[1])) {
      status_message = JS_ToCString(ctx, argv[1]);
      if (argc >= 3 && JS_IsObject(argv[2])) {
        headers_obj = argv[2];
      }
    } else if (JS_IsObject(argv[1])) {
      headers_obj = argv[1];
    }
  }

  // Update status
  res->status_code = status_code;
  if (status_message) {
    free(res->status_message);
    res->status_message = strdup(status_message);
    JS_FreeCString(ctx, status_message);
  }

  // Merge headers if provided
  if (!JS_IsUndefined(headers_obj)) {
    JSPropertyEnum* props;
    uint32_t prop_count;
    if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, headers_obj,
                               JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
      for (uint32_t i = 0; i < prop_count; i++) {
        JSValue key = JS_AtomToString(ctx, props[i].atom);
        JSValue val = JS_GetProperty(ctx, headers_obj, props[i].atom);
        JS_SetProperty(ctx, res->headers, props[i].atom, val);
        JS_FreeValue(ctx, key);
      }
      js_free(ctx, props);
    }
  }

  // Write status line and headers to socket
  // ... (existing implementation)

  res->headers_sent = true;
  return JS_DupValue(ctx, this_val);  // Return this for chaining
}
```

**Testing:**
```javascript
// Test duplicate writeHead()
try {
  res.writeHead(200);
  res.writeHead(404);  // Should throw
  assert(false);
} catch (e) {
  console.log('✅ Duplicate writeHead() prevented');
}

// Test statusMessage parameter
res.writeHead(200, 'Everything OK');
res.writeHead(404, 'Custom Not Found', { 'X-Custom': 'value' });
```

---

##### Task 2.4.2-ATOMIC: Header manipulation methods
**Status:** PARTIAL (setHeader exists, others need implementation)
**Files:** `src/node/http/http_response.c`
**Complexity:** SIMPLE
**Estimated Time:** 20 min
**Parallel With:** 2.4.1-ATOMIC (writeHead - independent)

**What exists:**
- setHeader() (http_internal.h:171)
- getHeader(), removeHeader(), getHeaders() declared (lines 172-174)

**What to implement:**
- getHeader() - case-insensitive retrieval
- removeHeader() - only before headers sent
- getHeaders() - return all headers object

**Implementation:**

```c
// getHeader(name) - case-insensitive
JSValue js_http_response_get_header(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || argc < 1) {
    return JS_UNDEFINED;
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_UNDEFINED;

  // Normalize to lowercase
  char* lower_name = str_to_lower(name);
  JS_FreeCString(ctx, name);

  if (!lower_name) return JS_UNDEFINED;

  JSValue value = JS_GetPropertyStr(ctx, res->headers, lower_name);
  free(lower_name);

  return value;
}

// removeHeader(name) - only before headers sent
JSValue js_http_response_remove_header(JSContext* ctx, JSValueConst this_val,
                                       int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || argc < 1) {
    return JS_UNDEFINED;
  }

  if (res->headers_sent) {
    return JS_ThrowError(ctx, "Cannot remove headers after they are sent");
  }

  const char* name = JS_ToCString(ctx, argv[0]);
  if (!name) return JS_UNDEFINED;

  char* lower_name = str_to_lower(name);
  JS_FreeCString(ctx, name);

  if (lower_name) {
    JS_DeletePropertyStr(ctx, res->headers, lower_name);
    free(lower_name);
  }

  return JS_UNDEFINED;
}

// getHeaders() - return all headers
JSValue js_http_response_get_headers(JSContext* ctx, JSValueConst this_val,
                                     int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_UNDEFINED;
  }

  // Return a copy of the headers object
  JSValue headers_copy = JS_NewObject(ctx);

  JSPropertyEnum* props;
  uint32_t prop_count;
  if (JS_GetOwnPropertyNames(ctx, &props, &prop_count, res->headers,
                             JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) == 0) {
    for (uint32_t i = 0; i < prop_count; i++) {
      JSValue val = JS_GetProperty(ctx, res->headers, props[i].atom);
      JS_SetProperty(ctx, headers_copy, props[i].atom, val);
    }
    js_free(ctx, props);
  }

  return headers_copy;
}
```

**Registration:**
```c
// In js_http_response_constructor():
JS_SetPropertyStr(ctx, obj, "getHeader",
                 JS_NewCFunction(ctx, js_http_response_get_header, "getHeader", 1));
JS_SetPropertyStr(ctx, obj, "removeHeader",
                 JS_NewCFunction(ctx, js_http_response_remove_header, "removeHeader", 1));
JS_SetPropertyStr(ctx, obj, "getHeaders",
                 JS_NewCFunction(ctx, js_http_response_get_headers, "getHeaders", 0));
```

**Testing:**
```javascript
res.setHeader('Content-Type', 'text/html');
assert(res.getHeader('content-type') === 'text/html');  // Case-insensitive
res.removeHeader('Content-Type');
assert(res.getHeader('content-type') === undefined);

const headers = res.getHeaders();
assert(typeof headers === 'object');
```

---

#### Group 3.B: Body Writing (Sequential after 3.A)

##### Task 2.4.3-ATOMIC: Enhanced write() with flow control
**Status:** PARTIAL (basic exists, needs back-pressure)
**Files:** `src/node/http/http_response.c` (js_http_response_write)
**Complexity:** MEDIUM
**Estimated Time:** 30 min
**Dependencies:** 2.4.1-ATOMIC, 2.4.2-ATOMIC

**What exists:**
- write() method exists (http_internal.h:169)
- Writes to socket

**What to enhance:**
1. Implicit header writing on first write
2. Return boolean for flow control
3. Handle back-pressure from socket
4. Support encoding parameter

**Implementation:**

```c
JSValue js_http_response_write(JSContext* ctx, JSValueConst this_val,
                               int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res || argc < 1) {
    return JS_FALSE;
  }

  // Check if response finished
  if (res->finished) {
    return JS_ThrowError(ctx, "Cannot write after end()");
  }

  // Implicit header writing
  if (!res->headers_sent) {
    JSValue write_head = JS_GetPropertyStr(ctx, this_val, "writeHead");
    if (JS_IsFunction(ctx, write_head)) {
      JSValue args[] = {JS_NewInt32(ctx, res->status_code)};
      JS_Call(ctx, write_head, this_val, 1, args);
      JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, write_head);
  }

  // Get data to write
  const char* data = NULL;
  size_t data_len = 0;

  if (JS_IsString(argv[0])) {
    data = JS_ToCStringLen(ctx, &data_len, argv[0]);
  } else {
    // Handle Buffer/Uint8Array
    data = JS_ToCString(ctx, argv[0]);
    data_len = data ? strlen(data) : 0;
  }

  if (!data) return JS_FALSE;

  // Write to socket
  JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
  JSValue write_result = JS_FALSE;

  if (JS_IsFunction(ctx, write_method)) {
    JSValue chunk = JS_NewStringLen(ctx, data, data_len);
    JSValue args[] = {chunk};
    write_result = JS_Call(ctx, write_method, res->socket, 1, args);
    JS_FreeValue(ctx, chunk);
  }

  JS_FreeValue(ctx, write_method);
  JS_FreeCString(ctx, data);

  // Return boolean indicating if write queue is full (for back-pressure)
  // If socket.write() returns false, caller should pause
  bool can_continue = true;
  if (JS_IsBool(write_result)) {
    can_continue = JS_ToBool(ctx, write_result);
  }
  JS_FreeValue(ctx, write_result);

  return can_continue ? JS_TRUE : JS_FALSE;
}
```

**Testing:**
```javascript
// Test implicit headers
const canContinue = res.write('Hello');
assert(typeof canContinue === 'boolean');

// Test back-pressure
const largeData = 'X'.repeat(1000000);
const result = res.write(largeData);
if (!result) {
  // Wait for 'drain' event
  res.socket.once('drain', () => res.write('More data'));
}
```

---

##### Task 2.4.4-ATOMIC: Chunked transfer encoding
**Status:** NEW (infrastructure exists in llhttp)
**Files:** `src/node/http/http_response.c`
**Complexity:** MEDIUM
**Estimated Time:** 35 min
**Dependencies:** 2.4.3-ATOMIC

**What exists:**
- use_chunked flag in JSHttpResponse (http_internal.h:101)
- llhttp handles chunked parsing (on_chunk_header, on_chunk_complete callbacks)

**What to implement:**
1. Detect when chunked encoding is needed
2. Format chunks properly
3. Send chunk terminator (0\r\n\r\n)
4. Set Transfer-Encoding header

**Implementation:**

**Step 1:** Detect chunked encoding need
```c
// In js_http_response_write_head() after setting headers:

// Determine if chunked encoding is needed
bool has_content_length = false;
JSValue cl_header = JS_GetPropertyStr(ctx, res->headers, "content-length");
if (!JS_IsUndefined(cl_header)) {
  has_content_length = true;
}
JS_FreeValue(ctx, cl_header);

// Use chunked if no Content-Length and HTTP/1.1
// (For now, assume HTTP/1.1 - can get from request later)
if (!has_content_length && res->status_code != 204 && res->status_code != 304) {
  res->use_chunked = true;
  JS_SetPropertyStr(ctx, res->headers, "transfer-encoding",
                   JS_NewString(ctx, "chunked"));
}
```

**Step 2:** Format chunks in write()
```c
// In js_http_response_write() when writing data:

if (res->use_chunked) {
  // Format as chunk: <size-hex>\r\n<data>\r\n
  char chunk_header[32];
  snprintf(chunk_header, sizeof(chunk_header), "%zx\r\n", data_len);

  // Write chunk header
  JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
  if (JS_IsFunction(ctx, write_method)) {
    JSValue header_str = JS_NewString(ctx, chunk_header);
    JSValue args[] = {header_str};
    JS_Call(ctx, write_method, res->socket, 1, args);
    JS_FreeValue(ctx, args[0]);

    // Write chunk data
    JSValue data_str = JS_NewStringLen(ctx, data, data_len);
    JSValue args2[] = {data_str};
    JS_Call(ctx, write_method, res->socket, 1, args2);
    JS_FreeValue(ctx, args2[0]);

    // Write chunk terminator
    JSValue term_str = JS_NewString(ctx, "\r\n");
    JSValue args3[] = {term_str};
    write_result = JS_Call(ctx, write_method, res->socket, 1, args3);
    JS_FreeValue(ctx, args3[0]);
  }
  JS_FreeValue(ctx, write_method);
} else {
  // Non-chunked write (existing code)
  // ...
}
```

**Step 3:** Send final chunk in end()
```c
// In js_http_response_end() before closing:

if (res->use_chunked) {
  // Send final chunk: 0\r\n\r\n
  JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
  if (JS_IsFunction(ctx, write_method)) {
    JSValue final_chunk = JS_NewString(ctx, "0\r\n\r\n");
    JSValue args[] = {final_chunk};
    JS_Call(ctx, write_method, res->socket, 1, args);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, write_method);
}
```

**Testing:**
```javascript
// Test chunked encoding
const server = http.createServer((req, res) => {
  // Don't set Content-Length
  res.writeHead(200);
  res.write('Chunk 1');
  res.write('Chunk 2');
  res.end('Final chunk');
  // Expected: Transfer-Encoding: chunked header
  // Expected: Properly formatted chunks
});
```

---

##### Task 2.4.5-ATOMIC: Enhanced end() method
**Status:** PARTIAL (basic exists, needs enhancement)
**Files:** `src/node/http/http_response.c` (js_http_response_end)
**Complexity:** SIMPLE
**Estimated Time:** 20 min
**Dependencies:** 2.4.4-ATOMIC

**What exists:**
- end() method exists (http_internal.h:170)
- Closes socket

**What to enhance:**
1. Support optional data parameter
2. Handle chunked terminator
3. Emit 'finish' event
4. Respect Connection header for keep-alive
5. Set finished flag

**Implementation:**

```c
JSValue js_http_response_end(JSContext* ctx, JSValueConst this_val,
                             int argc, JSValueConst* argv) {
  JSHttpResponse* res = JS_GetOpaque(this_val, js_http_response_class_id);
  if (!res) {
    return JS_UNDEFINED;
  }

  // Prevent double end()
  if (res->finished) {
    return JS_UNDEFINED;
  }

  // Write final data if provided
  if (argc > 0 && !JS_IsUndefined(argv[0])) {
    JSValue write_method = JS_GetPropertyStr(ctx, this_val, "write");
    if (JS_IsFunction(ctx, write_method)) {
      JS_Call(ctx, write_method, this_val, 1, argv);
    }
    JS_FreeValue(ctx, write_method);
  }

  // Send chunked terminator if needed
  if (res->use_chunked) {
    JSValue write_method = JS_GetPropertyStr(ctx, res->socket, "write");
    if (JS_IsFunction(ctx, write_method)) {
      JSValue final_chunk = JS_NewString(ctx, "0\r\n\r\n");
      JSValue args[] = {final_chunk};
      JS_Call(ctx, write_method, res->socket, 1, args);
      JS_FreeValue(ctx, args[0]);
    }
    JS_FreeValue(ctx, write_method);
  }

  // Mark as finished
  res->finished = true;

  // Emit 'finish' event
  JSValue emit = JS_GetPropertyStr(ctx, this_val, "emit");
  if (JS_IsFunction(ctx, emit)) {
    JSValue args[] = {JS_NewString(ctx, "finish")};
    JSValue result = JS_Call(ctx, emit, this_val, 1, args);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, args[0]);
  }
  JS_FreeValue(ctx, emit);

  // TODO: Check Connection header for keep-alive vs close
  // For now, close the connection (Phase 2.2 handles keep-alive)
  JSValue end_method = JS_GetPropertyStr(ctx, res->socket, "end");
  if (JS_IsFunction(ctx, end_method)) {
    JS_Call(ctx, end_method, res->socket, 0, NULL);
  }
  JS_FreeValue(ctx, end_method);

  return JS_UNDEFINED;
}
```

**Testing:**
```javascript
// Test end() with data
res.end('Final data');

// Test 'finish' event
res.on('finish', () => console.log('✅ Response finished'));

// Test double end()
res.end();
res.end();  // Should be no-op
```

---

#### Group 3.C: Error Handling & Testing

##### Task 2.4.6-ATOMIC: Write-after-end error handling
**Status:** PARTIAL (needs enhancement)
**Files:** `src/node/http/http_response.c`
**Complexity:** TRIVIAL
**Estimated Time:** 10 min
**Dependencies:** 2.4.5-ATOMIC

**What to implement:**
- Throw error when writing after end()
- Throw error when calling writeHead() after headers sent

**Implementation:**

```c
// Already implemented in enhanced write() (2.4.3-ATOMIC):
if (res->finished) {
  return JS_ThrowError(ctx, "Cannot write after end()");
}

// Already implemented in enhanced writeHead() (2.4.1-ATOMIC):
if (res->headers_sent) {
  return JS_ThrowError(ctx, "Headers already sent");
}
```

**Testing:**
```javascript
try {
  res.end();
  res.write('Too late');
  assert(false);
} catch (e) {
  console.log('✅ Write after end() prevented');
}
```

---

##### Task 2.4.7-ATOMIC: Integration testing for response writing
**Status:** NEW
**Files:** `target/tmp/test_response_writing.js`
**Complexity:** SIMPLE
**Estimated Time:** 30 min
**Dependencies:** All of Wave 3

**Test Cases:**

```javascript
// Test 1: writeHead() with parameters
// Test 2: Header manipulation
// Test 3: Implicit header writing
// Test 4: write() return value (back-pressure)
// Test 5: Chunked encoding
// Test 6: end() with data
// Test 7: Error handling (write after end)
// Test 8: 'finish' event

// Full test suite (~200 lines)
```

**Validation:**
- All response writing tests pass
- Chunked encoding works correctly
- Error handling prevents invalid states
- No memory leaks (ASAN)

---

### Wave 3 Summary

**Total Atomic Tasks:** 7
**Parallel Groups:** 1 (Group 3.A has 2 parallel tracks)
**Estimated Time:** 170 minutes (2h 50m)
**Risk Level:** MEDIUM-HIGH (chunked encoding complexity)

**Execution Order:**
1. Parallel: 2.4.1-ATOMIC || 2.4.2-ATOMIC (25 min)
2. Sequential: 2.4.3-ATOMIC (30 min)
3. Sequential: 2.4.4-ATOMIC (35 min)
4. Sequential: 2.4.5-ATOMIC, 2.4.6-ATOMIC (30 min)
5. Sequential: 2.4.7-ATOMIC (30 min)
6. Final validation: `make test && make wpt` (20 min)

**Critical Success Factors:**
- Chunked encoding must handle all edge cases
- Back-pressure mechanism prevents memory bloat
- Header state management prevents invalid transitions

---

## Dependency Graph

```
Phase 2.1 (Complete) ✅
    |
    v
Wave 1: Connection Handling
    |
    +-- 2.2.1-ATOMIC (HTTPConnection structure) [SEQUENTIAL]
    +-- 2.2.2-ATOMIC (Connection handler) [SEQUENTIAL, depends on 2.2.1]
    |       |
    |       +-- 2.2.3-ATOMIC (Lifecycle) [PARALLEL track 1]
    |       +-- 2.2.5-ATOMIC (Timeouts) [PARALLEL track 2]
    |               |
    +-- 2.2.6-ATOMIC (Close handling) [SEQUENTIAL, depends on 2.2.3+2.2.5]
    +-- 2.2.7-ATOMIC (Testing) [SEQUENTIAL, depends on all]
    |
    v
Wave 2: Request Handling
    |
    +-- 2.3.1-ATOMIC (Request line) [SEQUENTIAL verification]
    +-- 2.3.2-ATOMIC (Headers) [SEQUENTIAL verification]
    +-- 2.3.3-ATOMIC (URL parsing) [SEQUENTIAL verification]
    |       |
    |       +-- 2.3.4-ATOMIC (Body streaming) [PARALLEL track 1]
    |       +-- 2.3.5-ATOMIC (Special headers) [PARALLEL track 2]
    |               |
    +-- 2.3.7-ATOMIC (Error handling) [SEQUENTIAL, depends on 2.3.4+2.3.5]
    +-- 2.3.8-ATOMIC (Testing) [SEQUENTIAL, depends on all]
    |
    v
Wave 3: Response Writing
    |
    +-- 2.4.1-ATOMIC (writeHead) [PARALLEL track 1]
    +-- 2.4.2-ATOMIC (Header methods) [PARALLEL track 2]
    |       |
    +-- 2.4.3-ATOMIC (write) [SEQUENTIAL, depends on 2.4.1+2.4.2]
    +-- 2.4.4-ATOMIC (Chunked) [SEQUENTIAL, depends on 2.4.3]
    +-- 2.4.5-ATOMIC (end) [SEQUENTIAL, depends on 2.4.4]
    +-- 2.4.6-ATOMIC (Errors) [SEQUENTIAL, depends on 2.4.5]
    +-- 2.4.7-ATOMIC (Testing) [SEQUENTIAL, depends on all]
```

**Parallelization Efficiency:**
- Wave 1: 2/7 tasks parallel (29% parallel time)
- Wave 2: 2/5 tasks parallel (40% parallel time)
- Wave 3: 2/7 tasks parallel (29% parallel time)
- Overall: 6/19 tasks parallel (32% parallel opportunities)

---

## File Modification Matrix

| File | Wave 1 | Wave 2 | Wave 3 | Total Changes | Complexity |
|------|--------|--------|--------|---------------|------------|
| `http_internal.h` | ✅ Add 3 fields | ✅ Add 2 fields | - | +5 fields | TRIVIAL |
| `http_parser.c` | ✅✅✅ 3 functions | ✅✅ 2 functions | - | ~200 lines | MEDIUM |
| `http_server.c` | - | - | - | 0 lines | - |
| `http_incoming.c` | - | - | - | 0 lines | - |
| `http_response.c` | - | ✅ 1 function | ✅✅✅✅✅ 5 functions | ~300 lines | HIGH |
| `http_module.c` | - | - | - | 0 lines | - |
| **Total** | **~70 lines** | **~130 lines** | **~300 lines** | **~500 lines** | **MEDIUM** |

**Most Impacted Files:**
1. `http_response.c` - 5 major enhancements (~300 lines)
2. `http_parser.c` - 5 enhancements (~200 lines)
3. `http_internal.h` - Structure updates (~7 fields)

**Least Impacted Files:**
- `http_server.c`, `http_incoming.c`, `http_module.c` - No changes needed

---

## Testing Strategy

### Per-Wave Testing

**Wave 1 (Connection):**
```bash
# Create temporary test file
cat > target/tmp/test_connection_lifecycle.js << 'EOF'
// Test connection events, keep-alive, timeouts
// ~100 lines of test cases
EOF

# Run test
./target/debug/jsrt_g target/tmp/test_connection_lifecycle.js

# ASAN validation
./target/debug/jsrt_m target/tmp/test_connection_lifecycle.js
```

**Wave 2 (Request):**
```bash
# Create temporary test file
cat > target/tmp/test_request_handling.js << 'EOF'
// Test request parsing, body streaming, special headers
// ~150 lines of test cases
EOF

./target/debug/jsrt_g target/tmp/test_request_handling.js
./target/debug/jsrt_m target/tmp/test_request_handling.js
```

**Wave 3 (Response):**
```bash
# Create temporary test file
cat > target/tmp/test_response_writing.js << 'EOF'
// Test response writing, chunked encoding, headers
// ~200 lines of test cases
EOF

./target/debug/jsrt_g target/tmp/test_response_writing.js
./target/debug/jsrt_m target/tmp/test_response_writing.js
```

### After-Wave Validation

**After Each Wave:**
```bash
# Build
make clean && make jsrt_g

# Run all tests
make test

# Run WPT tests
make wpt

# Format check
make format
```

**Success Criteria:**
- ✅ All existing 165 tests pass
- ✅ New wave tests pass (100% pass rate)
- ✅ No ASAN errors or memory leaks
- ✅ WPT tests maintain 29/32 pass rate (no regressions)
- ✅ Code formatted correctly

### Final Integration Testing

**After Wave 3 Complete:**
```bash
# Comprehensive integration test
cat > target/tmp/test_phase2_complete.js << 'EOF'
const http = require('http');

// Test full request/response cycle with all features
const server = http.createServer((req, res) => {
  // Connection: keep-alive
  // Body streaming
  // Chunked encoding
  // Timeout handling
  // Error cases
});

server.setTimeout(5000);
server.listen(3000);

// Run multiple client requests
// ~300 lines of comprehensive tests
EOF

./target/debug/jsrt_g target/tmp/test_phase2_complete.js
./target/debug/jsrt_m target/tmp/test_phase2_complete.js
```

---

## Risk Assessment

### High-Risk Tasks

| Task | Risk Level | Mitigation |
|------|------------|------------|
| 2.2.5-ATOMIC (Timeouts) | HIGH | Use Fix #1.1 pattern (async timer cleanup) |
| 2.3.4-ATOMIC (Body streaming) | HIGH | Keep buffering for Phase 2, full streaming in Phase 4 |
| 2.4.4-ATOMIC (Chunked encoding) | HIGH | Follow RFC 7230 spec exactly, extensive testing |

**Mitigation Strategies:**

1. **Timeout Timers (2.2.5):**
   - MUST use uv_close() with async callback (Fix #1.1 pattern)
   - MUST stop timer before closing
   - Test with ASAN to catch use-after-free

2. **Body Streaming (2.3.4):**
   - Keep existing buffering for backward compatibility
   - Add 'data'/'end' events as Phase 4 prep
   - Don't break existing _body property access

3. **Chunked Encoding (2.4.4):**
   - Verify hex size formatting
   - Test with large chunks (>64KB)
   - Test with empty chunks
   - Ensure final chunk terminator is correct (0\r\n\r\n)

### Medium-Risk Tasks

| Task | Risk Level | Issue | Mitigation |
|------|------------|-------|------------|
| 2.2.3-ATOMIC (Lifecycle) | MEDIUM | Keep-alive parser state | Use Fix #1.2 pattern (llhttp_reset) |
| 2.3.7-ATOMIC (Error handling) | MEDIUM | Edge cases | Send proper HTTP error responses |
| 2.4.3-ATOMIC (write) | MEDIUM | Back-pressure | Return boolean from socket.write() |

### Low-Risk Tasks

All verification tasks (2.3.1, 2.3.2, 2.3.3) are LOW risk - just confirm existing functionality.

---

## Execution Timeline

### Estimated Timeline (Sequential Execution)

| Wave | Tasks | Parallel Time | Sequential Time | Testing | Total |
|------|-------|---------------|-----------------|---------|-------|
| **Wave 1** | 7 | 25 min | 55 min | 50 min | **130 min (2h 10m)** |
| **Wave 2** | 5 | 30 min | 60 min | 45 min | **135 min (2h 15m)** |
| **Wave 3** | 7 | 25 min | 95 min | 50 min | **170 min (2h 50m)** |
| **Integration** | - | - | - | 30 min | **30 min** |
| **Total** | **19** | **80 min** | **210 min** | **175 min** | **465 min (7h 45m)** |

### Optimized Timeline (Maximum Parallelization)

**Assumptions:**
- 2 parallel tracks can run simultaneously
- Wave transitions require full wave completion
- Testing is sequential

| Phase | Duration | Notes |
|-------|----------|-------|
| Wave 1 - Group 1.A | 15 min | Sequential foundation |
| Wave 1 - Group 1.B | 25 min | 2 parallel tracks |
| Wave 1 - Group 1.C | 15 min | Sequential cleanup |
| Wave 1 - Testing | 30 min | Sequential validation |
| **Wave 1 Total** | **85 min** | **Saved: 45 min** |
| | | |
| Wave 2 - Verification | 30 min | Sequential (3 tasks) |
| Wave 2 - Group 2.B | 30 min | 2 parallel tracks |
| Wave 2 - Group 2.C | 50 min | Sequential (2 tasks) |
| **Wave 2 Total** | **110 min** | **Saved: 25 min** |
| | | |
| Wave 3 - Group 3.A | 25 min | 2 parallel tracks |
| Wave 3 - Group 3.B | 85 min | Sequential (3 tasks) |
| Wave 3 - Group 3.C | 40 min | Sequential (2 tasks) |
| **Wave 3 Total** | **150 min** | **Saved: 20 min** |
| | | |
| **Integration Testing** | **30 min** | Final validation |
| **Grand Total** | **375 min (6h 15m)** | **Total Saved: 90 min (19%)** |

### Realistic Timeline (With Context Switching)

**Adding 20% overhead for context switching, debugging, and unexpected issues:**

- **Optimistic:** 375 min × 1.2 = **450 min (7h 30m)**
- **Realistic:** 375 min × 1.4 = **525 min (8h 45m)**
- **Pessimistic:** 375 min × 1.6 = **600 min (10h)**

**Recommended Execution Schedule:**
- **Day 1:** Wave 1 (3 hours with breaks)
- **Day 2:** Wave 2 (3 hours with breaks)
- **Day 3:** Wave 3 (4 hours with breaks)
- **Total:** 3 working days

---

## Quick Start Execution Guide

### Step 1: Prepare Environment

```bash
# Ensure clean state
make clean
make jsrt_g
make test  # Verify baseline: 165/165 passing

# Create temporary test directory
mkdir -p target/tmp
```

### Step 2: Execute Wave 1 (Connection Handling)

```bash
# 1. Structure enhancement (5 min)
# Edit http_internal.h - add 3 fields to JSHttpConnection

# 2. Connection handler (10 min)
# Edit http_parser.c - enhance js_http_connection_handler()

# 3. Parallel implementation (25 min)
# Track 1: Edit http_parser.c - enhance on_message_complete() for lifecycle
# Track 2: Edit http_parser.c - add timeout implementation

# 4. Close handling (15 min)
# Edit http_parser.c - enhance cleanup_connection()

# 5. Testing (30 min)
cat > target/tmp/test_connection_lifecycle.js << 'EOF'
// Test connection lifecycle
EOF
./target/debug/jsrt_g target/tmp/test_connection_lifecycle.js
./target/debug/jsrt_m target/tmp/test_connection_lifecycle.js

# 6. Validation
make test && make wpt
```

### Step 3: Execute Wave 2 (Request Handling)

```bash
# 1. Verification (30 min)
# Run verification tests for existing functionality

# 2. Parallel implementation (30 min)
# Track 1: Edit http_parser.c - add body streaming events
# Track 2: Edit http_parser.c/http_response.c - add special header handling

# 3. Error handling (25 min)
# Edit http_parser.c - add HTTP error responses

# 4. Testing (25 min)
cat > target/tmp/test_request_handling.js << 'EOF'
// Test request handling
EOF
./target/debug/jsrt_g target/tmp/test_request_handling.js
./target/debug/jsrt_m target/tmp/test_request_handling.js

# 5. Validation
make test && make wpt
```

### Step 4: Execute Wave 3 (Response Writing)

```bash
# 1. Parallel implementation (25 min)
# Track 1: Edit http_response.c - enhance writeHead()
# Track 2: Edit http_response.c - add getHeader/removeHeader/getHeaders

# 2. write() enhancement (30 min)
# Edit http_response.c - add flow control

# 3. Chunked encoding (35 min)
# Edit http_response.c - implement chunked transfer encoding

# 4. end() and errors (30 min)
# Edit http_response.c - enhance end() and error handling

# 5. Testing (30 min)
cat > target/tmp/test_response_writing.js << 'EOF'
// Test response writing
EOF
./target/debug/jsrt_g target/tmp/test_response_writing.js
./target/debug/jsrt_m target/tmp/test_response_writing.js

# 6. Validation
make test && make wpt
```

### Step 5: Final Integration

```bash
# Comprehensive integration test
cat > target/tmp/test_phase2_complete.js << 'EOF'
// Full Phase 2 integration test
EOF
./target/debug/jsrt_g target/tmp/test_phase2_complete.js
./target/debug/jsrt_m target/tmp/test_phase2_complete.js

# Final validation
make clean
make
make test
make wpt
make format

# Update plan document
# Mark Phase 2 as complete in node-http-plan.md
```

---

## Success Metrics

### Quantitative Metrics

| Metric | Target | Current | Post-Wave 1 | Post-Wave 2 | Post-Wave 3 |
|--------|--------|---------|-------------|-------------|-------------|
| Tests Passing | 100% | 165/165 | 175/175 | 185/185 | 195/195 |
| WPT Tests | ≥29/32 | 29/32 | 29/32 | 30/32 | 30/32 |
| API Coverage | 45 methods | 28/45 (62%) | 31/45 (69%) | 35/45 (78%) | 40/45 (89%) |
| Memory Leaks | 0 | 0 | 0 | 0 | 0 |
| Code Lines | ~500 new | 0 | ~70 | ~200 | ~500 |

### Qualitative Metrics

**Wave 1 Success:**
- ✅ Connection lifecycle managed correctly
- ✅ Keep-alive works for multiple requests
- ✅ Timeout handling prevents hung connections
- ✅ Resources cleaned up properly

**Wave 2 Success:**
- ✅ Request parsing handles all edge cases
- ✅ Body streaming events emitted correctly
- ✅ Special headers (Expect, Upgrade) handled
- ✅ Error responses prevent protocol violations

**Wave 3 Success:**
- ✅ Response writing supports all Node.js patterns
- ✅ Chunked encoding works correctly
- ✅ Header management prevents invalid states
- ✅ Flow control prevents memory bloat

---

## Conclusion

This parallel execution strategy provides a detailed roadmap for completing Phase 2 of the HTTP module implementation. By breaking down 22 subtasks into 19 atomic tasks organized into 3 sequential waves with internal parallelization, we can efficiently implement connection handling, request enhancements, and response writing features.

**Key Takeaways:**
1. **32% parallelization** reduces execution time by ~90 minutes
2. **Incremental testing** after each wave ensures quality
3. **Risk mitigation** addresses all high-risk areas with proven patterns
4. **Clear dependencies** prevent blocking and enable smooth execution
5. **Realistic timeline** of 3 working days with proper validation

**Next Steps:**
1. Review this strategy document
2. Execute Wave 1 (Connection Handling)
3. Validate and proceed to Wave 2
4. Complete Wave 3
5. Update main plan document with completion status

**Ready to begin execution!**
