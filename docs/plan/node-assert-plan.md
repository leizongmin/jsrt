---
Created: 2025-10-06T18:05:00Z
Last Updated: 2025-10-06T18:05:00Z
Status: 🟡 PLANNING
Overall Progress: 0/42 tasks completed (0%)
---

# Node.js assert Module Compatibility Plan

## 📋 Executive Summary

### Current Status
- **jsrt:assert** is fully implemented with 10 core assertion functions
- **node:assert** is NOT currently aliased or implemented
- Basic test coverage exists for jsrt:assert (CommonJS and ESM)
- Implementation uses QuickJS C API with custom AssertionError handling

### Target Goals
1. Create node:assert module that uses the SAME implementation as jsrt:assert
2. Achieve 100% Node.js assert API compatibility
3. Add missing APIs: fail, ifError, match, doesNotMatch, rejects, doesNotReject, deepStrictEqual, notDeepStrictEqual
4. Implement strict mode variant (assert.strict)
5. Enhance AssertionError class to match Node.js specification
6. Comprehensive test coverage for all edge cases
7. Maintain backward compatibility with existing jsrt:assert module

### Constraints
- Follow jsrt development guidelines (minimal changes, test-driven)
- Memory management using QuickJS patterns (js_malloc, js_free)
- MANDATORY: All changes must pass `make test && make wpt`
- Code formatting required: `make format` before commit
- Module system integration (both CommonJS and ESM)

---

## 🔍 Current Implementation Analysis

### What Exists Today

**Location:** `/home/lei/work/jsrt/src/std/assert.c` and `assert.h`

**Implemented APIs (10 functions):**
1. ✅ `assert(value, message)` - Basic truthiness assertion
2. ✅ `assert.ok(value, message)` - Alias for assert()
3. ✅ `assert.equal(actual, expected, message)` - Loose equality (==)
4. ✅ `assert.notEqual(actual, expected, message)` - Loose inequality (!=)
5. ✅ `assert.strictEqual(actual, expected, message)` - Strict equality (===)
6. ✅ `assert.notStrictEqual(actual, expected, message)` - Strict inequality (!==)
7. ✅ `assert.deepEqual(actual, expected, message)` - Deep equality (JSON-based)
8. ✅ `assert.notDeepEqual(actual, expected, message)` - Deep inequality
9. ✅ `assert.throws(fn, error, message)` - Expects function to throw
10. ✅ `assert.doesNotThrow(fn, error, message)` - Expects function not to throw

**Module Registration:**
- **jsrt:assert**: Registered in `src/std/module.c` (line 669-676) for ES modules
- **node:assert**: NOT currently registered in `src/node/node_modules.c`
- CommonJS support: Available via `require('jsrt:assert')`
- ESM support: Available via `import assert from 'jsrt:assert'`

**Test Coverage:**
- `/home/lei/work/jsrt/test/test_std_assert.js` - CommonJS tests (11 test groups)
- `/home/lei/work/jsrt/test/test_std_assert.mjs` - ESM tests (12 test groups)
- Tests cover: basic assertions, equality variants, deep equality, throws, edge cases

**Current Implementation Quality:**
- ✅ Basic assertions work correctly
- ✅ Error messages are clear and colorized
- ✅ AssertionError has name and message properties
- ⚠️ Deep equality uses JSON comparison (limited - no Set, Map, circular refs)
- ⚠️ Missing operator, expected, actual properties on AssertionError
- ⚠️ No error type/constructor validation in throws()
- ⚠️ No async assertion support (rejects, doesNotReject)
- ⚠️ No string matching (match, doesNotMatch)

---

## 📊 API Completeness Matrix

### Core Assertion Functions

| API Function | Status | Priority | Dependencies | Notes |
|-------------|--------|----------|--------------|-------|
| `assert(value, message)` | ✅ DONE | HIGH | None | Working correctly |
| `assert.ok(value, message)` | ✅ DONE | HIGH | None | Alias working |
| `assert.equal(actual, expected, message)` | ✅ DONE | HIGH | None | Uses == comparison |
| `assert.notEqual(actual, expected, message)` | ✅ DONE | HIGH | None | Uses != comparison |
| `assert.strictEqual(actual, expected, message)` | ✅ DONE | HIGH | None | Uses === comparison |
| `assert.notStrictEqual(actual, expected, message)` | ✅ DONE | HIGH | None | Uses !== comparison |
| `assert.deepEqual(actual, expected, message)` | ⚠️ PARTIAL | HIGH | None | Limited JSON-based impl |
| `assert.notDeepEqual(actual, expected, message)` | ⚠️ PARTIAL | HIGH | None | Limited JSON-based impl |
| `assert.deepStrictEqual(actual, expected, message)` | ❌ MISSING | HIGH | None | Need proper deep comparison |
| `assert.notDeepStrictEqual(actual, expected, message)` | ❌ MISSING | HIGH | None | Need proper deep comparison |
| `assert.throws(fn, error, message)` | ⚠️ PARTIAL | HIGH | None | No error type validation |
| `assert.doesNotThrow(fn, error, message)` | ✅ DONE | MEDIUM | None | Working correctly |
| `assert.rejects(asyncFn, error, message)` | ❌ MISSING | HIGH | Promise handling | Async assertion |
| `assert.doesNotReject(asyncFn, error, message)` | ❌ MISSING | HIGH | Promise handling | Async assertion |
| `assert.fail(message)` | ❌ MISSING | MEDIUM | None | Unconditional failure |
| `assert.ifError(value)` | ❌ MISSING | MEDIUM | None | Special error check |
| `assert.match(string, regexp, message)` | ❌ MISSING | MEDIUM | Regexp support | String matching |
| `assert.doesNotMatch(string, regexp, message)` | ❌ MISSING | MEDIUM | Regexp support | String matching |

### Strict Mode Variant

| API Function | Status | Priority | Dependencies | Notes |
|-------------|--------|----------|--------------|-------|
| `assert.strict` | ❌ MISSING | HIGH | All functions | Namespace for strict mode |
| `assert.strict.equal` → `strictEqual` | ❌ MISSING | HIGH | strictEqual | Alias mapping |
| `assert.strict.deepEqual` → `deepStrictEqual` | ❌ MISSING | HIGH | deepStrictEqual | Alias mapping |

