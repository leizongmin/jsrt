** DONE [#A] Phase 1: Research & Design [S][R:LOW][C:SIMPLE] :research:
CLOSED: [2025-10-19T08:00:00Z]
:PROPERTIES:
:ID: phase-1
:CREATED: 2025-10-16T22:45:00Z
:COMPLETED: 2025-10-19T08:00:00Z
:DEPS: None
:PROGRESS: 8/8
:COMPLETION: 100%
:STATUS: ✅ COMPLETED
:END:

*** DONE [#A] Task 1.1: Analyze Node.js WASI API specification [P][R:LOW][C:SIMPLE]
CLOSED: [2025-10-19T08:00:00Z]
:PROPERTIES:
:ID: 1.1
:CREATED: 2025-10-16T22:45:00Z
:COMPLETED: 2025-10-19T08:00:00Z
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
- [X] Document all WASI class constructor options
- [X] Document all public methods and their signatures
- [X] Document return value types and error conditions
- [X] Identify version-specific differences (preview1 vs unstable)
- [X] Create design notes in docs/plan/node-wasi-plan/

**** Testing Strategy
Manual review of Node.js documentation and source code.

*** DONE [#A] Task 1.2: Study WAMR WASI capabilities [P][R:LOW][C:SIMPLE]
CLOSED: [2025-10-19T08:00:00Z]
:PROPERTIES:
:ID: 1.2
:CREATED: 2025-10-16T22:45:00Z
:COMPLETED: 2025-10-19T08:00:00Z
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
- [X] Document WAMR WASI import functions available
- [X] List supported WASI preview1 functions
- [X] Identify gaps between WAMR and Node.js WASI
- [X] Document WAMR API for registering native WASI imports

**** Testing Strategy
Code review and WAMR documentation analysis.

*** DONE [#A] Task 1.3: Review existing jsrt WebAssembly integration [P][R:LOW][C:SIMPLE]
CLOSED: [2025-10-19T08:00:00Z]
:PROPERTIES:
:ID: 1.3
:CREATED: 2025-10-16T22:45:00Z
:COMPLETED: 2025-10-19T08:00:00Z
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
- [X] Document current WASM module lifecycle
- [X] Document import object handling mechanism
- [X] Identify code patterns to reuse for WASI
- [X] List integration points for WASI module

**** Testing Strategy
Code review and pattern analysis.

*** DONE [#B] Task 1.4: Study jsrt module registration patterns [P][R:LOW][C:SIMPLE]
CLOSED: [2025-10-19T08:00:00Z]
:PROPERTIES:
:ID: 1.4
:CREATED: 2025-10-16T22:45:00Z
:COMPLETED: 2025-10-19T08:00:00Z
:DEPS: None
:END:

**** Description
Understand how to register WASI as a builtin module:
- Review src/module/loaders/builtin_loader.c
- Study src/node/node_modules.c registration pattern
- Understand dual protocol support (node:/jsrt:)
- Check module caching behavior

**** Acceptance Criteria
- [X] Document builtin module registration steps
- [X] Understand CommonJS vs ESM module initialization
- [X] Plan dual-protocol support (node:wasi + jsrt:wasi)
- [X] Identify module cache integration points

**** Testing Strategy
Code review and existing module analysis.

*** DONE [#A] Task 1.5: Design WASI class architecture [S][R:MED][C:MEDIUM][D:1.1,1.2,1.3]
CLOSED: [2025-10-19T08:00:00Z]
:PROPERTIES:
:ID: 1.5
:CREATED: 2025-10-16T22:45:00Z
:COMPLETED: 2025-10-19T08:00:00Z
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
- [X] Create header file structure (src/node/wasi/wasi.h)
- [X] Define all C data structures
- [X] Document memory ownership rules
- [X] Design state transition diagram
- [X] Plan error handling strategy

**** Testing Strategy
Architecture review against Node.js WASI behavior.

*** DONE [#B] Task 1.6: Design file system preopen strategy [S][R:MED][C:MEDIUM][D:1.1,1.2]
CLOSED: [2025-10-19T08:00:00Z]
:PROPERTIES:
:ID: 1.6
:CREATED: 2025-10-16T22:45:00Z
:COMPLETED: 2025-10-19T08:00:00Z
:DEPS: 1.1,1.2
:END:

**** Description
Design how to handle preopens (sandboxed directory mapping):
- Understand WASI preopens concept
- Design mapping from JS object to WAMR structures
- Plan security validation (path traversal prevention)
- Design error handling for invalid paths

**** Acceptance Criteria
- [X] Document preopen data structure design
- [X] Plan security validation strategy
- [X] Design path resolution algorithm
- [X] Document memory management for preopen paths

**** Testing Strategy
Security review and path traversal testing plan.

*** DONE [#B] Task 1.7: Design environment variable handling [P][R:LOW][C:SIMPLE][D:1.1]
CLOSED: [2025-10-19T08:00:00Z]
:PROPERTIES:
:ID: 1.7
:CREATED: 2025-10-16T22:45:00Z
:COMPLETED: 2025-10-19T08:00:00Z
:DEPS: 1.1
:END:

**** Description
Design how to pass environment variables from JS to WASM:
- Convert JS env object to WAMR format
- Handle Unicode/encoding issues
- Plan memory management
- Design validation strategy

**** Acceptance Criteria
- [X] Document env structure conversion algorithm
- [X] Plan encoding strategy (UTF-8)
- [X] Design memory management approach
- [X] Document validation rules

**** Testing Strategy
Encoding and special character testing plan.

*** DONE [#B] Task 1.8: Design argument passing mechanism [P][R:LOW][C:SIMPLE][D:1.1]
CLOSED: [2025-10-19T08:00:00Z]
:PROPERTIES:
:ID: 1.8
:CREATED: 2025-10-16T22:45:00Z
:COMPLETED: 2025-10-19T08:00:00Z
:DEPS: 1.1
:END:

**** Description
Design how to pass command-line arguments to WASM module:
- Convert JS args array to WAMR format
- Handle Unicode/encoding
- Plan memory management
- Design validation

**** Acceptance Criteria
- [X] Document args structure conversion algorithm
- [X] Plan encoding strategy
- [X] Design memory management
- [X] Document validation rules

**** Testing Strategy
Unicode argument testing plan.

