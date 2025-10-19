const assert = require('jsrt:assert');

// Note: Standalone WebAssembly.Table constructor is not supported due to WAMR C API limitation
// These tests verify that the constructor throws helpful errors
// For functional table tests, see test/web/webassembly/test_web_wasm_exported_table.js

// Test 1: Table constructor throws TypeError
console.log('\nTest 1: Table constructor throws TypeError');
try {
  const table = new WebAssembly.Table({ element: 'funcref', initial: 1 });
  console.log('❌ Test 1 failed: should have thrown TypeError');
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  assert.ok(
    error.message.includes('not supported'),
    'Error message should mention not supported'
  );
  assert.ok(
    error.message.includes('instance.exports'),
    'Error message should provide example'
  );
  console.log('✅ Test 1 passed: Constructor throws helpful TypeError');
}

// Test 2: Error message guides users to exported tables
console.log('\nTest 2: Error message guides users to exported tables');
try {
  new WebAssembly.Table({
    element: 'externref',
    initial: 1,
    maximum: 10,
  });
  console.log('❌ Test 2 failed: should have thrown');
} catch (error) {
  assert.ok(
    error.message.includes('exported'),
    'Should mention exported tables'
  );
  console.log('✅ Test 2 passed: Error message mentions exported tables');
}

// Test 3: Constructor fails regardless of element type
console.log('\nTest 3: Constructor fails regardless of element type');
try {
  new WebAssembly.Table({ element: 'funcref', initial: 0 });
  console.log('❌ Test 3 failed: should have thrown');
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  console.log('✅ Test 3 passed: Constructor fails with any element type');
}

console.log('\n========================================');
console.log('Table constructor tests completed!');
console.log('All tests verify constructor is properly disabled');
console.log('For functional table tests, see exported table tests');
console.log('========================================');
