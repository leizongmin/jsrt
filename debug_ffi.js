const ffi = require('std:ffi');

console.log('=== Debug FFI Function Properties ===');

const lib = ffi.Library('libc.so.6', {
  abs: ['int', ['int']]
});

console.log('Function:', typeof lib.abs);
console.log('Properties:', Object.getOwnPropertyNames(lib.abs));
console.log('__ffi_func_ptr:', lib.abs.__ffi_func_ptr);

// Try to call the function
try {
  const result = lib.abs(-42);
  console.log('Result:', result);
} catch (err) {
  console.log('Error:', err.message);
}