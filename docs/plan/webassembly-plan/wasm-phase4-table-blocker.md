# Task 4.1 Blocker: WAMR C API Standalone Table Limitation

## Summary

**Task 4.1** (WebAssembly.Table Constructor) is **BLOCKED** due to a fundamental limitation in WAMR v2.4.1's C API implementation.

## Problem Description

The WebAssembly JavaScript API specification requires creating standalone `WebAssembly.Table` objects via the constructor:

```javascript
const table = new WebAssembly.Table({
  element: 'funcref',
  initial: 10,
  maximum: 20
});
```

However, WAMR v2.4.1's C API implementation does **NOT** support creating standalone tables. Tables created with `wasm_table_new()` require a module instance runtime context to function correctly.

## Technical Details

### Evidence from WAMR Source Code

1. **Sample Code Confirmation** (`deps/wamr/samples/wasm-c-api/src/table.c:206-208`):
   ```c
   // Create stand-alone table.
   // DO NOT SUPPORT
   // TODO(wasm+): Once Wasm allows multiple tables, turn this into import.
   printf("Bypass Creating stand-alone table...\n");
   ```

2. **`wasm_table_size()` Implementation** (`deps/wamr/core/iwasm/common/wasm_c_api.c`):
   ```c
   wasm_table_size_t
   wasm_table_size(const wasm_table_t *table)
   {
       if (!table || !table->inst_comm_rt) {
           return 0;  // Returns 0 if no instance runtime!
       }
       // ... requires module instance to access table data
   }
   ```

3. **`wasm_table_grow()` Error Message**:
   ```
   Calling wasm_table_grow() by host is not supported.
   Only allow growing a table via the opcode table.grow
   ```

### What Works vs What Doesn't

✅ **Works:**
- Creating tables exported from a WASM module instance
- Accessing/modifying tables that are part of a module instance
- Table operations from within WASM code (via opcodes)

❌ **Doesn't Work:**
- Creating standalone tables with `wasm_table_new()`
- Getting size of standalone tables (always returns 0)
- Growing standalone tables from host code
- Getting/setting elements in standalone tables

## Test Results

Created comprehensive test suite (`test/jsrt/test_jsrt_wasm_table.js`) with 24 tests:
- **11 tests failed** due to the blocker
- **13 tests passed** (error handling and validation tests)

Key failures:
- Tables created with constructor always have `length === 0`
- `table.grow()` throws "not supported" error
- `table.get()` and `table.set()` fail with "index out of bounds"

## Impact on Implementation Plan

### Current Status
- **Phase 4, Task 4.1**: Implementation complete but non-functional ✗ BLOCKED
- All C code written and compiles successfully
- Comprehensive test suite created
- **Root cause**: WAMR C API limitation

### Affected Tasks
- **Task 4.1**: WebAssembly.Table Constructor ✗ BLOCKED
- **Task 4.2**: Table Import Support ⚠️ May be blocked (needs investigation)
- **Task 4.3**: Table Export Support ✓ Should work (tables from instances)

## Resolution Options

### Option 1: Upgrade WAMR (Recommended)
Check if newer WAMR versions support standalone tables.

**Steps:**
1. Review WAMR changelog/releases since v2.4.1
2. Test with newer WAMR version if available
3. Update WAMR submodule if compatible

**Pros:** Proper standards-compliant solution
**Cons:** May introduce breaking changes or require code updates

### Option 2: Use WAMR Internal APIs
Bypass the C API and use WAMR's internal table structures directly.

**Steps:**
1. Study WAMR internal table implementation
2. Create tables using internal structures
3. Manage table memory manually

**Pros:** Works with current WAMR version
**Cons:**
- Breaks API encapsulation
- May be incompatible with future WAMR updates
- More complex memory management

### Option 3: Defer Implementation
Document as "not yet implemented" and defer until WAMR support improves.

**Steps:**
1. Throw appropriate error from constructor
2. Document limitation
3. Skip Phase 4 in WPT tests

**Pros:** Clean, honest approach
**Cons:** Missing WebAssembly API feature

## Recommendation

**Option 1** (Upgrade WAMR) is recommended because:
1. WebAssembly multi-table proposal is now standard
2. Newer WAMR versions likely support this
3. Maintains clean API usage
4. Standards-compliant implementation

## Code Artifacts

### Implementation Files
- `src/std/webassembly.c:1498-1755` - Table constructor and methods
- `test/jsrt/test_jsrt_wasm_table.js` - Comprehensive test suite (24 tests)

### Current Implementation Status
✅ Constructor parameter validation
✅ Element type validation (funcref/externref)
✅ Initial/maximum size validation
✅ Error handling (TypeError, RangeError)
✅ Table.prototype.length getter
✅ Table.prototype.grow() method
✅ Table.prototype.get() method
✅ Table.prototype.set() method
✅ Proper memory management and finalizers
❌ **Functional standalone tables (BLOCKED by WAMR)**

## Next Steps

1. ✅ Document blocker (this document)
2. ⏳ Update implementation plan with blocked status
3. ⏳ Run `make format` on completed code
4. ⏳ Research WAMR upgrade path
5. ⏳ Decide on resolution approach
6. ⏳ Update task 4.2 and 4.3 status based on findings

---

**Created:** 2025-10-17
**WAMR Version:** v2.4.1
**Status:** BLOCKED - Awaiting decision on resolution approach
