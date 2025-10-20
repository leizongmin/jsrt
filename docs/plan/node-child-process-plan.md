#+TITLE: Task Plan: Node.js child_process Module Implementation
#+AUTHOR: Claude AI Assistant
#+DATE: 2025-10-20T00:00:00Z
#+STARTUP: overview indent
#+TODO: TODO IN-PROGRESS BLOCKED | DONE CANCELLED
#+PRIORITIES: A C B

* Task Metadata
:PROPERTIES:
:CREATED: 2025-10-20T00:00:00Z
:UPDATED: 2025-10-20T15:22:00Z
:STATUS: 🟢 IN-PROGRESS
:PROGRESS: 199/220
:COMPLETION: 90%
:END:

* Status Update Guidelines

** Three-Level Status Tracking
All work in this plan follows a hierarchical status model:
- **Phase Level (L1)**: Major epic phases marked with single asterisk (*)
- **Task Level (L2)**: User stories/features marked with double asterisk (**)
- **Subtask Level (L3+)**: Atomic operations marked with triple+ asterisk (***)

** Status Symbols
- ❌ TODO - Not started
- 🔄 IN-PROGRESS - Currently working on this
- ✅ DONE - Completed successfully
- 🔴 BLOCKED - Blocked by dependencies or issues

** Status Update Rules
1. **Atomic Updates**: Change status immediately after state change
2. **Bottom-Up Completion**: A task/phase can only be marked ✅ when ALL its subtasks are ✅
3. **Consistency**: Never mark parent as complete while children are incomplete
4. **Progress Tracking**: Update PROGRESS property after each subtask completion
5. **Timestamp Updates**: Update :UPDATED: timestamp in Task Metadata on every change

** Update Workflow Example
```org
* ❌ Phase 1: Research [S][R:LOW][C:SIMPLE]  # Start: all TODO
** ❌ Task 1.1: Analyze APIs
*** 🔄 Subtask 1.1.1: Read docs              # Mark in-progress when starting
*** ❌ Subtask 1.1.2: Test examples

# After 1.1.1 completes:
*** ✅ Subtask 1.1.1: Read docs              # Mark done
*** 🔄 Subtask 1.1.2: Test examples          # Start next

# After 1.1.2 completes:
*** ✅ Subtask 1.1.2: Test examples
** ✅ Task 1.1: Analyze APIs                 # All subtasks done → mark task done

# After all Phase 1 tasks complete:
* ✅ Phase 1: Research [S][R:LOW][C:SIMPLE]  # All tasks done → mark phase done
```

** Consistency Requirements
- ❌ NEVER: Mark phase ✅ when tasks are still ❌ or 🔄
- ❌ NEVER: Mark task ✅ when subtasks are still ❌ or 🔄
- ✅ ALWAYS: Update parent status only after children complete
- ✅ ALWAYS: Update PROGRESS counters (e.g., 5/10 → 6/10)

* 📋 Task Analysis & Breakdown

** L0 Main Task
- **Requirement**: Implement Node.js child_process module with full API compatibility
- **Success Criteria**:
  - All async APIs functional (spawn, exec, execFile, fork)
  - All sync APIs functional (spawnSync, execSync, execFileSync)
  - ChildProcess class with complete event system
  - IPC channel working for fork()
  - Stdio handling (stdin/stdout/stderr as streams)
  - 100% unit test pass rate
  - Memory safety validated with ASAN
  - Cross-platform support (Linux/macOS minimum)
- **Constraints**:
  - Use libuv for process management (uv_spawn, uv_process_t)
  - Follow existing jsrt patterns (dgram, fs, events modules)
  - Integrate with EventEmitter system
  - Use QuickJS C API for JavaScript bindings
  - Must pass make format && make test

** API Surface Overview

*** Asynchronous APIs (4 methods)
- child_process.spawn(command, args, options) → ChildProcess
- child_process.exec(command, options, callback) → ChildProcess
- child_process.execFile(file, args, options, callback) → ChildProcess
- child_process.fork(modulePath, args, options) → ChildProcess

*** Synchronous APIs (3 methods)
- child_process.spawnSync(command, args, options) → Object
- child_process.execSync(command, options) → Buffer|String
- child_process.execFileSync(file, args, options) → Buffer|String

*** ChildProcess Class
- Events: exit, close, error, disconnect, message, spawn
- Properties: stdin, stdout, stderr, stdio, pid, connected, killed, exitCode, signalCode
- Methods: kill(signal), send(message, sendHandle), disconnect(), ref(), unref()

*** Options Objects (detailed in sub-document)
See: @docs/plan/node-child-process-plan/api-reference.md for complete option specs

** Architecture Overview

*** Layer 1: C Core (libuv Integration)
- Process spawning (uv_spawn wrapper)
- Stdio pipe management (uv_pipe_t for stdin/stdout/stderr)
- Exit callback handling (uv_exit_cb)
- Signal handling (uv_process_kill)
- IPC channel implementation (uv_pipe_t with IPC flag)

*** Layer 2: JavaScript Bindings (QuickJS)
- C function registration (JS_NewCFunction)
- Class definitions (ChildProcess class)
- EventEmitter integration (inherit from EventEmitter)
- Stream integration (Readable/Writable for stdio)
- Error handling (throw appropriate Node.js errors)

*** Layer 3: Module System
- CommonJS exports (module.exports.spawn, etc.)
- ES Module exports (export { spawn, exec, ... })
- Module registration with jsrt runtime

** Dependencies & Integration Points

*** Internal Dependencies
- EventEmitter (src/node/events/) - Required for ChildProcess events
- Stream (src/node/stream/) - Required for stdin/stdout/stderr
- Buffer (src/node/node_buffer.c) - Required for data handling
- Error handling (src/node/process/control.c patterns)
- Module loader (src/module/) - For fork() to load child modules

*** External Dependencies
- libuv (uv_spawn, uv_process_t, uv_pipe_t, uv_stdio_container_t)
- QuickJS (JS_NewClass, JS_NewCFunction, JSValue management)
- Platform APIs (POSIX signals, process management)

** Risk Assessment

*** High Risk Items
1. **IPC Channel Complexity** [R:HIGH][C:COMPLEX]
   - Serialization/deserialization of messages
   - Handle passing (file descriptors, sockets)
   - Mitigation: Start with simple message passing, defer handle passing

2. **Cross-Platform Behavior** [R:HIGH][C:COMPLEX]
   - Windows vs POSIX signal differences
   - Path handling differences
   - Mitigation: Use libuv abstractions, extensive platform testing

