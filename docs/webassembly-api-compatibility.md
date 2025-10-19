# WebAssembly JavaScript API - Compatibility Matrix

## Implementation Status

Last Updated: 2025-10-19

### Namespace & Validation
| API | Status | Notes |
|-----|--------|-------|
| WebAssembly.validate(bytes) | ✅ Implemented | Full validation via WAMR |
| WebAssembly.compile(bytes) | ✅ Implemented | Async compilation via Promise |
| WebAssembly.instantiate(bytes, imports) | ✅ Implemented | Returns Promise<{module, instance}> |
| WebAssembly.instantiate(module, imports) | ✅ Implemented | Returns Promise<Instance> |
| WebAssembly.compileStreaming(source) | ⚠️ Limited | Fallback: buffers full response, no incremental compilation |
| WebAssembly.instantiateStreaming(source, imports) | ⚠️ Limited | Fallback: buffers full response, no incremental compilation |

### WebAssembly.Module
| API | Status | Notes |
|-----|--------|-------|
| new WebAssembly.Module(bytes) | ✅ Implemented | Full support |
| Module.exports(module) | ✅ Implemented | Returns export descriptors |
| Module.imports(module) | ✅ Implemented | Returns import descriptors |
| Module.customSections(module, name) | ✅ Implemented | Direct binary parsing |

### WebAssembly.Instance
| API | Status | Notes |
|-----|--------|-------|
| new WebAssembly.Instance(module, imports) | ✅ Implemented | i32 function imports only |
| instance.exports | ✅ Implemented | Functions (i32), Memory, Table (partial) |
| instance.exports.function | ✅ Implemented | i32 type support only |
| instance.exports.memory | ✅ Implemented | Exported Memory fully functional (2025-10-19) |
| instance.exports.table | ⚠️ Limited | Exported Table: length ✅, get/set/grow ❌ (2025-10-19) |

### WebAssembly.Memory
| API | Status | Notes |
|-----|--------|-------|
| new WebAssembly.Memory(descriptor) | 🔴 Blocked | Constructor throws helpful TypeError (2025-10-19) |
| memory.buffer | ✅ Implemented | **Exported memories only** - returns ArrayBuffer |
| memory.grow(delta) | ✅ Implemented | **Exported memories only** - via wasm_runtime_enlarge_memory |
| **Workaround** | ✅ Available | Use `instance.exports.mem` instead of constructor |

### WebAssembly.Table
| API | Status | Notes |
|-----|--------|-------|
| new WebAssembly.Table(descriptor) | 🔴 Blocked | Constructor throws helpful TypeError (2025-10-19) |
| table.length | ✅ Implemented | **Exported tables only** - reads table_inst.cur_size |
| table.get(index) | 🔴 Blocked | Not supported for exported tables (WAMR Runtime API limitation) |
| table.set(index, value) | 🔴 Blocked | Not supported for exported tables (WAMR Runtime API limitation) |
| table.grow(delta, value) | 🔴 Blocked | Not supported for exported tables (WAMR Runtime API limitation) |
| **Workaround** | ⚠️ Partial | Use `instance.exports.table.length` - get/set/grow unavailable |

### WebAssembly.Global
| API | Status | Notes |
|-----|--------|-------|
| new WebAssembly.Global(descriptor, value) | 🔴 Blocked | WAMR C API limitation - returns garbage values |
| global.value (getter) | 🔴 Blocked | Returns uninitialized memory (WAMR limitation) |
| global.value (setter) | 🔴 Blocked | Cannot set values (WAMR limitation) |
| global.valueOf() | 🔴 Blocked | Returns garbage values (WAMR limitation) |

### Error Types
| API | Status | Notes |
|-----|--------|-------|
| WebAssembly.CompileError | ✅ Implemented | Proper Error subclass |
| WebAssembly.LinkError | ✅ Implemented | Proper Error subclass |
| WebAssembly.RuntimeError | ✅ Implemented | Proper Error subclass |

## Known Limitations

### Type Support
- **Function imports/exports:** i32 only (f32/f64/i64/BigInt planned for Phase 3.2B)
- **Multi-value returns:** Not yet supported
- **Multiple module namespaces:** Only "env" supported for imports

