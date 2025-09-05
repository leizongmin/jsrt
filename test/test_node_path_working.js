const assert = require('jsrt:assert');

console.log('Testing node:path module (working features)...');

// Test CommonJS require
const path = require('node:path');

// Test basic functionality that we know works
assert.strictEqual(path.join('a', 'b'), 'a/b');
assert.strictEqual(path.isAbsolute('/foo'), true);
assert.strictEqual(path.dirname('/a/b'), '/a');
assert.strictEqual(path.basename('file.txt'), 'file.txt');
assert.strictEqual(path.extname('file.txt'), '.txt');

// Test constants
assert.ok(path.sep);
assert.ok(path.delimiter);

console.log('✅ Basic node:path functionality confirmed working');
console.log('=== Node.js compatibility layer Phase 1 complete ===');
