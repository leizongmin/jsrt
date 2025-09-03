const ffi = require('jsrt:ffi');

console.log('Loading FFI module...');
console.log('FFI version:', ffi.version);

try {
  console.log('Loading libc...');
  const libc = ffi.Library('libc.so.6', {
    abs: ['int', ['int']],
  });

  console.log('libc loaded, trying to call abs...');
  console.log('abs function:', libc.abs);
  console.log('abs properties:', Object.getOwnPropertyNames(libc.abs));

  const result = libc.abs(-42);
  console.log('abs(-42) =', result);
} catch (error) {
  console.log('Error:', error.message);
  console.log('Stack:', error.stack);
}
