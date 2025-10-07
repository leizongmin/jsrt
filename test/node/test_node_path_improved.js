const assert = require('jsrt:assert');

// Test CommonJS require
const path = require('node:path');

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
assert.ok(rel1.includes('c')); // Should contain 'c' in some form

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
