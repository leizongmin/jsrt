# Dependencies

## External Dependencies

### Already Present

- **WAMR** (deps/wamr/)
  - WebAssembly Micro Runtime
  - Provides WASM execution engine
  - Includes WASI preview1 support
  - Used via: wasm_export.h API

- **QuickJS** (embedded)
  - JavaScript engine
  - Memory management primitives
  - Class system for native objects
  - Used via: quickjs.h API

- **libuv** (system library)
  - Async I/O operations
  - Event loop management
  - File system operations
  - Used via: uv.h API

### Required for Testing

- **WASI SDK**
  - Compiles C/C++ to wasm32-wasi
  - Needed for creating test WASM modules
  - Download from: https://github.com/WebAssembly/wasi-sdk/releases
  - Installation: Extract to /opt/wasi-sdk or similar

## Internal Dependencies

### Core Runtime

- **src/std/webassembly.c**
  - Existing WebAssembly support
  - WebAssembly.Module, WebAssembly.Instance classes
  - Integration point for WASI import objects

- **src/wasm/runtime.c**
  - WAMR initialization
  - Runtime configuration
  - Memory management

### Module System

- **src/module/loaders/builtin_loader.c**
  - Builtin module loading
  - Protocol handling (node:, jsrt:)
  - Module cache integration

- **src/node/node_modules.c**
  - Node.js module registry
  - Module initialization
  - Export handling

### Utilities

- **src/util/debug.h**
  - JSRT_Debug macro for logging
  - Debug build support

## Dependency Graph

```
WASI Module (src/node/wasi/)
├── Runtime (src/runtime.c)
│   ├── QuickJS (quickjs.h)
│   └── WAMR (wasm_export.h)
├── Module System
│   ├── builtin_loader.c
│   ├── node_modules.c
│   └── module_cache.c
├── WebAssembly Integration
│   ├── src/std/webassembly.c
│   └── src/wasm/runtime.c
└── System APIs
    ├── libuv (file I/O)
    └── POSIX (stdio, file descriptors)
```

## Build Dependencies

Required for compilation:

- **CMake** >= 3.10
- **C compiler** (GCC or Clang)
- **Make** or Ninja
- **pkg-config**

## Development Dependencies

Required for development workflow:

- **clang-format** (code formatting)
- **AddressSanitizer** (memory safety)
- **Python 3** (test runner scripts)
- **WASI SDK** (test WASM compilation)

## Version Requirements

| Dependency | Minimum Version | Notes |
|------------|----------------|-------|
| CMake | 3.10 | Build system |
| GCC | 7.0 | C11 support |
| Clang | 6.0 | Alternative to GCC |
| WASI SDK | 20.0 | For test WASM modules |
| Python | 3.6 | WPT runner |

## Optional Dependencies

For enhanced testing:

- **Rust toolchain** (wasm32-wasi target for Rust tests)
- **AssemblyScript** (WASI test modules)
- **wabt** (WebAssembly Binary Toolkit for debugging)
