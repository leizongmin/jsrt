# WebAssembly Examples for jsrt

This directory contains comprehensive examples demonstrating jsrt's WebAssembly JavaScript API implementation.

## Examples

### 1. `hello.js` - Basic Module and Instance
**Topics**: Module creation, validation, instantiation, error handling

```bash
./bin/jsrt examples/wasm/hello.js
```

Demonstrates:
- Creating minimal valid WASM bytecode
- Using `WebAssembly.validate()`
- Creating `Module` and `Instance` objects
- Basic error handling for invalid bytecode

### 2. `exports.js` - Exported Functions
**Topics**: Function exports, calling WASM from JS, type conversion

```bash
./bin/jsrt examples/wasm/exports.js
```

Demonstrates:
- Creating modules with exported functions
- Using `Module.exports()` to inspect exports
- Calling exported functions from JavaScript
- Type coercion between JS and WASM (i32)

### 3. `imports.js` - JavaScript Imports
**Topics**: Import objects, calling JS from WASM, Module.imports()

```bash
./bin/jsrt examples/wasm/imports.js
```

Demonstrates:
- Creating import objects with JavaScript functions
- WASM modules importing and calling JS functions
- Using `Module.imports()` and `Module.exports()`
- LinkError when imports are missing

### 4. `errors.js` - Error Handling
**Topics**: CompileError, LinkError, RuntimeError, error types

```bash
./bin/jsrt examples/wasm/errors.js
```

Demonstrates:
- `WebAssembly.CompileError` - invalid bytecode
- `WebAssembly.LinkError` - instantiation failures
- `WebAssembly.RuntimeError` - execution errors
- `TypeError` - invalid arguments
- Error inheritance and constructors

## Running All Examples

```bash
# Run all examples in sequence
for file in examples/wasm/*.js; do
  [ -f "$file" ] || continue
  echo "=== Running $file ==="
  ./bin/jsrt "$file"
  echo ""
done
```

Or run individually:
```bash
./bin/jsrt examples/wasm/hello.js
./bin/jsrt examples/wasm/exports.js
./bin/jsrt examples/wasm/imports.js
./bin/jsrt examples/wasm/errors.js
```

## Implementation Status

jsrt currently implements the following WebAssembly JavaScript APIs:

✅ **Fully Implemented**:
- `WebAssembly.validate(bytes)` - bytecode validation
- `WebAssembly.Module` constructor - synchronous compilation
- `WebAssembly.Module.exports/imports/customSections()` - static methods
- `WebAssembly.Instance` constructor - synchronous instantiation
- `WebAssembly.CompileError/LinkError/RuntimeError` - error types
- Import/export functionality for functions

⚠️ **Not Yet Implemented**:
- `WebAssembly.compile/instantiate()` - async APIs (Phase 5)
- `WebAssembly.compileStreaming/instantiateStreaming()` - streaming APIs (Phase 6)
- Memory, Table, Global constructors (WAMR C API limitations)
- Some advanced validation edge cases may differ from spec

See `docs/webassembly-api-compatibility.md` for complete API status.

## WASM Bytecode Reference

All examples use hand-crafted WASM bytecode for clarity. Key sections:

- **Magic**: `0x00 0x61 0x73 0x6d` ("\0asm")
- **Version**: `0x01 0x00 0x00 0x00` (version 1)
- **Type Section**: Function signatures
- **Import Section**: Imported functions/memories/tables/globals
- **Function Section**: Maps functions to types
- **Export Section**: Exported functions/memories/tables/globals
- **Code Section**: Function implementations

## Resources

- [WebAssembly JavaScript API Spec](https://webassembly.github.io/spec/js-api/)
- [MDN WebAssembly Documentation](https://developer.mozilla.org/en-US/docs/WebAssembly)
- [WASM Binary Format](https://webassembly.github.io/spec/core/binary/)
- jsrt API Compatibility: `docs/webassembly-api-compatibility.md`
