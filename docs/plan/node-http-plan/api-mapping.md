# Node.js HTTP API Mapping to jsrt

**Created**: 2025-10-10
**Status**: Phase 0 - Research Complete

## Overview

Complete mapping of Node.js HTTP API (45 methods/properties) to jsrt implementation strategy.

## Server API (15 items)

| Node.js API | Current Status | Implementation File | Priority | Notes |
|-------------|----------------|---------------------|----------|-------|
| **http.createServer([options][, requestListener])** | PARTIAL | http_module.c | P0 | Add options support |
| **http.Server** class | PARTIAL | http_server.c | P0 | Enhance with full API |
| **server.listen([port][, host][, backlog][, callback])** | EXISTS | http_server.c | P0 | Already working |
| **server.close([callback])** | EXISTS | http_server.c | P0 | Already working |
| **server.address()** | MISSING | http_server.c | P1 | Return {address, family, port} |
| **server.setTimeout([msecs][, callback])** | MISSING | http_server.c | P1 | Set default timeout |
| **server.maxHeadersCount** | MISSING | http_server.c (property) | P2 | Default 2000 |
| **server.timeout** | MISSING | http_server.c (property) | P2 | Default 0 (no timeout) |
| **server.keepAliveTimeout** | MISSING | http_server.c (property) | P2 | Default 5000ms |
| **server.headersTimeout** | MISSING | http_server.c (property) | P2 | Default 60000ms |
| **server.requestTimeout** | MISSING | http_server.c (property) | P2 | Default 300000ms |
| **Event: 'request'** | EXISTS | http_callbacks.c | P0 | Already emitted |
| **Event: 'connection'** | MISSING | http_callbacks.c | P1 | Emit on new connection |
| **Event: 'close'** | MISSING | http_callbacks.c | P1 | Emit when server stops |
| **Event: 'checkContinue'** | MISSING | http_parser.c | P3 | For Expect: 100-continue |
| **Event: 'upgrade'** | MISSING | http_parser.c | P3 | For WebSocket handshake |

## Client API (20 items)

| Node.js API | Current Status | Implementation File | Priority | Notes |
|-------------|----------------|---------------------|----------|-------|
| **http.request(url[, options][, callback])** | MOCK | http_module.c | P0 | Full implementation needed |
| **http.get(url[, options][, callback])** | MISSING | http_module.c | P0 | Wrapper for request() |
| **http.ClientRequest** class | PARTIAL | http_client.c | P0 | Needs full implementation |
| **request.write(chunk[, encoding][, callback])** | MISSING | http_client.c | P0 | Write request body |
| **request.end([data][, encoding][, callback])** | MISSING | http_client.c | P0 | Finalize request |
| **request.abort()** | MISSING | http_client.c | P1 | Cancel request |
| **request.setTimeout([timeout][, callback])** | MISSING | http_client.c | P1 | Set request timeout |
| **request.setHeader(name, value)** | MISSING | http_client.c | P0 | Set request header |
| **request.getHeader(name)** | MISSING | http_client.c | P1 | Get request header |
| **request.removeHeader(name)** | MISSING | http_client.c | P1 | Remove header before send |
| **request.setNoDelay([noDelay])** | MISSING | http_client.c | P2 | TCP_NODELAY option |
| **request.setSocketKeepAlive([enable][, initialDelay])** | MISSING | http_client.c | P2 | SO_KEEPALIVE option |
| **request.flushHeaders()** | MISSING | http_client.c | P2 | Send headers immediately |
| **Event: 'response'** | MISSING | http_client.c | P0 | Emit when response received |
| **Event: 'socket'** | MISSING | http_client.c | P1 | Provide socket access |
| **Event: 'connect'** | MISSING | http_client.c | P1 | For CONNECT method |
| **Event: 'timeout'** | MISSING | http_client.c | P1 | Request timeout |
| **Event: 'error'** | MISSING | http_client.c | P0 | Error event |
| **Event: 'abort'** | MISSING | http_client.c | P1 | Request aborted |
| **Event: 'finish'** | MISSING | http_client.c | P1 | Request sent |

## IncomingMessage API (10 items)

| Node.js API | Current Status | Implementation File | Priority | Notes |
|-------------|----------------|---------------------|----------|-------|
| **message.headers** | EXISTS | http_incoming.c | P0 | Already working |
| **message.rawHeaders** | MISSING | http_incoming.c | P1 | Array format |
| **message.httpVersion** | EXISTS | http_incoming.c | P0 | Already working |
| **message.method** | EXISTS | http_incoming.c | P0 | Request only |
| **message.statusCode** | EXISTS | http_incoming.c | P0 | Response only |
| **message.statusMessage** | EXISTS | http_incoming.c | P0 | Response only |
| **message.url** | EXISTS | http_incoming.c | P0 | Request only |
| **message.socket** | PARTIAL | http_incoming.c | P1 | Reference to socket |
| **message.trailers** | MISSING | http_incoming.c | P2 | Trailing headers |
| **message.rawTrailers** | MISSING | http_incoming.c | P2 | Array format |
| **Event: 'data'** | MISSING | http_parser.c | P0 | Body chunks |
| **Event: 'end'** | MISSING | http_parser.c | P0 | Message complete |
| **Event: 'close'** | MISSING | http_parser.c | P1 | Connection closed |

## ServerResponse API (12 items)

