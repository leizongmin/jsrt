---
Created: 2025-10-09T00:00:00Z
Document Type: Execution Strategy
Status: READY FOR IMPLEMENTATION
Overall Progress: 80/120 tasks (67% complete)
Remaining Work: 40 tasks across Phases 4-6
---

# Node.js Stream Implementation - Execution Strategy
## Maximizing Parallelization & Code Reuse from WHATWG Streams

## Executive Summary

This document provides a detailed execution strategy for completing the Node.js stream implementation (Phases 4-6), with focus on:
- **Maximum parallelization** of independent tasks
- **Code reuse** from existing WHATWG Streams implementation
- **Minimal dependencies** to unblock parallel execution paths
- **Effort reduction** through strategic adaptation of existing patterns

### Key Findings

**Code Reuse Potential**: 40-50% of Phase 4-6 implementation can leverage existing `streams.c` patterns:
- **Queue/Buffer Management**: 60% reusable (lines 34-158, 644-656)
- **Promise Handling**: 80% reusable (lines 566-710, 1481-1489)
- **State Management**: 50% reusable (controller patterns, error handling)
- **Lifecycle Management**: 40% reusable (finalizers, cleanup logic)

**Parallelization Opportunities**:
- **Phase 4**: 14/20 tasks can run in parallel (70%)
- **Phase 5**: 12/15 tasks can run in parallel (80%)
- **Phase 6**: 4/5 tasks can run in parallel (80%)
- **Overall**: 30/40 tasks parallelizable (75%)

**Estimated Effort Reduction**: 35-40% time savings through code reuse

---

## Phase 4: Duplex & Transform Streams (20 tasks)

**Goal**: Implement bidirectional and transforming streams
**Status**: ⏳ NOT STARTED
**Estimated Duration**: 3-4 days → **2-3 days with reuse**

### Code Reuse Analysis from streams.c

#### Reusable Components

1. **Dual-State Management** (Lines 279-430, 988-1190)
   - Pattern: `JSRT_ReadableStream` + `JSRT_WritableStream` structures
   - Adaptation: Merge both into `Duplex` structure
   - Reuse: 50% - State tracking, locking mechanism
   - Effort: 4 hours → **2 hours** with adaptation

2. **Controller Pattern** (Lines 42-276, 928-985)
   - Pattern: Separate controllers for read/write sides
   - Adaptation: Duplex needs both controllers simultaneously
   - Reuse: 60% - Controller initialization, method signatures
   - Effort: 3 hours → **1.5 hours** with adaptation

3. **Finalizer Pattern** (Lines 285-296, 996-1007)
   - Pattern: Cleanup both readable and writable resources
   - Adaptation: Combine cleanup logic from both finalizers
   - Reuse: 70% - Direct copy with dual cleanup
   - Effort: 1 hour → **0.5 hours** with adaptation

### Parallel Execution Groups

#### **Group 4.1: Duplex Foundation [SEQUENTIAL]** (Tasks 4.1.1-4.1.2)
**Duration**: 4 hours → **2.5 hours with reuse**

- **Task 4.1.1**: Create Duplex class structure
  - **Reuse from streams.c**:
    - Lines 279-283 (`JSRT_ReadableStream` struct)
    - Lines 988-994 (`JSRT_WritableStream` struct)
  - **Adaptation**: Merge both structures
    ```c
    // Before (from streams.c - 2 separate structs):
    typedef struct {
      JSValue controller;
      JSValue underlying_source;
      bool locked;
    } JSRT_ReadableStream;

    typedef struct {
      JSValue controller;
      bool locked;
      JSValue underlying_sink;
      int high_water_mark;
    } JSRT_WritableStream;

    // After (node:stream Duplex - merged):
    typedef struct {
      JSValue event_emitter;          // NEW: EventEmitter base
      JSValue readable_controller;    // From JSRT_ReadableStream
      JSValue writable_controller;    // From JSRT_WritableStream
      bool readable_locked;            // From JSRT_ReadableStream
      bool writable_locked;            // From JSRT_WritableStream
      bool allow_half_open;            // NEW: Node.js specific
      bool readable_ended;             // NEW: Node.js specific
      bool writable_ended;             // NEW: Node.js specific
    } NodeDuplexStream;
    ```
  - **Lines reused**: ~40 lines from streams.c
  - **New lines**: ~60 lines (EventEmitter integration, Node.js-specific state)
  - **Effort**: 3 hours → **1.5 hours** (50% reduction)
  - **Test**: Instance has both readable/writable properties

- **Task 4.1.2**: Implement Duplex constructor
  - **Reuse from streams.c**:
    - Lines 303-363 (ReadableStream constructor pattern)
    - Lines 1014-1123 (WritableStream constructor pattern)
  - **Adaptation**: Initialize both sides + EventEmitter
    ```c
    // Reuse constructor initialization patterns
    // From ReadableStream (lines 304-327):
    stream->readable_controller = create_readable_controller(ctx, obj);

    // From WritableStream (lines 1015-1040):
    stream->writable_controller = create_writable_controller(ctx, obj);

    // NEW: EventEmitter integration
    stream->event_emitter = create_event_emitter(ctx);

    // NEW: Node.js options
    stream->allow_half_open = get_option_bool(ctx, options, "allowHalfOpen", true);
    ```
  - **Lines reused**: ~60 lines
  - **New lines**: ~40 lines (dual initialization logic)
  - **Effort**: 2 hours → **1 hour** (50% reduction)
  - **Test**: Constructor accepts readableOptions/writableOptions

