// Test npm module support
const assert = require('jsrt:assert');
console.log('=== NPM Module Loading Tests ===');

// Test 1: lodash (popular utility library - exports as function)
console.log('\nTest 1: Loading lodash package');
const _ = require('lodash');
assert.strictEqual(typeof _, 'function', 'lodash should be a function');
assert.strictEqual(typeof _.map, 'function', 'lodash.map should be a function');
assert.strictEqual(
  typeof _.filter,
  'function',
  'lodash.filter should be a function'
);
console.log('✓ Lodash package loaded successfully');
console.log(
  '  lodash.map test:',
  _.map([1, 2, 3], (x) => x * 2)
);

// Test 2: moment (date utility - exports as function)
console.log('\nTest 2: Loading moment package');
const moment = require('moment');
assert.strictEqual(typeof moment, 'function', 'moment should be a function');
const now = moment();
assert.strictEqual(
  typeof now.format,
  'function',
  'moment instance should have format method'
);
console.log('✓ Moment package loaded successfully');
console.log('  current time:', now.format('YYYY-MM-DD HH:mm:ss'));

// Test 3: async (async utilities - exports as object)
console.log('\nTest 3: Loading async package');
const async = require('async');
assert.strictEqual(typeof async, 'object', 'async should be an object');
assert.strictEqual(
  typeof async.map,
  'function',
  'async.map should be a function'
);
assert.strictEqual(
  typeof async.series,
  'function',
  'async.series should be a function'
);
console.log('✓ Async package loaded successfully');

// Test 4: Module caching - require same module again
console.log('\nTest 4: Module caching');
const _2 = require('lodash');
assert.strictEqual(_, _2, 'Cached modules should be the same instance');
console.log('✓ Module caching works correctly');

// Test 5: Non-existent package
console.log('\nTest 5: Non-existent package handling');
try {
  require('non-existent-package');
  assert.fail('Should have thrown an error for non-existent package');
} catch (error) {
  assert.ok(
    error.message.includes('Cannot find module'),
    'Should throw appropriate error message'
  );
  console.log('✓ Non-existent package handled correctly');
}

console.log('\n=== All npm module CommonJS tests passed ===');
