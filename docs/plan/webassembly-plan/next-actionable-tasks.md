# WebAssembly Plan - Next Actionable Tasks

## Status Summary (2025-10-19)

**Overall Progress**: 83/220 tasks (38%)

**Key Discovery**: Exported globals work perfectly via WAMR Runtime API!
- Instance-exported Memory/Table/Global likely also work
- Blocker only affects standalone host-created objects
- Practical WASM use cases are functional

## Completed Phases

- ✅ **Phase 1**: Infrastructure & Error Types (100%)
- ✅ **Phase 5**: Async Compilation API (100%)
- ✅ **Phase 6**: Streaming API (100% - fallback implementation)
- ✅ **Phase 8**: Documentation & Polish (100%)

## Blocked Phases

- 🔴 **Phase 2**: Memory (52%) - Tasks 2.4-2.6 blocked
- 🔴 **Phase 4**: Table & Global (0%) - All tasks blocked
- 🔵 **Phase 3**: Instance & Exports (42%) - Partial (i32 only)
- 🔵 **Phase 7**: WPT Integration (20%) - Limited by blockers

## Immediate Actionable Tasks (Priority Order)

### 1. Implement Exported Memory Support ⭐ **HIGH VALUE**

**Rationale**: Same pattern as Global - already proven to work

**Implementation**:
```c
// In Instance.exports getter (src/std/webassembly.c:~1410)
} else if (export_info.kind == WASM_IMPORT_EXPORT_KIND_MEMORY) {
  wasm_memory_inst_t mem_inst = wasm_runtime_get_default_memory(instance_data->instance);

  // Create Memory object
  JSValue memory_obj = JS_NewObjectClass(ctx, js_webassembly_memory_class_id);

  // Store instance reference + memory_inst
  // Use is_host=false, similar to Global
  // Implement buffer getter using wasm_runtime_get_memory_data()
}
```

**Files to modify**:
- `src/std/webassembly.c` (Instance.exports, Memory data structure)
- Add union to `jsrt_wasm_memory_data_t` (host vs exported)

**Expected outcome**:
- `instance.exports.mem.buffer` returns working ArrayBuffer
- `memory.grow()` works for exported memories
- New test: `test/web/webassembly/test_web_wasm_exported_memory.js`

**Effort**: 2-3 hours

---

### 2. Implement Exported Table Support ⭐ **MEDIUM VALUE**

**Rationale**: Complete the exported object pattern

**Implementation**:
```c
} else if (export_info.kind == WASM_IMPORT_EXPORT_KIND_TABLE) {
  wasm_table_inst_t table_inst;
  wasm_runtime_get_export_table_inst(instance_data->instance, export_info.name, &table_inst);

  // Create Table object
  // Store instance reference + table_inst
  // Implement get/set/grow via Runtime API
}
```

**Expected outcome**:
- `instance.exports.table.length` returns actual size
- `table.get(index)` / `table.set(index, value)` work
- New test: `test/web/webassembly/test_web_wasm_exported_table.js`

**Effort**: 2-3 hours

---

### 3. Fix Standalone Constructor Behavior ⭐ **QUICK WIN**

**Rationale**: Stop returning garbage, throw helpful errors instead

**Implementation**:
```c
// In WebAssembly.Global constructor (line ~2068)
static JSValue js_webassembly_global_constructor(JSContext* ctx, ...) {
  // Add at the beginning:
  return JS_ThrowTypeError(ctx,
    "WebAssembly.Global constructor not supported. "
    "Use globals exported from WASM modules instead. "
    "See: https://github.com/anthropics/jsrt/blob/main/docs/webassembly-api-compatibility.md");
}
```

**Apply to**:
- `WebAssembly.Global` constructor
- `WebAssembly.Memory` constructor
- `WebAssembly.Table` constructor

**Expected outcome**:
- Clear error messages instead of garbage values
- Users understand limitation immediately
- No false hope from "successful" construction

**Effort**: 30 minutes

---

### 4. Extend Function Type Support (Phase 3.2B) ⭐ **HIGH IMPACT**