### AssertionError Class

| Property/Feature | Status | Priority | Dependencies | Notes |
|-----------------|--------|----------|--------------|-------|
| `error.name = 'AssertionError'` | ✅ DONE | HIGH | None | Currently set |
| `error.message` | ✅ DONE | HIGH | None | Currently set |
| `error.actual` | ❌ MISSING | HIGH | None | Actual value property |
| `error.expected` | ❌ MISSING | HIGH | None | Expected value property |
| `error.operator` | ❌ MISSING | HIGH | None | Operator string (===, !==, etc) |
| `error.generatedMessage` | ❌ MISSING | MEDIUM | None | Boolean flag |
| `error.code = 'ERR_ASSERTION'` | ❌ MISSING | MEDIUM | None | Error code property |
| `error.stack` | ⚠️ UNKNOWN | LOW | QuickJS | May need verification |
| Proper inheritance from Error | ⚠️ UNKNOWN | MEDIUM | QuickJS | May need verification |

### Module Aliasing & Exports

| Feature | Status | Priority | Dependencies | Notes |
|---------|--------|----------|--------------|-------|
| `require('jsrt:assert')` | ✅ DONE | HIGH | None | CommonJS working |
| `import from 'jsrt:assert'` | ✅ DONE | HIGH | None | ESM working |
| `require('node:assert')` | ❌ MISSING | HIGH | Module registration | Alias needed |
| `import from 'node:assert'` | ❌ MISSING | HIGH | Module registration | Alias needed |
| Named ESM exports | ❌ MISSING | MEDIUM | None | export { ok, strictEqual, ... } |
| `assert.strict` export | ❌ MISSING | HIGH | Strict variant | Namespace object |

---

## 🎯 Unification Strategy

### Approach: Single Implementation, Multiple Aliases

**Design Decision:** node:assert and jsrt:assert will use THE SAME C implementation.

**Rationale:**
1. Avoid code duplication and maintenance burden
2. Ensure behavioral consistency between module variants
3. Simplify testing (single source of truth)
4. Follow Node.js compatibility pattern used in other modules

### Implementation Steps

1. **Keep existing jsrt:assert implementation** in `src/std/assert.c`
2. **Extend implementation** to add missing APIs
3. **Register node:assert alias** in `src/node/node_modules.c`
4. **Both modules** call `JSRT_CreateAssertModule(ctx)` function
5. **Export handling** differs slightly between jsrt: and node: variants

### Module Registration Pattern

```c
// In src/node/node_modules.c - Add to node_modules array
{"assert", JSRT_InitNodeAssert, js_node_assert_init, NULL, false, {0}},

// CommonJS init function (reuses existing)
JSValue JSRT_InitNodeAssert(JSContext* ctx) {
    return JSRT_CreateAssertModule(ctx);
}

// ES Module init function
static int js_node_assert_init(JSContext *ctx, JSModuleDef *m) {
    JSValue assert_func = JSRT_CreateAssertModule(ctx);

    // Add default export
    JS_SetModuleExport(ctx, m, "default", assert_func);

    // Add named exports
    JS_SetModuleExport(ctx, m, "ok", JS_GetPropertyStr(ctx, assert_func, "ok"));
    JS_SetModuleExport(ctx, m, "equal", JS_GetPropertyStr(ctx, assert_func, "equal"));
    // ... more named exports

    return 0;
}
```

### Compatibility Matrix

| Module Specifier | CommonJS | ES Module | Status |
|-----------------|----------|-----------|--------|
| `jsrt:assert` | ✅ Works | ✅ Works | Existing |
| `node:assert` | ❌ Missing | ❌ Missing | To implement |
| `assert` (bare) | ❌ Not planned | ❌ Not planned | Node compat only with node: prefix |

---

## 🔬 Gap Analysis

### Missing APIs (8 functions)

#### 1. assert.fail([message])
**Priority:** MEDIUM
**Complexity:** TRIVIAL
**Description:** Throws AssertionError unconditionally with optional message.
**Implementation:** Simple wrapper around throw_assertion_error()

#### 2. assert.ifError(value)
**Priority:** MEDIUM
**Complexity:** SIMPLE
**Description:** Throws value if truthy, special error formatting for Error objects.
**Implementation:** Check truthiness, format message, throw

#### 3. assert.match(string, regexp[, message])
**Priority:** MEDIUM
**Complexity:** MEDIUM
**Description:** Test if string matches regex pattern.
**Implementation:** Requires QuickJS RegExp support, JS_RegExpExec()

#### 4. assert.doesNotMatch(string, regexp[, message])
**Priority:** MEDIUM
**Complexity:** MEDIUM
**Description:** Test if string does NOT match regex pattern.
**Implementation:** Requires QuickJS RegExp support

#### 5. assert.deepStrictEqual(actual, expected[, message])
**Priority:** HIGH
**Complexity:** COMPLEX
**Description:** Deep strict equality without type coercion.
**Implementation:** Proper deep comparison (not JSON-based), handles:
- Primitives with strict equality
- Arrays (length + element-wise)
- Objects (keys + values recursively)
- Special types: Set, Map, Date, RegExp, TypedArrays
- Circular references detection
- Prototype chain consideration

#### 6. assert.notDeepStrictEqual(actual, expected[, message])
**Priority:** HIGH
**Complexity:** MEDIUM
**Description:** Inverse of deepStrictEqual.
**Implementation:** Reuse deepStrictEqual logic, negate result

#### 7. assert.rejects(asyncFn[, error][, message])
**Priority:** HIGH
**Complexity:** COMPLEX
**Description:** Async assertion - expects Promise rejection.
**Implementation:**
- Handle async function or Promise
- Await result
- Verify rejection occurs
- Validate error type/message if provided
- Return Promise for chaining

#### 8. assert.doesNotReject(asyncFn[, error][, message])
**Priority:** HIGH
**Complexity:** COMPLEX
**Description:** Async assertion - expects Promise resolution.
**Implementation:** Similar to rejects but expects success

### API Improvements Needed

#### assert.throws() Enhancement
**Current:** Only checks if function throws, ignores error parameter
**Needed:**
- Validate error constructor/class
- Validate error message (string or RegExp)
- Validate error properties
- Return the caught error for inspection

**Example:**
```javascript
// Should validate error type
assert.throws(() => { throw new TypeError('bad') }, TypeError);

// Should validate error message
assert.throws(() => { throw new Error('foo') }, /foo/);

// Should validate error object properties
assert.throws(() => { throw new Error('x') }, { message: 'x' });
```

#### Deep Equality Enhancement
**Current Issues:**
- JSON-based comparison fails for:
  - Functions (JSON.stringify omits them)
  - undefined values (JSON omits them)
  - Symbol keys/values
  - Circular references (JSON throws)
  - Set, Map, TypedArray (converted to objects)
  - Date objects (stringified)
  - RegExp (stringified)
  - NaN (becomes null)
  - -0 vs +0 distinction

**Solution:** Implement proper recursive deep comparison in C

### AssertionError Enhancement

**Missing Properties:**
- `actual` - The actual value in the assertion
- `expected` - The expected value
- `operator` - The comparison operator as a string
- `code` - Should be 'ERR_ASSERTION'
- `generatedMessage` - Boolean indicating if message was auto-generated

**Implementation Approach:**
```c
static JSValue create_assertion_error(
    JSContext* ctx,
    const char* message,
    JSValueConst actual,
    JSValueConst expected,
    const char* operator,
    bool generated_message
) {
    JSValue error = JS_NewError(ctx);

    JS_SetPropertyStr(ctx, error, "name", JS_NewString(ctx, "AssertionError"));
    JS_SetPropertyStr(ctx, error, "message", JS_NewString(ctx, message));
    JS_SetPropertyStr(ctx, error, "code", JS_NewString(ctx, "ERR_ASSERTION"));
    JS_SetPropertyStr(ctx, error, "actual", JS_DupValue(ctx, actual));
    JS_SetPropertyStr(ctx, error, "expected", JS_DupValue(ctx, expected));
    JS_SetPropertyStr(ctx, error, "operator", JS_NewString(ctx, operator));
    JS_SetPropertyStr(ctx, error, "generatedMessage", JS_NewBool(ctx, generated_message));

    return error;
}
```

### Edge Cases to Handle

1. **NaN Comparisons:**
   - `assert.strictEqual(NaN, NaN)` should fail (NaN !== NaN)
   - `assert.deepStrictEqual(NaN, NaN)` should pass (special case)
   - Object.is() semantics

2. **+0 vs -0:**
   - `assert.strictEqual(0, -0)` should pass (0 === -0)
   - `assert.deepStrictEqual(0, -0)` should fail (Object.is distinguishes)

3. **Circular References:**
   - Deep equality should handle circular structures
   - Use visited set to track recursion

4. **Special Objects:**
   - Date: Compare numeric values
   - RegExp: Compare source and flags
   - Set/Map: Compare contents
   - TypedArrays: Compare type and contents
   - Buffer: Byte-by-byte comparison

5. **Prototype Chain:**
   - Should compare own properties only (Node.js behavior)
   - Ignore prototype differences

6. **undefined vs missing:**
   - `{a: undefined}` vs `{}` should NOT be equal
   - Array holes vs undefined values

---

## 📝 Implementation Tasks

### Phase 0: Setup & Foundation (2 tasks)

**0.1** [S][R:LOW][C:TRIVIAL] Create plan document and analyze requirements
   - Execution: SEQUENTIAL
   - Dependencies: None (completed)
   - ✅ COMPLETED

**0.2** [S][R:LOW][C:SIMPLE] Run baseline tests to establish current state
   - Execution: SEQUENTIAL
   - Dependencies: 0.1
   - Status: ⏳ PENDING
   - Tasks:
     - Run `make test` and capture results
     - Run `make wpt` and capture results
     - Document current assertion test results

### Phase 1: Module Aliasing (5 tasks)

**1.1** [S][R:LOW][C:SIMPLE] Add node:assert to node module registry
   - Execution: SEQUENTIAL
   - Dependencies: 0.2
   - Status: ⏳ PENDING
   - Files: `src/node/node_modules.c`
   - Tasks:
     - Add entry to `node_modules[]` array
     - Create `JSRT_InitNodeAssert` CommonJS init function
     - Create `js_node_assert_init` ESM init function

**1.2** [S][R:LOW][C:SIMPLE] Implement CommonJS require('node:assert')
   - Execution: SEQUENTIAL
   - Dependencies: 1.1
   - Status: ⏳ PENDING
   - Implementation: Reuse `JSRT_CreateAssertModule(ctx)`
   - Test: `const assert = require('node:assert'); assert.ok(true);`

**1.3** [S][R:MEDIUM][C:MEDIUM] Implement ES Module import from 'node:assert'
   - Execution: SEQUENTIAL
   - Dependencies: 1.1
   - Status: ⏳ PENDING
   - Tasks:
     - Add default export
     - Add named exports for all assertion functions
     - Register exports in JSRT_LoadNodeModule switch

**1.4** [P][R:LOW][C:SIMPLE] Create basic node:assert tests
   - Execution: PARALLEL (can run with 1.5)
   - Dependencies: 1.2, 1.3
   - Status: ⏳ PENDING
   - Files: Create `test/test_node_assert.js` and `test/test_node_assert.mjs`
   - Content: Copy and adapt from test_std_assert.* with node: prefix

**1.5** [P][R:LOW][C:TRIVIAL] Run aliasing tests and verify
   - Execution: PARALLEL (can run with 1.4)
   - Dependencies: 1.2, 1.3
   - Status: ⏳ PENDING
   - Verify: `make test` passes with new tests

### Phase 2: AssertionError Enhancement (4 tasks)

**2.1** [S][R:MEDIUM][C:MEDIUM] Refactor error creation to include all properties
   - Execution: SEQUENTIAL
   - Dependencies: 1.5
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Tasks:
     - Replace `throw_assertion_error()` with enhanced version
     - Add actual, expected, operator, code, generatedMessage properties
     - Update all call sites to pass proper parameters

**2.2** [S][R:LOW][C:SIMPLE] Update all assertion functions to use enhanced error
   - Execution: SEQUENTIAL
   - Dependencies: 2.1
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Tasks:
     - Update assert(), ok(), equal(), strictEqual(), etc.
     - Pass appropriate operator strings ("==", "===", "deepEqual", etc.)
     - Pass actual and expected values

**2.3** [P][R:LOW][C:MEDIUM] Create AssertionError property tests
   - Execution: PARALLEL (can run with 2.4)
   - Dependencies: 2.2
   - Status: ⏳ PENDING
   - Files: `test/test_assertion_error.js`
   - Tests:
     - Verify error.name === 'AssertionError'
     - Verify error.code === 'ERR_ASSERTION'
     - Verify error.actual matches input
     - Verify error.expected matches input
     - Verify error.operator is correct
     - Verify error.message formatting

**2.4** [P][R:LOW][C:TRIVIAL] Run enhanced error tests
   - Execution: PARALLEL (can run with 2.3)
   - Dependencies: 2.2
   - Status: ⏳ PENDING
   - Verify: All tests pass with enhanced error properties

### Phase 3: Missing Simple APIs (6 tasks)

**3.1** [S][R:LOW][C:TRIVIAL] Implement assert.fail(message)
   - Execution: SEQUENTIAL
   - Dependencies: 2.4
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`, `src/std/assert.h`
   - Implementation:
     ```c
     static JSValue jsrt_assert_fail(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
         const char* message = "Failed";
         if (argc > 0 && JS_IsString(argv[0])) {
             message = JS_ToCString(ctx, argv[0]);
         }
         JSValue error = create_assertion_error(ctx, message, JS_UNDEFINED, JS_UNDEFINED, "fail", false);
         if (argc > 0 && JS_IsString(argv[0])) {
             JS_FreeCString(ctx, message);
         }
         return error;
     }
     ```

**3.2** [S][R:LOW][C:SIMPLE] Implement assert.ifError(value)
   - Execution: SEQUENTIAL
   - Dependencies: 3.1
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Logic:
     - If value is falsy, return undefined
     - If value is truthy Error object, throw it
     - If value is truthy non-Error, throw AssertionError with value

**3.3** [S][R:MEDIUM][C:MEDIUM] Implement assert.match(string, regexp, message)
   - Execution: SEQUENTIAL
   - Dependencies: 3.2
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Research: QuickJS RegExp API (JS_RegExpExec or JS_Eval with .test())
   - Tasks:
     - Validate string argument is string
     - Validate regexp argument is RegExp
     - Test string against regexp
     - Throw AssertionError if no match

**3.4** [S][R:MEDIUM][C:MEDIUM] Implement assert.doesNotMatch(string, regexp, message)
   - Execution: SEQUENTIAL
   - Dependencies: 3.3
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Implementation: Inverse of match()

**3.5** [P][R:LOW][C:SIMPLE] Create tests for fail, ifError, match, doesNotMatch
   - Execution: PARALLEL (can run with 3.6)
   - Dependencies: 3.4
   - Status: ⏳ PENDING
   - Files: `test/test_assert_additional.js`
   - Test cases:
     - fail with and without message
     - ifError with null, undefined, Error, string
     - match with various patterns and strings
     - doesNotMatch with various patterns

**3.6** [P][R:LOW][C:TRIVIAL] Run and verify simple API tests
   - Execution: PARALLEL (can run with 3.5)
   - Dependencies: 3.4
   - Status: ⏳ PENDING
   - Verify: `make test` passes

### Phase 4: Deep Equality Enhancement (6 tasks)

**4.1** [S][R:HIGH][C:COMPLEX] Design deep comparison algorithm
   - Execution: SEQUENTIAL
   - Dependencies: 3.6
   - Status: ⏳ PENDING
   - Design considerations:
     - Circular reference detection (visited set)
     - Type checking (primitives, objects, arrays)
     - Special object handling (Date, RegExp, Set, Map, TypedArray)
     - NaN and -0/+0 handling
     - Recursion depth limit

**4.2** [S][R:HIGH][C:COMPLEX] Implement deep_strict_equal() helper in C
   - Execution: SEQUENTIAL
   - Dependencies: 4.1
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Implementation:
     ```c
     // Recursive deep comparison with visited tracking
     static int deep_strict_equal_internal(
         JSContext* ctx,
         JSValueConst a,
         JSValueConst b,
         JSValue visited  // Array of visited pairs
     ) {
         // Handle primitives with Object.is() semantics
         // Handle circular references
         // Handle arrays (length + elements)
         // Handle objects (keys + values)
         // Handle special types
     }
     ```

**4.3** [S][R:MEDIUM][C:MEDIUM] Implement assert.deepStrictEqual()
   - Execution: SEQUENTIAL
   - Dependencies: 4.2
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Use deep_strict_equal_internal() helper

**4.4** [S][R:LOW][C:SIMPLE] Implement assert.notDeepStrictEqual()
   - Execution: SEQUENTIAL
   - Dependencies: 4.3
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Negate result of deepStrictEqual

**4.5** [P][R:MEDIUM][C:COMPLEX] Create comprehensive deep equality tests
   - Execution: PARALLEL (can run with 4.6)
   - Dependencies: 4.4
   - Status: ⏳ PENDING
   - Files: `test/test_assert_deep_strict.js`
   - Test cases:
     - Primitives: NaN, -0, +0, null, undefined
     - Arrays: nested, empty, holes vs undefined
     - Objects: nested, different key order, prototype differences
     - Date objects with same/different times
     - RegExp with same/different patterns and flags
     - Circular references
     - Mixed types
     - Large structures (performance)

**4.6** [P][R:LOW][C:TRIVIAL] Run deep equality tests
   - Execution: PARALLEL (can run with 4.5)
   - Dependencies: 4.4
   - Status: ⏳ PENDING
   - Verify: All edge cases pass

### Phase 5: Enhanced throws() Validation (4 tasks)

**5.1** [S][R:MEDIUM][C:MEDIUM] Enhance assert.throws() with error validation
   - Execution: SEQUENTIAL
   - Dependencies: 4.6
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Add support for:
     - Error constructor validation (typeof)
     - Error message validation (string or RegExp)
     - Error property validation (object)
     - Return caught error for inspection

**5.2** [S][R:LOW][C:SIMPLE] Update assert.doesNotThrow() if needed
   - Execution: SEQUENTIAL
   - Dependencies: 5.1
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Ensure consistent error handling

**5.3** [P][R:LOW][C:MEDIUM] Create throws() validation tests
   - Execution: PARALLEL (can run with 5.4)
   - Dependencies: 5.2
   - Status: ⏳ PENDING
   - Files: `test/test_assert_throws.js`
   - Test cases:
     - throws with Error constructor
     - throws with TypeError, RangeError, etc.
     - throws with message string matching
     - throws with RegExp message matching
     - throws with property validation
     - doesNotThrow edge cases

**5.4** [P][R:LOW][C:TRIVIAL] Run throws validation tests
   - Execution: PARALLEL (can run with 5.3)
   - Dependencies: 5.2
   - Status: ⏳ PENDING
   - Verify: All validation works correctly

### Phase 6: Async Assertions (6 tasks)

**6.1** [S][R:HIGH][C:COMPLEX] Research QuickJS Promise/async handling
   - Execution: SEQUENTIAL
   - Dependencies: 5.4
   - Status: ⏳ PENDING
   - Research topics:
     - How to check if value is Promise (JS_IsObject + then property)
     - How to await/resolve Promise from C
     - How to catch Promise rejection
     - Event loop integration requirements

**6.2** [S][R:HIGH][C:COMPLEX] Implement assert.rejects() helper logic
   - Execution: SEQUENTIAL
   - Dependencies: 6.1
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Implementation challenges:
     - Handle async function (call it, get Promise)
     - Handle Promise directly
     - Return new Promise that resolves when validation completes
     - Validate rejection error (type, message, properties)
     - Propagate rejection if validation fails

**6.3** [S][R:HIGH][C:COMPLEX] Implement assert.rejects()
   - Execution: SEQUENTIAL
   - Dependencies: 6.2
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Signature: `JSValue jsrt_assert_rejects(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)`

**6.4** [S][R:MEDIUM][C:MEDIUM] Implement assert.doesNotReject()
   - Execution: SEQUENTIAL
   - Dependencies: 6.3
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Similar to rejects but expects resolution

**6.5** [P][R:MEDIUM][C:COMPLEX] Create async assertion tests
   - Execution: PARALLEL (can run with 6.6)
   - Dependencies: 6.4
   - Status: ⏳ PENDING
   - Files: `test/test_assert_async.js`
   - Test cases:
     - rejects with async function that rejects
     - rejects with Promise that rejects
     - rejects with error validation
     - doesNotReject with async function that resolves
     - doesNotReject with Promise that resolves
     - Chaining multiple async assertions

**6.6** [P][R:LOW][C:TRIVIAL] Run async tests
   - Execution: PARALLEL (can run with 6.5)
   - Dependencies: 6.4
   - Status: ⏳ PENDING
   - Verify: Async assertions work correctly

### Phase 7: Strict Mode Variant (4 tasks)

**7.1** [S][R:LOW][C:MEDIUM] Create assert.strict namespace object
   - Execution: SEQUENTIAL
   - Dependencies: 6.6
   - Status: ⏳ PENDING
   - Files: `src/std/assert.c`
   - Implementation:
     ```c
     // In JSRT_CreateAssertModule:
     JSValue strict_obj = JS_NewObject(ctx);

     // Map to strict variants
     JS_SetPropertyStr(ctx, strict_obj, "equal",
         JS_GetPropertyStr(ctx, assert_func, "strictEqual"));
     JS_SetPropertyStr(ctx, strict_obj, "deepEqual",
         JS_GetPropertyStr(ctx, assert_func, "deepStrictEqual"));
     JS_SetPropertyStr(ctx, strict_obj, "notEqual",
         JS_GetPropertyStr(ctx, assert_func, "notStrictEqual"));
     JS_SetPropertyStr(ctx, strict_obj, "notDeepEqual",
         JS_GetPropertyStr(ctx, assert_func, "notDeepStrictEqual"));

     // Copy other methods as-is
     JS_SetPropertyStr(ctx, strict_obj, "ok",
         JS_GetPropertyStr(ctx, assert_func, "ok"));
     // ... etc

     JS_SetPropertyStr(ctx, assert_func, "strict", strict_obj);
     ```

**7.2** [S][R:LOW][C:SIMPLE] Add all methods to strict namespace
   - Execution: SEQUENTIAL
   - Dependencies: 7.1
   - Status: ⏳ PENDING
   - Ensure all assertion functions are accessible via assert.strict.*

**7.3** [P][R:LOW][C:MEDIUM] Create strict mode tests
   - Execution: PARALLEL (can run with 7.4)
   - Dependencies: 7.2
   - Status: ⏳ PENDING
   - Files: `test/test_assert_strict.js`
   - Test cases:
     - assert.strict.equal maps to strictEqual
     - assert.strict.deepEqual maps to deepStrictEqual
     - All other methods work correctly
     - Both jsrt:assert and node:assert support strict mode

**7.4** [P][R:LOW][C:TRIVIAL] Run strict mode tests
   - Execution: PARALLEL (can run with 7.3)
   - Dependencies: 7.2
   - Status: ⏳ PENDING
   - Verify: Strict mode works correctly

### Phase 8: Final Integration & Testing (5 tasks)

**8.1** [S][R:MEDIUM][C:MEDIUM] Update module exports for named ESM exports
   - Execution: SEQUENTIAL
   - Dependencies: 7.4
   - Status: ⏳ PENDING
   - Files: `src/std/module.c`, `src/node/node_modules.c`
   - Add named exports:
     - export { ok, equal, strictEqual, deepEqual, ... } from 'node:assert'
     - Ensure both default and named exports work

**8.2** [S][R:LOW][C:SIMPLE] Run all existing tests to ensure no regressions
   - Execution: SEQUENTIAL
   - Dependencies: 8.1
   - Status: ⏳ PENDING
   - Commands:
     - `make test` - All unit tests must pass
     - `make wpt` - WPT tests must not regress
     - Review any failures

**8.3** [P][R:LOW][C:MEDIUM] Create comprehensive integration tests
   - Execution: PARALLEL (can run with 8.4)
   - Dependencies: 8.2
   - Status: ⏳ PENDING
   - Files: `test/test_assert_complete.js`
   - Test all APIs together in realistic scenarios
   - Test both jsrt:assert and node:assert behave identically

**8.4** [P][R:LOW][C:SIMPLE] Run code formatting
   - Execution: PARALLEL (can run with 8.3)
   - Dependencies: 8.2
   - Status: ⏳ PENDING
   - Command: `make format`
   - Verify: All code follows jsrt formatting standards

**8.5** [S][R:LOW][C:TRIVIAL] Final verification and sign-off
   - Execution: SEQUENTIAL
   - Dependencies: 8.3, 8.4
   - Status: ⏳ PENDING
   - Checklist:
     - ✅ All tests pass (`make test`)
     - ✅ No WPT regressions (`make wpt`)
     - ✅ Code formatted (`make format`)
     - ✅ Both jsrt:assert and node:assert work
     - ✅ All Node.js assert APIs implemented
     - ✅ AssertionError properties complete
     - ✅ Documentation updated

---

## 🔗 Dependency Graph

### Execution Order (showing critical path)

```
Phase 0: Setup
0.1 (DONE) → 0.2

Phase 1: Aliasing (can start after 0.2)
0.2 → 1.1 → 1.2 → 1.3 → [1.4 || 1.5]
                          (parallel)

Phase 2: Error Enhancement (can start after Phase 1)
1.5 → 2.1 → 2.2 → [2.3 || 2.4]
                    (parallel)

Phase 3: Simple APIs (can start after Phase 2)
2.4 → 3.1 → 3.2 → 3.3 → 3.4 → [3.5 || 3.6]
                                 (parallel)

Phase 4: Deep Equality (can start after Phase 3)
3.6 → 4.1 → 4.2 → 4.3 → 4.4 → [4.5 || 4.6]
                                 (parallel)

Phase 5: Enhanced throws (can start after Phase 4)
4.6 → 5.1 → 5.2 → [5.3 || 5.4]
                    (parallel)

Phase 6: Async (can start after Phase 5)
5.4 → 6.1 → 6.2 → 6.3 → 6.4 → [6.5 || 6.6]
                                 (parallel)

Phase 7: Strict Mode (can start after Phase 6)
6.6 → 7.1 → 7.2 → [7.3 || 7.4]
                    (parallel)

Phase 8: Final Integration (can start after Phase 7)
7.4 → 8.1 → 8.2 → [8.3 || 8.4] → 8.5
                    (parallel)
```

### Parallel Execution Opportunities

**Can Run in Parallel:**
- Tasks 1.4 and 1.5 (test creation and running)
- Tasks 2.3 and 2.4 (test creation and running)
- Tasks 3.5 and 3.6 (test creation and running)
- Tasks 4.5 and 4.6 (test creation and running)
- Tasks 5.3 and 5.4 (test creation and running)
- Tasks 6.5 and 6.6 (test creation and running)
- Tasks 7.3 and 7.4 (test creation and running)
- Tasks 8.3 and 8.4 (integration tests and formatting)

**Blocking Tasks (must be sequential):**
- All implementation tasks (cannot parallelize C code changes)
- Phases must complete in order (dependencies on previous functionality)

### Critical Path
The longest dependency chain (critical path) is:
**0.2 → 1.1 → 1.2 → 1.3 → 1.5 → 2.1 → 2.2 → 2.4 → 3.1 → 3.2 → 3.3 → 3.4 → 3.6 → 4.1 → 4.2 → 4.3 → 4.4 → 4.6 → 5.1 → 5.2 → 5.4 → 6.1 → 6.2 → 6.3 → 6.4 → 6.6 → 7.1 → 7.2 → 7.4 → 8.1 → 8.2 → 8.5**

Total: 30 sequential tasks on critical path

---

## 🧪 Testing Strategy

### Test Organization

**Existing Tests (to maintain):**
- `test/test_std_assert.js` - jsrt:assert CommonJS tests
- `test/test_std_assert.mjs` - jsrt:assert ESM tests

**New Tests (to create):**
- `test/test_node_assert.js` - node:assert CommonJS tests (mirrors test_std_assert.js)
- `test/test_node_assert.mjs` - node:assert ESM tests (mirrors test_std_assert.mjs)
- `test/test_assertion_error.js` - AssertionError property validation
- `test/test_assert_additional.js` - fail, ifError, match, doesNotMatch
- `test/test_assert_deep_strict.js` - Deep strict equality edge cases
- `test/test_assert_throws.js` - Enhanced throws validation
- `test/test_assert_async.js` - Async assertions (rejects, doesNotReject)
- `test/test_assert_strict.js` - Strict mode variant
- `test/test_assert_complete.js` - Integration tests

### Test Coverage Requirements

#### 1. Basic Assertions
- ✅ Truthy/falsy values
- ✅ Custom error messages
- ✅ Error thrown on failure
- ⚠️ Error properties validation (NEW)

#### 2. Equality Assertions
- ✅ Loose equality (==)
- ✅ Strict equality (===)
- ⚠️ Deep equality edge cases (ENHANCE)
- Type coercion behavior
- null vs undefined
- Number edge cases (NaN, Infinity, -0, +0)

#### 3. Deep Equality (NEW/ENHANCED)
- Primitive values (all types)
- Arrays: nested, empty, sparse (holes)
- Objects: nested, different key order, computed keys
- Special objects: Date, RegExp, Error
- Collections: Set, Map (if QuickJS supports)
- TypedArrays: various types
- Circular references
- Symbol keys/values
- Prototype differences
- undefined values vs missing properties

#### 4. Throws Assertions (ENHANCED)
- ✅ Function throws any error
- ❌ Function throws specific error type (NEW)
- ❌ Function throws error with message (NEW)
- ❌ Function throws error with properties (NEW)
- ✅ Function does not throw
- ⚠️ Return value of throws() (NEW)

#### 5. Async Assertions (NEW)
- Promise rejection with any error
- Promise rejection with specific error type
- Promise rejection with message validation
- Promise resolution (doesNotReject)
- Async function that throws
- Async function that returns
- Error handling and propagation
- Chaining async assertions

#### 6. String Matching (NEW)
- String matches RegExp
- String does not match RegExp
- Invalid argument types
- Complex regex patterns

#### 7. Special Functions (NEW)
- assert.fail() with/without message
- assert.ifError() with null, Error, truthy values

#### 8. Strict Mode (NEW)
- assert.strict.equal behaves like strictEqual
- assert.strict.deepEqual behaves like deepStrictEqual
- All methods available in strict mode

#### 9. Module Loading
- ✅ require('jsrt:assert') works
- ✅ import from 'jsrt:assert' works
- ❌ require('node:assert') works (NEW)
- ❌ import from 'node:assert' works (NEW)
- ❌ Named ESM exports work (NEW)
- ❌ Both modules behave identically (NEW)

### Performance Tests

While not critical for initial implementation, consider:
- Deep equality on large structures (1000+ elements)
- Circular reference handling (deeply nested)
- Memory leak testing (repeated assertions)
- Stack overflow prevention (deep recursion)

### Edge Case Testing Matrix

| Category | Test Cases | Priority |
|----------|-----------|----------|
| **NaN** | strictEqual(NaN, NaN), deepStrictEqual(NaN, NaN) | HIGH |
| **+0/-0** | strictEqual(0, -0), deepStrictEqual(0, -0) | HIGH |
| **null/undefined** | equal(null, undefined), strictEqual(null, undefined) | HIGH |
| **Circular refs** | deepEqual with self-referencing objects | HIGH |
| **Array holes** | [1,,3] vs [1,undefined,3] | MEDIUM |
| **Symbol** | Objects with Symbol keys/values | MEDIUM |
| **TypedArray** | Different TypedArray types | MEDIUM |
| **RegExp** | Same pattern, different flags | MEDIUM |
| **Date** | Same time, different Date objects | MEDIUM |
| **Error objects** | Deep equality of Error instances | LOW |
| **Functions** | Deep equality with function properties | LOW |

---

## ✅ Success Criteria

### Functional Completeness

1. ✅ **All 18 Node.js assert APIs implemented:**
   - assert(), ok(), equal(), notEqual()
   - strictEqual(), notStrictEqual()
   - deepEqual(), notDeepEqual()
   - deepStrictEqual(), notDeepStrictEqual()
   - throws(), doesNotThrow()
   - rejects(), doesNotReject()
   - match(), doesNotMatch()
   - fail(), ifError()

2. ✅ **Strict mode variant working:**
   - assert.strict namespace exists
   - Correct method aliasing

3. ✅ **AssertionError fully compliant:**
   - Properties: name, message, code, actual, expected, operator, generatedMessage
   - Proper inheritance from Error

4. ✅ **Module aliasing complete:**
   - jsrt:assert works (existing)
   - node:assert works (new)
   - Both use same implementation
   - CommonJS and ESM support

### Quality Metrics

1. ✅ **Test Coverage:**
   - All APIs have unit tests
   - All edge cases covered
   - Both module variants tested
   - Integration tests pass

2. ✅ **Compliance:**
   - `make test` passes 100%
   - `make wpt` shows no regressions
   - `make format` applied to all code

3. ✅ **Memory Safety:**
   - No memory leaks (verified with AddressSanitizer if possible)
   - All allocated memory freed
   - Proper QuickJS value management

4. ✅ **Performance:**
   - No significant performance degradation
   - Deep equality handles reasonable structure sizes
   - Circular reference detection doesn't cause hangs

### Documentation

1. ✅ **Code Documentation:**
   - Function comments explain behavior
   - Complex algorithms have inline comments
   - AssertionError properties documented

2. ✅ **Test Documentation:**
   - Test files have clear descriptions
   - Edge cases explained

3. ⚠️ **User Documentation (if requested):**
   - API reference (only if user requests)
   - Usage examples (only if user requests)

---

## 📅 Timeline & Effort Estimates

### Effort Breakdown (Rough Estimates)

**Note:** These are rough estimates for planning purposes. Actual time may vary based on:
- Complexity of QuickJS integration
- Edge case discoveries
- Testing thoroughness
- Code review iterations

| Phase | Tasks | Est. Effort | Complexity | Risk |
|-------|-------|-------------|------------|------|
| **Phase 0: Setup** | 2 | 30 min | TRIVIAL | LOW |
| **Phase 1: Aliasing** | 5 | 2 hours | SIMPLE-MEDIUM | LOW |
| **Phase 2: Error Enhancement** | 4 | 3 hours | MEDIUM | MEDIUM |
| **Phase 3: Simple APIs** | 6 | 4 hours | SIMPLE-MEDIUM | MEDIUM |
| **Phase 4: Deep Equality** | 6 | 8 hours | COMPLEX | HIGH |
| **Phase 5: Enhanced throws** | 4 | 3 hours | MEDIUM | MEDIUM |
| **Phase 6: Async** | 6 | 8 hours | COMPLEX | HIGH |
| **Phase 7: Strict Mode** | 4 | 2 hours | SIMPLE-MEDIUM | LOW |
| **Phase 8: Integration** | 5 | 3 hours | MEDIUM | MEDIUM |
| **Total** | **42 tasks** | **~33 hours** | - | - |

### Risk Factors

**HIGH RISK (may take longer):**
- **Phase 4: Deep Equality** - Complex algorithm, many edge cases, QuickJS API exploration
- **Phase 6: Async Assertions** - Promise handling from C, event loop integration, async testing

**MEDIUM RISK:**
- **Phase 2: Error Enhancement** - Refactoring all call sites, potential for regressions
- **Phase 3: RegExp Matching** - QuickJS RegExp API may have quirks
- **Phase 5: throws() Enhancement** - Error type validation complexity

**LOW RISK:**
- **Phase 1: Module Aliasing** - Straightforward registration pattern
- **Phase 7: Strict Mode** - Simple object aliasing
- **Phase 8: Integration** - Verification and cleanup

### Recommended Execution Approach

**Week 1: Foundation**
- Days 1-2: Phase 0, 1, 2 (Setup, Aliasing, Error Enhancement)
- Days 3-5: Phase 3 (Simple APIs)

**Week 2: Core Complexity**
- Days 1-3: Phase 4 (Deep Equality - allocate extra time)
- Days 4-5: Phase 5 (Enhanced throws)

**Week 3: Advanced Features**
- Days 1-3: Phase 6 (Async - allocate extra time)
- Day 4: Phase 7 (Strict Mode)
- Day 5: Phase 8 (Integration & Testing)

**Buffer Time:** Add 20-30% buffer for:
- Unexpected QuickJS API challenges
- Edge case debugging
- Test failures and iterations
- Code review and revisions

---

## 📚 References

### Node.js Documentation
- [Node.js Assert Module v24.9.0](https://nodejs.org/api/assert.html) - Official API reference
- [Node.js Errors](https://nodejs.org/api/errors.html#class-error) - Error class specification

### JSRT Codebase References
- `/home/lei/work/jsrt/src/std/assert.c` - Current implementation
- `/home/lei/work/jsrt/src/std/assert.h` - Header file
- `/home/lei/work/jsrt/src/std/module.c` - Module registration (jsrt:)
- `/home/lei/work/jsrt/src/node/node_modules.c` - Node module registration
- `/home/lei/work/jsrt/test/test_std_assert.js` - Existing tests (CommonJS)
- `/home/lei/work/jsrt/test/test_std_assert.mjs` - Existing tests (ESM)

### Similar Module Examples
- `/home/lei/work/jsrt/src/node/node_path.c` - Example of node: module
- `/home/lei/work/jsrt/src/node/node_os.c` - Another node: module example
- `/home/lei/work/jsrt/src/node/node_util.c` - util module patterns

### QuickJS References
- QuickJS header files in `deps/quickjs/`
- Existing QuickJS API usage in jsrt codebase
- QuickJS documentation (if available)

### Standards & Compatibility
- [WinterCG Runtime Keys](https://runtime-keys.proposal.wintercg.org/) - For potential compliance
- [Web Platform Tests](https://github.com/web-platform-tests/wpt) - For standards alignment

### Development Guidelines
- `/home/lei/work/jsrt/CLAUDE.md` - Project development rules
- `/home/lei/work/jsrt/.claude/docs/testing.md` - Testing guidelines
- `/home/lei/work/jsrt/.claude/docs/modules.md` - Module development guide

---

## 📝 Notes & Considerations

### Design Decisions

1. **Single Implementation Approach:**
   - Chose to use one C implementation for both jsrt:assert and node:assert
   - Avoids code duplication and ensures consistency
   - Simplifies maintenance and testing

2. **Deep Equality Strategy:**
   - Current JSON-based approach is insufficient
   - Will implement proper recursive comparison in C
   - Must handle circular references explicitly

3. **Async Assertion Complexity:**
   - May require significant QuickJS Promise API research
   - Could be the highest-risk component
   - May need event loop integration

4. **Strict Mode Design:**
   - Using namespace object (assert.strict.*)
   - Aliasing to existing strict functions
   - Matches Node.js behavior

### Open Questions

1. **QuickJS Promise Handling:**
   - How to properly await/resolve Promise from C code?
   - Is there existing jsrt code handling Promises we can reference?
   - Will async assertions need special event loop considerations?

2. **Deep Equality Scope:**
   - Should we support Set/Map comparison (depends on QuickJS support)?
   - What's the recursion depth limit?
   - How to handle WeakMap/WeakSet if present?

3. **Performance Targets:**
   - What's acceptable performance for deep equality on large objects?
   - Should we optimize for common cases (small objects)?

4. **Error Message Format:**
   - Should error messages match Node.js exactly?
   - Current colored output - keep or make Node.js compatible?

### Future Enhancements (Out of Scope)

These are NOT part of this plan but could be considered later:

1. **assert.CallTracker** - Experimental Node.js feature
2. **Legacy assert.deepEqual behavior** - Deprecated loose deep equality
3. **Custom error messages with template strings** - Advanced formatting
4. **Stack trace enhancement** - Better error location reporting
5. **Performance optimizations** - Caching, memoization for deep equality
6. **Snapshot testing** - assert.snapshot() (experimental in Node.js)

### Breaking Changes

**None expected.** This plan maintains backward compatibility:
- Existing jsrt:assert behavior unchanged
- New APIs are additions, not modifications
- Error enhancement adds properties, doesn't remove
- Tests will verify no regressions

---

## 🎯 Next Recommended Actions

### Immediate Next Steps

1. **Review this plan** - Ensure stakeholder agreement on approach
2. **Run baseline tests** (Task 0.2):
   ```bash
   make test > /tmp/baseline_test.log 2>&1
   make wpt > /tmp/baseline_wpt.log 2>&1
   ```
3. **Begin Phase 1: Module Aliasing** (Tasks 1.1-1.5)
4. **Create git branch** for development:
   ```bash
   git checkout -b feature/node-assert-compatibility
   ```

### Questions to Resolve Before Starting

1. **Approval needed?** - Is this plan approved to execute?
2. **Priority order?** - Should any phases be re-ordered?
3. **Async assertions?** - Are async assertions (Phase 6) required or optional?
4. **Test coverage target?** - Is 100% API coverage sufficient or need specific edge case requirements?

### Development Workflow Reminder

For each task:
1. Read relevant code
2. Make minimal, targeted changes
3. Run `make format`
4. Run `make test`
5. Run `make wpt`
6. Verify no regressions
7. Commit with clear message

---

**Plan Status:** 🟡 READY FOR REVIEW
**Awaiting:** Approval to begin implementation
**First Task:** 0.2 - Run baseline tests
**Contact:** Ready to execute upon confirmation