### WAMR API Blockers
- **Memory API:** WAMR v2.4.1 C API does not support standalone Memory objects
  - Created Memory objects have no accessible data region
  - `wasm_memory_data_size()` returns 0 for host-created memories
  - `wasm_memory_grow()` explicitly blocked for host calls
  - **✅ Resolution (2025-10-19):** Exported Memory fully functional via Runtime API
    - Constructor throws helpful TypeError with usage example
    - `instance.exports.mem.buffer` returns ArrayBuffer (wasm_memory_get_base_address)
    - `instance.exports.mem.grow()` works (wasm_runtime_enlarge_memory)
    - Tests: test/web/webassembly/test_web_wasm_exported_memory.js (2 tests passing)

- **Table API:** WAMR v2.4.1 C API does not support standalone Table objects
  - Created Table objects are non-functional
  - `wasm_table_size()` returns 0 for host-created tables
  - `wasm_table_grow()` explicitly blocked for host calls
  - **⚠️ Partial Resolution (2025-10-19):** Exported Table partially functional via Runtime API
    - Constructor throws helpful TypeError with usage example
    - `instance.exports.table.length` works (reads table_inst.cur_size)
    - ❌ get/set/grow not available (WAMR Runtime API lacks these functions)
    - Tests: test/web/webassembly/test_web_wasm_exported_table.js (1 test passing)

- **Global API:** WAMR v2.4.1 C API does not support standalone Global objects (discovered 2025-10-19)
  - Created Global objects are non-functional - **return garbage values**
  - `wasm_global_get()` returns uninitialized memory
  - Initial values passed to `wasm_global_new()` are not accessible
  - `wasm_global_set()` cannot reliably store values
  - **Impact:** Tasks 4.6-4.8 reopened as BLOCKED, WPT Global tests 0% pass rate
  - See: `docs/plan/webassembly-plan/wasm-phase4-global-blocker.md`
  - Resolution: Global objects may work when exported from WASM instances (not yet tested)

### Validation Limitations
- **WAMR validation:** WAMR is more permissive than spec requires
  - Some invalid modules may be accepted
  - Cannot be fixed without WAMR changes or custom validation layer

### WPT Test Status
- **Tests run:** 8 test files (wasm/jsapi category)
- **Pass rate:** 0% (expected - test infrastructure issues + implementation gaps)
- **Main blockers:**
  - WasmModuleBuilder helper not loading properly
  - Standalone Memory/Table/Global constructors blocked by WAMR limitations
- **Unit tests:** 100% pass rate (215/215 tests) - Updated 2025-10-19
  - Exported Memory tests: 2 passing (test_web_wasm_exported_memory.js)
  - Exported Table tests: 1 passing (test_web_wasm_exported_table.js)
  - Constructor error handling: 6 tests (Memory/Table/Global)
- **Note:** WPT tests require standalone constructors; exported objects tested via unit tests

## Environment

### WAMR Configuration
- Version: 2.4.1
- Mode: Interpreter (AOT disabled)
- Allocator: System allocator
- Bulk Memory: Enabled
- Reference Types: Enabled
- SIMD: Disabled
- GC: Disabled
- Threads: Disabled

### jsrt Version
- Version: 0.1.0
- Engine: QuickJS 2025-04-26
- Platform: Linux/macOS/Windows

## Implementation Phases

### Completed Phases
- ✅ **Phase 1:** Infrastructure & Error Types (100%)
- ✅ **Phase 2:** Core Module API - Partial (52% - standalone Memory blocked)
- ✅ **Phase 3:** Instance & Exports - Partial (42% - i32 functions + exported Memory/Table)
- ⚠️ **Phase 4:** Table & Global - Partial (3% - exported Table.length only)

### In Progress
- 🔵 **Phase 7:** WPT Integration & Testing (20% complete)
- 🔵 **Phase 8:** Documentation & Polish (40% complete)

### Planned
- **Phase 3.2B:** Full type support (f32/f64/i64/BigInt)
- **Phase 4:** Table & Global objects (requires WAMR resolution)
- **Phase 5:** Async compilation API
- **Phase 6:** Streaming API (optional)
- **Phase 8:** Documentation & Polish

## Practical Usage

### Working Examples