**Rationale**: Unlock f32/f64/i64 function imports/exports

**Current limitation**: Only i32 supported

**Investigation needed**:
- Review WAMR native registration signature requirements
- Determine if per-type stubs are needed
- Consider dynamic code generation approach

**Blocker**: Phase 3.2B plan documents this as blocked by WAMR API design

**Recommendation**: Defer until Memory/Table exports complete

**Effort**: 1-2 days (research + implementation)

---

### 5. Add WPT Tests for Exported Objects ⭐ **VALIDATION**

**Rationale**: Prove exported Memory/Table/Global work

**Implementation**:
- Create custom WPT tests specifically for exported objects
- Don't rely on standard WPT tests (they test constructors)
- Add to `wasm` category in run-wpt.py

**Tests needed**:
- `wasm/jsapi-jsrt/exported-memory.any.js`
- `wasm/jsapi-jsrt/exported-table.any.js`
- `wasm/jsapi-jsrt/exported-global.any.js` (already done in unit tests)

**Effort**: 2-3 hours

---

### 6. Update Documentation ⭐ **USER CLARITY**

**Files to update**:
1. `docs/webassembly-api-compatibility.md`:
   - Split "Constructor" vs "Exported from instance" rows
   - Show ✅ for exported, 🔴 for constructor

2. `AGENTS.md` / `CLAUDE.md`:
   - Update WebAssembly section with working patterns
   - Add examples of using exported Memory/Table/Global

3. `examples/wasm/`:
   - Add `exported-memory.js`
   - Add `exported-table.js`
   - Add `exported-global.js`

**Effort**: 1-2 hours

---

## Long-term Unblocking Strategies

### Option A: Upgrade WAMR

**Action**: Test with WAMR v2.5+ or latest main branch

**Steps**:
1. Update `deps/wamr` submodule to newer version
2. Run full test suite (212 tests)
3. Test standalone Global/Memory/Table constructors
4. If fixed, enable WPT tests

**Risk**: Breaking API changes, requires regression testing

**Timeline**: 1 day investigation + potential fixes

---

### Option B: Contribute to WAMR

**Action**: Submit PR to WAMR to fix standalone object support

**Approach**:
- Modify C API to support host-created objects
- Or document why it's not supported
- Work with WAMR maintainers

**Timeline**: Weeks/months (upstream process)

---

### Option C: Accept Limitation

**Action**: Document as "not supported", rely on exported objects

**Benefits**:
- No code changes needed
- Practical use cases already work
- Clear user expectations

**Documentation**:
- ✅ Already done in blocker docs
- Need to update public-facing docs

---

## Recommended Execution Order

1. **Quick win**: Task 3 (Fix constructors - throw errors) - 30 min
2. **High value**: Task 1 (Exported Memory) - 2-3 hours
3. **Complete pattern**: Task 2 (Exported Table) - 2-3 hours
4. **Validate**: Task 5 (WPT tests for exported objects) - 2-3 hours
5. **Communicate**: Task 6 (Documentation updates) - 1-2 hours

**Total estimated time**: 1-2 days of focused work

**Expected outcome**:
- All practical WASM use cases functional
- Clear error messages for unsupported features
- Comprehensive test coverage
- User-friendly documentation

---

## Success Metrics

After completing tasks 1-6:

- ✅ Exported Memory: buffer access, grow() working
- ✅ Exported Table: get/set/grow() working
- ✅ Exported Global: value get/set working (already done)
- ✅ Unit tests: 220+ tests (currently 213)
- ✅ Clear errors: Constructors throw helpful messages
- ✅ Documentation: Working paths documented
- ⚠️ WPT tests: Still limited (requires standalone constructors)
- ✅ User satisfaction: Practical WASM apps work

---

## Notes

- Focus on **what works** rather than what's blocked
- Exported objects pattern is the path forward
- WAMR limitation affects all three APIs equally
- Runtime API is more functional than C API
- Current implementation is closer to complete than initially thought

---

**Last Updated**: 2025-10-19
**Next Review**: After completing tasks 1-3
