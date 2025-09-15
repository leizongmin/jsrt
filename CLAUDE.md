# jsrt Development Guidelines

## Project Overview

**jsrt** is a lightweight JavaScript runtime written in C, designed for minimal resource footprint while providing essential JavaScript execution capabilities.

### Core Technologies
- **QuickJS**: JavaScript engine for parsing and executing JS code
- **libuv**: Asynchronous I/O operations and event loop management
- **Custom Stdlib**: Minimal standard library implementations (console, timers, etc.)

## Project Structure

```
jsrt/
├── src/                    # Core runtime source code
│   ├── main.c             # Application entry point
│   ├── jsrt.c             # Core runtime implementation
│   ├── jsrt.h             # Main public API header
│   ├── runtime.c          # Runtime environment setup
│   ├── std/               # Standard library modules
│   │   ├── console.c      # Console object implementation
│   │   ├── timer.c        # Timer functions (setTimeout, setInterval)
│   │   └── ...            # Other stdlib modules
│   └── util/              # Utility functions
├── test/                   # Test suite
│   ├── test_*.js          # JavaScript test files
│   └── test_js.c          # C test runner
├── deps/                   # Dependencies (git submodules)
│   ├── quickjs/           # QuickJS JavaScript engine
│   └── libuv/             # libuv async I/O library
├── examples/              # Example JavaScript files
├── CMakeLists.txt         # CMake build configuration
└── Makefile               # Convenient build wrapper
```

## Build System

### Quick Start
```bash
# Clone with submodules
git clone --recursive <repo_url>

# Build release version
make

# Run tests
make test
```

### Build Targets
| Command | Description | Use Case |
|---------|-------------|----------|
| `make` or `make jsrt` | Release build (optimized) | Production use |
| `make jsrt_g` | Debug build with symbols | Development/debugging |
| `make jsrt_m` | Debug with AddressSanitizer | Memory debugging |
| `make test` | Run all tests | CI/verification |
| `make clean` | Remove build artifacts | Clean rebuild |
| `make clang-format` | Format all C code | Before committing |

### Prerequisites
- **C Compiler**: gcc or clang
- **Build Tools**: make, cmake (>= 3.16)
- **Formatting**: clang-format (for code style)
- **Optional**: OpenSSL/libcrypto (for WebCrypto API)

## Development Workflow

### 1. Before Starting Work
```bash
# Update submodules
git submodule update --init --recursive

# Build debug version for development
make jsrt_g
```

### 2. During Development
- Use debug build (`jsrt_g`) for better error messages
- Test changes frequently with individual test files
- Use AddressSanitizer build (`jsrt_m`) for memory issue detection

### 3. Before Committing
```bash
# Format code (MANDATORY)
make clang-format

# Run tests (MANDATORY)
make test

# Build release to verify optimization
make clean && make
```

### Code Style Requirements
- ✅ **MANDATORY**: Run `make clang-format` before every commit
- ✅ **MANDATORY**: Ensure `make test` passes without failures
- ✅ **MANDATORY**: Keep source files under 500 lines - split into smaller modules if exceeded
- ✅ Follow existing patterns and conventions in the codebase
- ❌ Never commit without formatting and testing

### Dependency Management Rules
- ❌ **NEVER**: Modify code directly in the `deps/` directory (git submodules)
- ✅ **ALWAYS**: Use CMake configuration or build flags to address dependency issues
- ✅ **ALWAYS**: Update submodule references to use official upstream versions
- ❌ **NEVER**: Commit changes that modify submodule source code directly

## Testing Guidelines

### Test Organization
- **JavaScript tests**: `test/test_*.js`
- **Test runner**: `test/test_js.c`
- **Individual execution**: `./target/release/jsrt_test_js test/<specific_test>.js`

### Writing Tests (MANDATORY Requirements)

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

## Architecture & Key Components

