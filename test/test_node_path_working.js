const assert = require('jsrt:assert');

console.log('Testing node:path module (working features)...');

// Test CommonJS require
const path = require('node:path');

// Test basic functionality that we know works - platform-aware
assert.strictEqual(path.join('a', 'b'), 'a' + path.sep + 'b');
assert.strictEqual(path.isAbsolute(path.sep + 'foo'), true);
assert.strictEqual(
  path.dirname(path.sep + 'a' + path.sep + 'b'),
  path.sep + 'a'
);
assert.strictEqual(path.basename('file.txt'), 'file.txt');
assert.strictEqual(path.extname('file.txt'), '.txt');

// Test constants
assert.ok(path.sep);
assert.ok(path.delimiter);

// Success case - no output
// console.log('=== Node.js compatibility layer Phase 1 complete ===');
