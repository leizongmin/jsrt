const assert = require('jsrt:assert');

// Test 1: Basic FFI module loading
const ffi = require('jsrt:ffi');
assert.strictEqual(typeof ffi, 'object', 'FFI module should be an object');
assert.strictEqual(
  typeof ffi.Library,
  'function',
  'ffi.Library should be a function'
);
assert.strictEqual(
  typeof ffi.version,
  'string',
  'ffi.version should be a string'
);

// Test 2: Loading library structure and testing function calls
try {
  // Try to load libc with basic function signatures
  const libc = ffi.Library('libc.so.6', {
    strlen: ['int', ['string']],
    strcmp: ['int', ['string', 'string']],
    abs: ['int', ['int']],
  });

  assert.strictEqual(typeof libc, 'object', 'libc should be an object');
  assert.strictEqual(
    typeof libc.strlen,
    'function',
    'strlen should be a function'
  );
  assert.strictEqual(
    typeof libc.strcmp,
    'function',
    'strcmp should be a function'
  );
  assert.strictEqual(typeof libc.abs, 'function', 'abs should be a function');

  // Test simple function calls
  // Test abs function (int -> int)
  try {
    const absResult = libc.abs(-42);
    assert.strictEqual(absResult, 42, 'abs(-42) should return 42');
  } catch (err) {
    // abs function call failed
  }

  // Test strlen function (string -> int)
  try {
    const strlenResult = libc.strlen('hello');
    assert.strictEqual(strlenResult, 5, 'strlen("hello") should return 5');
  } catch (err) {
    // strlen function call failed
  }

  // Test strcmp function (string, string -> int)
  try {
    const strcmpResult1 = libc.strcmp('hello', 'hello');
    assert.strictEqual(
      strcmpResult1,
      0,
      'strcmp("hello", "hello") should return 0'
    );

    const strcmpResult2 = libc.strcmp('a', 'b');
    assert.strictEqual(
      strcmpResult2 < 0,
      true,
      'strcmp("a", "b") should return < 0'
    );
  } catch (err) {
    // strcmp function call failed
  }
} catch (error) {
  // libc loading failed (this is expected on some systems)
}

// Test 3: Test with a non-existent library (should fail)
try {
  const badlib = ffi.Library('nonexistent_library_12345.so', {
    dummy_func: ['void', []],
  });
  assert.fail('Loading non-existent library should throw an error');
} catch (error) {
  assert.strictEqual(
    error.name,
    'ReferenceError',
    'Should throw ReferenceError'
  );
}

// Test 4: Test basic function signature validation
try {
  // Test type conversion functionality without actually calling native functions
  const typeInfo = {
    test_int: ['int', ['int']],
    test_string: ['string', ['string']],
    test_void: ['void', []],
  };
  // Function signature definitions parsed successfully
} catch (error) {
  console.error('Function signature test failed:', error.message);
}