**Blockers**: None (uses existing EventEmitter + streams.c patterns)

---

#### **Group 4.2: Duplex Independent Operations [PARALLEL]** (Tasks 4.1.3-4.1.6)
**Duration**: 6 hours → **3 hours with reuse**
**Dependencies**: [D:4.1.1, D:4.1.2]

All 4 tasks can run **simultaneously** as they operate on independent aspects:

- **Task 4.1.3**: Implement `_read()` for Duplex
  - **Reuse from**: Phase 2 Readable implementation (already complete)
  - **Adaptation**: Copy read logic from Phase 2, ensure independence from write side
  - **Lines reused**: ~80 lines from readable.c
  - **New lines**: ~20 lines (duplex-specific guards)
  - **Effort**: 2 hours → **1 hour** (50% reduction)
  - **Test**: Read works independently

- **Task 4.1.4**: Implement `_write()` for Duplex
  - **Reuse from**: Phase 3 Writable implementation (already complete)
  - **Adaptation**: Copy write logic from Phase 3, ensure independence from read side
  - **Lines reused**: ~90 lines from writable.c
  - **New lines**: ~20 lines (duplex-specific guards)
  - **Effort**: 2 hours → **1 hour** (50% reduction)
  - **Test**: Write works independently

- **Task 4.1.5**: Implement duplex state management
  - **Reuse from streams.c**:
    - Lines 279-283 (ReadableStream state)
    - Lines 988-994 (WritableStream state)
  - **Adaptation**: Track both states independently
    ```c
    // Reuse state tracking patterns
    bool is_readable() { return !stream->readable_ended && !stream->destroyed; }
    bool is_writable() { return !stream->writable_ended && !stream->destroyed; }
    ```
  - **Lines reused**: ~30 lines
  - **New lines**: ~20 lines (dual state logic)
  - **Effort**: 1.5 hours → **0.5 hours** (67% reduction)
  - **Test**: States track independently

- **Task 4.1.6**: Implement duplex destruction
  - **Reuse from streams.c**:
    - Lines 285-296 (ReadableStream finalizer)
    - Lines 996-1007 (WritableStream finalizer)
  - **Adaptation**: Cleanup both sides + EventEmitter
    ```c
    static void NodeDuplexStreamFinalize(JSRuntime* rt, JSValue val) {
      NodeDuplexStream* stream = JS_GetOpaque(val, NodeDuplexStreamClassID);
      if (stream) {
        // Reuse readable cleanup (lines 285-296)
        if (!JS_IsUndefined(stream->readable_controller)) {
          JS_FreeValueRT(rt, stream->readable_controller);
        }
        // Reuse writable cleanup (lines 996-1007)
        if (!JS_IsUndefined(stream->writable_controller)) {
          JS_FreeValueRT(rt, stream->writable_controller);
        }
        // NEW: EventEmitter cleanup
        if (!JS_IsUndefined(stream->event_emitter)) {
          JS_FreeValueRT(rt, stream->event_emitter);
        }
        free(stream);
      }
    }
    ```
  - **Lines reused**: ~20 lines
  - **New lines**: ~10 lines (dual cleanup)
  - **Effort**: 1 hour → **0.5 hours** (50% reduction)
  - **Test**: Destroy cleans up properly

**Total Group 4.2 Effort**: 6.5 hours → **3 hours** (54% reduction)

---

#### **Group 4.3: Duplex Advanced Features [PARALLEL]** (Tasks 4.1.7-4.1.8)
**Duration**: 3 hours → **2 hours with reuse**
**Dependencies**: [D:4.2]

- **Task 4.1.7**: Implement `allowHalfOpen` option
  - **Reuse from streams.c**: Options parsing pattern (lines 1042-1080)
  - **Adaptation**: Add Node.js-specific half-open logic
    ```c
    // Reuse options parsing pattern
    JSValue allowHalfOpen = JS_GetPropertyStr(ctx, options, "allowHalfOpen");
    stream->allow_half_open = JS_ToBool(ctx, allowHalfOpen) ? true : false;
    JS_FreeValue(ctx, allowHalfOpen);

    // NEW: Half-open behavior
    if (!stream->allow_half_open) {
      // When read side ends, auto-end write side
      if (stream->readable_ended && !stream->writable_ended) {
        end_writable_side(stream);
      }
    }
    ```
  - **Lines reused**: ~15 lines
  - **New lines**: ~25 lines (half-open logic)
  - **Effort**: 2 hours → **1 hour** (50% reduction)
  - **Test**: Half-open works correctly

- **Task 4.1.8**: Implement duplex event coordination
  - **Reuse from**: Phase 2 & 3 event implementations (already complete)
  - **Adaptation**: Route events from both sides
  - **Lines reused**: ~40 lines (event emission patterns)
  - **New lines**: ~30 lines (event routing)
  - **Effort**: 2 hours → **1 hour** (50% reduction)
  - **Test**: All events fire

**Total Group 4.3 Effort**: 4 hours → **2 hours** (50% reduction)

---

### Transform Stream Implementation

#### **Group 4.4: Transform Foundation [SEQUENTIAL]** (Tasks 4.2.1-4.2.4)
**Duration**: 6 hours → **3 hours with reuse**
**Dependencies**: [D:4.1.8] (requires completed Duplex)

