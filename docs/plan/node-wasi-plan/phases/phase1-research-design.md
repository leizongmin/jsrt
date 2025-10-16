** TODO [#A] Phase 1: Research & Design [S][R:LOW][C:SIMPLE] :research:
:PROPERTIES:
:ID: phase-1
:CREATED: 2025-10-16T22:45:00Z
:DEPS: None
:PROGRESS: 0/8
:COMPLETION: 0%
:STATUS: 🟡 TODO
:END:

*** TODO [#A] Task 1.1: Analyze Node.js WASI API specification [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 1.1
:CREATED: 2025-10-16T22:45:00Z
:DEPS: None
:END:

**** Description
Study Node.js WASI API documentation (v24.x) to understand complete interface:
- WASI class constructor and options
- getImportObject() method behavior
- start() method requirements
- initialize() method requirements
- wasiImport property structure
- Error handling patterns

**** Acceptance Criteria
- [ ] Document all WASI class constructor options
- [ ] Document all public methods and their signatures
- [ ] Document return value types and error conditions
- [ ] Identify version-specific differences (preview1 vs unstable)
- [ ] Create design notes in docs/plan/node-wasi-plan/

**** Testing Strategy
Manual review of Node.js documentation and source code.

*** TODO [#A] Task 1.2: Study WAMR WASI capabilities [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 1.2
:CREATED: 2025-10-16T22:45:00Z
:DEPS: None
:END:

**** Description
Analyze WAMR (WebAssembly Micro Runtime) WASI support:
- Review deps/wamr/core/iwasm/libraries/libc-wasi/
- Review deps/wamr/core/iwasm/libraries/libc-uvwasi/
- Understand WAMR WASI import registration
- Check WAMR WASI preview1 compliance
- Identify missing features vs Node.js requirements

**** Acceptance Criteria
- [ ] Document WAMR WASI import functions available
- [ ] List supported WASI preview1 functions
- [ ] Identify gaps between WAMR and Node.js WASI
- [ ] Document WAMR API for registering native WASI imports

**** Testing Strategy
Code review and WAMR documentation analysis.

*** TODO [#A] Task 1.3: Review existing jsrt WebAssembly integration [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 1.3
:CREATED: 2025-10-16T22:45:00Z
:DEPS: None
:END:

**** Description
Analyze existing WASM support in jsrt:
- Review src/std/webassembly.c implementation
- Review src/wasm/runtime.c WAMR initialization
- Understand how WebAssembly.Module/Instance work
- Check how import objects are handled
- Identify reusable patterns

**** Acceptance Criteria
- [ ] Document current WASM module lifecycle
- [ ] Document import object handling mechanism
- [ ] Identify code patterns to reuse for WASI
- [ ] List integration points for WASI module

**** Testing Strategy
Code review and pattern analysis.

*** TODO [#B] Task 1.4: Study jsrt module registration patterns [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 1.4
:CREATED: 2025-10-16T22:45:00Z
:DEPS: None
:END:

**** Description
Understand how to register WASI as a builtin module:
- Review src/module/loaders/builtin_loader.c
- Study src/node/node_modules.c registration pattern
- Understand dual protocol support (node:/jsrt:)
- Check module caching behavior

**** Acceptance Criteria
- [ ] Document builtin module registration steps
- [ ] Understand CommonJS vs ESM module initialization
- [ ] Plan dual-protocol support (node:wasi + jsrt:wasi)
- [ ] Identify module cache integration points

**** Testing Strategy
Code review and existing module analysis.

*** TODO [#A] Task 1.5: Design WASI class architecture [S][R:MED][C:MEDIUM][D:1.1,1.2,1.3]
:PROPERTIES:
:ID: 1.5
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 1.1,1.2,1.3
:END:

**** Description
Design C data structures and architecture for WASI implementation:
- Define jsrt_wasi_t structure (opaque WASI instance)
- Design options structure matching WASIOptions
- Plan memory management strategy
- Design import object structure
- Plan lifecycle state machine (initialized, started)

**** Acceptance Criteria
- [ ] Create header file structure (src/node/wasi/wasi.h)
- [ ] Define all C data structures
- [ ] Document memory ownership rules
- [ ] Design state transition diagram
- [ ] Plan error handling strategy

**** Testing Strategy
Architecture review against Node.js WASI behavior.

*** TODO [#B] Task 1.6: Design file system preopen strategy [S][R:MED][C:MEDIUM][D:1.1,1.2]
:PROPERTIES:
:ID: 1.6
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 1.1,1.2
:END:

**** Description
Design how to handle preopens (sandboxed directory mapping):
- Understand WASI preopens concept
- Design mapping from JS object to WAMR structures
- Plan security validation (path traversal prevention)
- Design error handling for invalid paths

**** Acceptance Criteria
- [ ] Document preopen data structure design
- [ ] Plan security validation strategy
- [ ] Design path resolution algorithm
- [ ] Document memory management for preopen paths

**** Testing Strategy
Security review and path traversal testing plan.

*** TODO [#B] Task 1.7: Design environment variable handling [P][R:LOW][C:SIMPLE][D:1.1]
:PROPERTIES:
:ID: 1.7
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 1.1
:END:

**** Description
Design how to pass environment variables from JS to WASM:
- Convert JS env object to WAMR format
- Handle Unicode/encoding issues
- Plan memory management
- Design validation strategy

**** Acceptance Criteria
- [ ] Document env structure conversion algorithm
- [ ] Plan encoding strategy (UTF-8)
- [ ] Design memory management approach
- [ ] Document validation rules

**** Testing Strategy
Encoding and special character testing plan.

*** TODO [#B] Task 1.8: Design argument passing mechanism [P][R:LOW][C:SIMPLE][D:1.1]
:PROPERTIES:
:ID: 1.8
:CREATED: 2025-10-16T22:45:00Z
:DEPS: 1.1
:END:

**** Description
Design how to pass command-line arguments to WASM module:
- Convert JS args array to WAMR format
- Handle Unicode/encoding
- Plan memory management
- Design validation

**** Acceptance Criteria
- [ ] Document args structure conversion algorithm
- [ ] Plan encoding strategy
- [ ] Design memory management
- [ ] Document validation rules

**** Testing Strategy
Unicode argument testing plan.

