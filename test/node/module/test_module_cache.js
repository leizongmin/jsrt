/**
 * Test suite for Module._cache
 */

const { Module } = require('node:module');

// Test Module._cache exists
try {
  console.assert(
    typeof Module._cache === 'object',
    'Module._cache should be an object'
  );
  console.assert(Module._cache !== null, 'Module._cache should not be null');
  console.log('✓ Module._cache exists');
} catch (e) {
  console.error('✗ Module._cache existence test failed:', e.message);
}

// Test Module._cache is writable/configurable
try {
  // Should be able to add entries to cache
  const mod = new Module('test-module');
  Module._cache['/test/path.js'] = mod;

  console.assert(
    Module._cache['/test/path.js'] === mod,
    'Should be able to add entries to Module._cache'
  );

  // Should be able to delete entries from cache
  delete Module._cache['/test/path.js'];
  console.assert(
    Module._cache['/test/path.js'] === undefined,
    'Should be able to delete entries from Module._cache'
  );

  console.log('✓ Module._cache is writable/configurable');
} catch (e) {
  console.error('✗ Module._cache writable test failed:', e.message);
}

// Test Module._cache can store module instances
try {
  const mod1 = new Module('module1');
  const mod2 = new Module('module2');

  Module._cache['/path/to/module1.js'] = mod1;
  Module._cache['/path/to/module2.js'] = mod2;

  console.assert(
    Module._cache['/path/to/module1.js'].id === 'module1',
    'Cached module 1 should be retrievable'
  );
  console.assert(
    Module._cache['/path/to/module2.js'].id === 'module2',
    'Cached module 2 should be retrievable'
  );

  // Clean up
  delete Module._cache['/path/to/module1.js'];
  delete Module._cache['/path/to/module2.js'];

  console.log('✓ Module._cache can store module instances');
} catch (e) {
  console.error('✗ Module._cache storage test failed:', e.message);
}

console.log('\nModule cache tests completed');