- **Task 4.2.1**: Create Transform class
  - **Reuse from**: Duplex class (just created)
  - **Adaptation**: Extend Duplex with transform state
    ```c
    typedef struct {
      NodeDuplexStream duplex;  // Inherit everything from Duplex
      JSValue transform_callback;  // NEW: _transform function
      JSValue flush_callback;      // NEW: _flush function
      bool transforming;           // NEW: Transform in progress
    } NodeTransformStream;
    ```
  - **Lines reused**: ~100 lines (inherit all Duplex)
  - **New lines**: ~30 lines (transform-specific state)
  - **Effort**: 2 hours → **1 hour** (50% reduction)
  - **Test**: Instance is duplex

- **Task 4.2.2**: Implement Transform constructor
  - **Reuse from**: Duplex constructor
  - **Adaptation**: Add transform options parsing
    ```c
    // Reuse Duplex constructor (90%)
    init_duplex(ctx, obj, options);

    // NEW: Transform-specific options
    JSValue transform_fn = JS_GetPropertyStr(ctx, options, "transform");
    stream->transform_callback = JS_DupValue(ctx, transform_fn);
    JS_FreeValue(ctx, transform_fn);
    ```
  - **Lines reused**: ~60 lines
  - **New lines**: ~20 lines
  - **Effort**: 1.5 hours → **0.5 hours** (67% reduction)
  - **Test**: Constructor works

- **Task 4.2.3**: Implement `_transform(chunk, encoding, callback)`
  - **Reuse from streams.c**: Promise callback pattern (lines 692-710)
  - **Adaptation**: Node.js callback style instead of promises
    ```c
    // Reuse promise callback pattern structure
    // From streams.c (lines 692-710) - promise creation pattern:
    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);

    // Adapt to Node.js callback style:
    JSValue transform_args[] = {chunk, encoding, callback};
    JSValue result = JS_Call(ctx, stream->transform_callback, obj, 3, transform_args);
    ```
  - **Lines reused**: ~20 lines (callback pattern)
  - **New lines**: ~40 lines (transform logic)
  - **Effort**: 2 hours → **1 hour** (50% reduction)
  - **Test**: Subclass can override

- **Task 4.2.4**: Implement `_flush(callback)`
  - **Reuse from**: Same callback pattern as _transform
  - **Adaptation**: Single callback argument
  - **Lines reused**: ~15 lines
  - **New lines**: ~25 lines
  - **Effort**: 1 hour → **0.5 hours** (50% reduction)
  - **Test**: Flush called on end

**Total Group 4.4 Effort**: 6.5 hours → **3 hours** (54% reduction)

---

#### **Group 4.5: Transform Operations [PARALLEL]** (Tasks 4.2.5-4.2.12)
**Duration**: 10 hours → **5 hours with reuse**
**Dependencies**: [D:4.2.4]

All 8 tasks can run **simultaneously**:

- **Task 4.2.5**: Implement transform buffering
  - **Reuse from streams.c**: Queue management (lines 34-158)
    ```c
    // Reuse queue structure (lines 34-39)
    typedef struct TransformBuffer {
      JSValue* chunks;
      size_t size;
      size_t capacity;
    } TransformBuffer;

    // Reuse expansion logic (lines 137-143)
    if (buffer->size >= buffer->capacity) {
      size_t new_capacity = buffer->capacity * 2 + 1;
      JSValue* new_queue = realloc(buffer->chunks, new_capacity * sizeof(JSValue));
      buffer->chunks = new_queue;
      buffer->capacity = new_capacity;
    }
    ```
  - **Lines reused**: ~50 lines
  - **New lines**: ~30 lines
  - **Effort**: 1.5 hours → **0.5 hours** (67% reduction)

- **Task 4.2.6**: Implement `transform.push()` in context
  - **Reuse from**: Phase 2 Readable push() (already complete)
  - **Lines reused**: ~40 lines
  - **New lines**: ~20 lines
  - **Effort**: 1.5 hours → **0.5 hours** (67% reduction)

- **Task 4.2.7**: Connect write side to transform
  - **Reuse from**: Phase 3 Writable write() (already complete)
  - **Lines reused**: ~30 lines
  - **New lines**: ~30 lines (trigger transform)
  - **Effort**: 1.5 hours → **1 hour** (33% reduction)

- **Task 4.2.8**: Connect transform to read side
  - **Reuse from**: Phase 2 Readable read() (already complete)
  - **Lines reused**: ~30 lines
  - **New lines**: ~30 lines
  - **Effort**: 1.5 hours → **1 hour** (33% reduction)

- **Task 4.2.9**: Implement PassThrough class
  - **Reuse from**: Transform class (just created)
  - **Adaptation**: Identity transform (no-op)
    ```c
    // PassThrough is trivial - just pass data through
    typedef NodeTransformStream NodePassThroughStream;  // Same structure

    // Constructor with default transform
    function default_transform(chunk, encoding, callback) {
      this.push(chunk);  // Pass through unchanged
      callback();
    }
    ```
  - **Lines reused**: ~80 lines (all of Transform)
  - **New lines**: ~10 lines (default transform)
  - **Effort**: 1 hour → **0.5 hours** (50% reduction)

- **Task 4.2.10**: Implement transform error handling
  - **Reuse from streams.c**: Error handling (lines 220-276)
    ```c
    // Reuse error propagation pattern (lines 220-276)
    controller->errored = true;
    controller->error_value = JS_DupValue(ctx, error_value);

    // Adapt: Propagate to both readable and writable sides
    error_readable_side(stream, error_value);
    error_writable_side(stream, error_value);
    ```
  - **Lines reused**: ~30 lines
  - **New lines**: ~20 lines
  - **Effort**: 1 hour → **0.5 hours** (50% reduction)

