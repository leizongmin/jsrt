# GitHub Copilot Instructions for jsrt

## Project Overview

jsrt is a small JavaScript runtime written in C that provides a minimal environment for executing JavaScript code. It's built on top of QuickJS (JavaScript engine) and libuv (asynchronous I/O library).

## Project Structure

- `src/` - Main source code for the JavaScript runtime
  - `main.c` - Entry point of the application
  - `jsrt.c` - Core runtime functionality
  - `runtime.c` - Runtime environment setup
  - `std/` - Standard library implementations (console, timer, etc.)
  - `util/` - Utility functions
- `test/` - Test files (both C and JavaScript tests)
- `deps/` - Git submodules for dependencies
  - `quickjs/` - QuickJS JavaScript engine
  - `libuv/` - libuv for async operations
- `examples/` - Example JavaScript files
- `CMakeLists.txt` - CMake build configuration
- `Makefile` - Make wrapper for common build tasks

## Build System

The project uses CMake as the primary build system with Make as a convenient wrapper:

- `make` or `make jsrt` - Build release version
- `make jsrt_g` - Build debug version with debug symbols
- `make jsrt_m` - Build debug version with AddressSanitizer
- `make test` - Run tests
- `make clean` - Clean build artifacts
- `make clang-format` - Format code with clang-format

### Dependencies

- **gcc** - C compiler
- **make** - Build system
- **cmake** (>= 3.16) - Build configuration
- **clang-format** - Code formatting

## Code Style

- The project uses clang-format for consistent code formatting
- **ALWAYS run `make clang-format` before committing changes** - this is mandatory to ensure consistent formatting
- **ALWAYS ensure `make test` passes before committing changes** - this is mandatory to ensure code quality
- Follow existing C coding conventions in the codebase

## Testing

- JavaScript tests are in `test/test_*.js`
- C test runner is `test/test_js.c`
- Use `make test` to run all tests
- Tests can be run individually with `./target/release/jsrt_test_js test/test_file.js`
- **MANDATORY: All JavaScript test files MUST use the std:assert module for assertions instead of manual console.log checks**
- **MANDATORY: Import assert module with `const assert = require("std:assert")` at the beginning of test files**
- **MANDATORY: Use proper assert methods like `assert.strictEqual()`, `assert.throws()`, etc. instead of manual if/throw patterns**

## Key Files to Understand

- `src/jsrt.h` - Main header with public API
- `src/jsrt.c` - Core runtime implementation
- `src/std/console.c` - Console object implementation
- `src/std/timer.c` - Timer functions (setTimeout, setInterval)

## Development Tips

- The runtime provides a minimal JavaScript environment
- Focus on keeping the runtime small and efficient
- New standard library functions should go in `src/std/`
- Utility functions should go in `src/util/`
- Always run tests after making changes
- **ALWAYS ensure `make test` passes before committing changes**
- Use debug build (`make jsrt_g`) for development and debugging
- **MANDATORY: Run `make clang-format` before committing any code changes**
- **MANDATORY: Do NOT create example JavaScript files in the root directory - always place them in the `examples/` directory**

## Architecture

jsrt integrates:
- QuickJS as the JavaScript engine for parsing and executing JS code
- libuv for asynchronous I/O operations and event loop
- Custom standard library implementations for console, timers, etc.

## Cross-Platform Handling

The project supports multiple platforms (Linux, macOS, Windows) and requires careful attention to platform-specific differences:

### Dynamic Loading
- **Unix/Linux/macOS**: Use `dlopen()`, `dlsym()`, `dlclose()`, `dlerror()` from `<dlfcn.h>`
- **Windows**: Use `LoadLibraryA()`, `GetProcAddress()`, `FreeLibrary()`, `GetLastError()` from `<windows.h>`

**Key Issue**: `GetProcAddress()` returns `FARPROC` (specific function signature) while `dlsym()` returns `void*` (generic pointer). 

**Solution**: Cast Windows result to `void*` for compatibility:
```c
#ifdef _WIN32
#define JSRT_DLSYM(handle, name) ((void*)GetProcAddress(handle, name))
#else
#define JSRT_DLSYM(handle, name) dlsym(handle, name)
#endif
```

