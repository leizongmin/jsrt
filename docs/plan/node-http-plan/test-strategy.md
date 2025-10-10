# Test Strategy for node:http Module

**Created**: 2025-10-10
**Status**: Phase 0 - Research Complete

## Overview

Comprehensive testing strategy for node:http implementation covering unit tests, integration tests, ASAN validation, and WPT compliance.

## Test Directory Structure

```
test/node/http/
├── server/
│   ├── test_basic.js              # Basic server functionality
│   ├── test_request.js            # Request handling
│   ├── test_response.js           # Response writing
│   ├── test_headers.js            # Header management
│   ├── test_chunked.js            # Chunked encoding
│   ├── test_keepalive.js          # Keep-alive connections
│   ├── test_timeout.js            # Timeout handling
│   └── test_edge_cases.js         # Error scenarios
├── client/
│   ├── test_basic.js              # Basic client requests
│   ├── test_request.js            # ClientRequest API
│   ├── test_response.js           # Response parsing
│   ├── test_headers.js            # Client headers
│   ├── test_methods.js            # HTTP methods (GET, POST, etc.)
│   └── test_edge_cases.js         # Client errors
├── parser/
│   ├── test_request_parsing.js   # Request line parsing
│   ├── test_response_parsing.js  # Response parsing
│   ├── test_headers.js            # Header parsing
│   ├── test_chunked.js            # Chunked encoding parsing
│   ├── test_malformed.js          # Error handling
│   └── test_http10.js             # HTTP/1.0 compatibility
├── integration/
│   ├── test_client_server.js     # Local client↔server
│   ├── test_streaming.js         # Pipe, streams
│   ├── test_keepalive.js         # Connection reuse
│   ├── test_agent.js              # Agent pooling
│   └── test_errors.js             # Error propagation
└── asan/
    ├── test_memory_leaks.js       # Memory leak detection
    ├── test_parser_lifecycle.js   # Parser cleanup
    └── test_connection_cleanup.js # Connection cleanup
```

## Existing HTTP Tests

### Current Test Files (to be organized)
```bash
$ grep -r "http" test/ --include="*.js" | grep -v node_modules
test/node/test_node_http.js
test/node/test_node_http_client.js
test/node/test_node_http_events.js
test/node/test_node_http_multiple.js
test/node/test_node_http_server.js
test/node/test_node_http_timeout.js
```

**Migration Plan**:
1. Move test_node_http_server.js → test/node/http/server/test_basic.js
2. Move test_node_http_client.js → test/node/http/client/test_basic.js
3. Move test_node_http_events.js → test/node/http/integration/test_client_server.js
4. Enhance tests based on new modular structure

## Unit Test Coverage Matrix

### Server Tests (test/node/http/server/)

| Test File | Coverage | Key Tests |
|-----------|----------|-----------|
| **test_basic.js** | createServer, listen, close | - createServer() with/without listener<br>- listen() with port/host/callback<br>- close() callback<br>- address() after listen<br>- Multiple servers on different ports |
| **test_request.js** | Request parsing | - method, url, httpVersion<br>- headers (case-insensitive)<br>- rawHeaders array<br>- URL parsing (/path?query)<br>- Multiple requests per connection |
| **test_response.js** | Response writing | - writeHead() status/headers<br>- setHeader/getHeader/removeHeader<br>- write() chunks<br>- end() with data<br>- headersSent flag |
| **test_headers.js** | Header handling | - Case-insensitive access<br>- Multi-value headers<br>- Special headers (Host, Connection)<br>- maxHeadersCount limit<br>- Header size limits |
| **test_chunked.js** | Chunked encoding | - Transfer-Encoding: chunked<br>- Auto-chunking (no Content-Length)<br>- Chunk boundaries<br>- Final 0-chunk |
| **test_keepalive.js** | Connection reuse | - Keep-Alive header<br>- Multiple requests/responses<br>- Parser reset<br>- Idle timeout |
| **test_timeout.js** | Timeout handling | - server.setTimeout()<br>- Per-connection timeout<br>- headersTimeout<br>- requestTimeout<br>- keepAliveTimeout |
| **test_edge_cases.js** | Error scenarios | - Invalid HTTP syntax<br>- Oversized headers<br>- Write after end<br>- clientError event<br>- Malformed requests → 400 |

### Client Tests (test/node/http/client/)

| Test File | Coverage | Key Tests |
|-----------|----------|-----------|
| **test_basic.js** | Basic requests | - http.request() URL<br>- http.get() convenience<br>- ClientRequest methods<br>- Response event |
| **test_request.js** | ClientRequest API | - write() request body<br>- end() finalize<br>- abort() cancel<br>- setHeader/getHeader/removeHeader<br>- setTimeout() |
| **test_response.js** | Response parsing | - IncomingMessage (response)<br>- statusCode, statusMessage<br>- headers<br>- Body streaming ('data', 'end') |
| **test_headers.js** | Client headers | - Custom headers<br>- Host header (auto-set)<br>- Connection header<br>- Content-Length/Transfer-Encoding |
| **test_methods.js** | HTTP methods | - GET, POST, PUT, DELETE<br>- HEAD, OPTIONS, PATCH<br>- Body with POST/PUT |
| **test_edge_cases.js** | Error scenarios | - DNS errors<br>- Connection refused<br>- Timeout errors<br>- Parse errors<br>- Abort handling |

