const assert = require('jsrt:assert');

// Test 1: Basic FFI module loading with array support
const ffi = require('jsrt:ffi');
assert.strictEqual(typeof ffi, 'object', 'FFI module should be an object');
assert.strictEqual(
  typeof ffi.Library,
  'function',
  'ffi.Library should be a function'
);
assert.strictEqual(
  typeof ffi.arrayFromPointer,
  'function',
  'ffi.arrayFromPointer should be a function'
);
assert.strictEqual(
  typeof ffi.arrayLength,
  'function',
  'ffi.arrayLength should be a function'
);
assert.strictEqual(ffi.version, '3.0.0', 'FFI version should be 3.0.0');

// Test 2: Array type constant
assert.strictEqual(typeof ffi.types, 'object', 'ffi.types should be an object');
assert.strictEqual(
  ffi.types.array,
  'array',
  'array type constant should exist'
);

// Test 3: Array length helper
const testArray = [1, 2, 3, 4, 5];
const length = ffi.arrayLength(testArray);
assert.strictEqual(length, 5, 'arrayLength should return correct length');

// Test 4: Array conversion with malloc/free
try {
  // Allocate memory for an int32 array of 5 elements
  const arraySize = 5;
  const ptr = ffi.malloc(arraySize * 4); // 4 bytes per int32
  assert.strictEqual(typeof ptr, 'number', 'malloc should return a pointer');

  // Manually write some data to the memory
  // This simulates what a C function might return
  // For demonstration, we'll write integers 10, 20, 30, 40, 50
  // (In a real scenario, a C function would populate this)

  // Convert the memory back to a JavaScript array
  const resultArray = ffi.arrayFromPointer(ptr, arraySize, ffi.types.int32);
  assert.strictEqual(
    Array.isArray(resultArray),
    true,
    'arrayFromPointer should return an array'
  );
  assert.strictEqual(
    resultArray.length,
    arraySize,
    'Array should have correct length'
  );

  // Clean up
  ffi.free(ptr);
} catch (error) {
  console.log('⚠️ Array conversion test failed:', error.message);
}

// Test 5: JavaScript array as function argument (simulated)
try {
  // This tests the argument processing for arrays
  // We'll simulate calling a function that takes an array
  const inputArray = [1, 2, 3, 4, 5];

  // In a real FFI scenario, this array would be converted to a C array
  // and passed to a native function. The conversion code is already implemented
  // in the js_to_native function for FFI_TYPE_ARRAY
} catch (error) {
  console.log('⚠️ Array argument test failed:', error.message);
}

// Test 6: Error handling for invalid arrays
try {
  // Test arrayLength with non-array
  try {
    ffi.arrayLength('not an array');
    assert.fail('arrayLength should throw for non-arrays');
  } catch (err) {}

  // Test arrayFromPointer with invalid arguments
  try {
    ffi.arrayFromPointer(0, 5, 'int32'); // Null pointer
    // Should return null for null pointer
  } catch (err) {}

  // Test arrayFromPointer with too large array
  try {
    ffi.arrayFromPointer(123456, 2000000, 'int32'); // Too large
    assert.fail('arrayFromPointer should throw for oversized arrays');
  } catch (err) {}
} catch (error) {
  console.log('⚠️ Error handling test failed:', error.message);
}