### Core Files
| File | Purpose | Key Functions |
|------|---------|---------------|
| `src/jsrt.h` | Public API definitions | Runtime initialization, execution |
| `src/jsrt.c` | Core runtime logic | Context management, module loading |
| `src/runtime.c` | Environment setup | Global object initialization |
| `src/std/console.c` | Console implementation | log, error, warn methods |
| `src/std/timer.c` | Timer functions | setTimeout, setInterval, clearTimeout |

### Adding New Features

#### Standard Library Module
```c
// 1. Create new file in src/std/
// src/std/mymodule.c

// 2. Implement module initialization
JSValue jsrt_init_module_mymodule(JSContext *ctx) {
    JSValue module = JS_NewObject(ctx);
    // Add properties and methods
    JS_SetPropertyStr(ctx, module, "myMethod",
                      JS_NewCFunction(ctx, my_method, "myMethod", 1));
    return module;
}

// 3. Register in runtime.c
```

#### File Organization Rules
- **`src/std/`**: Standard library modules only
- **`src/util/`**: Shared utility functions
- **`examples/`**: ALL example JS files (never in root)
- **`test/`**: Test files prefixed with `test_`

## Cross-Platform Development

### Supported Platforms
- ✅ Linux (primary development)
- ✅ macOS (full support)
- ✅ Windows (via MinGW/MSVC)

### Platform Abstraction Patterns

#### 1. Dynamic Library Loading
```c
// Cross-platform macro for symbol lookup
#ifdef _WIN32
  #include <windows.h>
  #define JSRT_DLOPEN(name) LoadLibraryA(name)
  #define JSRT_DLSYM(handle, name) ((void*)GetProcAddress(handle, name))
  #define JSRT_DLCLOSE(handle) FreeLibrary(handle)
  typedef HMODULE jsrt_handle_t;
#else
  #include <dlfcn.h>
  #define JSRT_DLOPEN(name) dlopen(name, RTLD_LAZY)
  #define JSRT_DLSYM(handle, name) dlsym(handle, name)
  #define JSRT_DLCLOSE(handle) dlclose(handle)
  typedef void* jsrt_handle_t;
#endif
```

#### 2. Build Configuration (CMakeLists.txt)
```cmake
if(NOT WIN32)
  # Unix-like systems need these libraries
  target_link_libraries(jsrt PRIVATE dl m pthread)
else()
  # Windows handles these internally
  target_link_libraries(jsrt PRIVATE ws2_32)  # For networking
endif()
```

#### 3. System Functions Implementation

```c
// Platform detection and includes
#ifdef _WIN32
  #include <windows.h>
  #include <process.h>     // getpid()
  #include <winsock2.h>    // struct timeval
  #include <tlhelp32.h>    // Process enumeration
#else
  #include <unistd.h>
  #include <sys/time.h>
#endif

// Cross-platform process functions
#ifdef _WIN32
  // Windows: Implement missing getppid()
  static int jsrt_getppid(void) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe32 = {.dwSize = sizeof(PROCESSENTRY32)};
    DWORD currentPID = GetCurrentProcessId();
    DWORD parentPID = 0;

    if (Process32First(hSnapshot, &pe32)) {
      do {
        if (pe32.th32ProcessID == currentPID) {
          parentPID = pe32.th32ParentProcessID;
          break;
        }
      } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return (int)parentPID;
  }

  // Windows: Implement gettimeofday()
  static int jsrt_gettimeofday(struct timeval *tv, void *tz) {
    if (!tv) return -1;

    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    unsigned __int64 time = ((unsigned __int64)ft.dwHighDateTime << 32) |
                            ft.dwLowDateTime;
    time /= 10;  // Convert to microseconds
    time -= 11644473600000000ULL;  // Convert to Unix epoch

    tv->tv_sec = (long)(time / 1000000UL);
    tv->tv_usec = (long)(time % 1000000UL);
    return 0;
  }

  #define JSRT_GETPID() getpid()  // Use system function
  #define JSRT_GETPPID() jsrt_getppid()
  #define JSRT_GETTIMEOFDAY(tv, tz) jsrt_gettimeofday(tv, tz)
#else
  // Unix: Use native functions
  #define JSRT_GETPID() getpid()
  #define JSRT_GETPPID() getppid()
  #define JSRT_GETTIMEOFDAY(tv, tz) gettimeofday(tv, tz)
#endif
```

