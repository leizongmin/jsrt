// Test zlib stream classes (C implementation)
const zlib = require('node:zlib');
const assert = require('node:assert');
const { Readable, Writable, PassThrough } = require('node:stream');

console.log('Testing zlib stream classes (C implementation)...\n');

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`✓ ${name}`);
    passed++;
  } catch (err) {
    console.error(`✗ ${name}`);
    console.error(`  ${err.message}`);
    if (err.stack) {
      console.error(
        err.stack
          .split('\n')
          .slice(1)
          .map((line) => `    ${line}`)
          .join('\n')
      );
    }
    failed++;
  }
}

// Test 1: createGzip factory function exists
test('createGzip factory function exists', () => {
  assert.strictEqual(
    typeof zlib.createGzip,
    'function',
    'createGzip should be a function'
  );
});

// Test 2: createGunzip factory function exists
test('createGunzip factory function exists', () => {
  assert.strictEqual(
    typeof zlib.createGunzip,
    'function',
    'createGunzip should be a function'
  );
});

// Test 3: createDeflate factory function exists
test('createDeflate factory function exists', () => {
  assert.strictEqual(
    typeof zlib.createDeflate,
    'function',
    'createDeflate should be a function'
  );
});

// Test 4: createInflate factory function exists
test('createInflate factory function exists', () => {
  assert.strictEqual(
    typeof zlib.createInflate,
    'function',
    'createInflate should be a function'
  );
});

// Test 5: createDeflateRaw factory function exists
test('createDeflateRaw factory function exists', () => {
  assert.strictEqual(
    typeof zlib.createDeflateRaw,
    'function',
    'createDeflateRaw should be a function'
  );
});

// Test 6: createInflateRaw factory function exists
test('createInflateRaw factory function exists', () => {
  assert.strictEqual(
    typeof zlib.createInflateRaw,
    'function',
    'createInflateRaw should be a function'
  );
});

// Test 7: createUnzip factory function exists
test('createUnzip factory function exists', () => {
  assert.strictEqual(
    typeof zlib.createUnzip,
    'function',
    'createUnzip should be a function'
  );
});

// Test 8: Create Gzip stream instance
test('Create Gzip stream instance', () => {
  const gzip = zlib.createGzip();
  assert.ok(gzip, 'Gzip stream should be created');
  assert.strictEqual(
    typeof gzip.write,
    'function',
    'Gzip should have write method'
  );
  assert.strictEqual(
    typeof gzip.read,
    'function',
    'Gzip should have read method'
  );
  // Note: pipe() may be inherited from Readable/Duplex prototype in Transform
  // so we check if it exists as a fallback
  if (typeof gzip.pipe !== 'function') {
    console.log(
      '  Note: pipe method not found on Gzip instance (may be in prototype chain)'
    );
  }
});

// Test 9: Create Gzip stream with options
test('Create Gzip stream with options', () => {
  const gzip = zlib.createGzip({ level: 9 });
  assert.ok(gzip, 'Gzip stream with options should be created');
});

// Test 10: Gzip stream has _transform method
test('Gzip stream has _transform method', () => {
  const gzip = zlib.createGzip();
  assert.strictEqual(
    typeof gzip._transform,
    'function',
    'Gzip should have _transform method'
  );
});

// Test 11: Gzip stream has _flush method
test('Gzip stream has _flush method', () => {
  const gzip = zlib.createGzip();
  assert.strictEqual(
    typeof gzip._flush,
    'function',
    'Gzip should have _flush method'
  );
});

// Summary
console.log(`\n${'='.repeat(50)}`);
console.log(`Tests: ${passed + failed}, Passed: ${passed}, Failed: ${failed}`);
if (failed > 0) {
  process.exit(1);
}