### Parser Tests (test/node/http/parser/)

| Test File | Coverage | Key Tests |
|-----------|----------|-----------|
| **test_request_parsing.js** | Request line | - Various HTTP methods<br>- URL formats<br>- HTTP/1.0 vs 1.1<br>- Query string parsing |
| **test_response_parsing.js** | Response line | - Status codes (200, 404, 500)<br>- Status messages<br>- HTTP version |
| **test_headers.js** | Header parsing | - Single-line headers<br>- Multi-line headers (folding)<br>- Duplicate headers<br>- Case preservation |
| **test_chunked.js** | Chunked parsing | - Chunk size parsing<br>- Chunk data extraction<br>- Final chunk<br>- Trailers after chunks |
| **test_malformed.js** | Error cases | - Invalid methods<br>- Malformed URLs<br>- Invalid headers<br>- Protocol errors |
| **test_http10.js** | HTTP/1.0 | - No keep-alive by default<br>- Connection: keep-alive<br>- No chunked encoding |

### Integration Tests (test/node/http/integration/)

| Test File | Coverage | Key Tests |
|-----------|----------|-----------|
| **test_client_server.js** | Full cycle | - Local server + client<br>- Request → Response flow<br>- Various methods & bodies<br>- Error propagation |
| **test_streaming.js** | Streams integration | - req.pipe(res) proxy<br>- fs.createReadStream().pipe(res)<br>- req.pipe(fs.createWriteStream())<br>- Back-pressure handling |
| **test_keepalive.js** | Connection reuse | - Multiple requests on one socket<br>- Agent pooling<br>- Socket limits<br>- Idle timeout |
| **test_agent.js** | Agent API | - new Agent() options<br>- maxSockets enforcement<br>- Request queueing<br>- Socket reuse<br>- destroy() cleanup |
| **test_errors.js** | Error scenarios | - Network errors<br>- Timeout errors<br>- Parse errors<br>- Event propagation |

## ASAN Validation Tests

### Memory Leak Detection (test/node/http/asan/)

**Purpose**: Ensure zero memory leaks in all scenarios

**Test Files**:

1. **test_memory_leaks.js** - General leak detection
   ```javascript
   // Create/destroy many servers
   for (let i = 0; i < 100; i++) {
     const server = http.createServer();
     server.listen(0, () => server.close());
   }

   // Create/destroy many clients
   for (let i = 0; i < 100; i++) {
     const req = http.request('http://localhost:1234');
     req.abort();
   }
   ```

2. **test_parser_lifecycle.js** - Parser cleanup
   ```javascript
   // Parse many requests
   // Ensure parsers are properly destroyed
   // Check llhttp_t cleanup
   ```

3. **test_connection_cleanup.js** - Connection cleanup
   ```javascript
   // Open/close many connections
   // Verify deferred cleanup (close_count)
   // Check all libuv handles closed
   ```

**Validation**:
```bash
make jsrt_m
./target/debug/jsrt_m test/node/http/asan/test_memory_leaks.js
# Check output for leaks
```

**Success Criteria**: Zero leaks, zero errors

## Test Coverage Goals

### By Phase

| Phase | Coverage Target | Key Tests |
|-------|-----------------|-----------|
| **Phase 0** | Baseline documented | Existing tests pass |
| **Phase 1** | 40% (modular structure) | Server, Response, IncomingMessage unit tests |
| **Phase 2** | 60% (server complete) | Full server API, parser integration |
| **Phase 3** | 80% (client complete) | Client API, response parsing |
| **Phase 4** | 90% (streaming) | Stream integration, pipe() |
| **Phase 5** | 95% (advanced) | Timeouts, limits, special features |
| **Phase 6** | 100% (testing phase) | All edge cases, ASAN clean |

### Coverage Categories

1. **API Coverage**: All 45 methods/properties tested
2. **Scenario Coverage**: Common use cases + edge cases
3. **Error Coverage**: All error paths exercised
4. **Memory Coverage**: ASAN clean on all tests
5. **Compatibility Coverage**: HTTP/1.0 and HTTP/1.1

## Test Execution Strategy

### Manual Test Run
```bash
# Run specific test
./target/release/jsrt test/node/http/server/test_basic.js

# Run all HTTP tests
for f in test/node/http/**/*.js; do
  ./target/release/jsrt "$f" || echo "FAILED: $f"
done
```

### Automated Test Suite
```bash
# Via make test (integrated)
make test
# Should include all test/node/http/ tests
```

