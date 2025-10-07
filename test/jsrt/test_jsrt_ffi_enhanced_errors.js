const assert = require('jsrt:assert');

// Test enhanced error reporting with stack traces
try {
  const ffi = require('jsrt:ffi');

  try {
    ffi.malloc(0); // Invalid size
    assert.fail('Should have thrown an error');
  } catch (error) {
    assert.strictEqual(typeof error, 'object', 'Error should be an object');
    assert.strictEqual(
      typeof error.message,
      'string',
      'Error should have message'
    );
    assert.strictEqual(
      error.message.includes('FFI Error in ffi.malloc'),
      true,
      'Error should have function context'
    );
    assert.strictEqual(
      typeof error.stack,
      'string',
      'Error should have stack trace'
    );

    // Check for enhanced error properties
    if (error.ffiFunction) {
      assert.strictEqual(
        error.ffiFunction,
        'ffi.malloc',
        'Error should have FFI function property'
      );
    }
    if (error.ffiModule) {
      assert.strictEqual(
        error.ffiModule,
        'jsrt:ffi',
        'Error should have FFI module property'
      );
    }

    console.log('Error message:', error.message);
  }

  try {
    ffi.Library('nonexistent.so', {});
    assert.fail('Should have thrown an error for non-existent library');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('FFI Error in ffi.Library'),
      true,
      'Library error should have function context'
    );
    assert.strictEqual(
      error.message.includes('Failed to load library'),
      true,
      'Library error should have detailed message'
    );
    assert.strictEqual(
      error.message.includes('Please check'),
      true,
      'Library error should have troubleshooting suggestions'
    );
    assert.strictEqual(
      typeof error.stack,
      'string',
      'Library error should have stack trace'
    );

    console.log('Detailed error message:', error.message);
  }

  try {
    ffi.Library(); // Missing arguments
    assert.fail('Should have thrown an error for missing arguments');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('FFI Error in ffi.Library'),
      true,
      'Missing args error should have function context'
    );
    assert.strictEqual(
      error.message.includes('Expected 2 arguments'),
      true,
      'Missing args error should specify expected count'
    );
    assert.strictEqual(
      typeof error.stack,
      'string',
      'Missing args error should have stack trace'
    );
  }

  try {
    ffi.writeString(0, 'test'); // Invalid pointer for writeString
    assert.fail('Should have thrown an error for invalid pointer');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('FFI Error in ffi.writeString'),
      true,
      'Memory error should have function context'
    );
    assert.strictEqual(
      typeof error.stack,
      'string',
      'Memory error should have stack trace'
    );
  }

  try {
    ffi.arrayFromPointer(); // Missing arguments
    assert.fail('Should have thrown an error for missing arguments');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('FFI Error in ffi.arrayFromPointer'),
      true,
      'Array error should have function context'
    );
    assert.strictEqual(
      typeof error.stack,
      'string',
      'Array error should have stack trace'
    );
  }

  try {
    ffi.Callback(); // Missing arguments
    assert.fail('Should have thrown an error for missing arguments');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('FFI Error in ffi.Callback'),
      true,
      'Callback error should have function context'
    );
    assert.strictEqual(
      error.message.includes('Expected 3 arguments'),
      true,
      'Callback error should specify expected count'
    );
    assert.strictEqual(
      typeof error.stack,
      'string',
      'Callback error should have stack trace'
    );
  }

  try {
    ffi.callAsync(); // Missing arguments
    assert.fail('Should have thrown an error for missing arguments');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('FFI Error in ffi.callAsync'),
      true,
      'Async error should have function context'
    );
    assert.strictEqual(
      error.message.includes('Expected 4 arguments'),
      true,
      'Async error should specify expected count'
    );
    assert.strictEqual(
      typeof error.stack,
      'string',
      'Async error should have stack trace'
    );
  }

  try {
    ffi.Struct(); // Missing arguments
    assert.fail('Should have thrown an error for missing arguments');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('FFI Error in ffi.Struct'),
      true,
      'Struct error should have function context'
    );
    assert.strictEqual(
      error.message.includes('Expected 2 arguments'),
      true,
      'Struct error should specify expected count'
    );
    assert.strictEqual(
      typeof error.stack,
      'string',
      'Struct error should have stack trace'
    );
  }

  try {
    ffi.allocStruct(); // Missing arguments
    assert.fail('Should have thrown an error for missing arguments');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('FFI Error in ffi.allocStruct'),
      true,
      'allocStruct error should have function context'
    );
    assert.strictEqual(
      error.message.includes('Expected 1 argument'),
      true,
      'allocStruct error should specify expected count'
    );
    assert.strictEqual(
      typeof error.stack,
      'string',
      'allocStruct error should have stack trace'
    );
  }

  const errorFunctions = [
    () => ffi.malloc(0),
    () => ffi.Library(),
    () => ffi.Callback(),
    () => ffi.callAsync(),
    () => ffi.Struct(),
    () => ffi.allocStruct(),
    () => ffi.arrayFromPointer(),
  ];

  let errorCount = 0;
  for (const func of errorFunctions) {
    try {
      func();
    } catch (error) {
      assert.strictEqual(
        error.message.startsWith('FFI Error in'),
        true,
        'All errors should start with "FFI Error in"'
      );
      assert.strictEqual(
        typeof error.stack,
        'string',
        'All errors should have stack traces'
      );
      errorCount++;
    }
  }

  assert.strictEqual(
    errorCount,
    errorFunctions.length,
    'All test functions should throw errors'
  );
  function testFunction() {
    try {
      ffi.malloc(-1); // Invalid size
    } catch (error) {
      // Check that stack trace contains this function name
      assert.strictEqual(
        typeof error.stack,
        'string',
        'Error should have stack trace'
      );
      assert.strictEqual(
        error.stack.length > 0,
        true,
        'Stack trace should not be empty'
      );

      console.log('Sample stack trace:');
      console.log(error.stack.split('\n').slice(0, 3).join('\n') + '...');

      throw error; // Re-throw to be caught by outer try-catch
    }
  }

  try {
    testFunction();
    assert.fail('Should have thrown an error');
  } catch (error) {
    // Expected
  }
} catch (error) {
  console.error('❌ Enhanced error reporting test failed:', error.message);
  if (error.stack) {
    console.error('Stack trace:', error.stack);
  }
  throw error;
}
