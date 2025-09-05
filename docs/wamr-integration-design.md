# WAMR WebAssembly Integration Design

## Overview

This document outlines the design and implementation plan for integrating WAMR (WebAssembly Micro Runtime) into the jsrt JavaScript runtime, enabling WebAssembly module loading, execution, and JavaScript interoperability.

## Background

### Why WebAssembly in jsrt?

WebAssembly (WASM) provides:
- **Near-native performance** for compute-intensive tasks
- **Security sandbox** with controlled memory access
- **Language interoperability** supporting C/C++, Rust, Go, etc.
- **Portable bytecode** that runs consistently across platforms
- **Complementary to JavaScript** for performance-critical operations

### Why WAMR?

WAMR was selected as the WebAssembly runtime engine based on:

| Criteria | WAMR Advantages |
|----------|----------------|
| **Size** | 85KB (interpreter) / 50KB (AOT) - suitable for embedded |
| **Performance** | Fast interpreter with 150% improvement over classic |
| **Maintenance** | Active development by Intel/Bytecode Alliance |
| **Features** | Dual interpreter modes, JIT/AOT support |
| **Platform** | Cross-platform C implementation |
| **Licensing** | Apache 2.0 License - compatible with jsrt |

## Architecture Design

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        jsrt Runtime                         │
├─────────────────────────────────────────────────────────────┤
│  JavaScript Engine (QuickJS)                               │
│  ┌─────────────────┐    ┌─────────────────┐               │
│  │  JS Context     │◄──►│  WASM Context   │               │
│  │                 │    │                 │               │
│  │  - Global Obj   │    │  - WAMR Runtime │               │
│  │  - Modules      │    │  - WASM Modules │               │
│  │  - Functions    │    │  - Memory       │               │
│  └─────────────────┘    └─────────────────┘               │
├─────────────────────────────────────────────────────────────┤
│                    WASM-JS Bridge                          │
│  ┌─────────────────────────────────────────────────────────┤
│  │  - Type conversion (JS ↔ WASM)                         │
│  │  - Function call forwarding                            │
│  │  - Memory sharing and management                       │
│  │  - Error handling and exception propagation            │
│  └─────────────────────────────────────────────────────────┤
├─────────────────────────────────────────────────────────────┤
│              WAMR WebAssembly Runtime                      │
│  ┌─────────────────────────────────────────────────────────┤
│  │  - Module loading and validation                       │
│  │  - Fast interpreter execution                          │
│  │  - Memory management                                   │
│  │  - Host function bindings                              │
│  └─────────────────────────────────────────────────────────┤
├─────────────────────────────────────────────────────────────┤
│                   System Layer                             │
│  ┌─────────────────────────────────────────────────────────┤
│  │  libuv (async I/O) + Platform APIs                     │
│  └─────────────────────────────────────────────────────────┤
└─────────────────────────────────────────────────────────────┘
```

### Core Components

#### 1. WASM Runtime Manager (`src/wasm/runtime.c`)
- Initialize/destroy WAMR runtime
- Manage global WASM execution environment
- Handle runtime configuration and memory limits

#### 2. WASM Module Loader (`src/wasm/loader.c`)
- Load WASM modules from files or buffers
- Validate WASM bytecode
- Instantiate modules with import resolution

#### 3. WASM-JS Bridge (`src/wasm/bridge.c`)
- Bidirectional function calls between JS and WASM
- Type conversion and marshaling
- Memory sharing mechanisms

#### 4. WASM Standard Library Integration (`src/std/wasm.c`)
- JavaScript `WebAssembly` global object
- `WebAssembly.Module`, `WebAssembly.Instance` constructors
- `WebAssembly.instantiate()`, `WebAssembly.compile()` functions

## Implementation Plan

### Phase 1: Core Integration

#### 1.1 Build System Integration

**Objective**: Add WAMR as a dependency and configure build system

**Tasks**:
- Add WAMR as git submodule: `deps/wamr`
- Update `CMakeLists.txt` with WAMR configuration
- Configure minimal WAMR build (interpreter-only)
- Ensure cross-platform compatibility

**CMake Configuration**:
```cmake
# WAMR Configuration
set(WAMR_BUILD_PLATFORM "linux")
set(WAMR_BUILD_INTERP 1)           # Enable interpreter
set(WAMR_BUILD_AOT 0)              # Disable AOT for now
set(WAMR_BUILD_JIT 0)              # Disable JIT for now
set(WAMR_BUILD_LIBC_BUILTIN 1)    # Use built-in libc subset
set(WAMR_BUILD_LIBC_WASI 0)       # Disable WASI initially
set(WAMR_BUILD_MINI_LOADER 1)     # Enable mini loader for smaller size

