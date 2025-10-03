# Complete Testing Guide

## Test Organization
- **JavaScript tests**: `test/test_*.js`
- **Test runner**: `test/test_js.c`
- **Individual execution**: `./target/release/jsrt_test_js test/<specific_test>.js`

## Writing Tests

### Mandatory Requirements

```javascript
// ✅ CORRECT: Use jsrt:assert module
const assert = require("jsrt:assert");

// Test basic functionality
assert.strictEqual(1 + 1, 2, "Addition should work");
assert.throws(() => { throw new Error("test"); }, Error);
assert.ok(someCondition, "Condition should be true");

// ❌ WRONG: Manual checks
// if (result !== expected) { throw new Error(...); }  // Don't do this
// console.log("Test passed");  // Don't do this
```

### Platform-Specific Testing
```javascript
// Handle optional dependencies gracefully
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== Tests Completed (Skipped) ===');
  return;
}
// ... crypto tests here ...
```

## Web Platform Tests (WPT)

**Current Status: 90.6% Pass Rate (29/32 tests passing) ✅**

All WPT tests now pass except for 3 browser-specific Fetch API tests that require DOM environment.

### Running WPT Tests
```bash
# Run all WPT tests
make wpt

# Run specific category
make wpt N=url
make wpt N=console
make wpt N=encoding

# Debug mode - show all failures
SHOW_ALL_FAILURES=1 make wpt N=url

# Limit failures shown
MAX_FAILURES=10 make wpt

# Save debug output
mkdir -p target/tmp
SHOW_ALL_FAILURES=1 make wpt N=console > target/tmp/wpt-debug.log 2>&1
```

### WPT Categories (All Passing ✅)
- `console` - Console API tests (100% pass)
- `url` - URL and URLSearchParams tests (100% pass)
- `encoding` - TextEncoder/TextDecoder tests (100% pass)
- `streams` - Streams API tests (100% pass)
- `crypto` - WebCrypto subset tests (100% pass)
- `timers` - Timer functions (100% pass)
- `performance` - Performance API (100% pass)
- `abort` - AbortController/AbortSignal (100% pass)
- `base64` - Base64 utilities (100% pass)

### Debugging WPT Failures

1. **Identify failing tests**:
```bash
SHOW_ALL_FAILURES=1 make wpt N=category > target/tmp/failures.log 2>&1
```

2. **Analyze specific test**:
```bash
# Find the test file
find wpt -name "failing-test-name*"

# Run single test
./target/release/jsrt wpt/category/test-file.js
```

3. **Debug with AddressSanitizer**:
```bash
make jsrt_m
./target/debug/jsrt_m wpt/category/test-file.js
```

## Memory Testing

### AddressSanitizer Build
```bash
# Build with ASAN
make jsrt_m

# Run with leak detection
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m script.js

# Debug specific memory issues
ASAN_OPTIONS=detect_leaks=1,halt_on_error=0 ./target/debug/jsrt_m test.js
```

### Common Memory Issues
- **Use-after-free**: Often in callback handlers
- **Memory leaks**: Missing `JS_FreeValue` calls
- **Buffer overflows**: String manipulation errors

## Test Development Workflow

### 1. Create Test File
```bash
# For unit tests
touch test/test_myfeature.js

# For temporary debugging
mkdir -p target/tmp
touch target/tmp/debug_test.js
```

### 2. Write Test Cases
```javascript
const assert = require("jsrt:assert");

// Setup
const myModule = require("jsrt:mymodule");

// Test cases
assert.strictEqual(myModule.foo(), "expected", "foo should return expected");
assert.throws(() => myModule.bar(null), TypeError, "bar should throw on null");

// Cleanup if needed
console.log("=== Tests Completed ===");
```

### 3. Run and Debug
```bash
# Run individual test
./target/debug/jsrt_g test/test_myfeature.js

# Debug with verbose output
./target/debug/jsrt_g -v test/test_myfeature.js

# Check memory issues
./target/debug/jsrt_m test/test_myfeature.js
```

### 4. Add to Test Suite
```bash
# Ensure test is named test_*.js
# Run full suite
make test
```

## Continuous Integration

### Pre-commit Checklist
```bash
# Format code
make format

# Run unit tests
make test

# Run WPT tests
make wpt

# Clean build
make clean && make
```

### CI Requirements
- All tests must pass
- No memory leaks detected
- Code formatted correctly
- Cross-platform compatibility

## Test File Organization

| Location | Purpose | Git Status |
|----------|---------|------------|
| `test/test_*.js` | Permanent unit tests | Committed |
| `target/tmp/*.js` | Temporary debug tests | Ignored |
| `wpt/` | Web Platform Tests | Submodule |
| `examples/` | Demo scripts | Committed |

## Best Practices

1. **Test isolation**: Each test should be independent
2. **Clear assertions**: Use descriptive error messages
3. **Platform awareness**: Handle optional features gracefully
4. **Resource cleanup**: Free all allocated resources
5. **Error cases**: Test both success and failure paths