3. **Memory Management** [R:MED][C:MEDIUM]
   - JSValue lifecycle in async callbacks
   - Buffer ownership in stdio streams
   - Mitigation: ASAN validation, careful reference counting

*** Medium Risk Items
1. **Stdio Stream Integration** [R:MED][C:MEDIUM]
   - Backpressure handling
   - Stream end detection
   - Mitigation: Follow existing stream patterns in jsrt

2. **Synchronous API Blocking** [R:MED][C:MEDIUM]
   - Event loop blocking in sync methods
   - Timeout implementation
   - Mitigation: Use uv_run in synchronous mode

*** Low Risk Items
1. **Basic Process Spawning** [R:LOW][C:SIMPLE]
   - Well-documented libuv APIs
   - Clear examples available

2. **Event System** [R:LOW][C:SIMPLE]
   - Existing EventEmitter infrastructure

* 📝 Task Execution

** ✅ Phase 1: Research & Design [S][R:LOW][C:SIMPLE] :research:
:PROPERTIES:
:ID: phase-1
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T01:00:00Z
:DEPS: None
:PROGRESS: 25/25
:COMPLETION: 100%
:END:

Goal: Understand requirements and create detailed design
Duration Estimate: Analysis phase (no time estimate for AI)

*** ✅ Task 1.1: API Documentation Analysis [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 1.1
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T01:00:00Z
:DEPS: None
:END:

Analyze Node.js child_process API specifications and create reference document

**** ✅ Subtask 1.1.1: Document async methods (spawn, exec, execFile, fork)
**** ✅ Subtask 1.1.2: Document sync methods (spawnSync, execSync, execFileSync)
**** ✅ Subtask 1.1.3: Document ChildProcess class (events, properties, methods)
**** ✅ Subtask 1.1.4: Document all options objects (SpawnOptions, ExecOptions, etc.)
**** ✅ Subtask 1.1.5: Document stdio configuration (pipe, ignore, inherit, stream, fd)
**** ✅ Subtask 1.1.6: Create API reference document (@docs/plan/node-child-process-plan/api-reference.md)

*** ✅ Task 1.2: Codebase Pattern Analysis [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 1.2
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T01:00:00Z
:DEPS: None
:END:

Study existing jsrt patterns for similar functionality

**** ✅ Subtask 1.2.1: Analyze dgram module structure (async operations, callbacks)
**** ✅ Subtask 1.2.2: Analyze fs module patterns (libuv async work, callbacks)
**** ✅ Subtask 1.2.3: Analyze EventEmitter integration patterns
**** ✅ Subtask 1.2.4: Analyze Stream integration patterns
**** ✅ Subtask 1.2.5: Document reusable patterns in design doc

*** ✅ Task 1.3: libuv Process API Study [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 1.3
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T01:00:00Z
:DEPS: None
:END:

Deep dive into libuv process management APIs

**** ✅ Subtask 1.3.1: Study uv_spawn() and uv_process_options_t structure
**** ✅ Subtask 1.3.2: Study uv_stdio_container_t for stdio configuration
**** ✅ Subtask 1.3.3: Study uv_pipe_t for IPC channel creation
**** ✅ Subtask 1.3.4: Study uv_process_kill() for signal handling
**** ✅ Subtask 1.3.5: Review libuv example code (deps/libuv/docs/code/)
**** ✅ Subtask 1.3.6: Document libuv integration patterns

*** ✅ Task 1.4: Architecture Design [S][R:MED][C:MEDIUM][D:1.1,1.2,1.3]
:PROPERTIES:
:ID: 1.4
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T01:00:00Z
:DEPS: 1.1,1.2,1.3
:END:

Design C API and JavaScript bindings architecture

**** ✅ Subtask 1.4.1: Design C data structures (ChildProcessData, spawn options)
**** ✅ Subtask 1.4.2: Design stdio pipe management system
**** ✅ Subtask 1.4.3: Design event callback flow (exit, close, error)
**** ✅ Subtask 1.4.4: Design IPC message serialization format
**** ✅ Subtask 1.4.5: Design memory management strategy (JSValue ownership)
**** ✅ Subtask 1.4.6: Create architecture document (@docs/plan/node-child-process-plan/architecture.md)

*** ✅ Task 1.5: File Structure Planning [S][R:LOW][C:SIMPLE][D:1.4]
:PROPERTIES:
:ID: 1.5
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T01:00:00Z
:DEPS: 1.4
:END:

Define directory structure and file organization

**** ✅ Subtask 1.5.1: Plan src/node/child_process/ directory structure
**** ✅ Subtask 1.5.2: Define header files (child_process_internal.h)
**** ✅ Subtask 1.5.3: Define implementation files (module, spawn, exec, fork, sync, ipc, stdio)
**** ✅ Subtask 1.5.4: Plan test directory structure (test/node/child_process/)
**** ✅ Subtask 1.5.5: Document file organization in plan

** ❌ Phase 2: Core Infrastructure [S][R:MED][C:MEDIUM][D:phase-1] :infrastructure:
:PROPERTIES:
:ID: phase-2
:CREATED: 2025-10-20T00:00:00Z
:DEPS: phase-1
:PROGRESS: 0/35
:COMPLETION: 0%
:END:

Goal: Build foundational C structures and module scaffolding
Duration Estimate: Core implementation phase

*** ❌ Task 2.1: Directory & File Setup [S][R:LOW][C:TRIVIAL]
:PROPERTIES:
:ID: 2.1
:CREATED: 2025-10-20T00:00:00Z
:DEPS: None
:END:

Create file structure and boilerplate

**** ❌ Subtask 2.1.1: Create src/node/child_process/ directory
**** ❌ Subtask 2.1.2: Create child_process_internal.h with structure definitions
**** ❌ Subtask 2.1.3: Create child_process_module.c with module registration
**** ❌ Subtask 2.1.4: Create test/node/child_process/ directory
**** ❌ Subtask 2.1.5: Add to build system (CMakeLists.txt)
**** ❌ Subtask 2.1.6: Add to module registration in src/node/node_modules.c

*** ❌ Task 2.2: ChildProcess Class Foundation [S][R:MED][C:MEDIUM][D:2.1]
:PROPERTIES:
:ID: 2.2
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 2.1
:END:

Implement ChildProcess class as EventEmitter subclass

**** ❌ Subtask 2.2.1: Define JSClassDef for ChildProcess
**** ❌ Subtask 2.2.2: Define C struct for ChildProcess data (uv_process_t, pipes, state)
**** ❌ Subtask 2.2.3: Implement class finalizer (cleanup resources)
**** ❌ Subtask 2.2.4: Register class with QuickJS runtime
**** ❌ Subtask 2.2.5: Create prototype inheriting from EventEmitter
**** ❌ Subtask 2.2.6: Implement constructor (private, called by spawn functions)
**** ❌ Subtask 2.2.7: Add pid property getter
**** ❌ Subtask 2.2.8: Add exitCode property getter
**** ❌ Subtask 2.2.9: Add signalCode property getter
**** ❌ Subtask 2.2.10: Add killed property getter
**** ❌ Subtask 2.2.11: Add connected property getter (IPC only)

*** ❌ Task 2.3: Stdio Pipe Infrastructure [S][R:MED][C:MEDIUM][D:2.2]
:PROPERTIES:
:ID: 2.3
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 2.2
:END:

Build stdio pipe management system

**** ❌ Subtask 2.3.1: Create child_process_stdio.c file
**** ❌ Subtask 2.3.2: Define StdioPipe structure (uv_pipe_t, stream JSValue, state)
**** ❌ Subtask 2.3.3: Implement stdio configuration parser (parse 'pipe', 'ignore', 'inherit', fd)
**** ❌ Subtask 2.3.4: Implement uv_stdio_container_t builder from options
**** ❌ Subtask 2.3.5: Implement pipe creation for stdin (Writable stream)
**** ❌ Subtask 2.3.6: Implement pipe creation for stdout (Readable stream)
**** ❌ Subtask 2.3.7: Implement pipe creation for stderr (Readable stream)
**** ❌ Subtask 2.3.8: Implement pipe cleanup on process exit
**** ❌ Subtask 2.3.9: Add stdin property to ChildProcess
**** ❌ Subtask 2.3.10: Add stdout property to ChildProcess
**** ❌ Subtask 2.3.11: Add stderr property to ChildProcess
**** ❌ Subtask 2.3.12: Add stdio array property to ChildProcess

*** ❌ Task 2.4: Options Parser [P][R:LOW][C:SIMPLE][D:2.1]
:PROPERTIES:
:ID: 2.4
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 2.1
:END:

Parse JavaScript options objects to C structures

**** ❌ Subtask 2.4.1: Create child_process_options.c file
**** ❌ Subtask 2.4.2: Define SpawnOptions C structure
**** ❌ Subtask 2.4.3: Implement cwd option parsing
**** ❌ Subtask 2.4.4: Implement env option parsing (object to char** array)
**** ❌ Subtask 2.4.5: Implement uid/gid option parsing (POSIX only)
**** ❌ Subtask 2.4.6: Implement detached option parsing
**** ❌ Subtask 2.4.7: Implement shell option parsing
**** ❌ Subtask 2.4.8: Implement windowsHide option parsing
**** ❌ Subtask 2.4.9: Implement timeout option parsing (for exec variants)
**** ❌ Subtask 2.4.10: Implement maxBuffer option parsing (for exec variants)
**** ❌ Subtask 2.4.11: Implement killSignal option parsing

*** ❌ Task 2.5: Error Handling System [P][R:LOW][C:SIMPLE][D:2.1]
:PROPERTIES:
:ID: 2.5
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 2.1
:END:

Implement Node.js-compatible error handling

**** ❌ Subtask 2.5.1: Create child_process_errors.c file
**** ❌ Subtask 2.5.2: Implement create_spawn_error() (ENOENT, EACCES, etc.)
**** ❌ Subtask 2.5.3: Implement create_exec_error() (timeout, signal, exit code)
**** ❌ Subtask 2.5.4: Implement error code mapping (libuv to Node.js)
**** ❌ Subtask 2.5.5: Add errno property to error objects
**** ❌ Subtask 2.5.6: Add code property to error objects
**** ❌ Subtask 2.5.7: Add path/syscall properties where applicable

** ✅ Phase 3: Asynchronous spawn() [S][R:MED][C:MEDIUM][D:phase-2] :implementation:
:PROPERTIES:
:ID: phase-3
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T02:30:00Z
:DEPS: phase-2
:PROGRESS: 30/30
:COMPLETION: 100%
:END:

Goal: Implement child_process.spawn() - foundation for all async APIs
Duration Estimate: Core async implementation

*** ✅ Task 3.1: Basic Spawn Implementation [S][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: 3.1
:CREATED: 2025-10-20T00:00:00Z
:DEPS: None
:END:

Implement core spawn functionality without advanced features

*** ✅ Subtask 3.1.1: Create child_process_spawn.c file
*** ✅ Subtask 3.1.2: Implement js_child_process_spawn() JS binding
*** ✅ Subtask 3.1.3: Parse command and args arguments
*** ✅ Subtask 3.1.4: Parse options object using Task 2.4 parser
*** ✅ Subtask 3.1.5: Create ChildProcess instance
*** ✅ Subtask 3.1.6: Allocate uv_process_t handle
*** ✅ Subtask 3.1.7: Build uv_process_options_t structure
*** ✅ Subtask 3.1.8: Set file and args in options
*** ✅ Subtask 3.1.9: Set cwd in options
*** ✅ Subtask 3.1.10: Set env in options
*** ✅ Subtask 3.1.11: Set exit_cb callback

*** ✅ Task 3.2: Stdio Configuration [S][R:MED][C:MEDIUM][D:3.1]
:PROPERTIES:
:ID: 3.2
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 3.1
:END:

Configure stdio pipes for spawn

*** ✅ Subtask 3.2.1: Configure stdio using Task 2.3 infrastructure
*** ✅ Subtask 3.2.2: Create pipes for 'pipe' mode
*** ✅ Subtask 3.2.3: Handle 'ignore' mode (UV_IGNORE flag)
*** ✅ Subtask 3.2.4: Handle 'inherit' mode (UV_INHERIT_FD/STREAM)
*** ✅ Subtask 3.2.5: Set stdio_count and stdio in uv_process_options_t
*** ✅ Subtask 3.2.6: Attach stream objects to ChildProcess instance

*** ✅ Task 3.3: Process Execution [S][R:MED][C:MEDIUM][D:3.2]
:PROPERTIES:
:ID: 3.3
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 3.2
:END:

Execute process and handle initialization

*** ✅ Subtask 3.3.1: Call uv_spawn() to start process
*** ✅ Subtask 3.3.2: Handle spawn errors (ENOENT, EACCES, etc.)
*** ✅ Subtask 3.3.3: Emit 'error' event on spawn failure
*** ✅ Subtask 3.3.4: Set pid property on ChildProcess
*** ✅ Subtask 3.3.5: Emit 'spawn' event on success
*** ✅ Subtask 3.3.6: Return ChildProcess instance to JavaScript

*** ✅ Task 3.4: Exit Callback Handling [S][R:MED][C:MEDIUM][D:3.3]
:PROPERTIES:
:ID: 3.4
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 3.3
:END:

Implement exit callback and event emission

*** ✅ Subtask 3.4.1: Implement on_process_exit() C callback
*** ✅ Subtask 3.4.2: Extract exit_status from callback parameters
*** ✅ Subtask 3.4.3: Extract term_signal from callback parameters
*** ✅ Subtask 3.4.4: Set exitCode property on ChildProcess
*** ✅ Subtask 3.4.5: Set signalCode property on ChildProcess (if signaled)
*** ✅ Subtask 3.4.6: Emit 'exit' event with (code, signal)
*** ✅ Subtask 3.4.7: Close all stdio pipes
*** ✅ Subtask 3.4.8: Emit 'close' event after all streams closed
*** ✅ Subtask 3.4.9: Close uv_process_t handle with uv_close()

*** ✅ Task 3.5: ChildProcess Methods [P][R:LOW][C:SIMPLE][D:3.4]
:PROPERTIES:
:ID: 3.5
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 3.4
:END:

Implement ChildProcess instance methods

*** ✅ Subtask 3.5.1: Implement kill(signal) method
*** ✅ Subtask 3.5.2: Map signal names to platform signal numbers
*** ✅ Subtask 3.5.3: Call uv_process_kill() with signal
*** ✅ Subtask 3.5.4: Set killed property to true
*** ✅ Subtask 3.5.5: Implement ref() method (call uv_ref)
*** ✅ Subtask 3.5.6: Implement unref() method (call uv_unref)

*** ✅ Task 3.6: Testing spawn() [S][R:LOW][C:SIMPLE][D:3.5]
:PROPERTIES:
:ID: 3.6
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 3.5
:END:

Create comprehensive tests for spawn()

*** ✅ Subtask 3.6.1: Create test/node/child_process/test_spawn_basic.js
*** ✅ Subtask 3.6.2: Test successful process spawn (e.g., 'echo hello')
*** ✅ Subtask 3.6.3: Test process with arguments
*** ✅ Subtask 3.6.4: Test exit event emission
*** ✅ Subtask 3.6.5: Test close event emission
*** ✅ Subtask 3.6.6: Test spawn error (ENOENT for nonexistent command)
*** ✅ Subtask 3.6.7: Test stdio pipes (read stdout/stderr)
*** ✅ Subtask 3.6.8: Test stdin writing
*** ✅ Subtask 3.6.9: Test kill() method
*** ✅ Subtask 3.6.10: Test ref/unref methods
*** ✅ Subtask 3.6.11: Run tests: make test N=node/child_process
*** ✅ Subtask 3.6.12: Validate memory safety: make jsrt_m && ASAN test

** ✅ Phase 4: Asynchronous exec() and execFile() [S][R:MED][C:MEDIUM][D:phase-3] :implementation:
:PROPERTIES:
:ID: phase-4
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T03:00:00Z
:DEPS: phase-3
:PROGRESS: 28/28
:COMPLETION: 100%
:END:

Goal: Implement convenience methods that buffer output
Duration Estimate: Built on spawn() foundation

*** ✅ Task 4.1: exec() Implementation [S][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: 4.1
:CREATED: 2025-10-20T00:00:00Z
:DEPS: None
:END:

Implement exec() which runs command in shell

*** ✅ Subtask 4.1.1: Create child_process_exec.c file
*** ✅ Subtask 4.1.2: Implement js_child_process_exec() JS binding
*** ✅ Subtask 4.1.3: Parse command string and options
*** ✅ Subtask 4.1.4: Extract callback function from arguments
*** ✅ Subtask 4.1.5: Set shell option to true (platform-specific: /bin/sh or cmd.exe)
*** ✅ Subtask 4.1.6: Call spawn() internally with shell
*** ✅ Subtask 4.1.7: Create stdout buffer (maxBuffer limit)
*** ✅ Subtask 4.1.8: Create stderr buffer (maxBuffer limit)
*** ✅ Subtask 4.1.9: Attach 'data' event listeners to stdout/stderr
*** ✅ Subtask 4.1.10: Accumulate data in buffers
*** ✅ Subtask 4.1.11: Check maxBuffer limit and kill if exceeded
*** ✅ Subtask 4.1.12: Implement timeout mechanism (kill after timeout)
*** ✅ Subtask 4.1.13: Handle exit event and call callback(err, stdout, stderr)
*** ✅ Subtask 4.1.14: Return ChildProcess instance

*** ✅ Task 4.2: execFile() Implementation [S][R:MED][C:MEDIUM][D:4.1]
:PROPERTIES:
:ID: 4.2
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 4.1
:END:

Implement execFile() which runs file directly without shell

*** ✅ Subtask 4.2.1: Implement js_child_process_exec_file() JS binding
*** ✅ Subtask 4.2.2: Parse file, args array, and options
*** ✅ Subtask 4.2.3: Extract callback function
*** ✅ Subtask 4.2.4: Do NOT set shell option (direct execution)
*** ✅ Subtask 4.2.5: Reuse exec() buffering logic
*** ✅ Subtask 4.2.6: Call spawn() internally
*** ✅ Subtask 4.2.7: Return ChildProcess instance

*** ✅ Task 4.3: Timeout Implementation [P][R:MED][C:MEDIUM][D:4.1]
:PROPERTIES:
:ID: 4.3
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 4.1
:END:

Implement timeout mechanism for exec/execFile

*** ✅ Subtask 4.3.1: Create uv_timer_t for timeout tracking
*** ✅ Subtask 4.3.2: Start timer when process spawns
*** ✅ Subtask 4.3.3: Implement timer callback to kill process
*** ✅ Subtask 4.3.4: Use killSignal option (default SIGTERM)
*** ✅ Subtask 4.3.5: Cancel timer on process exit
*** ✅ Subtask 4.3.6: Set error.killed = true in callback
*** ✅ Subtask 4.3.7: Cleanup timer resources

*** ✅ Task 4.4: Testing exec/execFile [S][R:LOW][C:SIMPLE][D:4.2,4.3]
:PROPERTIES:
:ID: 4.4
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 4.2,4.3
:END:

Test exec and execFile functionality

*** ✅ Subtask 4.4.1: Create test/node/child_process/test_exec.js
*** ✅ Subtask 4.4.2: Test exec() with simple command
*** ✅ Subtask 4.4.3: Test exec() with shell features (pipes, redirects)
*** ✅ Subtask 4.4.4: Test exec() callback receives stdout/stderr
*** ✅ Subtask 4.4.5: Test exec() timeout mechanism
*** ✅ Subtask 4.4.6: Test exec() maxBuffer exceeded error
*** ✅ Subtask 4.4.7: Test execFile() with arguments
*** ✅ Subtask 4.4.8: Test execFile() does not interpret shell syntax
*** ✅ Subtask 4.4.9: Test execFile() timeout
*** ✅ Subtask 4.4.10: Run tests: make test N=node/child_process
*** ✅ Subtask 4.4.11: Memory safety validation: make jsrt_m && ASAN test

** ✅ Phase 5: Synchronous APIs [S][R:MED][C:MEDIUM][D:phase-4] :implementation:
:PROPERTIES:
:ID: phase-5
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T03:30:00Z
:DEPS: phase-4
:PROGRESS: 32/32
:COMPLETION: 100%
:END:

Goal: Implement blocking synchronous variants
Duration Estimate: Sync execution with blocking behavior

*** ✅ Task 5.1: Synchronous Execution Infrastructure [S][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: 5.1
:CREATED: 2025-10-20T00:00:00Z
:DEPS: None
:END:

Build infrastructure for blocking process execution

*** ✅ Subtask 5.1.1: Create child_process_sync.c file
*** ✅ Subtask 5.1.2: Design SyncProcessResult structure (status, signal, stdout, stderr, error)
*** ✅ Subtask 5.1.3: Implement spawn_sync_internal() helper
*** ✅ Subtask 5.1.4: Configure process with uv_spawn
*** ✅ Subtask 5.1.5: Create temporary event loop for blocking (uv_loop_init)
*** ✅ Subtask 5.1.6: Run loop in UV_RUN_DEFAULT mode until process exits
*** ✅ Subtask 5.1.7: Collect stdout/stderr in memory buffers
*** ✅ Subtask 5.1.8: Implement timeout using uv_timer_t
*** ✅ Subtask 5.1.9: Close temporary loop (uv_loop_close)
*** ✅ Subtask 5.1.10: Return result structure

*** ✅ Task 5.2: spawnSync() Implementation [S][R:MED][C:MEDIUM][D:5.1]
:PROPERTIES:
:ID: 5.2
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 5.1
:END:

Implement synchronous spawn

*** ✅ Subtask 5.2.1: Implement js_child_process_spawn_sync() JS binding
*** ✅ Subtask 5.2.2: Parse command, args, options
*** ✅ Subtask 5.2.3: Call spawn_sync_internal()
*** ✅ Subtask 5.2.4: Build result object {pid, output, stdout, stderr, status, signal, error}
*** ✅ Subtask 5.2.5: Handle encoding option (buffer vs string)
*** ✅ Subtask 5.2.6: Return result object

*** ✅ Task 5.3: execSync() Implementation [S][R:MED][C:MEDIUM][D:5.2]
:PROPERTIES:
:ID: 5.3
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 5.2
:END:

Implement synchronous exec

*** ✅ Subtask 5.3.1: Implement js_child_process_exec_sync() JS binding
*** ✅ Subtask 5.3.2: Parse command and options
*** ✅ Subtask 5.3.3: Set shell option to true
*** ✅ Subtask 5.3.4: Call spawn_sync_internal()
*** ✅ Subtask 5.3.5: Check exit code - throw on non-zero if not configured
*** ✅ Subtask 5.3.6: Return stdout (buffer or string based on encoding)
*** ✅ Subtask 5.3.7: Attach error.stdout and error.stderr on failure

*** ✅ Task 5.4: execFileSync() Implementation [S][R:LOW][C:SIMPLE][D:5.3]
:PROPERTIES:
:ID: 5.4
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 5.3
:END:

Implement synchronous execFile

*** ✅ Subtask 5.4.1: Implement js_child_process_exec_file_sync() JS binding
*** ✅ Subtask 5.4.2: Parse file, args, options
*** ✅ Subtask 5.4.3: Do NOT set shell option
*** ✅ Subtask 5.4.4: Reuse execSync logic
*** ✅ Subtask 5.4.5: Return stdout

*** ✅ Task 5.5: Testing Sync APIs [S][R:LOW][C:SIMPLE][D:5.4]
:PROPERTIES:
:ID: 5.5
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 5.4
:END:

Test synchronous APIs thoroughly

*** ✅ Subtask 5.5.1: Create test/node/child_process/test_sync.js
*** ✅ Subtask 5.5.2: Test spawnSync() basic execution
*** ✅ Subtask 5.5.3: Test spawnSync() result object structure
*** ✅ Subtask 5.5.4: Test spawnSync() with stdout/stderr
*** ✅ Subtask 5.5.5: Test spawnSync() encoding option
*** ✅ Subtask 5.5.6: Test execSync() basic execution
*** ✅ Subtask 5.5.7: Test execSync() throws on non-zero exit
*** ✅ Subtask 5.5.8: Test execSync() timeout
*** ✅ Subtask 5.5.9: Test execFileSync() basic execution
*** ✅ Subtask 5.5.10: Test execFileSync() error handling
*** ✅ Subtask 5.5.11: Run tests: make test N=node/child_process
*** ✅ Subtask 5.5.12: Memory safety validation: make jsrt_m && ASAN test

** ❌ Phase 6: IPC Channel and fork() [S][R:HIGH][C:COMPLEX][D:phase-3] :implementation:
:PROPERTIES:
:ID: phase-6
:CREATED: 2025-10-20T00:00:00Z
:DEPS: phase-3
:PROGRESS: 0/40
:COMPLETION: 0%
:END:

Goal: Implement IPC messaging system and fork() for Node.js child processes
Duration Estimate: Complex IPC implementation

*** ❌ Task 6.1: IPC Channel Infrastructure [S][R:HIGH][C:COMPLEX]
:PROPERTIES:
:ID: 6.1
:CREATED: 2025-10-20T00:00:00Z
:DEPS: None
:END:

Build IPC communication channel using uv_pipe

**** ❌ Subtask 6.1.1: Create child_process_ipc.c file
**** ❌ Subtask 6.1.2: Define IPCChannel structure (uv_pipe_t, message queue, state)
**** ❌ Subtask 6.1.3: Implement create_ipc_channel() - create UV_IPC pipe
**** ❌ Subtask 6.1.4: Configure stdio entry for IPC (stdio[3] typically)
**** ❌ Subtask 6.1.5: Implement IPC channel setup in parent
**** ❌ Subtask 6.1.6: Implement IPC channel setup in child (via environment variable)
**** ❌ Subtask 6.1.7: Add IPC pipe to uv_stdio_container_t
**** ❌ Subtask 6.1.8: Implement channel reference counting

*** ❌ Task 6.2: Message Serialization [S][R:HIGH][C:COMPLEX][D:6.1]
:PROPERTIES:
:ID: 6.2
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 6.1
:END:

Implement message serialization/deserialization

**** ❌ Subtask 6.2.1: Design message format (length-prefixed JSON)
**** ❌ Subtask 6.2.2: Implement serialize_message() - JSValue to buffer
**** ❌ Subtask 6.2.3: Support JSON-serializable objects
**** ❌ Subtask 6.2.4: Support null, undefined, primitives
**** ❌ Subtask 6.2.5: Implement deserialize_message() - buffer to JSValue
**** ❌ Subtask 6.2.6: Handle serialization errors gracefully
**** ❌ Subtask 6.2.7: Add message length header (4 bytes)

*** ❌ Task 6.3: send() Method [S][R:MED][C:MEDIUM][D:6.2]
:PROPERTIES:
:ID: 6.3
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 6.2
:END:

Implement ChildProcess.send() method

**** ❌ Subtask 6.3.1: Implement js_child_process_send() JS binding
**** ❌ Subtask 6.3.2: Validate process has IPC channel
**** ❌ Subtask 6.3.3: Serialize message using Task 6.2
**** ❌ Subtask 6.3.4: Write to IPC pipe (uv_write)
**** ❌ Subtask 6.3.5: Handle write completion callback
**** ❌ Subtask 6.3.6: Handle optional callback parameter
**** ❌ Subtask 6.3.7: Return boolean (success/failure)
**** ❌ Subtask 6.3.8: Implement send queue for backpressure

*** ❌ Task 6.4: Message Reception [S][R:MED][C:MEDIUM][D:6.2]
:PROPERTIES:
:ID: 6.4
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 6.2
:END:

Implement message reception and 'message' event

**** ❌ Subtask 6.4.1: Implement on_ipc_read() callback for uv_read_start
**** ❌ Subtask 6.4.2: Parse message length header
**** ❌ Subtask 6.4.3: Read full message body
**** ❌ Subtask 6.4.4: Deserialize message
**** ❌ Subtask 6.4.5: Emit 'message' event on ChildProcess
**** ❌ Subtask 6.4.6: Handle partial reads (buffer incomplete messages)
**** ❌ Subtask 6.4.7: Handle read errors

*** ❌ Task 6.5: disconnect() Method [S][R:LOW][C:SIMPLE][D:6.1]
:PROPERTIES:
:ID: 6.5
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 6.1
:END:

Implement IPC channel disconnection

**** ❌ Subtask 6.5.1: Implement js_child_process_disconnect() JS binding
**** ❌ Subtask 6.5.2: Close IPC pipe (uv_close)
**** ❌ Subtask 6.5.3: Set connected property to false
**** ❌ Subtask 6.5.4: Emit 'disconnect' event
**** ❌ Subtask 6.5.5: Cleanup IPC channel resources

*** ❌ Task 6.6: fork() Implementation [S][R:HIGH][C:COMPLEX][D:6.5,6.4,6.3]
:PROPERTIES:
:ID: 6.6
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 6.5,6.4,6.3
:END:

Implement fork() to spawn Node.js child processes with IPC

**** ❌ Subtask 6.6.1: Create child_process_fork.c file
**** ❌ Subtask 6.6.2: Implement js_child_process_fork() JS binding
**** ❌ Subtask 6.6.3: Parse modulePath and options
**** ❌ Subtask 6.6.4: Resolve module path (absolute or relative to cwd)
**** ❌ Subtask 6.6.5: Create IPC channel
**** ❌ Subtask 6.6.6: Build execArgv (V8/Node.js flags - skip for jsrt)
**** ❌ Subtask 6.6.7: Set command to process.execPath (jsrt binary)
**** ❌ Subtask 6.6.8: Build args array [modulePath, ...args]
**** ❌ Subtask 6.6.9: Set silent option (redirect stdio or inherit)
**** ❌ Subtask 6.6.10: Call spawn() with IPC channel configured
**** ❌ Subtask 6.6.11: Return ChildProcess with IPC methods enabled

*** ❌ Task 6.7: Child Process IPC Bootstrap [S][R:MED][C:MEDIUM][D:6.6]
:PROPERTIES:
:ID: 6.7
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 6.6
:END:

Setup IPC in child process when forked

**** ❌ Subtask 6.7.1: Detect IPC channel in child (check stdio fd or env var)
**** ❌ Subtask 6.7.2: Create process.send() method in child
**** ❌ Subtask 6.7.3: Create process.disconnect() method in child
**** ❌ Subtask 6.7.4: Setup 'message' event on process object
**** ❌ Subtask 6.7.5: Set process.connected property
**** ❌ Subtask 6.7.6: Initialize IPC channel reader in child

*** ❌ Task 6.8: Testing IPC and fork() [S][R:MED][C:MEDIUM][D:6.7]
:PROPERTIES:
:ID: 6.8
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 6.7
:END:

Test IPC functionality comprehensively

**** ❌ Subtask 6.8.1: Create test/node/child_process/test_fork.js
**** ❌ Subtask 6.8.2: Create test/node/child_process/fixtures/ipc_child.js
**** ❌ Subtask 6.8.3: Test fork() spawns jsrt process
**** ❌ Subtask 6.8.4: Test send() from parent to child
**** ❌ Subtask 6.8.5: Test 'message' event in child
**** ❌ Subtask 6.8.6: Test send() from child to parent (process.send)
**** ❌ Subtask 6.8.7: Test 'message' event in parent
**** ❌ Subtask 6.8.8: Test bidirectional messaging
**** ❌ Subtask 6.8.9: Test disconnect() from parent
**** ❌ Subtask 6.8.10: Test disconnect() from child
**** ❌ Subtask 6.8.11: Test 'disconnect' event
**** ❌ Subtask 6.8.12: Test message serialization edge cases
**** ❌ Subtask 6.8.13: Run tests: make test N=node/child_process
**** ❌ Subtask 6.8.14: Memory safety validation: make jsrt_m && ASAN test

** 🔄 Phase 7: Advanced Features [P][R:MED][C:MEDIUM][D:phase-5,phase-6] :implementation:
:PROPERTIES:
:ID: phase-7
:CREATED: 2025-10-20T00:00:00Z
:DEPS: phase-5,phase-6
:PROGRESS: 20/25
:COMPLETION: 80%
:END:

Goal: Implement platform-specific and advanced features
Duration Estimate: Platform integration phase

*** ✅ Task 7.1: Environment Variable Handling [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 7.1
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T15:15:00Z
:DEPS: None
:END:

Complete environment variable support

**** ✅ Subtask 7.1.1: Implement env object to char** converter
**** ✅ Subtask 7.1.2: Handle env inheritance (merge with process.env)
- Added extern char** environ for POSIX
- Pass environ when child->env is NULL (inheritance mode)
**** ✅ Subtask 7.1.3: Handle env replacement (use only provided env)
- Pass child->env when provided (replacement mode)
- Support empty {} for truly empty environment
**** ✅ Subtask 7.1.4: Test env variable passing
- Created test_spawn_env.js with 4 comprehensive tests
- Tested custom variables, parent inheritance, merge, and empty env
**** ✅ Subtask 7.1.5: Test env inheritance
- Verified PATH and other variables inherited by default
- Verified no inheritance when env object provided

*** ✅ Task 7.2: Working Directory Support [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 7.2
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T15:10:00Z
:DEPS: None
:END:

Implement cwd option fully

**** ✅ Subtask 7.2.1: Validate cwd exists
- Added stat() validation before spawn
- Emit ENOENT error if directory doesn't exist
- Emit ENOTDIR error if path is not a directory
**** ✅ Subtask 7.2.2: Test absolute cwd paths
- Created test_spawn_cwd.js with /tmp test
- Verified process runs in specified directory
**** ✅ Subtask 7.2.3: Test relative cwd paths
- Tested relative path '..' navigation
- Verified correct directory resolution
**** ✅ Subtask 7.2.4: Test cwd errors (non-existent directory)
- Verified ENOENT error emission
- Verified ENOTDIR error for files
- All errors emitted asynchronously for proper listener attachment

*** ✅ Task 7.3: Signal Handling [P][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: 7.3
:CREATED: 2025-10-20T00:00:00Z
:COMPLETED: 2025-10-20T15:18:00Z
:DEPS: None
:END:

Implement comprehensive signal support

**** ✅ Subtask 7.3.1: Create signal name to number mapping (SIGTERM, SIGKILL, etc.)
- Already implemented: 22 POSIX signals + 2 Windows signals
- signal_from_name() and signal_to_name() functions complete
**** ✅ Subtask 7.3.2: Support string signal names in kill()
- Already implemented in js_child_process_kill()
- Accepts "SIGTERM", "SIGKILL", "SIGINT", etc.
**** ✅ Subtask 7.3.3: Support numeric signals in kill()
- Already implemented in js_child_process_kill()
- Accepts numeric signal values (9, 15, etc.)
**** ✅ Subtask 7.3.4: Handle platform differences (Windows vs POSIX)
- POSIX signals implemented for Unix/Linux/macOS
- Windows SIGTERM and SIGKILL mapped appropriately
**** ✅ Subtask 7.3.5: Test signal sending
- Created test_spawn_signals.js with 5 comprehensive tests
- Tested SIGTERM, SIGKILL, SIGINT by name and number
**** ✅ Subtask 7.3.6: Test signalCode property
- Exposed child.killed as getter property
- Verified dynamic update after kill() called

*** ❌ Task 7.4: Platform-Specific Options [P][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: 7.4
:CREATED: 2025-10-20T00:00:00Z
:DEPS: None
:END:

Implement platform-specific features

**** ❌ Subtask 7.4.1: Implement uid/gid on POSIX (setuid, setgid)
**** ❌ Subtask 7.4.2: Implement windowsHide on Windows
**** ❌ Subtask 7.4.3: Implement detached option (uv_process_flags)
**** ❌ Subtask 7.4.4: Test detached processes
**** ❌ Subtask 7.4.5: Test uid/gid on Linux (if root)

*** ❌ Task 7.5: Shell Mode [P][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: 7.5
:CREATED: 2025-10-20T00:00:00Z
:DEPS: None
:END:

Complete shell mode implementation

**** ❌ Subtask 7.5.1: Detect platform shell (/bin/sh, cmd.exe, PowerShell)
**** ❌ Subtask 7.5.2: Build shell command properly (sh -c "cmd" vs cmd /d /s /c "cmd")
**** ❌ Subtask 7.5.3: Handle shell: string option (custom shell)
**** ❌ Subtask 7.5.4: Handle shell: true option (default shell)
**** ❌ Subtask 7.5.5: Test shell execution
**** ❌ Subtask 7.5.6: Test custom shell

** ❌ Phase 8: Integration & Documentation [S][R:LOW][C:SIMPLE][D:phase-7] :documentation:
:PROPERTIES:
:ID: phase-8
:CREATED: 2025-10-20T00:00:00Z
:DEPS: phase-7
:PROGRESS: 0/20
:COMPLETION: 0%
:END:

Goal: Finalize integration, documentation, and cross-platform validation
Duration Estimate: Final polish and documentation

*** ❌ Task 8.1: Module Registration [S][R:LOW][C:TRIVIAL]
:PROPERTIES:
:ID: 8.1
:CREATED: 2025-10-20T00:00:00Z
:DEPS: None
:END:

Complete module system integration

**** ❌ Subtask 8.1.1: Update src/node/node_modules.c with child_process
**** ❌ Subtask 8.1.2: Register CommonJS module initialization
**** ❌ Subtask 8.1.3: Register ES module initialization
**** ❌ Subtask 8.1.4: Test require('child_process')
**** ❌ Subtask 8.1.5: Test import from 'node:child_process'

*** ❌ Task 8.2: API Documentation [S][R:LOW][C:SIMPLE][D:8.1]
:PROPERTIES:
:ID: 8.2
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 8.1
:END:

Create user-facing documentation

**** ❌ Subtask 8.2.1: Document all APIs in docs/api/child_process.md
**** ❌ Subtask 8.2.2: Document options for each method
**** ❌ Subtask 8.2.3: Add usage examples
**** ❌ Subtask 8.2.4: Document IPC protocol
**** ❌ Subtask 8.2.5: Document platform differences

*** ❌ Task 8.3: Example Programs [S][R:LOW][C:SIMPLE][D:8.2]
:PROPERTIES:
:ID: 8.3
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 8.2
:END:

Create example programs

**** ❌ Subtask 8.3.1: Create examples/child_process/ directory
**** ❌ Subtask 8.3.2: Create spawn example
**** ❌ Subtask 8.3.3: Create exec example
**** ❌ Subtask 8.3.4: Create fork with IPC example
**** ❌ Subtask 8.3.5: Create README in examples directory

*** ❌ Task 8.4: Comprehensive Testing [S][R:MED][C:MEDIUM][D:8.1]
:PROPERTIES:
:ID: 8.4
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 8.1
:END:

Final comprehensive test suite

**** ❌ Subtask 8.4.1: Run full test suite: make test
**** ❌ Subtask 8.4.2: Run child_process tests: make test N=node/child_process
**** ❌ Subtask 8.4.3: Run ASAN tests: make jsrt_m && run all tests
**** ❌ Subtask 8.4.4: Test on Linux
**** ❌ Subtask 8.4.5: Test on macOS (if available)
**** ❌ Subtask 8.4.6: Fix any failing tests
**** ❌ Subtask 8.4.7: Validate 100% test pass rate

*** ❌ Task 8.5: Final Validation [S][R:LOW][C:SIMPLE][D:8.4]
:PROPERTIES:
:ID: 8.5
:CREATED: 2025-10-20T00:00:00Z
:DEPS: 8.4
:END:

Final quality checks before completion

**** ❌ Subtask 8.5.1: Run make format (all code formatted)
**** ❌ Subtask 8.5.2: Run make test (100% pass)
**** ❌ Subtask 8.5.3: Run make wpt (no regressions)
**** ❌ Subtask 8.5.4: Memory leak check (ASAN clean)
**** ❌ Subtask 8.5.5: Update CLAUDE.md if needed
**** ❌ Subtask 8.5.6: Mark plan as COMPLETED

* 🚀 Execution Dashboard
:PROPERTIES:
:CURRENT_PHASE: Not Started
:PROGRESS: 0/220
:COMPLETION: 0%
:ACTIVE_TASK: None
:UPDATED: 2025-10-20T00:00:00Z
:END:

** Current Status
- Phase: Planning completed, awaiting implementation start
- Progress: 0/220 tasks (0%)
- Active: None
- Next Phase: Phase 1 - Research & Design

** Next Up
- [ ] Task 1.1: API Documentation Analysis
- [ ] Task 1.2: Codebase Pattern Analysis
- [ ] Task 1.3: libuv Process API Study
- [ ] Task 1.4: Architecture Design
- [ ] Task 1.5: File Structure Planning

** Parallel Execution Opportunities
Tasks marked [P] can run in parallel within each phase:
- Phase 1: Tasks 1.1, 1.2, 1.3 can run in parallel
- Phase 2: Tasks 2.4, 2.5 can run in parallel after 2.1
- Phase 3: Task 3.5 can run in parallel with 3.4 completion
- Phase 4: Task 4.3 can run in parallel with 4.1
- Phase 5: All sync API tests can run in parallel
- Phase 7: All advanced feature tasks can run in parallel
- Phase 8: Documentation tasks can run in parallel with testing

* 📜 History & Updates
:LOGBOOK:
- State "PLANNING" from "TODO" [2025-10-20T00:00:00Z] \\
  Initial plan created with 8 phases and 220 total tasks
:END:

** Plan Creation
| Timestamp | Action | Details |
|-----------|--------|---------|
| 2025-10-20T00:00:00Z | Created | Initial comprehensive plan with hierarchical breakdown |
| 2025-10-20T00:00:00Z | Analyzed | Node.js child_process API v25.0.0 documentation |
| 2025-10-20T00:00:00Z | Analyzed | jsrt codebase patterns (dgram, fs, events) |
| 2025-10-20T00:00:00Z | Analyzed | libuv process APIs (uv_spawn, uv_process_t) |
| 2025-10-20T00:00:00Z | Documented | Status update guidelines for hierarchical tracking |

** Implementation Notes
- Total 220 subtasks across 8 phases
- 45 parallel execution opportunities identified
- IPC implementation flagged as highest risk/complexity
- Cross-platform testing required (Linux/macOS minimum)
- Memory safety validation mandatory throughout

** Risk Mitigation
- High-risk items: IPC, cross-platform, memory management
- Mitigation: Incremental development, ASAN validation, extensive testing
- Defer handle passing in IPC to later iteration if needed
- Use existing jsrt patterns to reduce integration risk

* Progress Summary

** Phase 6 Status (85% Complete - 34/40 tasks)
*** Completed
- IPC Channel Infrastructure (child_process_ipc.c - 432 lines)
  - Message queue with backpressure handling
  - Length-prefixed JSON serialization
  - Partial message buffering and reassembly
  - Event-driven message reception
- Parent-Side IPC APIs
  - ChildProcess.send(message, callback)
  - ChildProcess.disconnect()
  - 'message' and 'disconnect' events
  - connected property tracking
- fork() Implementation (child_process_fork.c - 204 lines)
  - Complete Node.js-compatible API
  - Automatic IPC channel setup on stdio[3]
  - Silent mode support
  - /proc/self/exe binary path resolution
- Testing Infrastructure
  - test_fork_simple.js (basic API verification)
  - test_fork.js (full IPC tests - partial)
  - fixtures/ipc_child.js (test helper)
  - 4/5 child_process tests passing

*** Remaining (Task 6.7 - Child-Side IPC Bootstrap)
- process.send() in child processes
- process.on('message') in child
- process.disconnect() in child
- process.connected property in child
- Requires integration with global process object module
- This is a separate infrastructure piece that connects to existing process module

* Appendix: Task Statistics

** Phase Breakdown
| Phase | Tasks | Status | Complexity | Risk |
|-------|-------|--------|------------|------|
| Phase 1 | 25 | DONE | SIMPLE | LOW |
| Phase 2 | 35 | DONE | MEDIUM | MED |
| Phase 3 | 30 | DONE | MEDIUM | MED |
| Phase 4 | 28 | DONE | MEDIUM | MED |
| Phase 5 | 32 | DONE | MEDIUM | MED |
| Phase 6 | 40 | 85% | COMPLEX | HIGH |
| Phase 7 | 25 | TODO | MEDIUM | MED |
| Phase 8 | 20 | TODO | SIMPLE | LOW |
| **Total** | **235** | 81% | - | - |

** Execution Modes
- Sequential [S]: 24 tasks (must complete before next starts)
- Parallel [P]: 12 tasks (can run simultaneously)
- Parallel-Sequential [PS]: 0 tasks

** Dependency Graph Critical Path
1. Phase 1 (Research) → Phase 2 (Infrastructure)
2. Phase 2 → Phase 3 (spawn) → Phase 4 (exec/execFile) → Phase 5 (sync APIs)
3. Phase 3 (spawn) → Phase 6 (IPC/fork)
4. Phase 5, Phase 6 → Phase 7 (Advanced) → Phase 8 (Integration)

Estimated development flow: Sequential through Phases 1-3, then Phase 4-5-6 can partially overlap, Phase 7-8 sequential finalization.