| Node.js API | Current Status | Implementation File | Priority | Notes |
|-------------|----------------|---------------------|----------|-------|
| **response.writeHead(statusCode[, statusMessage][, headers])** | EXISTS | http_response.c | P0 | Already working |
| **response.setHeader(name, value)** | EXISTS | http_response.c | P0 | Already working |
| **response.getHeader(name)** | MISSING | http_response.c | P1 | Case-insensitive get |
| **response.getHeaders()** | MISSING | http_response.c | P1 | All headers object |
| **response.removeHeader(name)** | MISSING | http_response.c | P1 | Remove before send |
| **response.hasHeader(name)** | MISSING | http_response.c | P2 | Check existence |
| **response.write(chunk[, encoding][, callback])** | EXISTS | http_response.c | P0 | Already working |
| **response.end([data][, encoding][, callback])** | EXISTS | http_response.c | P0 | Already working |
| **response.writeContinue()** | MISSING | http_response.c | P3 | Send 100 Continue |
| **response.writeProcessing()** | MISSING | http_response.c | P3 | Send 102 Processing |
| **response.addTrailers(headers)** | MISSING | http_response.c | P2 | Add trailing headers |
| **response.headersSent** | EXISTS | http_response.c | P0 | Boolean property |
| **response.statusCode** | EXISTS | http_response.c | P0 | Status property |
| **response.statusMessage** | EXISTS | http_response.c | P0 | Message property |
| **response.sendDate** | MISSING | http_response.c (property) | P2 | Auto-add Date header |

## Agent API (8 items)

| Node.js API | Current Status | Implementation File | Priority | Notes |
|-------------|----------------|---------------------|----------|-------|
| **new Agent([options])** | PARTIAL | http_agent.c | P1 | Enhance with pooling |
| **agent.maxSockets** | EXISTS | http_agent.c | P1 | Default 5 |
| **agent.maxFreeSockets** | EXISTS | http_agent.c | P1 | Default 256 |
| **agent.sockets** | MISSING | http_agent.c | P1 | Active sockets map |
| **agent.freeSockets** | MISSING | http_agent.c | P1 | Free sockets map |
| **agent.requests** | MISSING | http_agent.c | P1 | Queued requests |
| **agent.destroy()** | MISSING | http_agent.c | P2 | Close all sockets |
| **http.globalAgent** | EXISTS | http_module.c | P1 | Default agent |

## Implementation Priority Legend

- **P0**: Critical path (must have for basic functionality)
- **P1**: Important (needed for common use cases)
- **P2**: Nice to have (improves compatibility)
- **P3**: Advanced (edge cases, special protocols)

## Phase Alignment

### Phase 0: Research & Architecture (Current)
- ✅ API mapping complete
- ✅ Priority assignment done

### Phase 1: Modular Refactoring
- Extract existing P0 APIs into modular structure
- Server, Response, IncomingMessage classes

### Phase 2: Server Enhancement
- Add missing server P0/P1 APIs
- Implement full parser integration
- Add server events

### Phase 3: Client Implementation
- Implement P0 client APIs (request, get, write, end)
- Add response parsing
- Client events

### Phase 4: Streaming & Pipes
- IncomingMessage Readable stream
- ServerResponse Writable stream
- ClientRequest Writable stream
- 'data' and 'end' events

### Phase 5: Advanced Features
- P1/P2 timeout handling
- Header limits
- Trailer support
- Special features (100-continue, upgrade)

## API Coverage Target

**Total API Surface**: 45 methods/properties/events

**Current Coverage**:
- Server: 7/15 (47%)
- Client: 0/20 (0%)
- IncomingMessage: 6/10 (60%)
- ServerResponse: 6/12 (50%)
- Agent: 2/8 (25%)

**Overall**: 21/65 items (32%)

**Phase Targets**:
- Phase 2 complete: 35/65 (54%)
- Phase 3 complete: 50/65 (77%)
- Phase 5 complete: 65/65 (100%)

## Implementation Notes

### Case-Insensitive Headers
- Use `strcasecmp()` for lookups
- Store original case for rawHeaders
- Node.js compatibility: `Content-Type` === `content-type`

### Multi-Value Headers
- Some headers allow multiples (Set-Cookie, Vary)
- Store as array when multiple values
- rawHeaders preserves all occurrences

### Chunked Encoding
- Auto-enable when no Content-Length
- Format: `{size-hex}\r\n{data}\r\n`
- Terminal chunk: `0\r\n\r\n`

### Keep-Alive
- Parse Connection header
- Reset parser for next request
- Agent tracks reusable sockets

### Timeout Hierarchy
- server.timeout → default for all connections
- request.setTimeout() → per-request override
- server.headersTimeout → headers arrival limit
- server.requestTimeout → total request limit
- agent.timeout → keep-alive idle timeout

## Testing Strategy per API

### Server APIs
```javascript
// test/node/http/server/test_server_api.js
- createServer() with options
- listen() with various args
- address() returns correct info
- setTimeout() applies to connections
- Events: request, connection, close
```

### Client APIs
```javascript
// test/node/http/client/test_client_api.js
- request() with URL/options
- get() convenience method
- write() request body
- end() finalize
- abort() cancel
- setHeader/getHeader/removeHeader
- Events: response, error, socket, timeout
```

### Message APIs
```javascript
// test/node/http/message/test_message_api.js
- headers (case-insensitive)
- rawHeaders (preserves case)
- httpVersion, method, url
- statusCode, statusMessage
- Events: data, end, close
```

### Response APIs
```javascript
// test/node/http/response/test_response_api.js
- writeHead() with headers
- setHeader/getHeader/removeHeader/hasHeader
- write() body chunks
- end() finalize
- headersSent flag
- Chunked encoding
```

### Agent APIs
```javascript
// test/node/http/agent/test_agent_api.js
- new Agent() with options
- Socket pooling (maxSockets)
- Socket reuse (keep-alive)
- Request queueing
- freeSockets management
```

## Reference

**Node.js HTTP Docs**: https://nodejs.org/api/http.html

**Current Implementation**: src/node/node_http.c (992 lines)

**Modular Target**: src/node/http/ (10 files, ~2300 lines)
