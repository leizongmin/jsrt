// Test Blob API
const assert = require('jsrt:assert');
// console.log('Testing Blob API...');

// Test basic Blob constructor
const blob1 = new Blob();
// Success case - no output needed
assert.strictEqual(typeof blob1, 'object', 'Blob should be an object');
// console.log('Empty blob size:', blob1.size);
assert.strictEqual(blob1.size, 0n, 'Empty blob should have size 0');
// console.log('Empty blob type:', blob1.type);
assert.strictEqual(blob1.type, '', 'Empty blob should have empty type');

// Test Blob with string array
const blob2 = new Blob(['hello', ' world']);
// Success case - no output needed
// console.log('String blob size:', blob2.size);
assert.strictEqual(blob2.size, 11n, 'String blob should have correct size');
// console.log('String blob type:', blob2.type);
assert.strictEqual(blob2.type, '', 'Default blob type should be empty');

// Test Blob with options
const blob3 = new Blob(['test'], { type: 'text/plain' });
// Success case - no output needed
// console.log('Typed blob size:', blob3.size);
assert.strictEqual(blob3.size, 4n, 'Typed blob should have correct size');
// console.log('Typed blob type:', blob3.type);
assert.strictEqual(
  blob3.type,
  'text/plain',
  'Blob type should be set correctly'
);

// Test Blob methods
const blob4 = new Blob(['hello world']);

// Test slice
const sliced = blob4.slice(0, 5);
// Success case - no output needed
assert.strictEqual(typeof sliced, 'object', 'slice() should return an object');
assert.strictEqual(sliced.size, 5n, 'Sliced blob should have correct size');
// console.log('Sliced blob size:', sliced.size);

// Test text() - now returns string directly
const text = blob4.text();
// Success case - no output needed
assert.strictEqual(typeof text, 'string', 'text() should return a string');
assert.strictEqual(text, 'hello world', 'text() should return correct content');

// Test arrayBuffer() - now returns ArrayBuffer directly
const buffer = blob4.arrayBuffer();
// Success case - no output needed
assert.strictEqual(
  typeof buffer,
  'object',
  'arrayBuffer() should return an object'
);
assert.strictEqual(
  buffer.byteLength,
  11,
  'arrayBuffer() should have correct byte length'
);

// Test stream() - should create ReadableStream
try {
  const stream = blob4.stream();
  // Success case - no output needed
} catch (err) {
  console.log(
    '⚠ Blob.stream() error (expected if ReadableStream not available):',
    err.message
  );
}
