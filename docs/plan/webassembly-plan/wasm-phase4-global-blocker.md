# Tasks 4.6-4.8 Blocker: WAMR C API Standalone Global Limitation

## Summary

**Tasks 4.6-4.8** (WebAssembly.Global Constructor and Methods) are **BLOCKED** due to a fundamental limitation in WAMR v2.4.1's C API implementation.

## Discovery

This blocker was discovered on 2025-10-19 during **Task 7.9** (Add Global WPT Tests). While Tasks 4.6-4.8 were previously marked DONE, they were implemented **without proper unit tests** and the implementation was **never validated** with actual value read/write operations.

### Test Results

When WPT Global tests were added and run:
- **11 test files** executed
- **0 passed, 11 failed** (0.0% pass rate)
- All Global value operations return **garbage/uninitialized memory**

Example failures:
```javascript
const g = new WebAssembly.Global({value: 'i32'}, 42);
console.log(g.value);  // Expected: 42, Got: -1995696936 (garbage)
```

## Problem Description

The WebAssembly JavaScript API specification requires creating standalone `WebAssembly.Global` objects via the constructor:

```javascript
const global = new WebAssembly.Global({
  value: 'i32',
  mutable: true
}, 42);

console.log(global.value);  // Should be 42
```

However, WAMR v2.4.1's C API implementation does **NOT** support creating functional standalone globals. Globals created with `wasm_global_new()` appear to create objects, but:
1. `wasm_global_get()` returns uninitialized/garbage values
2. The initial value passed to `wasm_global_new()` is not properly stored
3. Values cannot be reliably read back from host-created globals

## Technical Details

### Implementation Status

The current implementation in `src/std/webassembly.c`:
- ✅ Creates `wasm_global_t` objects successfully
- ✅ Stores metadata (type, mutability) correctly
- ❌ **Cannot read/write actual values** - `wasm_global_get()` returns garbage
- ❌ Initial value passed to `wasm_global_new()` is not accessible

### Code Analysis

**Constructor** (`src/std/webassembly.c:2068-2195`):
```c
// This code runs without error but values are not functional
wasm_global_t* global = wasm_global_new(store, global_type, &initial);
// initial.of.i32 = 42 is passed, but cannot be retrieved!
```

**Value Getter** (`src/std/webassembly.c:2197-2219`):
```c
wasm_val_t value;
memset(&value, 0, sizeof(value));
value.kind = data->kind;

if (data->is_host) {
  wasm_global_get(data->u.host, &value);  // Returns garbage!
}

return jsrt_wasm_global_value_to_js(ctx, data->kind, &value);
```

### Evidence of WAMR Limitation

This limitation is **identical to the Table blocker** (Task 4.1, documented in `wasm-phase4-table-blocker.md`):

1. **Memory API**: WAMR C API Memorys require module instance context
2. **Table API**: WAMR explicitly documents "DO NOT SUPPORT" standalone tables
3. **Global API**: Same pattern - globals created without instance context are non-functional

### What Works vs What Doesn't

✅ **Works:**
- Creating global objects (no errors)
- Accessing global metadata (type, mutability)
- **Globals exported from a WASM module instance** ✅ VERIFIED WORKING
  - Tested 2025-10-19: Exported globals work perfectly
  - `instance.exports.myGlobal.value` returns correct values
  - `global.value = newValue` sets values correctly
  - Uses WAMR Runtime API (not C API)
  - See: `target/tmp/test_exported_global.js`

❌ **Doesn't Work:**
- Creating functional standalone globals with `wasm_global_new()`
- Getting/setting values of host-created globals
- Reading initial values passed to constructor
- All WPT Global constructor tests (0% pass rate)

## Impact

### Blocked Features
- WebAssembly.Global constructor (Task 4.6) - Appears to work but is non-functional
- Global.prototype.value getter/setter (Task 4.7) - Returns garbage values
- Global.prototype.valueOf() (Task 4.8) - Returns garbage values
- All Global WPT tests (Task 7.9) - 0/11 passing

### Test Results
```
$ make wpt N=wasm
wasm/jsapi/global/constructor.any.js - FAIL (128+ failures)
wasm/jsapi/global/value-get-set.any.js - FAIL (191+ failures)
wasm/jsapi/global/valueOf.any.js - FAIL (1 failure)
```

