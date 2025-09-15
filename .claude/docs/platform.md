# Cross-Platform Development Guide

## Supported Platforms
- ✅ Linux (primary development)
- ✅ macOS (full support)
- ✅ Windows (via MinGW/MSVC)

## Platform Abstraction Patterns

### 1. Dynamic Library Loading
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

### 2. Build Configuration (CMakeLists.txt)
```cmake
if(NOT WIN32)
  # Unix-like systems need these libraries
  target_link_libraries(jsrt PRIVATE dl m pthread)
else()
  # Windows handles these internally
  target_link_libraries(jsrt PRIVATE ws2_32)  # For networking
endif()
```

### 3. System Functions Implementation

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

## Critical Platform Rules

⚠️ **NEVER DO THIS:**
- Redefine system functions (causes compilation errors)
- Define structs that already exist in system headers
- Use unprefixed function names that might conflict

✅ **ALWAYS DO THIS:**
- Prefix custom implementations with `jsrt_`
- Use system-provided types when available
- Test on all target platforms before merging
- Use cross-platform macros for consistent API

## Platform-Specific Testing

### Windows CI Considerations
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

### Cross-Platform Path Handling
```javascript
// Use forward slashes consistently
const path = "src/test/file.js";  // Works on all platforms

// Or normalize if needed
function normalizePath(p) {
  return p.replace(/\\/g, '/');
}
```

## Common Platform Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Undefined symbols | Missing libraries | Check CMakeLists.txt platform conditions |
| Tests failing on Windows | Missing OpenSSL | Add crypto availability checks |
| Compilation errors | System function conflicts | Use `jsrt_` prefix for custom implementations |
| Different behavior | Platform-specific APIs | Use abstraction macros |