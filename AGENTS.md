# jsrt Development Guidelines

## Project Overview

**jsrt** is a lightweight JavaScript runtime written in C, designed for minimal resource footprint while providing essential JavaScript execution capabilities.

### Core Technologies

- **QuickJS**: JavaScript engine for parsing and executing JS code
- **libuv**: Asynchronous I/O operations and event loop management
- **Custom Stdlib**: Minimal standard library implementations

## Quick Start

```bash
# Clone with submodules
git clone --recursive <repo_url>

# Build and test
make            # Build release version
make test       # Run unit tests (MANDATORY before commit)
make wpt        # Run Web Platform Tests (MANDATORY before commit)

# Development builds
make jsrt_g     # Debug build with symbols
make jsrt_m     # Debug with AddressSanitizer
make format     # Format all code (MANDATORY before commit)
```

## AI Assistant Core Rules

### Three Core Principles

1. **Understanding First**: Read code > Guess interfaces | Confirm > Assume
2. **Quality Assurance**: Test > Commit | Reuse > Innovate
3. **Honest Collaboration**: Admit ignorance > Pretend knowledge | Human decisions > AI autonomy

### Critical Development Rules

- **MANDATORY**: Run baseline tests before modifications
- **MANDATORY**: Make minimal, targeted changes only
- **MANDATORY**: Test immediately after each change
- **MANDATORY**: Report exact numbers (e.g., "653 failures" not "many")
- **NEVER**: Modify code without understanding its purpose
- **NEVER**: Claim improvement without concrete evidence

### Verification Checklist

Must complete after every modification:

```bash
□ make format    # Code formatting
□ make test      # Unit tests (100% pass rate)
□ make wpt       # WPT tests (failures ≤ baseline)
```

## Agent System

Project uses specialized agents for different tasks. See `.claude/AGENTS_SUMMARY.md` for complete guide.

| Agent                   | Purpose              | Key Commands                           |
| ----------------------- | -------------------- | -------------------------------------- |
| **jsrt-developer**      | Feature development  | `make format && make test && make wpt` |
| **jsrt-tester**         | Testing & debugging  | `make test`, `make wpt`, ASAN analysis |
| **jsrt-compliance**     | Standards compliance | WPT/WinterCG validation                |
| **jsrt-code-reviewer**  | Code quality         | Security, memory, standards review     |
| **jsrt-cross-platform** | Platform support     | Windows/Linux/macOS compatibility      |
| **jsrt-formatter**      | Code formatting      | `make format` enforcement              |

## Project Structure

```
jsrt/
├── src/                    # Core runtime source
│   ├── main.c             # Entry point
│   ├── jsrt.c/h           # Core runtime & API
│   ├── runtime.c/h        # Runtime environment setup
│   ├── repl.c/h           # REPL implementation
│   ├── build.c/h          # Build utilities
│   ├── module/            # Module loading system (NEW)
│   │   ├── core/          # Core infrastructure (cache, context, errors, debug)
│   │   ├── resolver/      # Path resolution (Node.js-compatible)
│   │   ├── detector/      # Format detection (CommonJS/ESM/JSON)
│   │   ├── protocols/     # Protocol handlers (file://, http://, https://)
│   │   ├── loaders/       # Module loaders (CJS/ESM/JSON/builtin)
│   │   └── module_loader.c # Main dispatcher
│   ├── node/              # Node.js-compatible modules (fs, http, net, dns, etc.)
│   ├── std/               # Standard library modules (console, timers, etc.)
│   ├── util/              # Utility functions (debug.h, etc.)
│   ├── crypto/            # Cryptographic functions
│   ├── http/              # HTTP implementation
│   ├── url/               # URL parsing
│   └── wasm/              # WebAssembly support
├── test/                   # Test suite (test_*.js)
├── wpt/                    # Web Platform Tests
├── deps/                   # Dependencies (DO NOT MODIFY)
├── examples/              # Example JavaScript files
├── docs/                   # Project documentation
├── scripts/               # Build and utility scripts
├── bin/                    # Binary executables (symlinks to target/)
│   ├── jsrt -> ../target/release/jsrt      # Release build
│   ├── jsrt_g -> ../target/debug/jsrt      # Debug build
│   ├── jsrt_m -> ../target/asan/jsrt       # AddressSanitizer build
│   ├── jsrt_s -> ../target/release/jsrt_s  # Static release
│   └── jsrt_static -> ../target/static/jsrt # Static build
├── build/                  # CMake build directory
├── .claude/               # AI assistant configuration
│   ├── agents/            # Specialized agent definitions
│   ├── docs/              # Detailed documentation
│   └── AGENTS_SUMMARY.md  # Agent usage guide
└── target/                # Build outputs
    ├── release/           # Release builds
    ├── debug/             # Debug builds
    ├── asan/              # AddressSanitizer builds
    ├── static/            # Static builds
    └── tmp/               # Temporary test files (git ignored)
```

