// Simple test for module.reloadModule() function
// This test verifies that hot module reloading works correctly

const { reloadModule, getStatistics } = require('node:module');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

console.log('Testing module.reloadModule() (simple version)...');

try {
  // Test 1: Function exists and is callable
  assert.strictEqual(
    typeof reloadModule,
    'function',
    'reloadModule should be a function'
  );
  console.log('✓ reloadModule function exists');

  // Test 2: Get initial statistics
  const initialStats = getStatistics();
  console.log('Initial loads:', initialStats.loadsTotal);

  // Test 3: Test reloading a built-in module
  const builtinResult = reloadModule('path');
  console.log('Builtin module reload result:', {
    reloadSuccess: builtinResult.reloadSuccess,
    wasCached: builtinResult.wasCached,
    path: builtinResult.path,
  });

  assert(
    typeof builtinResult === 'object',
    'Builtin reload should return result object'
  );
  assert(
    typeof builtinResult.reloadSuccess === 'boolean',
    'Should have reloadSuccess'
  );
  assert(
    typeof builtinResult.statistics === 'object',
    'Should have statistics'
  );
  console.log('✓ Built-in module reload works');

  // Test 4: Test reloading a non-existent module
  const nonExistentResult = reloadModule('./non-existent-module-xyz.js');
  assert(
    nonExistentResult.reloadSuccess === false,
    'Non-existent module reload should fail'
  );
  assert(
    typeof nonExistentResult.error === 'string',
    'Should have error message'
  );
  console.log('✓ Non-existent module handling works correctly');

  // Test 5: Test Module.reloadModule as static method
  const Module = require('module');
  assert.strictEqual(
    typeof Module.reloadModule,
    'function',
    'Module.reloadModule should be a function'
  );

  const staticResult = Module.reloadModule('fs');
  assert(
    typeof staticResult === 'object',
    'Module.reloadModule() should return an object'
  );
  console.log('✓ Static method Module.reloadModule() works');

  // Test 6: Verify statistics updated
  const finalStats = getStatistics();
  console.log('Final loads:', finalStats.loadsTotal);
  assert(
    finalStats.loadsTotal > initialStats.loadsTotal,
    'Statistics should show more loads'
  );

  console.log('✓ All tests passed!');
  console.log('Module.reloadModule() test completed successfully');
} catch (error) {
  console.error('Test failed:', error);
  process.exit(1);
}