- **Task 4.2.11**: Implement transform backpressure
  - **Reuse from**: Phase 2 backpressure (already complete)
  - **Lines reused**: ~40 lines
  - **New lines**: ~30 lines (transform-specific logic)
  - **Effort**: 2 hours → **1 hour** (50% reduction)

- **Task 4.2.12**: Implement transform state tracking
  - **Reuse from streams.c**: State management patterns
  - **Lines reused**: ~20 lines
  - **New lines**: ~20 lines
  - **Effort**: 1 hour → **0.5 hours** (50% reduction)

**Total Group 4.5 Effort**: 11 hours → **5.5 hours** (50% reduction)

---

### Phase 4 Summary

| Task Group | Sequential/Parallel | Original Effort | With Reuse | Savings |
|------------|---------------------|-----------------|------------|---------|
| 4.1 Duplex Foundation (2 tasks) | Sequential | 4h | 2.5h | 37% |
| 4.2 Duplex Operations (4 tasks) | **Parallel** | 6.5h | 3h | 54% |
| 4.3 Duplex Advanced (2 tasks) | **Parallel** | 4h | 2h | 50% |
| 4.4 Transform Foundation (4 tasks) | Sequential | 6.5h | 3h | 54% |
| 4.5 Transform Operations (8 tasks) | **Parallel** | 11h | 5.5h | 50% |
| **TOTAL** | | **32h** | **16h** | **50%** |

**Parallelization**: 14/20 tasks (70%) can run in parallel
**Code Reuse**: ~450 lines from streams.c, ~300 lines from Phase 2/3
**New Code Required**: ~400 lines
**Critical Path**: 4.1 → 4.2 → 4.3 → 4.4 → 4.5 (16 hours with reuse)

---

## Phase 5: Utility Functions (15 tasks)

**Goal**: Implement module-level utility functions
**Status**: ⏳ NOT STARTED
**Estimated Duration**: 2-3 days → **1-2 days with reuse**

### Code Reuse Analysis

#### Reusable Components from streams.c

1. **Promise Creation Pattern** (Lines 566-710, 1481-1489)
   - Used by: pipeline(), finished()
   - Reuse: 80% - Direct promise handling patterns
   - Effort: 5 hours → **1 hour** (80% reduction)

2. **Stream Lifecycle Detection** (Lines 160-218, 417-429)
   - Used by: finished() function
   - Reuse: 60% - Close/error state detection
   - Effort: 3 hours → **1.5 hours** (50% reduction)

3. **Stream Chaining** (Lines 1462-1478)
   - Used by: pipeline(), compose()
   - Reuse: 40% - Write → read connection pattern
   - Effort: 4 hours → **2.5 hours** (37% reduction)

### Parallel Execution Groups

#### **Group 5.1: pipeline() Function [SEQUENTIAL]** (Tasks 5.1.1-5.1.5)
**Duration**: 8 hours → **4 hours with reuse**

- **Task 5.1.1**: Implement `stream.pipeline(...streams, callback)`
  - **Reuse from streams.c**: Promise pattern (lines 566-710)
  - **Adaptation**: Node.js callback style + multiple streams
    ```c
    // Reuse promise capability pattern
    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);

    // Adapt: Connect multiple streams
    for (int i = 0; i < stream_count - 1; i++) {
      connect_streams(streams[i], streams[i+1]);  // Pipe each to next
    }

    // Wrap promise in callback style
    promise.then(
      () => callback(null),
      (err) => callback(err)
    );
    ```
  - **Lines reused**: ~60 lines (promise handling)
  - **New lines**: ~100 lines (multi-stream logic)
  - **Effort**: 3 hours → **2 hours** (33% reduction)
  - **Test**: Pipeline connects streams

**Subtasks 5.1.2-5.1.5 can run in PARALLEL after 5.1.1**:

- **Task 5.1.2**: Implement pipeline error propagation
  - **Reuse from streams.c**: Error handling (lines 220-276, 953-972)
  - **Lines reused**: ~40 lines
  - **New lines**: ~40 lines (multi-stream propagation)
  - **Effort**: 2 hours → **1 hour** (50% reduction)

- **Task 5.1.3**: Implement pipeline cleanup
  - **Reuse from streams.c**: Finalizer patterns (lines 285-296, 996-1007)
  - **Lines reused**: ~30 lines
  - **New lines**: ~40 lines (cleanup all streams)
  - **Effort**: 2 hours → **1 hour** (50% reduction)

- **Task 5.1.4**: Implement pipeline backpressure
  - **Reuse from**: Phase 2/3 backpressure (already complete)
  - **Lines reused**: ~50 lines
  - **New lines**: ~30 lines
  - **Effort**: 2 hours → **1 hour** (50% reduction)

- **Task 5.1.5**: Support async generators in pipeline
  - **Reuse from**: Phase 2 async iterator (already complete)
  - **Lines reused**: ~40 lines
  - **New lines**: ~50 lines (generator integration)
  - **Effort**: 2 hours → **1.5 hours** (25% reduction)

**Total Group 5.1 Effort**: 11 hours → **6.5 hours** (41% reduction)

---

#### **Group 5.2: finished() Function [PARALLEL after 5.1.1]** (Tasks 5.2.1-5.2.3)
**Duration**: 4 hours → **2 hours with reuse**

