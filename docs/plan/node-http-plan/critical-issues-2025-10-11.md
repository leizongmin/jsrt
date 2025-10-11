# Critical HTTP Issues - 2025-10-11

**Status**: 🔴 **CRITICAL - Phase 4 Tests Failing**
**Discovered**: 2025-10-11
**Severity**: HIGH

## Executive Summary

While Phase 4 documentation claims "Core Complete", actual testing reveals:
- ❌ **0/8 Phase 4.2 tests passing** (test_response_writable.js)
- ❌ **0/10 Phase 4.1 tests passing** (test_stream_incoming.js)
- ❌ **HTTP GET requests hang** - client-server communication broken

## Completed Fixes Today

### ✅ Fix #1: Timer Use-After-Free (Commit d7a5481)
**Issue**: Heap-use-after-free in `http_listen_async_cleanup()`
- ASAN error: freed memory accessed by libuv event loop
- Caused: SIGSEGV crashes when starting HTTP server

**Fix**:
- Added `http_listen_timer_close_cb()` callback
- Changed `uv_timer_stop()` to `uv_close()` with proper async cleanup
- File: `src/node/http/http_server.c:8-36`

**Result**: ✅ No more crashes

### ✅ Fix #2: Listen Callback Not Working (Commit d7a5481)
**Issue**: `http.Server.listen(port, callback)` passed callback to wrong position
- Error: "Invalid bind address: () => { ... }"
- Root cause: net.Server.listen expects (port, host, callback)

**Fix**:
- Added argument parsing logic in `js_http_server_listen()`
- Detects callback position and rearranges args for net.Server
- File: `src/node/http/http_server.c:59-110`

**Result**: ✅ Listen callbacks execute properly

### ✅ Fix #3: Missing server.address() Method (Commit 15c3082)
**Issue**: Tests hung because `server.address()` was undefined
- HTTP server didn't delegate to net.Server.address()

**Fix**:
- Added `js_http_server_address()` method
- Delegates to underlying net.Server
- File: `src/node/http/http_server.c:130-147, 241`

**Result**: ✅ Can retrieve server port/address

## Outstanding Critical Issues

### ❌ Issue #4: HTTP GET Requests Hang

**Symptom**:
```javascript
http.get(`http://127.0.0.1:${port}/`, (res) => {
  // Never executes
});
// Request hangs forever
```

**Evidence**:
```bash
$ target/release/jsrt /tmp/test_http_get.js
1. Creating server
3. Server listening on port 52058
4. Making GET request
8. Request initiated
TIMEOUT  # <-- Hangs here
```

**Impact**:
- ALL Phase 4 tests require HTTP GET to work
- test_response_writable.js: 0/8 tests passing
- test_stream_incoming.js: 0/10 tests passing

**Root Cause**: Unknown - needs investigation
- Server listens successfully
- Client request initiated
- No connection made between them

**Next Steps**:
1. Check if `http.get()` is implemented
2. Verify HTTP client sends actual TCP requests
3. Debug connection establishment
4. Check if server accepts connections

### ❌ Issue #5: Phase 4 Stream Tests Not Executing

**Symptom**:
```bash
$ target/release/jsrt test/node/http/test_response_writable.js
Running ServerResponse Writable stream tests...

0/8 tests passed  # No test output at all
```

**Root Cause**: Depends on Issue #4 (HTTP GET)
- Tests use async pattern with `http.get()`
- Without working client, tests can't run

## Test Status

| Test File | Status | Passing | Notes |
|-----------|--------|---------|-------|
| test_basic.js | ✅ PASS | All | API existence checks only |
| test_server_functionality.js | ✅ PASS | All | No HTTP GET used |
| test_response_writable.js | ❌ FAIL | 0/8 | Requires HTTP GET |
| test_stream_incoming.js | ❌ FAIL | 0/10 | Requires HTTP GET |

## Action Items

### Immediate (P0)
- [ ] **Debug HTTP GET hanging issue**
  - Verify http.get() implementation exists
  - Check if TCP connection is attempted
  - Add debug logging to client request flow

- [ ] **Fix HTTP client-server communication**
  - Ensure client sends requests
  - Ensure server receives connections
  - Verify llhttp parser integration

### Short Term (P1)
- [ ] **Verify Phase 4 streaming functionality**
  - Once GET works, retest Phase 4.1 and 4.2
  - May reveal additional stream implementation issues

- [ ] **Update documentation to reflect actual status**
  - Phase 4 status should be "BLOCKED" not "COMPLETE"
  - Add critical issues section
  - Update progress numbers

## Files Modified Today

1. `src/node/http/http_server.c` - 3 fixes (timer, args, address)
2. Git commits: d7a5481, 15c3082

## Reproduction Steps

```bash
# 1. Build jsrt
make clean && make

# 2. Test HTTP GET (hangs)
cat > /tmp/test_http_get.js << 'EOF'
const http = require('node:http');
const server = http.createServer((req, res) => {
  console.log('Got request');
  res.end('OK');
});
server.listen(0, () => {
  const port = server.address().port;
  http.get(`http://127.0.0.1:${port}/`, (res) => {
    console.log('Got response');
    server.close();
  });
});
EOF

timeout 2 target/release/jsrt /tmp/test_http_get.js
# Expected: "Got request" + "Got response"
# Actual: Hangs after "Making GET request"

# 3. Run Phase 4 tests (0/8 pass)
target/release/jsrt test/node/http/test_response_writable.js
```

## Conclusion

**Phase 4 is NOT complete**. While the code was written, critical runtime issues prevent it from functioning:
1. ✅ Server crashes fixed
2. ✅ API methods added
3. ❌ Client-server communication broken
4. ❌ All streaming tests fail

**Recommendation**: Mark Phase 4 as BLOCKED until HTTP GET is fixed.
