/**
 * Test suite for Module class properties and methods
 */

const { Module } = require('node:module');

// Test Module.wrap() static method
try {
  console.assert(
    typeof Module.wrap === 'function',
    'Module.wrap should be a function'
  );
  const wrapped = Module.wrap('console.log("hello");');
  console.assert(
    typeof wrapped === 'string',
    'Module.wrap should return a string'
  );
  console.assert(
    wrapped.includes(
      'function (exports, require, module, __filename, __dirname)'
    ),
    'wrapped code should contain CommonJS wrapper'
  );
  console.assert(
    wrapped.includes('console.log("hello");'),
    'wrapped code should contain original code'
  );
  console.log('✓ Module.wrap() works correctly');
} catch (e) {
  console.error('✗ Module.wrap() test failed:', e.message);
}

// Test Module.wrapper property
try {
  console.assert(
    Array.isArray(Module.wrapper),
    'Module.wrapper should be an array'
  );
  console.assert(
    Module.wrapper.length === 2,
    'Module.wrapper should have 2 elements'
  );
  console.assert(
    Module.wrapper[0].includes('function (exports, require, module'),
    'wrapper[0] should be the prefix'
  );
  console.assert(
    Module.wrapper[1].includes('});'),
    'wrapper[1] should be the suffix'
  );
  console.log('✓ Module.wrapper property exists and is correct');
} catch (e) {
  console.error('✗ Module.wrapper test failed:', e.message);
}

// Test Module instance properties with getters/setters
try {
  const mod = new Module('test-id');

  // Test id property
  console.assert(mod.id === 'test-id', 'initial id should be set');
  mod.id = 'new-id';
  console.assert(mod.id === 'new-id', 'id should be updatable');

  // Test filename and path properties
  mod.filename = '/path/to/module.js';
  console.assert(
    mod.filename === '/path/to/module.js',
    'filename should be set'
  );
  console.assert(
    mod.path === '/path/to',
    'path should be auto-extracted from filename'
  );

  // Test filename with Windows path
  mod.filename = 'C:\\Users\\test\\module.js';
  console.assert(
    mod.filename === 'C:\\Users\\test\\module.js',
    'Windows filename should be set'
  );
  console.assert(
    mod.path === 'C:\\Users\\test',
    'Windows path should be auto-extracted'
  );

  // Test loaded property
  console.assert(mod.loaded === false, 'initial loaded should be false');
  mod.loaded = true;
  console.assert(mod.loaded === true, 'loaded should be updatable');

  // Test exports property
  console.assert(
    typeof mod.exports === 'object',
    'exports should be an object'
  );
  mod.exports.foo = 'bar';
  console.assert(mod.exports.foo === 'bar', 'exports should be modifiable');

  // Test children and paths arrays
  console.assert(Array.isArray(mod.children), 'children should be an array');
  console.assert(Array.isArray(mod.paths), 'paths should be an array');

  console.log('✓ Module instance properties work correctly');
} catch (e) {
  console.error('✗ Module properties test failed:', e.message);
  console.error(e.stack);
}

// Test Module instance methods
try {
  const mod = new Module('test-module');
  console.assert(
    typeof mod.require === 'function',
    'module.require should be a function'
  );
  console.assert(
    typeof mod._compile === 'function',
    'module._compile should be a function'
  );
  console.log('✓ Module instance methods exist');
} catch (e) {
  console.error('✗ Module instance methods test failed:', e.message);
}

// Test Module with parent
try {
  const parent = new Module('parent');
  const child = new Module('child', parent);
  console.assert(child.parent !== undefined, 'child should have parent');
  console.log('✓ Module parent relationship works');
} catch (e) {
  console.error('✗ Module parent test failed:', e.message);
}

console.log('\nModule properties tests completed');