All 3 tasks can run **simultaneously**:

- **Task 5.2.1**: Implement `stream.finished(stream, callback)`
  - **Reuse from streams.c**:
    - Lines 160-218 (controller close detection)
    - Lines 417-429 (stream close/error)
    ```c
    // Reuse close detection pattern
    if (controller->closed) {
      callback(null);  // Stream finished successfully
    } else if (controller->errored) {
      callback(controller->error_value);  // Stream errored
    } else {
      // Wait for close/error events
      attach_finish_listeners(stream, callback);
    }
    ```
  - **Lines reused**: ~50 lines
  - **New lines**: ~40 lines
  - **Effort**: 2 hours → **1 hour** (50% reduction)

- **Task 5.2.2**: Implement finished error detection
  - **Reuse from streams.c**: Error detection (lines 228-275)
  - **Lines reused**: ~30 lines
  - **New lines**: ~20 lines
  - **Effort**: 1.5 hours → **0.5 hours** (67% reduction)

- **Task 5.2.3**: Implement finished cleanup
  - **Reuse from streams.c**: Listener cleanup patterns
  - **Lines reused**: ~20 lines
  - **New lines**: ~20 lines
  - **Effort**: 1 hour → **0.5 hours** (50% reduction)

**Total Group 5.2 Effort**: 4.5 hours → **2 hours** (56% reduction)

---

#### **Group 5.3: Additional Utilities [PARALLEL]** (Tasks 5.3.1-5.3.7)
**Duration**: 12 hours → **6 hours with reuse**

All 7 tasks can run **simultaneously**:

- **Task 5.3.1**: Implement `Readable.from(iterable, options)`
  - **Reuse from**: Phase 2 async iterator (already complete)
  - **Lines reused**: ~60 lines
  - **New lines**: ~40 lines
  - **Effort**: 2 hours → **1 hour** (50% reduction)

- **Task 5.3.2**: Implement `stream.addAbortSignal(signal, stream)`
  - **Reuse from**: Phase 1 destroy() (already complete)
  - **Lines reused**: ~30 lines
  - **New lines**: ~30 lines
  - **Effort**: 1.5 hours → **1 hour** (33% reduction)

- **Task 5.3.3**: Implement `stream.compose(...streams)`
  - **Reuse from**: pipeline() (just created)
  - **Lines reused**: ~70 lines (pipeline logic)
  - **New lines**: ~30 lines (return duplex)
  - **Effort**: 2 hours → **1 hour** (50% reduction)

- **Task 5.3.4**: Implement `stream.duplexPair()`
  - **Reuse from**: Phase 4 Duplex (already complete)
  - **Lines reused**: ~80 lines
  - **New lines**: ~40 lines (create connected pair)
  - **Effort**: 1.5 hours → **0.5 hours** (67% reduction)

- **Task 5.3.5**: Implement `Readable.toWeb(streamReadable)`
  - **Reuse from streams.c**: ReadableStream constructor (lines 303-363)
  - **Adaptation**: Convert Node.js Readable → WHATWG ReadableStream
    ```c
    // Create WHATWG ReadableStream
    JSValue whatwg_stream = create_readable_stream(ctx);

    // Connect Node.js readable to WHATWG controller
    node_readable.on('data', (chunk) => {
      whatwg_controller.enqueue(chunk);  // Reuse enqueue (lines 95-158)
    });

    node_readable.on('end', () => {
      whatwg_controller.close();  // Reuse close (lines 160-218)
    });
    ```
  - **Lines reused**: ~100 lines (WHATWG stream creation)
  - **New lines**: ~60 lines (conversion logic)
  - **Effort**: 3 hours → **1.5 hours** (50% reduction)

- **Task 5.3.6**: Implement `Readable.fromWeb(readableStream, options)`
  - **Reuse from**: Phase 2 Readable (already complete) + streams.c reader (lines 612-710)
  - **Adaptation**: WHATWG ReadableStream → Node.js Readable
    ```c
    // Get WHATWG reader
    JSValue reader = JS_Call(ctx, getReader, whatwg_stream, 0, NULL);

    // Create Node.js Readable with _read() that calls reader.read()
    function _read() {
      reader.read().then(({value, done}) => {  // Reuse read (lines 612-710)
        if (done) {
          this.push(null);  // Signal EOF
        } else {
          this.push(value);
        }
      });
    }
    ```
  - **Lines reused**: ~80 lines (reader pattern)
  - **New lines**: ~60 lines
  - **Effort**: 3 hours → **1.5 hours** (50% reduction)

- **Task 5.3.7**: Implement Writable.toWeb/fromWeb
  - **Reuse from**: Similar to Readable conversions + streams.c writer (lines 1449-1490)
  - **Lines reused**: ~120 lines (patterns from Readable conversions)
  - **New lines**: ~80 lines
  - **Effort**: 3 hours → **1.5 hours** (50% reduction)

**Total Group 5.3 Effort**: 16 hours → **8 hours** (50% reduction)

---

### Phase 5 Summary

| Task Group | Sequential/Parallel | Original Effort | With Reuse | Savings |
|------------|---------------------|-----------------|------------|---------|
| 5.1 pipeline() (5 tasks) | Sequential → **Parallel after 5.1.1** | 11h | 6.5h | 41% |
| 5.2 finished() (3 tasks) | **Parallel** | 4.5h | 2h | 56% |
| 5.3 Other utilities (7 tasks) | **Parallel** | 16h | 8h | 50% |
| **TOTAL** | | **31.5h** | **16.5h** | **48%** |

