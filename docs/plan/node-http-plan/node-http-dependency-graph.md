# Node.js HTTP Module - Task Dependency Graph

**Visual representation of task dependencies and execution order**

---

## High-Level Phase Dependencies

```
┌─────────────┐
│  Phase 0    │
│  Research   │  ⏸️ CAN SKIP (Architecture exists)
│  (0/15)     │
└─────────────┘
       │
       ▼
┌─────────────┐
│  Phase 1    │
│ Refactoring │  ✅ COMPLETE (25/25)
│   100%      │
└─────────────┘
       │
       ├──────────────────────────────────┐
       │                                  │
       ▼                                  ▼
┌─────────────┐                    ┌─────────────┐
│  Phase 2    │                    │  Phase 3    │
│   Server    │  🟡 87% (26/30)    │   Client    │  ✅ COMPLETE (35/35)
│ Enhancement │                    │    Full     │
└─────────────┘                    └─────────────┘
       │                                  │
       │                                  │
       └──────────────┬───────────────────┘
                      │
                      ▼
               ┌─────────────┐
               │  Phase 4.1  │
               │ IncomingMsg │  ✅ COMPLETE (6/6)
               │  Readable   │
               └─────────────┘
                      │
                      ▼
               ┌─────────────┐
               │  Phase 4.2  │
               │ServerResp   │  ✅ COMPLETE (6/6)
               │  Writable   │
               └─────────────┘
                      │
                      ├───────────────────────┐
                      │                       │
                      ▼                       ▼
               ┌─────────────┐        ┌─────────────┐
               │  Phase 4.3  │        │  Phase 4.4  │
               │ClientReq    │ ⏸️     │  Advanced   │ ⏸️
               │  Writable   │OPTIONAL│  Streaming  │OPTIONAL
               └─────────────┘        └─────────────┘
                      │
       ┌──────────────┴──────────────┐
       │                             │
       ▼                             │
┌─────────────┐                     │
│  Phase 5    │                     │
│  Advanced   │  ⏳ NEXT (0/25)     │
│  Features   │                     │
└─────────────┘                     │
       │                             │
       └─────────────┬───────────────┘
                     │
                     ▼
              ┌─────────────┐
              │  Phase 6    │
              │  Testing &  │  ⏳ GATE (0/20)
              │ Validation  │  ⚠️ MUST PASS 100%
              └─────────────┘
                     │
                     ▼
              ┌─────────────┐
              │  Phase 7    │
              │Documentation│  ⏳ FINAL (0/10)
              │  & Cleanup  │
              └─────────────┘
                     │
                     ▼
              🚀 PRODUCTION READY
```

---

## Detailed Task Dependencies - Group 1 (Phase 2 Completion)

**Execution Mode**: SEQUENTIAL (must complete in order)

```
┌────────────────────────────────────────────────┐
│ Task 2.2.4: Handle keep-alive reuse            │
│ [S][R:MEDIUM][C:MEDIUM]                        │
│ - Implement parser reset                       │
│ - Track connection state                       │
│ - Test multiple requests                       │
│ Dependencies: None                             │
│ Time: 1.5-2h                                   │
└────────────────────────────────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────────────┐
│ Task 2.2.5: Connection timeout handling        │
│ [S][R:MEDIUM][C:SIMPLE]                        │
│ - Active timeout enforcement                   │
│ - Cleanup on timeout                           │
│ - Emit timeout event                           │
│ Dependencies: None                             │
│ Time: 1-1.5h                                   │
└────────────────────────────────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────────────┐
│ Task 2.3.4: Request body streaming             │
│ [S][R:LOW][C:SIMPLE]                           │
│ - Integrate Phase 4.1 (may be done)           │
│ - Verify data/end events                       │
│ - Test streaming                               │
│ Dependencies: Phase 4.1 complete (✅)          │
│ Time: 0.5-1h                                   │
└────────────────────────────────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────────────┐
│ Task 2.3.6: Upgrade event emission             │
│ [S][R:LOW][C:SIMPLE]                           │
│ - Emit 'upgrade' event                         │
│ - Detection exists (is_upgrade flag)           │
│ - Simple event emission                        │
│ Dependencies: None                             │
│ Time: 0.5-1h                                   │
└────────────────────────────────────────────────┘
                    │
                    ▼
            ✅ Checkpoint 1
     make format && make test && make wpt
```

