// Test simple missing assert APIs: fail, ifError, match, doesNotMatch
const assert = require('node:assert');

console.log('Testing simple assert APIs...');

// Test 1: assert.fail() with no message
try {
  assert.fail();
  console.log('✗ fail() should have thrown');
} catch (e) {
  if (e.name === 'AssertionError' && e.message === 'Failed') {
    console.log('✓ fail() with no message works');
  } else {
    console.log('✗ fail() error incorrect:', e.message);
  }
}

// Test 2: assert.fail() with custom message
try {
  assert.fail('custom failure message');
  console.log('✗ fail(message) should have thrown');
} catch (e) {
  if (e.message === 'custom failure message') {
    console.log('✓ fail(message) with custom message works');
  } else {
    console.log('✗ fail(message) error incorrect:', e.message);
  }
}

// Test 3: assert.ifError() with null (should not throw)
try {
  assert.ifError(null);
  console.log('✓ ifError(null) does not throw');
} catch (e) {
  console.log('✗ ifError(null) should not throw');
}

// Test 4: assert.ifError() with undefined (should not throw)
try {
  assert.ifError(undefined);
  console.log('✓ ifError(undefined) does not throw');
} catch (e) {
  console.log('✗ ifError(undefined) should not throw');
}

// Test 5: assert.ifError() with Error object
try {
  const err = new Error('test error');
  assert.ifError(err);
  console.log('✗ ifError(Error) should throw');
} catch (e) {
  if (e.message === 'test error') {
    console.log('✓ ifError(Error) throws the error');
  } else {
    console.log('✗ ifError(Error) threw wrong error:', e.message);
  }
}

// Test 6: assert.ifError() with non-Error truthy value
try {
  assert.ifError('some string');
  console.log('✗ ifError(string) should throw');
} catch (e) {
  if (e.name === 'AssertionError') {
    console.log('✓ ifError(truthy) throws AssertionError');
  } else {
    console.log('✗ ifError(truthy) threw wrong type:', e.name);
  }
}

// Test 7: assert.match() with matching string
try {
  assert.match('hello world', /world/);
  console.log('✓ match() with matching pattern succeeds');
} catch (e) {
  console.log('✗ match() with matching pattern should not throw:', e.message);
}

// Test 8: assert.match() with non-matching string
try {
  assert.match('hello', /world/);
  console.log('✗ match() with non-matching pattern should throw');
} catch (e) {
  if (e.name === 'AssertionError' && e.operator === 'match') {
    console.log('✓ match() with non-matching pattern throws');
  } else {
    console.log('✗ match() error incorrect');
  }
}

// Test 9: assert.match() with custom message
try {
  assert.match('hello', /world/, 'custom match error');
  console.log('✗ Should have thrown');
} catch (e) {
  if (e.message === 'custom match error' && e.generatedMessage === false) {
    console.log('✓ match() custom message works');
  } else {
    console.log(
      '✗ match() custom message incorrect:',
      e.message,
      e.generatedMessage
    );
  }
}

// Test 10: assert.doesNotMatch() with non-matching string
try {
  assert.doesNotMatch('hello', /world/);
  console.log('✓ doesNotMatch() with non-matching pattern succeeds');
} catch (e) {
  console.log('✗ doesNotMatch() should not throw:', e.message);
}

// Test 11: assert.doesNotMatch() with matching string
try {
  assert.doesNotMatch('hello world', /world/);
  console.log('✗ doesNotMatch() with matching pattern should throw');
} catch (e) {
  if (e.name === 'AssertionError' && e.operator === 'doesNotMatch') {
    console.log('✓ doesNotMatch() with matching pattern throws');
  } else {
    console.log('✗ doesNotMatch() error incorrect');
  }
}

// Test 12: Complex regex pattern
try {
  assert.match('test@example.com', /^[a-z]+@[a-z]+\.[a-z]+$/);
  console.log('✓ match() with complex pattern works');
} catch (e) {
  console.log('✗ match() with complex pattern failed:', e.message);
}

console.log('All simple assert API tests completed!');
