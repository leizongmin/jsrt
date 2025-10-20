:PROPERTIES:
:ID: phase-5
:CREATED: 2025-10-16T22:45:00Z
:DEPS: phase-4
:PROGRESS: 15/15
:COMPLETION: 100%
:STATUS: ✅ DONE
:COMPLETED: 2025-10-20T04:50:00Z
:END:

*** DONE [#A] Task 5.1: Implement start() method [S][R:HIGH][C:COMPLEX][D:3.3,4.9]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.1
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.3,4.9
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Implement WASI.start(instance) method:

```c
static JSValue js_wasi_start(JSContext* ctx, JSValueConst this_val,
                              int argc, JSValueConst* argv) {
  // Validate WASI instance
  // Validate WASM instance argument
  // Check not already started
  // Validate instance has _start export
  // Validate instance has memory export
  // Call _start()
  // Handle proc_exit and return code
  // Mark as started
}
```

**** Acceptance Criteria
- [X] Method implemented
- [X] Validates all requirements
- [X] Calls _start() export
- [X] Returns exit code
- [X] Handles proc_exit correctly
- [X] State updated correctly

**** Notes
- Start() now uses shared attach helpers, enforces memory validation, and returns explicit exit codes.

**** Testing Strategy
Test with WASM command modules.

*** DONE [#A] Task 5.2: Implement initialize() method [S][R:HIGH][C:COMPLEX][D:3.3,4.9]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.2
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.3,4.9
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Implement WASI.initialize(instance) method:

```c
static JSValue js_wasi_initialize(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv) {
  // Validate WASI instance
  // Validate WASM instance argument
  // Check not already initialized
  // Validate instance has _initialize export (if present)
  // Validate instance does NOT have _start export
  // Validate instance has memory export
  // Call _initialize() if present
  // Mark as initialized
}
```

**** Acceptance Criteria
- [X] Method implemented
- [X] Validates all requirements
- [X] Calls _initialize() if present
- [X] Returns undefined
- [X] State updated correctly

**** Notes
- initialize() enforces _start absence, treats missing _initialize as noop, and converts optional exits.

**** Testing Strategy
Test with WASM reactor modules.

*** DONE [#A] Task 5.3: Extract WASM instance from JS object [S][R:MED][C:MEDIUM][D:5.1,5.2,1.3]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.3
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1,5.2,1.3
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Implement helper to extract wasm_module_inst_t from WebAssembly.Instance:

```c
wasm_module_inst_t jsrt_wasi_get_wasm_instance(JSContext* ctx,
                                                 JSValue instance_obj);
```

Integrate with existing WebAssembly implementation.

**** Acceptance Criteria
- [X] Helper implemented
- [X] Extracts WAMR instance correctly
- [X] Validates instance type
- [X] Error handling for invalid objects

**** Notes
- Attachment relies on jsrt_webassembly_get_instance to capture WAMR handles safely.

**** Testing Strategy
Test with valid/invalid instances.

*** DONE [#A] Task 5.4: Validate _start export [S][R:MED][C:SIMPLE][D:5.1]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.4
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Implement validation that WASM instance has _start export:

```c
bool jsrt_wasi_has_start_export(wasm_module_inst_t instance);
```

Use WAMR API to check for export.

**** Acceptance Criteria
- [X] Function implemented
- [X] Checks for _start export
- [X] Returns correct boolean

**** Notes
- _start export is required via jsrt_wasi_require_export_function, matching Node semantics.

**** Testing Strategy
Test with modules with/without _start.

*** DONE [#A] Task 5.5: Validate _initialize export [S][R:MED][C:SIMPLE][D:5.2]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.5
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.2
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Implement validation for _initialize export:

```c
bool jsrt_wasi_has_initialize_export(wasm_module_inst_t instance);
```

**** Acceptance Criteria
- [X] Function implemented
- [X] Checks for _initialize export
- [X] Returns correct boolean

**** Notes
- _initialize export is optional and validated for function shape before invocation.

**** Testing Strategy
Test with reactor modules.

*** DONE [#A] Task 5.6: Validate memory export [S][R:MED][C:SIMPLE][D:5.1,5.2,3.35]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.6
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1,5.2,3.35
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Implement validation for memory export:

```c
bool jsrt_wasi_has_memory_export(wasm_module_inst_t instance);
wasm_memory_t jsrt_wasi_get_memory(wasm_module_inst_t instance);
```

**** Acceptance Criteria
- [X] Function implemented
- [X] Checks for memory export
- [X] Extracts memory object

**** Notes
- Attach helper now checks default memory exports, blocking modules without linear memory.

**** Testing Strategy
Test with modules with/without memory.

*** DONE [#A] Task 5.7: Call WASM _start function [S][R:HIGH][C:MEDIUM][D:5.4,5.1]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.7
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.4,5.1
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Implement calling _start() export:

```c
int jsrt_wasi_call_start(jsrt_wasi_t* wasi, wasm_module_inst_t instance);
```

Use WAMR API to invoke function.

**** Acceptance Criteria
- [X] Function implemented
- [X] Calls _start correctly
- [X] Returns exit code
- [X] Handles exceptions

**** Notes
- start() builds/destroys exec env per call and captures exit codes with proc_exit integration.

**** Testing Strategy
Test with WASM command modules.

*** DONE [#A] Task 5.8: Call WASM _initialize function [S][R:HIGH][C:MEDIUM][D:5.5,5.2]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.8
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.5,5.2
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Implement calling _initialize() export:

```c
int jsrt_wasi_call_initialize(jsrt_wasi_t* wasi, wasm_module_inst_t instance);
```

**** Acceptance Criteria
- [X] Function implemented
- [X] Calls _initialize if present
- [X] Handles missing _initialize gracefully
- [X] Handles exceptions

**** Notes
- initialize() creates an exec env only when _initialize exists and mirrors Node error handling.

**** Testing Strategy
Test with WASM reactor modules.

*** DONE [#B] Task 5.9: Handle proc_exit in start() [S][R:MED][C:MEDIUM][D:5.7,3.20]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.9
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.7,3.20
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Handle proc_exit called during _start():
- If returnOnExit=true: return exit code
- If returnOnExit=false: process should exit
- Coordinate with proc_exit implementation

**** Acceptance Criteria
- [X] Exit code captured correctly
- [X] returnOnExit option respected
- [X] Behavior matches Node.js

**** Notes
- proc_exit handling records exit codes and returns them when returnOnExit is enabled.

**** Testing Strategy
Test both returnOnExit modes.

*** DONE [#B] Task 5.10: Enforce start/initialize mutual exclusion [S][R:MED][C:SIMPLE][D:5.1,5.2]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.10
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1,5.2
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Enforce rules:
- Cannot call start() if instance has _initialize export
- Cannot call initialize() if instance has _start export
- Cannot call start() after initialize()
- Cannot call initialize() after start()

**** Acceptance Criteria
- [X] Validation implemented
- [X] Throws errors for violations
- [X] Behavior matches Node.js

**** Notes
- Lifecycle guards prevent mixing start()/initialize() and reject conflicting exports.

**** Testing Strategy
Test invalid combinations.

*** DONE [#B] Task 5.11: Handle exceptions from WASM functions [P][R:MED][C:MEDIUM][D:5.7,5.8]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.11
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.7,5.8
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Handle WASM exceptions during _start/_initialize:
- Catch WAMR exceptions
- Convert to JS exceptions
- Include useful error messages

**** Acceptance Criteria
- [X] Exception handling implemented
- [X] JS exceptions thrown correctly
- [X] Error messages informative

**** Notes
- All failure paths detach instances and propagate JS exceptions without leaking exec envs.

**** Testing Strategy
Test with WASM that throws.

*** DONE [#B] Task 5.12: Prevent multiple start() calls [P][R:MED][C:SIMPLE][D:5.1]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.12
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Ensure start() can only be called once:
- Check started flag
- Throw error if already started
- Behavior matches Node.js

**** Acceptance Criteria
- [X] Validation implemented
- [X] Error thrown on second call
- [X] Error message matches Node.js

**** Notes
- Repeated start() attempts throw ERR_ALREADY_STARTED equivalents using state flags.

**** Testing Strategy
Test calling start() twice.

*** DONE [#B] Task 5.13: Prevent multiple initialize() calls [P][R:MED][C:SIMPLE][D:5.2]
CLOSED: [2025-10-20T04:50:00Z]
:PROPERTIES:
:ID: 5.13
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.2
:COMPLETED: 2025-10-20T04:50:00Z
:END:

**** Description
Ensure initialize() can only be called once:
- Check initialized flag
- Throw error if already initialized
- Behavior matches Node.js

**** Acceptance Criteria
- [X] Validation implemented
- [X] Error thrown on second call
- [X] Error message matches Node.js

**** Notes
- Repeated initialize() attempts trigger ERR_ALREADY_INITIALIZED equivalents via state tracking.

**** Testing Strategy
Test calling initialize() twice.

*** DONE [#B] Task 5.14: Phase 5 code formatting [S][R:LOW][C:TRIVIAL][D:5.1-5.13]
CLOSED: [2025-10-20T04:55:00Z]
:PROPERTIES:
:ID: 5.14
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1,5.2,5.3,5.4,5.5,5.6,5.7,5.8,5.9,5.10,5.11,5.12,5.13
:COMPLETED: 2025-10-20T04:55:00Z
:END:

**** Description
Format all Phase 5 code.

**** Acceptance Criteria
- [X] make format runs successfully

**** Notes
- Executed make format after lifecycle updates to keep C/JS sources consistent.

**** Testing Strategy
make format

*** DONE [#A] Task 5.15: Phase 5 integration test [S][R:HIGH][C:MEDIUM][D:5.14]
CLOSED: [2025-10-20T04:55:00Z]
:PROPERTIES:
:ID: 5.15
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.14
:COMPLETED: 2025-10-20T04:55:00Z
:END:

**** Description
Create end-to-end integration test:
- Create WASM module with _start
- Instantiate with WASI
- Call wasi.start(instance)
- Verify output
- Test returnOnExit modes

**** Acceptance Criteria
- [X] Integration test passes
- [X] WASM executes correctly
- [X] Output captured
- [X] Exit codes correct

**** Notes
- Expanded test/module/wasi/test_wasi_lifecycle.js to cover Node-parity lifecycle flows.

**** Testing Strategy
End-to-end functional test.

** TODO [#A] Phase 6: Testing & Validation [S][R:HIGH][C:COMPLEX][D:phase-5] :testing:
