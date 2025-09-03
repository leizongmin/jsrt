const assert = require('jsrt:assert');

console.log('=== FFI Callback Tests ===');

// Test callback support
try {
  const ffi = require('jsrt:ffi');

  console.log('Test 1: Creating a simple callback');

  // Create a JavaScript function to be called from C
  function jsCallback(arg1) {
    console.log(`JavaScript callback called with: ${arg1}`);
    return arg1 * 2; // Double the input
  }

  // Create callback with signature: returns int, takes one int argument
  const callbackPtr = ffi.Callback(jsCallback, 'int', ['int']);

  assert.strictEqual(
    typeof callbackPtr,
    'number',
    'Callback should return a pointer'
  );
  assert.notStrictEqual(callbackPtr, 0, 'Callback pointer should not be null');

  console.log('✓ Callback created successfully');
  console.log('Callback pointer:', callbackPtr);

  console.log('Test 2: Testing callback with multiple arguments');

  function multiArgCallback(arg1, arg2) {
    console.log(`Multi-arg callback called with: ${arg1}, ${arg2}`);
    return arg1 + arg2;
  }

  const multiCallbackPtr = ffi.Callback(multiArgCallback, 'int', [
    'int',
    'int',
  ]);
  assert.strictEqual(
    typeof multiCallbackPtr,
    'number',
    'Multi-arg callback should return a pointer'
  );

  console.log('✓ Multi-argument callback created successfully');

  console.log('Test 3: Testing callback with different return types');

  function stringCallback(arg1) {
    return 'Hello from callback!';
  }

  const stringCallbackPtr = ffi.Callback(stringCallback, 'string', ['int']);
  assert.strictEqual(
    typeof stringCallbackPtr,
    'number',
    'String callback should return a pointer'
  );

  console.log('✓ String return callback created successfully');

  console.log('Test 4: Testing void callback');

  function voidCallback(arg1) {
    console.log(`Void callback called with: ${arg1}`);
    // No return value
  }

  const voidCallbackPtr = ffi.Callback(voidCallback, 'void', ['int']);
  assert.strictEqual(
    typeof voidCallbackPtr,
    'number',
    'Void callback should return a pointer'
  );

  console.log('✓ Void callback created successfully');

  console.log('Test 5: Testing error handling - invalid function');

  try {
    ffi.Callback('not a function', 'int', ['int']);
    assert.fail('Should have thrown an error for invalid function');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('First argument must be a JavaScript function'),
      true
    );
    console.log('✓ Error handling for invalid function works');
  }

  console.log('Test 6: Testing error handling - invalid argument types');

  try {
    ffi.Callback(jsCallback, 'int', 'not an array');
    assert.fail('Should have thrown an error for invalid argument types');
  } catch (error) {
    assert.strictEqual(
      error.message.includes('Argument types must be an array'),
      true
    );
    console.log('✓ Error handling for invalid argument types works');
  }

  console.log('Test 7: Testing FFI version includes callback support');
  assert.strictEqual(ffi.version, '3.0.0', 'FFI version should be 3.0.0');
  assert.strictEqual(
    ffi.types.callback,
    'callback',
    'Callback type should be available'
  );

  console.log('✓ All callback tests passed');
} catch (error) {
  console.error('❌ Callback test failed:', error.message);
  if (error.stack) {
    console.error('Stack trace:', error.stack);
  }
  throw error;
}

console.log('=== Callback Tests Completed ===');
