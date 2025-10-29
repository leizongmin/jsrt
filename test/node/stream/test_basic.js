const assert = require('jsrt:assert');

// // console.log('=== Node.js Stream Tests ===');

// Test CommonJS import
const stream = require('node:stream');
assert.ok(stream, 'Should be able to require node:stream');
assert.ok(stream.Readable, 'Should have Readable class');
assert.ok(stream.Writable, 'Should have Writable class');
assert.ok(stream.Transform, 'Should have Transform class');
assert.ok(stream.PassThrough, 'Should have PassThrough class');

// Test named destructuring
const { Readable, Writable, Transform, PassThrough } = require('node:stream');
assert.ok(Readable, 'Should have Readable via destructuring');
assert.ok(Writable, 'Should have Writable via destructuring');
assert.ok(Transform, 'Should have Transform via destructuring');
assert.ok(PassThrough, 'Should have PassThrough via destructuring');

// console.log('✓ Module imports work correctly');

// Test Readable stream
console.log('\nTesting Readable stream...');
const readable = new Readable();
assert.ok(readable, 'Should create Readable instance');
assert.strictEqual(readable.readable, true, 'Should be readable');
assert.strictEqual(readable.destroyed, false, 'Should not be destroyed');

// Test read/push
assert.strictEqual(readable.read(), null, 'Should return null when no data');
readable.push('hello');
assert.strictEqual(readable.read(), 'hello', 'Should return pushed data');
assert.strictEqual(readable.read(), null, 'Should return null after reading');

// Test end stream
readable.push(null);
assert.strictEqual(
  readable.readable,
  false,
  'Should not be readable after end'
);

// console.log('✓ Readable stream works correctly');

// Test Writable stream
console.log('\nTesting Writable stream...');
const writable = new Writable();
assert.ok(writable, 'Should create Writable instance');
assert.strictEqual(writable.writable, true, 'Should be writable');
assert.strictEqual(writable.destroyed, false, 'Should not be destroyed');

// Test write
assert.strictEqual(writable.write('test'), true, 'Should write successfully');
writable.end();
assert.strictEqual(
  writable.writable,
  false,
  'Should not be writable after end'
);

// console.log('✓ Writable stream works correctly');

// Test PassThrough stream
console.log('\nTesting PassThrough stream...');
const passthrough = new PassThrough();
assert.ok(passthrough, 'Should create PassThrough instance');
assert.strictEqual(passthrough.readable, true, 'Should be readable');
assert.strictEqual(passthrough.writable, true, 'Should be writable');

// Test write and read
assert.strictEqual(
  passthrough.write('data'),
  true,
  'Should write successfully'
);
assert.strictEqual(passthrough.read(), 'data', 'Should read written data');

// console.log('✓ PassThrough stream works correctly');

// Test Transform stream (alias for PassThrough)
console.log('\nTesting Transform stream...');
const transform = new Transform();
assert.ok(transform, 'Should create Transform instance');
assert.strictEqual(transform.readable, true, 'Should be readable');
assert.strictEqual(transform.writable, true, 'Should be writable');

// Verify util.inherits() compatibility
const util = require('node:util');
function CustomTransform() {
  Transform.call(this);
}
util.inherits(CustomTransform, Transform);
const inherited = new CustomTransform();
assert.ok(
  inherited instanceof Transform,
  'Custom transform should inherit from Transform'
);
assert.strictEqual(
  typeof inherited.write,
  'function',
  'Custom transform exposes writable methods'
);

// console.log('✓ Transform stream works correctly');

// console.log('\n=== All Node.js Stream tests passed! ===');
