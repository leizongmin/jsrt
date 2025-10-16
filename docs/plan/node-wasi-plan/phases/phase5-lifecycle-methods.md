:PROPERTIES:
:ID: phase-5
:CREATED: 2025-10-16T22:45:00Z
:DEPS: phase-4
:PROGRESS: 0/15
:COMPLETION: 0%
:STATUS: 🟡 TODO
:END:

*** TODO [#A] Task 5.1: Implement start() method [S][R:HIGH][C:COMPLEX][D:3.3,4.9]
:PROPERTIES:
:ID: 5.1
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.3,4.9
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
- [ ] Method implemented
- [ ] Validates all requirements
- [ ] Calls _start() export
- [ ] Returns exit code
- [ ] Handles proc_exit correctly
- [ ] State updated correctly

**** Testing Strategy
Test with WASM command modules.

*** TODO [#A] Task 5.2: Implement initialize() method [S][R:HIGH][C:COMPLEX][D:3.3,4.9]
:PROPERTIES:
:ID: 5.2
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 3.3,4.9
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
- [ ] Method implemented
- [ ] Validates all requirements
- [ ] Calls _initialize() if present
- [ ] Returns undefined
- [ ] State updated correctly

**** Testing Strategy
Test with WASM reactor modules.

*** TODO [#A] Task 5.3: Extract WASM instance from JS object [S][R:MED][C:MEDIUM][D:5.1,5.2,1.3]
:PROPERTIES:
:ID: 5.3
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1,5.2,1.3
:END:

**** Description
Implement helper to extract wasm_module_inst_t from WebAssembly.Instance:

```c
wasm_module_inst_t jsrt_wasi_get_wasm_instance(JSContext* ctx,
                                                 JSValue instance_obj);
```

Integrate with existing WebAssembly implementation.

**** Acceptance Criteria
- [ ] Helper implemented
- [ ] Extracts WAMR instance correctly
- [ ] Validates instance type
- [ ] Error handling for invalid objects

**** Testing Strategy
Test with valid/invalid instances.

*** TODO [#A] Task 5.4: Validate _start export [S][R:MED][C:SIMPLE][D:5.1]
:PROPERTIES:
:ID: 5.4
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1
:END:

**** Description
Implement validation that WASM instance has _start export:

```c
bool jsrt_wasi_has_start_export(wasm_module_inst_t instance);
```

Use WAMR API to check for export.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Checks for _start export
- [ ] Returns correct boolean

**** Testing Strategy
Test with modules with/without _start.

*** TODO [#A] Task 5.5: Validate _initialize export [S][R:MED][C:SIMPLE][D:5.2]
:PROPERTIES:
:ID: 5.5
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.2
:END:

**** Description
Implement validation for _initialize export:

```c
bool jsrt_wasi_has_initialize_export(wasm_module_inst_t instance);
```

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Checks for _initialize export
- [ ] Returns correct boolean

**** Testing Strategy
Test with reactor modules.

*** TODO [#A] Task 5.6: Validate memory export [S][R:MED][C:SIMPLE][D:5.1,5.2,3.35]
:PROPERTIES:
:ID: 5.6
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1,5.2,3.35
:END:

**** Description
Implement validation for memory export:

```c
bool jsrt_wasi_has_memory_export(wasm_module_inst_t instance);
wasm_memory_t jsrt_wasi_get_memory(wasm_module_inst_t instance);
```

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Checks for memory export
- [ ] Extracts memory object

**** Testing Strategy
Test with modules with/without memory.

*** TODO [#A] Task 5.7: Call WASM _start function [S][R:HIGH][C:MEDIUM][D:5.4,5.1]
:PROPERTIES:
:ID: 5.7
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.4,5.1
:END:

**** Description
Implement calling _start() export:

```c
int jsrt_wasi_call_start(jsrt_wasi_t* wasi, wasm_module_inst_t instance);
```

Use WAMR API to invoke function.

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Calls _start correctly
- [ ] Returns exit code
- [ ] Handles exceptions

**** Testing Strategy
Test with WASM command modules.

*** TODO [#A] Task 5.8: Call WASM _initialize function [S][R:HIGH][C:MEDIUM][D:5.5,5.2]
:PROPERTIES:
:ID: 5.8
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.5,5.2
:END:

**** Description
Implement calling _initialize() export:

```c
int jsrt_wasi_call_initialize(jsrt_wasi_t* wasi, wasm_module_inst_t instance);
```

**** Acceptance Criteria
- [ ] Function implemented
- [ ] Calls _initialize if present
- [ ] Handles missing _initialize gracefully
- [ ] Handles exceptions

**** Testing Strategy
Test with WASM reactor modules.

*** TODO [#B] Task 5.9: Handle proc_exit in start() [S][R:MED][C:MEDIUM][D:5.7,3.20]
:PROPERTIES:
:ID: 5.9
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.7,3.20
:END:

**** Description
Handle proc_exit called during _start():
- If returnOnExit=true: return exit code
- If returnOnExit=false: process should exit
- Coordinate with proc_exit implementation

**** Acceptance Criteria
- [ ] Exit code captured correctly
- [ ] returnOnExit option respected
- [ ] Behavior matches Node.js

**** Testing Strategy
Test both returnOnExit modes.

*** TODO [#B] Task 5.10: Enforce start/initialize mutual exclusion [S][R:MED][C:SIMPLE][D:5.1,5.2]
:PROPERTIES:
:ID: 5.10
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1,5.2
:END:

**** Description
Enforce rules:
- Cannot call start() if instance has _initialize export
- Cannot call initialize() if instance has _start export
- Cannot call start() after initialize()
- Cannot call initialize() after start()

**** Acceptance Criteria
- [ ] Validation implemented
- [ ] Throws errors for violations
- [ ] Behavior matches Node.js

**** Testing Strategy
Test invalid combinations.

*** TODO [#B] Task 5.11: Handle exceptions from WASM functions [P][R:MED][C:MEDIUM][D:5.7,5.8]
:PROPERTIES:
:ID: 5.11
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.7,5.8
:END:

**** Description
Handle WASM exceptions during _start/_initialize:
- Catch WAMR exceptions
- Convert to JS exceptions
- Include useful error messages

**** Acceptance Criteria
- [ ] Exception handling implemented
- [ ] JS exceptions thrown correctly
- [ ] Error messages informative

**** Testing Strategy
Test with WASM that throws.

*** TODO [#B] Task 5.12: Prevent multiple start() calls [P][R:MED][C:SIMPLE][D:5.1]
:PROPERTIES:
:ID: 5.12
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1
:END:

**** Description
Ensure start() can only be called once:
- Check started flag
- Throw error if already started
- Behavior matches Node.js

**** Acceptance Criteria
- [ ] Validation implemented
- [ ] Error thrown on second call
- [ ] Error message matches Node.js

**** Testing Strategy
Test calling start() twice.

*** TODO [#B] Task 5.13: Prevent multiple initialize() calls [P][R:MED][C:SIMPLE][D:5.2]
:PROPERTIES:
:ID: 5.13
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.2
:END:

**** Description
Ensure initialize() can only be called once:
- Check initialized flag
- Throw error if already initialized
- Behavior matches Node.js

**** Acceptance Criteria
- [ ] Validation implemented
- [ ] Error thrown on second call
- [ ] Error message matches Node.js

**** Testing Strategy
Test calling initialize() twice.

*** TODO [#B] Task 5.14: Phase 5 code formatting [S][R:LOW][C:TRIVIAL][D:5.1-5.13]
:PROPERTIES:
:ID: 5.14
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.1,5.2,5.3,5.4,5.5,5.6,5.7,5.8,5.9,5.10,5.11,5.12,5.13
:END:

**** Description
Format all Phase 5 code.

**** Acceptance Criteria
- [ ] make format runs successfully

**** Testing Strategy
make format

*** TODO [#A] Task 5.15: Phase 5 integration test [S][R:HIGH][C:MEDIUM][D:5.14]
:PROPERTIES:
:ID: 5.15
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 5.14
:END:

**** Description
Create end-to-end integration test:
- Create WASM module with _start
- Instantiate with WASI
- Call wasi.start(instance)
- Verify output
- Test returnOnExit modes

**** Acceptance Criteria
- [ ] Integration test passes
- [ ] WASM executes correctly
- [ ] Output captured
- [ ] Exit codes correct

**** Testing Strategy
End-to-end functional test.

** TODO [#A] Phase 6: Testing & Validation [S][R:HIGH][C:COMPLEX][D:phase-5] :testing:
