const assert = require('jsrt:assert');

console.log('=== Built-in Modules Console Output Tests ===');

// Test console.log with require() for various built-in modules
// This test specifically covers the case that was causing stack overflow:
// echo 'console.log(require("node:path"))' | ./bin/jsrt_g

console.log('Testing console.log(require("node:path")):');
console.log(require('node:path'));

console.log('\nTesting console.log(require("node:os")):');
console.log(require('node:os'));

console.log('\nTesting console.log(require("node:util")):');
console.log(require('node:util'));

// Test that the modules are properly cached and don't cause infinite recursion
console.log('\nTesting module caching (should return same objects):');
const path1 = require('node:path');
const path2 = require('node:path');
assert.strictEqual(
  path1,
  path2,
  'Modules should be cached and return same instance'
);
// Success case - no output

// Test that circular reference issue is fixed
console.log('\nTesting circular reference fix:');
const path = require('node:path');

// These properties should not cause infinite loops when serialized
if (path.posix) {
  console.log('path.posix exists:', typeof path.posix === 'object');
  // Ensure posix object has methods but is not circular
  assert.ok(
    typeof path.posix.join === 'function',
    'path.posix should have join method'
  );
}

if (path.win32) {
  console.log('path.win32 exists:', typeof path.win32 === 'object');
  // Ensure win32 object exists but doesn't cause issues
  assert.ok(typeof path.win32 === 'object', 'path.win32 should be an object');
}

// Test that we can stringify the module without infinite recursion
console.log('\nTesting JSON.stringify on path module:');
try {
  const pathStr = JSON.stringify(path, null, 2);
  // Success case - no output
  // Verify the string contains expected properties
  assert.ok(
    pathStr.includes('"sep"'),
    'Serialized path should contain sep property'
  );
} catch (e) {
  console.error('✗ JSON.stringify failed:', e.message);
  throw e;
}

// Test require with different module styles
console.log('\nTesting different require styles:');

// Test with node: prefix
const pathWithPrefix = require('node:path');
assert.ok(pathWithPrefix.join, 'node:path should have join method');

// Test multiple requires in same statement
console.log('\nTesting multiple requires in expressions:');
const joinResult = require('node:path').join('a', 'b');
// Platform-aware path checking - expect appropriate separator for platform
const expectedResult = 'a' + path.sep + 'b';
assert.strictEqual(
  joinResult,
  expectedResult,
  'Direct method call on require result should work'
);
// Success case - no output

// Test console.log with method calls
console.log('\nTesting console.log with method calls:');
console.log(
  'path.join("tmp", "file.txt"):',
  require('node:path').join('tmp', 'file.txt')
);
console.log('os.platform():', require('node:os').platform());

// Test edge cases that previously caused issues
console.log('\nTesting edge cases:');

// Nested console.log calls
console.log(
  'Nested test:',
  console.log(require('node:path').sep) || 'nested call completed'
);

// Multiple modules in same line
const modules = [
  require('node:path'),
  require('node:os'),
  require('node:util'),
];
console.log('Multiple modules loaded:', modules.length, 'modules');

// Success case - no output
// Success case - no output
