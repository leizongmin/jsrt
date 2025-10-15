# HTTP Module Test Suite

This directory contains tests for the Node.js-compatible HTTP module implementation.

## Directory Structure

```
test/node/http/
├── server/         # Server-side HTTP tests
├── client/         # Client-side HTTP tests (ClientRequest, http.request, http.get)
└── integration/    # Integration tests (server + client interactions)
```

## Test Categories

### Server Tests (`server/`)

Tests focused on HTTP server functionality:

- **test_server_api_validation.js**: HTTP and Net module integration, API validation
- **test_server_functionality.js**: Basic server creation, listening, and lifecycle
- **test_response_writable.js**: ServerResponse as Writable stream (Phase 4.2)
- **test_stream_incoming.js**: IncomingMessage as Readable stream (Phase 4.1)

### Client Tests (`client/`)

Tests focused on HTTP client functionality:

- *(To be added as client features are implemented)*

### Integration Tests (`integration/`)

Tests that exercise both client and server together:

- **test_basic.js**: Module loading and basic API validation
- **test_advanced_networking.js**: HTTP/HTTPS agents, advanced networking features
- **test_edge_cases.js**: Error handling, edge cases, API validation

## Running Tests

### Run all HTTP tests
```bash
make test N=node/http
```

### Run specific test category
```bash
make test N=node/http/server
make test N=node/http/client
make test N=node/http/integration
```

### Run with ASAN (memory safety validation)
```bash
make jsrt_m
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 timeout 10 ./bin/jsrt_m test/node/http/server/test_server_api_validation.js
```

### Run individual test
```bash
timeout 10 ./bin/jsrt test/node/http/integration/test_basic.js
```

## Test Status

Current test coverage:

- **Server tests**: 4 tests
- **Client tests**: 0 tests (pending implementation)
- **Integration tests**: 3 tests

**Total**: 7 HTTP tests

## Expected Behavior

All tests should:
- Pass with 100% success rate
- Be ASAN-clean (no memory leaks, no segfaults)
- Complete within timeout (typically 10 seconds)

## Known Issues

- **test_stream_incoming.js**: Currently failing (pre-existing issue, not related to recent changes)
- All other tests should pass

## Implementation Progress

See `docs/plan/node-http-plan.md` for detailed implementation plan and progress tracking.

Current phase: **Phase 4 - Streaming & Pipes** (72% complete)
- Phase 4.1: IncomingMessage Readable stream ✅
- Phase 4.2: ServerResponse Writable stream ✅
- Phase 4.3: ClientRequest Writable stream ✅
