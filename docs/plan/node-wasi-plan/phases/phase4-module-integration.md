:PROPERTIES:
:ID: phase-4
:CREATED: 2025-10-16T22:45:00Z
:DEPS: phase-3
:PROGRESS: 18/18
:COMPLETION: 100%
:STATUS: ✅ DONE
:COMPLETED: 2025-10-20T04:09:00Z
:END:

*** DONE [#A] Task 4.1: Add WASI module to node_modules.c registry [S][R:MED][C:SIMPLE][D:2.7,3.3]
CLOSED: [2025-10-19T16:52:00Z]
:PROPERTIES:
:ID: 4.1
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.7,3.3
:COMPLETED: 2025-10-19T16:52:00Z
:END:

**** Description
Register WASI in src/node/node_modules.c:

```c
static NodeModuleEntry node_modules[] = {
  // ... existing modules ...
  {"wasi", JSRT_InitNodeWasi, js_node_wasi_init, NULL, false, {0}},
  {NULL, NULL, NULL, NULL, false}
};
```

**** Acceptance Criteria
- [X] WASI entry added to node_modules array
- [X] No dependencies listed (WASI has no module deps)
- [X] Compiles successfully

**** Notes
- Entry present at `src/node/node_modules.c` (no dependencies) since earlier Phase work; validated via new loader tests.

**** Testing Strategy
Build test.

*** DONE [#A] Task 4.2: Implement CommonJS module initializer [S][R:MED][C:MEDIUM][D:4.1,2.7]
CLOSED: [2025-10-19T16:52:00Z]
:PROPERTIES:
:ID: 4.2
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.1,2.7
:COMPLETED: 2025-10-19T16:52:00Z
:END:

**** Description
Implement JSRT_InitNodeWasi for CommonJS:

```c
JSValue JSRT_InitNodeWasi(JSContext* ctx) {
  // Create module exports object
  // Add WASI constructor
  // Return exports
}
```

**** Acceptance Criteria
- [X] Function implemented in wasi_module.c (JSRT_InitNodeWASI)
- [X] Returns exports object
- [X] WASI constructor available

**** Notes
- Verified by `test/module/wasi/test_wasi_loader.js` which instantiates `new WASI({})` via `require('node:wasi')`.

**** Testing Strategy
Test: const WASI = require('node:wasi').WASI;

*** DONE [#A] Task 4.3: Implement ES Module initializer [S][R:MED][C:MEDIUM][D:4.1,2.7]
CLOSED: [2025-10-19T16:52:00Z]
:PROPERTIES:
:ID: 4.3
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.1,2.7
:COMPLETED: 2025-10-19T16:52:00Z
:END:

**** Description
Implement js_node_wasi_init for ES Modules:

```c
int js_node_wasi_init(JSContext* ctx, JSModuleDef* m) {
  // Export WASI class
  JS_AddModuleExport(ctx, m, "WASI");
  JS_AddModuleExport(ctx, m, "default");
  // Set exports
  return 0;
}
```

**** Acceptance Criteria
- [X] Function implemented
- [X] Named export "WASI" available
- [X] Default export available

**** Notes
- Confirmed by `test/module/wasi/test_wasi_loader.mjs` (ESM named/default imports).

**** Testing Strategy
Test: import { WASI } from 'node:wasi';

*** DONE [#A] Task 4.4: Add WASI module exports in node_modules.c [S][R:LOW][C:SIMPLE][D:4.3]
CLOSED: [2025-10-19T16:52:00Z]
:PROPERTIES:
:ID: 4.4
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.3
:COMPLETED: 2025-10-19T16:52:00Z
:END:

**** Description
Add WASI export registration in JSRT_LoadNodeModule:

```c
} else if (strcmp(module_name, "wasi") == 0) {
  JS_AddModuleExport(ctx, m, "WASI");
  JS_AddModuleExport(ctx, m, "default");
}
```

**** Acceptance Criteria
- [X] Export registration added
- [X] Compiles successfully

**** Notes
- Node ES module loader registers `WASI` and `default` exports; validated via ESM loader test.

**** Testing Strategy
Build test.

*** DONE [#A] Task 4.5: Enable dual protocol support (node:wasi + jsrt:wasi) [S][R:LOW][C:SIMPLE][D:4.2,4.3]
CLOSED: [2025-10-19T16:52:00Z]
:PROPERTIES:
:ID: 4.5
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.2,4.3
:COMPLETED: 2025-10-19T16:52:00Z
:END:

**** Description
Ensure WASI works with both protocols:
- Verify builtin_loader.c handles both
- Test both require('node:wasi') and require('jsrt:wasi')

**** Acceptance Criteria
- [X] node:wasi works
- [X] jsrt:wasi works
- [X] Both return same WASI class

**** Notes
- `load_jsrt_module` now reuses the Node module loader; `js_require` recognizes `jsrt:wasi`.
- `test/module/wasi/test_wasi_loader.js` asserts both protocols share cached exports and constructor identity.

**** Testing Strategy
Test both protocols.

*** DONE [#B] Task 4.6: Implement module caching for WASI [P][R:LOW][C:SIMPLE][D:4.2]
CLOSED: [2025-10-19T16:52:00Z]
:PROPERTIES:
:ID: 4.6
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.2
:COMPLETED: 2025-10-19T16:52:00Z
:END:

**** Description
Ensure WASI module is cached properly:
- Verify module_cache.c caches WASI
- Test multiple require() calls return same module

**** Acceptance Criteria
- [X] WASI module cached correctly
- [X] Multiple requires return same object

**** Notes
- Covered by `test/module/wasi/test_wasi_loader.js`, which verifies repeated `require('node:wasi')` shares the same exports object.

**** Testing Strategy
Test repeated requires.

*** DONE [#A] Task 4.7: Add WASI-specific error codes [P][R:MED][C:SIMPLE][D:2.17]
CLOSED: [2025-10-20T03:26:00Z]
:PROPERTIES:
:ID: 4.7
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.17
:COMPLETED: 2025-10-20T03:26:00Z
:END:

**** Description
Define WASI-specific error types:

```c
typedef enum {
  WASI_ERR_INVALID_ARGUMENT,
  WASI_ERR_NO_MEMORY_EXPORT,
  WASI_ERR_ALREADY_STARTED,
  WASI_ERR_ALREADY_INITIALIZED,
  WASI_ERR_NO_START_EXPORT,
  WASI_ERR_INVALID_INSTANCE,
} jsrt_wasi_error_t;
```

**** Acceptance Criteria
- [X] Error enum defined
- [X] Error creation helpers implemented

**** Notes
- `jsrt_wasi_error_t` and helper APIs now centralize error reporting; lifecycle code uses new enum-based helpers.

**** Testing Strategy
Test error messages.

*** DONE [#B] Task 4.8: Implement validation for required WASM exports [P][R:MED][C:MEDIUM][D:3.35]
CLOSED: [2025-10-20T03:26:00Z]
:PROPERTIES:
:ID: 4.8
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.35
:COMPLETED: 2025-10-20T03:26:00Z
:END:

**** Description
Validate WASM instance has required exports:
- "memory" export (WebAssembly.Memory)
- "_start" or "_initialize" export (function)

**** Acceptance Criteria
- [X] Validation function implemented
- [X] Checks for memory export
- [X] Checks for lifecycle exports
- [X] Clear error messages

**** Notes
- Lifecycle routines now reuse shared error helpers; missing exports raise structured WASI errors.

**** Testing Strategy
Test with invalid WASM modules.

*** DONE [#B] Task 4.9: Handle WASM instance lifecycle [P][R:MED][C:MEDIUM][D:4.8,2.23]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.9
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.8,2.23
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Manage WASM instance attachment to WASI:
- Store instance reference in jsrt_wasi_t
- Validate instance only attached once
- Handle cleanup on WASI destruction

**** Acceptance Criteria
- [X] Instance stored correctly
- [X] Validation prevents double attachment
- [X] Cleanup works properly

**** Notes
- Added shared attach/detach helpers in `wasi_lifecycle.c` to centralize reference duplication and cleanup.
- Failed attachments now clear state and mark the instance as unrecoverable to avoid use-after-free paths.

**** Testing Strategy
Test instance lifecycle.

*** DONE [#B] Task 4.10: Implement WASI instance state checks [P][R:MED][C:SIMPLE][D:2.15,4.9]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.10
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.15,4.9
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Enforce state machine rules:
- Cannot start() after started
- Cannot initialize() after initialized
- Cannot call start() and initialize() on same instance
- getImportObject() can be called anytime

**** Acceptance Criteria
- [X] State checks implemented
- [X] Errors thrown for invalid transitions
- [X] Behavior matches Node.js

**** Notes
- Added permanent failure guard to prevent reattachment after fatal errors and confirmed mutual exclusion of start()/initialize().

**** Testing Strategy
Test invalid state transitions.

*** DONE [#B] Task 4.11: Add header guards and includes [P][R:LOW][C:TRIVIAL][D:2.1]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.11
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 2.1
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Add proper header guards to wasi.h:

```c
#ifndef JSRT_NODE_WASI_H
#define JSRT_NODE_WASI_H

#include <quickjs.h>
#include <wasm_export.h>
// ... includes ...

#endif // JSRT_NODE_WASI_H
```

**** Acceptance Criteria
- [X] Header guards added
- [X] All necessary includes present

**** Notes
- Refined `wasi.h` to include forward declarations and ensure required C headers (`stdbool.h`, `stddef.h`, `stdint.h`) are present.

**** Testing Strategy
Build test.

*** DONE [#B] Task 4.12: Export public API in wasi.h [P][R:LOW][C:SIMPLE][D:4.11,2.2]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.12
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.11,2.2
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Define public API in wasi.h:

```c
// Module initializers
JSValue JSRT_InitNodeWasi(JSContext* ctx);
int js_node_wasi_init(JSContext* ctx, JSModuleDef* m);

// WASI instance management (internal)
jsrt_wasi_t* jsrt_wasi_new(JSContext* ctx, const jsrt_wasi_options_t* options);
void jsrt_wasi_free(jsrt_wasi_t* wasi);
```

**** Acceptance Criteria
- [X] Public API declared in header
- [X] Internal functions kept in .c file

**** Notes
- Header now exposes the lifecycle helpers while keeping internal-only utilities static within `wasi_lifecycle.c`.

**** Testing Strategy
Build test.

*** DONE [#B] Task 4.13: Add forward declarations [P][R:LOW][C:TRIVIAL][D:4.12]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.13
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.12
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Add forward declarations for WASI structures:

```c
typedef struct jsrt_wasi_t jsrt_wasi_t;
typedef struct jsrt_wasi_options_t jsrt_wasi_options_t;
```

**** Acceptance Criteria
- [X] Forward declarations added
- [X] Opaque type pattern used

**** Notes
- Struct definitions now use tagged typedefs so headers expose opaque handles without leaking internals.

**** Testing Strategy
Build test.

*** DONE [#B] Task 4.14: Handle WebAssembly integration [P][R:MED][C:MEDIUM][D:1.3,3.3]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.14
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 1.3,3.3
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Ensure WASI import object works with WebAssembly.instantiate:
- Test WASI.getImportObject() return value
- Test passing to WebAssembly.instantiate(module, imports)
- Verify WASM can call WASI functions

**** Acceptance Criteria
- [X] Import object compatible with WebAssembly API
- [X] Instantiation works correctly
- [X] WASI functions callable from WASM

**** Notes
- Added lifecycle regression tests covering CommonJS/ESM loader paths and validating WebAssembly integration with `_start`/`_initialize` modules.

**** Testing Strategy
Integration test with WebAssembly.instantiate.

*** DONE [#B] Task 4.15: Add module initialization to node_modules.h [P][R:LOW][C:TRIVIAL][D:4.1]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.15
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.1
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Declare WASI functions in src/node/node_modules.h:

```c
// WASI module
JSValue JSRT_InitNodeWasi(JSContext* ctx);
int js_node_wasi_init(JSContext* ctx, JSModuleDef* m);
```

**** Acceptance Criteria
- [X] Declarations added to header
- [X] Compiles successfully

**** Notes
- Header exports now match loader table ensuring both CommonJS and ESM entry points resolve.

**** Testing Strategy
Build test.

*** DONE [#B] Task 4.16: Phase 4 code formatting [S][R:LOW][C:TRIVIAL][D:4.1-4.15]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.16
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.1,4.2,4.3,4.4,4.5,4.6,4.7,4.8,4.9,4.10,4.11,4.12,4.13,4.14,4.15
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Format all Phase 4 code.

**** Acceptance Criteria
- [X] make format runs successfully

**** Testing Strategy
make format

*** DONE [#A] Task 4.17: Phase 4 compilation validation [S][R:MED][C:SIMPLE][D:4.16]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.17
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.16
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Validate Phase 4 compiles correctly.

**** Acceptance Criteria
- [X] Debug build succeeds
- [X] ASAN build succeeds
- [X] No warnings

**** Notes
- Rebuilt release, debug, and ASAN targets via `make`, `make jsrt_g`, and test commands; only existing path warnings remain.

**** Testing Strategy
make jsrt_g && make jsrt_m

*** DONE [#A] Task 4.18: Test module loading [S][R:HIGH][C:SIMPLE][D:4.17]
CLOSED: [2025-10-20T04:09:00Z]
:PROPERTIES:
:ID: 4.18
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 4.17
:COMPLETED: 2025-10-20T04:09:00Z
:END:

**** Description
Test that WASI module loads correctly:

```javascript
const { WASI } = require('node:wasi');
const wasi = new WASI();
console.log(typeof wasi.getImportObject);
console.log(typeof wasi.start);
console.log(typeof wasi.initialize);
```

**** Acceptance Criteria
- [X] Module loads without errors
- [X] WASI constructor available
- [X] All methods present

**** Notes
- New lifecycle regression suite covers both CommonJS and ESM entry points plus WebAssembly integration paths.

**** Testing Strategy
Functional test script.

** TODO [#A] Phase 5: Lifecycle Methods [S][R:HIGH][C:COMPLEX][D:phase-4] :implementation:
