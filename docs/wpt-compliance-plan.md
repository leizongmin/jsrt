# WPT Compliance Plan for WinterCG Minimum Common API

## Executive Summary

Current Status: **21.9% pass rate (7/32 tests passing)**

This document outlines a comprehensive plan to achieve full WPT (Web Platform Tests) compliance according to the WinterCG Minimum Common API specification. The plan prioritizes fixes based on impact, complexity, and dependency relationships.

## Current Test Results Analysis

### ✅ Passing Tests (7)
- `console/console-log-large-array.any.js`
- `console/console-tests-historical.any.js` 
- `hr-time/monotonic-clock.any.js`
- `url/url-tojson.any.js`
- `url/urlsearchparams-get.any.js`
- `html/webappapis/timers/cleartimeout-clearinterval.any.js`

### ❌ Failing Tests (21)
- **Console**: 1 failure
- **Encoding**: 8 failures (TextEncoder/TextDecoder issues)
- **URL**: 6 failures (URLSearchParams functionality)
- **WebCrypto**: 1 failure
- **Base64**: 1 failure
- **Timers**: 2 failures
- **Streams**: 2 failures

### ⭕ Skipped Tests (4)
- Fetch API tests (require window global)
- URLSearchParams constructor (needs DOMException)

## Priority-Based Implementation Plan

### Phase 1: Critical Foundation APIs (2-3 weeks)

#### 1.1 TextEncoder/TextDecoder (HIGH PRIORITY)
**Files to modify**: `src/std/encoding.c`
**Current issues**:
- Missing `encodings_table` reference
- Incomplete UTF-8/UTF-16 support
- Fatal error handling missing

**Implementation plan**:
```c
// Add missing encodings table
static const struct {
    const char* name;
    const char* canonical;
} encodings_table[] = {
    {"utf-8", "utf-8"},
    {"utf8", "utf-8"},
    {"unicode-1-1-utf-8", "utf-8"},
    // ... additional encodings
};

// Fix TextDecoder constructor
static JSValue js_textdecoder_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);

// Fix TextEncoder constructor  
static JSValue js_textencoder_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
```

**Tests to fix**:
- `encoding/api-basics.any.js`
- `encoding/textdecoder-fatal.any.js`
- `encoding/textdecoder-labels.any.js`
- `encoding/textencoder-*.js` (4 tests)

#### 1.2 Base64 Implementation (MEDIUM PRIORITY)
**Files to modify**: `src/std/base64.c`
**Current issues**:
- Lexical identifier redefinition error
- Missing proper atob/btoa implementations

**Implementation plan**:
```c
// Fix global namespace conflicts
static JSValue js_atob(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_btoa(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// Ensure proper base64 validation and error handling
```

**Tests to fix**:
- `html/webappapis/atob/base64.any.js`

### Phase 2: URL and URLSearchParams (2 weeks)

#### 2.1 URLSearchParams Improvements
**Files to modify**: `src/std/url.c`
**Current issues**:
- Missing methods: `getAll()`, `has()`, `set()`, `size`
- Incorrect stringifier behavior

**Implementation plan**:
```c
// Add missing URLSearchParams methods
static JSValue js_urlsearchparams_getall(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_urlsearchparams_has(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_urlsearchparams_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// Fix size property implementation
static JSValue js_urlsearchparams_size_get(JSContext *ctx, JSValueConst this_val);

// Fix stringifier
static JSValue js_urlsearchparams_tostring(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
```

**Tests to fix**:
- `url/urlsearchparams-getall.any.js`
- `url/urlsearchparams-has.any.js` 
- `url/urlsearchparams-set.any.js`
- `url/urlsearchparams-size.any.js`
- `url/urlsearchparams-stringifier.any.js`

#### 2.2 URL Constructor and Origin
**Files to modify**: `src/std/url.c`
**Current issues**:
- URL constructor parameter validation
- Origin property implementation

**Tests to fix**:
- `url/url-constructor.any.js`
- `url/url-origin.any.js`

### Phase 3: Timer and Event Loop (1 week)

#### 3.1 Timer Edge Cases
**Files to modify**: `src/std/timer.c`
**Current issues**:
- Missing timeout handling for setInterval
- Negative timeout behavior
- Test framework integration issues

**Implementation plan**:
```c
// Fix negative timeout handling
static JSValue js_settimeout(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // Ensure negative timeouts are clamped to 0
    double delay = 0;
    if (argc > 1) {
        JS_ToFloat64(ctx, &delay, argv[1]);
        if (delay < 0) delay = 0;
    }
    // ... rest of implementation
}

// Fix setInterval timeout behavior
static JSValue js_setinterval(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
```

**Tests to fix**:
- `html/webappapis/timers/missing-timeout-setinterval.any.js`
- `html/webappapis/timers/negative-setinterval.any.js`
- `html/webappapis/timers/negative-settimeout.any.js`

### Phase 4: Streams and AbortController (2-3 weeks)

#### 4.1 Streams Implementation
**Files to modify**: `src/std/streams.c`
**Current issues**:
- Incomplete ReadableStream implementation
- Missing WritableStream constructor
- Integration with other APIs

**Implementation plan**:
```c
// Implement ReadableStream default reader
typedef struct {
    JSValue stream;
    bool closed;
    bool locked;
} JSReadableStreamDefaultReader;

static JSValue js_readable_stream_getreader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// Implement WritableStream constructor
static JSValue js_writable_stream_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
```

**Tests to fix**:
- `streams/readable-streams/default-reader.any.js`
- `streams/writable-streams/constructor.any.js`

#### 4.2 AbortController/AbortSignal
**Files to modify**: `src/std/abort.c`
**Current issues**:
- Missing test helper functions
- Incomplete AbortSignal.any() implementation

**Implementation plan**:
```c
// Add AbortSignal.any() static method
static JSValue js_abort_signal_any(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// Ensure proper integration with test harness
```

**Tests to fix**:
- `dom/abort/abort-signal-any.any.js`

### Phase 5: Console and WebCrypto (1 week)

#### 5.1 Console Namespace
**Files to modify**: `src/std/console.c`
**Current issues**:
- Console should be a namespace object, not a constructor

**Implementation plan**:
```c
// Ensure console is not constructible
static JSValue js_console_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    return JS_ThrowTypeError(ctx, "console is not a constructor");
}
```

**Tests to fix**:
- `console/console-is-a-namespace.any.js`

#### 5.2 WebCrypto getRandomValues
**Files to modify**: `src/std/crypto.c`
**Current issues**:
- Missing or incomplete getRandomValues implementation

**Tests to fix**:
- `WebCryptoAPI/getRandomValues.any.js`

### Phase 6: DOMException Support (1 week)

#### 6.1 DOMException Implementation
**Files to create/modify**: `src/std/dom.c` (new), `src/runtime.c`
**Current issues**:
- URLSearchParams constructor test skipped due to missing DOMException

**Implementation plan**:
```c
// Create DOMException constructor
typedef struct {
    const char* name;
    const char* message;
    uint16_t code;
} JSDOMException;

static JSValue js_dom_exception_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);

// Add to runtime setup
void JSRT_RuntimeSetupStdDOM(JSRT_Runtime* rt);
```

**Tests to enable**:
- `url/urlsearchparams-constructor.any.js`

## Implementation Strategy

### Code Organization
1. **Modular approach**: Each API in separate files under `src/std/`
2. **Shared utilities**: Common functions in `src/util/`
3. **Test-driven development**: Fix tests incrementally
4. **Memory management**: Proper cleanup for all allocated resources

### Testing Approach
1. **Incremental testing**: Run `make wpt` after each fix
2. **Regression prevention**: Ensure existing passing tests continue to pass  
3. **Cross-platform validation**: Test on Linux, macOS, Windows
4. **Memory leak detection**: Use `make jsrt_m` for AddressSanitizer builds

### Quality Assurance
1. **Code formatting**: Run `make clang-format` before commits
2. **Static analysis**: Regular code review
3. **Performance testing**: Monitor runtime performance impact
4. **Documentation**: Update API docs as features are added

## Dependencies and Prerequisites

### Build Dependencies
- OpenSSL/libcrypto (for WebCrypto APIs)
- libuv (for streams and async operations)
- QuickJS (JavaScript engine)

### Code Dependencies
```c
// Key header files that need updates
src/jsrt.h           // Public API definitions
src/runtime.h        // Runtime setup functions  
src/std/*.h          // Individual module headers
```

## Risk Assessment

### High Risk
- **Streams implementation**: Complex async semantics
- **WebCrypto**: Security-sensitive cryptographic operations
- **Memory management**: Potential leaks in error paths

### Medium Risk  
- **URL parsing**: Edge cases in RFC compliance
- **Timer behavior**: Platform-specific timing issues
- **Encoding**: Character set conversion accuracy

### Low Risk
- **Console improvements**: Simple namespace fixes
- **Base64**: Well-defined algorithm
- **DOMException**: Standard error object

## Success Metrics

### Target Goals
- **Phase 1 completion**: 50% pass rate (16/32 tests)
- **Phase 2 completion**: 70% pass rate (22/32 tests)
- **Phase 3 completion**: 80% pass rate (26/32 tests)
- **Final completion**: 95%+ pass rate (30+/32 tests)

### Performance Benchmarks
- Memory usage increase < 20% from baseline
- Startup time increase < 10% from baseline
- Runtime performance impact < 5% for core operations

## Timeline Summary

| Phase | Duration | Focus Area | Expected Pass Rate |
|-------|----------|------------|-------------------|
| Phase 1 | 3 weeks | Encoding, Base64 | 50% |
| Phase 2 | 2 weeks | URL APIs | 70% |
| Phase 3 | 1 week | Timers | 80% |
| Phase 4 | 3 weeks | Streams, Abort | 90% |
| Phase 5 | 1 week | Console, Crypto | 95% |
| Phase 6 | 1 week | DOMException | 100% |

**Total Duration**: 11 weeks

## Next Steps

1. **Immediate**: Start with Phase 1 - TextEncoder/TextDecoder fixes
2. **Setup**: Create feature branches for each phase
3. **Validation**: Establish automated WPT testing in CI
4. **Documentation**: Update user-facing docs as APIs are completed

This plan provides a systematic approach to achieving full WPT compliance while maintaining code quality and project stability.