**Total Time**: 4-6 hours
**Parallelism**: None (all sequential)

---

## Detailed Task Dependencies - Group 2 (Phase 5 Critical)

### Subgroup 2A: Timeout Handling (SEQUENTIAL)

```
Task 5.1.1 ──► Task 5.1.2 ──► Task 5.1.3 ──► Task 5.1.4 ──► Task 5.1.5
server.      per-request    client        various       test
setTimeout()   timeout      timeout       timeouts      scenarios

Time: 0.5h     0.5h         0.5h          1h            1h
Total: 3-4 hours
```

### Subgroup 2B: Header Limits (SEQUENTIAL)

```
Task 5.2.1 ──► Task 5.2.2 ──► Task 5.2.3 ──► Task 5.2.4
maxHeaders     maxHeader     track size    test
Count          Size          in parsing    limits

Time: 0.5h     0.5h          0.5h          0.5h
Total: 2-3 hours
```

### Subgroup 2C: Connection Events (PARALLEL)

```
        ┌─────────────────────────────────┐
        │                                 │
        │    All can run in parallel      │
        │                                 │
        ▼                                 ▼
Task 5.6.1                        Task 5.6.2
'connection'                      'close' events
event

        │                                 │
        │                                 │
        └────────────┬────────────────────┘
                     │
                     ▼
                Task 5.6.3
                'finish' events

Time: 0.5h each
Total: 2 hours (with parallelism) or 1.5 hours (sequential)
```

### Group 2 Summary

```
Subgroup 2A (3-4h)
      │
      ▼
Subgroup 2B (2-3h)
      │
      ▼
Subgroup 2C (1.5-2h)  ◄── Can overlap with 2B
      │
      ▼
✅ Checkpoint 2
make format && make test N=node
```

**Total Time**: 7-10 hours (sequential) or 9-12 hours (with buffer)
**Parallelism**: Subgroup 2C tasks can run in parallel

---

## Detailed Task Dependencies - Group 3 (Phase 6 Validation)

### Subgroup 3A: Test Organization (SEQUENTIAL)

```
Task 6.1.1 ──► Task 6.1.2 ──► Task 6.1.3 ──► Task 6.1.4
Create dir     Move tests    Create index  Update runner
structure

Time: 0.5h     0.5h          0.5h          0.5h
Total: 1-2 hours
```

### Subgroup 3B: Comprehensive Testing (MIXED)

```
SERVER TESTS (PARALLEL)                CLIENT TESTS (PARALLEL)
┌─────────────┐  ┌─────────────┐       ┌─────────────┐  ┌─────────────┐
│ Task 6.2.1  │  │ Task 6.2.2  │       │ Task 6.3.1  │  │ Task 6.3.2  │
│   Basic     │  │  Request    │       │   Basic     │  │  Request    │
│   server    │  │  handling   │       │   client    │  │   tests     │
└─────────────┘  └─────────────┘       └─────────────┘  └─────────────┘
       │                │                      │                │
       ▼                ▼                      ▼                ▼
┌─────────────┐  ┌─────────────┐       ┌─────────────┐  ┌─────────────┐
│ Task 6.2.3  │  │ Task 6.2.4  │       │ Task 6.3.3  │  │ Task 6.3.4  │
│  Response   │  │    Edge     │       │  Response   │  │    Edge     │
│   tests     │  │   cases     │       │   tests     │  │   cases     │
└─────────────┘  └─────────────┘       └─────────────┘  └─────────────┘
       │                │                      │                │
       └────────┬───────┘                      └────────┬───────┘
                │                                       │
                └───────────────┬───────────────────────┘
                                │
                                ▼
                    INTEGRATION TESTS (SEQUENTIAL)

                    Task 6.4.1 ──► Task 6.4.2 ──► Task 6.4.3 ──► Task 6.4.4
                    Client-srv     Streaming      Keep-alive     Errors
                    integration    tests          tests          tests

                    Time: 1h        1h             1h             1h
```

