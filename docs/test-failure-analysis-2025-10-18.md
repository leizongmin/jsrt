# Test Failure Analysis - 2025-10-18

## Executive Summary

**Status**: 4 out of 208 tests failing (98% pass rate)
**Module**: All failures in `node/net` module
**Root Cause**: Network socket cleanup and data encoding issues

## Failed Tests

1. `test_node_net_test_api.js` - **Subprocess aborted**
2. `test_node_net_test_concurrent_operations.js` - **Subprocess aborted**
3. `test_node_net_test_encoding.js` - **Failed (1 test)**
4. `test_node_net_test_properties.js` - **Timeout**

## Detailed Analysis

### 1. test_encoding.js - Data Encoding Issue

**Status**: 8 passed, 1 failed
**Failure**: Test "data received as string by default or with utf8 encoding" times out

#### Root Cause

File: `/repo/src/node/net/net_callbacks.c:208`

```c
// Current implementation - ALWAYS returns string
JSValue data = JS_NewStringLen(ctx, buf->base, nread);
```

**Problem**: The code always converts received data to a string, regardless of whether `setEncoding()` has been called. When encoding is not set, data should be returned as a Buffer (Uint8Array), not a string.

**Expected Behavior**:
- If `conn->encoding` is set (e.g., "utf8"): return string
- If `conn->encoding` is NULL: return Buffer/Uint8Array

**Impact**: The test at `test/node/net/test_encoding.js:62-104` expects:
1. Data to be received as string after calling `setEncoding('utf8')`
2. The 'data' event to fire with the correct type

The current implementation may be causing the 'data' event not to fire correctly, leading to timeout.

#### Proposed Fix

```c
// Around line 200-210 in net_callbacks.c
JSValue data;
if (conn->encoding && strcmp(conn->encoding, "utf8") == 0) {
    // Return string for utf8 encoding
    data = JS_NewStringLen(ctx, buf->base, nread);
} else {
    // Return Uint8Array (Buffer) for binary data
    data = JS_NewArrayBufferCopy(ctx, (const uint8_t*)buf->base, nread);
    // Wrap in Uint8Array for Buffer compatibility
    JSValue uint8_array_ctor = JS_GetGlobalObject(ctx);
    JSValue uint8_array_proto = JS_GetPropertyStr(ctx, uint8_array_ctor, "Uint8Array");
    data = JS_CallConstructor(ctx, uint8_array_proto, 1, &data);
    JS_FreeValue(ctx, uint8_array_proto);
    JS_FreeValue(ctx, uint8_array_ctor);
}
```

---

### 2. test_api.js - Subprocess Aborted

**Status**: 6 tests pass, then crash during cleanup
**Error**: `Aborted (core dumped)`

#### Symptoms
```
✓ net.isIP helpers
✓ server.listen emits "listening" and calls callback
✓ server.close without listen triggers callback immediately
✓ net.connect invokes callback once and connection listener fires
✓ socket writes queued before connect flush after connection
✓ Socket constructor creates valid disconnected socket
[Crash during cleanup]
```

#### Root Cause

The test completes successfully but crashes during runtime cleanup. This was previously addressed by the jsrt-developer agent who added socket cleanup in finalizers:

File: `/repo/src/node/net/net_finalizers.c`

Recent changes added:
- Type checking before closing handles
- `jsrt_net_cleanup_active_sockets()` function

**Current Status**:
- ASAN build: PASSES ✓
- Release/Debug builds: CRASH ✗

#### Analysis

The crash occurs in release builds but not ASAN builds, indicating:
1. Use-after-free or undefined behavior that ASAN catches
2. Race condition in cleanup sequence
3. Invalid handle state during shutdown

The debug output shows:
```
[RA-] timer    0x60c50377e3e8
[RA-] tcp      0x60c50378f2b0
[RA-] timer    0x60c50378b250
```

These are "resource not freed" warnings indicating handles still active during shutdown.

#### Proposed Investigation

1. Verify the order of cleanup: GC → finalizers → event loop → close loop
2. Ensure all socket handles are properly closed before runtime shutdown
3. Add defensive checks in finalizers to prevent double-free
4. Consider adding explicit cleanup phase before runtime destruction

---

### 3. test_concurrent_operations.js - Subprocess Aborted

**Status**: 1 test passes, then crash
**Error**: `Aborted (core dumped)`

