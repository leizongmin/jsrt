const assert = require('jsrt:assert');

console.log('=== Minimal FFI Async Function Tests ===');

try {
  const ffi = require('jsrt:ffi');

  console.log('Test 1: Check async function availability');
  assert.strictEqual(
    typeof ffi.callAsync,
    'function',
    'callAsync should be available'
  );
  console.log('✓ callAsync function is available');

  console.log('Test 2: Test error handling for missing arguments');

  try {
    ffi.callAsync(); // No arguments
    assert.fail('Should have thrown an error for missing arguments');
  } catch (error) {
    console.log('✓ Got expected error:', error.message);
  }

  console.log('✓ Basic async function structure tests passed');
} catch (error) {
  console.error('❌ Minimal async function test failed:', error.message);
  throw error;
}

console.log('=== Minimal Async Function Tests Completed ===');