```javascript
// ✅ Module loading and validation
const bytes = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
const isValid = WebAssembly.validate(bytes);  // true
const module = new WebAssembly.Module(bytes);

// ✅ Module introspection
const exports = WebAssembly.Module.exports(module);  // []
const imports = WebAssembly.Module.imports(module);  // []

// ✅ Instance creation without imports
const instance = new WebAssembly.Instance(module);

// ✅ Function imports (i32 only)
const moduleWithImport = new WebAssembly.Module(wasmBytesWithImport);
const instance = new WebAssembly.Instance(moduleWithImport, {
  env: {
    log: (x) => console.log(x)  // i32 parameter
  }
});

// ✅ Function exports (i32 only)
const result = instance.exports.add(1, 2);  // Works for i32

// ✅ Error types
try {
  new WebAssembly.Module(invalidBytes);
} catch (e) {
  console.log(e instanceof WebAssembly.CompileError);  // true
}
```

### Exported Memory/Table Examples (Updated 2025-10-19)

```javascript
// ✅ Exported Memory - FULLY FUNCTIONAL
// WASM module with exported memory: (module (memory (export "mem") 1))
const wasmBytesWithMemory = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,  // WASM header
  0x05, 0x03, 0x01, 0x00, 0x01,                    // Memory section: 1 page
  0x07, 0x07, 0x01, 0x03, 0x6d, 0x65, 0x6d, 0x02, 0x00  // Export "mem"
]);

const module = new WebAssembly.Module(wasmBytesWithMemory);
const instance = new WebAssembly.Instance(module);

// Access exported memory
const mem = instance.exports.mem;
console.log(mem instanceof WebAssembly.Memory);  // true
console.log(mem.buffer.byteLength);  // >= 65536 (WAMR may allocate extra pages)

// Write to memory
const view = new Uint8Array(mem.buffer);
view[0] = 42;
console.log(view[0]);  // 42

// Grow memory
const oldSize = mem.grow(1);  // Grows by 1 page (64KB)
console.log(oldSize);  // Previous size in pages

// ⚠️ Exported Table - PARTIAL FUNCTIONALITY
// WASM module with exported table: (module (table (export "table") 10 funcref))
const wasmBytesWithTable = new Uint8Array([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,  // WASM header
  0x04, 0x04, 0x01, 0x70, 0x00, 0x0a,              // Table section: 10 funcref
  0x07, 0x09, 0x01, 0x05, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x01, 0x00  // Export "table"
]);

const tableModule = new WebAssembly.Module(wasmBytesWithTable);
const tableInstance = new WebAssembly.Instance(tableModule);

const table = tableInstance.exports.table;
console.log(table.length);  // 10 ✅ Works!
// table.get(0);  // ❌ Not supported for exported tables
// table.set(0, null);  // ❌ Not supported for exported tables
// table.grow(5);  // ❌ Not supported for exported tables
```

### Blocked/Limited Examples

```javascript
// 🔴 Standalone constructors - BLOCKED (throw helpful errors)
try {
  new WebAssembly.Memory({ initial: 1 });
} catch (e) {
  // TypeError: WebAssembly.Memory constructor not supported.
  // Use memories exported from WASM module instances instead.
  // Example: instance.exports.mem.buffer
}

try {
  new WebAssembly.Table({ element: 'funcref', initial: 1 });
} catch (e) {
  // TypeError: WebAssembly.Table constructor not supported.
  // Use tables exported from WASM module instances instead.
  // Example: instance.exports.table.get(0)
}

try {
  new WebAssembly.Global({ value: 'i32' }, 42);
} catch (e) {
  // TypeError: WebAssembly.Global constructor not supported.
  // Use globals exported from WASM module instances instead.
  // Example: instance.exports.myGlobal.value
}

// ✅ Async APIs - IMPLEMENTED (2025-10-18)
const module = await WebAssembly.compile(bytes);
const { module, instance } = await WebAssembly.instantiate(bytes, imports);
const instance2 = await WebAssembly.instantiate(module, imports);

// ⚠️ Streaming APIs - LIMITED (fallback implementation)
const module = await WebAssembly.compileStreaming(fetch('module.wasm'));
const { module, instance } = await WebAssembly.instantiateStreaming(fetch('module.wasm'));
```

## References
- [MDN WebAssembly](https://developer.mozilla.org/en-US/docs/WebAssembly)
- [WebAssembly JS API Spec](https://webassembly.github.io/spec/js-api/)
- [WAMR Documentation](https://github.com/bytecodealliance/wasm-micro-runtime)
- [Implementation Plan](/repo/docs/plan/webassembly-plan.md)
- [Phase 4 Table Blocker](/repo/docs/plan/webassembly-plan/wasm-phase4-table-blocker.md)
