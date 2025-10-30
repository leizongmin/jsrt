#+TITLE: Task Plan: Node.js Process API Implementation
#+AUTHOR: Claude AI Assistant
#+DATE: 2025-10-30T00:00:00Z
#+STARTUP: overview indent
#+TODO: TODO IN-PROGRESS BLOCKED | DONE CANCELLED
#+PRIORITIES: A C B

* Task Metadata
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:UPDATED: 2025-10-30T00:00:00Z
:STATUS: 🟡 PLANNING
:PROGRESS: 0/153
:COMPLETION: 0%
:END:

* Status Update Guidelines

This plan uses three-level status tracking for precise progress monitoring:

** Phase Status (Org-mode keywords)
- TODO: Phase not started
- IN-PROGRESS: Currently working on this phase
- DONE: Phase completed and verified
- BLOCKED: Phase cannot proceed due to dependencies or issues

** Task Checkboxes (within phases)
- [ ]: Task not started
- [x]: Task completed
- [!]: Task blocked or has issues

** Progress Tracking
- Update PROGRESS property after each completed task
- Update COMPLETION percentage at phase boundaries
- Always include timestamp in History section when changing status
- Mark dependencies clearly with [D:PhaseN] or [D:TaskID]

** Consistency Rules
1. Phase status MUST match task checkbox states (all [x] → DONE)
2. Progress counter MUST match actual completed tasks
3. Document blockers immediately in History section with mitigation plan
4. Update plan after EVERY significant state change

* Overview

** Project Summary
Implement remaining Node.js-compatible process APIs in jsrt to achieve comprehensive Node.js process compatibility. This extends the existing process module at `/repo/src/node/process/` with missing properties, methods, and events while maintaining cross-platform support (Linux, macOS, Windows).

** Current Implementation Status (Verified)

*** Implemented Properties (13)
- process.pid, ppid, argv, argv0, platform, arch
- process.version, versions
- process.env (Proxy-based)
- process.stdin, stdout, stderr
- process.connected (IPC)

*** Implemented Methods (13)
- process.exit(), cwd(), chdir()
- process.uptime(), hrtime(), hrtime.bigint()
- process.nextTick(), memoryUsage(), abort()
- process.send(), disconnect(), on(), emit() (IPC events)

*** Missing APIs (Summary)
- **Properties**: 15 (execPath, execArgv, exitCode, title, config, release, etc.)
- **Methods**: 29 (kill, cpuUsage, resourceUsage, uid/gid, warnings, etc.)
- **Events**: 8+ (beforeExit, exit, warning, signals, uncaught exceptions)

** Implementation Strategy
- **Phases**: 8 phases organized by priority and dependencies
- **Pattern**: Follow existing code structure in `/repo/src/node/process/`
- **Organization**: Group by functional area (properties.c, signals.c, resources.c, etc.)
- **Testing**: Comprehensive unit tests + WPT regression checks
- **Platform**: Unix/Linux primary, Windows where possible, graceful stubs otherwise

** Document Structure
- Phase 1-2: High-priority missing APIs (properties, signals, events)
- Phase 3-4: Resource monitoring and timing enhancements
- Phase 5-6: Unix-specific features and advanced APIs
- Phase 7-8: Future stubs and comprehensive testing

* 📋 Task Analysis & Breakdown