add_subdirectory(deps/wamr)
target_link_libraries(jsrt PRIVATE iwasm_static)
```

#### 1.2 WASM Runtime Initialization

**Files to create**:
- `src/wasm/runtime.h` - Public API definitions
- `src/wasm/runtime.c` - WAMR runtime management

**API Design**:
```c
// Runtime lifecycle
int jsrt_wasm_init(void);
void jsrt_wasm_cleanup(void);

// Runtime configuration
typedef struct {
    uint32_t heap_size;      // WASM linear memory heap size
    uint32_t stack_size;     // WASM execution stack size
    uint32_t max_modules;    // Maximum concurrent modules
} jsrt_wasm_config_t;

int jsrt_wasm_configure(const jsrt_wasm_config_t *config);
```

#### 1.3 Basic Module Loading

**Files to create**:
- `src/wasm/loader.h`
- `src/wasm/loader.c`

**API Design**:
```c
// Module management
typedef struct jsrt_wasm_module jsrt_wasm_module_t;
typedef struct jsrt_wasm_instance jsrt_wasm_instance_t;

// Load module from buffer
jsrt_wasm_module_t *jsrt_wasm_load_module(
    const uint8_t *wasm_bytes,
    size_t size,
    char *error_buf,
    size_t error_buf_size
);

// Instantiate module
jsrt_wasm_instance_t *jsrt_wasm_instantiate(
    jsrt_wasm_module_t *module,
    const jsrt_wasm_imports_t *imports,
    char *error_buf,
    size_t error_buf_size
);

// Cleanup
void jsrt_wasm_module_free(jsrt_wasm_module_t *module);
void jsrt_wasm_instance_free(jsrt_wasm_instance_t *instance);
```

### Phase 2: JavaScript API Implementation

#### 2.1 WebAssembly Global Object

**Files to create**:
- `src/std/webassembly.h`
- `src/std/webassembly.c`

**JavaScript API to implement**:
```javascript
// Constructor functions
WebAssembly.Module(bytes)
WebAssembly.Instance(module, importObject)

// Utility functions  
WebAssembly.validate(bytes) -> boolean
WebAssembly.compile(bytes) -> Promise<Module>
WebAssembly.instantiate(bytes, importObject) -> Promise<{module, instance}>
```

**C Implementation Structure**:
```c
// Constructor implementations
static JSValue js_webassembly_module_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
static JSValue js_webassembly_instance_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);

