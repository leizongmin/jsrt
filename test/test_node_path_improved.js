const assert = require('jsrt:assert');

console.log('Testing improved node:path module functionality...');

// Test CommonJS require
const path = require('node:path');

console.log('Testing path.normalize...');
// Test path normalization - platform-aware expectations
assert.strictEqual(
  path.normalize(
    path.sep +
      'a' +
      path.sep +
      'b' +
      path.sep +
      'c' +
      path.sep +
      '..' +
      path.sep +
      'd'
  ),
  path.sep + 'a' + path.sep + 'b' + path.sep + 'd'
);
assert.strictEqual(
  path.normalize(
    path.sep +
      'a' +
      path.sep +
      'b' +
      path.sep +
      'c' +
      path.sep +
      '.' +
      path.sep +
      'd'
  ),
  path.sep + 'a' + path.sep + 'b' + path.sep + 'c' + path.sep + 'd'
);
assert.strictEqual(path.normalize('a/../b'), 'b');
assert.strictEqual(path.normalize('./a/b'), 'a' + path.sep + 'b');
assert.strictEqual(path.normalize('a/./b'), 'a' + path.sep + 'b');
assert.strictEqual(path.normalize('a//b'), 'a' + path.sep + 'b');
console.log('✅ path.normalize tests passed');

console.log('Testing path.relative...');
// Test path.relative - platform-aware expectations
assert.strictEqual(
  path.relative(
    path.sep + 'a' + path.sep + 'b',
    path.sep + 'a' + path.sep + 'b' + path.sep + 'c'
  ),
  'c'
);
assert.strictEqual(
  path.relative(
    path.sep + 'a' + path.sep + 'b' + path.sep + 'c',
    path.sep + 'a' + path.sep + 'b'
  ),
  '..'
);
assert.strictEqual(
  path.relative(
    path.sep + 'a' + path.sep + 'b',
    path.sep + 'c' + path.sep + 'd'
  ),
  '..' + path.sep + '..' + path.sep + 'c' + path.sep + 'd'
);
assert.strictEqual(
  path.relative(
    path.sep + 'a' + path.sep + 'b',
    path.sep + 'a' + path.sep + 'b'
  ),
  '.'
);

// Test with relative paths
const rel1 = path.relative('a/b', 'a/c');
console.log('path.relative("a/b", "a/c") =', rel1);
assert.ok(rel1.includes('c')); // Should contain 'c' in some form

console.log('✅ path.relative tests passed');

console.log('Testing existing functionality still works...');
// Test that existing functionality still works - platform-aware
assert.strictEqual(
  path.join('a', 'b', 'c'),
  'a' + path.sep + 'b' + path.sep + 'c'
);
assert.strictEqual(path.isAbsolute(path.sep + 'foo'), true);
assert.strictEqual(
  path.dirname(path.sep + 'a' + path.sep + 'b' + path.sep + 'c'),
  path.sep + 'a' + path.sep + 'b'
);
assert.strictEqual(path.basename('file.txt'), 'file.txt');
assert.strictEqual(path.extname('file.txt'), '.txt');
console.log('✅ Existing functionality confirmed working');

console.log('=== Node.js path module improvements complete ===');