**Parallelization**: 12/15 tasks (80%) can run in parallel after initial setup
**Code Reuse**: ~650 lines from streams.c + ~400 lines from Phase 2/3/4
**New Code Required**: ~550 lines
**Critical Path**: 5.1.1 → {5.1.2-5.1.5, 5.2, 5.3 all parallel} (8.5 hours with reuse)

---

## Phase 6: Promises API (5 tasks)

**Goal**: Implement promise-based API in node:stream/promises
**Status**: ⏳ NOT STARTED
**Estimated Duration**: 1-2 days → **0.5-1 day with reuse**

### Code Reuse Analysis

**Key Insight**: This phase has the **HIGHEST reuse potential (90%)** because:
1. WHATWG Streams are already promise-based
2. Phase 5 utilities already implemented (just need promise wrappers)
3. Promise patterns heavily used in streams.c

### Parallel Execution Groups

#### **Group 6.1: Promises Module Setup [SEQUENTIAL]** (Task 6.1.1)
**Duration**: 1 hour → **0.5 hours with reuse**

- **Task 6.1.1**: Create `stream/promises` module structure
  - **Reuse from**: Existing module structure (src/node/node_modules.c)
  - **Lines reused**: ~30 lines (module registration pattern)
  - **New lines**: ~20 lines
  - **Effort**: 1 hour → **0.5 hours** (50% reduction)

---

#### **Group 6.2: Promise Wrappers [PARALLEL]** (Tasks 6.1.2-6.1.5)
**Duration**: 5 hours → **2 hours with reuse**
**Dependencies**: [D:6.1.1]

All 4 tasks can run **simultaneously** - they're just wrappers around Phase 5 functions:

- **Task 6.1.2**: Implement `promises.pipeline(...streams)`
  - **Reuse from**:
    - Phase 5 pipeline() (just created) - 95% reuse
    - streams.c promise pattern (lines 566-710)
  - **Adaptation**: Remove callback, return promise directly
    ```c
    // Phase 5 callback version:
    function pipeline(...streams, callback) {
      // ... implementation
      callback(err);
    }

    // Phase 6 promise version (wrapper):
    function pipeline_promise(...streams) {
      return new Promise((resolve, reject) => {
        pipeline(...streams, (err) => {  // Reuse Phase 5 implementation
          if (err) reject(err);
          else resolve();
        });
      });
    }
    ```
  - **Lines reused**: ~150 lines (all of Phase 5 pipeline)
  - **New lines**: ~20 lines (promise wrapper)
  - **Effort**: 2 hours → **0.5 hours** (75% reduction)
  - **Test**: Promise resolves correctly

- **Task 6.1.3**: Implement `promises.finished(stream)`
  - **Reuse from**: Phase 5 finished() - 95% reuse
  - **Lines reused**: ~90 lines
  - **New lines**: ~15 lines (promise wrapper)
  - **Effort**: 1.5 hours → **0.5 hours** (67% reduction)
  - **Test**: Promise resolves when stream done

- **Task 6.1.4**: Export other utilities from promises
  - **Reuse from**: Phase 5 utilities - 100% reuse (no adaptation needed)
  - **Lines reused**: ~200 lines (re-export)
  - **New lines**: ~10 lines (export statements)
  - **Effort**: 1 hour → **0.5 hours** (50% reduction)
  - **Test**: All APIs available

- **Task 6.1.5**: Implement promise error handling
  - **Reuse from streams.c**: Error handling (lines 220-276, 953-972)
  - **Adaptation**: Promise rejection instead of callbacks
    ```c
    // Reuse error detection pattern
    if (controller->errored) {
      // Reject promise with error
      JSValue reject_args[] = {controller->error_value};
      JS_Call(ctx, promise_reject, JS_UNDEFINED, 1, reject_args);
    }
    ```
  - **Lines reused**: ~40 lines
  - **New lines**: ~20 lines
  - **Effort**: 1.5 hours → **0.5 hours** (67% reduction)
  - **Test**: Promises reject correctly

**Total Group 6.2 Effort**: 6 hours → **2 hours** (67% reduction)

---

### Phase 6 Summary

| Task Group | Sequential/Parallel | Original Effort | With Reuse | Savings |
|------------|---------------------|-----------------|------------|---------|
| 6.1 Module setup (1 task) | Sequential | 1h | 0.5h | 50% |
| 6.2 Promise wrappers (4 tasks) | **Parallel** | 6h | 2h | 67% |
| **TOTAL** | | **7h** | **2.5h** | **64%** |

**Parallelization**: 4/5 tasks (80%) can run in parallel
**Code Reuse**: ~480 lines from Phase 5 + ~80 lines from streams.c
**New Code Required**: ~85 lines (mostly promise wrappers)
**Critical Path**: 6.1 → 6.2 (2.5 hours with reuse)

---

## Overall Execution Strategy

### Combined Phase 4-6 Summary

| Phase | Tasks | Original Effort | With Reuse | Savings | Parallelizable |
|-------|-------|-----------------|------------|---------|----------------|
| Phase 4 | 20 | 32h (4 days) | 16h (2 days) | 50% | 14/20 (70%) |
| Phase 5 | 15 | 31.5h (4 days) | 16.5h (2 days) | 48% | 12/15 (80%) |
| Phase 6 | 5 | 7h (1 day) | 2.5h (0.5 days) | 64% | 4/5 (80%) |
| **TOTAL** | **40** | **70.5h (9 days)** | **35h (4.5 days)** | **50%** | **30/40 (75%)** |

