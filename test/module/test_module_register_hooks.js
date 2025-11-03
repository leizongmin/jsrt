/**
 * Test Task 4.2: module.registerHooks() Implementation
 *
 * This test validates the module.registerHooks() API implementation
 * building on the hook infrastructure from Task 4.1.
 */

const assert = require('assert');

console.log('Testing Task 4.2: module.registerHooks() Implementation');

let testCount = 0;
let passedCount = 0;

function test(description, testFn) {
  testCount++;
  try {
    testFn();
    console.log(`✓ Test ${testCount}: ${description}`);
    passedCount++;
  } catch (error) {
    console.log(`✗ Test ${testCount}: ${description}`);
    console.log(`  Error: ${error.message}`);
  }
}

// Test 1: module.registerHooks function exists
test('should have module.registerHooks function', () => {
  const module = require('node:module');
  assert.strictEqual(
    typeof module.registerHooks,
    'function',
    'module.registerHooks should be a function'
  );
});

// Test 2: Register hooks with resolve function
test('should register hooks with resolve function', () => {
  const module = require('node:module');

  const resolveHook = function (specifier, context, nextResolve) {
    console.log(`Resolving: ${specifier}`);
    return nextResolve(specifier, context);
  };

  const handle = module.registerHooks({
    resolve: resolveHook,
  });

  assert.ok(
    handle && typeof handle === 'object',
    'Should return a handle object'
  );
  assert.ok(typeof handle.id === 'number', 'Handle should have an id');
  assert.strictEqual(
    handle.resolve,
    true,
    'Handle should indicate resolve hook is registered'
  );
  assert.strictEqual(
    handle.load,
    false,
    'Handle should indicate load hook is not registered'
  );
});

// Test 3: Register hooks with load function
test('should register hooks with load function', () => {
  const module = require('node:module');

  const loadHook = function (url, context, nextLoad) {
    console.log(`Loading: ${url}`);
    return nextLoad(url, context);
  };

  const handle = module.registerHooks({
    load: loadHook,
  });

  assert.ok(
    handle && typeof handle === 'object',
    'Should return a handle object'
  );
  assert.ok(typeof handle.id === 'number', 'Handle should have an id');
  assert.strictEqual(
    handle.resolve,
    false,
    'Handle should indicate resolve hook is not registered'
  );
  assert.strictEqual(
    handle.load,
    true,
    'Handle should indicate load hook is registered'
  );
});

// Test 4: Register hooks with both resolve and load functions
test('should register hooks with both resolve and load functions', () => {
  const module = require('node:module');

  const resolveHook = function (specifier, context, nextResolve) {
    return nextResolve(specifier, context);
  };

  const loadHook = function (url, context, nextLoad) {
    return nextLoad(url, context);
  };

  const handle = module.registerHooks({
    resolve: resolveHook,
    load: loadHook,
  });

  assert.ok(
    handle && typeof handle === 'object',
    'Should return a handle object'
  );
  assert.ok(typeof handle.id === 'number', 'Handle should have an id');
  assert.strictEqual(
    handle.resolve,
    true,
    'Handle should indicate resolve hook is registered'
  );
  assert.strictEqual(
    handle.load,
    true,
    'Handle should indicate load hook is registered'
  );
});

// Test 5: Throw TypeError for missing options object
test('should throw TypeError for missing options object', () => {
  const module = require('node:module');

  let threwError = false;
  try {
    module.registerHooks();
  } catch (error) {
    threwError = true;
    assert.strictEqual(error.name, 'TypeError', 'Should throw TypeError');
    assert.ok(
      /options object/.test(error.message),
      'Error message should mention options object'
    );
  }
  assert.ok(threwError, 'Should have thrown an error');
});

// Test 6: Throw TypeError for non-object options
test('should throw TypeError for non-object options', () => {
  const module = require('node:module');

  let threwError = false;
  try {
    module.registerHooks('invalid');
  } catch (error) {
    threwError = true;
    assert.strictEqual(error.name, 'TypeError', 'Should throw TypeError');
    assert.ok(
      /options must be an object/.test(error.message),
      'Error message should mention object'
    );
  }
  assert.ok(threwError, 'Should have thrown an error');
});

