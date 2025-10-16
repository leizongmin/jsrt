#+TITLE: Node.js HTTP Module Completion Plan
#+TODO: TODO IN-PROGRESS BLOCKED | DONE CANCELLED
#+STARTUP: overview
#+FILETAGS: :node:http:completion:production:

* Task Metadata
:PROPERTIES:
:CREATED: [2025-10-16]
:LAST_UPDATED: [2025-10-16 10:00]
:STATUS: IN-PROGRESS
:PROGRESS: 0/15
:COMPLETION: 0%
:PRIORITY: A
:END:

** Document Information
- *Created*: 2025-10-16T10:00:00Z
- *Last Updated*: 2025-10-16T10:00:00Z
- *Status*: 🔵 IN-PROGRESS
- *Overall Progress*: 0/15 tasks (0%)
- *Test Status*: 198/200 passing (99%)

* 📋 Executive Summary

** Current Status

*** Test Results
- *Tests Passing*: 198/200 (99%)
- *Tests Failing*: 2 tests
  - test_stream_incoming.js (Test #9: POST body streaming)
  - test_stream_advanced.js (Test #2: IncomingMessage readable state)

*** Phase Completion
- Phase 1: ✅ DONE (100%) - Modular refactoring complete
- Phase 2: 🟡 97% (29/30) - Missing keep-alive connection reuse
- Phase 3: ✅ DONE (100%) - Client implementation complete
- Phase 4: 🟡 72% (18/25) - Core streaming done, advanced optional
- Phase 5: 🟡 68% (17/25) - Timeouts done, some optional
- Phase 6: ✅ DONE (100%) - Testing complete
- Phase 7: ⚪ 0% (0/10) - Documentation (can defer)

*** Critical Blocker
**POST Request Body Handling Bug**
- *Symptom*: Server never receives response after client sends POST data
- *Impact*: Blocks 2 tests from passing
- *Root Cause*: IncomingMessage 'end' event not emitted properly after body data
- *Location*: src/node/http/http_parser.c:on_message_complete(), http_incoming.c

** Objective
Complete the remaining 2% of mandatory HTTP module implementation to achieve 100% test pass rate and production-ready core HTTP server/client functionality.

** Success Criteria
- ✅ 200/200 tests passing (100%)
- ✅ POST body streaming works correctly
- ✅ Keep-alive connection reuse implemented
- ✅ All mandatory Phase 2-5 tasks completed
- ✅ make format && make test && make wpt all pass
- ✅ Zero memory leaks (ASAN validation)
- ✅ Plan document updated with three-level status tracking

* 🔍 Root Cause Analysis: POST Body Bug

** Problem Statement
Test #9 in test_stream_incoming.js sends a POST request with body data. The server handler sets up 'data' and 'end' event listeners on the IncomingMessage (req). However:
1. Client sends POST data via req.write() and req.end()
2. Server's 'data' event fires (receives body chunks)
3. Server's 'end' event NEVER fires
4. Server never calls res.end('OK')
5. Client hangs waiting for response
6. Test times out after 3 seconds

** Current Implementation Analysis

*** on_body() Callback (http_parser.c:490-504)
```c
int on_body(llhttp_t* parser, const char* at, size_t length) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;

  // Phase 4: Stream data to IncomingMessage instead of buffering
  if (!JS_IsUndefined(conn->current_request)) {
    js_http_incoming_push_data(conn->ctx, conn->current_request, at, length);
  }

  // Also keep accumulating for backwards compatibility (_body property)
  if (buffer_append(&conn->body_buffer, &conn->body_size,
                    &conn->body_capacity, at, length) != 0) {
    return -1;
  }

  return 0;
}
```
- ✅ Correctly calls js_http_incoming_push_data() to emit 'data' events
- ✅ Works: Server receives body data

*** on_message_complete() Callback (http_parser.c:507-553)
```c
int on_message_complete(llhttp_t* parser) {
  JSHttpConnection* conn = (JSHttpConnection*)parser->data;
  JSContext* ctx = conn->ctx;

  // Set body on request if any (backwards compatibility)
  if (conn->body_buffer && conn->body_size > 0) {
    JSValue body_str = JS_NewStringLen(ctx, conn->body_buffer, conn->body_size);
    JS_SetPropertyStr(ctx, conn->current_request, "_body", body_str);
  }

  // Phase 4: Signal end of stream to IncomingMessage
  if (!JS_IsUndefined(conn->current_request)) {
    js_http_incoming_end(ctx, conn->current_request);
  }

  // ... rest of code ...
  return 0;
}
```
- ✅ Correctly calls js_http_incoming_end() to emit 'end' event
- ❓ But 'end' event is not reaching JavaScript listener

*** js_http_incoming_end() (http_incoming.c)
Need to check this function's implementation for issues.

** Hypothesis
The 'end' event emission in js_http_incoming_end() may have one of these issues:
1. Event not emitted at all
2. Event emitted before listener is attached
3. readableEnded state not set correctly
4. Event emitter inheritance broken for IncomingMessage

** Debugging Steps Required
1. ✅ Read http_incoming.c to check js_http_incoming_end() implementation
2. Add debug logging to trace 'end' event emission
3. Verify event listener attachment timing
4. Check readableEnded property state transitions
5. Test with debug build: make jsrt_g && ./bin/jsrt_g test/...

* 🎯 Phase 0: Critical Bug Fix [0/5] [P:A]
:PROPERTIES:
:EXECUTION_MODE: SEQUENTIAL
:DEPENDENCIES: None
:COMPLEXITY: MEDIUM
:RISK: HIGH
:ESTIMATED_TIME: 2-3 hours
:END:

** TODO [#A] Task 0.1: Analyze js_http_incoming_end() implementation [S][R:LOW][C:SIMPLE]
:PROPERTIES:
:ID: 0.1
:CREATED: [2025-10-16]
:DEPS: None
:END:

*** Subtasks
- [ ] Read http_incoming.c to find js_http_incoming_end()
- [ ] Check if emit('end') is called
- [ ] Verify readableEnded = true is set
- [ ] Check for any error handling that might silently fail
- [ ] Document findings in this plan

** TODO [#A] Task 0.2: Add debug logging to trace event flow [S][R:LOW][C:SIMPLE][D:0.1]
:PROPERTIES:
:ID: 0.2
:CREATED: [2025-10-16]
:DEPS: 0.1
:END:

*** Subtasks
- [ ] Add JSRT_Debug() in on_body() when data arrives
- [ ] Add JSRT_Debug() in on_message_complete() before calling js_http_incoming_end()
- [ ] Add JSRT_Debug() in js_http_incoming_end() when emitting 'end'
- [ ] Add JSRT_Debug() in test to show when listeners are attached
- [ ] Build debug: make jsrt_g
- [ ] Run failing test: timeout 10 ./bin/jsrt_g test/node/http/server/test_stream_incoming.js

** TODO [#A] Task 0.3: Fix 'end' event emission bug [S][R:HIGH][C:MEDIUM][D:0.2]
:PROPERTIES:
:ID: 0.3
:CREATED: [2025-10-16]
:DEPS: 0.2
:END:

*** Root Causes (will be determined by 0.2)
- TBD after analysis

*** Subtasks
- [ ] Implement fix based on root cause analysis
- [ ] Ensure emit('end') is called with proper arguments
- [ ] Set readableEnded = true before emitting
- [ ] Handle case where no listeners are attached (should not block)
- [ ] Test fix: timeout 10 ./bin/jsrt test/node/http/server/test_stream_incoming.js

** TODO [#A] Task 0.4: Fix IncomingMessage readable state properties [S][R:MED][C:MEDIUM][D:0.3]
:PROPERTIES:
:ID: 0.4
:CREATED: [2025-10-16]
:DEPS: 0.3
:END:

*** Subtasks
- [ ] Ensure readable=true initially
- [ ] Set readableEnded=true after 'end' event
- [ ] Implement readableHighWaterMark (default 16384)
- [ ] Implement destroyed property
- [ ] Implement destroy() method
- [ ] Emit 'close' after destroy()
- [ ] Test: timeout 10 ./bin/jsrt test/node/http/server/test_stream_advanced.js

** TODO [#A] Task 0.5: Validate bug fix with full test suite [S][R:LOW][C:SIMPLE][D:0.4]
:PROPERTIES:
:ID: 0.5
:CREATED: [2025-10-16]
:DEPS: 0.4
:END:

*** Subtasks
- [ ] Run all HTTP tests: make test N=node/http
- [ ] Verify 200/200 tests passing
- [ ] Run ASAN build: make jsrt_m
- [ ] Check for memory leaks: ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/node/http/server/test_stream*.js
- [ ] Document fix in plan

* 🎯 Phase 1: Keep-Alive Connection Reuse [0/3] [P:A]
:PROPERTIES:
:EXECUTION_MODE: SEQUENTIAL
:DEPENDENCIES: Phase-0
:COMPLEXITY: MEDIUM
:RISK: MEDIUM
:ESTIMATED_TIME: 2-3 hours
:END:

** TODO [#A] Task 1.1: Analyze existing keep-alive implementation [S][R:LOW][C:SIMPLE][D:0.5]
:PROPERTIES:
:ID: 1.1
:CREATED: [2025-10-16]
:DEPS: 0.5
:END:

*** Current State
- Connection header parsing: ✅ Implemented in on_headers_complete()
- keep_alive flag: ✅ Set based on Connection header
- Parser reset: ✅ Implemented in on_message_complete()
- Socket reuse: ❌ NOT implemented

*** Subtasks
- [ ] Read http_parser.c keep-alive logic (lines 544-550)
- [ ] Check if parser is reset for next request: llhttp_init()
- [ ] Verify socket is NOT closed after response
- [ ] Test current behavior: Are connections kept alive?
- [ ] Document what's missing for full keep-alive

** TODO [#A] Task 1.2: Implement HTTP Agent connection pooling [S][R:MED][C:MEDIUM][D:1.1]
:PROPERTIES:
:ID: 1.2
:CREATED: [2025-10-16]
:DEPS: 1.1
:END:

*** Requirements
- Agent.maxSockets: Limit concurrent connections per host
- Agent.freeSockets: Track idle keep-alive connections
- Agent.requests: Queue pending requests when limit reached
- Connection reuse: Prefer existing socket over new connection

*** Subtasks
- [ ] Read http_client.c Agent implementation (find JSHTTPAgent struct)
- [ ] Implement socket pool data structure (hash map by host:port)
- [ ] Add socket to pool when response completes (if keep-alive)
- [ ] Reuse pooled socket for new requests to same host
- [ ] Implement maxSockets limit (default: Infinity)
- [ ] Implement request queueing when limit reached
- [ ] Handle socket close/error (remove from pool)
- [ ] Test: make test N=node/http/integration/test_keepalive

** TODO [#A] Task 1.3: Validate keep-alive with integration tests [S][R:LOW][C:SIMPLE][D:1.2]
:PROPERTIES:
:ID: 1.3
:CREATED: [2025-10-16]
:DEPS: 1.2
:END:

*** Subtasks
- [ ] Run keep-alive tests: make test N=node/http/integration/test_keepalive
- [ ] Verify same socket is reused for multiple requests
- [ ] Check Connection: keep-alive header is honored
- [ ] Check Connection: close header closes connection
- [ ] Test with debug logging to trace socket reuse
- [ ] Update plan with results

* 🎯 Phase 2: Optional Advanced Features Assessment [0/4] [P:B]
:PROPERTIES:
:EXECUTION_MODE: PARALLEL
:DEPENDENCIES: Phase-1
:COMPLEXITY: SIMPLE
:RISK: LOW
:ESTIMATED_TIME: 30 minutes
:END:

** TODO [#B] Task 2.1: Review Phase 4 remaining tasks [P][R:LOW][C:SIMPLE][D:1.3]
:PROPERTIES:
:ID: 2.1
:CREATED: [2025-10-16]
:DEPS: 1.3
:END:

*** Phase 4 Status: 18/25 tasks (72%)
- Core streaming: ✅ DONE (IncomingMessage, ServerResponse)
- Advanced features: ❓ 7 tasks remaining

*** Subtasks
- [ ] Read node-http-plan.md Phase 4 tasks
- [ ] Identify which tasks are MANDATORY vs OPTIONAL
- [ ] Mark OPTIONAL tasks that don't affect core functionality
- [ ] List MANDATORY tasks that must be completed
- [ ] Update this plan with mandatory task list

** TODO [#B] Task 2.2: Review Phase 5 remaining tasks [P][R:LOW][C:SIMPLE][D:1.3]
:PROPERTIES:
:ID: 2.2
:CREATED: [2025-10-16]
:DEPS: 1.3
:END:

*** Phase 5 Status: 17/25 tasks (68%)
- Timeouts: ✅ DONE (server.setTimeout, request.setTimeout)
- Advanced features: ❓ 8 tasks remaining

*** Subtasks
- [ ] Read node-http-plan.md Phase 5 tasks
- [ ] Identify which tasks are MANDATORY vs OPTIONAL
- [ ] Check if HTTPS, maxHeadersCount, etc. are core features
- [ ] List MANDATORY tasks for production-ready HTTP
- [ ] Update this plan with mandatory task list

** TODO [#B] Task 2.3: Review Phase 2 remaining task [P][R:LOW][C:SIMPLE][D:1.3]
:PROPERTIES:
:ID: 2.3
:CREATED: [2025-10-16]
:DEPS: 1.3
:END:

*** Phase 2 Status: 29/30 tasks (97%)
- Missing: Task 2.2.4 - Keep-alive connection reuse
- Note: This is covered by Phase 1 above

*** Subtasks
- [ ] Confirm Task 2.2.4 is same as Phase 1.2 above
- [ ] Mark as complete after Phase 1 finishes
- [ ] No additional work needed

** TODO [#B] Task 2.4: Decide on optional feature completion [P][R:LOW][C:SIMPLE][D:2.1,2.2,2.3]
:PROPERTIES:
:ID: 2.4
:CREATED: [2025-10-16]
:DEPS: 2.1,2.2,2.3
:END:

*** Decision Criteria
- *MANDATORY*: Required for core HTTP server/client functionality
- *OPTIONAL*: Nice-to-have, can be deferred to future work
- *Focus*: Get to 100% test pass rate first

*** Subtasks
- [ ] List all MANDATORY tasks from Phase 4/5
- [ ] Estimate time for each mandatory task
- [ ] Decide: Complete now or defer to future work?
- [ ] Update plan document with decision
- [ ] If deferring: Document in "Future Work" section

* 🎯 Phase 3: Documentation Updates [0/3] [P:B]
:PROPERTIES:
:EXECUTION_MODE: SEQUENTIAL
:DEPENDENCIES: Phase-0,Phase-1
:COMPLEXITY: SIMPLE
:RISK: LOW
:ESTIMATED_TIME: 30 minutes
:END:

** TODO [#B] Task 3.1: Update main plan with three-level status [S][R:LOW][C:SIMPLE][D:0.5,1.3]
:PROPERTIES:
:ID: 3.1
:CREATED: [2025-10-16]
:DEPS: 0.5,1.3
:END:

*** Three-Level Status Tracking
- *Level 1*: Phase status (e.g., Phase 2: 30/30 - 100%)
- *Level 2*: Task status (e.g., Task 2.2: 4/4 subtasks - DONE)
- *Level 3*: Subtask status (e.g., Subtask 2.2.4: ✅ DONE)

*** Subtasks
- [ ] Open docs/plan/node-http-plan.md
- [ ] Update Phase 2 status to 30/30 (100%)
- [ ] Update Phase 4 status with completed tasks
- [ ] Update Phase 5 status with completed tasks
- [ ] Mark Task 0.3.x subtasks as DONE
- [ ] Update overall progress percentage
- [ ] Save and commit

** TODO [#B] Task 3.2: Document POST bug fix details [S][R:LOW][C:SIMPLE][D:3.1]
:PROPERTIES:
:ID: 3.2
:CREATED: [2025-10-16]
:DEPS: 3.1
:END:

*** Subtasks
- [ ] Create detailed bug report in plan
- [ ] Document root cause analysis
- [ ] Document fix implementation
- [ ] Add code references (file:line)
- [ ] Include before/after behavior
- [ ] Note any related issues fixed

** TODO [#B] Task 3.3: Update test results summary [S][R:LOW][C:SIMPLE][D:3.2]
:PROPERTIES:
:ID: 3.3
:CREATED: [2025-10-16]
:DEPS: 3.2
:END:

*** Subtasks
- [ ] Update test count: 200/200 (100%)
- [ ] List all passing test files
- [ ] Update completion timestamp
- [ ] Update status to COMPLETED
- [ ] Add "Production Ready" badge to summary

* 🎯 Phase 4: Final Validation [0/3] [P:A]
:PROPERTIES:
:EXECUTION_MODE: SEQUENTIAL
:DEPENDENCIES: Phase-0,Phase-1,Phase-3
:COMPLEXITY: SIMPLE
:RISK: LOW
:ESTIMATED_TIME: 15 minutes
:END:

** TODO [#A] Task 4.1: Run complete validation suite [S][R:LOW][C:SIMPLE][D:0.5,1.3,3.3]
:PROPERTIES:
:ID: 4.1
:CREATED: [2025-10-16]
:DEPS: 0.5,1.3,3.3
:END:

*** Validation Commands
```bash
make format              # Code formatting
make test                # All unit tests (100% pass)
make wpt                 # Web Platform Tests (no new failures)
make jsrt_m              # AddressSanitizer build
ASAN_OPTIONS=detect_leaks=1 make test N=node/http
```

*** Subtasks
- [ ] Run make format (must pass)
- [ ] Run make test (must be 100%)
- [ ] Run make wpt (no new failures)
- [ ] Run ASAN build and test
- [ ] Check for memory leaks (must be zero)
- [ ] Document any issues found

** TODO [#A] Task 4.2: Performance smoke test [S][R:LOW][C:SIMPLE][D:4.1]
:PROPERTIES:
:ID: 4.2
:CREATED: [2025-10-16]
:DEPS: 4.1
:END:

*** Basic Performance Checks
- Server handles concurrent requests without deadlock
- Keep-alive connections properly reused
- No memory growth over multiple requests
- Response times reasonable (<100ms local)

*** Subtasks
- [ ] Run test_advanced_networking.js (5 concurrent requests)
- [ ] Run test_keepalive.js (verify socket reuse)
- [ ] Monitor memory usage during tests
- [ ] Check for any unusual delays
- [ ] Document performance notes

** TODO [#A] Task 4.3: Final checklist and sign-off [S][R:LOW][C:SIMPLE][D:4.2]
:PROPERTIES:
:ID: 4.3
:CREATED: [2025-10-16]
:DEPS: 4.2
:END:

*** Production Readiness Checklist
- [ ] ✅ 200/200 tests passing (100%)
- [ ] ✅ POST body streaming works
- [ ] ✅ Keep-alive connection reuse works
- [ ] ✅ All mandatory features complete
- [ ] ✅ make format passes
- [ ] ✅ make test passes (100%)
- [ ] ✅ make wpt passes (no new failures)
- [ ] ✅ Zero memory leaks (ASAN)
- [ ] ✅ Plan document updated
- [ ] ✅ Code reviewed for quality
- [ ] ✅ Performance acceptable
- [ ] ✅ Ready for production use

*** Subtasks
- [ ] Review all checkboxes above
- [ ] Mark any incomplete items
- [ ] Document any deferred work
- [ ] Update plan status to COMPLETED
- [ ] Commit all changes with proper message

* 📊 Execution Dashboard
:PROPERTIES:
:CURRENT_PHASE: Phase 0 - Critical Bug Fix
:PROGRESS: 0/15
:COMPLETION: 0%
:ACTIVE_TASK: Task 0.1 - Analyze js_http_incoming_end()
:UPDATED: [2025-10-16 10:00]
:END:

** Current Status
- *Phase*: Phase 0 - Critical Bug Fix
- *Progress*: 0/15 tasks (0%)
- *Active*: Task 0.1 - Analyzing IncomingMessage 'end' event bug

** Next Up
- [ ] Task 0.1: Analyze js_http_incoming_end() implementation
- [ ] Task 0.2: Add debug logging to trace event flow
- [ ] Task 0.3: Fix 'end' event emission bug

** Blockers
- None currently

* 📜 History & Updates
:LOGBOOK:
- State "IN-PROGRESS" from "TODO" [2025-10-16 10:00] \\
  Started work on HTTP module completion plan
- Note taken on [2025-10-16 10:00] \\
  Created comprehensive plan for completing final 2% of HTTP implementation
:END:

** [2025-10-16 10:00] Plan Created
- Analyzed current test failures (2/200 tests failing)
- Identified root cause: POST body 'end' event not emitted
- Created dependency-ordered task breakdown
- Ready to begin Phase 0: Critical Bug Fix

* 🔮 Future Work (Optional Features)

** Phase 4 Optional Tasks (TBD after Phase 2.1 analysis)
- Advanced streaming features
- Stream transformation support
- Backpressure handling enhancements

** Phase 5 Optional Tasks (TBD after Phase 2.2 analysis)
- HTTPS full implementation (requires OpenSSL integration)
- Advanced timeout configurations (maxHeadersCount, headersTimeout, requestTimeout)
- Server-side upgrades (WebSocket protocol switching)
- Trailer headers support

** Performance Optimizations
- Connection pool optimization
- Header parsing optimization
- Buffer management tuning
- Zero-copy optimizations where possible

** Additional Testing
- Stress testing (1000+ concurrent connections)
- Long-running stability tests
- Edge case coverage expansion
- Fuzzing integration

* 📝 Notes

** Design Decisions

*** Three-Level Status Tracking
To improve plan maintainability and visibility, we track progress at three levels:
1. *Phase Level*: Overall phase completion (e.g., "Phase 2: 30/30 tasks")
2. *Task Level*: Individual task completion (e.g., "Task 2.2: 4/4 subtasks")
3. *Subtask Level*: Granular checklist items (e.g., "✅ Implement socket pool")

This provides:
- Clear visibility into what's done vs remaining
- Easy identification of blocking tasks
- Better estimation of remaining work

*** Focus on Core Functionality
The plan prioritizes core HTTP server/client functionality required for production use:
- ✅ Request/response handling
- ✅ Streaming (data/end events)
- ✅ Keep-alive connection reuse
- ✅ Timeouts
- ❌ HTTPS (requires OpenSSL - deferred)
- ❌ WebSocket upgrades (advanced - deferred)
- ❌ Advanced headers (nice-to-have - deferred)

*** Dependency-Ordered Execution
Tasks are ordered to maximize parallel execution while respecting dependencies:
- Phase 0 (bug fix) blocks all other work
- Phase 1 (keep-alive) can start after Phase 0
- Phase 2 (assessment) runs in parallel after Phase 1
- Phase 3 (docs) runs in parallel with Phase 2
- Phase 4 (validation) runs last after all work complete

** References
- Main plan: docs/plan/node-http-plan.md
- HTTP implementation: src/node/http/
- Test suite: test/node/http/
- Guidelines: CLAUDE.md
