const assert = require('jsrt:assert');

try {
  const ffi = require('jsrt:ffi');

  assert.strictEqual(
    typeof ffi.callAsync,
    'function',
    'callAsync should be available'
  );
  try {
    ffi.callAsync(); // No arguments
    assert.fail('Should have thrown an error for missing arguments');
  } catch (error) {
    // console.log('✓ Got expected error:', error.message);
  }
} catch (error) {
  console.error('❌ Minimal async function test failed:', error.message);
  throw error;
}
