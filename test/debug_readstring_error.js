const ffi = require('std:ffi');

try {
  ffi.readString(0);
} catch (error) {
  console.log('Actual readString error message:', error.message);
}
