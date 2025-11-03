/**
 * Test suite for Module._resolveFilename()
 */

const { Module } = require('node:module');
const path = require('node:path');
const fs = require('node:fs');

// Test 1: Builtin module resolution (return as-is)
try {
  const resolved = Module._resolveFilename('fs');
  console.assert(resolved === 'fs', 'Should resolve builtin module as-is');
  console.log('✓ Builtin module resolution (fs)');
} catch (e) {
  console.error('✗ Builtin module resolution failed:', e.message);
}

// Test 2: node: prefixed builtin
try {
  const resolved = Module._resolveFilename('node:path');
  console.assert(
    resolved === 'node:path',
    'Should resolve node: prefixed builtin as-is'
  );
  console.log('✓ node: prefixed builtin resolution');
} catch (e) {
  console.error('✗ node: prefixed builtin resolution failed:', e.message);
}

// Test 3: Absolute path resolution
try {
  const testFile = '/tmp/test.js';
  // Create test file if it doesn't exist
  try {
    fs.writeFileSync(testFile, 'module.exports = {};');
  } catch (err) {
    // Ignore if we can't write to /tmp
  }

  const resolved = Module._resolveFilename(testFile);
  console.assert(resolved.includes('test.js'), 'Should resolve absolute path');
  console.log('✓ Absolute path resolution');
} catch (e) {
  console.error('✗ Absolute path resolution failed:', e.message);
}

// Test 4: Relative path resolution with parent module
try {
  // Create a dummy parent module
  const parent = new Module('/test/parent.js');
  parent.filename = '/test/parent.js';

  // This should resolve relative to /test/
  const resolved = Module._resolveFilename('./child.js', parent);
  console.assert(resolved.includes('child.js'), 'Should resolve relative path');
  console.log('✓ Relative path resolution with parent');
} catch (e) {
  console.error('✗ Relative path resolution failed:', e.message);
}

// Test 5: MODULE_NOT_FOUND error for non-existent module
try {
  Module._resolveFilename('/nonexistent/module/path/that/does/not/exist.js');
  console.error('✗ Should throw MODULE_NOT_FOUND for non-existent module');
} catch (e) {
  if (e.code === 'MODULE_NOT_FOUND') {
    console.log('✓ MODULE_NOT_FOUND error thrown correctly');
  } else {
    console.error('✗ Wrong error type:', e.code || e.message);
  }
}

// Test 6: Resolution without parent (from CWD)
try {
  const resolved = Module._resolveFilename('node:module');
  console.assert(
    resolved === 'node:module',
    'Should work without parent module'
  );
  console.log('✓ Resolution without parent module');
} catch (e) {
  console.error('✗ Resolution without parent failed:', e.message);
}

console.log('\nModule._resolveFilename tests completed');
