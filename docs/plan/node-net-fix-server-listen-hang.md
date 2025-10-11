#+TITLE: Task Plan: Fix server.listen() Hang Issue
#+AUTHOR: Claude AI Assistant
#+DATE: 2025-10-11T00:00:00Z
#+STARTUP: overview indent
#+TODO: TODO IN-PROGRESS BLOCKED | DONE CANCELLED
#+PRIORITIES: A C B

* Task Metadata
:PROPERTIES:
:CREATED: 2025-10-11T00:00:00Z
:UPDATED: 2025-10-11T00:00:00Z
:STATUS: 🟡 PLANNING
:PROGRESS: 0/47
:COMPLETION: 0%
:END:

* 📋 Task Analysis & Breakdown

** L0 Main Task
- **Requirement**: Fix runtime hang after server.listen() call where JavaScript execution stops even though C function returns successfully
- **Success Criteria**:
  - JavaScript code continues executing after server.listen() returns
  - All tests in test/node/net/test_properties.js pass
  - make test completes successfully without hanging
  - make wpt passes with no regressions
  - No memory leaks detected with AddressSanitizer
- **Constraints**:
  - Must maintain backward compatibility with existing working code
  - Must follow jsrt development guidelines (format, test, wpt)
  - Must not break any currently passing tests
  - Must handle both synchronous and asynchronous event emission correctly

** Core Problem Analysis
The symptom shows JavaScript execution stops after C function js_server_listen() returns:
- console.log('BEFORE listen') ✓ executes
- js_server_listen() ✓ completes and returns
- console.log('AFTER listen') ✗ never executes
- Program ✗ hangs indefinitely

Debug evidence confirms the C function completes all operations including emitting the 'listening' event, but JavaScript execution context is not restored.

** Suspected Root Causes (To Investigate)
1. EventEmitter initialization loading 'node:events' module may interfere with execution flow
2. Event loop timing in JSRT_RuntimeAwaitEvalResult() (50 cycles) may be insufficient or incorrectly timed
3. Synchronous event emission from C may cause context confusion
4. JavaScript execution stack may not be properly restored after native function call
5. Event loop may be waiting for something that never completes

** L1 Epic Phases

*** Phase 1: Root Cause Investigation [S][R:MED][C:MEDIUM]
:PROPERTIES:
:ID: phase-1
:CREATED: 2025-10-11T00:00:00Z
:DEPS: None
:TASKS: 12
:END:
Systematically investigate each suspected cause to identify the actual root cause.

*** Phase 2: Fix Implementation [S][R:HIGH][C:COMPLEX][D:phase-1]
:PROPERTIES:
:ID: phase-2
:CREATED: 2025-10-11T00:00:00Z
:DEPS: phase-1
:TASKS: 15
:END:
Implement the fix based on root cause findings, with proper error handling and validation.

*** Phase 3: Testing & Validation [PS][R:MED][C:MEDIUM][D:phase-2]
:PROPERTIES:
:ID: phase-3
:CREATED: 2025-10-11T00:00:00Z
:DEPS: phase-2
:TASKS: 12
:END:
Comprehensive testing to ensure fix works and no regressions introduced.

*** Phase 4: Edge Cases & Cleanup [P][R:LOW][C:SIMPLE][D:phase-3]
:PROPERTIES:
:ID: phase-4
:CREATED: 2025-10-11T00:00:00Z
:DEPS: phase-3
:TASKS: 8
:END:
Handle edge cases, memory leak verification, and final cleanup.

* 📝 Task Execution

