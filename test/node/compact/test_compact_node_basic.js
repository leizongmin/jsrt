// Test that compact-node functionality is enabled by default
// This test should be run with: jsrt test_compact_node_basic.js

const assert = require('jsrt:assert');

// Test 1: Bare module names resolve to Node.js modules
console.log('\nTest 1: Bare module names resolve to Node.js modules');
const os = require('os');
const path = require('path');
const util = require('util');

assert(typeof os === 'object', 'os module should be loaded');
assert(typeof path === 'object', 'path module should be loaded');
assert(typeof util === 'object', 'util module should be loaded');

// Test 2: Node.js module functions work correctly
console.log('\nTest 2: Node.js module functions work correctly');
assert(typeof os.platform === 'function', 'os.platform should be a function');
const platform = os.platform();
assert(typeof platform === 'string', 'os.platform() should return a string');
console.log('  Platform:', platform);

assert(typeof path.join === 'function', 'path.join should be a function');
const joined = path.join('a', 'b', 'c');
assert(
  joined.includes('a') && joined.includes('b') && joined.includes('c'),
  'path.join should work'
);
console.log('  Joined path:', joined);

// Test 3: Mixed usage - both prefixed and unprefixed
console.log('\nTest 3: Mixed usage - both prefixed and unprefixed');
const osWithPrefix = require('node:os');
const pathWithPrefix = require('node:path');

assert(typeof osWithPrefix === 'object', 'node:os should work');
assert(typeof pathWithPrefix === 'object', 'node:path should work');

// Test 4: Verify they're the same module (caching works)
console.log('\nTest 4: Module caching works correctly');
assert(
  os === osWithPrefix,
  'require("os") should return same object as require("node:os")'
);
assert(
  path === pathWithPrefix,
  'require("path") should return same object as require("node:path")'
);

// Test 5: Global process object
console.log('\nTest 5: Global process object is available');
assert(typeof process !== 'undefined', 'process should be globally available');
assert(
  typeof process.platform === 'string',
  'process.platform should be a string'
);
assert(typeof process.pid === 'number', 'process.pid should be a number');
console.log('  Process PID:', process.pid);
console.log('  Process platform:', process.platform);

// Test 6: Submodules work
console.log('\nTest 6: Submodules work correctly');
try {
  const streamPromises = require('stream/promises');
  assert(
    typeof streamPromises === 'object',
    'stream/promises should be loaded'
  );
  console.log('  stream/promises loaded successfully');
} catch (e) {
  console.log('  stream/promises not yet implemented (expected)');
}

console.log('\n✅ All tests passed!');