** TODO [#A] Phase 1: Missing Properties Implementation [S][R:LOW][C:MEDIUM] :properties:
:PROPERTIES:
:ID: phase-1
:CREATED: 2025-10-30T00:00:00Z
:DEPS: None
:PROGRESS: 0/35
:COMPLETION: 0%
:END:

Implement 15 missing process properties with getter/setter support where applicable.

*** Architecture
- Create `/repo/src/node/process/properties.c` for new property implementations
- Update `/repo/src/node/process/process.h` with declarations
- Follow getter pattern from basic.c (JS_DefinePropertyGetSet)
- Platform-specific implementations in process_platform.c

*** Tasks

**** TODO [#A] Task 1.1: Setup properties.c infrastructure [S][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: None
:END:

- [ ] Create properties.c and properties.h files
- [ ] Add to CMakeLists.txt build system
- [ ] Create initialization function jsrt_process_init_properties()
- [ ] Wire up to module.c initialization chain
- [ ] Add cleanup function for dynamic properties
- [ ] Test: Build succeeds with empty stubs

**** TODO [#A] Task 1.2: Implement process.execPath [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Implement getter: readlink /proc/self/exe (Linux)
- [ ] Implement getter: _NSGetExecutablePath() (macOS)
- [ ] Implement getter: GetModuleFileName() (Windows)
- [ ] Cache result at startup (immutable)
- [ ] Return string property
- [ ] Test: Verify executable path returned correctly

**** TODO [#A] Task 1.3: Implement process.execArgv [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Parse runtime flags from jsrt_argv (--inspect, --max-old-space-size, etc.)
- [ ] Filter out script name and script arguments
- [ ] Return array of jsrt-specific flags
- [ ] Test: Verify flags parsed correctly

**** TODO [#A] Task 1.4: Implement process.exitCode [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Add global variable g_exit_code (default: undefined)
- [ ] Implement getter returning g_exit_code
- [ ] Implement setter validating number/undefined
- [ ] Modify process.exit() to use exitCode if no arg provided
- [ ] Test: Set exitCode, call exit() without arg, verify code

**** TODO [#A] Task 1.5: Implement process.title [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Implement getter using platform-specific APIs
- [ ] Implement setter: prctl(PR_SET_NAME) (Linux)
- [ ] Implement setter: setproctitle() emulation (other Unix)
- [ ] Implement setter: SetConsoleTitle() (Windows)
- [ ] Limit title length (platform-dependent)
- [ ] Test: Set and get process title

**** TODO [#A] Task 1.6: Implement process.config [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Create static config object
- [ ] Add target_defaults.default_configuration (Release/Debug)
- [ ] Add variables.host_arch (x64, arm64, etc.)
- [ ] Add variables.target_arch
- [ ] Add variables.node_prefix (install path)
- [ ] Test: Verify config structure matches Node.js format

**** TODO [#A] Task 1.7: Implement process.release [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Create release object with name: "jsrt"
- [ ] Add sourceUrl (GitHub repo URL if applicable)
- [ ] Add headersUrl (empty or stub)
- [ ] Add libUrl (empty or stub)
- [ ] Add lts: false (jsrt not LTS)
- [ ] Test: Verify release object structure

**** TODO [#B] Task 1.8: Implement process.features [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Create features object
- [ ] Add inspector: false (no inspector yet)
- [ ] Add debug: true/false (based on DEBUG flag)
- [ ] Add uv: true (libuv available)
- [ ] Add ipv6: true (check at runtime)
- [ ] Add tls_alpn, tls_sni, tls_ocsp, tls: false (not implemented)
- [ ] Test: Verify features object

**** TODO [#B] Task 1.9: Implement process.debugPort [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Return default value 9229 (no debugger implemented)
- [ ] Add getter only (immutable for now)
- [ ] Test: Verify returns 9229

**** TODO [#B] Task 1.10: Implement process.mainModule [P][R:LOW][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Track main module in module loader
- [ ] Return reference to require.main
- [ ] Handle case when no main module (REPL)
- [ ] Return undefined if not applicable
- [ ] Test: Verify main module reference

**** TODO [#B] Task 1.11: Implement process.channel [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Return IPC channel object if connected
- [ ] Return undefined if no IPC
- [ ] Reuse existing IPC state from process_ipc.c
- [ ] Test: Verify channel object when forked

**** TODO [#C] Task 1.12: Implement deprecation flags [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Add process.noDeprecation (default: false)
- [ ] Add process.throwDeprecation (default: false)
- [ ] Add process.traceDeprecation (default: false)
- [ ] Make writable properties
- [ ] Test: Set and get each flag

**** TODO [#C] Task 1.13: Implement process.allowedNodeEnvironmentFlags [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Create Set-like object
- [ ] Add common Node.js flags (--max-old-space-size, --inspect, etc.)
- [ ] Implement has() method
- [ ] Make immutable
- [ ] Test: Verify Set operations

**** TODO [#C] Task 1.14: Implement process.sourceMapsEnabled [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.1
:END:

- [ ] Add getter returning false (not implemented yet)
- [ ] Add setter (no-op for now, accept boolean)
- [ ] Test: Set and get value

**** TODO [#B] Task 1.15: Wire up all properties to module [S][R:LOW][C:SIMPLE][D:1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,1.11,1.12,1.13,1.14]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.2,1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,1.11,1.12,1.13,1.14
:END:

- [ ] Update module.c to call jsrt_process_init_properties()
- [ ] Register all getters/setters in jsrt_init_unified_process_module()
- [ ] Update process.h with all declarations
- [ ] Test: All properties accessible via process object

**** TODO [#A] Task 1.16: Create unit tests for properties [S][R:LOW][C:SIMPLE][D:1.15]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.15
:END:

- [ ] Create test/jsrt/test_jsrt_process_properties.js
- [ ] Test execPath returns valid path
- [ ] Test execArgv is array
- [ ] Test exitCode getter/setter
- [ ] Test title getter/setter
- [ ] Test config structure
- [ ] Test release structure
- [ ] Test features object
- [ ] Test all flags accessible
- [ ] Run: make test N=jsrt/test_jsrt_process_properties

**** TODO [#A] Task 1.17: Memory safety validation [S][R:LOW][C:SIMPLE][D:1.16]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 1.16
:END:

- [ ] Build with ASAN: make jsrt_m
- [ ] Run all property tests with ASAN
- [ ] Fix any memory leaks detected
- [ ] Verify no buffer overflows in title setter
- [ ] Test: ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/jsrt/test_jsrt_process_properties.js

*** Acceptance Criteria
- [ ] All 15 properties implemented and accessible
- [ ] Properties follow Node.js API semantics
- [ ] Cross-platform support (graceful degradation where needed)
- [ ] All unit tests pass
- [ ] No memory leaks (ASAN validated)
- [ ] Code formatted: make format

** TODO [#A] Phase 2: Process Control & Signal Events [S][R:MED][C:COMPLEX][D:phase-1] :signals:events:
:PROPERTIES:
:ID: phase-2
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:PROGRESS: 0/42
:COMPLETION: 0%
:END:

Implement process.kill(), signal handling, and critical process lifecycle events.

*** Architecture
- Create `/repo/src/node/process/signals.c` for signal handling
- Extend process_ipc.c event system for process events
- Use libuv signal handlers (uv_signal_t)
- EventEmitter pattern from existing stream/events infrastructure
- Platform abstraction for Windows vs Unix signals

*** Tasks

**** TODO [#A] Task 2.1: Setup signals.c infrastructure [S][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Create signals.c and signals.h files
- [ ] Add to CMakeLists.txt
- [ ] Define signal name constants (SIGINT, SIGTERM, etc.)
- [ ] Create signal handler registry (map signal → callbacks)
- [ ] Initialize libuv signal handlers
- [ ] Add cleanup function to remove handlers
- [ ] Test: Build succeeds

**** TODO [#A] Task 2.2: Implement process.kill(pid, signal) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.1
:END:

- [ ] Parse pid argument (number or string)
- [ ] Parse signal argument (string or number, default SIGTERM)
- [ ] Map signal names to numbers ("SIGINT" → 2)
- [ ] Use kill() syscall (Unix) or TerminateProcess (Windows)
- [ ] Validate pid exists and is accessible
- [ ] Return true on success, throw on error
- [ ] Test: Send signal to own process

**** TODO [#A] Task 2.3: Implement signal event registration [P][R:MED][C:COMPLEX]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.1
:END:

- [ ] Extend process.on() to handle signal events
- [ ] Create uv_signal_t handle for each registered signal
- [ ] Map signal names (SIGINT, SIGTERM, SIGHUP, etc.)
- [ ] Start libuv signal watcher on first listener
- [ ] Stop watcher when last listener removed
- [ ] Handle multiple listeners per signal
- [ ] Test: Register SIGINT handler, send signal, verify callback

**** TODO [#A] Task 2.4: Implement SIGINT event [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.3
:END:

- [ ] Implement 'SIGINT' event
- [ ] Default behavior: process.exit(128 + SIGINT)
- [ ] Allow user override with process.on('SIGINT', handler)
- [ ] Prevent default exit if handler registered
- [ ] Test: Ctrl+C handling

**** TODO [#A] Task 2.5: Implement SIGTERM event [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.3
:END:

- [ ] Implement 'SIGTERM' event
- [ ] Default behavior: process.exit(128 + SIGTERM)
- [ ] Allow user override
- [ ] Test: Send SIGTERM, verify handler

**** TODO [#B] Task 2.6: Implement additional signal events [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.3
:END:

- [ ] Implement SIGHUP (Unix only)
- [ ] Implement SIGQUIT (Unix only)
- [ ] Implement SIGUSR1 (Unix only)
- [ ] Implement SIGUSR2 (Unix only)
- [ ] Implement SIGBREAK (Windows only)
- [ ] Stub unsupported signals gracefully
- [ ] Test: Each signal on appropriate platform

**** TODO [#A] Task 2.7: Implement 'beforeExit' event [P][R:MED][C:COMPLEX]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.1
:END:

- [ ] Emit before event loop would exit
- [ ] Pass exit code to listeners
- [ ] Allow async operations (keeps loop alive)
- [ ] Hook into main event loop (runtime.c)
- [ ] Don't emit on explicit exit() or fatal signals
- [ ] Test: Register beforeExit, schedule async work, verify executes

**** TODO [#A] Task 2.8: Implement 'exit' event [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.1
:END:

- [ ] Emit when process about to exit
- [ ] Pass exit code to listeners
- [ ] Emit synchronously only (no async allowed)
- [ ] Hook into process_exit() function
- [ ] Emit on both explicit exit() and natural termination
- [ ] Test: Register exit handler, verify called with correct code

**** TODO [#A] Task 2.9: Implement 'warning' event [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.1
:END:

- [ ] Emit when process.emitWarning() called
- [ ] Pass warning object (name, message, code, detail)
- [ ] Default behavior: print to stderr
- [ ] Allow user override with process.on('warning', handler)
- [ ] Test: Emit warning, verify event and stderr output

**** TODO [#A] Task 2.10: Implement process.emitWarning() [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.9
:END:

- [ ] Parse warning argument (string or Error)
- [ ] Parse optional type, code, ctor arguments
- [ ] Create warning object
- [ ] Emit 'warning' event
- [ ] Check process.noDeprecation for deprecation warnings
- [ ] Throw if process.throwDeprecation set
- [ ] Test: Emit various warning types

**** TODO [#A] Task 2.11: Implement 'uncaughtException' event [P][R:HIGH][C:COMPLEX]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.1
:END:

- [ ] Hook into QuickJS exception handler
- [ ] Emit when exception not caught by try/catch
- [ ] Pass error object to listeners
- [ ] Default behavior: print stack trace and exit(1)
- [ ] Prevent default exit if handler registered
- [ ] Support origin parameter (promise rejection vs throw)
- [ ] Test: Throw uncaught exception, verify handler

**** TODO [#A] Task 2.12: Implement 'uncaughtExceptionMonitor' event [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.11
:END:

- [ ] Emit before 'uncaughtException'
- [ ] Monitor only (doesn't prevent exit)
- [ ] Use for logging/telemetry
- [ ] Test: Verify monitor fires before main handler

**** TODO [#A] Task 2.13: Implement 'unhandledRejection' event [P][R:HIGH][C:COMPLEX]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.1
:END:

- [ ] Hook into QuickJS promise rejection tracking
- [ ] Emit when promise rejected without .catch()
- [ ] Pass reason and promise to listeners
- [ ] Track unhandled rejections in registry
- [ ] Default behavior: print warning (future: exit)
- [ ] Test: Reject promise without catch, verify event

**** TODO [#A] Task 2.14: Implement 'rejectionHandled' event [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.13
:END:

- [ ] Emit when late .catch() added to previously unhandled rejection
- [ ] Pass promise to listeners
- [ ] Remove from unhandled rejection registry
- [ ] Test: Late catch handler, verify event

**** TODO [#A] Task 2.15: Implement setUncaughtExceptionCaptureCallback [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.11
:END:

- [ ] Store global exception capture callback
- [ ] Mutually exclusive with 'uncaughtException' listeners
- [ ] Throw if uncaughtException listeners exist
- [ ] Call callback instead of emitting event
- [ ] Test: Set capture callback, throw, verify called

**** TODO [#A] Task 2.16: Implement hasUncaughtExceptionCaptureCallback [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.15
:END:

- [ ] Return true if capture callback set
- [ ] Return false otherwise
- [ ] Test: Verify before and after setting callback

**** TODO [#B] Task 2.17: Wire up events to EventEmitter system [S][R:MED][C:MEDIUM][D:2.4,2.5,2.6,2.7,2.8,2.9,2.11,2.12,2.13,2.14]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.4,2.5,2.6,2.7,2.8,2.9,2.11,2.12,2.13,2.14
:END:

- [ ] Integrate with existing event system (event_emitter_core.c)
- [ ] Make process inherit from EventEmitter
- [ ] Support on(), once(), off(), emit() for all events
- [ ] Support removeListener(), removeAllListeners()
- [ ] Test: Standard EventEmitter operations on process

**** TODO [#A] Task 2.18: Create unit tests for process.kill [S][R:MED][C:MEDIUM][D:2.2]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.2
:END:

- [ ] Create test/jsrt/test_jsrt_process_signals.js
- [ ] Test kill with numeric pid
- [ ] Test kill with signal name
- [ ] Test kill with signal number
- [ ] Test error handling (invalid pid, permission denied)
- [ ] Run: make test N=jsrt/test_jsrt_process_signals

**** TODO [#A] Task 2.19: Create unit tests for signal events [S][R:MED][C:MEDIUM][D:2.17]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.17
:END:

- [ ] Test SIGINT handler registration
- [ ] Test SIGTERM handler
- [ ] Test signal handler removal
- [ ] Test multiple listeners per signal
- [ ] Test default signal behavior
- [ ] Run: make test N=jsrt/test_jsrt_process_signals

**** TODO [#A] Task 2.20: Create unit tests for lifecycle events [S][R:MED][C:MEDIUM][D:2.17]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.17
:END:

- [ ] Test beforeExit event
- [ ] Test exit event
- [ ] Test warning event
- [ ] Test emitWarning() function
- [ ] Test event order (beforeExit → exit)
- [ ] Create test/jsrt/test_jsrt_process_lifecycle.js
- [ ] Run: make test N=jsrt/test_jsrt_process_lifecycle

**** TODO [#A] Task 2.21: Create unit tests for exception events [S][R:HIGH][C:COMPLEX][D:2.17]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.17
:END:

- [ ] Test uncaughtException event
- [ ] Test uncaughtExceptionMonitor event
- [ ] Test unhandledRejection event
- [ ] Test rejectionHandled event
- [ ] Test exception capture callback
- [ ] Test mutual exclusivity of capture callback and listeners
- [ ] Create test/jsrt/test_jsrt_process_exceptions.js
- [ ] Run: make test N=jsrt/test_jsrt_process_exceptions

**** TODO [#A] Task 2.22: Memory safety validation [S][R:MED][C:MEDIUM][D:2.18,2.19,2.20,2.21]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 2.18,2.19,2.20,2.21
:END:

- [ ] Build with ASAN: make jsrt_m
- [ ] Run all signal tests with ASAN
- [ ] Fix any signal handler leaks
- [ ] Verify event listener cleanup
- [ ] Test: ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/jsrt/test_jsrt_process_*.js

*** Acceptance Criteria
- [ ] process.kill() works on all platforms
- [ ] All signal events functional (platform-appropriate)
- [ ] All lifecycle events implemented (beforeExit, exit, warning)
- [ ] All exception events functional
- [ ] No memory leaks in signal handlers
- [ ] All unit tests pass
- [ ] Code formatted: make format

** TODO [#A] Phase 3: Memory & Resource Monitoring [S][R:MED][C:COMPLEX][D:phase-1] :resources:
:PROPERTIES:
:ID: phase-3
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:PROGRESS: 0/25
:COMPLETION: 0%
:END:

Implement CPU usage, resource monitoring, and enhanced memory APIs.

*** Architecture
- Create `/repo/src/node/process/resources.c` for resource monitoring
- Use getrusage() (Unix) and GetProcessTimes() (Windows)
- Extend existing memoryUsage() implementation
- Platform-specific implementations for available memory

*** Tasks

**** TODO [#A] Task 3.1: Setup resources.c infrastructure [S][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Create resources.c and resources.h files
- [ ] Add to CMakeLists.txt
- [ ] Platform detection macros
- [ ] Initialization function
- [ ] Test: Build succeeds

**** TODO [#A] Task 3.2: Implement process.cpuUsage([previousValue]) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.1
:END:

- [ ] Use getrusage(RUSAGE_SELF) for user/system time (Unix)
- [ ] Use GetProcessTimes() (Windows)
- [ ] Return { user: micros, system: micros }
- [ ] Support optional previousValue for delta calculation
- [ ] Convert to microseconds
- [ ] Test: Verify returns valid CPU times

**** TODO [#A] Task 3.3: Implement process.resourceUsage() [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.1
:END:

- [ ] Use getrusage() for comprehensive stats (Unix)
- [ ] Return object with: userCPUTime, systemCPUTime
- [ ] Add: maxRSS, sharedMemorySize, unsharedDataSize
- [ ] Add: unsharedStackSize, minorPageFault, majorPageFault
- [ ] Add: swappedOut, fsRead, fsWrite
- [ ] Add: ipcSent, ipcReceived, signalsCount
- [ ] Add: voluntaryContextSwitches, involuntaryContextSwitches
- [ ] Windows: subset using available APIs
- [ ] Test: Verify structure and values

**** TODO [#A] Task 3.4: Implement process.memoryUsage.rss() [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.1
:END:

- [ ] Extract RSS calculation from existing memoryUsage()
- [ ] Create getter function on memoryUsage
- [ ] Return RSS in bytes
- [ ] Test: Verify matches memoryUsage().rss

**** TODO [#A] Task 3.5: Implement process.availableMemory() [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.1
:END:

- [ ] Use sysconf(_SC_AVPHYS_PAGES) * pagesize (Linux)
- [ ] Use host_statistics64() (macOS)
- [ ] Use GlobalMemoryStatusEx() (Windows)
- [ ] Return available physical memory in bytes
- [ ] Test: Verify returns reasonable value

**** TODO [#B] Task 3.6: Implement process.constrainedMemory() [P][R:LOW][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.1
:END:

- [ ] Check cgroup memory limits (Linux)
- [ ] Check setrlimit(RLIMIT_AS) limits
- [ ] Return undefined if no constraint
- [ ] Return limit in bytes if constrained
- [ ] Test: Verify on system with/without limits

**** TODO [#A] Task 3.7: Enhance existing memoryUsage() [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.1
:END:

- [ ] Move implementation from process_node.c to resources.c
- [ ] Improve heapTotal calculation (QuickJS memory stats)
- [ ] Improve heapUsed calculation
- [ ] Add external memory tracking
- [ ] Add arrayBuffers tracking
- [ ] Test: Verify all fields accurate

**** TODO [#B] Task 3.8: Wire up resource APIs to module [S][R:LOW][C:SIMPLE][D:3.2,3.3,3.4,3.5,3.6,3.7]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.2,3.3,3.4,3.5,3.6,3.7
:END:

- [ ] Update module.c to call jsrt_process_init_resources()
- [ ] Register all resource functions
- [ ] Update process.h declarations
- [ ] Test: All APIs accessible

**** TODO [#A] Task 3.9: Create unit tests for CPU usage [S][R:MED][C:MEDIUM][D:3.2]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.2
:END:

- [ ] Create test/jsrt/test_jsrt_process_resources.js
- [ ] Test cpuUsage() returns valid structure
- [ ] Test delta calculation with previousValue
- [ ] Test user and system times increase
- [ ] Run: make test N=jsrt/test_jsrt_process_resources

**** TODO [#A] Task 3.10: Create unit tests for resource usage [S][R:MED][C:MEDIUM][D:3.3]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.3
:END:

- [ ] Test resourceUsage() returns all fields
- [ ] Verify types (all numbers)
- [ ] Verify reasonable ranges
- [ ] Test on different platforms
- [ ] Run: make test N=jsrt/test_jsrt_process_resources

**** TODO [#A] Task 3.11: Create unit tests for memory APIs [S][R:MED][C:MEDIUM][D:3.4,3.5,3.6,3.7]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.4,3.5,3.6,3.7
:END:

- [ ] Test memoryUsage.rss() returns number
- [ ] Test availableMemory() returns reasonable value
- [ ] Test constrainedMemory() behavior
- [ ] Test enhanced memoryUsage() fields
- [ ] Run: make test N=jsrt/test_jsrt_process_resources

**** TODO [#A] Task 3.12: Memory safety validation [S][R:MED][C:MEDIUM][D:3.9,3.10,3.11]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 3.9,3.10,3.11
:END:

- [ ] Build with ASAN: make jsrt_m
- [ ] Run all resource tests with ASAN
- [ ] Verify no leaks in repeated calls
- [ ] Test: ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/jsrt/test_jsrt_process_resources.js

*** Acceptance Criteria
- [ ] All resource monitoring APIs functional
- [ ] Cross-platform support (graceful degradation)
- [ ] Accurate memory and CPU statistics
- [ ] All unit tests pass
- [ ] No memory leaks
- [ ] Code formatted: make format

** TODO [#B] Phase 4: Timing Enhancements [S][R:LOW][C:SIMPLE][D:phase-1] :timing:
:PROPERTIES:
:ID: phase-4
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:PROGRESS: 0/8
:COMPLETION: 0%
:END:

Enhance existing hrtime implementation and add edge case handling.

*** Architecture
- Extend `/repo/src/node/process/process_time.c`
- Improve hrtime.bigint() implementation
- Add platform-specific high-resolution timer optimization

*** Tasks

**** TODO [#B] Task 4.1: Enhance hrtime.bigint() [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Verify nanosecond precision
- [ ] Test on all platforms
- [ ] Handle edge cases (overflow, negative values)
- [ ] Test: Verify bigint operations

**** TODO [#B] Task 4.2: Add hrtime error handling [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Validate previousTime argument
- [ ] Throw on invalid input
- [ ] Handle clock monotonicity issues
- [ ] Test: Invalid inputs

**** TODO [#B] Task 4.3: Optimize high-resolution timers [P][R:LOW][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Use clock_gettime(CLOCK_MONOTONIC) on Linux
- [ ] Use mach_absolute_time() on macOS
- [ ] Use QueryPerformanceCounter() on Windows
- [ ] Benchmark performance
- [ ] Test: Verify nanosecond resolution

**** TODO [#B] Task 4.4: Add process.uptime() precision [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Use high-resolution timer for uptime
- [ ] Return fractional seconds
- [ ] Test: Verify precision

**** TODO [#B] Task 4.5: Create comprehensive timing tests [S][R:LOW][C:SIMPLE][D:4.1,4.2,4.3,4.4]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 4.1,4.2,4.3,4.4
:END:

- [ ] Create test/jsrt/test_jsrt_process_timing.js
- [ ] Test hrtime precision
- [ ] Test hrtime.bigint() precision
- [ ] Test uptime precision
- [ ] Test error handling
- [ ] Run: make test N=jsrt/test_jsrt_process_timing

*** Acceptance Criteria
- [ ] High-resolution timers optimized
- [ ] Edge cases handled
- [ ] All tests pass
- [ ] Code formatted: make format

** TODO [#B] Phase 5: User & Group Management (Unix) [S][R:LOW][C:MEDIUM][D:phase-1] :unix:permissions:
:PROPERTIES:
:ID: phase-5
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:PROGRESS: 0/28
:COMPLETION: 0%
:END:

Implement Unix-specific user/group ID management and umask operations.

*** Architecture
- Create `/repo/src/node/process/unix_permissions.c`
- Unix-only compilation (ifdef guards)
- Use setuid/setgid family of syscalls
- Proper permission checking and error handling

*** Tasks

**** TODO [#B] Task 5.1: Setup unix_permissions.c [S][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Create unix_permissions.c (Unix only)
- [ ] Add ifdef guards for non-Unix platforms
- [ ] Add to CMakeLists.txt with platform check
- [ ] Initialization function
- [ ] Test: Build on Linux/macOS, skip on Windows

**** TODO [#B] Task 5.2: Implement process.getuid() [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Call getuid() syscall
- [ ] Return numeric user ID
- [ ] Test: Verify returns current UID

**** TODO [#B] Task 5.3: Implement process.geteuid() [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Call geteuid() syscall
- [ ] Return numeric effective user ID
- [ ] Test: Verify returns effective UID

**** TODO [#B] Task 5.4: Implement process.getgid() [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Call getgid() syscall
- [ ] Return numeric group ID
- [ ] Test: Verify returns current GID

**** TODO [#B] Task 5.5: Implement process.getegid() [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Call getegid() syscall
- [ ] Return numeric effective group ID
- [ ] Test: Verify returns effective GID

**** TODO [#B] Task 5.6: Implement process.setuid(id) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Parse id argument (number or string username)
- [ ] Resolve username to UID if string
- [ ] Call setuid() syscall
- [ ] Check for permission errors
- [ ] Throw on failure
- [ ] Test: Requires root, skip if unprivileged

**** TODO [#B] Task 5.7: Implement process.seteuid(id) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Parse id argument
- [ ] Call seteuid() syscall
- [ ] Error handling
- [ ] Test: Requires root

**** TODO [#B] Task 5.8: Implement process.setgid(id) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Parse id argument (number or group name)
- [ ] Resolve group name to GID
- [ ] Call setgid() syscall
- [ ] Error handling
- [ ] Test: Requires root

**** TODO [#B] Task 5.9: Implement process.setegid(id) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Parse id argument
- [ ] Call setegid() syscall
- [ ] Error handling
- [ ] Test: Requires root

**** TODO [#B] Task 5.10: Implement process.getgroups() [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Call getgroups() syscall
- [ ] Return array of supplementary group IDs
- [ ] Handle variable group count
- [ ] Test: Verify group list

**** TODO [#B] Task 5.11: Implement process.setgroups(groups) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Parse groups array (numbers or group names)
- [ ] Resolve group names to GIDs
- [ ] Call setgroups() syscall
- [ ] Requires root privilege
- [ ] Error handling
- [ ] Test: Requires root

**** TODO [#B] Task 5.12: Implement process.initgroups(user, extraGroup) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Parse user argument (string or UID)
- [ ] Parse extraGroup argument
- [ ] Call initgroups() syscall
- [ ] Initialize supplementary groups
- [ ] Requires root privilege
- [ ] Test: Requires root

**** TODO [#B] Task 5.13: Implement process.umask() getter [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Call umask() syscall to get current mask
- [ ] Restore previous value immediately (umask is set+get)
- [ ] Return current umask as octal number
- [ ] Test: Verify returns current umask

**** TODO [#B] Task 5.14: Implement process.umask(mask) setter [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.1
:END:

- [ ] Parse mask argument (number or string octal)
- [ ] Call umask(mask) syscall
- [ ] Return previous umask value
- [ ] Test: Set umask, verify change

**** TODO [#B] Task 5.15: Wire up Unix APIs to module [S][R:LOW][C:SIMPLE][D:5.2,5.3,5.4,5.5,5.6,5.7,5.8,5.9,5.10,5.11,5.12,5.13,5.14]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.2,5.3,5.4,5.5,5.6,5.7,5.8,5.9,5.10,5.11,5.12,5.13,5.14
:END:

- [ ] Conditionally register Unix functions (ifdef __unix__)
- [ ] Update module.c initialization
- [ ] Update process.h with Unix function declarations
- [ ] Test: APIs available on Unix, undefined on Windows

**** TODO [#B] Task 5.16: Create unit tests for user/group IDs [S][R:LOW][C:MEDIUM][D:5.15]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.15
:END:

- [ ] Create test/jsrt/test_jsrt_process_unix.js (skip on Windows)
- [ ] Test getuid(), geteuid(), getgid(), getegid()
- [ ] Test getgroups()
- [ ] Mark setter tests as root-only (skip if unprivileged)
- [ ] Run: make test N=jsrt/test_jsrt_process_unix

**** TODO [#B] Task 5.17: Create unit tests for umask [S][R:LOW][C:SIMPLE][D:5.15]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 5.15
:END:

- [ ] Test umask() getter
- [ ] Test umask(mask) setter
- [ ] Verify return value is previous mask
- [ ] Test octal string parsing
- [ ] Run: make test N=jsrt/test_jsrt_process_unix

*** Acceptance Criteria
- [ ] All Unix permission APIs functional
- [ ] Gracefully unavailable on Windows
- [ ] Proper error handling for permission denied
- [ ] Tests pass on Unix platforms
- [ ] Code formatted: make format

** TODO [#B] Phase 6: Advanced Features [S][R:MED][C:COMPLEX][D:phase-2,phase-3] :advanced:
:PROPERTIES:
:ID: phase-6
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-2,phase-3
:PROGRESS: 0/15
:COMPLETION: 0%
:END:

Implement advanced process APIs including loadEnvFile, active resources, source maps.

*** Architecture
- Extend existing components with advanced features
- Integration with module loader and runtime
- Platform-specific optimizations

*** Tasks

**** TODO [#B] Task 6.1: Implement process.loadEnvFile(path) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Parse .env file format (KEY=VALUE)
- [ ] Support comments (# prefix)
- [ ] Support quoted values ("value", 'value')
- [ ] Support variable expansion ($VAR, ${VAR})
- [ ] Merge into process.env
- [ ] Default path: .env in current directory
- [ ] Error handling for missing file
- [ ] Test: Load .env file, verify environment variables

**** TODO [#B] Task 6.2: Implement process.getActiveResourcesInfo() [P][R:MED][C:COMPLEX]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-3
:END:

- [ ] Track active libuv handles (timers, sockets, etc.)
- [ ] Return array of resource descriptions
- [ ] Include handle types (TCPSocket, Timer, FSRequest, etc.)
- [ ] Hook into libuv handle lifecycle
- [ ] Test: Verify resource tracking

**** TODO [#B] Task 6.3: Implement process.setSourceMapsEnabled(val) [P][R:LOW][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Store source map enabled flag
- [ ] Update process.sourceMapsEnabled getter
- [ ] Hook into error stack trace generation (if source map support exists)
- [ ] Stub if source maps not implemented
- [ ] Test: Enable/disable source maps

**** TODO [#C] Task 6.4: Implement process.ref() [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Create libuv handle for process object
- [ ] Call uv_ref() to keep event loop alive
- [ ] Test: Verify loop doesn't exit prematurely

**** TODO [#C] Task 6.5: Implement process.unref() [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Call uv_unref() on process handle
- [ ] Allow event loop to exit
- [ ] Test: Verify loop exits when unref'd

**** TODO [#B] Task 6.6: Wire up advanced features [S][R:LOW][C:SIMPLE][D:6.1,6.2,6.3,6.4,6.5]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 6.1,6.2,6.3,6.4,6.5
:END:

- [ ] Update module.c
- [ ] Register all advanced functions
- [ ] Update process.h
- [ ] Test: All APIs accessible

**** TODO [#B] Task 6.7: Create unit tests for loadEnvFile [S][R:MED][C:MEDIUM][D:6.1]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 6.1
:END:

- [ ] Create test/jsrt/test_jsrt_process_env_file.js
- [ ] Test basic KEY=VALUE parsing
- [ ] Test quoted values
- [ ] Test comments
- [ ] Test variable expansion
- [ ] Test error handling
- [ ] Run: make test N=jsrt/test_jsrt_process_env_file

**** TODO [#B] Task 6.8: Create unit tests for active resources [S][R:MED][C:MEDIUM][D:6.2]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 6.2
:END:

- [ ] Test getActiveResourcesInfo() returns array
- [ ] Create timer, verify in list
- [ ] Create socket, verify in list
- [ ] Close resources, verify removed
- [ ] Run: make test N=jsrt/test_jsrt_process_resources

**** TODO [#B] Task 6.9: Create unit tests for ref/unref [S][R:LOW][C:SIMPLE][D:6.4,6.5]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 6.4,6.5
:END:

- [ ] Test ref() keeps loop alive
- [ ] Test unref() allows exit
- [ ] Verify interaction with timers
- [ ] Run: make test N=jsrt/test_jsrt_process

*** Acceptance Criteria
- [ ] loadEnvFile() works correctly
- [ ] Active resource tracking functional
- [ ] ref()/unref() control event loop
- [ ] All tests pass
- [ ] Code formatted: make format

** TODO [#C] Phase 7: Report & Permission APIs (Future Stubs) [S][R:LOW][C:SIMPLE][D:phase-1] :future:
:PROPERTIES:
:ID: phase-7
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:PROGRESS: 0/10
:COMPLETION: 0%
:END:

Create stub implementations for future enhancement (diagnostic reports, permissions, finalization).

*** Tasks

**** TODO [#C] Task 7.1: Stub process.report APIs [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Create process.report object
- [ ] Stub: report.writeReport([filename][, err])
- [ ] Stub: report.getReport([err])
- [ ] Stub: report.directory, filename, reportOnFatalError properties
- [ ] Stub: report.reportOnSignal, reportOnUncaughtException, signal
- [ ] All return null or empty values for now

**** TODO [#C] Task 7.2: Stub process.permission APIs [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Create process.permission object
- [ ] Stub: permission.has(scope[, reference])
- [ ] Always return true (no permission system yet)
- [ ] Document as future enhancement

**** TODO [#C] Task 7.3: Stub process.finalization APIs [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Create process.finalization object
- [ ] Stub: finalization.register(ref, callback)
- [ ] Stub: finalization.unregister(token)
- [ ] No-op implementations for now

**** TODO [#C] Task 7.4: Stub process.dlopen() [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Create dlopen function
- [ ] Throw "Not implemented" error
- [ ] Document as future native addon support

**** TODO [#C] Task 7.5: Stub process.getBuiltinModule() [P][R:LOW][C:TRIVIAL]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1
:END:

- [ ] Create getBuiltinModule function
- [ ] Return builtin module if exists
- [ ] Return null if not builtin
- [ ] Integrate with module loader

**** TODO [#C] Task 7.6: Wire up stubs [S][R:LOW][C:TRIVIAL][D:7.1,7.2,7.3,7.4,7.5]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 7.1,7.2,7.3,7.4,7.5
:END:

- [ ] Update module.c
- [ ] Register stub functions
- [ ] Document stub status in comments

*** Acceptance Criteria
- [ ] All stub APIs present and documented
- [ ] No crashes on stub API calls
- [ ] Clear "not implemented" messages where appropriate

** TODO [#A] Phase 8: Documentation & Comprehensive Testing [S][R:LOW][C:MEDIUM][D:phase-1,phase-2,phase-3,phase-4,phase-5,phase-6] :testing:docs:
:PROPERTIES:
:ID: phase-8
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:PROGRESS: 0/20
:COMPLETION: 0%
:END:

Create comprehensive documentation, integration tests, and cross-platform validation.

*** Tasks

**** TODO [#A] Task 8.1: Create API documentation [P][R:LOW][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:END:

- [ ] Document all implemented process APIs
- [ ] Create docs/api/process.md
- [ ] Include usage examples
- [ ] Document platform-specific behavior
- [ ] Document stub APIs and limitations

**** TODO [#A] Task 8.2: Create comprehensive integration tests [P][R:MED][C:COMPLEX]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:END:

- [ ] Create test/integration/test_process_integration.js
- [ ] Test property access patterns
- [ ] Test event combinations (beforeExit + exit, signals + exceptions)
- [ ] Test resource cleanup
- [ ] Test IPC + signals interaction
- [ ] Run: make test N=integration/test_process_integration

**** TODO [#A] Task 8.3: Cross-platform validation (Linux) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:END:

- [ ] Run all tests on Ubuntu 20.04+
- [ ] Run all tests on Debian
- [ ] Verify all Unix-specific APIs functional
- [ ] Verify signal handling
- [ ] Document any platform quirks

**** TODO [#A] Task 8.4: Cross-platform validation (macOS) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:END:

- [ ] Run all tests on macOS 12+
- [ ] Verify Unix APIs functional
- [ ] Test signal handling (BSD signals)
- [ ] Document platform differences

**** TODO [#A] Task 8.5: Cross-platform validation (Windows) [P][R:MED][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:END:

- [ ] Run all tests on Windows 10+
- [ ] Verify Unix APIs gracefully absent
- [ ] Test Windows-specific signal behavior (SIGBREAK)
- [ ] Test resource monitoring on Windows
- [ ] Document Windows limitations

**** TODO [#A] Task 8.6: WPT regression testing [P][R:HIGH][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:END:

- [ ] Run full WPT suite: make wpt
- [ ] Verify no new failures introduced
- [ ] Document baseline if changed
- [ ] Fix any regressions

**** TODO [#A] Task 8.7: Memory safety full validation [P][R:HIGH][C:COMPLEX]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:END:

- [ ] Build ASAN: make jsrt_m
- [ ] Run entire test suite with ASAN
- [ ] Fix all memory leaks
- [ ] Fix all use-after-free
- [ ] Fix all buffer overflows
- [ ] Test: ASAN_OPTIONS=detect_leaks=1 make test

**** TODO [#A] Task 8.8: Performance benchmarking [P][R:LOW][C:MEDIUM]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:END:

- [ ] Create benchmarks for property access
- [ ] Benchmark cpuUsage() overhead
- [ ] Benchmark memoryUsage() overhead
- [ ] Benchmark event emission
- [ ] Compare with Node.js baseline
- [ ] Document performance characteristics

**** TODO [#A] Task 8.9: Create migration guide [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: phase-1,phase-2,phase-3,phase-4,phase-5,phase-6
:END:

- [ ] Document Node.js compatibility status
- [ ] List implemented APIs
- [ ] List stub/unimplemented APIs
- [ ] Provide workarounds for missing features
- [ ] Create docs/migration/node-process.md

**** TODO [#A] Task 8.10: Final verification checklist [S][R:HIGH][C:MEDIUM][D:8.1,8.2,8.3,8.4,8.5,8.6,8.7,8.8,8.9]
:PROPERTIES:
:CREATED: 2025-10-30T00:00:00Z
:DEPS: 8.1,8.2,8.3,8.4,8.5,8.6,8.7,8.8,8.9
:END:

- [ ] All unit tests pass: make test
- [ ] All WPT tests pass: make wpt
- [ ] Code formatted: make format
- [ ] No ASAN errors: make jsrt_m && ASAN_OPTIONS=detect_leaks=1 make test
- [ ] Documentation complete
- [ ] Cross-platform validated
- [ ] Performance acceptable
- [ ] Ready for production use

*** Acceptance Criteria
- [ ] Complete API documentation
- [ ] All tests pass on all platforms
- [ ] No WPT regressions
- [ ] No memory safety issues
- [ ] Performance benchmarked
- [ ] Migration guide complete

* 🚀 Execution Dashboard
:PROPERTIES:
:CURRENT_PHASE: Phase 1 (Planning Complete)
:PROGRESS: 0/153
:COMPLETION: 0%
:ACTIVE_TASK: None (awaiting start)
:UPDATED: 2025-10-30T00:00:00Z
:END:

** Current Status
- Phase: Planning Complete, ready for Phase 1 execution
- Progress: 0/153 tasks (0%)
- Active: Awaiting user approval to begin

** Next Steps (Priority Order)
1. [ ] Phase 1: Missing Properties Implementation (35 tasks)
   - Start with Task 1.1: Setup properties.c infrastructure
   - High impact: Foundational for other phases
   - Low risk: Simple property getters/setters

2. [ ] Phase 2: Process Control & Signal Events (42 tasks)
   - Depends on: Phase 1 complete
   - High priority: Core Node.js functionality
   - Medium risk: Signal handling complexity

3. [ ] Phase 3: Memory & Resource Monitoring (25 tasks)
   - Can run parallel to Phase 2 (independent)
   - High value: Performance monitoring
   - Medium complexity

** Parallel Execution Opportunities
- Phase 1 and Phase 3 can be developed simultaneously (no dependencies)
- Phase 4 (Timing) can start after Phase 1 (independent of Phase 2)
- Phase 5 (Unix permissions) independent of Phase 2-4
- Phase 6 (Advanced) depends on Phase 2 and 3

** Risk Mitigation
- Phase 2 (Signals): High complexity, allocate extra time for testing
- Phase 8 (Cross-platform): Test early and often on all platforms
- Memory safety: Run ASAN frequently throughout development

* 📜 History & Updates
:LOGBOOK:
- State "PLANNING" from "" [2025-10-30T00:00:00Z] \\
  Created comprehensive task breakdown plan for Node.js process API implementation
- Note taken on [2025-10-30T00:00:00Z] \\
  Plan structure: 8 phases, 153 total tasks, organized by priority and dependencies
- Note taken on [2025-10-30T00:00:00Z] \\
  Verified existing implementation: 26 APIs already implemented in /repo/src/node/process/
- Note taken on [2025-10-30T00:00:00Z] \\
  Identified missing APIs: 15 properties, 29 methods, 8+ events to implement
- Note taken on [2025-10-30T00:00:00Z] \\
  Strategy: Follow existing code patterns, maintain cross-platform support, comprehensive testing
:END:

** Recent Changes
| Timestamp           | Action  | Task/Phase | Details                                    |
|---------------------|---------|------------|--------------------------------------------|
| 2025-10-30T00:00:00 | Created | Plan       | Initial comprehensive plan document        |
| 2025-10-30T00:00:00 | Analyze | All        | Code analysis of existing implementation   |
| 2025-10-30T00:00:00 | Define  | All Phases | 8 phases with 153 tasks defined            |
| 2025-10-30T00:00:00 | Ready   | Phase 1    | Phase 1 ready to start on user approval    |

** Blockers & Resolutions
*None currently*

** Key Decisions
1. **File Organization**: Follow existing pattern (properties.c, signals.c, resources.c, etc.)
2. **Platform Support**: Primary Unix/Linux, Windows where possible, graceful stubs otherwise
3. **Testing Strategy**: Unit tests per phase + comprehensive integration tests in Phase 8
4. **EventEmitter**: Reuse existing event system from /repo/src/node/events/
5. **Signal Handling**: Use libuv signal handlers (uv_signal_t) for cross-platform support
6. **Stub APIs**: Clearly document as future enhancement, no-op or throw "Not implemented"

** Implementation Notes
- Existing code base: `/repo/src/node/process/` (19 files already implemented)
- Test location: `/repo/test/jsrt/test_jsrt_process_*.js`
- Build system: CMakeLists.txt (already configured for process module)
- Dependencies: QuickJS, libuv (already available)
- Platform abstraction: process_platform.c (already established pattern)
- Memory management: Follow QuickJS patterns (JS_FreeValue, js_malloc, js_free)
- Mandatory checks: `make format && make test && make wpt` before each commit

** Complexity Estimates
- **TRIVIAL**: 15 tasks (stubs, simple getters)
- **SIMPLE**: 72 tasks (basic implementations, straightforward APIs)
- **MEDIUM**: 48 tasks (platform-specific, moderate logic)
- **COMPLEX**: 18 tasks (signals, events, exceptions, integration)

** Risk Assessment
- **LOW**: 95 tasks (properties, basic methods, stubs)
- **MEDIUM**: 50 tasks (signals, resources, Unix permissions)
- **HIGH**: 8 tasks (exception handling, WPT regression, memory safety)

** Estimated Effort by Phase
1. Phase 1 (Properties): ~2-3 days (35 tasks, mostly simple)
2. Phase 2 (Signals/Events): ~4-5 days (42 tasks, complex)
3. Phase 3 (Resources): ~2-3 days (25 tasks, medium)
4. Phase 4 (Timing): ~1 day (8 tasks, simple)
5. Phase 5 (Unix): ~2 days (28 tasks, Unix-only)
6. Phase 6 (Advanced): ~2-3 days (15 tasks, medium-complex)
7. Phase 7 (Stubs): ~0.5 days (10 tasks, trivial)
8. Phase 8 (Testing/Docs): ~3-4 days (20 tasks, validation-heavy)

**Total Estimated Effort**: ~17-23 days (depends on parallel execution and platform access)

---
*End of Plan Document*
*Last Updated: 2025-10-30T00:00:00Z*
*Status: PLANNING COMPLETE - Ready for Phase 1 Execution*
