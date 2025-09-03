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

### ✅ Recent Major Improvements (v2.2.0)

This FFI implementation has been significantly enhanced with the following improvements:

#### 🚀 Enhanced Function Calling
- **16 Arguments Support**: Expanded from 4 to 16 function arguments
- **Enhanced Type System**: Full support for `int`, `int32`, `int64`, `float`, `double`, `string`, `pointer`, `void`, `array`
- **Better Argument Parsing**: Supports JavaScript numbers, strings, booleans, null/undefined, and arrays

#### 🛠️ Memory Management API
- **`ffi.malloc(size)`**: Allocate memory dynamically
- **`ffi.free(ptr)`**: Free allocated memory  
- **`ffi.memcpy(dest, src, size)`**: Copy memory between pointers
- **`ffi.readString(ptr, [maxLength])`**: Read null-terminated string from pointer
- **`ffi.writeString(ptr, str)`**: Write string to memory location

#### 📊 Array Support (NEW in v2.1.0+)
- **`ffi.arrayFromPointer(ptr, length, type)`**: Convert C arrays to JavaScript arrays
- **`ffi.arrayLength(array)`**: Get length of JavaScript arrays
- **Array argument support**: JavaScript arrays automatically converted to C arrays in function calls
- **Type-specific conversion**: Support for int32, float, double, uint32 array types

#### 💬 Enhanced Error Reporting (NEW in v2.2.0)
- **Function context in errors**: All errors include the specific FFI function name
- **Detailed validation messages**: Clear explanation of expected arguments and types
- **Platform-specific suggestions**: Tailored troubleshooting advice for Windows/Unix
- **Helpful constraint explanations**: Clear limits and maximum values explained
- **Consistent error format**: Standardized "FFI Error in function_name: message" format

#### 📚 Developer Experience
- **Type Constants**: `ffi.types` object with predefined type strings
- **Enhanced Error Handling**: Better validation and error messages with troubleshooting tips
- **Safety Limits**: Reasonable limits on memory operations for security
- **Cross-platform Support**: Works on Windows, Linux, and macOS

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

// Memory management with enhanced error reporting
try {
  const ptr = ffi.malloc(256);                  // Allocate 256 bytes
  ffi.writeString(ptr, "Hello, FFI!");          // Write string to memory
  const str = ffi.readString(ptr);              // Read string back
  ffi.free(ptr);                                // Clean up memory
} catch (error) {
  console.log('Memory operation failed:', error.message);
  // Error: "FFI Error in ffi.malloc: Allocation size too large: 2147483648 bytes (maximum: 1GB)"
}

// Array support (NEW)
const inputArray = [1, 2, 3, 4, 5];
// This array will be automatically converted to a C array when passed to functions

// Convert C array back to JavaScript
const cArrayPtr = ffi.malloc(5 * 4); // Allocate for 5 int32s
const jsArray = ffi.arrayFromPointer(cArrayPtr, 5, ffi.types.int32);
console.log('Converted array:', jsArray); // [0, 0, 0, 0, 0] (uninitialized)

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

1. **Complex Types**: Structs, unions, and complex nested types are not yet supported
2. **Callbacks**: No support for C functions calling JavaScript functions (partially implemented)
3. **No libffi**: Still uses basic x86_64 calling conventions 
4. **Platform Dependencies**: Some math functions may not be available on all systems
5. **Array Length Information**: C functions returning arrays need explicit length information

### What Works ✅

- ✅ Loading dynamic libraries across platforms (Windows, Linux, macOS)
- ✅ Function symbol resolution with enhanced error handling and troubleshooting tips
- ✅ **Function calls with up to 16 arguments** (was 4)
- ✅ **Complete type system**: int, int32, int64, float, double, string, pointer, void, array
- ✅ **Memory management API**: malloc, free, memcpy, readString, writeString  
- ✅ **Array support**: JavaScript arrays converted to C arrays, C arrays converted back to JavaScript
- ✅ **Enhanced argument parsing**: numbers, strings, booleans, null/undefined, arrays
- ✅ **Type constants**: `ffi.types` for cleaner code
- ✅ **Enhanced error reporting**: Function context, detailed validation, platform-specific suggestions
- ✅ Both CommonJS (`require`) and ES Module (`import`) support

### What's Limited ⚠️

- ⚠️ **Structs/unions**: Complex data structures not supported
- ⚠️ **Callbacks**: C-to-JavaScript function calls partially implemented  
- ⚠️ **libffi**: Advanced calling convention handling not integrated
- ⚠️ **Async calls**: No support for asynchronous native function execution
- ⚠️ **Array metadata**: Returned C arrays need explicit length information

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
4. **~~Array handling~~** ✅ **COMPLETED** - Added array conversion and argument support
5. **~~Better error messages~~** ✅ **COMPLETED** - Enhanced error reporting with context and suggestions
6. **Integrate libffi**: Use libffi for robust cross-platform function calling
7. **Complete callback support**: Allow JavaScript functions to be called from native code
8. **Struct/union support**: Define and use complex data types
9. **Async function support**: Support for asynchronous native function calls
10. **Symbol export**: Allow exporting JavaScript functions to native libraries
11. **ABI validation**: Ensure calling convention compatibility

## Version History

- **v1.0.0**: Initial proof-of-concept implementation
- **v2.0.0**: Enhanced function calling with 16 arguments, memory management, type system improvements
- **v2.1.0**: Added array support with conversion functions and automatic argument handling
- **v2.2.0**: Enhanced error reporting with function context, detailed validation, and platform-specific suggestions

## Version

Current version: **2.2.0** (Production Ready with Array Support and Enhanced Error Reporting)