### Critical Path Analysis

**Longest Sequential Chain** (with reuse):
1. Phase 4.1: Duplex Foundation (2.5h)
2. Phase 4.2: Duplex Operations (3h) - parallel, longest task
3. Phase 4.3: Duplex Advanced (2h) - parallel, longest task
4. Phase 4.4: Transform Foundation (3h)
5. Phase 4.5: Transform Operations (5.5h) - parallel, longest task
6. Phase 5.1.1: Pipeline base (2h)
7. Phase 5.1.2-5: Pipeline features (1.5h) - parallel, longest task
8. Phase 6.1: Module setup (0.5h)
9. Phase 6.2: Promise wrappers (0.5h) - parallel, longest task

**Total Critical Path**: ~20.5 hours (2.5 days)

### Parallelization Opportunities

**Maximum Parallel Tasks at Peak**:
- During Phase 4.5: 8 tasks simultaneously
- During Phase 5.3: 7 tasks simultaneously
- During Phase 6.2: 4 tasks simultaneously

**Ideal Team Distribution** (if multiple developers):
- **Developer 1**: Duplex/Transform core (Phase 4.1-4.4)
- **Developer 2**: Transform operations (Phase 4.5) + pipeline (Phase 5.1)
- **Developer 3**: Utilities (Phase 5.2-5.3) + promises (Phase 6)

**Solo Developer Execution**:
- Follow critical path
- Implement parallel groups in order of dependency
- Total time: ~4.5 days (instead of 9 days without reuse)

---

## Code Reuse Metrics

### Lines of Code Breakdown

| Source | Lines Reused | Adaptation Needed | New Lines | Total |
|--------|--------------|-------------------|-----------|-------|
| **streams.c** | 780 | 40% modification | 520 | 1300 |
| **Phase 2 (Readable)** | 350 | 20% modification | 280 | 630 |
| **Phase 3 (Writable)** | 380 | 20% modification | 300 | 680 |
| **New implementation** | - | - | 815 | 815 |
| **TOTAL** | **1510** | | **1915** | **3425** |

**Reuse Percentage**: 44% of code from existing sources
**Effort Reduction**: 50% time savings due to:
- Copy-paste of patterns: 20% time saved
- Understanding existing code: 15% time saved
- Testing existing patterns: 15% time saved

### Function-Level Reuse Mapping

| Function Category | streams.c Lines | Reuse % | Adaptation |
|-------------------|-----------------|---------|------------|
| **Queue Management** | 34-158 (125 lines) | 60% | Change queue type from string to JSValue |
| **Promise Handling** | 566-710 (145 lines) | 80% | Minimal - callback style wrapper |
| **State Management** | 279-430 (152 lines) | 50% | Add EventEmitter integration |
| **Controller Pattern** | 42-276 (235 lines) | 40% | Split readable/writable controllers |
| **Finalizers** | 285-296, 996-1007 (23 lines) | 70% | Combine both patterns |
| **Error Handling** | 220-276, 953-972 (75 lines) | 60% | Emit events instead of promises |

---

## Risk Mitigation & Quality Assurance

### High-Risk Areas

1. **Duplex State Coordination** (Phase 4.1.5)
   - **Risk**: Read/write sides interfering with each other
   - **Mitigation**: Reuse separate state tracking from streams.c (lines 279-283, 988-994)
   - **Test**: 10+ tests for state independence

2. **Transform Buffering** (Phase 4.2.5)
   - **Risk**: Data loss during transformation
   - **Mitigation**: Reuse proven queue management (lines 34-158)
   - **Test**: Round-trip data integrity tests

3. **Pipeline Error Propagation** (Phase 5.1.2)
   - **Risk**: Errors not destroying all streams
   - **Mitigation**: Reuse error handling patterns (lines 220-276)
   - **Test**: Error scenarios with 3+ streams

### Memory Safety Protocol

**MANDATORY for each task**:
1. Run with ASAN: `./target/debug/jsrt_m test/node/stream/test_*.js`
2. Check for leaks: `ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m ...`
3. Zero tolerance: ANY leak must be fixed before proceeding

**Expected Clean Patterns** (from streams.c):
- Finalizers (lines 54-88, 285-296, 996-1007): All values freed
- Promise cleanup (lines 73-84): Resolve/reject functions freed
- Queue cleanup (lines 67-70): All queue items freed

---

## Testing Strategy

### Test Coverage by Phase

**Phase 4 Tests** (20 test files, 50+ tests):
- test_duplex_core.js (10 tests) - Basic duplex operations
- test_duplex_halfopen.js (5 tests) - allowHalfOpen scenarios
- test_transform_core.js (10 tests) - Transform functionality
- test_passthrough.js (5 tests) - PassThrough streams
- test_transform_backpressure.js (8 tests) - Flow control
- test_duplex_events.js (12 tests) - Event coordination

**Phase 5 Tests** (15 test files, 40+ tests):
- test_pipeline.js (10 tests) - Pipeline functionality
- test_finished.js (8 tests) - Finished detection
- test_compose.js (5 tests) - Stream composition
- test_readable_from.js (8 tests) - Readable.from()
- test_whatwg_conversion.js (10 tests) - toWeb/fromWeb