## Essential Commands

### Build Commands

```bash
make            # Release build (optimized)
make jsrt_g     # Debug build with symbols
make jsrt_m     # AddressSanitizer build
make clean      # Clean build artifacts
make format     # Format C/JS code (MANDATORY)
```

### Test Commands

```bash
make test                          # Unit tests (MUST PASS)
make test N=dir                    # Test specific directory (test/dir)
                                   # Useful for focused testing on a specific module
                                   # Avoids interference from other issues and improves efficiency
make wpt                           # All WPT tests (MUST PASS)
make wpt N=url                     # Specific WPT category
SHOW_ALL_FAILURES=1 make wpt N=url # Debug mode
MAX_FAILURES=10 make wpt           # Limit failures shown

# Single file testing with timeout
timeout 20 ./bin/jsrt test/file.js # Test individual file with 20s timeout
                                   # Prevents hanging when code has infinite loops or deadlocks
```

### Debug Commands

```bash
# Memory debugging
./bin/jsrt_m script.js                      # Run with AddressSanitizer
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m script.js  # Enable leak detection

# Debug logging with JSRT_Debug macro
make jsrt_g                 # Build debug version with DEBUG flag enabled
./bin/jsrt_g script.js      # Run with debug logging output
# JSRT_Debug prints to stderr with green color, showing file:line info

# WPT debugging
mkdir -p target/tmp
SHOW_ALL_FAILURES=1 make wpt N=console > target/tmp/debug.log 2>&1
```

## File Organization Rules

| File Type           | Location      | Purpose                   |
| ------------------- | ------------- | ------------------------- |
| **Permanent tests** | `test/`       | Committed test cases      |
| **Temporary tests** | `target/tmp/` | Debug files (git ignored) |
| **Examples**        | `examples/`   | Demo scripts              |
| **Modules**         | `src/std/`    | Standard library          |

### Code Organization Guidelines

- **Test files**: Keep test output minimal - only print on failures or warnings
- **Source files**: If a single file exceeds 500 lines, refactor into multiple files within a subdirectory

## Development Workflow

```bash
# 1. Before starting
git submodule update --init --recursive
make jsrt_g  # Build debug version

# 2. Development cycle
# Edit code...
make format     # Format code
make test       # Run tests
make wpt        # Check compliance

# 3. Before commit (ALL MANDATORY)
make format
make test
make wpt
make clean && make  # Verify release build
```

## Memory Management Rules

- Every `malloc` needs corresponding `free`
- Use QuickJS functions: `js_malloc`, `js_free`
- Handle JS values: `JS_FreeValue` for temporaries
- Check all allocation returns

## Error Handling Pattern

```c
JSValue result = some_operation(ctx);
if (JS_IsException(result)) {
    JS_FreeValue(ctx, result);
    return JS_EXCEPTION;
}
// Use result...
JS_FreeValue(ctx, result);
```

## Debug Logging

Use the `JSRT_Debug` macro for adding debug logging to the codebase:

```c
#include "util/debug.h"

// Add debug logging in your code
JSRT_Debug("Variable value: %d", some_value);
JSRT_Debug("Processing request from %s:%d", host, port);
```

**How it works:**

- `JSRT_Debug` is only active when building with `make jsrt_g` (DEBUG flag enabled)
- Outputs to stderr with green color formatting
- Automatically includes file name and line number
- In release builds, `JSRT_Debug` calls are compiled out (no overhead)

**Usage:**

```bash
make jsrt_g                 # Build with debug logging enabled
./bin/jsrt_g script.js      # Run and see debug output
```

## Module System

jsrt features a unified, extensible module loading system that supports multiple formats and protocols.

### Features

- **Multiple Formats**: CommonJS, ES Modules (ESM), JSON, and builtin modules
- **Protocol Support**: file://, http://, https:// (extensible for custom protocols)
- **Node.js Compatibility**: Full Node.js module resolution algorithm
- **Smart Detection**: Three-stage format detection (extension → package.json → content)
- **Performance**: FNV-1a hash-based caching with high hit rates (85-95%)
- **Safety**: Comprehensive error handling (60+ error codes), memory safety validated

