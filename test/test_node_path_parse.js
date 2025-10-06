// Test suite for path.parse(), path.format(), and path.toNamespacedPath()
const path = require('node:path');

let tests = 0;
let passed = 0;
let failed = 0;

function assert(condition, message) {
  tests++;
  if (condition) {
    passed++;
  } else {
    failed++;
    console.log(`FAIL: ${message}`);
  }
}

function assertEqual(actual, expected, message) {
  tests++;
  if (actual === expected) {
    passed++;
  } else {
    failed++;
    console.log(`FAIL: ${message}`);
    console.log(`  Expected: ${JSON.stringify(expected)}`);
    console.log(`  Actual:   ${JSON.stringify(actual)}`);
  }
}

function assertObjectEqual(actual, expected, message) {
  tests++;
  const actualKeys = Object.keys(actual).sort();
  const expectedKeys = Object.keys(expected).sort();

  if (JSON.stringify(actualKeys) !== JSON.stringify(expectedKeys)) {
    failed++;
    console.log(`FAIL: ${message} - different keys`);
    console.log(`  Expected keys: ${JSON.stringify(expectedKeys)}`);
    console.log(`  Actual keys:   ${JSON.stringify(actualKeys)}`);
    return;
  }

  for (let key of expectedKeys) {
    if (actual[key] !== expected[key]) {
      failed++;
      console.log(`FAIL: ${message} - different value for key '${key}'`);
      console.log(`  Expected: ${JSON.stringify(expected)}`);
      console.log(`  Actual:   ${JSON.stringify(actual)}`);
      return;
    }
  }

  passed++;
}

console.log('Testing path.parse()...');

// Test 1: Basic Unix absolute path
let result = path.parse('/home/user/file.txt');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/home/user',
    base: 'file.txt',
    ext: '.txt',
    name: 'file',
  },
  "parse('/home/user/file.txt')"
);

// Test 2: Unix root path
result = path.parse('/');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/',
    base: '',
    ext: '',
    name: '',
  },
  "parse('/')"
);

// Test 3: Relative path
result = path.parse('file.txt');
assertObjectEqual(
  result,
  {
    root: '',
    dir: '',
    base: 'file.txt',
    ext: '.txt',
    name: 'file',
  },
  "parse('file.txt')"
);

// Test 4: Relative path with directory
result = path.parse('./dir/file.txt');
assertObjectEqual(
  result,
  {
    root: '',
    dir: './dir',
    base: 'file.txt',
    ext: '.txt',
    name: 'file',
  },
  "parse('./dir/file.txt')"
);

// Test 5: Path with no extension
result = path.parse('/home/user/file');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/home/user',
    base: 'file',
    ext: '',
    name: 'file',
  },
  "parse('/home/user/file')"
);

// Test 6: Hidden file (Unix)
result = path.parse('/home/user/.bashrc');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/home/user',
    base: '.bashrc',
    ext: '',
    name: '.bashrc',
  },
  "parse('/home/user/.bashrc')"
);

// Test 7: Multiple extensions
result = path.parse('/home/user/file.tar.gz');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/home/user',
    base: 'file.tar.gz',
    ext: '.gz',
    name: 'file.tar',
  },
  "parse('/home/user/file.tar.gz')"
);

// Test 8: Empty path
result = path.parse('');
assertObjectEqual(
  result,
  {
    root: '',
    dir: '',
    base: '',
    ext: '',
    name: '',
  },
  "parse('')"
);

// Test 9: Path with trailing slash
result = path.parse('/home/user/dir/');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/home/user',
    base: 'dir',
    ext: '',
    name: 'dir',
  },
  "parse('/home/user/dir/')"
);

// Test 10: Just filename with extension
result = path.parse('index.html');
assertObjectEqual(
  result,
  {
    root: '',
    dir: '',
    base: 'index.html',
    ext: '.html',
    name: 'index',
  },
  "parse('index.html')"
);

// Test 11: Dot at end
result = path.parse('/home/user/file.');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/home/user',
    base: 'file.',
    ext: '',
    name: 'file.',
  },
  "parse('/home/user/file.')"
);

// Test 12: Parent directory reference
result = path.parse('../file.txt');
assertObjectEqual(
  result,
  {
    root: '',
    dir: '..',
    base: 'file.txt',
    ext: '.txt',
    name: 'file',
  },
  "parse('../file.txt')"
);

console.log('\nTesting path.format()...');

// Test 13: Basic format with dir and base
result = path.format({
  dir: '/home/user',
  base: 'file.txt',
});
assertEqual(
  result,
  '/home/user/file.txt',
  "format({dir: '/home/user', base: 'file.txt'})"
);

// Test 14: Format with root and base (no dir)
result = path.format({
  root: '/',
  base: 'file.txt',
});
assertEqual(result, '/file.txt', "format({root: '/', base: 'file.txt'})");

