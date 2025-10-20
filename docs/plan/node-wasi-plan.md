#+TITLE: Task Plan: Node.js WASI Module Implementation
#+AUTHOR: Claude AI Assistant
#+DATE: 2025-10-16T22:45:00Z
#+STARTUP: overview indent
#+TODO: TODO IN-PROGRESS BLOCKED | DONE CANCELLED
#+PRIORITIES: A C B

* Task Metadata
:PROPERTIES:
:CREATED: 2025-10-16T22:45:00Z
:UPDATED: 2025-10-20T15:16:44Z
:STATUS: 🔵 IN_PROGRESS
:PROGRESS: 113/141
:COMPLETION: 80%
:WASM_DEPENDENCIES: ✅ VERIFIED - All required APIs functional (2025-10-19)
:WASM_BLOCKERS: NONE - Standalone Memory/Table/Global not needed by WASI
:PHASE3_SYSCALLS: 🟢 COMPLETE - 13/13 core syscalls implemented with path/FD extensions validated (2025-10-19)
:PHASE3_STATUS: ✅ COMPLETED (2025-10-19) - 38/38 tasks complete; optional advanced FD/path features scheduled for later phases
:PHASE3_CAPABILITIES: args, env, stdio, preopens, path fs, time, random, proc_exit, ENOSYS stubs for poll/socket
:ASAN_VALIDATION: ✅ CLEAN - No leaks or memory errors
:END:

* STATUS UPDATE GUIDELINES

See [[file:node-wasi-plan/status-update-guidelines.md][Status Update Guidelines]] for complete three-level tracking system and update discipline.

* 📋 Task Analysis & Breakdown

** L0 Main Task
*** Requirement
Implement a full Node.js-compatible WASI (WebAssembly System Interface) module for jsrt runtime.

*** Success Criteria
1. Full API compatibility with Node.js WASI module (v24.x)
2. Both "node:wasi" and "jsrt:wasi" module aliases work
3. All WASI preview1 imports properly exposed
4. Memory safe (ASAN validation passes)
5. All tests pass (unit + integration)
6. Cross-platform support (Linux, macOS, Windows)
7. Complete documentation and examples

*** Constraints
- Must integrate with existing jsrt module loader system
- Must reuse existing WAMR infrastructure (src/wasm/)
- Minimal resource footprint (jsrt design principle)
- No external dependencies beyond WAMR
- Follow existing patterns from src/node/ modules

** L1 Epic Phases

This implementation is broken down into 7 major phases:

*** Phase 1: Research & Design (8 tasks)
Foundation phase - understand Node.js WASI API, WAMR capabilities, and design architecture.

See [[file:node-wasi-plan/phases/phase1-research-design.md][Phase 1: Research & Design]] for detailed tasks.

*** Phase 2: Core Infrastructure (25 tasks)
Build the foundational WASI class, data structures, and WAMR integration layer.

See [[file:node-wasi-plan/phases/phase2-core-infrastructure.md][Phase 2: Core Infrastructure]] for detailed tasks.

*** Phase 3: WASI Import Implementation (38 tasks)
Implement all WASI preview1 system calls and the getImportObject() method.

See [[file:node-wasi-plan/phases/phase3-wasi-imports.md][Phase 3: WASI Import Implementation]] for detailed tasks.

*** Phase 4: Module Integration (18 tasks)
Register as builtin module, integrate with module loader, add error handling.

See [[file:node-wasi-plan/phases/phase4-module-integration.md][Phase 4: Module Integration]] for detailed tasks.

*** Phase 5: Lifecycle Methods (15 tasks)
Implement initialize() and start() methods with proper validation and error handling.

See [[file:node-wasi-plan/phases/phase5-lifecycle-methods.md][Phase 5: Lifecycle Methods]] for detailed tasks.

*** Phase 6: Testing & Validation (27 tasks)
Comprehensive testing including unit tests, integration tests, ASAN validation, cross-platform testing.

See [[file:node-wasi-plan/phases/phase6-testing-validation.md][Phase 6: Testing & Validation]] for detailed tasks.

*** Phase 7: Documentation (10 tasks)
API documentation, usage examples, migration guide from Node.js.

See [[file:node-wasi-plan/phases/phase7-documentation.md][Phase 7: Documentation]] for detailed tasks.

** Sub-document References

- [[file:node-wasi-plan/status-update-guidelines.md][Status Update Guidelines]] - Three-level tracking system
- [[file:node-wasi-plan/design-decisions.md][Design Decisions]] - Architecture and technology choices
- [[file:node-wasi-plan/dependencies.md][Dependencies]] - External and internal dependencies
- [[file:node-wasi-plan/testing-strategy.md][Testing Strategy]] - Comprehensive testing approach
- [[file:node-wasi-plan/completion-criteria.md][Completion Criteria]] - Definition of done
- [[file:node-wasi-plan/phases/][phases/]] - Detailed task breakdowns for each phase

* 📝 Task Execution Summary

** Phase Overview