** TODO [#A] Phase 1: Root Cause Investigation [S][R:MED][C:MEDIUM] :investigation:
:PROPERTIES:
:ID: phase-1
:CREATED: 2025-10-11T00:00:00Z
:DEPS: None
:PROGRESS: 0/12
:COMPLETION: 0%
:END:

*** TODO [#A] Task 1.1: Baseline verification [P][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 1.1
:CREATED: 2025-10-11T00:00:00Z
:DEPS: None
:END:
Verify current state and establish baseline before making changes.

**** Subtasks:
- Run make test and capture current failure behavior
- Run make wpt and save baseline results
- Document current test pass/fail counts
- Build debug version with make jsrt_g
- Run test_properties.js standalone to reproduce hang

**** Success Criteria:
- Baseline test results documented
- Hang behavior confirmed reproducible
- Debug build available

*** TODO [#A] Task 1.2: Create minimal reproduction case [S][R:LOW][C:SIMPLE][D:1.1]
:PROPERTIES:
:ID: 1.2
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.1
:END:
Create smallest possible script that reproduces the hang issue.

**** Subtasks:
- Create target/tmp/minimal_listen_test.js with just server.listen()
- Test with various listen configurations (port 0, specific port, callback)
- Test with and without event listeners
- Document which configurations hang

**** Success Criteria:
- Minimal test case created (< 20 lines)
- Hang behavior confirmed in minimal case
- Different configurations tested and documented

*** TODO [#A] Task 1.3: Investigate EventEmitter module loading [P][R:MED][C:MEDIUM][D:1.2]
:PROPERTIES:
:ID: 1.3
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.2
:END:
Check if loading 'node:events' module during add_event_emitter_methods() causes execution issues.

**** Subtasks:
- Add debug logging to add_event_emitter_methods() entry/exit
- Check if JS_Eval() for module loading affects execution context
- Test if module loading happens during or after script eval
- Create test that doesn't use EventEmitter to compare behavior

**** Success Criteria:
- Module loading timing documented
- Execution context state verified before/after module load
- Comparison test results available

*** TODO [#A] Task 1.4: Investigate event loop timing [P][R:MED][C:MEDIUM][D:1.2]
:PROPERTIES:
:ID: 1.4
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.2
:END:
Analyze JSRT_RuntimeAwaitEvalResult() and its 50-cycle event loop behavior.

**** Subtasks:
- Add debug logging to JSRT_RuntimeAwaitEvalResult() showing cycle count
- Check if 50 cycles is reached or if it exits early
- Verify JSRT_RuntimeRun() return values during cycles
- Test with different cycle counts (10, 100, 1000)

**** Success Criteria:
- Event loop cycle behavior documented
- Exit conditions verified
- Alternative cycle counts tested

*** TODO [#A] Task 1.5: Investigate event emission timing [P][R:HIGH][C:COMPLEX][D:1.2]
:PROPERTIES:
:ID: 1.5
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.2
:END:
Analyze how emitting 'listening' event from C affects JavaScript execution flow.

**** Subtasks:
- Add detailed logging in js_server_listen() around emit() call
- Check JS_Call() return value for emit() invocation
- Verify JavaScript callback execution if listener is attached
- Test synchronous vs asynchronous emission patterns
- Check if emit() call blocks or returns immediately

**** Success Criteria:
- Emit() call behavior fully documented
- Callback execution timing verified
- Synchronous/async patterns identified

*** TODO [#A] Task 1.6: Investigate JavaScript execution stack [P][R:HIGH][C:COMPLEX][D:1.2]
:PROPERTIES:
:ID: 1.6
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.2
:END:
Check if JavaScript execution context is properly maintained after native function returns.

**** Subtasks:
- Add logging before/after js_server_listen() call in JavaScript
- Check QuickJS execution stack depth during native call
- Verify ctx state is valid when returning from C function
- Test if JS_UpdateStackTop() or similar is needed
- Compare with working native functions (e.g., net.connect)

**** Success Criteria:
- Execution stack state documented
- Context validity verified
- Comparison with working functions completed

*** TODO [#A] Task 1.7: Check pending jobs queue [P][R:MED][C:MEDIUM][D:1.3,1.4,1.5,1.6]
:PROPERTIES:
:ID: 1.7
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.3,1.4,1.5,1.6
:END:
Investigate if QuickJS pending jobs queue is causing execution to pause.

**** Subtasks:
- Check JS_IsJobPending() status after listen() call
- Add logging for JS_ExecutePendingJob() calls
- Verify all pending jobs are executed before script continues
- Test if manual job execution resolves the hang

**** Success Criteria:
- Pending jobs state documented
- Job execution behavior verified
- Manual execution test results available

*** TODO [#A] Task 1.8: Investigate libuv event loop integration [P][R:MED][C:MEDIUM][D:1.4]
:PROPERTIES:
:ID: 1.8
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.4
:END:
Check if libuv event loop and QuickJS runtime are properly synchronized.

**** Subtasks:
- Verify uv_run() is called correctly in JSRT_RuntimeRun()
- Check if UV_RUN_NOWAIT vs UV_RUN_ONCE affects behavior
- Add logging for libuv active handles/requests
- Test if server handle keeps event loop alive unexpectedly

**** Success Criteria:
- Event loop modes tested
- Active handles count documented
- Synchronization behavior verified

*** TODO [#A] Task 1.9: Check for infinite wait conditions [P][R:HIGH][C:MEDIUM][D:1.7,1.8]
:PROPERTIES:
:ID: 1.9
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.7,1.8
:END:
Identify if any component is waiting for a condition that never occurs.

**** Subtasks:
- Review all blocking calls in execution path
- Check for mutex/condition variable issues
- Verify no circular wait dependencies
- Test with timeout mechanisms

**** Success Criteria:
- All wait conditions identified
- Blocking patterns documented
- No circular dependencies found

*** TODO [#A] Task 1.10: Compare with working server code [P][R:LOW][C:SIMPLE][D:1.1]
:PROPERTIES:
:ID: 1.10
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.1
:END:
Compare hanging test with working server examples to identify differences.

**** Subtasks:
- Find working server examples in test/ or examples/
- Run working examples to confirm they don't hang
- Identify code differences between working and hanging cases
- Document pattern differences

**** Success Criteria:
- Working examples identified
- Differences documented
- Pattern analysis complete

*** TODO [#A] Task 1.11: Analyze HTTP server for comparison [P][R:LOW][C:SIMPLE][D:1.10]
:PROPERTIES:
:ID: 1.11
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.10
:END:
Check how HTTP server's listen() implementation differs from net server.

**** Subtasks:
- Review src/node/http/ listen implementation
- Compare HTTP vs net event emission patterns
- Test if HTTP server has same hang issue
- Document implementation differences

**** Success Criteria:
- HTTP implementation reviewed
- Comparison documented
- Test results available

*** TODO [#A] Task 1.12: Root cause identification [S][R:HIGH][C:COMPLEX][D:1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,1.11]
:PROPERTIES:
:ID: 1.12
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.3,1.4,1.5,1.6,1.7,1.8,1.9,1.10,1.11
:END:
Synthesize investigation findings to identify definitive root cause.

**** Subtasks:
- Review all investigation results
- Identify the exact point where execution stops
- Determine why JavaScript context is not restored
- Document root cause with evidence
- Create fix strategy based on root cause

**** Success Criteria:
- Root cause identified with confidence
- Evidence from multiple investigations
- Fix strategy outlined
- Risks and alternatives documented

** TODO [#A] Phase 2: Fix Implementation [S][R:HIGH][C:COMPLEX][D:phase-1] :implementation:
:PROPERTIES:
:ID: phase-2
:CREATED: 2025-10-11T00:00:00Z
:DEPS: phase-1
:PROGRESS: 0/15
:COMPLETION: 0%
:END:

*** TODO [#A] Task 2.1: Design fix approach [S][R:HIGH][C:COMPLEX][D:1.12]
:PROPERTIES:
:ID: 2.1
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 1.12
:END:
Design the fix based on identified root cause, considering all constraints.

**** Subtasks:
- Document fix approach with rationale
- Identify all files that need modification
- Plan backward compatibility strategy
- Design error handling approach
- Review with project patterns and conventions

**** Success Criteria:
- Fix design documented
- File modification list ready
- Compatibility verified
- Error handling planned

*** TODO [#A] Task 2.2: Create fix branch and backup [S][R:LOW][C:TRIVIAL][D:2.1]
:PROPERTIES:
:ID: 2.2
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.1
:END:
Prepare environment for fix implementation.

**** Subtasks:
- Save current baseline test results
- Create backups of files to be modified
- Ensure debug build is ready

**** Success Criteria:
- Baseline saved
- Backups created
- Clean build available

*** TODO [#A] Task 2.3: Implement core fix [S][R:HIGH][C:COMPLEX][D:2.2]
:PROPERTIES:
:ID: 2.3
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.2
:END:
Implement the primary fix based on root cause analysis.

**** Subtasks:
- Modify identified files per design
- Add necessary error handling
- Preserve existing functionality
- Add inline comments explaining fix
- Follow jsrt coding conventions

**** Success Criteria:
- Code changes implemented
- Error handling added
- Comments added
- Conventions followed

*** TODO [#A] Task 2.4: Fix event loop timing if needed [S][R:MED][C:MEDIUM][D:2.3]
:PROPERTIES:
:ID: 2.4
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.3
:END:
Adjust JSRT_RuntimeAwaitEvalResult() if timing is part of the issue.

**** Subtasks:
- Modify event loop cycle handling
- Adjust exit conditions
- Add proper synchronization
- Document changes

**** Success Criteria:
- Event loop behavior corrected
- Synchronization proper
- Changes documented

*** TODO [#A] Task 2.5: Fix EventEmitter integration if needed [S][R:MED][C:MEDIUM][D:2.3]
:PROPERTIES:
:ID: 2.5
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.3
:END:
Fix add_event_emitter_methods() if module loading causes issues.

**** Subtasks:
- Adjust module loading timing
- Fix context handling
- Ensure proper cleanup
- Add error checks

**** Success Criteria:
- Module loading fixed
- Context handling correct
- Cleanup proper
- Error handling added

*** TODO [#A] Task 2.6: Fix event emission if needed [S][R:HIGH][C:COMPLEX][D:2.3]
:PROPERTIES:
:ID: 2.6
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.3
:END:
Fix emit() call in js_server_listen() if synchronous emission is problematic.

**** Subtasks:
- Adjust event emission timing
- Handle asynchronous emission properly
- Ensure callbacks execute correctly
- Verify execution flow continues

**** Success Criteria:
- Event emission fixed
- Callbacks work correctly
- Execution flow restored

*** TODO [#A] Task 2.7: Fix JavaScript execution context if needed [S][R:HIGH][C:COMPLEX][D:2.3]
:PROPERTIES:
:ID: 2.7
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.3
:END:
Fix execution stack/context restoration after native call.

**** Subtasks:
- Add proper context save/restore
- Ensure stack is maintained
- Verify QuickJS state is valid
- Test context across native calls

**** Success Criteria:
- Context properly maintained
- Stack handling correct
- State validity verified

*** TODO [#A] Task 2.8: Handle pending jobs properly [S][R:MED][C:MEDIUM][D:2.6,2.7]
:PROPERTIES:
:ID: 2.8
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.6,2.7
:END:
Ensure QuickJS pending jobs are executed at correct time.

**** Subtasks:
- Add job execution after listen() if needed
- Ensure proper job queue handling
- Test job execution timing
- Document job handling strategy

**** Success Criteria:
- Jobs execute properly
- Timing correct
- Strategy documented

*** TODO [#A] Task 2.9: Add debug logging for troubleshooting [P][R:LOW][C:SIMPLE][D:2.3]
:PROPERTIES:
:ID: 2.9
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.3
:END:
Add appropriate debug logging to aid future troubleshooting.

**** Subtasks:
- Add DEBUG_LOG statements at key points
- Log execution flow through fix
- Log error conditions
- Keep logging minimal for production

**** Success Criteria:
- Debug logs added
- Execution flow traceable
- Production impact minimal

*** TODO [#A] Task 2.10: Build and verify compilation [S][R:LOW][C:SIMPLE][D:2.4,2.5,2.6,2.7,2.8,2.9]
:PROPERTIES:
:ID: 2.10
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.4,2.5,2.6,2.7,2.8,2.9
:END:
Compile the fix and verify no build errors.

**** Subtasks:
- Run make clean
- Run make jsrt_g (debug build)
- Fix any compilation errors
- Verify no warnings

**** Success Criteria:
- Clean build successful
- No compilation errors
- No warnings

*** TODO [#A] Task 2.11: Test minimal reproduction case [S][R:HIGH][C:SIMPLE][D:2.10]
:PROPERTIES:
:ID: 2.11
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.10
:END:
Test the fix with minimal reproduction case first.

**** Subtasks:
- Run target/tmp/minimal_listen_test.js
- Verify no hang occurs
- Verify execution continues after listen()
- Check all console.log statements execute

**** Success Criteria:
- No hang with minimal case
- All statements execute
- Fix behavior confirmed

*** TODO [#A] Task 2.12: Test with original failing test [S][R:HIGH][C:MEDIUM][D:2.11]
:PROPERTIES:
:ID: 2.12
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.11
:END:
Test with test/node/net/test_properties.js.

**** Subtasks:
- Run test_properties.js standalone
- Verify no hang
- Check test output for correctness
- Note any remaining test failures (not hangs)

**** Success Criteria:
- No hang with full test
- Script completes execution
- Output appears correct

*** TODO [#A] Task 2.13: Run make test [S][R:HIGH][C:SIMPLE][D:2.12]
:PROPERTIES:
:ID: 2.13
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.12
:END:
Run full test suite to check for regressions.

**** Subtasks:
- Run make test
- Compare results with baseline
- Document any new failures
- Verify no new hangs introduced

**** Success Criteria:
- make test completes (no hangs)
- No new test failures vs baseline
- Results documented

*** TODO [#A] Task 2.14: Run make wpt [S][R:MED][C:SIMPLE][D:2.12]
:PROPERTIES:
:ID: 2.14
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.12
:END:
Run Web Platform Tests to check for compliance regressions.

**** Subtasks:
- Run make wpt
- Compare results with baseline
- Document any new failures
- Verify no regressions

**** Success Criteria:
- make wpt completes
- Failure count ≤ baseline
- No compliance regressions

*** TODO [#A] Task 2.15: Iterate on fix if tests fail [S][R:HIGH][C:COMPLEX][D:2.13,2.14]
:PROPERTIES:
:ID: 2.15
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.13,2.14
:END:
Refine fix if any tests fail or regressions occur.

**** Subtasks:
- Analyze test failures
- Identify issues with fix
- Refine implementation
- Re-test until passing
- Document iteration findings

**** Success Criteria:
- All tests pass or match baseline
- Fix refined as needed
- Iteration documented

** TODO [#B] Phase 3: Testing & Validation [PS][R:MED][C:MEDIUM][D:phase-2] :testing:
:PROPERTIES:
:ID: phase-3
:CREATED: 2025-10-11T00:00:00Z
:DEPS: phase-2
:PROGRESS: 0/12
:COMPLETION: 0%
:END:

*** TODO [#B] Task 3.1: Test with various listen configurations [P][R:MED][C:SIMPLE][D:2.15]
:PROPERTIES:
:ID: 3.1
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.15
:END:
Ensure fix works with all listen() parameter variations.

**** Subtasks:
- Test listen(port) with various ports
- Test listen(port, host)
- Test listen(port, callback)
- Test listen(port, host, callback)
- Test listen({ port, host })
- Test listen(0) for random port

**** Success Criteria:
- All configurations work
- No hangs
- All behave correctly

*** TODO [#B] Task 3.2: Test with event listeners [P][R:MED][C:SIMPLE][D:2.15]
:PROPERTIES:
:ID: 3.2
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.15
:END:
Verify event emission and listener callbacks work correctly.

**** Subtasks:
- Test with 'listening' event listener
- Test with multiple listeners
- Test listener callback execution
- Test event data correctness

**** Success Criteria:
- Listeners triggered correctly
- Callbacks execute
- Event data correct

*** TODO [#B] Task 3.3: Test without event listeners [P][R:LOW][C:SIMPLE][D:2.15]
:PROPERTIES:
:ID: 3.3
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.15
:END:
Ensure fix works when no listeners attached.

**** Subtasks:
- Test listen() with no 'listening' listener
- Verify execution continues
- Check for any errors

**** Success Criteria:
- Works without listeners
- Execution continues
- No errors

*** TODO [#B] Task 3.4: Test server connection acceptance [P][R:MED][C:MEDIUM][D:2.15]
:PROPERTIES:
:ID: 3.4
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.15
:END:
Verify server actually accepts connections after fix.

**** Subtasks:
- Create test that listens and accepts connection
- Test with net.connect() to same port
- Verify 'connection' event fires
- Check data transfer works

**** Success Criteria:
- Connections accepted
- Events fire correctly
- Data transfer works

*** TODO [#B] Task 3.5: Test multiple server instances [P][R:MED][C:MEDIUM][D:2.15]
:PROPERTIES:
:ID: 3.5
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.15
:END:
Ensure multiple servers can listen simultaneously.

**** Subtasks:
- Create test with 2+ servers
- Test different ports
- Verify all listen correctly
- Test connections to each

**** Success Criteria:
- Multiple servers work
- No interference
- All functional

*** TODO [#B] Task 3.6: Test server close after listen [P][R:MED][C:SIMPLE][D:2.15]
:PROPERTIES:
:ID: 3.6
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.15
:END:
Verify server can be closed properly after listening.

**** Subtasks:
- Test server.close() after listen()
- Test with 'close' event listener
- Verify clean shutdown
- Check for resource leaks

**** Success Criteria:
- Close works correctly
- Events fire properly
- Resources cleaned up

*** TODO [#B] Task 3.7: Test error conditions [P][R:MED][C:MEDIUM][D:2.15]
:PROPERTIES:
:ID: 3.7
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.15
:END:
Ensure error handling works correctly.

**** Subtasks:
- Test listen on already-used port
- Test invalid port numbers
- Test invalid host names
- Verify error events fire
- Check error handling in fix

**** Success Criteria:
- Errors handled correctly
- Error events fire
- No crashes

*** TODO [#B] Task 3.8: Test with HTTP server [P][R:LOW][C:SIMPLE][D:2.15]
:PROPERTIES:
:ID: 3.8
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.15
:END:
Verify HTTP server still works (uses net.Server internally).

**** Subtasks:
- Run HTTP server tests
- Verify http.listen() works
- Check for any regressions
- Test HTTP request handling

**** Success Criteria:
- HTTP server works
- No regressions
- Requests handled correctly

*** TODO [#B] Task 3.9: Run all net module tests [PS][R:MED][C:SIMPLE][D:3.1,3.2,3.3,3.4,3.5,3.6,3.7]
:PROPERTIES:
:ID: 3.9
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 3.1,3.2,3.3,3.4,3.5,3.6,3.7
:END:
Run complete test suite for net module.

**** Subtasks:
- Run all test/node/net/test_*.js files
- Document results
- Fix any failures
- Verify all pass

**** Success Criteria:
- All net tests pass
- No hangs
- Behavior correct

*** TODO [#B] Task 3.10: Run complete make test [PS][R:HIGH][C:SIMPLE][D:3.9]
:PROPERTIES:
:ID: 3.10
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 3.9
:END:
Full test suite validation.

**** Subtasks:
- Run make test
- Verify completion without hangs
- Compare with baseline
- Document any differences

**** Success Criteria:
- make test completes
- Pass rate ≥ baseline
- No new failures

*** TODO [#B] Task 3.11: Run complete make wpt [PS][R:MED][C:SIMPLE][D:3.9]
:PROPERTIES:
:ID: 3.11
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 3.9
:END:
Web Platform Tests validation.

**** Subtasks:
- Run make wpt
- Compare with baseline
- Document any differences
- Verify no regressions

**** Success Criteria:
- make wpt completes
- Failure count ≤ baseline
- No compliance regressions

*** TODO [#B] Task 3.12: Verify release build [PS][R:MED][C:SIMPLE][D:3.10,3.11]
:PROPERTIES:
:ID: 3.12
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 3.10,3.11
:END:
Ensure fix works in optimized release build.

**** Subtasks:
- Run make clean
- Run make (release build)
- Run make test with release build
- Verify no optimization issues

**** Success Criteria:
- Release build succeeds
- Tests pass with release
- No optimization issues

** TODO [#B] Phase 4: Edge Cases & Cleanup [P][R:LOW][C:SIMPLE][D:phase-3] :cleanup:
:PROPERTIES:
:ID: phase-4
:CREATED: 2025-10-11T00:00:00Z
:DEPS: phase-3
:PROGRESS: 0/8
:COMPLETION: 0%
:END:

*** TODO [#B] Task 4.1: Memory leak verification [P][R:MED][C:SIMPLE][D:3.12]
:PROPERTIES:
:ID: 4.1
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 3.12
:END:
Verify no memory leaks introduced by fix.

**** Subtasks:
- Build with make jsrt_m (AddressSanitizer)
- Run test_properties.js with ASAN
- Run net module tests with ASAN
- Check ASAN reports
- Fix any leaks found

**** Success Criteria:
- No memory leaks detected
- ASAN reports clean
- All leaks fixed

*** TODO [#B] Task 4.2: Test rapid listen/close cycles [P][R:LOW][C:SIMPLE][D:3.12]
:PROPERTIES:
:ID: 4.2
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 3.12
:END:
Stress test with rapid server creation/destruction.

**** Subtasks:
- Create test with 100+ listen/close cycles
- Verify no resource exhaustion
- Check for memory leaks
- Ensure stable behavior

**** Success Criteria:
- Stress test passes
- No resource issues
- Stable behavior

*** TODO [#B] Task 4.3: Test concurrent operations [P][R:LOW][C:MEDIUM][D:3.12]
:PROPERTIES:
:ID: 4.3
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 3.12
:END:
Test listen() with other concurrent async operations.

**** Subtasks:
- Test listen() with timers running
- Test with pending promises
- Test with file I/O operations
- Verify no interference

**** Success Criteria:
- Concurrent ops work
- No interference
- All complete correctly

*** TODO [#B] Task 4.4: Document the fix [P][R:LOW][C:SIMPLE][D:2.1]
:PROPERTIES:
:ID: 4.4
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 2.1
:END:
Create documentation for the fix.

**** Subtasks:
- Document root cause in comments
- Update relevant documentation files
- Add notes to CHANGELOG if exists
- Document any API changes

**** Success Criteria:
- Fix documented
- Comments clear
- Documentation updated

*** TODO [#B] Task 4.5: Clean up debug logging [P][R:LOW][C:TRIVIAL][D:4.1,4.2,4.3]
:PROPERTIES:
:ID: 4.5
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 4.1,4.2,4.3
:END:
Remove or guard temporary debug logging.

**** Subtasks:
- Review all debug logging added
- Remove temporary logs
- Keep useful permanent logs under DEBUG_LOG
- Ensure clean output in production

**** Success Criteria:
- Temp logs removed
- Production output clean
- Useful logs retained

*** TODO [#B] Task 4.6: Run make format [P][R:LOW][C:TRIVIAL][D:4.5]
:PROPERTIES:
:ID: 4.6
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 4.5
:END:
Format all code per project guidelines.

**** Subtasks:
- Run make format
- Verify formatting applied
- Check for any format issues
- Commit formatting changes

**** Success Criteria:
- make format completes
- Code properly formatted
- No format issues

*** TODO [#B] Task 4.7: Final verification [P][R:MED][C:SIMPLE][D:4.6]
:PROPERTIES:
:ID: 4.7
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 4.6
:END:
Final complete check before considering fix complete.

**** Subtasks:
- Run make clean && make
- Run make test
- Run make wpt
- Verify all pass/match baseline
- Review all changes

**** Success Criteria:
- Clean build succeeds
- All tests pass/baseline
- WPT no regressions
- Changes reviewed

*** TODO [#B] Task 4.8: Create minimal test case for regression prevention [P][R:LOW][C:SIMPLE][D:4.7]
:PROPERTIES:
:ID: 4.8
:CREATED: 2025-10-11T00:00:00Z
:DEPS: 4.7
:END:
Add permanent test to prevent regression of this issue.

**** Subtasks:
- Create focused test case in test/node/net/
- Test specifically for the hang issue
- Keep test minimal and fast
- Add to test suite
- Document test purpose

**** Success Criteria:
- Test created
- Test passes
- Test added to suite
- Purpose documented

* 🚀 Execution Dashboard
:PROPERTIES:
:CURRENT_PHASE: Phase 1: Root Cause Investigation
:PROGRESS: 0/47
:COMPLETION: 0%
:ACTIVE_TASK: Not started
:UPDATED: 2025-10-11T00:00:00Z
:END:

** Current Status
- Phase: Phase 1: Root Cause Investigation
- Progress: 0/47 tasks (0%)
- Active: Not started - ready to begin with baseline verification

** Next Up (Phase 1 Parallel Tasks)
- [ ] Task 1.1: Baseline verification
- [ ] Task 1.2: Create minimal reproduction case (depends on 1.1)
- Then parallel investigation: 1.3, 1.4, 1.5, 1.6, 1.10, 1.11

** Critical Path
1.1 → 1.2 → [1.3,1.4,1.5,1.6] → 1.7 → 1.9 → 1.12 → Phase 2 → Phase 3 → Phase 4

** Risk Areas
- High Risk: Tasks 1.5, 1.6, 1.9, 2.1, 2.3, 2.6, 2.7 (complex investigation/implementation)
- Medium Risk: Tasks 1.3, 1.4, 1.7, 1.8, 2.4, 2.5, 2.8 (integration points)
- Low Risk: All testing and cleanup tasks

* 📜 History & Updates

** 2025-10-11T00:00:00Z - Plan Created
- Initial task breakdown completed
- 47 total tasks across 4 phases
- Plan saved to docs/plan/fix-server-listen-hang.md
- Ready to begin Phase 1 investigation

** Key Decisions
- Investigation-first approach: Spend time identifying exact root cause before implementing fix
- Parallel investigation strategy: Multiple hypotheses tested simultaneously
- Incremental testing: Test minimal case first, then full suite
- Safety-first: Memory leak checks and regression testing mandatory

** Assumptions
- Fix will be in one of: runtime.c, jsrt.c, net_server.c, or net_callbacks.c
- Existing tests are correct and should pass after fix
- Hang is deterministic and reproducible
- Fix should not require API changes

** Dependencies External to Tasks
- QuickJS API behavior (documented)
- libuv event loop semantics (documented)
- Existing test suite validity (assumed correct)

** Success Metrics
- Primary: make test completes without hanging
- Secondary: All net module tests pass
- Tertiary: No regressions in make wpt
- Quality: No memory leaks (ASAN clean)
- Process: All code formatted (make format)

---
**End of Plan Document**