// Static method implementations  
static JSValue js_webassembly_validate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_webassembly_compile(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
static JSValue js_webassembly_instantiate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// Registration function
JSValue jsrt_init_module_webassembly(JSContext *ctx);
```

#### 2.2 Type Conversion Bridge

**Files to create**:
- `src/wasm/bridge.h`
- `src/wasm/bridge.c`

**Key Functions**:
```c
// JS ↔ WASM type conversion
JSValue jsrt_wasm_val_to_js(JSContext *ctx, wasm_val_t *val);
int jsrt_js_to_wasm_val(JSContext *ctx, JSValue js_val, wasm_val_t *wasm_val, wasm_valkind_t kind);

// Function call forwarding
JSValue jsrt_wasm_call_js_function(JSContext *ctx, JSValue func, wasm_val_t *args, size_t nargs);
int jsrt_js_call_wasm_function(jsrt_wasm_instance_t *instance, const char *func_name, JSValue *args, size_t nargs, JSValue *result);
```

#### 2.3 Memory Management

**Memory Sharing Design**:
```c
// WebAssembly.Memory object support
typedef struct {
    JSContext *ctx;
    wasm_memory_t *memory;
    JSValue array_buffer;  // Shared ArrayBuffer
} jsrt_wasm_memory_t;

// Memory operations
jsrt_wasm_memory_t *jsrt_wasm_memory_create(JSContext *ctx, uint32_t initial_pages, uint32_t max_pages);
JSValue jsrt_wasm_memory_get_buffer(jsrt_wasm_memory_t *memory);
int jsrt_wasm_memory_grow(jsrt_wasm_memory_t *memory, uint32_t delta_pages);
```

### Phase 3: Advanced Features

#### 3.1 Import/Export Resolution

**Import Resolution**:
```c
// Host function binding
typedef struct {
    const char *module_name;
    const char *field_name;
    JSValue js_function;
} jsrt_wasm_import_t;

// Import object processing
int jsrt_wasm_resolve_imports(
    JSContext *ctx,
    JSValue import_obj,
    jsrt_wasm_import_t **imports,
    size_t *import_count
);
```

**Export Access**:
```c
// Export enumeration and access
JSValue jsrt_wasm_get_exports(JSContext *ctx, jsrt_wasm_instance_t *instance);
JSValue jsrt_wasm_get_export(jsrt_wasm_instance_t *instance, const char *name);
```

#### 3.2 Error Handling

**Error Propagation**:
```c
// WASM exception → JS Error conversion
JSValue jsrt_wasm_exception_to_js(JSContext *ctx, wasm_trap_t *trap);

// JS Error → WASM trap conversion  
wasm_trap_t *jsrt_js_error_to_wasm_trap(JSContext *ctx, JSValue error);
```

#### 3.3 Async Support

**Promise Integration**:
```c
// Async compilation and instantiation
JSValue jsrt_webassembly_compile_async(JSContext *ctx, const uint8_t *bytes, size_t size);
JSValue jsrt_webassembly_instantiate_async(JSContext *ctx, const uint8_t *bytes, size_t size, JSValue imports);
```

## File Structure

```
src/
├── wasm/
│   ├── runtime.h          # WASM runtime management API
│   ├── runtime.c          # WAMR initialization and configuration
│   ├── loader.h           # Module loading API
│   ├── loader.c           # WASM module loading and instantiation
│   ├── bridge.h           # JS-WASM interop API
│   ├── bridge.c           # Type conversion and call forwarding
│   ├── memory.h           # Memory management API
│   └── memory.c           # Shared memory implementation
├── std/
│   ├── webassembly.h      # WebAssembly global object API
│   └── webassembly.c      # WebAssembly JavaScript API implementation
└── ...

deps/
└── wamr/                  # WAMR git submodule

test/
├── test_wasm_basic.js     # Basic WASM loading tests
├── test_wasm_interop.js   # JS-WASM interop tests
├── test_wasm_memory.js    # Memory sharing tests
└── ...

examples/
├── wasm/
│   ├── hello.wat          # Simple hello world WASM
│   ├── math.wat           # Math operations example
│   └── fibonacci.wat      # Recursive function example
└── ...
```

## Configuration and Build Options

### Build Targets

```makefile
# New build targets for WASM support
.PHONY: wasm-examples
wasm-examples:
	@echo "Building WebAssembly examples..."
	wat2wasm examples/wasm/hello.wat -o examples/wasm/hello.wasm
	wat2wasm examples/wasm/math.wat -o examples/wasm/math.wasm

.PHONY: test-wasm  
test-wasm: jsrt_g
	@echo "Running WebAssembly tests..."
	./target/debug/jsrt_g test/test_wasm_basic.js
	./target/debug/jsrt_g test/test_wasm_interop.js
```

### Runtime Configuration

```c
// Default configuration
#define JSRT_WASM_DEFAULT_HEAP_SIZE    (1 * 1024 * 1024)  // 1MB
#define JSRT_WASM_DEFAULT_STACK_SIZE   (64 * 1024)        // 64KB  
#define JSRT_WASM_DEFAULT_MAX_MODULES  16

// Configurable limits
#define JSRT_WASM_MIN_HEAP_SIZE        (64 * 1024)        // 64KB
#define JSRT_WASM_MAX_HEAP_SIZE        (16 * 1024 * 1024) // 16MB
#define JSRT_WASM_MIN_STACK_SIZE       (16 * 1024)        // 16KB
#define JSRT_WASM_MAX_STACK_SIZE       (256 * 1024)       // 256KB
```

## Testing Strategy

### Unit Tests

1. **WASM Runtime Tests**
   - Initialize/cleanup lifecycle
   - Configuration validation
   - Error handling

2. **Module Loading Tests**
   - Valid/invalid bytecode handling
   - Import resolution
   - Export enumeration

3. **Type Conversion Tests**
   - All WASM types ↔ JS values
   - Edge cases and error conditions
   - Memory efficiency

4. **Interop Tests**
   - JS calling WASM functions
   - WASM calling JS functions  
   - Nested calls and recursion

### Integration Tests

1. **End-to-End Scenarios**
   - Complete module loading and execution
   - Complex import/export scenarios
   - Memory sharing between JS and WASM

2. **Performance Tests**
   - Benchmark WASM vs JS execution
   - Memory usage comparison
   - Startup time measurement

### Example Test Structure

```javascript
// test/test_wasm_basic.js
const assert = require("jsrt:assert");

// Test basic module validation
const validWasm = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
assert.ok(WebAssembly.validate(validWasm), "Valid WASM should pass validation");

// Test module compilation
const module = new WebAssembly.Module(validWasm);
assert.ok(module instanceof WebAssembly.Module, "Should create Module instance");

// Test instantiation
const instance = new WebAssembly.Instance(module, {});
assert.ok(instance instanceof WebAssembly.Instance, "Should create Instance");

console.log("✅ Basic WASM tests passed");
```

## Performance Considerations

### Memory Management
- Minimize JS-WASM boundary crossings
- Implement efficient type conversion
- Pool and reuse WASM instances where possible
- Use WAMR's memory management optimizations

### Execution Optimization
- Start with interpreter mode for simplicity
- Consider AOT compilation for frequently-used modules
- Implement module caching for repeated loads
- Profile and optimize hot paths in bridge code

### Resource Limits
- Enforce memory and execution time limits
- Prevent resource exhaustion attacks
- Implement graceful degradation for resource constraints

## Security Considerations

### Sandboxing
- WASM execution is inherently sandboxed
- Limit host function access to safe operations
- Validate all import/export bindings
- Prevent access to sensitive system resources

### Memory Safety
- Use WAMR's built-in memory protection
- Validate array buffer access patterns
- Prevent buffer overflow in bridge code
- Implement bounds checking for shared memory

## Future Enhancements

### Phase 4: Advanced Features (Future)
- **WASI Support**: File system, networking access
- **Threading**: Web Workers with SharedArrayBuffer
- **Streaming**: WebAssembly.instantiateStreaming()
- **Debugging**: DWARF debug info support
- **JIT/AOT**: Just-in-time and ahead-of-time compilation

### Performance Optimizations
- **Function Caching**: Cache compiled functions
- **Import Optimization**: Fast path for common imports
- **Memory Pooling**: Reuse memory allocations
- **Batch Operations**: Minimize JS-WASM transitions

## Conclusion

This design provides a solid foundation for WebAssembly integration in jsrt, focusing on:

- **Minimal Footprint**: Using WAMR's lightweight interpreter
- **Standards Compliance**: Implementing WebAssembly JavaScript API
- **Performance**: Efficient JS-WASM interoperability  
- **Maintainability**: Clean architecture with well-defined interfaces
- **Extensibility**: Modular design for future enhancements

The phased implementation approach allows for incremental development and testing, ensuring stability at each milestone while building toward full WebAssembly support.