const ffi = require('std:ffi');

try {
  ffi.Struct(123, { x: 'int32' });
} catch (error) {
  console.log('Actual error message:', error.message);
}