**Time**:
- Server/Client tests: 1h each (parallel) = 2h total
- Integration tests: 4h sequential
- Total: 6h (with parallelism) or 8h (sequential)

### Subgroup 3C: ASAN & Compliance (SEQUENTIAL)

```
Task 6.5.1 ──► Task 6.5.2 ──► Task 6.5.3 ──► Task 6.5.4
ASAN          WPT tests     Format        Full test
validation                  check         suite

Time: 1-2h    0.5h          0.5h          0.5h
Total: 2-3 hours (may need 3h if leaks found)
```

### Group 3 Summary

```
Subgroup 3A (1-2h)
      │
      ▼
Subgroup 3B (6-8h)
      │
      ▼
Subgroup 3C (2-3h) ◄── ⚠️ PRODUCTION GATE
      │
      ▼
✅ ALL TESTS MUST PASS 100%
```

**Total Time**: 9-13 hours (with parallelism) or 11-15 hours (with buffer)
**Parallelism**: Server/client tests can run in parallel

---

## Detailed Task Dependencies - Group 4 (Phase 7 Documentation)

### Subgroup 4A: Code Documentation (PARALLEL)

```
        ┌─────────────────────────────────────────┐
        │                                         │
        │    All can run in parallel              │
        │                                         │
        ▼                                         ▼
Task 7.1.1                              Task 7.1.2
Header file                             Implementation
docs                                    comments

        │                                         │
        │                                         │
        └────────────┬────────────────────────────┘
                     │
                     ▼
                Task 7.1.3
                llhttp docs

Time: 1h each
Total: 2-3 hours (with parallelism) or 3 hours (sequential)
```

### Subgroup 4B: API Documentation (SEQUENTIAL)

```
Task 7.2.1 ──► Task 7.2.2 ──► Task 7.2.3
API            Compatibility  Usage
reference      docs           guide

Time: 1h       1h             1h
Total: 2-3 hours
```

### Subgroup 4C: Code Cleanup (SEQUENTIAL)

```
Task 7.3.1 ──► Task 7.3.2 ──► Task 7.3.3 ──► Task 7.3.4
Remove dead    Optimize      Final code    Performance
code           imports       review        review

Time: 0.5h     0.5h          1h            1h
Total: 2-3 hours
```

### Group 4 Summary

```
Subgroup 4A (2-3h)  ◄── Tasks can be parallel
      │
      ▼
Subgroup 4B (2-3h)
      │
      ▼
Subgroup 4C (2-3h)
      │
      ▼
✅ Final Checkpoint
make clean && make && make test && make wpt
```

**Total Time**: 6-9 hours (with parallelism) or 7-10 hours (with buffer)
**Parallelism**: Subgroup 4A tasks can run in parallel

---

## Critical Path Analysis

### Longest Sequential Path (Critical Path)

```
Group 1 (5-7h)
    │
    ▼
Group 2 Subgroup 2A (3-4h)
    │
    ▼
Group 2 Subgroup 2B (2-3h)
    │
    ▼
Group 2 Subgroup 2C (1.5-2h)
    │
    ▼
Group 3 Subgroup 3A (1-2h)
    │
    ▼
Group 3 Subgroup 3B Integration (4h)
    │
    ▼
Group 3 Subgroup 3C (2-3h) ⚠️ GATE
    │
    ▼
Group 4 Subgroup 4B (2-3h)
    │
    ▼
Group 4 Subgroup 4C (2-3h)

TOTAL: 23-33 hours (critical path only)
```

### With Parallelism

```
- Group 3B: Server/client tests run parallel: Save 4h
- Group 2C: Events run parallel: Save 1h
- Group 4A: Docs run parallel: Save 1h

Total savings: ~6-7h
Optimized total: 32-44h
```

---

## Dependency Matrix

### Task → Blocker Relationship

| Task | Blocks | Blocked By |
|------|--------|------------|
| 2.2.4 | 2.2.5 | None |
| 2.2.5 | 2.3.4 | None |
| 2.3.4 | 2.3.6 | Phase 4.1 (✅) |
| 2.3.6 | Phase 5 | None |
| Phase 5 | Phase 6 | Phase 2 complete |
| Phase 6 | Phase 7 | Phase 5 complete |
| Phase 7 | Production | Phase 6 complete |