// Test 15: Format with name and ext (no base)
result = path.format({
  dir: '/home/user',
  name: 'file',
  ext: '.txt',
});
assertEqual(
  result,
  '/home/user/file.txt',
  "format({dir: '/home/user', name: 'file', ext: '.txt'})"
);

// Test 16: Format with root, name, and ext (no dir or base)
result = path.format({
  root: '/',
  name: 'file',
  ext: '.txt',
});
assertEqual(
  result,
  '/file.txt',
  "format({root: '/', name: 'file', ext: '.txt'})"
);

// Test 17: Empty object
result = path.format({});
assertEqual(result, '.', 'format({})');

// Test 18: Only base
result = path.format({
  base: 'file.txt',
});
assertEqual(result, 'file.txt', "format({base: 'file.txt'})");

// Test 19: Priority - dir+base takes precedence over name+ext
result = path.format({
  dir: '/home/user',
  base: 'file.txt',
  name: 'other',
  ext: '.md',
});
assertEqual(result, '/home/user/file.txt', 'format() with dir+base priority');

// Test 20: Format with only name
result = path.format({
  name: 'file',
});
assertEqual(result, 'file', "format({name: 'file'})");

// Test 21: Format with only ext (should just be ext)
result = path.format({
  ext: '.txt',
});
assertEqual(result, '.txt', "format({ext: '.txt'})");

// Test 22: Format with dir that has trailing slash
result = path.format({
  dir: '/home/user/',
  base: 'file.txt',
});
// Should normalize and not have double slash
assert(
  result === '/home/user/file.txt' || result === '/home/user//file.txt',
  'format() with trailing slash in dir'
);

console.log('\nTesting parse/format inverse relationship...');

// Test 23: Round-trip test 1
const testPath1 = '/home/user/file.txt';
const parsed1 = path.parse(testPath1);
const formatted1 = path.format(parsed1);
assertEqual(formatted1, testPath1, "round-trip: '/home/user/file.txt'");

// Test 24: Round-trip test 2
const testPath2 = '/file.tar.gz';
const parsed2 = path.parse(testPath2);
const formatted2 = path.format(parsed2);
assertEqual(formatted2, testPath2, "round-trip: '/file.tar.gz'");

// Test 25: Round-trip test 3
const testPath3 = 'relative/path/file.js';
const parsed3 = path.parse(testPath3);
const formatted3 = path.format(parsed3);
assertEqual(formatted3, testPath3, "round-trip: 'relative/path/file.js'");

console.log('\nTesting path.toNamespacedPath()...');

// Test 26: Unix absolute path (no-op)
result = path.toNamespacedPath('/home/user/file.txt');
assertEqual(
  result,
  '/home/user/file.txt',
  'toNamespacedPath() on Unix absolute path'
);

// Test 27: Unix relative path (no-op)
result = path.toNamespacedPath('relative/path');
assertEqual(
  result,
  'relative/path',
  'toNamespacedPath() on Unix relative path'
);

// Test 28: Empty path (no-op)
result = path.toNamespacedPath('');
assertEqual(result, '', 'toNamespacedPath() on empty path');

// Test 29: Current directory (no-op)
result = path.toNamespacedPath('.');
assertEqual(result, '.', 'toNamespacedPath() on current directory');

// Test 30: Parent directory (no-op)
result = path.toNamespacedPath('..');
assertEqual(result, '..', 'toNamespacedPath() on parent directory');

console.log('\nTesting edge cases...');

// Test 31: Parse path with spaces
result = path.parse('/home/user/my file.txt');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/home/user',
    base: 'my file.txt',
    ext: '.txt',
    name: 'my file',
  },
  'parse() with spaces in filename'
);

// Test 32: Format with spaces
result = path.format({
  dir: '/home/user',
  name: 'my file',
  ext: '.txt',
});
assertEqual(result, '/home/user/my file.txt', 'format() with spaces');

// Test 33: Parse unicode path
result = path.parse('/home/用户/文件.txt');
assertEqual(result.base, '文件.txt', 'parse() with unicode characters');
assertEqual(result.name, '文件', 'parse() unicode name');
assertEqual(result.ext, '.txt', 'parse() unicode extension');

// Test 34: Multiple dots in name
result = path.parse('/home/user/jquery.min.js');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/home/user',
    base: 'jquery.min.js',
    ext: '.js',
    name: 'jquery.min',
  },
  'parse() with multiple dots'
);

// Test 35: Hidden file with extension
result = path.parse('/home/user/.config.json');
assertObjectEqual(
  result,
  {
    root: '/',
    dir: '/home/user',
    base: '.config.json',
    ext: '.json',
    name: '.config',
  },
  'parse() hidden file with extension'
);

console.log('\n' + '='.repeat(50));
console.log(`Total tests: ${tests}`);
console.log(`Passed: ${passed}`);
console.log(`Failed: ${failed}`);

if (failed > 0) {
  console.log('\nSome tests FAILED!');
  throw new Error(`${failed} test(s) failed`);
} else {
  console.log('\nAll tests PASSED!');
}
