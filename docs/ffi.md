# FFI (Foreign Function Interface) Module

The FFI module provides the ability to call functions from native dynamic libraries (.dll on Windows, .so on Linux, .dylib on macOS) directly from JavaScript code.

## Import

```javascript
// CommonJS
const ffi = require('std:ffi');

// ES Modules
import ffi from 'std:ffi';
```

## API Reference

### `ffi.Library(libraryName, functionDefinitions)`

Loads a dynamic library and creates JavaScript bindings for specified functions.

**Parameters:**
- `libraryName` (string): Name or path to the dynamic library
- `functionDefinitions` (object): Object mapping function names to their signatures

**Returns:** Object with methods corresponding to the loaded functions

**Function Signature Format:**
```javascript
{
  'function_name': [return_type, [arg_type1, arg_type2, ...]]
}
```

**Example:**
```javascript
const ffi = require('std:ffi');

// Load libc with basic functions
const libc = ffi.Library('libc.so.6', {
  'strlen': ['int', ['string']],
  'strcmp': ['int', ['string', 'string']],
  'printf': ['int', ['string']]
});

// Functions are now available as methods
// console.log(libc.strlen('hello')); // Would return 5 if fully implemented
```

## Supported Data Types

### Return Types and Argument Types

| Type | Description | C Equivalent |
|------|-------------|--------------|
| `void` | No return value | `void` |
| `int` | Signed integer | `int` |
| `uint` | Unsigned integer | `unsigned int` |
| `int32` | 32-bit signed integer | `int32_t` |
| `uint32` | 32-bit unsigned integer | `uint32_t` |
| `int64` | 64-bit signed integer | `int64_t` |
| `uint64` | 64-bit unsigned integer | `uint64_t` |
| `float` | Single-precision floating point | `float` |
| `double` | Double-precision floating point | `double` |
| `string` | Null-terminated string | `char*` |
| `pointer` | Generic pointer | `void*` |

## Cross-Platform Support

The FFI module automatically handles platform-specific differences:

### Library Loading
- **Windows**: Uses `LoadLibraryA()`, `GetProcAddress()`, `FreeLibrary()`
- **Unix/Linux/macOS**: Uses `dlopen()`, `dlsym()`, `dlclose()`

### Library Naming Conventions
- **Windows**: `.dll` files (e.g., `kernel32.dll`, `user32.dll`)
- **Linux**: `.so` files (e.g., `libc.so.6`, `libm.so.6`)
- **macOS**: `.dylib` files (e.g., `libSystem.dylib`)

## Examples

### Basic Usage

```javascript
const ffi = require('std:ffi');

// Load system math library
const libm = ffi.Library('libm.so.6', {
  'cos': ['double', ['double']],
  'sin': ['double', ['double']],
  'sqrt': ['double', ['double']]
});

// Note: Actual function calls are not fully implemented in this version
console.log('Math functions loaded:', Object.keys(libm));
```

### Windows Example

```javascript
const ffi = require('std:ffi');

// Load Windows kernel32.dll
const kernel32 = ffi.Library('kernel32.dll', {
  'GetCurrentProcessId': ['uint32', []],
  'GetTickCount': ['uint32', []]
});

console.log('Windows functions loaded:', Object.keys(kernel32));
```

### Error Handling

```javascript
const ffi = require('std:ffi');

try {
  const lib = ffi.Library('nonexistent.so', {
    'dummy': ['void', []]
  });
} catch (error) {
  console.log('Failed to load library:', error.message);
  // Error: Failed to load library 'nonexistent.so'
}
```

## Limitations and Current Status

### ⚠️ Important Notice

This is a **proof-of-concept** implementation with the following limitations:

1. **Function Calls Not Fully Implemented**: While the module can load libraries and create function bindings, actually calling the functions may cause crashes or unpredictable behavior.

2. **No libffi Integration**: A production FFI implementation would use libffi for proper calling convention handling. This implementation uses a simplified approach that doesn't handle all calling conventions correctly.

3. **Limited Type Support**: Complex types like structs, unions, and function pointers are not supported.

4. **Memory Management**: No automatic memory management for allocated memory from native functions.

### What Works

- ✅ Loading dynamic libraries across platforms
- ✅ Function symbol resolution
- ✅ Basic type definitions
- ✅ Error handling for missing libraries/functions
- ✅ Both CommonJS (`require`) and ES Module (`import`) support

### What Doesn't Work

- ❌ Actual function calls (may crash)
- ❌ Complex parameter types
- ❌ Callback functions
- ❌ Memory management for native allocations
- ❌ Struct/union support

## Architecture

### Class Structure

The FFI module uses two main classes:

1. **FFILibrary**: Represents a loaded dynamic library
2. **FFIFunction**: Represents a function within a library with its signature

### Memory Management

- Libraries are automatically closed when the JavaScript object is garbage collected
- Function signatures and metadata are properly freed
- Cross-platform dynamic loading handles are managed appropriately

## Future Enhancements

To make this a production-ready FFI implementation:

1. **Integrate libffi**: Use libffi for proper cross-platform function calling
2. **Add callback support**: Allow JavaScript functions to be called from native code
3. **Struct/union support**: Define and use complex data types
4. **Memory management helpers**: Provide utilities for manual memory management
5. **Async function support**: Support for asynchronous native function calls
6. **Better error messages**: More detailed error reporting with stack traces

## Security Considerations

- Loading arbitrary dynamic libraries can pose security risks
- Functions have direct access to system resources
- No sandboxing is provided - use with trusted libraries only
- Memory corruption is possible with incorrect type definitions

## Platform-Specific Notes

### Linux
- Standard libraries are typically in `/lib`, `/usr/lib`, or `/usr/local/lib`
- Use `ldd` command to find library dependencies
- Common libraries: `libc.so.6`, `libm.so.6`, `libpthread.so.0`

### macOS
- System libraries are in `/usr/lib` and `/System/Library/Frameworks`
- Homebrew libraries are in `/opt/homebrew/lib` or `/usr/local/lib`
- Common libraries: `libSystem.dylib`, `libc.dylib`

### Windows
- System libraries are in `System32` folder
- Common libraries: `kernel32.dll`, `user32.dll`, `msvcrt.dll`
- Use dependency walker or similar tools to analyze library dependencies

## Version

Current version: **1.0.0** (Proof of Concept)