| Phase | Tasks | Focus Area | Status |
|-------|-------|------------|--------|
| Phase 1 | 8 | Research & Design | ✅ COMPLETED |
| Phase 2 | 25 | Core Infrastructure | ✅ COMPLETED |
| Phase 3 | 38 | WASI Import Implementation | ✅ COMPLETED (2025-10-19) |
| Phase 4 | 18 | Module Integration | ✅ COMPLETED (2025-10-20) |
| Phase 5 | 15 | Lifecycle Methods | ✅ COMPLETED (2025-10-20) |
| Phase 6 | 27 | Testing & Validation | 🟡 TODO |
| Phase 7 | 10 | Documentation | 🟡 TODO |
| **Total** | **141** | **All phases** | **🔵 IN_PROGRESS** |

** Critical Path

Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5 → Phase 6 → Phase 7

** Parallel Execution Opportunities

Within each phase, many tasks marked [P] can run in parallel.
See individual phase documents for dependency graphs.

** High-Risk Areas

- Phase 6 focus: Comprehensive WASI validation needs disciplined coverage to avoid regressions
- Security: Sandboxing and path validation (Tasks 2.10, 4.5, 6.8)
- Test infrastructure: Ensure WAMR + WASI fixtures run consistently across CI environments

* 🚀 Execution Dashboard
:PROPERTIES:
:CURRENT_PHASE: Phase 6: Testing & Validation
:PROGRESS: 113/141
:COMPLETION: 80%
:ACTIVE_TASK: Task 6.13 - Test memory safety with ASAN
:UPDATED: 2025-10-20T15:16:44Z
:END:

** Current Status
- Phase: Phase 6: Testing & Validation
- Progress: 113/141 tasks (80%)
- Active: Test memory safety with ASAN (Task 6.13)

** Next Up
High-priority tasks ready to start (Phase 6):
- [ ] Task 6.13: Test memory safety with ASAN
- [ ] Task 6.14: Test with various WASM compilers
- [ ] Task 6.15: Test module caching

See [[file:node-wasi-plan/phases/phase6-testing-validation.md][Phase 6 document]] for details.

** Parallel Opportunities
Phase 6 combines test authoring and automation; with scaffolding complete, option/fixture tasks (6.4–6.8) and lifecycle validation suites (6.9–6.12) can proceed in parallel.

** Blocking Dependencies
None - All WebAssembly dependencies are satisfied.

**WebAssembly API Status (2025-10-19):**
- ✅ Exported Memory fully functional (required for WASI)
  - instance.exports.mem.buffer works (access memory data)
  - instance.exports.mem.grow() works (grow memory)
  - Tasks 3.35, 5.6: Can validate and access memory exports ✅
- ❌ Standalone Memory constructor blocked (NOT needed for WASI)
  - new WebAssembly.Memory() throws TypeError
  - WASI only needs exported memories from instances
- ✅ No blockers for WASI implementation

See: docs/webassembly-api-compatibility.md for details.

** Risk Areas
- Expanding Phase 6 coverage requires thorough fixtures for args/env/preopens
- Security critical: sandboxing and path validation
- WAMR integration complexity

* 📜 History & Updates
:LOGBOOK:
- State "TODO" from "" [2025-10-16T22:45:00Z] \\
  Initial task plan created. Ready to begin Phase 1: Research & Design.
:END:

** Recent Changes
| Timestamp | Action | Task ID | Details |
|-----------|--------|---------|---------|
| 2025-10-20T15:16:44Z | Completed | Task 6.12 | Confirmed mutual exclusion via lifecycle suite (start after initialize and duplicate initialize attempts). |
| 2025-10-20T14:59:51Z | Completed | Task 6.11 | Confirmed initialize() validation coverage via lifecycle suite (tests 4–7) and reran baseline test matrix. |
| 2025-10-20T14:42:21Z | Completed | Task 6.10 | Added start() validation tests covering missing exports and documented lifecycle coverage. |
| 2025-10-20T13:30:48Z | Completed | Task 6.9 | Added returnOnExit regression tests + fixture to assert default-mode termination. |
| 2025-10-20T12:46:19Z | Completed | Tasks 6.7-6.8 | Added WASI file I/O + sandbox tests with fd_read/fd_write support and updated docs. |
| 2025-10-20T04:58:00Z | Completed | Tasks 6.4-6.6 | Validated args/env/preopen behaviour with new WASI option tests. |
| 2025-10-20T04:57:00Z | Completed | Tasks 6.1-6.3 | Established WASI test harness, baseline constructor tests, and hello-world fixtures. |
| 2025-10-20T04:55:00Z | Completed | Tasks 5.1-5.15 | Finalized WASI lifecycle semantics, documented Phase 5, and promoted plan to Phase 6. |
| 2025-10-20T04:09:00Z | Completed | Tasks 4.9-4.18 | Finalized WASI lifecycle handling, header exports, and regression tests; Phase 4 closed out. |
| 2025-10-19T16:00:00Z | Completed | Tasks 3.14-3.19, 3.28-3.29, 3.38 | Implemented path_* syscalls, poll/socket stubs, and added integration test coverage |
| 2025-10-19T15:06:00Z | Updated | Task 3.33 | Added fd table scaffolding covering stdio + preopen metadata |
| 2025-10-19T14:33:00Z | Updated | Phase 3 | Completed fd_tell/fd_fdstat*, clock_res_get, proc_exit logic, and memory validation |
| 2025-10-20T03:26:00Z | Updated | Phase 4 | Added WASI error helpers & lifecycle validation (Tasks 4.7-4.8) |
| 2025-10-19T16:52:00Z | Updated | Phase 4 | Established loader aliases/tests (Tasks 4.1-4.6) and kicked off error-handling work |
| 2025-10-19T16:00:00Z | Completed | Tasks 3.1, 3.2, 3.33 | Catalogued WASI syscall coverage, finalized FD table management, and refreshed plan metadata |
| 2025-10-19T13:38:53Z | Updated | Phase 2 & 3 | Documented Phase 2 completion, Phase 3 progress, and WASI import safety fix |
| 2025-10-19T08:00:00Z | Completed | Phase 1 | All 8 research and design tasks completed |
| 2025-10-19T08:00:00Z | Created | Research | Comprehensive research notes in research-notes.md |
| 2025-10-19T08:00:00Z | Created | Architecture | Complete architecture design in architecture-design.md |
| 2025-10-19T07:30:00Z | Verified | Dependencies | WebAssembly exported Memory fully functional - no blockers for WASI |
| 2025-10-16T22:45:00Z | Created | ALL | Initial task plan created with 141 tasks across 7 phases |

