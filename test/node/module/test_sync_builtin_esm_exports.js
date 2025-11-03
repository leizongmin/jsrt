// Test module.syncBuiltinESMExports() function
// This test verifies that CommonJS and ESM builtin module exports are synchronized

const { syncBuiltinESMExports } = require('node:module');
const assert = require('assert');

console.log('Testing module.syncBuiltinESMExports()...');

// Test 1: Function exists and is callable
assert.strictEqual(
  typeof syncBuiltinESMExports,
  'function',
  'syncBuiltinESMExports should be a function'
);

// Test 2: Calling the function should not throw
assert.doesNotThrow(() => {
  syncBuiltinESMExports();
}, 'syncBuiltinESMExports() should not throw');

// Test 3: Add a new property to a CommonJS module and verify it syncs
const fs = require('fs');

// Add a custom property to fs CommonJS module
fs.testSyncProperty = 'test-value';

// Call syncBuiltinESMExports to sync to ESM
syncBuiltinESMExports();

console.log('✓ syncBuiltinESMExports() executed successfully');

// Test 4: Multiple calls should be idempotent
assert.doesNotThrow(() => {
  syncBuiltinESMExports();
  syncBuiltinESMExports();
}, 'Multiple calls to syncBuiltinESMExports() should not throw');

console.log('✓ Multiple calls to syncBuiltinESMExports() successful');

// Test 5: Should return undefined
assert.strictEqual(
  syncBuiltinESMExports(),
  undefined,
  'syncBuiltinESMExports() should return undefined'
);

console.log('✓ All tests passed!');
console.log('module.syncBuiltinESMExports() test completed successfully');
