/**
 * Test suite for Module class
 */

// Test Module class existence
try {
  const { Module } = require('node:module');
  console.assert(Module !== undefined, 'Module should be defined');
  console.assert(
    typeof Module === 'function',
    'Module should be a constructor'
  );
  console.log('✓ Module class exists');
} catch (e) {
  console.error('✗ Failed to load Module class:', e.message);
}

// Test Module constructor
try {
  const { Module } = require('node:module');
  const mod = new Module('test-module');
  console.assert(mod !== undefined, 'Module instance should be created');
  console.assert(mod.id === 'test-module', 'Module id should be set');
  console.assert(
    typeof mod.exports === 'object',
    'Module exports should be an object'
  );
  console.assert(
    mod.loaded === false,
    'Module loaded should be false initially'
  );
  console.assert(
    Array.isArray(mod.children),
    'Module children should be an array'
  );
  console.assert(Array.isArray(mod.paths), 'Module paths should be an array');
  console.log('✓ Module constructor works correctly');
} catch (e) {
  console.error('✗ Module constructor test failed:', e.message);
}

// Test Module static methods
try {
  const { Module } = require('node:module');
  console.assert(
    typeof Module._load === 'function',
    'Module._load should exist'
  );
  console.assert(
    typeof Module._resolveFilename === 'function',
    'Module._resolveFilename should exist'
  );
  console.log('✓ Module static methods exist');
} catch (e) {
  console.error('✗ Module static methods test failed:', e.message);
}

console.log('Module class tests completed');