### Can Start Immediately (No Dependencies)

```
✅ Task 2.2.4 (keep-alive)
✅ Task 2.2.5 (timeout) - can run parallel with 2.2.4
✅ Task 2.3.6 (upgrade) - can run parallel with 2.2.4/5
```

### Cannot Start Until Phase 2 Complete

```
⏳ All Phase 5 tasks
⏳ All Phase 6 tasks
⏳ All Phase 7 tasks
```

### Optional (Can Skip/Defer)

```
⏸️ Phase 4.3 (ClientRequest Writable)
⏸️ Phase 4.4 (Advanced Streaming)
⏸️ Phase 5.3 (Trailers)
⏸️ Phase 5.4 (Special Features) - partial
⏸️ Phase 5.5 (HTTP/1.0)
⏸️ Phase 3.5 (Socket Pooling)
```

---

## Execution Order Optimization

### Fastest Path (Maximize Parallelism)

```
Day 1 (8h):
├─ Group 1: Task 2.2.4 (2h)
├─ Group 1: Task 2.2.5 (1.5h) [parallel start after 2.2.4]
├─ Group 1: Task 2.3.4 (1h)
├─ Group 1: Task 2.3.6 (1h)
└─ Checkpoint 1 (0.5h)
└─ Group 2: Subgroup 2A start (2h remaining)

Day 2 (8h):
├─ Group 2: Subgroup 2A complete (1h)
├─ Group 2: Subgroup 2B (2h)
├─ Group 2: Subgroup 2C (1.5h) [parallel with 2B end]
└─ Checkpoint 2 (0.5h)
└─ Group 3: Subgroup 3A (2h)

Day 3 (8h):
├─ Group 3: Subgroup 3B Server tests (2h)
├─ Group 3: Subgroup 3B Client tests (2h) [parallel]
├─ Group 3: Subgroup 3B Integration (4h)

Day 4 (8h):
├─ Group 3: Subgroup 3C ASAN (2h)
├─ Checkpoint 3 GATE (1h) ⚠️
└─ Group 4: Subgroup 4A (2h) [parallel tasks]
└─ Group 4: Subgroup 4B start (3h remaining)

Day 5 (8h):
├─ Group 4: Subgroup 4B complete (0h)
├─ Group 4: Subgroup 4C (3h)
└─ Final checkpoint (1h)
└─ Buffer/polish (4h)

Total: 4-5 days (32-40 hours)
```

---

## Dependency Legend

```
[S] = Sequential (must complete before next)
[P] = Parallel (can run simultaneously)
[PS] = Parallel-Sequential (parallel within group, sequential between groups)

[D:TaskID] = Hard dependency (blocks until complete)
[SD:TaskID] = Soft dependency (preferred but not blocking)
[AFTER:TaskID] = Order dependency (must start after, not complete)
[BLOCKS:TaskID] = Blocking other tasks

[R:LEVEL] = Risk level (LOW/MEDIUM/HIGH)
[C:LEVEL] = Complexity (TRIVIAL/SIMPLE/MEDIUM/COMPLEX)
```

---

## Next Task to Execute

**Immediately Available**:

```
🎯 START HERE: Task 2.2.4 - Handle keep-alive reuse

   Location: docs/plan/node-http-plan.md:650
   Status: TODO → IN-PROGRESS
   Dependencies: None
   Risk: MEDIUM
   Complexity: MEDIUM
   Time: 1.5-2 hours

   Files to modify:
   - src/node/http/http_parser.c (parser reset)
   - src/node/http/http_internal.h (connection state)

   Tests:
   - Create test/node/http/test_keepalive.js
   - Test multiple requests on same connection

   Checkpoint:
   - make format
   - timeout 20 ./bin/jsrt test/node/http/test_keepalive.js
   - make test N=node
```

---

**Document Version**: 1.0
**Last Updated**: 2025-10-14
**Status**: Ready for execution
