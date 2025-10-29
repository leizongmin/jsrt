#+TITLE: Task Plan: Stream Test Fixes and CLI Compatibility
#+AUTHOR: Claude AI Assistant
#+DATE: 2025-10-29T10:00:00Z
#+STARTUP: overview indent
#+TODO: TODO IN-PROGRESS BLOCKED | DONE CANCELLED
#+PRIORITIES: A C B

* Task Metadata
:PROPERTIES:
:CREATED: 2025-10-29T10:00:00Z
:UPDATED: 2025-10-29T10:00:00Z
:STATUS: 🟡 PLANNING
:PROGRESS: 0/68
:COMPLETION: 0%
:END:

* 📋 Task Analysis & Breakdown

## Executive Summary

### Objective
Fix failing stream tests and implement CLI compatibility features to support real-world Node.js applications like `loose-envify`.

### Current Issues Identified
1. **`make test N=node/stream` failure** - `read()` throws "Not a readable stream" error
   - Root cause: `js_stream_get_data()` fails to retrieve stream data from regular instances
   - Impact: Core functionality broken, blocks all stream usage

2. **Transform constructor semantics** - Native constructors reject `Transform.call(this)` pattern
   - Used by `util.inherits()` for subclassing
   - Real-world packages like `loose-envify` rely on this pattern

3. **Process stdio streams** - `process.stdin`/`stdout` are plain objects
   - Missing `pipe()`, `read()`, `write()` methods
   - CLI tools cannot read/write to stdio

4. **`fs.ReadStream` missing** - File streaming APIs unimplemented
   - `fs.createReadStream()` not available
   - Prevents file-based piping workflows

### Success Criteria
- ✅ `make test N=node/stream` passes 100%
- ✅ Zero memory leaks (ASAN validation)
- ✅ WPT baseline maintained (no regressions)
- ✅ `Transform.call(this)` via `util.inherits` works
- ✅ `process.stdin.pipe(transform).pipe(process.stdout)` works
- ✅ `fs.createReadStream(__filename).pipe(writable)` works
- ✅ Real CLI example (`loose-envify`) runs successfully

### Implementation Strategy
**Critical Path**: Fix core read() issue → Transform dual-call → stdio streams → fs.ReadStream → CLI tests

**Parallel Opportunities**: Tests can be written in parallel with implementation once interfaces are defined

---

* 📝 Task Execution

