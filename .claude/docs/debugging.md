# Debugging and Troubleshooting Guide

## Debug Build Usage

### Building Debug Versions
```bash
# Debug build with symbols
make jsrt_g
./target/debug/jsrt_g script.js

# Debug with AddressSanitizer
make jsrt_m
./target/debug/jsrt_m script.js

# Verbose output
./target/debug/jsrt_g -v script.js
```

## Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| Segmentation fault | Memory issues | Use `jsrt_m` build with AddressSanitizer |
| Tests failing on Windows | Missing OpenSSL | Add crypto availability checks |
| Undefined symbols | Missing libraries | Check CMakeLists.txt platform conditions |
| Event loop hanging | Unhandled async ops | Debug with `uv_run` mode flags |
| Memory leaks | Missing cleanup | Use ASAN leak detection |
| JavaScript exceptions | Runtime errors | Check exception with `JS_IsException()` |

## Debugging Tools

### 1. GDB (Linux/Unix)
```bash
# Start debugging
gdb ./target/debug/jsrt_g

# Set breakpoints
(gdb) break jsrt_execute_script
(gdb) break src/std/console.c:42

# Run with arguments
(gdb) run script.js

# Examine variables
(gdb) print ctx
(gdb) print *argv
(gdb) backtrace

# Continue execution
(gdb) continue
```

### 2. LLDB (macOS)
```bash
# Start debugging
lldb ./target/debug/jsrt_g

# Set breakpoints
(lldb) b jsrt_execute_script
(lldb) b src/std/console.c:42

# Run with arguments
(lldb) run script.js

# Examine variables
(lldb) p ctx
(lldb) p *argv
(lldb) bt

# Continue execution
(lldb) continue
```

### 3. AddressSanitizer
```bash
# Build with ASAN
make jsrt_m

# Run with leak detection
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m script.js

# Run with detailed output
ASAN_OPTIONS=verbosity=3,detect_leaks=1 ./target/debug/jsrt_m script.js

# Continue on error
ASAN_OPTIONS=halt_on_error=0 ./target/debug/jsrt_m script.js
```

### 4. Valgrind (Linux)
```bash
# Memory leak detection
valgrind --leak-check=full ./target/release/jsrt script.js

# Detailed memory analysis
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./target/release/jsrt script.js

# Generate suppression file
valgrind --gen-suppressions=all ./target/release/jsrt script.js
```

## Debugging JavaScript Code

### 1. Console Debugging
```javascript
// Basic logging
console.log("Value:", value);
console.error("Error occurred:", error);
console.warn("Warning:", message);

// Stack traces
console.trace("Trace point");

// Timing
console.time("operation");
// ... code ...
console.timeEnd("operation");
```

### 2. Assert Module
```javascript
const assert = require("jsrt:assert");

// Debug assertions
assert.ok(condition, "Condition should be true");
assert.strictEqual(actual, expected, "Values should match");

// Type checking
assert.throws(() => {
    problematicCode();
}, Error, "Should throw error");
```

### 3. Error Handling
```javascript
try {
    riskyOperation();
} catch (error) {
    console.error("Error:", error.message);
    console.error("Stack:", error.stack);
}
```

## Memory Debugging

### Common Memory Issues

#### Use-After-Free
```c
// WRONG
JS_FreeValue(ctx, value);
JS_Call(ctx, value, ...);  // Use after free!

// CORRECT
JSValue result = JS_Call(ctx, value, ...);
JS_FreeValue(ctx, value);
```

#### Memory Leaks
```c
// WRONG
JSValue str = JS_NewString(ctx, "test");
return JS_UNDEFINED;  // Leak!

// CORRECT
JSValue str = JS_NewString(ctx, "test");
JS_FreeValue(ctx, str);
return JS_UNDEFINED;
```

#### Buffer Overflows
```c
// WRONG
char buffer[100];
strcpy(buffer, user_input);  // Overflow risk!

// CORRECT
char buffer[100];
snprintf(buffer, sizeof(buffer), "%s", user_input);
```

### Memory Debugging Workflow
```bash
# 1. Build with ASAN
make jsrt_m

# 2. Create minimal test case
echo 'console.log("test");' > target/tmp/minimal.js

# 3. Run with ASAN
ASAN_OPTIONS=detect_leaks=1 ./target/debug/jsrt_m target/tmp/minimal.js

# 4. Analyze output
# Look for:
# - heap-use-after-free
# - heap-buffer-overflow
# - memory leaks
# - stack-buffer-overflow
```

## Performance Debugging

### 1. Profiling with perf (Linux)
```bash
# Record performance data
perf record ./target/release/jsrt script.js

# Analyze results
perf report

# CPU flame graphs
perf record -g ./target/release/jsrt script.js
perf script | flamegraph.pl > flame.svg
```

### 2. Profiling with Instruments (macOS)
```bash
# Build with symbols
make jsrt_g

# Profile with Instruments
instruments -t "Time Profiler" ./target/debug/jsrt_g script.js
```

### 3. Built-in Timing
```javascript
// JavaScript timing
console.time("operation");
heavyOperation();
console.timeEnd("operation");

// Multiple timers
console.time("total");
console.time("step1");
step1();
console.timeEnd("step1");
console.time("step2");
step2();
console.timeEnd("step2");
console.timeEnd("total");
```

## WPT Test Debugging

### 1. Identify Failures
```bash
# Show all failures
SHOW_ALL_FAILURES=1 make wpt N=url > target/tmp/failures.log 2>&1

# Analyze specific test
grep "FAIL" target/tmp/failures.log | head -20
```

### 2. Run Individual Test
```bash
# Find test file
find wpt -name "*url-constructor*"

# Run single test
./target/release/jsrt wpt/url/url-constructor.any.js
```

### 3. Debug Test Execution
```bash
# Use debug build
./target/debug/jsrt_g wpt/url/url-constructor.any.js

# With ASAN
./target/debug/jsrt_m wpt/url/url-constructor.any.js

# With verbose output
./target/debug/jsrt_g -v wpt/url/url-constructor.any.js
```

## Troubleshooting Checklist

### Build Issues
- [ ] Submodules initialized: `git submodule update --init`
- [ ] Dependencies installed: cmake, build tools
- [ ] Clean build: `make clean && make`

### Runtime Issues
- [ ] Check error messages in console
- [ ] Use debug build for better errors
- [ ] Enable verbose mode with `-v`
- [ ] Check memory with ASAN

### Test Failures
- [ ] Run individual test to isolate
- [ ] Check for platform-specific issues
- [ ] Verify test prerequisites
- [ ] Compare with expected output

### Performance Issues
- [ ] Profile with appropriate tools
- [ ] Check for memory leaks
- [ ] Analyze algorithm complexity
- [ ] Review async operations