** Notes
- Total tasks: 141
- Estimated timeline: 2-3 weeks for full implementation
- Critical path: Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5 → Phase 6 → Phase 7
- Parallel opportunities: Many tasks within each phase can run in parallel
- Testing is integrated throughout (not just Phase 6)
- Pending work: deepen Phase 6 coverage (6.4–6.12) across options, I/O, and sandboxing tests

* Implementation Notes

See [[file:node-wasi-plan/design-decisions.md][Design Decisions]] for:
- Key architectural decisions and rationale
- Technology stack details
- Performance and security considerations
- Compatibility targets

* Dependencies

See [[file:node-wasi-plan/dependencies.md][Dependencies]] for:
- External dependencies (WAMR, QuickJS, libuv, WASI SDK)
- Internal dependencies (WebAssembly, module system)
- Version requirements and build dependencies
- Dependency graph

** WebAssembly Dependency Analysis (2025-10-19)

*** Required WebAssembly APIs for WASI
WASI implementation requires the following WebAssembly capabilities:

1. **Exported Memory Access** (REQUIRED) ✅
   - Access: instance.exports.mem.buffer
   - Status: FULLY FUNCTIONAL
   - Used by: Tasks 3.35, 5.6 (memory export validation)
   - Notes: WASI needs to read/write WASM instance memory for system calls

2. **Memory Growth** (REQUIRED) ✅
   - API: instance.exports.mem.grow(delta)
   - Status: FULLY FUNCTIONAL
   - Used by: WASI fd_write, fd_read operations
   - Notes: Some WASI operations may need to grow memory

3. **Module Compilation** (REQUIRED) ✅
   - API: new WebAssembly.Module(bytes)
   - Status: FULLY FUNCTIONAL
   - Used by: WASI constructor (create module from .wasm file)

4. **Instance Creation with Imports** (REQUIRED) ✅
   - API: new WebAssembly.Instance(module, imports)
   - Status: FULLY FUNCTIONAL
   - Used by: Tasks 5.1-5.15 (start/initialize methods)
   - Notes: WASI provides import object via getImportObject()

*** NOT Required for WASI (Blocked APIs)

1. **Standalone Memory Constructor** ❌
   - API: new WebAssembly.Memory({initial: N})
   - Status: BLOCKED (WAMR C API limitation)
   - Impact: NONE - WASI doesn't need to create standalone Memory objects
   - Reason: WASI only consumes memories exported from WASM instances

2. **Standalone Table Constructor** ❌
   - API: new WebAssembly.Table({element: 'funcref', initial: N})
   - Status: BLOCKED (WAMR C API limitation)
   - Impact: NONE - WASI doesn't use Table objects

3. **Standalone Global Constructor** ❌
   - API: new WebAssembly.Global({value: 'i32'}, 42)
   - Status: BLOCKED (WAMR C API limitation)
   - Impact: NONE - WASI doesn't use Global objects

*** Conclusion
✅ **All required WebAssembly APIs are functional**
✅ **No blockers for WASI implementation**
❌ **Blocked APIs (Memory/Table/Global constructors) are not needed by WASI**

Reference: docs/webassembly-api-compatibility.md

* Testing Strategy

See [[file:node-wasi-plan/testing-strategy.md][Testing Strategy]] for:
- Unit, integration, security, and performance tests
- ASAN validation requirements
- Test organization and structure
- Coverage and quality requirements

* Completion Criteria

See [[file:node-wasi-plan/completion-criteria.md][Completion Criteria]] for:
- Functional requirements checklist
- Quality and testing requirements
- Documentation requirements
- Integration requirements
- Final validation checklist and sign-off procedure