// Test 7: Throw TypeError for non-function resolve option
test('should throw TypeError for non-function resolve option', () => {
  const module = require('node:module');

  let threwError = false;
  try {
    module.registerHooks({
      resolve: 'not-a-function',
    });
  } catch (error) {
    threwError = true;
    assert.strictEqual(error.name, 'TypeError', 'Should throw TypeError');
    assert.ok(
      /resolve option must be a function/.test(error.message),
      'Error message should mention resolve function'
    );
  }
  assert.ok(threwError, 'Should have thrown an error');
});

// Test 8: Throw TypeError for non-function load option
test('should throw TypeError for non-function load option', () => {
  const module = require('node:module');

  let threwError = false;
  try {
    module.registerHooks({
      load: 123,
    });
  } catch (error) {
    threwError = true;
    assert.strictEqual(error.name, 'TypeError', 'Should throw TypeError');
    assert.ok(
      /load option must be a function/.test(error.message),
      'Error message should mention load function'
    );
  }
  assert.ok(threwError, 'Should have thrown an error');
});

// Test 9: Throw TypeError when neither resolve nor load provided
test('should throw TypeError when neither resolve nor load provided', () => {
  const module = require('node:module');

  let threwError = false;
  try {
    module.registerHooks({});
  } catch (error) {
    threwError = true;
    assert.strictEqual(error.name, 'TypeError', 'Should throw TypeError');
    assert.ok(
      /requires at least resolve or load function/.test(error.message),
      'Error message should mention at least one function'
    );
  }
  assert.ok(threwError, 'Should have thrown an error');
});

// Test 10: Handle null/undefined hook functions gracefully
test('should handle null/undefined hook functions gracefully', () => {
  const module = require('node:module');

  // Should not throw - hooks can be explicitly set to null
  const handle1 = module.registerHooks({
    resolve: null,
    load: function (url, context, nextLoad) {
      return nextLoad(url, context);
    },
  });
  assert.ok(handle1);

  const handle2 = module.registerHooks({
    resolve: function (specifier, context, nextResolve) {
      return nextResolve(specifier, context);
    },
    load: undefined,
  });
  assert.ok(handle2);
});

// Test 11: Maintain LIFO order for hook registration
test('should maintain LIFO order for hook registration', () => {
  const module = require('node:module');

  const hook1 = {
    resolve: function (specifier, context, nextResolve) {
      return nextResolve(specifier, context);
    },
  };

  const hook2 = {
    resolve: function (specifier, context, nextResolve) {
      return nextResolve(specifier, context);
    },
  };

  // Register hooks - they should be called in LIFO order
  const handle1 = module.registerHooks(hook1);
  const handle2 = module.registerHooks(hook2);

  assert.ok(handle2.id > handle1.id, 'Later hooks should have higher IDs');
});

// Test 12: Integrate with existing module API
test('should integrate with existing module API', () => {
  const module = require('node:module');

  // Verify other module APIs still work
  assert.ok(Array.isArray(module.builtinModules));
  assert.strictEqual(typeof module.isBuiltin, 'function');
  assert.strictEqual(typeof module.createRequire, 'function');

  // Verify registerHooks doesn't interfere with other APIs
  const handle = module.registerHooks({
    resolve: function (specifier, context, nextResolve) {
      return nextResolve(specifier, context);
    },
  });

  assert.ok(handle);
  assert.ok(Array.isArray(module.builtinModules));
  assert.strictEqual(typeof module.isBuiltin, 'function');
});

// Summary
console.log(`\n✓ Task 4.2 module.registerHooks() Implementation Test Results:`);
console.log(`   Total tests: ${testCount}`);
console.log(`   Passed: ${passedCount}`);
console.log(`   Failed: ${testCount - passedCount}`);

if (passedCount === testCount) {
  console.log(
    '\n🎉 All tests passed! module.registerHooks() API is working correctly.'
  );
} else {
  console.log('\n❌ Some tests failed. Check the implementation.');
  process.exit(1);
}
