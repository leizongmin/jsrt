# Node.js HTTP Implementation - Quick Cheat Sheet

**One-page reference for rapid execution**

---

## Current Status

```
Progress: 98/185 (53.0%)    Time Left: 32-44 hours (4-6 days)

✅ Phase 1: Refactoring     100% (25/25)
🟡 Phase 2: Server          87%  (26/30)  ◄── 4 tasks left
✅ Phase 3: Client          100% (35/35)
🟢 Phase 4: Streaming       48%  (12/25)  ◄── Core done, rest optional
⏳ Phase 5: Advanced        0%   (0/25)   ◄── Next
⏳ Phase 6: Testing         0%   (0/20)   ◄── Gate
⏳ Phase 7: Documentation   0%   (0/10)   ◄── Final
```

---

## Next 4 Tasks (Start Here)

### Group 1: Phase 2 Completion (5-7 hours)

```
1. Task 2.2.4: Keep-alive reuse         [2h] [R:MED][C:MED]
   └─ Implement parser reset, test multiple requests

2. Task 2.2.5: Timeout enforcement      [1h] [R:MED][C:SIMPLE]
   └─ Active timeout enforcement, cleanup

3. Task 2.3.4: Request body streaming   [1h] [R:LOW][C:SIMPLE]
   └─ Verify Phase 4.1 integration, test

4. Task 2.3.6: Upgrade event emission   [1h] [R:LOW][C:SIMPLE]
   └─ Emit 'upgrade' event with socket

✓ Checkpoint: make format && make test && make wpt
```

---

## Execution Commands

### Per Task
```bash
# Start
make format
timeout 20 ./bin/jsrt test/node/http/relevant_test.js

# Complete
make format
make test N=node
# Update plan: TODO → DONE
```

### Per Subgroup
```bash
make format
make test N=node
make wpt N=relevant
# Update plan: progress counters
```

### Per Group
```bash
make format && make test && make wpt
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/node/http/*.js
# All MUST pass 100%
```

### Production Gate (Group 3)
```bash
make format           # ZERO warnings
make test             # 100% pass
make wpt              # No regressions
# ASAN: ZERO leaks, ZERO use-after-free
# If ANY fail: STOP
```

---

## File Locations

### Source
```
src/node/http/
├── http_internal.h    # Types, structs (230 lines)
├── http_server.c      # Server (164 lines)
├── http_client.c      # Client (730 lines)
├── http_incoming.c    # IncomingMessage (45 lines)
├── http_response.c    # ServerResponse (190 lines)
├── http_parser.c      # llhttp (254 lines)
└── http_module.c      # Registration (265 lines)
```

### Tests
```
test/node/
├── test_node_http*.js        # 6 files, 165+ tests
└── http/ (create in Task 6.1.1)
    ├── server/
    ├── client/
    └── integration/
```

### Plans
```
docs/plan/
├── node-http-plan.md                    # Main (26k lines)
├── node-http-execution-strategy.md      # Full guide
├── node-http-execution-summary.md       # Quick ref
├── node-http-dependency-graph.md        # Visual
├── node-http-cheatsheet.md              # This file
└── README-node-http.md                  # Index
```

---

## Remaining Work (Option A)

### Group 1: Phase 2 (4 tasks, 5-7h)
```
Keep-alive, timeout, streaming, upgrade
```

### Group 2: Phase 5 Critical (15-18 tasks, 9-12h)
```
Subgroup 2A: Timeout handling (5 tasks)
Subgroup 2B: Header limits (4 tasks)
Subgroup 2C: Events (3 tasks)
Optional: Trailers (4), Special (3), HTTP/1.0 (3)
```

### Group 3: Phase 6 Validation (20 tasks, 11-15h)
```
Subgroup 3A: Test organization (4 tasks)
Subgroup 3B: Comprehensive tests (12 tasks)
Subgroup 3C: ASAN & compliance (4 tasks) ◄── GATE
```

### Group 4: Phase 7 Documentation (10 tasks, 7-10h)
```
Subgroup 4A: Code docs (3 tasks)
Subgroup 4B: API docs (3 tasks)
Subgroup 4C: Cleanup (4 tasks)
```

---

## Deferred Tasks (26 total)

```
Phase 4.3: ClientRequest Writable (6)
Phase 4.4: Advanced Streaming (7)
Phase 5.3: Trailers (4)
Phase 5.4: Special Features (3)
Phase 5.5: HTTP/1.0 (3)
Phase 3.5: Socket Pooling (3)

Reason: Optional enhancements, core complete
```

---

## API Coverage

```
Server:  8/15  (53%)  ◄── 7 methods missing
Client:  15/20 (75%)  ◄── 5 methods missing
Message: 8/10  (80%)  ◄── 2 methods missing
Total:   31/45 (69%)  ◄── 14 methods to add
```

---

## High-Risk Tasks