### ASAN Validation
```bash
# Build ASAN version
make jsrt_m

# Run critical tests
./target/debug/jsrt_m test/node/http/asan/test_memory_leaks.js
./target/debug/jsrt_m test/node/http/integration/test_client_server.js
```

### WPT Validation
```bash
# Ensure no regressions
make wpt
# Should maintain or improve pass rate
```

## Test Writing Guidelines

### Test File Template

```javascript
#!/usr/bin/env jsrt
// test/node/http/server/test_example.js

const assert = require('assert');
const http = require('http');

console.log('Testing HTTP server example...');

// Test 1: Basic functionality
{
  const server = http.createServer((req, res) => {
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.end('Hello World');
  });

  server.listen(0, () => {
    const port = server.address().port;

    // Make request
    http.get(`http://localhost:${port}`, (res) => {
      assert.strictEqual(res.statusCode, 200);

      let body = '';
      res.on('data', (chunk) => body += chunk);
      res.on('end', () => {
        assert.strictEqual(body, 'Hello World');
        server.close();
      });
    });
  });
}

console.log('✓ All tests passed');
```

### Minimal Output Principle

**DO**:
- Print test name at start
- Print "✓ Test passed" for success
- Print error details for failures
- Keep output under 10 lines per test

**DON'T**:
- Print verbose debug info
- Dump large objects
- Print on every assertion
- Spam console with progress

### Error Handling Pattern

```javascript
// Good: Fail fast with clear message
try {
  doSomething();
  assert(condition, 'Expected condition to be true');
} catch (err) {
  console.error('✗ Test failed:', err.message);
  process.exit(1);
}

// Bad: Silent failures
doSomething();  // No error handling
```

## Test Organization Migration

### Phase 1: Create Structure
```bash
mkdir -p test/node/http/{server,client,parser,integration,asan}
```

### Phase 2: Move Existing Tests
```bash
# Identify existing HTTP tests
ls test/node/test_node_http*.js

# Move to new structure
mv test/node/test_node_http_server.js test/node/http/server/test_basic.js
mv test/node/test_node_http_client.js test/node/http/client/test_basic.js
# ... etc
```

### Phase 3: Add New Tests
- Create tests for missing APIs
- Add parser-specific tests
- Add ASAN validation tests

### Phase 4: Update Test Runner
- Ensure make test finds all test/node/http/ tests
- Add to CMakeLists.txt if needed

## Baseline Test Results

### Current State (from Phase 0)

**make test**: ✅ 165/165 tests passed (100%)
**make wpt**: ✅ 29/32 passed (90.6%, 3 skipped)

**Existing HTTP Tests** (to verify):
```bash
test/node/test_node_http.js
test/node/test_node_http_client.js
test/node/test_node_http_events.js
test/node/test_node_http_multiple.js
test/node/test_node_http_server.js
test/node/test_node_http_timeout.js
```

**Action**: Run each individually to establish baseline

## Success Criteria per Phase

### Phase 1: Modular Refactoring
- ✅ All existing HTTP tests pass
- ✅ No new test failures
- ✅ ASAN clean on existing tests

### Phase 2: Server Enhancement
- ✅ test/node/http/server/ all pass
- ✅ test/node/http/parser/ request parsing pass
- ✅ ASAN clean

### Phase 3: Client Implementation
- ✅ test/node/http/client/ all pass
- ✅ test/node/http/parser/ response parsing pass
- ✅ ASAN clean

### Phase 4: Streaming
- ✅ test/node/http/integration/test_streaming.js pass
- ✅ Pipe operations work
- ✅ ASAN clean

### Phase 5: Advanced Features
- ✅ All timeout tests pass
- ✅ Header limit tests pass
- ✅ Special feature tests pass
- ✅ ASAN clean

### Phase 6: Testing & Validation
- ✅ 100% API coverage
- ✅ All tests pass (make test)
- ✅ No WPT regressions (make wpt)
- ✅ Zero ASAN errors on all tests
- ✅ Test coverage report generated

## Continuous Validation

### After Every Code Change

```bash
# 1. Format code
make format

# 2. Run tests
make test

# 3. Check WPT
make wpt

# 4. ASAN validation (for significant changes)
make jsrt_m
./target/debug/jsrt_m test/node/http/integration/test_client_server.js
```

### Before Commit

```bash
# Full validation suite
make format && make test && make wpt
# Must all pass with no errors
```

## Test Documentation

### Test Index (to be created)

**test/node/http/README.md**:
- List all test files
- Describe what each tests
- Note any special requirements
- Link to Node.js HTTP docs

### Coverage Report (to be generated)

- API coverage: X/45 items (Y%)
- Code coverage: Z% of http/ directory
- ASAN status: Clean/Issues
- Performance benchmarks

## References

- **CLAUDE.md**: Testing guidelines
- **.claude/docs/testing.md**: Detailed testing guide
- **Node.js HTTP Docs**: https://nodejs.org/api/http.html
- **Existing Tests**: test/node/test_node_http*.js