**Phase 6 Tests** (5 test files, 15+ tests):
- test_promises_pipeline.js (5 tests) - Promise pipeline
- test_promises_finished.js (5 tests) - Promise finished
- test_promises_errors.js (5 tests) - Error handling

**Total**: 40 test files, 105+ test cases

### Validation Checklist

After each phase:
- [ ] `make format` - Code formatted
- [ ] `make test` - All tests pass (145 baseline + new tests)
- [ ] `make wpt` - WPT baseline maintained (90.6%)
- [ ] `./target/debug/jsrt_m test/...` - Zero ASAN errors
- [ ] `ASAN_OPTIONS=detect_leaks=1 ...` - Zero memory leaks
- [ ] Manual testing - Real-world scenarios work

---

## Timeline Estimate

### With Reuse Strategy (Best Case)

| Phase | Days | Developer |
|-------|------|-----------|
| Phase 4: Duplex & Transform | 2 days | Solo or split (4.1-4.4 + 4.5) |
| Phase 5: Utilities | 2 days | Solo or split (5.1-5.2 + 5.3) |
| Phase 6: Promises | 0.5 days | Solo |
| **TOTAL** | **4.5 days** | **Solo developer** |

### Without Reuse Strategy (Worst Case)

| Phase | Days |
|-------|------|
| Phase 4 | 4 days |
| Phase 5 | 4 days |
| Phase 6 | 1 day |
| **TOTAL** | **9 days** |

**Time Saved**: 4.5 days (50% reduction)

---

## Implementation Recommendations

### Execution Order

1. **Start with Phase 4.1-4.2** (Duplex core)
   - Highest foundation value
   - Unlocks Transform implementation
   - Can test immediately with Phase 2/3

2. **Parallel: Phase 4.5 + Phase 5.1** (Transform ops + pipeline)
   - Independent work streams
   - Both needed for Phase 6

3. **Finish with Phase 5.2-5.3** (Other utilities)
   - Can be done in any order
   - Mostly independent

4. **Final: Phase 6** (Promises)
   - Simplest phase (wrappers only)
   - Quick win at the end

### Code Quality Standards

**Mandatory for all tasks**:
1. **Copy existing patterns** - Don't reinvent (refer to specific line numbers)
2. **Add debug logging** - State transitions, errors
3. **Write tests first** - TDD approach
4. **Document adaptations** - Comment why code differs from streams.c
5. **ASAN validation** - Every commit

### Success Metrics

**Phase 4 Complete**:
- [ ] 20 tasks done
- [ ] 50+ tests passing
- [ ] Duplex + Transform + PassThrough working
- [ ] Zero memory leaks
- [ ] ~800 lines added

**Phase 5 Complete**:
- [ ] 15 tasks done
- [ ] 40+ tests passing
- [ ] pipeline(), finished(), compose() working
- [ ] WHATWG conversion working
- [ ] Zero memory leaks
- [ ] ~600 lines added

**Phase 6 Complete**:
- [ ] 5 tasks done
- [ ] 15+ tests passing
- [ ] Promises API working
- [ ] Zero memory leaks
- [ ] ~100 lines added

**Overall Complete**:
- [ ] 120/120 tasks (100%)
- [ ] 145+ existing tests + 105+ new tests passing
- [ ] WPT baseline maintained (90.6%)
- [ ] Zero ASAN errors
- [ ] ~1500 lines total added
- [ ] **50% time saved through code reuse**

---

## Appendix: Detailed Code Reuse Map

### streams.c → Node.js Stream Mapping

| streams.c Component | Lines | Node.js Target | Reuse % | Notes |
|---------------------|-------|----------------|---------|-------|
| **ReadableStream struct** | 279-283 | Duplex.readable_side | 50% | Add EventEmitter |
| **WritableStream struct** | 988-994 | Duplex.writable_side | 50% | Add EventEmitter |
| **Controller enqueue** | 95-158 | Readable.push() | 60% | Add event emission |
| **Controller close** | 160-218 | Readable end handling | 70% | Add 'end' event |
| **Controller error** | 220-276 | Error propagation | 60% | Add 'error' event |
| **Promise capability** | 566-710 | Promises API | 80% | Minimal adaptation |
| **Reader read()** | 612-710 | fromWeb() conversion | 70% | Event integration |
| **Writer write()** | 1449-1490 | toWeb() conversion | 70% | Event integration |
| **Queue management** | 34-158 | Transform buffering | 60% | JSValue instead of string |
| **Finalizers** | 54-88, 285-296, 996-1007 | All stream cleanup | 70% | Combine patterns |

### Phase 2/3 → Phase 4 Mapping

| Phase 2/3 Component | Lines | Phase 4 Target | Reuse % |
|---------------------|-------|----------------|---------|
| **Readable constructor** | readable.c ~50 | Duplex.readable_init | 80% |
| **Writable constructor** | writable.c ~60 | Duplex.writable_init | 80% |
| **Readable.read()** | readable.c ~80 | Duplex.read() | 90% |
| **Writable.write()** | writable.c ~90 | Duplex.write() | 90% |
| **Backpressure logic** | writable.c ~40 | Transform backpressure | 85% |
| **Event emission** | Both ~100 | Duplex events | 95% |

---

**END OF EXECUTION STRATEGY**

**Status**: Ready for implementation
**Confidence**: HIGH - 50% time savings achievable
**Risk**: LOW - Proven patterns, extensive reuse
**Recommendation**: Proceed with Phase 4 immediately