```
🔴 Task 2.2.4: Keep-alive (parser reset complexity)
   Buffer: +1h for issues

🔴 Tasks 2.2.5 & 5.1.x: Timeouts (timer lifecycle)
   Buffer: +1h for issues

🔴 Task 6.5.1: ASAN validation (may find leaks)
   Buffer: +2-3h for leak fixes
```

---

## Plan Update Template

```org
*** IN-PROGRESS Task X.Y.Z: Task name
- 🔄 Working on X
- ✅ Y complete
- ⏳ Z in progress

*** DONE Task X.Y.Z: Task name
CLOSED: [2025-10-14]
- ✅ Implementation complete
- ✅ Tests passing
- ✅ ASAN clean

** DONE [#A] Task X.Y: Subgroup [N/N]
:PROPERTIES:
:COMPLETED: [2025-10-14]
:END:

* Phase X: Name [N/N] COMPLETE
:PROPERTIES:
:COMPLETED: [2025-10-14]
:END:
```

---

## Testing Discipline

```
After Task:       make format + quick test
After Subgroup:   make format + make test N=node
After Group:      make format + make test + make wpt + ASAN
Production Gate:  ALL MUST PASS 100% or STOP
```

---

## Time Estimates

```
Group 1: 5-7h    (Phase 2 complete)
Group 2: 9-12h   (Phase 5 critical)
Group 3: 11-15h  (Phase 6 validation) ◄── GATE
Group 4: 7-10h   (Phase 7 docs)
Total:   32-44h  (4-6 days with parallelism)
```

---

## Critical Path

```
Task 2.2.4 → 2.2.5 → 2.3.4 → 2.3.6 → Phase 5 → Phase 6 → Phase 7
   2h        1.5h     1h      1h      7-10h    8-12h    6-9h

Parallelism saves: ~7 hours
```

---

## Validation Gates

```
Gate 1: After Group 1
  └─ make test passing (165+ tests)

Gate 2: After Group 2
  └─ make test passing (new features tested)

Gate 3: After Group 3 ◄── PRODUCTION GATE
  └─ make format: ZERO warnings
  └─ make test: 100% pass rate
  └─ make wpt: No regressions
  └─ ASAN: ZERO leaks/errors
  └─ IF ANY FAIL: STOP

Gate 4: After Group 4
  └─ Clean release build
  └─ All tests pass
  └─ Documentation complete
```

---

## Common Patterns

### Memory Management
```c
// Allocate
char* str = js_malloc(ctx, size);

// Use
...

// Free
js_free(ctx, str);
JS_FreeValue(ctx, jsval);
```

### Event Emission
```c
JSValue event = JS_NewString(ctx, "eventName");
JSValue args[] = { arg1, arg2 };
JS_Call(ctx, emit_func, obj, 2, args);
JS_FreeValue(ctx, event);
```

### ASAN Validation
```bash
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test_file.js
# Check output for leaks
```

---

## Debugging

### Enable Debug Logging
```bash
make jsrt_g
./bin/jsrt_g test_file.js
# JSRT_Debug() calls print to stderr
```

### Common Issues
```
Segfault?        → Check finalizers, use ASAN
Memory leak?     → Check JS_FreeValue calls
Test hanging?    → Use timeout 20, check for infinite loops
Parser error?    → Check llhttp_errno_name()
```

---

## Quick Decisions

### Use This If:
- Need production-ready HTTP module in 4-6 days
- Core features sufficient (client/server/streaming)
- Can defer 26 optional enhancements

### Use Option B If:
- Need FASTEST path (3-4 days)
- Accept missing features (timeouts, keep-alive)
- Can add features later

### Use Option C If:
- Need COMPLETE implementation (8-10 days)
- Want all 45 APIs implemented
- Have time for comprehensive work

---

## Emergency Contacts

**Stuck?**
1. Review main plan: docs/plan/node-http-plan.md
2. Check execution strategy: docs/plan/node-http-execution-strategy.md
3. See dependency graph: docs/plan/node-http-dependency-graph.md
4. Follow jsrt guidelines: CLAUDE.md

**Blocker?**
1. Document in plan with BLOCKED status
2. Try alternative approach
3. Mark as DEFERRED if critical
4. Seek help from maintainers

---

## Success Metrics

```
✅ All Group 1-4 tasks complete
✅ make format passes (ZERO warnings)
✅ make test passes (100% pass rate)
✅ make wpt passes (no new failures)
✅ ASAN validation passes (ZERO leaks)
✅ API coverage: 45/45 methods (100%)
✅ Code documented
✅ Tests organized
✅ Ready for production deployment
```

---

**Quick Start**: Read this + docs/plan/node-http-execution-summary.md (10 min)
**Full Context**: Read docs/plan/node-http-execution-strategy.md (30 min)
**Deep Dive**: Read docs/plan/node-http-plan.md (2 hours)

**Start Here**: Task 2.2.4 - Keep-alive reuse (docs/plan/node-http-plan.md:650)

---

**Version**: 1.0
**Date**: 2025-10-14
**Status**: Ready ✅