### Platform-Specific Headers
```c
#ifdef _WIN32
#include <windows.h>
extern HMODULE openssl_handle;
#else
#include <dlfcn.h>
extern void *openssl_handle;
#endif
```

### Library Linking
The CMakeLists.txt handles platform-specific libraries:
- **Windows**: No need for `dl`, `m`, `pthread` (handled by MSVCRT/libuv)
- **Unix**: Requires `dl m pthread` libraries

### Process and System Functions
Cross-platform implementations needed for process-related functionality:

**Unix/Linux/macOS**: Use standard POSIX functions
```c
#include <unistd.h>
#include <sys/time.h>
// getpid(), getppid(), gettimeofday() available directly
```

**Windows**: Implement equivalents using Win32 APIs
```c
#include <windows.h>
#include <tlhelp32.h>
#include <process.h>      // For getpid()
#include <winsock2.h>     // For struct timeval

// IMPORTANT: Avoid redefinition conflicts!
// Windows already provides getpid() in <process.h>
// Use system getpid() directly: getpid()

// Only implement getppid() since Windows doesn't have it
static int jsrt_getppid(void) {
  HANDLE hSnapshot;
  PROCESSENTRY32 pe32;
  DWORD dwParentPID = 0;
  DWORD dwCurrentPID = GetCurrentProcessId();
  
  hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
  
  pe32.dwSize = sizeof(PROCESSENTRY32);
  if (Process32First(hSnapshot, &pe32)) {
    do {
      if (pe32.th32ProcessID == dwCurrentPID) {
        dwParentPID = pe32.th32ParentProcessID;
        break;
      }
    } while (Process32Next(hSnapshot, &pe32));
  }
  
  CloseHandle(hSnapshot);
  return (int)dwParentPID;
}

// Windows gettimeofday() implementation
// IMPORTANT: Use unique name to avoid conflicts with system headers
static int jsrt_gettimeofday(struct timeval *tv, void *tz) {
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  
  if (NULL != tv) {
    GetSystemTimeAsFileTime(&ft);
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
    tmpres /= 10; // convert to microseconds
    tmpres -= 11644473600000000ULL; // convert to Unix epoch
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }
  
  return 0;
}

// Cross-platform macros for consistent API
#define JSRT_GETPID() getpid()                    // Use system function
#define JSRT_GETPPID() jsrt_getppid()            // Use our implementation
#define JSRT_GETTIMEOFDAY(tv, tz) jsrt_gettimeofday(tv, tz)  // Use our implementation
```

**Critical Notes:**
- **DO NOT redefine system functions** like `getpid()` or `gettimeofday()` on Windows - this causes compilation errors
- **Use system-provided struct timeval** from `<winsock2.h>` instead of defining your own
- **Prefix custom functions** with `jsrt_` to avoid naming conflicts
- **Use cross-platform macros** to provide consistent API across platforms

## Windows Testing Considerations

**Dependency Availability on Windows:**
- **OpenSSL/Crypto**: Often not available on Windows CI environments
- **Test files MUST check for crypto availability** before using `crypto.subtle`
- **Pattern for crypto tests**:
```javascript
// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== Tests Completed (Skipped) ===');
} else {
  // ... crypto tests here ...
}
```

**Test Suite Best Practices:**
- **Always gracefully handle missing dependencies** in JavaScript test files
- **Use availability checks** for platform-specific APIs like crypto, network features
- **Test runner should continue on failure** to identify all failing tests (not just the first one)
- **Debug builds show helpful messages** about which dependencies are missing

**Common Windows Test Failures:**
1. **Crypto tests failing** due to OpenSSL unavailability → Add availability checks
2. **Path separator issues** → Use consistent forward slashes or handle both
3. **Timing-sensitive tests** → Allow for different timing characteristics
4. **Process behavior differences** → Handle platform-specific process APIs

When suggesting code changes:
- Maintain compatibility with the existing API
- Keep memory management patterns consistent
- Follow the error handling patterns used throughout the codebase
- Consider cross-platform compatibility
- Test changes on multiple platforms when dealing with platform-specific code
- **CRITICAL**: Never redefine system functions on Windows - use unique prefixed names (e.g., `jsrt_function_name()`)
- **CRITICAL**: Always check for existing system headers before defining structs or functions
- **CRITICAL**: Use cross-platform macros for consistent API across different platforms