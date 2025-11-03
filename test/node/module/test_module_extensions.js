/**
 * Test suite for Module._extensions
 */

const { Module } = require('node:module');

// Test Module._extensions exists
try {
  console.assert(
    typeof Module._extensions === 'object',
    'Module._extensions should be an object'
  );
  console.assert(
    Module._extensions !== null,
    'Module._extensions should not be null'
  );
  console.log('✓ Module._extensions exists');
} catch (e) {
  console.error('✗ Module._extensions existence test failed:', e.message);
}

// Test Module._extensions is writable/configurable
try {
  // Should be able to add custom extension handler
  Module._extensions['.custom'] = function (module, filename) {
    console.log('Custom handler called');
  };
  console.assert(
    typeof Module._extensions['.custom'] === 'function',
    'Should be able to add custom extension'
  );
  console.log('✓ Module._extensions is writable');
} catch (e) {
  console.error('✗ Module._extensions writable test failed:', e.message);
}

// Test Module._cache exists (Task 1.6 stub)
try {
  console.assert(
    typeof Module._cache === 'object',
    'Module._cache should be an object'
  );
  console.assert(Module._cache !== null, 'Module._cache should not be null');
  console.log('✓ Module._cache exists (stub)');
} catch (e) {
  console.error('✗ Module._cache test failed:', e.message);
}

console.log('\nModule extensions tests completed');
