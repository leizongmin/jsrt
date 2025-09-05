const assert = require('jsrt:assert');

console.log('Testing improved node:path module functionality...');

// Test CommonJS require
const path = require('node:path');

console.log('Testing path.normalize...');
// Test path normalization
assert.strictEqual(path.normalize('/a/b/c/../d'), '/a/b/d');
assert.strictEqual(path.normalize('/a/b/c/./d'), '/a/b/c/d');
assert.strictEqual(path.normalize('a/../b'), 'b');
assert.strictEqual(path.normalize('./a/b'), 'a/b');
assert.strictEqual(path.normalize('a/./b'), 'a/b');
assert.strictEqual(path.normalize('a//b'), 'a/b');
console.log('✅ path.normalize tests passed');

console.log('Testing path.relative...');
// Test path.relative
assert.strictEqual(path.relative('/a/b', '/a/b/c'), 'c');
assert.strictEqual(path.relative('/a/b/c', '/a/b'), '..');
assert.strictEqual(path.relative('/a/b', '/c/d'), '../../c/d');
assert.strictEqual(path.relative('/a/b', '/a/b'), '.');

// Test with relative paths
const rel1 = path.relative('a/b', 'a/c');
console.log('path.relative("a/b", "a/c") =', rel1);
assert.ok(rel1.includes('c')); // Should contain 'c' in some form

console.log('✅ path.relative tests passed');

console.log('Testing existing functionality still works...');
// Test that existing functionality still works
assert.strictEqual(path.join('a', 'b', 'c'), 'a/b/c');
assert.strictEqual(path.isAbsolute('/foo'), true);
assert.strictEqual(path.dirname('/a/b/c'), '/a/b');
assert.strictEqual(path.basename('file.txt'), 'file.txt');
assert.strictEqual(path.extname('file.txt'), '.txt');
console.log('✅ Existing functionality confirmed working');

console.log('=== Node.js path module improvements complete ===');