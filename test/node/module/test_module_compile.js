/**
 * Test suite for module._compile()
 */

const { Module } = require('node:module');

// Test 1: Basic module compilation
try {
  const mod = new Module('test-compile');
  mod.filename = '/test/compile.js';

  const code = 'exports.value = 42;';
  mod._compile(code, mod.filename);

  console.assert(mod.exports.value === 42, 'Should compile and execute code');
  console.log('✓ Basic module compilation');
} catch (e) {
  console.error('✗ Basic compilation failed:', e.message);
}

// Test 2: Module.loaded flag set after compilation
try {
  const mod = new Module('test-loaded');
  mod.filename = '/test/loaded.js';

  console.assert(mod.loaded === false, 'Initially loaded should be false');

  mod._compile('exports.foo = "bar";', mod.filename);

  console.assert(
    mod.loaded === true,
    'loaded should be true after compilation'
  );
  console.log('✓ module.loaded flag updated');
} catch (e) {
  console.error('✗ module.loaded test failed:', e.message);
}

// Test 3: module.exports replacement
try {
  const mod = new Module('test-exports-replace');
  mod.filename = '/test/replace.js';

  const code = 'module.exports = { replaced: true };';
  mod._compile(code, mod.filename);

  console.assert(
    mod.exports.replaced === true,
    'Should handle module.exports replacement'
  );
  console.log('✓ module.exports replacement');
} catch (e) {
  console.error('✗ module.exports replacement failed:', e.message);
}

// Test 4: Access to __filename
try {
  const mod = new Module('test-filename');
  mod.filename = '/test/filename.js';

  const code = 'exports.filename = __filename;';
  mod._compile(code, mod.filename);

  console.assert(
    mod.exports.filename === mod.filename,
    '__filename should be available'
  );
  console.log('✓ __filename variable available');
} catch (e) {
  console.error('✗ __filename test failed:', e.message);
}

// Test 5: Access to __dirname
try {
  const mod = new Module('test-dirname');
  mod.filename = '/test/dir/file.js';

  const code = 'exports.dirname = __dirname;';
  mod._compile(code, mod.filename);

  console.assert(
    mod.exports.dirname === '/test/dir',
    '__dirname should be directory of file'
  );
  console.log('✓ __dirname variable available');
} catch (e) {
  console.error('✗ __dirname test failed:', e.message);
}

// Test 6: Access to require function
try {
  const mod = new Module('test-require');
  mod.filename = '/test/require.js';

  const code = 'exports.hasRequire = (typeof require === "function");';
  mod._compile(code, mod.filename);

  console.assert(
    mod.exports.hasRequire === true,
    'require should be available'
  );
  console.log('✓ require function available');
} catch (e) {
  console.error('✗ require function test failed:', e.message);
}

// Test 7: Access to exports shorthand
try {
  const mod = new Module('test-exports-shorthand');
  mod.filename = '/test/shorthand.js';

  const code = 'exports.value = 123;';
  mod._compile(code, mod.filename);

  console.assert(mod.exports.value === 123, 'exports shorthand should work');
  console.log('✓ exports shorthand works');
} catch (e) {
  console.error('✗ exports shorthand test failed:', e.message);
}

// Test 8: Syntax error handling
try {
  const mod = new Module('test-syntax-error');
  mod.filename = '/test/error.js';

  const code = 'this is invalid javascript {{{';
  mod._compile(code, mod.filename);

  console.error('✗ Should throw error for syntax error');
} catch (e) {
  console.log('✓ Syntax errors throw exception');
}

// Test 9: Missing arguments
try {
  const mod = new Module('test-missing-args');
  mod._compile('code');
  console.error('✗ Should throw error for missing filename');
} catch (e) {
  if (e.message.includes('Missing')) {
    console.log('✓ Error thrown for missing arguments');
  } else {
    console.error('✗ Wrong error:', e.message);
  }
}

// Test 10: Called on non-Module instance
try {
  const notAModule = {};
  Module.prototype._compile.call(notAModule, 'code', 'file.js');
  console.error('✗ Should throw error when called on non-Module');
} catch (e) {
  if (e.message.includes('Module instance')) {
    console.log('✓ Error thrown for non-Module instance');
  } else {
    console.error('✗ Wrong error:', e.message);
  }
}

console.log('\nmodule._compile tests completed');