### Critical Platform Rules

⚠️ **NEVER DO THIS:**
- Redefine system functions (causes compilation errors)
- Define structs that already exist in system headers
- Use unprefixed function names that might conflict

✅ **ALWAYS DO THIS:**
- Prefix custom implementations with `jsrt_`
- Use system-provided types when available
- Test on all target platforms before merging
- Use cross-platform macros for consistent API

## Debugging and Troubleshooting

### Debug Build Usage
```bash
# Build with debug symbols
make jsrt_g

# Run with verbose output
./target/debug/jsrt_g -v script.js

# Memory debugging with AddressSanitizer
make jsrt_m
./target/debug/jsrt_m script.js
```

### Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| Segmentation fault | Memory issues | Use `jsrt_m` build with AddressSanitizer |
| Tests failing on Windows | Missing OpenSSL | Add crypto availability checks |
| Undefined symbols | Missing libraries | Check CMakeLists.txt platform conditions |
| Event loop hanging | Unhandled async ops | Debug with `uv_run` mode flags |

### Platform-Specific Testing

#### Windows CI Considerations
```javascript
// Test template for optional features
function testWithOptionalFeature(feature, testFn) {
  if (typeof feature === 'undefined' || !feature) {
    console.log(`❌ SKIP: ${feature} not available`);
    return;
  }
  testFn();
}

// Example: Crypto tests
testWithOptionalFeature(crypto?.subtle, () => {
  // Crypto-dependent tests here
  assert.ok(crypto.subtle.digest);
});
```

#### Cross-Platform Path Handling
```javascript
// Use forward slashes consistently
const path = "src/test/file.js";  // Works on all platforms

// Or normalize if needed
function normalizePath(p) {
  return p.replace(/\\/g, '/');
}
```

## Contributing Guidelines

### Checklist Before PR
- [ ] Code formatted with `make clang-format`
- [ ] All tests pass with `make test`
- [ ] No memory leaks (check with `make jsrt_m`)
- [ ] Cross-platform compatibility considered
- [ ] Examples placed in `examples/` directory
- [ ] Documentation updated if API changed

### Memory Management Rules
1. **Always match allocations**: Every `malloc` needs a `free`
2. **Use QuickJS memory functions**: `js_malloc`, `js_free` for JS-related allocations
3. **Handle JS values properly**: Use `JS_FreeValue` for temporary values
4. **Check return values**: Handle allocation failures gracefully

### Error Handling Pattern
```c
// Standard error handling pattern
JSValue result = some_operation(ctx);
if (JS_IsException(result)) {
    // Clean up resources
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}
// Use result...
JS_FreeValue(ctx, result);
```

## Quick Reference

### Essential Commands
```bash
# Development cycle
make jsrt_g          # Debug build
./target/debug/jsrt_g examples/test.js  # Test changes
make clang-format    # Format code
make test            # Run tests

# Before commit
make clean && make && make test
```

### Module Registration Template
```c
// In src/std/mymodule.c
JSValue jsrt_init_module_mymodule(JSContext *ctx) {
    JSValue module = JS_NewObject(ctx);

    // Add functions
    JS_SetPropertyStr(ctx, module, "myFunc",
                      JS_NewCFunction(ctx, js_myFunc, "myFunc", 1));

    // Add constants
    JS_SetPropertyStr(ctx, module, "MY_CONST",
                      JS_NewInt32(ctx, 42));

    return module;
}

// Register in runtime.c
js_std_add_module(ctx, "mymodule", jsrt_init_module_mymodule);
```