### False Completion

Tasks 4.6-4.8 were marked DONE on 2025-10-18 with:
- "Tests: make test ✅ 208/208"
- But **NO Global-specific unit tests exist**
- Implementation was never validated with actual value operations
- Only metadata/structure was tested, not functionality

## Resolution Options

### Option 1: Upgrade WAMR (Recommended)

Check if newer WAMR versions support standalone globals. If WAMR > v2.4.1 fixes this:
- Update deps/wamr submodule
- Retest implementation
- Enable WPT Global tests

**Risk**: Breaking changes in newer WAMR, requires full regression testing

### Option 2: Use Internal WAMR APIs

Bypass the C API and use WAMR's internal Runtime API directly:
- Similar to how Memory/Table could potentially work with Runtime API
- Requires deep dive into WAMR internals
- May be fragile across WAMR versions

**Risk**: Maintenance burden, potential bugs, WAMR version lock-in

### Option 3: Defer Implementation (Current Status)

Document as "not yet implemented" until WAMR support available:
- Remove fake implementation that returns garbage
- Throw `Error: WebAssembly.Global not supported` from constructor
- Document limitation in compatibility matrix
- Wait for WAMR fix or alternative solution

**Risk**: Feature incompleteness, applications cannot use globals

### Option 4: Implement via Module Exports ✅ **ALREADY WORKING**

**Status Update (2025-10-19)**: Exported globals already work perfectly!

Current implementation:
- ✅ Instance.exports correctly wraps exported globals (line 1388-1409)
- ✅ Uses WAMR Runtime API (`wasm_runtime_get_export_global_inst`)
- ✅ `global.value` getter/setter fully functional
- ✅ `global.valueOf()` works correctly
- ✅ Tested and verified with instance-exported globals

**What's missing:**
- ❌ Standalone `new WebAssembly.Global()` constructor (blocked by WAMR)
- ❌ Passing Global objects as imports to instances
- ❌ WPT tests that require standalone constructor

**Recommendation:**
- Document that standalone Global constructor is not supported
- Keep exported global functionality (it works)
- Throw explicit error from constructor instead of returning garbage
- Update API compatibility matrix to clarify working vs blocked scenarios

**Risk**: Incomplete spec compliance, but practical use cases (exported globals) work

## Recommended Action

**Immediate (Task 7.9 completion):**
1. ✅ Document this blocker
2. ✅ Update Tasks 4.6-4.8 to BLOCKED status
3. ✅ Revert Global WPT tests from run-wpt.py (until unblocked)
4. ✅ Update plan with accurate status
5. ✅ Add proper validation before marking tasks DONE in future

**Short-term:**
- Investigate Option 4 (Module exports) as this matches Memory/Table approach
- Add unit tests that validate actual functionality, not just structure
- Consider throwing explicit "not supported" error instead of returning garbage

**Long-term:**
- Research WAMR roadmap for standalone global support
- Consider contributing upstream fix to WAMR
- Monitor WAMR releases for C API improvements

## Lessons Learned

1. **Never mark tasks DONE without comprehensive tests**
   - Task 4.6-4.8 had NO unit tests for value operations
   - Tests only verified structure, not functionality
   - WPT tests would have caught this immediately

2. **Test early, test often**
   - Phase 7 (WPT Integration) should run in parallel with implementation
   - Don't defer testing until "after implementation"
   - WPT tests are the source of truth for spec compliance

3. **WAMR C API pattern**
   - Memory, Table, and Global all have same limitation
   - C API objects require module instance context
   - Runtime API is more functional for host operations

## Related Documents

- [Task 4.1 Table Blocker](./wasm-phase4-table-blocker.md) - Identical issue with Table API
- [WebAssembly Plan](../webassembly-plan.md) - Main implementation plan
- [API Compatibility Matrix](../../webassembly-api-compatibility.md) - Feature status

## Status

- **Discovered**: 2025-10-19
- **Impact**: High (3 tasks, 0% WPT pass rate)
- **Priority**: Medium (non-critical feature, workarounds exist)
- **Resolution**: Pending investigation of Options 1-4