#### Symptoms
```
✓ basic server and client connection
[debug] uv_loop still alive counter=0
[-AI] async    0x60c50374de70
[RA-] timer    0x60c50377e3e8
[RA-] tcp      0x60c50378f2b0
[RA-] timer    0x60c50378b250
[Crash]
```

#### Root Cause

Same as test_api.js - cleanup issue. The test creates multiple concurrent network operations and the cleanup process doesn't properly handle:
1. Multiple active sockets
2. Pending timers associated with sockets
3. Async handles for the event loop

The "uv_loop still alive counter=0" message suggests the loop thinks it's done but there are still active handles.

---

### 4. test_properties.js - Timeout

**Status**: All tests pass when run individually, TIMEOUT during `make test`
**Error**: Test timeout (exceeds 20 seconds)

#### Symptoms
```bash
# Individual run - PASSES
$ timeout 25 ./bin/jsrt test/node/net/test_properties.js
✓ Socket state properties correct before connection
✓ Socket properties correct during connection
...
Test Results: 6 passed, 0 failed
[Completes successfully]

# During make test - TIMEOUT
Test #123: test_node_net_test_properties.js .....Timeout
```

#### Root Cause

**Event loop not draining properly**. The test completes all assertions but doesn't exit because:

1. **Lingering handles**: Sockets or timers from previous tests aren't cleaned up
2. **Event loop keeps running**: Some handle is keeping the event loop alive
3. **Cumulative effect**: Works in isolation, fails when run after other net tests

This is a **test isolation issue** - previous tests (test_api.js, test_concurrent_operations.js) leave resources that interfere with test_properties.js.

#### Evidence

1. Test passes standalone → no bug in test logic
2. Test times out in suite → previous test cleanup issue
3. Debug shows timers and tcp handles active → cleanup not completing

#### Proposed Fix

**Immediate**: Ensure all tests properly clean up resources
```javascript
// In each test's cleanup
server.close(() => {
    client.destroy();
    // Allow event loop to drain
    setTimeout(() => process.exit(0), 100);
});
```

**Long-term**: Fix the underlying cleanup issues in test_api.js and test_concurrent_operations.js

---

## Summary Table

| Test | Status | Root Cause | Priority | Difficulty |
|------|--------|-----------|----------|------------|
| test_encoding.js | 1 failure | Data encoding not checked | P0 | Easy |
| test_api.js | Crash | Cleanup/finalization issue | P0 | Medium |
| test_concurrent_operations.js | Crash | Cleanup/finalization issue | P0 | Medium |
| test_properties.js | Timeout | Event loop not draining | P1 | Easy |

## Recommended Fix Order

1. **test_encoding.js** (Easy, isolated fix)
   - Fix data encoding logic in `net_callbacks.c`
   - Add encoding check before creating JSValue

2. **test_api.js + test_concurrent_operations.js** (Same root cause)
   - Investigate finalizer cleanup sequence
   - Add better handle lifecycle management
   - Ensure proper close callbacks

3. **test_properties.js** (Dependency on #2)
   - Should resolve automatically after fixing cleanup issues
   - If not, add explicit event loop draining

## Files Requiring Changes

### High Priority
- `/repo/src/node/net/net_callbacks.c` - Fix encoding logic (line ~208)
- `/repo/src/node/net/net_finalizers.c` - Improve cleanup robustness

### Medium Priority
- `/repo/src/runtime.c` - Verify cleanup sequence
- `/repo/test/node/net/test_*.js` - Add better cleanup in tests

## Testing Strategy

After each fix:
```bash
# 1. Test individually
timeout 20 ./bin/jsrt test/node/net/test_encoding.js
timeout 20 ./bin/jsrt test/node/net/test_api.js
timeout 20 ./bin/jsrt test/node/net/test_concurrent_operations.js
timeout 25 ./bin/jsrt test/node/net/test_properties.js

# 2. Test with ASAN
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/node/net/test_api.js

# 3. Test full suite
make test N=node/net

# 4. Test full suite
make test
```

## Success Criteria

- All 208 tests pass
- No memory leaks detected by ASAN
- No crashes during cleanup
- Tests complete within timeout limits
- Tests pass both individually and in suite

---

**Analysis completed**: 2025-10-18
**Analyst**: Claude Code (jsrt-code-reviewer)
