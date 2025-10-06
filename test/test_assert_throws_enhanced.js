// Test enhanced assert.throws() validation
const assert = require('node:assert');

// Test basic throws without validation
const err = assert.throws(() => {
  throw new Error('test error');
});
assert.strictEqual(err.message, 'test error', 'Should return the error');

// Test Error constructor validation
assert.throws(() => {
  throw new Error('test');
}, Error);

// Test TypeError validation
assert.throws(() => {
  throw new TypeError('type error');
}, TypeError);

// Test wrong error type should fail
try {
  assert.throws(() => {
    throw new Error('regular error');
  }, TypeError);
  throw new Error('Should have thrown');
} catch (e) {
  if (e.name !== 'AssertionError') throw e;
}

// Test string message validation
assert.throws(() => {
  throw new Error('specific message');
}, 'specific');

// Test RegExp validation
assert.throws(() => {
  throw new Error('foo bar baz');
}, /bar/);

// Test RegExp that doesn't match
try {
  assert.throws(() => {
    throw new Error('hello');
  }, /goodbye/);
  throw new Error('Should have thrown');
} catch (e) {
  if (e.name !== 'AssertionError') throw e;
}

// Test object property validation
assert.throws(
  () => {
    const err = new Error('test');
    err.code = 'ETEST';
    throw err;
  },
  { code: 'ETEST' }
);

// Test custom message with validation
assert.throws(
  () => {
    throw new TypeError('type error');
  },
  TypeError,
  'Custom assertion message'
);

// Test function doesn't throw
try {
  assert.throws(() => {
    // No throw
  });
  throw new Error('Should have thrown');
} catch (e) {
  if (e.name !== 'AssertionError') throw e;
}

console.log('✓ All enhanced throws() tests passed');
