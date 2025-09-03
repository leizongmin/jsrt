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

### ✅ Recent Major Improvements (v2.0.0)

This FFI implementation has been significantly enhanced with the following improvements:

#### 🚀 Enhanced Function Calling
- **16 Arguments Support**: Expanded from 4 to 16 function arguments
- **Enhanced Type System**: Full support for `int`, `int32`, `int64`, `float`, `double`, `string`, `pointer`, `void`
- **Better Argument Parsing**: Supports JavaScript numbers, strings, booleans, null/undefined

#### 🛠️ Memory Management API
- **`ffi.malloc(size)`**: Allocate memory dynamically
- **`ffi.free(ptr)`**: Free allocated memory  
- **`ffi.memcpy(dest, src, size)`**: Copy memory between pointers
- **`ffi.readString(ptr, [maxLength])`**: Read null-terminated string from pointer
- **`ffi.writeString(ptr, str)`**: Write string to memory location

#### 📚 Developer Experience
- **Type Constants**: `ffi.types` object with predefined type strings
- **Enhanced Error Handling**: Better validation and error messages
- **Safety Limits**: Reasonable limits on memory operations for security

### 📖 Enhanced Usage Examples

```javascript
const ffi = require('std:ffi');

// Using type constants for cleaner code
const libc = ffi.Library('libc.so.6', {
  'strlen': [ffi.types.int, [ffi.types.string]],
  'sqrt': [ffi.types.double, [ffi.types.double]],
  'malloc': [ffi.types.pointer, [ffi.types.int]],
  'free': [ffi.types.void, [ffi.types.pointer]]
});

// Enhanced function calls with more types
const length = libc.strlen('hello');           // int return
const result = libc.sqrt(16.0);               // double return  

// Memory management
const ptr = ffi.malloc(256);                  // Allocate 256 bytes
ffi.writeString(ptr, "Hello, FFI!");          // Write string to memory
const str = ffi.readString(ptr);              // Read string back
ffi.free(ptr);                                // Clean up memory

// Copy memory between buffers
const src = ffi.malloc(100);
const dest = ffi.malloc(100);
ffi.writeString(src, "Source data");
ffi.memcpy(dest, src, 50);                    // Copy 50 bytes
ffi.free(src);
ffi.free(dest);
```

### ⚠️ Current Limitations

While significantly improved, some limitations remain:

1. **Complex Types**: Structs, unions, and arrays are not yet supported
2. **Callbacks**: No support for C functions calling JavaScript functions
3. **No libffi**: Still uses basic x86_64 calling conventions 
4. **Platform Dependencies**: Some math functions may not be available on all systems

### What Works ✅

- ✅ Loading dynamic libraries across platforms (Windows, Linux, macOS)
- ✅ Function symbol resolution with enhanced error handling
- ✅ **Function calls with up to 16 arguments** (was 4)
- ✅ **Complete type system**: int, int32, int64, float, double, string, pointer, void
- ✅ **Memory management API**: malloc, free, memcpy, readString, writeString  
- ✅ **Enhanced argument parsing**: numbers, strings, booleans, null/undefined
- ✅ **Type constants**: `ffi.types` for cleaner code
- ✅ Both CommonJS (`require`) and ES Module (`import`) support

### What's Limited ⚠️

- ⚠️ **Structs/unions**: Complex data structures not supported
- ⚠️ **Callbacks**: C-to-JavaScript function calls not implemented  
- ⚠️ **libffi**: Advanced calling convention handling not integrated
- ⚠️ **Async calls**: No support for asynchronous native function execution

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

To further enhance this FFI implementation:

1. **~~Expand argument support~~** ✅ **COMPLETED** - Now supports up to 16 arguments
2. **~~Add numeric types~~** ✅ **COMPLETED** - Added float, double, int64 support  
3. **~~Memory management~~** ✅ **COMPLETED** - Added malloc/free/memcpy/readString/writeString
4. **Integrate libffi**: Use libffi for robust cross-platform function calling
5. **Add callback support**: Allow JavaScript functions to be called from native code
6. **Struct/union support**: Define and use complex data types
7. **Array handling**: Support for native arrays and buffers
8. **Async function support**: Support for asynchronous native function calls
9. **Symbol export**: Allow exporting JavaScript functions to native libraries
10. **ABI validation**: Ensure calling convention compatibility
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