### Architecture

```
QuickJS Module System
    │
    │ JS_SetModuleLoaderFunc(normalize_callback, loader_callback)
    ▼
ES Module Bridge (loaders/esm_loader.c)
    │
    ├─► jsrt_esm_normalize_callback()
    │   - Compact node mode handling (bare → node:*)
    │   - Delegates to path resolver
    │   - Returns resolved absolute path
    │
    └─► jsrt_esm_loader_callback()
        - Handles builtin modules (jsrt:*, node:*)
        - Handles HTTP/HTTPS modules
        - Delegates to new module loader
        │
        ▼
Module Loader Core (module_loader.c)
    │
    ├─► Cache System (core/module_cache.c)
    │   - FNV-1a hash-based cache
    │   - Configurable capacity (default: 128)
    │   - Hit/miss/collision tracking
    │
    ├─► Path Resolver (resolver/)
    │   - Node.js-compatible resolution
    │   - Relative/absolute/bare specifiers
    │   - node_modules traversal
    │
    ├─► Format Detector (detector/)
    │   - Extension analysis (.cjs/.mjs/.js)
    │   - package.json type field
    │   - Content analysis (import/export)
    │
    ├─► Protocol Handlers (protocols/)
    │   - file:// (local filesystem)
    │   - http://, https:// (remote)
    │   - Extensible registry
    │
    └─► Module Loaders (loaders/)
        - CommonJS loader (require, module.exports)
        - ESM loader (import/export)
        - JSON loader
        - Builtin loader (jsrt:, node: prefixes)
```

### Module Loading Pipeline

1. **Cache Check**: Fast path for previously loaded modules
2. **Path Resolution**: Resolve specifier to absolute path
3. **Format Detection**: Determine CommonJS, ESM, or JSON
4. **Protocol Handling**: Load source via file://, http://, etc.
5. **Module Compilation**: Format-specific compilation
6. **Cache Update**: Store for future loads

### Usage in JavaScript

```javascript
// CommonJS
const fs = require('fs');
const myModule = require('./my-module');
const lodash = require('lodash');

// ES Modules
import fs from 'fs';
import { readFile } from 'fs';
import myModule from './my-module.mjs';

// Protocol-based (NEW)
const config = require('http://example.com/config.js');
import data from 'https://cdn.example.com/data.json';

// Builtin modules
const console = require('jsrt:console');
const http = require('node:http');
```

### Usage in C (Embedders)

```c
// Module loader is automatically created by runtime
JSRT_Runtime* rt = JSRT_RuntimeNew();

// Access module loader
JSRT_ModuleLoader* loader = rt->module_loader;

// Load a module programmatically
JSValue module = jsrt_load_module(loader, "./my-module.js", base_path);

// Register custom protocol handler
jsrt_register_protocol_handler("myproto", my_handler, userdata);

// Cleanup is automatic on runtime destruction
JSRT_RuntimeFree(rt);
```

### Module System Documentation

For detailed information:

- **Architecture**: `docs/module-system-architecture.md` - System design, data flow, extension points
- **API Reference**: `docs/module-system-api.md` - Complete API documentation with examples
- **Migration Guide**: `docs/module-system-migration.md` - Migration from legacy system
- **Validation Report**: `docs/plan/module-loader-phase7-validation.md` - Testing and validation

### Testing Module System

```bash
# Run all module tests
make test N=module

# Run specific module test category
make test N=module/core          # Core infrastructure
make test N=module/resolver      # Path resolution
make test N=module/detector      # Format detection
make test N=module/protocols     # Protocol handlers

# Memory safety validation
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/module/*.js
```

### Key Implementation Files

| Component | Location | Purpose |
|-----------|----------|---------|
| **Main dispatcher** | `src/module/module_loader.c` | Coordinates all subsystems |
| **Cache** | `src/module/core/module_cache.c` | FNV-1a hash-based cache |
| **Resolver** | `src/module/resolver/` | Node.js path resolution |
| **Detector** | `src/module/detector/` | Format detection logic |
| **Protocols** | `src/module/protocols/` | Protocol handler registry |
| **Loaders** | `src/module/loaders/` | Format-specific loaders |
| **Runtime integration** | `src/runtime.c` | Lifecycle management |

## WebAssembly Support

jsrt implements the WebAssembly JavaScript API using WAMR (WebAssembly Micro Runtime) v2.4.1 as the backend.

### Configuration

WAMR is configured for lightweight execution with the following features:

- **Runtime API**: Uses WAMR Runtime API for execution
- **Module validation**: Support for `WebAssembly.validate()`
- **Interpreter mode**: Fast startup, minimal memory footprint
- **AOT disabled**: Ahead-of-time compilation not enabled
- **Memory**: 4MB stack, 16MB heap per module
- **WASI**: System calls disabled (standalone JavaScript environment)

### Implemented APIs

✅ **Fully Functional**:
- `WebAssembly.validate(bytes)` - Bytecode validation
- `WebAssembly.Module` constructor - Synchronous compilation from bytecode
- `WebAssembly.Module.exports(module)` - Inspect module exports
- `WebAssembly.Module.imports(module)` - Inspect module imports
- `WebAssembly.Module.customSections(module, name)` - Access custom sections
- `WebAssembly.Instance` constructor - Instantiation with imports
- `instance.exports` - Access exported functions/memories/tables/globals
- `WebAssembly.CompileError` - Invalid bytecode errors
- `WebAssembly.LinkError` - Instantiation/linking errors
- `WebAssembly.RuntimeError` - Execution errors

⚠️ **Limitations**:
- **Memory/Table/Global APIs**: WAMR C API limitations prevent standalone object creation (blocked: Phase 2.4-2.6, Phase 4)
- **Async APIs**: `compile()` and `instantiate()` async variants not implemented (Phase 5)
- **Streaming APIs**: `compileStreaming()` and `instantiateStreaming()` not implemented (Phase 6)
- **Validation**: WAMR may accept some invalid modules that should fail per spec

### Testing

```bash
# Run all WebAssembly unit tests
make test N=jsrt/test_jsrt_wasm
make test N=web/webassembly

# Run WebAssembly WPT tests
make wpt N=wasm

# Memory safety validation with ASAN
make jsrt_m
ASAN_OPTIONS=detect_leaks=1 ./bin/jsrt_m test/jsrt/test_jsrt_wasm_*.js

# Test individual example
./bin/jsrt examples/wasm/hello.js
./bin/jsrt examples/wasm/exports.js
./bin/jsrt examples/wasm/imports.js
./bin/jsrt examples/wasm/errors.js
```

### Usage Examples

See `examples/wasm/` directory for comprehensive examples:

```javascript
// Basic validation and module creation
const bytes = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
const isValid = WebAssembly.validate(bytes);  // true
const module = new WebAssembly.Module(bytes);
const instance = new WebAssembly.Instance(module);

// Inspect module structure
const exports = WebAssembly.Module.exports(module);
const imports = WebAssembly.Module.imports(module);

// Error handling
try {
  new WebAssembly.Module(invalidBytes);
} catch (e) {
  console.log(e instanceof WebAssembly.CompileError); // true
}
```

### Key Implementation Files

| Component | Location | Purpose |
|-----------|----------|---------|
| **JavaScript API** | `src/std/webassembly.c` | WebAssembly namespace and constructors |
| **WAMR integration** | `src/wasm/runtime.c` | WAMR runtime initialization and management |
| **Module system** | `src/wasm/module.c` | Module compilation and caching |
| **Instance system** | `src/wasm/instance.c` | Instance creation and imports |
| **Import resolution** | `src/wasm/import.c` | JavaScript ↔ WASM function binding |
| **Error types** | `src/std/webassembly.c` | CompileError/LinkError/RuntimeError |

### Documentation

- **API Compatibility Matrix**: `docs/webassembly-api-compatibility.md` - Complete API status
- **Implementation Plan**: `docs/plan/webassembly-plan.md` - Full 8-phase roadmap
- **Examples**: `examples/wasm/README.md` - Detailed usage examples
- **Test Suite**: `test/jsrt/test_jsrt_wasm_*.js`, `test/web/webassembly/`

## Documentation References

Detailed documentation in `.claude/docs/`:

- `platform.md` - Cross-platform development
- `testing.md` - Complete testing guide
- `wpt.md` - Web Platform Tests guide
- `architecture.md` - System architecture
- `modules.md` - Module development
- `debugging.md` - Debugging techniques
- `contributing.md` - Contribution guidelines

## Quick Reference Links

- **Agent Guide**: `.claude/AGENTS_SUMMARY.md`
- **Agent Definitions**: `.claude/agents/`
- **Detailed Docs**: `.claude/docs/`
- **Local Settings**: `.claude/settings.local.json`

---

**Remember**: Always use appropriate agents for specific tasks. When in doubt, consult `.claude/AGENTS_SUMMARY.md` for agent selection guidance.