** TODO [#A] Phase 0: Emergency Bugfix - Core read() Failure [S][R:HIGH][C:MEDIUM] :bugfix:critical:
DEADLINE: <2025-10-29 Wed>
:PROPERTIES:
:ID: phase-0
:CREATED: 2025-10-29T10:00:00Z
:DEPS: None
:PROGRESS: 0/8
:COMPLETION: 0%
:ESTIMATE: 2-4 hours
:END:

**Context**: The `read()` method fails with "Not a readable stream" error, blocking all stream functionality. This is a critical regression that must be fixed before any other work.

**Root Cause Analysis**: The `js_stream_get_data()` function is trying to retrieve the stream data but failing. Need to investigate:
1. Is opaque data being set correctly in constructor?
2. Is the class ID matching?
3. Is the holder pattern being used incorrectly?

*** TODO [#A] Task 0.1: Investigate read() failure [S][R:HIGH][C:SIMPLE]
:PROPERTIES:
:ID: 0.1
:CREATED: 2025-10-29T10:00:00Z
:DEPS: None
:FILES: src/node/stream/readable.c, src/node/stream/stream.c
:END:

**Objective**: Identify why `js_stream_get_data()` returns NULL for valid Readable instances

**Steps**:
1. Add debug logging in `js_readable_read()` to check `this_val`
2. Add debug logging in `js_stream_get_data()` to trace execution path
3. Check if opaque data is set in `js_readable_constructor()`
4. Verify class ID matches between constructor and getter
5. Test with minimal example: `new Readable().read()`

**Acceptance Criteria**:
- Identify exact point of failure in `js_stream_get_data()`
- Document root cause in code comments

*** TODO [#A] Task 0.2: Fix opaque data retrieval [S][R:HIGH][C:MEDIUM][D:0.1]
:PROPERTIES:
:ID: 0.2
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 0.1
:FILES: src/node/stream/stream.c
:END:

**Objective**: Repair the `js_stream_get_data()` function to correctly retrieve stream data

**Implementation Options**:
A. Fix direct opaque lookup (if constructor sets it correctly)
B. Fix holder pattern lookup (if using __jsrt_stream_impl property)
C. Simplify to always use direct opaque (remove holder complexity)

**Steps**:
1. Based on 0.1 findings, choose correct approach
2. Update `js_stream_get_data()` implementation
3. Ensure magic number validation works
4. Test with all stream types (Readable, Writable, Duplex, Transform)

**Acceptance Criteria**:
- `js_stream_get_data()` returns valid stream data for all types
- Magic number validation passes

*** TODO [#A] Task 0.3: Verify all stream methods work [P][R:MEDIUM][C:SIMPLE][D:0.2]
:PROPERTIES:
:ID: 0.3
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 0.2
:FILES: src/node/stream/*.c
:END:

**Objective**: Test that all stream methods can retrieve stream data correctly

**Test Cases**:
- `readable.read()` - retrieve data
- `readable.push()` - add data
- `writable.write()` - write data
- `writable.end()` - end stream
- `transform.write()` and `transform.read()` - bidirectional

**Acceptance Criteria**:
- All methods can access stream data
- No "Not a readable/writable stream" errors

*** TODO [#A] Task 0.4: Run baseline stream tests [S][R:MEDIUM][C:SIMPLE][D:0.3]
:PROPERTIES:
:ID: 0.4
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 0.3
:FILES: test/node/stream/test_basic.js
:END:

**Objective**: Verify `test_basic.js` passes completely

**Commands**:
```bash
./bin/jsrt test/node/stream/test_basic.js
make test N=node/stream
```

**Acceptance Criteria**:
- All basic tests pass
- Zero failures in test output

*** TODO [#A] Task 0.5: Memory safety validation [P][R:HIGH][C:SIMPLE][D:0.4]
:PROPERTIES:
:ID: 0.5
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 0.4
:FILES: All stream sources
:END:

**Objective**: Ensure no memory leaks from the fix

**Commands**:
```bash
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/node/stream/test_basic.js
```

**Acceptance Criteria**:
- Zero memory leaks reported
- Zero use-after-free errors
- Clean ASAN output

*** TODO [#A] Task 0.6: Code formatting and cleanup [P][R:LOW][C:TRIVIAL][D:0.5]
:PROPERTIES:
:ID: 0.6
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 0.5
:END:

**Commands**:
```bash
make format
```

**Acceptance Criteria**:
- Code formatted according to project standards

*** TODO [#A] Task 0.7: WPT baseline check [P][R:MEDIUM][C:SIMPLE][D:0.6]
:PROPERTIES:
:ID: 0.7
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 0.6
:END:

**Objective**: Ensure no regressions in web platform tests

**Commands**:
```bash
make wpt
```

**Acceptance Criteria**:
- WPT baseline maintained at 90.6%
- No new failures introduced

*** TODO [#A] Task 0.8: Document the fix [P][R:LOW][C:TRIVIAL][D:0.7]
:PROPERTIES:
:ID: 0.8
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 0.7
:END:

**Objective**: Add comments explaining the issue and fix

**Deliverables**:
- Code comments in `stream.c` explaining opaque data retrieval
- Update this plan with resolution details

**Acceptance Criteria**:
- Clear comments added
- Future developers can understand the fix

---

** TODO [#A] Phase 1: Transform Dual-Call Constructor [S][R:HIGH][C:COMPLEX] :milestone:m1:
DEADLINE: <2025-11-01 Fri>
:PROPERTIES:
:ID: phase-1
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-0
:PROGRESS: 0/18
:COMPLETION: 0%
:ESTIMATE: 1-2 days
:END:

**Goal**: Enable `Transform.call(this)` without `new` keyword, supporting `util.inherits()` pattern used by legacy Node.js modules.

**Technical Approach**:
1. Create hidden native holder object with JSStreamData
2. Allow constructor to work in both modes: `new Transform()` and `Transform.call(obj)`
3. Attach holder to subclass instance via hidden property `__jsrt_stream_impl`
4. Update stream methods to check holder when direct opaque lookup fails

**Risk**: Memory ownership complexity - must avoid double-free when subclass owns holder

*** TODO [#A] Task 1.1: Design holder pattern architecture [S][R:HIGH][C:COMPLEX]
:PROPERTIES:
:ID: 1.1
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-0
:FILES: Design document
:END:

**Objective**: Document the dual-call constructor approach

**Deliverables**:
- Architecture diagram showing object relationships
- Memory ownership rules
- Lifecycle management strategy

**Design Decisions**:
1. **Holder creation**: Create native object with JSStreamData opaque
2. **Attachment**: Use `__jsrt_stream_impl` hidden property
3. **Retrieval**: Check direct opaque first, then holder property
4. **Cleanup**: Finalizer on holder frees JSStreamData, not on subclass

**Acceptance Criteria**:
- Clear design documented in code comments
- Architecture reviewed for memory safety

*** TODO [#A] Task 1.2: Implement holder creation in Transform constructor [S][R:HIGH][C:COMPLEX][D:1.1]
:PROPERTIES:
:ID: 1.2
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.1
:FILES: src/node/stream/transform.c
:END:

**Objective**: Modify `js_transform_constructor()` to support both call modes

**Implementation**:
```c
JSValue js_transform_constructor(JSContext* ctx, JSValueConst new_target,
                                  int argc, JSValueConst* argv) {
  JSValue obj;
  bool is_constructor_call = !JS_IsUndefined(new_target);

  if (is_constructor_call) {
    // Normal: new Transform()
    obj = JS_NewObjectClass(ctx, js_transform_class_id);
  } else {
    // Legacy: Transform.call(this)
    // Use first argument as 'this', or create new object
    obj = (argc > 0 && JS_IsObject(argv[0])) ? JS_DupValue(ctx, argv[0])
                                               : JS_NewObject(ctx);
  }

  // Create holder with opaque stream data
  JSValue holder = JS_NewObjectClass(ctx, js_transform_class_id);
  JSStreamData* stream = calloc(1, sizeof(JSStreamData));
  stream->magic = JS_STREAM_MAGIC;
  // ... initialize stream ...
  JS_SetOpaque(holder, stream);

  // Attach holder to obj
  js_stream_attach_impl(ctx, obj, holder);

  return obj;
}
```

**Acceptance Criteria**:
- `new Transform()` creates working instance
- `Transform.call({})` creates working instance
- Both modes have valid holder with stream data

*** TODO [#A] Task 1.3: Update js_stream_get_data() to check holder [S][R:HIGH][C:MEDIUM][D:1.2]
:PROPERTIES:
:ID: 1.3
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.2
:FILES: src/node/stream/stream.c
:END:

**Objective**: Modify stream data retrieval to support holder pattern

**Implementation** (already exists, but verify it works):
```c
JSStreamData* js_stream_get_data(JSContext* ctx, JSValueConst this_val, JSClassID class_id) {
  // Try direct opaque first
  JSStreamData* stream = js_stream_try_get(this_val, class_id);
  if (stream && stream->magic == JS_STREAM_MAGIC) {
    return stream;
  }

  // Try holder property
  JSValue holder = js_stream_get_impl_holder(ctx, this_val, class_id);
  if (JS_IsUndefined(holder)) return NULL;

  stream = js_stream_try_get(holder, class_id);
  if (!stream || stream->magic != JS_STREAM_MAGIC) {
    JS_FreeValue(ctx, holder);
    return NULL;
  }

  JS_FreeValue(ctx, holder);
  return stream;
}
```

**Acceptance Criteria**:
- Direct opaque lookup works (normal constructor)
- Holder property lookup works (legacy call)
- Magic number validation prevents invalid access

*** TODO [#A] Task 1.4: Implement holder pattern for all stream constructors [P][R:MEDIUM][C:MEDIUM][D:1.3]
:PROPERTIES:
:ID: 1.4
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.3
:FILES: src/node/stream/readable.c, writable.c, duplex.c, passthrough.c
:END:

**Objective**: Apply holder pattern to Readable, Writable, Duplex, PassThrough

**Rationale**: Consistency - all stream types should support both modes, not just Transform

**Acceptance Criteria**:
- All 5 stream types support dual-call constructor
- All types can be used with `util.inherits()`

*** TODO [#A] Task 1.5: Add magic number validation [P][R:HIGH][C:SIMPLE][D:1.4]
:PROPERTIES:
:ID: 1.5
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.4
:FILES: src/node/stream/stream_internal.h
:END:

**Objective**: Define and enforce magic number validation

**Implementation**:
```c
#define JS_STREAM_MAGIC 0x5354524D  // "STRM" in hex
```

**Validation Points**:
- In `js_stream_get_data()` before returning
- In finalizer before freeing
- In all public methods (defensive programming)

**Acceptance Criteria**:
- Magic number validated everywhere stream data is accessed
- Invalid magic causes NULL return, not crash

*** TODO [#A] Task 1.6: Write util.inherits() test cases [P][R:MEDIUM][C:SIMPLE][D:1.2]
:PROPERTIES:
:ID: 1.6
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.2
:FILES: test/node/stream/test_transform_inherits.js
:END:

**Objective**: Comprehensive test coverage for dual-call pattern

**Test Cases**:
```javascript
// Test 1: Normal constructor
const t1 = new Transform();
assert.ok(t1 instanceof Transform);

// Test 2: util.inherits() pattern
function CustomTransform() {
  Transform.call(this);
}
util.inherits(CustomTransform, Transform);
const t2 = new CustomTransform();
assert.ok(t2 instanceof Transform);
assert.ok(t2 instanceof CustomTransform);

// Test 3: Methods work
assert.strictEqual(typeof t2.write, 'function');
assert.strictEqual(typeof t2.read, 'function');
t2.write('test');
assert.strictEqual(t2.read(), 'test');

// Test 4: _transform override
function UpperTransform() {
  Transform.call(this);
}
util.inherits(UpperTransform, Transform);
UpperTransform.prototype._transform = function(chunk, enc, callback) {
  this.push(chunk.toString().toUpperCase());
  callback();
};
const t3 = new UpperTransform();
t3.write('hello');
assert.strictEqual(t3.read(), 'HELLO');

// Test 5: Multiple inheritance levels
function BaseTransform() {
  Transform.call(this);
}
util.inherits(BaseTransform, Transform);
function DerivedTransform() {
  BaseTransform.call(this);
}
util.inherits(DerivedTransform, BaseTransform);
const t4 = new DerivedTransform();
assert.ok(t4 instanceof Transform);
```

**Acceptance Criteria**:
- All 5+ test cases pass
- Tests cover both `new` and `call()` patterns
- Tests verify method availability

*** TODO [#A] Task 1.7: Test with real loose-envify pattern [P][R:HIGH][C:SIMPLE][D:1.6]
:PROPERTIES:
:ID: 1.7
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.6
:FILES: test/node/stream/test_loose_envify_pattern.js
:END:

**Objective**: Verify exact pattern used by `loose-envify` works

**Test Implementation**:
```javascript
// Simplified loose-envify pattern
const stream = require('node:stream');
const util = require('node:util');

function LooseEnvify(filename) {
  if (!(this instanceof LooseEnvify)) return new LooseEnvify(filename);
  stream.Transform.call(this);
  this._data = '';
}

util.inherits(LooseEnvify, stream.Transform);

LooseEnvify.prototype._transform = function(chunk, encoding, callback) {
  this._data += chunk;
  callback();
};

LooseEnvify.prototype._flush = function(callback) {
  // Simplified: just output accumulated data
  this.push(this._data);
  callback();
};

// Test it
const le = new LooseEnvify('test.js');
assert.ok(le instanceof stream.Transform);
le.write('process.env.NODE_ENV');
le.end();
const result = le.read();
assert.strictEqual(result, 'process.env.NODE_ENV');
```

**Acceptance Criteria**:
- Test passes without errors
- Pattern identical to real `loose-envify` usage

*** TODO [#A] Task 1.8: Memory leak testing for holder pattern [P][R:HIGH][C:SIMPLE][D:1.4]
:PROPERTIES:
:ID: 1.8
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.4
:END:

**Objective**: Ensure no memory leaks with holder pattern

**Test Strategy**:
1. Create 1000 instances using `Transform.call(this)`
2. Let them go out of scope
3. Force GC
4. Check ASAN output

**Commands**:
```bash
make jsrt_m
cat > /tmp/test_holder_leak.js << 'EOF'
const { Transform } = require('node:stream');
const util = require('node:util');

function CustomTransform() {
  Transform.call(this);
}
util.inherits(CustomTransform, Transform);

for (let i = 0; i < 1000; i++) {
  const t = new CustomTransform();
  t.write('test');
  t.read();
}
console.log('Created 1000 instances');
EOF

ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m /tmp/test_holder_leak.js
```

**Acceptance Criteria**:
- Zero memory leaks reported
- Holder finalizer correctly frees JSStreamData
- No double-free errors

*** TODO [#A] Task 1.9: Update all stream methods for holder support [P][R:MEDIUM][C:MEDIUM][D:1.3]
:PROPERTIES:
:ID: 1.9
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.3
:FILES: src/node/stream/*.c
:END:

**Objective**: Ensure all 60+ stream methods use `js_stream_get_data()` consistently

**Methods to Update**:
- Readable: read, push, pause, resume, pipe, unpipe, etc.
- Writable: write, end, cork, uncork, etc.
- Duplex: all readable + writable methods
- Transform: _transform, _flush, etc.

**Implementation Pattern**:
```c
static JSValue js_stream_method(JSContext* ctx, JSValueConst this_val,
                                 int argc, JSValueConst* argv) {
  JSStreamData* stream = js_stream_get_data(ctx, this_val, js_xxx_class_id);
  if (!stream) {
    return JS_ThrowTypeError(ctx, "Not a valid stream");
  }
  // ... method implementation ...
}
```

**Acceptance Criteria**:
- All methods use `js_stream_get_data()`
- No methods directly access opaque
- Consistent error messages

*** TODO [#A] Task 1.10: Test instanceof checks work correctly [P][R:MEDIUM][C:SIMPLE][D:1.4]
:PROPERTIES:
:ID: 1.10
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.4
:FILES: test/node/stream/test_instanceof.js
:END:

**Objective**: Verify prototype chain is correct for inherited classes

**Test Cases**:
```javascript
function CustomTransform() {
  Transform.call(this);
}
util.inherits(CustomTransform, Transform);

const t = new CustomTransform();

// Prototype chain checks
assert.ok(t instanceof CustomTransform);
assert.ok(t instanceof Transform);
assert.ok(t instanceof stream.Duplex);  // Transform extends Duplex
assert.ok(t instanceof stream.Stream);  // Base class

// Constructor checks
assert.strictEqual(t.constructor, CustomTransform);
assert.strictEqual(Object.getPrototypeOf(t).constructor, CustomTransform);
```

**Acceptance Criteria**:
- All instanceof checks pass
- Prototype chain correctly established
- Constructor property set correctly

*** TODO [#A] Task 1.11: Document holder pattern in code [P][R:LOW][C:TRIVIAL][D:1.9]
:PROPERTIES:
:ID: 1.11
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.9
:FILES: src/node/stream/stream.c
:END:

**Objective**: Add comprehensive documentation for holder pattern

**Documentation Sections**:
1. **Overview**: Why holder pattern is needed
2. **Architecture**: Object relationships diagram
3. **Memory ownership**: Who owns what
4. **Lifecycle**: Creation, access, destruction
5. **Examples**: Both normal and legacy usage

**Acceptance Criteria**:
- Clear block comment at top of stream.c
- Code comments at key points (constructor, getter, finalizer)

*** TODO [#A] Task 1.12: Run full stream test suite [S][R:MEDIUM][C:SIMPLE][D:1.9]
:PROPERTIES:
:ID: 1.12
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.9
:END:

**Commands**:
```bash
make test N=node/stream
```

**Acceptance Criteria**:
- All stream tests pass
- Zero failures

*** TODO [#A] Task 1.13: WPT baseline verification [P][R:MEDIUM][C:SIMPLE][D:1.12]
:PROPERTIES:
:ID: 1.13
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.12
:END:

**Commands**:
```bash
make wpt
```

**Acceptance Criteria**:
- WPT baseline maintained
- No new failures

*** TODO [#A] Task 1.14: Code formatting [P][R:LOW][C:TRIVIAL][D:1.13]
:PROPERTIES:
:ID: 1.14
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.13
:END:

**Commands**:
```bash
make format
```

*** TODO [#A] Task 1.15: Performance testing [P][R:LOW][C:SIMPLE][D:1.9]
:PROPERTIES:
:ID: 1.15
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.9
:FILES: test/node/stream/test_performance.js
:END:

**Objective**: Ensure holder pattern doesn't introduce significant overhead

**Test**:
```javascript
// Measure time for 10000 operations
const start = Date.now();
for (let i = 0; i < 10000; i++) {
  const t = new Transform();
  t.write('test');
  t.read();
}
const duration = Date.now() - start;
console.log(`Duration: ${duration}ms`);
// Should be under 1 second on typical hardware
```

**Acceptance Criteria**:
- Holder lookup overhead < 5% vs direct opaque
- No performance regression in existing tests

*** TODO [#A] Task 1.16: Edge case testing [P][R:MEDIUM][C:SIMPLE][D:1.9]
:PROPERTIES:
:ID: 1.16
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.9
:FILES: test/node/stream/test_edge_cases.js
:END:

**Objective**: Test unusual usage patterns

**Test Cases**:
- Call without arguments: `Transform.call()`
- Call with non-object: `Transform.call(null)`
- Call with frozen object: `Transform.call(Object.freeze({}))`
- Multiple inheritance levels (3+ deep)
- Mix of `new` and `call()` in same inheritance chain

**Acceptance Criteria**:
- All edge cases handled gracefully
- No crashes or memory corruption

*** TODO [#A] Task 1.17: Integration test with util.inherits [P][R:HIGH][C:SIMPLE][D:1.7,1.10,1.16]
:PROPERTIES:
:ID: 1.17
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.7,1.10,1.16
:FILES: test/node/stream/test_util_inherits_integration.js
:END:

**Objective**: Comprehensive integration test covering all aspects

**Test Scenarios**:
1. Basic inheritance
2. Override _transform
3. Override _flush
4. Piping inherited streams
5. Error handling in inherited streams
6. Event emission in inherited streams
7. Backpressure in inherited streams

**Acceptance Criteria**:
- All 7+ scenarios pass
- Real-world usage patterns validated

*** TODO [#A] Task 1.18: Milestone 1 validation [S][R:HIGH][C:SIMPLE][D:1.12,1.13,1.17]
:PROPERTIES:
:ID: 1.18
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 1.12,1.13,1.17
:END:

**Objective**: Verify M1 success criteria met

**Checklist**:
- [ ] `Transform.call(this)` works without throwing
- [ ] `util.inherits(CustomTransform, Transform)` creates working instances
- [ ] All stream tests pass
- [ ] Zero memory leaks (ASAN)
- [ ] WPT baseline maintained
- [ ] loose-envify pattern test passes

**Acceptance Criteria**:
- All checklist items pass
- M1 declared complete

---

** TODO [#B] Phase 2: Process Stdio Stream Adapters [S][R:MEDIUM][C:COMPLEX] :milestone:m2:
DEADLINE: <2025-11-03 Sun>
:PROPERTIES:
:ID: phase-2
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-1
:PROGRESS: 0/16
:COMPLETION: 0%
:ESTIMATE: 1.5-2 days
:END:

**Goal**: Replace placeholder `process.stdin`/`stdout`/`stderr` with real Readable/Writable stream wrappers backed by libuv TTY or file descriptors.

**Technical Approach**:
1. Create TTYReadStream and TTYWriteStream classes
2. Wrap libuv handles (uv_tty_t or uv_pipe_t)
3. Integrate with libuv event loop for async I/O
4. Initialize in process module during startup

**Risk**: Blocking I/O - must use libuv non-blocking mode, handle EAGAIN properly

*** TODO [#B] Task 2.1: Analyze current process.stdin/stdout [S][R:MEDIUM][C:SIMPLE]
:PROPERTIES:
:ID: 2.1
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-1
:FILES: src/node/node_process.c
:END:

**Objective**: Document current implementation and identify integration points

**Investigation**:
1. Where are stdin/stdout defined?
2. What are their current values?
3. Where should stream instances be created?
4. When should streams be initialized?

**Deliverables**:
- Documentation of current state
- Integration plan

*** TODO [#B] Task 2.2: Design TTY stream architecture [S][R:MEDIUM][C:COMPLEX][D:2.1]
:PROPERTIES:
:ID: 2.2
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.1
:FILES: Design document
:END:

**Objective**: Design TTY stream classes

**Architecture Decisions**:
1. **Class hierarchy**: TTYReadStream extends Readable, TTYWriteStream extends Writable
2. **libuv integration**: Store uv_tty_t or uv_pipe_t handle in JSStreamData
3. **Read callback**: uv_read_start() callback pushes data to readable buffer
4. **Write callback**: _write() calls uv_write() asynchronously
5. **Initialization**: During process module init, before any user code runs

**Deliverables**:
- Architecture diagram
- API design
- Memory ownership rules

*** TODO [#B] Task 2.3: Implement TTYReadStream class [S][R:MEDIUM][C:COMPLEX][D:2.2]
:PROPERTIES:
:ID: 2.3
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.2
:FILES: src/node/stream/tty.c (new)
:END:

**Objective**: Create TTYReadStream extending Readable

**Implementation**:
```c
typedef struct {
  JSStreamData base;  // Inherit from JSStreamData
  uv_tty_t handle;    // libuv TTY handle
  uv_loop_t* loop;    // Event loop
} JSTTYReadStream;

static void tty_read_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

static void tty_read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  JSTTYReadStream* tty = stream->data;
  if (nread > 0) {
    // Create Buffer and push to readable stream
    JSValue chunk = JS_NewArrayBufferCopy(ctx, buf->base, nread);
    js_readable_push(ctx, tty_obj, 1, &chunk);
  } else if (nread == UV_EOF) {
    // EOF - push null to end stream
    JSValue null_val = JS_NULL;
    js_readable_push(ctx, tty_obj, 1, &null_val);
  }
  free(buf->base);
}

JSValue js_tty_read_stream_constructor(JSContext* ctx, JSValueConst new_target,
                                        int argc, JSValueConst* argv) {
  // Create readable stream
  JSValue obj = js_readable_constructor(ctx, new_target, 0, NULL);

  // Initialize TTY handle
  JSTTYReadStream* tty = calloc(1, sizeof(JSTTYReadStream));
  int fd = 0;  // stdin
  uv_tty_init(loop, &tty->handle, fd, 1);  // 1 = readable
  tty->handle.data = tty;

  // Start reading
  uv_read_start((uv_stream_t*)&tty->handle, tty_read_alloc_cb, tty_read_cb);

  return obj;
}
```

**Acceptance Criteria**:
- TTYReadStream class created
- Extends Readable properly
- Reads from fd 0 (stdin)

*** TODO [#B] Task 2.4: Implement TTYWriteStream class [P][R:MEDIUM][C:COMPLEX][D:2.2]
:PROPERTIES:
:ID: 2.4
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.2
:FILES: src/node/stream/tty.c
:END:

**Objective**: Create TTYWriteStream extending Writable

**Implementation**:
```c
typedef struct {
  JSStreamData base;
  uv_tty_t handle;
  uv_loop_t* loop;
} JSTTYWriteStream;

static void tty_write_cb(uv_write_t* req, int status) {
  // Call write callback
  JSValue* cb = req->data;
  if (cb) {
    JSValue result = JS_Call(ctx, *cb, JS_UNDEFINED, 0, NULL);
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, *cb);
    free(cb);
  }
  free(req);
}

static JSValue js_tty_write_stream__write(JSContext* ctx, JSValueConst this_val,
                                          JSValue chunk, JSValue encoding, JSValue callback) {
  JSTTYWriteStream* tty = get_tty_write_stream(ctx, this_val);

  // Convert chunk to buffer
  size_t size;
  uint8_t* data = JS_GetArrayBuffer(ctx, &size, chunk);

  // Allocate write request
  uv_write_t* req = malloc(sizeof(uv_write_t));
  uv_buf_t buf = uv_buf_init((char*)data, size);

  // Store callback
  JSValue* cb_copy = malloc(sizeof(JSValue));
  *cb_copy = JS_DupValue(ctx, callback);
  req->data = cb_copy;

  // Write asynchronously
  uv_write(req, (uv_stream_t*)&tty->handle, &buf, 1, tty_write_cb);

  return JS_UNDEFINED;
}
```

**Acceptance Criteria**:
- TTYWriteStream class created
- Extends Writable properly
- Writes to fd 1 (stdout) or 2 (stderr)

*** TODO [#B] Task 2.5: Integrate TTY streams into process module [S][R:MEDIUM][C:MEDIUM][D:2.3,2.4]
:PROPERTIES:
:ID: 2.5
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.3,2.4
:FILES: src/node/node_process.c
:END:

**Objective**: Replace placeholder stdin/stdout/stderr with TTY streams

**Implementation**:
```c
// In process module init
void init_process_stdio(JSContext* ctx, JSValue process) {
  // Create stdin (TTYReadStream or pipe)
  JSValue stdin = create_stdio_stream(ctx, 0, true);  // fd 0, readable
  JS_SetPropertyStr(ctx, process, "stdin", stdin);

  // Create stdout
  JSValue stdout = create_stdio_stream(ctx, 1, false);  // fd 1, writable
  JS_SetPropertyStr(ctx, process, "stdout", stdout);

  // Create stderr
  JSValue stderr = create_stdio_stream(ctx, 2, false);  // fd 2, writable
  JS_SetPropertyStr(ctx, process, "stderr", stderr);
}

JSValue create_stdio_stream(JSContext* ctx, int fd, bool readable) {
  // Check if fd is TTY or pipe
  uv_handle_type type = uv_guess_handle(fd);

  if (type == UV_TTY) {
    return readable ? create_tty_read_stream(ctx, fd)
                    : create_tty_write_stream(ctx, fd);
  } else {
    // Pipe or file - use different stream type
    return readable ? create_pipe_read_stream(ctx, fd)
                    : create_pipe_write_stream(ctx, fd);
  }
}
```

**Acceptance Criteria**:
- process.stdin is TTYReadStream or ReadStream
- process.stdout is TTYWriteStream or WriteStream
- process.stderr is TTYWriteStream or WriteStream
- Stream type depends on fd type (TTY vs pipe)

*** TODO [#B] Task 2.6: Test stdin reading [P][R:MEDIUM][C:SIMPLE][D:2.5]
:PROPERTIES:
:ID: 2.6
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.5
:FILES: test/node/stream/test_stdin.js
:END:

**Objective**: Verify stdin stream reads data correctly

**Test**:
```javascript
// test_stdin.js
const assert = require('jsrt:assert');

assert.ok(process.stdin, 'stdin should exist');
assert.strictEqual(typeof process.stdin.read, 'function', 'stdin should have read()');
assert.strictEqual(typeof process.stdin.pipe, 'function', 'stdin should have pipe()');
assert.strictEqual(typeof process.stdin.pause, 'function', 'stdin should have pause()');

// Test reading
process.stdin.on('data', (chunk) => {
  console.log('Received:', chunk.toString());
});

process.stdin.resume();
```

**Manual Test**:
```bash
echo "test data" | ./bin/jsrt test/node/stream/test_stdin.js
```

**Acceptance Criteria**:
- stdin has all Readable methods
- stdin can read data from pipe
- Data event fires correctly

*** TODO [#B] Task 2.7: Test stdout writing [P][R:MEDIUM][C:SIMPLE][D:2.5]
:PROPERTIES:
:ID: 2.7
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.5
:FILES: test/node/stream/test_stdout.js
:END:

**Objective**: Verify stdout stream writes data correctly

**Test**:
```javascript
// test_stdout.js
const assert = require('jsrt:assert');

assert.ok(process.stdout, 'stdout should exist');
assert.strictEqual(typeof process.stdout.write, 'function', 'stdout should have write()');
assert.strictEqual(typeof process.stdout.end, 'function', 'stdout should have end()');

// Test writing
process.stdout.write('Hello ');
process.stdout.write('World\n');
```

**Manual Test**:
```bash
./bin/jsrt test/node/stream/test_stdout.js
# Should output: Hello World
```

**Acceptance Criteria**:
- stdout has all Writable methods
- stdout writes to console correctly
- Output appears immediately

*** TODO [#B] Task 2.8: Test stdin to stdout piping [P][R:HIGH][C:SIMPLE][D:2.6,2.7]
:PROPERTIES:
:ID: 2.8
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.6,2.7
:FILES: test/node/stream/test_stdio_pipe.js
:END:

**Objective**: Verify stdin can pipe to stdout

**Test**:
```javascript
// test_stdio_pipe.js
// Simple echo program
process.stdin.pipe(process.stdout);
```

**Manual Test**:
```bash
echo "test data" | ./bin/jsrt test/node/stream/test_stdio_pipe.js
# Should output: test data
```

**Acceptance Criteria**:
- Piping works
- Data flows from stdin to stdout
- No data loss

*** TODO [#B] Task 2.9: Test stdin through transform to stdout [P][R:HIGH][C:MEDIUM][D:2.8,phase-1]
:PROPERTIES:
:ID: 2.9
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.8,phase-1
:FILES: test/node/stream/test_stdio_transform.js
:END:

**Objective**: Full CLI pipeline test

**Test**:
```javascript
// test_stdio_transform.js
const { Transform } = require('node:stream');

const upperTransform = new Transform({
  transform(chunk, encoding, callback) {
    this.push(chunk.toString().toUpperCase());
    callback();
  }
});

process.stdin.pipe(upperTransform).pipe(process.stdout);
```

**Manual Test**:
```bash
echo "hello world" | ./bin/jsrt test/node/stream/test_stdio_transform.js
# Should output: HELLO WORLD
```

**Acceptance Criteria**:
- stdin → transform → stdout pipeline works
- Data is transformed correctly
- No crashes or hangs

*** TODO [#B] Task 2.10: Handle non-TTY stdin/stdout (pipes) [P][R:MEDIUM][C:MEDIUM][D:2.5]
:PROPERTIES:
:ID: 2.10
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.5
:FILES: src/node/stream/tty.c
:END:

**Objective**: Support piped input/output, not just TTY

**Implementation**:
- Detect fd type using `uv_guess_handle()`
- Use `uv_pipe_t` for pipes instead of `uv_tty_t`
- Use `uv_fs_t` for regular files

**Test Cases**:
```bash
# Piped input
echo "test" | ./bin/jsrt script.js

# Piped output
./bin/jsrt script.js | cat

# File input
./bin/jsrt script.js < input.txt

# File output
./bin/jsrt script.js > output.txt
```

**Acceptance Criteria**:
- All 4 test cases work
- Correct stream type used for each fd type

*** TODO [#B] Task 2.11: Memory leak testing [P][R:HIGH][C:SIMPLE][D:2.9]
:PROPERTIES:
:ID: 2.11
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.9
:END:

**Commands**:
```bash
make jsrt_m
echo "test data" | ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/node/stream/test_stdio_pipe.js
```

**Acceptance Criteria**:
- Zero memory leaks
- libuv handles properly closed

*** TODO [#B] Task 2.12: Handle EAGAIN and EWOULDBLOCK [P][R:MEDIUM][C:MEDIUM][D:2.5]
:PROPERTIES:
:ID: 2.12
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.5
:FILES: src/node/stream/tty.c
:END:

**Objective**: Properly handle non-blocking I/O errors

**Implementation**:
- In read callback, handle EAGAIN by returning and waiting for next event
- In write callback, handle EWOULDBLOCK by queuing write for later
- Set fds to non-blocking mode: `uv_tty_set_mode(&handle, UV_TTY_MODE_RAW)`

**Acceptance Criteria**:
- No busy loops on EAGAIN
- Writes queue properly when output blocks

*** TODO [#B] Task 2.13: Test stderr independently [P][R:LOW][C:SIMPLE][D:2.5]
:PROPERTIES:
:ID: 2.13
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.5
:FILES: test/node/stream/test_stderr.js
:END:

**Test**:
```javascript
process.stderr.write('Error message\n');
```

**Manual Test**:
```bash
./bin/jsrt test/node/stream/test_stderr.js 2>&1 | grep "Error message"
```

**Acceptance Criteria**:
- stderr writes independently of stdout
- stderr output appears on fd 2

*** TODO [#B] Task 2.14: Add isTTY property [P][R:LOW][C:SIMPLE][D:2.5]
:PROPERTIES:
:ID: 2.14
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.5
:FILES: src/node/stream/tty.c
:END:

**Objective**: Add `isTTY` boolean property to stdio streams

**Implementation**:
```javascript
assert.strictEqual(process.stdin.isTTY, true);  // If stdin is terminal
assert.strictEqual(process.stdin.isTTY, undefined);  // If stdin is pipe
```

**Acceptance Criteria**:
- `isTTY` property set correctly based on fd type
- Matches Node.js behavior

*** TODO [#B] Task 2.15: Document TTY stream implementation [P][R:LOW][C:TRIVIAL][D:2.9]
:PROPERTIES:
:ID: 2.15
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.9
:FILES: src/node/stream/tty.c
:END:

**Documentation Sections**:
1. libuv integration approach
2. Non-blocking I/O handling
3. TTY vs pipe detection
4. Memory ownership (uv handles)
5. Event loop integration

*** TODO [#B] Task 2.16: Milestone 2 validation [S][R:HIGH][C:SIMPLE][D:2.9,2.11]
:PROPERTIES:
:ID: 2.16
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 2.9,2.11
:END:

**Checklist**:
- [ ] `typeof process.stdin.pipe === 'function'`
- [ ] `process.stdin.pipe(transform).pipe(process.stdout)` works
- [ ] stdin reads from fd 0
- [ ] stdout writes to fd 1
- [ ] stderr writes to fd 2
- [ ] Pipes and TTYs both supported
- [ ] Zero memory leaks (ASAN)
- [ ] No blocking I/O

---

** TODO [#B] Phase 3: fs.ReadStream Implementation [S][R:MEDIUM][C:COMPLEX] :milestone:m3:
DEADLINE: <2025-11-05 Tue>
:PROPERTIES:
:ID: phase-3
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-2
:PROGRESS: 0/14
:COMPLETION: 0%
:ESTIMATE: 1.5 days
:END:

**Goal**: Implement `fs.ReadStream` and `fs.createReadStream()` for file-based streaming workflows.

**Technical Approach**:
1. Create ReadStream class extending Readable
2. Use libuv file operations (uv_fs_*)
3. Implement chunked reading with configurable buffer size
4. Support start/end/highWaterMark options
5. Integrate with existing fs module

*** TODO [#B] Task 3.1: Design fs.ReadStream architecture [S][R:MEDIUM][C:MEDIUM]
:PROPERTIES:
:ID: 3.1
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-2
:FILES: Design document
:END:

**Objective**: Design file streaming implementation

**Architecture Decisions**:
1. **Class**: ReadStream extends Readable
2. **File operations**: Use uv_fs_open, uv_fs_read, uv_fs_close
3. **Buffering**: Read chunks of highWaterMark size (default 64KB)
4. **Options**: start, end, highWaterMark, encoding, fd, autoClose
5. **Cleanup**: Auto-close fd on end/error if autoClose=true

*** TODO [#B] Task 3.2: Implement fs.ReadStream class [S][R:MEDIUM][C:COMPLEX][D:3.1]
:PROPERTIES:
:ID: 3.2
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.1
:FILES: src/node/fs/read_stream.c (new)
:END:

**Implementation Outline**:
```c
typedef struct {
  JSStreamData base;
  uv_fs_t open_req;
  uv_fs_t read_req;
  uv_fs_t close_req;
  uv_file fd;
  int64_t pos;
  int64_t start;
  int64_t end;
  bool auto_close;
  bool reading;
} JSReadStream;

static void read_stream_open_cb(uv_fs_t* req) {
  JSReadStream* rs = req->data;
  if (req->result < 0) {
    // Emit error
    emit_error(ctx, rs_obj, uv_strerror(req->result));
    return;
  }
  rs->fd = req->result;
  emit_event(ctx, rs_obj, "open", fd_value);
  // Start reading if in flowing mode
  if (rs->base.flowing) {
    read_stream_read_chunk(rs);
  }
}

static void read_stream_read_cb(uv_fs_t* req) {
  JSReadStream* rs = req->data;
  if (req->result < 0) {
    emit_error(ctx, rs_obj, uv_strerror(req->result));
    return;
  }
  if (req->result == 0) {
    // EOF
    js_readable_push(ctx, rs_obj, 1, &JS_NULL);
    if (rs->auto_close) {
      read_stream_close(rs);
    }
    return;
  }

  // Push data to stream
  JSValue chunk = JS_NewArrayBufferCopy(ctx, req->ptr, req->result);
  js_readable_push(ctx, rs_obj, 1, &chunk);

  rs->pos += req->result;

  // Continue reading if not paused and not at end
  if (rs->base.flowing && (rs->end == -1 || rs->pos < rs->end)) {
    read_stream_read_chunk(rs);
  }
}

static void read_stream_read_chunk(JSReadStream* rs) {
  size_t to_read = rs->base.options.high_water_mark;
  if (rs->end != -1 && rs->pos + to_read > rs->end) {
    to_read = rs->end - rs->pos;
  }

  uv_buf_t buf = uv_buf_init(malloc(to_read), to_read);
  uv_fs_read(loop, &rs->read_req, rs->fd, &buf, 1, rs->pos, read_stream_read_cb);
}
```

**Acceptance Criteria**:
- ReadStream class created
- Extends Readable
- Uses libuv async file operations
- Supports start/end range options

*** TODO [#B] Task 3.3: Implement fs.createReadStream() factory [S][R:MEDIUM][C:MEDIUM][D:3.2]
:PROPERTIES:
:ID: 3.3
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.2
:FILES: src/node/fs/node_fs.c
:END:

**Implementation**:
```c
static JSValue js_fs_create_read_stream(JSContext* ctx, JSValueConst this_val,
                                         int argc, JSValueConst* argv) {
  if (argc < 1) {
    return JS_ThrowTypeError(ctx, "path required");
  }

  const char* path = JS_ToCString(ctx, argv[0]);
  JSValue options = argc > 1 ? argv[1] : JS_UNDEFINED;

  // Parse options
  ReadStreamOptions opts = {0};
  parse_read_stream_options(ctx, options, &opts);

  // Create ReadStream instance
  JSValue rs = js_read_stream_constructor(ctx, JS_UNDEFINED, 2, (JSValueConst[]){argv[0], options});

  JS_FreeCString(ctx, path);
  return rs;
}

// Register in fs module
JS_SetPropertyStr(ctx, fs_obj, "createReadStream",
                 JS_NewCFunction(ctx, js_fs_create_read_stream, "createReadStream", 2));
JS_SetPropertyStr(ctx, fs_obj, "ReadStream",
                 JS_NewCFunction(ctx, js_read_stream_constructor, "ReadStream", 2));
```

**Acceptance Criteria**:
- `fs.createReadStream(path)` returns ReadStream
- `new fs.ReadStream(path)` works
- Options passed correctly

*** TODO [#B] Task 3.4: Test basic file reading [P][R:MEDIUM][C:SIMPLE][D:3.3]
:PROPERTIES:
:ID: 3.4
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.3
:FILES: test/node/stream/test_fs_read_stream.js
:END:

**Test**:
```javascript
const fs = require('node:fs');
const assert = require('jsrt:assert');

// Test 1: Read entire file
const rs = fs.createReadStream(__filename);
let data = '';
rs.on('data', (chunk) => {
  data += chunk.toString();
});
rs.on('end', () => {
  assert.ok(data.length > 0);
  assert.ok(data.includes('createReadStream'));
  console.log('✓ Read entire file');
});

// Test 2: Read with encoding
const rs2 = fs.createReadStream(__filename, { encoding: 'utf8' });
rs2.on('data', (chunk) => {
  assert.strictEqual(typeof chunk, 'string');
});
```

**Acceptance Criteria**:
- File read successfully
- Data chunks received
- End event fires
- Encoding option works

*** TODO [#B] Task 3.5: Test range reading (start/end options) [P][R:MEDIUM][C:SIMPLE][D:3.3]
:PROPERTIES:
:ID: 3.5
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.3
:FILES: test/node/stream/test_fs_read_stream_range.js
:END:

**Test**:
```javascript
// Read bytes 10-20
const rs = fs.createReadStream(__filename, { start: 10, end: 20 });
let data = '';
rs.on('data', (chunk) => {
  data += chunk.toString();
});
rs.on('end', () => {
  assert.strictEqual(data.length, 11);  // 10-20 inclusive
  console.log('✓ Range reading works');
});
```

**Acceptance Criteria**:
- start/end options respected
- Correct number of bytes read
- End event fires after range

*** TODO [#B] Task 3.6: Test piping file to stdout [P][R:HIGH][C:SIMPLE][D:3.4,phase-2]
:PROPERTIES:
:ID: 3.6
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.4,phase-2
:FILES: test/node/stream/test_fs_pipe_stdout.js
:END:

**Test**:
```javascript
// Simple cat program
const fs = require('node:fs');

const rs = fs.createReadStream(process.argv[2] || __filename);
rs.pipe(process.stdout);
```

**Manual Test**:
```bash
./bin/jsrt test/node/stream/test_fs_pipe_stdout.js /etc/hosts
# Should output contents of /etc/hosts
```

**Acceptance Criteria**:
- File contents appear on stdout
- Large files (>1MB) work correctly
- No data corruption

*** TODO [#B] Task 3.7: Test piping file through transform [P][R:HIGH][C:MEDIUM][D:3.6,phase-1]
:PROPERTIES:
:ID: 3.7
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.6,phase-1
:FILES: test/node/stream/test_fs_transform_pipe.js
:END:

**Test**:
```javascript
// Read file, transform to uppercase, output to stdout
const fs = require('node:fs');
const { Transform } = require('node:stream');

const upperTransform = new Transform({
  transform(chunk, encoding, callback) {
    this.push(chunk.toString().toUpperCase());
    callback();
  }
});

fs.createReadStream(__filename)
  .pipe(upperTransform)
  .pipe(process.stdout);
```

**Acceptance Criteria**:
- fs.ReadStream → Transform → stdout pipeline works
- Data transformed correctly
- Large files handled properly

*** TODO [#B] Task 3.8: Test error handling (missing file) [P][R:MEDIUM][C:SIMPLE][D:3.3]
:PROPERTIES:
:ID: 3.8
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.3
:FILES: test/node/stream/test_fs_read_stream_errors.js
:END:

**Test**:
```javascript
const rs = fs.createReadStream('/nonexistent/file.txt');
rs.on('error', (err) => {
  assert.ok(err);
  assert.ok(err.message.includes('ENOENT') || err.message.includes('no such file'));
  console.log('✓ Error handling works');
});
```

**Acceptance Criteria**:
- Error event fires for missing file
- Error message includes ENOENT
- Stream doesn't crash

*** TODO [#B] Task 3.9: Test autoClose option [P][R:LOW][C:SIMPLE][D:3.3]
:PROPERTIES:
:ID: 3.9
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.3
:FILES: test/node/stream/test_fs_autoclose.js
:END:

**Test**:
```javascript
// Test autoClose: false keeps fd open
const rs = fs.createReadStream(__filename, { autoClose: false });
let fd;
rs.on('open', (openFd) => {
  fd = openFd;
});
rs.on('end', () => {
  // fd should still be open
  assert.ok(fd !== undefined);
  // Manually close
  fs.close(fd, () => {
    console.log('✓ autoClose works');
  });
});
```

**Acceptance Criteria**:
- autoClose: true (default) closes fd automatically
- autoClose: false leaves fd open
- Manual close works

*** TODO [#B] Task 3.10: Test highWaterMark option [P][R:LOW][C:SIMPLE][D:3.3]
:PROPERTIES:
:ID: 3.10
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.3
:FILES: test/node/stream/test_fs_highwatermark.js
:END:

**Test**:
```javascript
// Test small highWaterMark
const rs = fs.createReadStream(__filename, { highWaterMark: 16 });
rs.on('data', (chunk) => {
  assert.ok(chunk.length <= 16);
});
```

**Acceptance Criteria**:
- Chunks respect highWaterMark
- Smaller highWaterMark = more chunks

*** TODO [#B] Task 3.11: Test fd option (use existing fd) [P][R:LOW][C:SIMPLE][D:3.3]
:PROPERTIES:
:ID: 3.11
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.3
:FILES: test/node/stream/test_fs_fd_option.js
:END:

**Test**:
```javascript
// Open file first, then pass fd
fs.open(__filename, 'r', (err, fd) => {
  const rs = fs.createReadStream(null, { fd, autoClose: true });
  rs.on('data', (chunk) => {
    console.log('Read chunk');
  });
  rs.on('end', () => {
    console.log('✓ fd option works');
  });
});
```

**Acceptance Criteria**:
- fd option accepts existing file descriptor
- Reading works with passed fd
- autoClose still closes fd at end

*** TODO [#B] Task 3.12: Memory leak testing [P][R:HIGH][C:SIMPLE][D:3.7]
:PROPERTIES:
:ID: 3.12
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.7
:END:

**Test**:
```bash
make jsrt_m
cat > /tmp/test_fs_leak.js << 'EOF'
const fs = require('node:fs');
for (let i = 0; i < 100; i++) {
  const rs = fs.createReadStream(__filename);
  rs.on('end', () => {});
  rs.resume();
}
EOF

ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m /tmp/test_fs_leak.js
```

**Acceptance Criteria**:
- Zero memory leaks
- File descriptors properly closed
- libuv requests cleaned up

*** TODO [#B] Task 3.13: Document ReadStream implementation [P][R:LOW][C:TRIVIAL][D:3.7]
:PROPERTIES:
:ID: 3.13
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.7
:FILES: src/node/fs/read_stream.c
:END:

**Documentation**:
1. API overview
2. libuv integration
3. Async I/O flow
4. Options reference
5. Error handling

*** TODO [#B] Task 3.14: Milestone 3 validation [S][R:HIGH][C:SIMPLE][D:3.7,3.12]
:PROPERTIES:
:ID: 3.14
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 3.7,3.12
:END:

**Checklist**:
- [ ] `fs.createReadStream(__filename)` works
- [ ] Piping to writable works
- [ ] `fs.createReadStream(file).pipe(transform).pipe(stdout)` works
- [ ] Error handling for missing files
- [ ] Zero memory leaks (ASAN)
- [ ] All options supported (start, end, highWaterMark, fd, encoding, autoClose)

---

** TODO [#B] Phase 4: End-to-End CLI Regression Tests [S][R:MEDIUM][C:MEDIUM] :milestone:m4:
DEADLINE: <2025-11-06 Wed>
:PROPERTIES:
:ID: phase-4
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-1,phase-2,phase-3
:PROGRESS: 0/12
:COMPLETION: 0%
:ESTIMATE: 0.5-1 day
:END:

**Goal**: Comprehensive testing with real-world CLI tools to ensure production readiness.

*** TODO [#B] Task 4.1: Create loose-envify test fixture [S][R:HIGH][C:SIMPLE]
:PROPERTIES:
:ID: 4.1
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-1,phase-2,phase-3
:FILES: test/fixtures/loose-envify/
:END:

**Objective**: Set up simplified `loose-envify` implementation for testing

**Implementation**:
```javascript
// test/fixtures/loose-envify/index.js
const stream = require('node:stream');
const util = require('node:util');

function LooseEnvify(filename, opts) {
  if (!(this instanceof LooseEnvify)) return new LooseEnvify(filename, opts);
  stream.Transform.call(this);
  this._data = '';
  this._filename = filename;
}

util.inherits(LooseEnvify, stream.Transform);

LooseEnvify.prototype._transform = function(chunk, encoding, callback) {
  this._data += chunk;
  callback();
};

LooseEnvify.prototype._flush = function(callback) {
  // Simplified: replace process.env.NODE_ENV with string literal
  const output = this._data.replace(/process\.env\.NODE_ENV/g, '"production"');
  this.push(output);
  callback();
};

module.exports = LooseEnvify;
```

**Acceptance Criteria**:
- loose-envify module created
- Implements real pattern (util.inherits + Transform.call)

*** TODO [#B] Task 4.2: Test loose-envify CLI invocation [S][R:HIGH][C:SIMPLE][D:4.1]
:PROPERTIES:
:ID: 4.2
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 4.1
:FILES: test/node/stream/test_loose_envify_cli.js
:END:

**Test**:
```javascript
// test/node/stream/test_loose_envify_cli.js
const LooseEnvify = require('../../fixtures/loose-envify');

const le = new LooseEnvify(__filename);

// Pipe stdin → loose-envify → stdout
process.stdin.pipe(le).pipe(process.stdout);
```

**Input file** (test input):
```javascript
// test/fixtures/loose-envify/input.js
if (process.env.NODE_ENV === 'production') {
  console.log('production mode');
}
```

**Manual Test**:
```bash
cat test/fixtures/loose-envify/input.js | ./bin/jsrt test/node/stream/test_loose_envify_cli.js
# Should output:
# if ("production" === 'production') {
#   console.log('production mode');
# }
```

**Acceptance Criteria**:
- CLI invocation works
- stdin → transform → stdout pipeline functional
- Output shows transformed code

*** TODO [#B] Task 4.3: Test with fs.createReadStream input [P][R:MEDIUM][C:SIMPLE][D:4.2]
:PROPERTIES:
:ID: 4.3
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 4.2
:FILES: test/node/stream/test_loose_envify_file.js
:END:

**Test**:
```javascript
const fs = require('node:fs');
const LooseEnvify = require('../../fixtures/loose-envify');

const inputFile = 'test/fixtures/loose-envify/input.js';
const le = new LooseEnvify(inputFile);

// Pipe file → loose-envify → stdout
fs.createReadStream(inputFile).pipe(le).pipe(process.stdout);
```

**Acceptance Criteria**:
- File input works
- fs.ReadStream → Transform → stdout pipeline functional

*** TODO [#B] Task 4.4: Create cat CLI example [P][R:LOW][C:SIMPLE][D:phase-2,phase-3]
:PROPERTIES:
:ID: 4.4
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-2,phase-3
:FILES: examples/cat.js
:END:

**Implementation**:
```javascript
#!/usr/bin/env jsrt
// examples/cat.js - Simple cat implementation
const fs = require('node:fs');

const filename = process.argv[2];
if (!filename) {
  process.stdin.pipe(process.stdout);
} else {
  fs.createReadStream(filename).pipe(process.stdout);
}
```

**Manual Tests**:
```bash
# Test 1: stdin
echo "hello" | ./bin/jsrt examples/cat.js

# Test 2: file
./bin/jsrt examples/cat.js /etc/hosts

# Test 3: pipe chain
cat /etc/hosts | ./bin/jsrt examples/cat.js | head -n 5
```

**Acceptance Criteria**:
- All 3 test modes work
- Output matches system `cat`

*** TODO [#B] Task 4.5: Create grep CLI example [P][R:LOW][C:MEDIUM][D:4.4]
:PROPERTIES:
:ID: 4.5
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 4.4
:FILES: examples/grep.js
:END:

**Implementation**:
```javascript
#!/usr/bin/env jsrt
// examples/grep.js - Simple grep using Transform
const { Transform } = require('node:stream');
const fs = require('node:fs');

const pattern = process.argv[2];
const filename = process.argv[3];

if (!pattern) {
  console.error('Usage: grep.js <pattern> [file]');
  process.exit(1);
}

const grepTransform = new Transform({
  transform(chunk, encoding, callback) {
    const lines = chunk.toString().split('\n');
    const matching = lines.filter(line => line.includes(pattern));
    if (matching.length > 0) {
      this.push(matching.join('\n') + '\n');
    }
    callback();
  }
});

const input = filename ? fs.createReadStream(filename) : process.stdin;
input.pipe(grepTransform).pipe(process.stdout);
```

**Manual Test**:
```bash
# Test with file
./bin/jsrt examples/grep.js "node" /etc/hosts

# Test with stdin
cat /etc/hosts | ./bin/jsrt examples/grep.js "127"
```

**Acceptance Criteria**:
- Grep example works
- Demonstrates Transform usage

*** TODO [#B] Task 4.6: Create wc CLI example [P][R:LOW][C:MEDIUM][D:4.4]
:PROPERTIES:
:ID: 4.6
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 4.4
:FILES: examples/wc.js
:END:

**Implementation**:
```javascript
#!/usr/bin/env jsrt
// examples/wc.js - Word count using Transform
const { Transform } = require('node:stream');
const fs = require('node:fs');

let lines = 0;
let words = 0;
let chars = 0;

const wcTransform = new Transform({
  transform(chunk, encoding, callback) {
    const str = chunk.toString();
    chars += str.length;
    lines += str.split('\n').length - 1;
    words += str.split(/\s+/).filter(w => w.length > 0).length;
    callback();
  },
  flush(callback) {
    this.push(`${lines} ${words} ${chars}\n`);
    callback();
  }
});

const input = process.argv[2] ? fs.createReadStream(process.argv[2]) : process.stdin;
input.pipe(wcTransform).pipe(process.stdout);
```

**Manual Test**:
```bash
./bin/jsrt examples/wc.js /etc/hosts
echo "hello world" | ./bin/jsrt examples/wc.js
```

**Acceptance Criteria**:
- Word count works correctly
- Demonstrates _flush usage

*** TODO [#B] Task 4.7: Test multiple CLI tools in pipeline [P][R:MEDIUM][C:SIMPLE][D:4.4,4.5,4.6]
:PROPERTIES:
:ID: 4.7
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 4.4,4.5,4.6
:END:

**Test**:
```bash
# Chain multiple jsrt programs
./bin/jsrt examples/cat.js /etc/hosts | ./bin/jsrt examples/grep.js "127" | ./bin/jsrt examples/wc.js
```

**Acceptance Criteria**:
- Multi-stage pipeline works
- No data loss or corruption

*** TODO [#B] Task 4.8: Stress test with large files [P][R:MEDIUM][C:SIMPLE][D:4.4]
:PROPERTIES:
:ID: 4.8
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 4.4
:END:

**Test**:
```bash
# Create 10MB test file
dd if=/dev/urandom of=/tmp/large_file.bin bs=1M count=10

# Test reading large file
./bin/jsrt examples/cat.js /tmp/large_file.bin > /dev/null

# Verify no memory leaks with ASAN
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m examples/cat.js /tmp/large_file.bin > /dev/null
```

**Acceptance Criteria**:
- Large files (10MB+) handled correctly
- Memory usage stays bounded
- Zero memory leaks

*** TODO [#B] Task 4.9: Test concurrent stream operations [P][R:MEDIUM][C:MEDIUM][D:phase-3]
:PROPERTIES:
:ID: 4.9
:CREATED: 2025-10-29T10:00:00Z
:DEPS: phase-3
:FILES: test/node/stream/test_concurrent.js
:END:

**Test**:
```javascript
// Read multiple files concurrently
const fs = require('node:fs');
const files = [__filename, '/etc/hosts', '/etc/passwd'];

let completed = 0;
files.forEach(file => {
  const rs = fs.createReadStream(file);
  rs.on('end', () => {
    completed++;
    if (completed === files.length) {
      console.log('✓ Concurrent reads completed');
    }
  });
  rs.resume();
});
```

**Acceptance Criteria**:
- Multiple streams can operate concurrently
- No interference between streams
- All streams complete successfully

*** TODO [#B] Task 4.10: Full integration test suite [S][R:HIGH][C:SIMPLE][D:4.2,4.3,4.7]
:PROPERTIES:
:ID: 4.10
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 4.2,4.3,4.7
:FILES: test/node/stream/test_integration_full.js
:END:

**Objective**: Single test file exercising all components together

**Test Scenarios**:
1. stdin → Transform → stdout
2. fs.ReadStream → Transform → stdout
3. fs.ReadStream → multiple Transforms → stdout
4. Error handling across pipeline
5. Backpressure with slow writer
6. Memory usage with large throughput

**Acceptance Criteria**:
- All 6 scenarios pass
- Comprehensive coverage of real usage

*** TODO [#B] Task 4.11: Performance benchmarking [P][R:LOW][C:SIMPLE][D:4.10]
:PROPERTIES:
:ID: 4.11
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 4.10
:FILES: test/node/stream/bench_throughput.js
:END:

**Benchmark**:
```javascript
// Measure throughput
const fs = require('node:fs');
const { Transform } = require('node:stream');

const start = Date.now();
let bytes = 0;

const counter = new Transform({
  transform(chunk, encoding, callback) {
    bytes += chunk.length;
    this.push(chunk);
    callback();
  }
});

fs.createReadStream('/tmp/large_file.bin')
  .pipe(counter)
  .on('finish', () => {
    const duration = (Date.now() - start) / 1000;
    const mbps = (bytes / 1024 / 1024) / duration;
    console.log(`Throughput: ${mbps.toFixed(2)} MB/s`);
  })
  .resume();
```

**Acceptance Criteria**:
- Throughput measured
- Baseline established for future optimization

*** TODO [#B] Task 4.12: Milestone 4 validation [S][R:HIGH][C:SIMPLE][D:4.2,4.3,4.10]
:PROPERTIES:
:ID: 4.12
:CREATED: 2025-10-29T10:00:00Z
:DEPS: 4.2,4.3,4.10
:END:

**Checklist**:
- [ ] loose-envify pattern works end-to-end
- [ ] `cat`, `grep`, `wc` examples work
- [ ] stdin → transform → stdout pipeline works
- [ ] fs.createReadStream → transform → stdout works
- [ ] Large files (10MB+) handled correctly
- [ ] Concurrent streams work
- [ ] Zero memory leaks (ASAN)
- [ ] All integration tests pass

---

* 🚀 Execution Dashboard
:PROPERTIES:
:CURRENT_PHASE: Phase 0: Emergency Bugfix
:PROGRESS: 0/68
:COMPLETION: 0%
:ACTIVE_TASK: None
:UPDATED: 2025-10-29T10:00:00Z
:END:

** Current Status
- Phase: Phase 0 - Emergency Bugfix
- Progress: 0/68 tasks (0%)
- Active: Ready to start with Task 0.1 investigation

** Phase Summary
| Phase | Tasks | Status | Completion |
|-------|-------|--------|------------|
| Phase 0: Bugfix | 8 | TODO | 0% |
| Phase 1: Transform | 18 | TODO | 0% |
| Phase 2: Stdio | 16 | TODO | 0% |
| Phase 3: fs.ReadStream | 14 | TODO | 0% |
| Phase 4: CLI Tests | 12 | TODO | 0% |

** Critical Path
Phase 0 → Phase 1 → Phase 2 → Phase 3 → Phase 4

All phases are sequential (marked [S]) as each depends on the previous phase completing.

** Risk Mitigation
- **Memory leaks**: ASAN testing after each phase
- **Blocking I/O**: Use libuv async operations throughout
- **Regression**: Run full test suite + WPT after each phase
- **Performance**: Benchmark in Phase 4 to establish baseline

** Next Steps
1. Start with Task 0.1: Investigate read() failure
2. Add debug logging to trace execution
3. Identify root cause
4. Proceed with fix in Task 0.2

* 📜 History & Updates
:LOGBOOK:
- State "PLANNING" from "" [2025-10-29T10:00:00Z] \\
  Created initial plan with 68 tasks across 5 phases
:END:

** Recent Changes
| Timestamp | Action | Phase | Details |
|-----------|--------|-------|---------|
| 2025-10-29T10:00:00Z | Created | All | Initial plan document created with full breakdown |

** Notes
- Plan created based on analysis of failing tests and node-stream-plan.md
- Emergency Phase 0 added to fix immediate read() failure blocking all work
- Phases aligned with milestones from node-stream-plan.md follow-up section
- Total estimate: 4-7 days for all phases

---

## 📊 Success Metrics Summary

### Phase Completion Criteria

**Phase 0: Emergency Bugfix** ✅ When:
- make test N=node/stream passes 100%
- read() works on all stream types
- Zero memory leaks (ASAN)

**Phase 1: Transform Constructor** ✅ When:
- Transform.call(this) works via util.inherits
- All stream types support dual-call pattern
- loose-envify pattern test passes

**Phase 2: Stdio Streams** ✅ When:
- process.stdin/stdout/stderr have pipe() methods
- stdin → transform → stdout pipeline works
- TTY and pipe modes both supported

**Phase 3: fs.ReadStream** ✅ When:
- fs.createReadStream() returns working stream
- File → transform → stdout pipeline works
- Large files (10MB+) handled correctly

**Phase 4: CLI Tests** ✅ When:
- loose-envify CLI example works end-to-end
- cat/grep/wc examples work
- All integration tests pass

### Final Success Criteria
- ✅ make test N=node/stream - 100% pass rate
- ✅ make wpt - baseline maintained (90.6%)
- ✅ make format - all code formatted
- ✅ ASAN - zero memory leaks
- ✅ Real CLI - loose-envify example works
- ✅ Examples - cat/grep/wc work correctly
- ✅ Stress - 10MB+ files handled
- ✅ Concurrent - multiple streams work together

---

**End of Plan Document**
