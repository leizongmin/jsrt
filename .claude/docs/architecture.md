# jsrt Architecture & Components

## System Architecture

### Core Components

| File | Purpose | Key Functions |
|------|---------|---------------|
| `src/jsrt.h` | Public API definitions | Runtime initialization, execution |
| `src/jsrt.c` | Core runtime logic | Context management, module loading |
| `src/runtime.c` | Environment setup | Global object initialization |
| `src/main.c` | Application entry point | Command-line processing, script execution |

### Standard Library Modules

| Module | File | Functionality |
|--------|------|---------------|
| Console | `src/std/console.c` | log, error, warn, debug methods |
| Timer | `src/std/timer.c` | setTimeout, setInterval, clearTimeout |
| Process | `src/std/process.c` | Process information, environment |
| URL | `src/std/url.c` | URL parsing and manipulation |
| Crypto | `src/std/crypto.c` | WebCrypto subset implementation |
| Encoding | `src/std/encoding.c` | TextEncoder/TextDecoder |

## Module System

### Adding New Modules

#### 1. Create Module File
```c
// src/std/mymodule.c
#include "../jsrt.h"

static JSValue js_mymodule_method(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    // Implementation
    return JS_NewInt32(ctx, 42);
}

JSValue jsrt_init_module_mymodule(JSContext *ctx) {
    JSValue module = JS_NewObject(ctx);
    
    // Add methods
    JS_SetPropertyStr(ctx, module, "myMethod",
                      JS_NewCFunction(ctx, js_mymodule_method, "myMethod", 1));
    
    // Add properties
    JS_SetPropertyStr(ctx, module, "MY_CONST",
                      JS_NewInt32(ctx, 42));
    
    return module;
}
```

#### 2. Register in Runtime
```c
// In src/runtime.c
extern JSValue jsrt_init_module_mymodule(JSContext *ctx);

// In jsrt_init_runtime()
js_std_add_module(ctx, "mymodule", jsrt_init_module_mymodule);
```

#### 3. Update Build System
```cmake
# In CMakeLists.txt
set(JSRT_SOURCES
    # ... existing sources ...
    src/std/mymodule.c
)
```

## Memory Management

### QuickJS Memory Functions
```c
// Allocation
void *js_malloc(JSContext *ctx, size_t size);
void *js_mallocz(JSContext *ctx, size_t size);  // Zero-initialized
void *js_realloc(JSContext *ctx, void *ptr, size_t size);

// Deallocation
void js_free(JSContext *ctx, void *ptr);

// JS Value management
void JS_FreeValue(JSContext *ctx, JSValue v);
JSValue JS_DupValue(JSContext *ctx, JSValueConst v);
```

### Memory Management Rules
1. **Match allocations**: Every `js_malloc` needs `js_free`
2. **Handle JS values**: Use `JS_FreeValue` for temporary values
3. **Check returns**: Handle allocation failures gracefully
4. **Avoid leaks**: Free resources in error paths

### Error Handling Pattern
```c
JSValue result = JS_UNDEFINED;
char *buffer = NULL;

// Allocate resources
buffer = js_malloc(ctx, size);
if (!buffer) {
    return JS_ThrowOutOfMemory(ctx);
}

// Perform operation
result = some_operation(ctx, buffer);
if (JS_IsException(result)) {
    js_free(ctx, buffer);
    return result;  // Propagate exception
}

// Clean up and return
js_free(ctx, buffer);
return result;
```

## Event Loop Integration

### libuv Integration
```c
// Event loop initialization
uv_loop_t *loop = uv_default_loop();

// Timer implementation
typedef struct {
    JSContext *ctx;
    JSValue callback;
    uv_timer_t timer;
} jsrt_timer_t;

// Running the event loop
uv_run(loop, UV_RUN_DEFAULT);
```

### Async Operation Pattern
```c
// 1. Setup async handle
uv_async_t *async = js_malloc(ctx, sizeof(uv_async_t));
uv_async_init(loop, async, async_callback);

// 2. Store JS callback
async->data = JS_DupValue(ctx, callback);

// 3. Trigger from any thread
uv_async_send(async);

// 4. Cleanup
JS_FreeValue(ctx, async->data);
uv_close((uv_handle_t*)async, close_callback);
```

## Build System

### CMake Configuration
```cmake
# Core configuration
cmake_minimum_required(VERSION 3.16)
project(jsrt C)

# Dependencies
add_subdirectory(deps/quickjs)
add_subdirectory(deps/libuv)

# Target definition
add_executable(jsrt ${JSRT_SOURCES})
target_link_libraries(jsrt PRIVATE quickjs uv)

# Platform-specific
if(NOT WIN32)
    target_link_libraries(jsrt PRIVATE dl m pthread)
endif()
```

### Build Variants
| Target | Flags | Purpose |
|--------|-------|---------|
| Release | `-O3 -DNDEBUG` | Production use |
| Debug | `-g -O0` | Development |
| ASAN | `-fsanitize=address` | Memory debugging |

## Dependency Management

### Git Submodules
- **QuickJS**: JavaScript engine (`deps/quickjs`)
- **libuv**: Event loop (`deps/libuv`)

### Submodule Rules
- ❌ **NEVER** modify submodule source directly
- ✅ Use CMake configuration for customization
- ✅ Update to official upstream versions only

## File Organization

### Source Tree
```
src/
├── main.c              # Entry point
├── jsrt.c              # Core runtime
├── jsrt.h              # Public API
├── runtime.c           # Environment setup
├── std/                # Standard library
│   ├── console.c
│   ├── timer.c
│   ├── process.c
│   └── ...
└── util/               # Utilities
    ├── helper.c
    └── helper.h
```

### Coding Standards
- Keep files under 500 lines
- One module per file in `src/std/`
- Shared utilities in `src/util/`
- Platform abstractions with `jsrt_` prefix