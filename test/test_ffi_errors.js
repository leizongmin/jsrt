const assert = require("std:assert");

console.log('=== FFI Enhanced Error Messages Tests ===');

// Test 1: Basic FFI module loading with enhanced error messages
console.log('Test 1: Loading FFI module with enhanced error messages');
const ffi = require('std:ffi');
assert.strictEqual(typeof ffi, 'object', 'FFI module should be an object');
assert.strictEqual(ffi.version, '3.0.0', 'FFI version should be 3.0.0');
console.log('✓ FFI module with enhanced error messages loaded successfully');

// Test 2: Enhanced error messages for invalid arguments
console.log('\nTest 2: Enhanced error messages for invalid arguments');

// Test ffi.Library with missing arguments
try {
  ffi.Library();
  assert.fail('ffi.Library should throw with no arguments');
} catch (error) {
  console.log('✓ Enhanced error for missing arguments:', error.message);
  assert(error.message.includes('FFI Error in ffi.Library'), 'Error should include function context');
  assert(error.message.includes('Expected 2 arguments'), 'Error should specify expected arguments');
}

// Test ffi.Library with invalid library name
try {
  ffi.Library(null, {});
  assert.fail('ffi.Library should throw with null library name');
} catch (error) {
  console.log('✓ Enhanced error for invalid library name:', error.message);
  assert(error.message.includes('FFI Error in ffi.Library'), 'Error should include function context');
}

// Test ffi.Library with invalid function definitions
try {
  ffi.Library('test.so', 'not an object');
  assert.fail('ffi.Library should throw with invalid function definitions');
} catch (error) {
  console.log('✓ Enhanced error for invalid function definitions:', error.message);
  assert(error.message.includes('FFI Error in ffi.Library'), 'Error should include function context');
  assert(error.message.includes('must be an object'), 'Error should specify expected type');
}

// Test 3: Enhanced error messages for memory functions
console.log('\nTest 3: Enhanced error messages for memory functions');

// Test ffi.malloc with invalid arguments
try {
  ffi.malloc();
  assert.fail('ffi.malloc should throw with no arguments');
} catch (error) {
  console.log('✓ Enhanced error for malloc no arguments:', error.message);
  assert(error.message.includes('FFI Error in ffi.malloc'), 'Error should include function context');
  assert(error.message.includes('Expected 1 argument'), 'Error should specify expected arguments');
}

// Test ffi.malloc with zero size
try {
  ffi.malloc(0);
  assert.fail('ffi.malloc should throw with zero size');
} catch (error) {
  console.log('✓ Enhanced error for malloc zero size:', error.message);
  assert(error.message.includes('FFI Error in ffi.malloc'), 'Error should include function context');
  assert(error.message.includes('Cannot allocate zero bytes'), 'Error should explain the issue');
}

// Test ffi.malloc with too large size
try {
  ffi.malloc(2 * 1024 * 1024 * 1024); // 2GB
  assert.fail('ffi.malloc should throw with oversized allocation');
} catch (error) {
  console.log('✓ Enhanced error for malloc oversized:', error.message);
  assert(error.message.includes('FFI Error in ffi.malloc'), 'Error should include function context');
  assert(error.message.includes('size too large'), 'Error should explain size limit');
  assert(error.message.includes('maximum: 1GB'), 'Error should specify maximum size');
}

// Test 4: Enhanced error messages for array functions
console.log('\nTest 4: Enhanced error messages for array functions');

// Test ffi.arrayFromPointer with missing arguments
try {
  ffi.arrayFromPointer();
  assert.fail('ffi.arrayFromPointer should throw with no arguments');
} catch (error) {
  console.log('✓ Enhanced error for arrayFromPointer no arguments:', error.message);
  assert(error.message.includes('FFI Error in ffi.arrayFromPointer'), 'Error should include function context');
  assert(error.message.includes('Expected 3 arguments'), 'Error should specify expected arguments');
}

// Test ffi.arrayFromPointer with invalid type
try {
  ffi.arrayFromPointer(12345, 5, 'invalid_type');
  assert.fail('ffi.arrayFromPointer should throw with invalid type');
} catch (error) {
  console.log('✓ Enhanced error for arrayFromPointer invalid type:', error.message);
  assert(error.message.includes('FFI Error in ffi.arrayFromPointer'), 'Error should include function context');
  assert(error.message.includes('Invalid array element type'), 'Error should explain the issue');
  assert(error.message.includes('invalid_type'), 'Error should show the invalid type');
}

// Test ffi.arrayFromPointer with too large array
try {
  ffi.arrayFromPointer(12345, 2000000, 'int32');
  assert.fail('ffi.arrayFromPointer should throw with oversized array');
} catch (error) {
  console.log('✓ Enhanced error for arrayFromPointer oversized array:', error.message);
  assert(error.message.includes('FFI Error in ffi.arrayFromPointer'), 'Error should include function context');
  assert(error.message.includes('Array length too large'), 'Error should explain the issue');
  assert(error.message.includes('maximum: 1M elements'), 'Error should specify maximum size');
}

// Test ffi.arrayLength with non-array
try {
  ffi.arrayLength('not an array');
  assert.fail('ffi.arrayLength should throw with non-array');
} catch (error) {
  console.log('✓ Enhanced error for arrayLength non-array:', error.message);
  assert(error.message.includes('must be an array'), 'Error should specify expected type');
}

// Test 5: Enhanced error messages for library loading failures
console.log('\nTest 5: Enhanced library loading error messages');

// Test loading non-existent library
try {
  ffi.Library('non_existent_library_xyz.so', {
    'dummy': ['void', []]
  });
  assert.fail('ffi.Library should throw with non-existent library');
} catch (error) {
  console.log('✓ Enhanced error for non-existent library:', error.message);
  assert(error.message.includes('FFI Error in ffi.Library'), 'Error should include function context');
  assert(error.message.includes('Failed to load library'), 'Error should explain the failure');
  assert(error.message.includes('non_existent_library_xyz.so'), 'Error should include library name');
  assert(error.message.includes('Please check'), 'Error should provide helpful suggestions');
  // Should include platform-specific suggestions
  console.log('Library loading error (truncated):', error.message.substring(0, 200) + '...');
  console.log('✓ Enhanced error includes helpful suggestions and platform-specific advice');
}

console.log('\n=== Enhanced Error Messages Tests Completed ===');
console.log('Enhanced error reporting provides:');
console.log('- Function context in error messages (e.g., "FFI Error in ffi.malloc")');
console.log('- Detailed argument validation with clear expectations');
console.log('- Platform-specific troubleshooting suggestions');
console.log('- Clear explanations of limits and constraints');
console.log('- Consistent error format across all FFI functions');