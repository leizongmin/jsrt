/**
 * Test suite for Module._load()
 */

const { Module } = require('node:module');
const fs = require('node:fs');
const path = require('node:path');

// Test 1: Load builtin module
try {
  const fsModule = Module._load('fs');
  console.assert(typeof fsModule === 'object', 'Should load builtin module');
  console.assert(
    typeof fsModule.readFileSync === 'function',
    'Builtin module should have expected methods'
  );
  console.log('✓ Load builtin module (fs)');
} catch (e) {
  console.error('✗ Builtin module loading failed:', e.message);
}

// Test 2: Load node: prefixed builtin
try {
  const pathModule = Module._load('node:path');
  console.assert(
    typeof pathModule === 'object',
    'Should load node: prefixed builtin'
  );
  console.assert(
    typeof pathModule.join === 'function',
    'node: prefixed module should have expected methods'
  );
  console.log('✓ Load node: prefixed builtin');
} catch (e) {
  console.error('✗ node: prefixed builtin loading failed:', e.message);
}

// Test 3: Module caching - same instance returned
try {
  const mod1 = Module._load('fs');
  const mod2 = Module._load('fs');
  console.assert(mod1 === mod2, 'Should return cached instance');
  console.log('✓ Module caching works');
} catch (e) {
  console.error('✗ Module caching test failed:', e.message);
}

// Test 4: Module added to Module._cache
try {
  // Clear cache first
  const cacheKeys = Object.keys(Module._cache);
  const initialCount = cacheKeys.length;

  // Load a builtin (note: builtins use their name as cache key)
  Module._load('util');

  // Check if it was added to cache
  console.assert(
    'util' in Module._cache || 'node:util' in Module._cache,
    'Module should be in cache after loading'
  );
  console.log('✓ Module added to Module._cache');
} catch (e) {
  console.error('✗ Module cache addition test failed:', e.message);
}

// Test 5: Missing argument
try {
  Module._load();
  console.error('✗ Should throw error for missing request');
} catch (e) {
  if (e.message.includes('Missing request')) {
    console.log('✓ Error thrown for missing request argument');
  } else {
    console.error('✗ Wrong error:', e.message);
  }
}

// Test 6: Non-existent module
try {
  Module._load('/this/path/definitely/does/not/exist/nowhere.js');
  console.error('✗ Should throw error for non-existent module');
} catch (e) {
  console.log('✓ Error thrown for non-existent module');
}

// Test 7: Parent module parameter
try {
  const parent = new Module('parent');
  parent.filename = '/test/parent.js';

  // Load a builtin with parent
  const childModule = Module._load('os', parent);
  console.assert(
    typeof childModule === 'object',
    'Should load module with parent parameter'
  );
  console.log('✓ Load with parent module parameter');
} catch (e) {
  console.error('✗ Parent module parameter test failed:', e.message);
}

// Test 8: Module.loaded flag set after loading
try {
  const moduleName = 'crypto';
  // Clear from cache if present
  delete Module._cache[moduleName];
  delete Module._cache['node:' + moduleName];

  const loaded = Module._load(moduleName);

  // Check the cached module's loaded flag
  const cached =
    Module._cache[moduleName] || Module._cache['node:' + moduleName];
  if (cached) {
    console.assert(
      cached.loaded === true,
      'Cached module should have loaded=true'
    );
    console.log('✓ Module.loaded flag set after loading');
  } else {
    console.log('⚠ Module not in cache, skipping loaded flag test');
  }
} catch (e) {
  console.error('✗ Module.loaded flag test failed:', e.message);
}

console.log('\nModule._load tests completed');
