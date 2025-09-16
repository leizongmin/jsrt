// Test npm module support
const assert = require('jsrt:assert');

// Test 1: lodash (popular utility library - exports as function)
const _ = require('lodash');
assert.strictEqual(typeof _, 'function', 'lodash should be a function');
assert.strictEqual(typeof _.map, 'function', 'lodash.map should be a function');
assert.strictEqual(
  typeof _.filter,
  'function',
  'lodash.filter should be a function'
);

// Test 2: moment (date utility - exports as function)
const moment = require('moment');
assert.strictEqual(typeof moment, 'function', 'moment should be a function');
const now = moment();
assert.strictEqual(
  typeof now.format,
  'function',
  'moment instance should have format method'
);

// Test 3: async (async utilities - exports as object)
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

// Test 4: Module caching - require same module again
const _2 = require('lodash');
assert.strictEqual(_, _2, 'Cached modules should be the same instance');

// Test 5: Non-existent package
try {
  require('non-existent-package');
  assert.fail('Should have thrown an error for non-existent package');
} catch (error) {
  assert.ok(
    error.message.includes('Cannot find module'),
    'Should throw appropriate error message'
  );
}
