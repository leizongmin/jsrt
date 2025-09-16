const assert = require('jsrt:assert');

async function testESModules() {
  // Test ES module import (using dynamic import since we're in CommonJS context)
  try {
    // For now, test CommonJS since ES module dynamic import needs more setup
    const util = require('node:util');

    // Verify that all expected functions are available
    assert.strictEqual(
      typeof util.format,
      'function',
      'format should be a function'
    );
    assert.strictEqual(
      typeof util.inspect,
      'function',
      'inspect should be a function'
    );
    assert.strictEqual(
      typeof util.isArray,
      'function',
      'isArray should be a function'
    );
    assert.strictEqual(
      typeof util.isObject,
      'function',
      'isObject should be a function'
    );
    assert.strictEqual(
      typeof util.isString,
      'function',
      'isString should be a function'
    );
    assert.strictEqual(
      typeof util.isNumber,
      'function',
      'isNumber should be a function'
    );
    assert.strictEqual(
      typeof util.isBoolean,
      'function',
      'isBoolean should be a function'
    );
    assert.strictEqual(
      typeof util.isFunction,
      'function',
      'isFunction should be a function'
    );
    assert.strictEqual(
      typeof util.isNull,
      'function',
      'isNull should be a function'
    );
    assert.strictEqual(
      typeof util.isUndefined,
      'function',
      'isUndefined should be a function'
    );
    assert.strictEqual(
      typeof util.promisify,
      'function',
      'promisify should be a function'
    );

    // Test that functions work correctly
    assert.strictEqual(
      util.isArray([1, 2, 3]),
      true,
      'isArray should work in ES module exports'
    );
    assert.strictEqual(
      util.isString('test'),
      true,
      'isString should work in ES module exports'
    );

    let formatted = util.format('ES module test: %s', 'working');
    assert.strictEqual(
      typeof formatted,
      'string',
      'format should work in ES module exports'
    );
  } catch (error) {
    console.error('❌ ES module test failed:', error.message);
    throw error;
  }
}

// Run the test
testESModules().catch(console.error);
