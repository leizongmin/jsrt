// Test CommonJS require
const assert = require('jsrt:assert');
// console.log('=== CommonJS Module Tests ===');

// Test 1: Basic require
const { greet, data } = require('./test/commonjs_module.js');
const greeting = greet('World');
assert.strictEqual(typeof greet, 'function', 'greet should be a function');
assert.strictEqual(typeof greeting, 'string', 'greet should return a string');
assert.strictEqual(typeof data, 'object', 'data should be an object');
console.log('greet("World"):', greeting);
console.log('data:', data);

// Test 2: Nested require (require the same module again)
console.log('\nTest 2: Nested require');
const mod2 = require('./test/commonjs_module.js');
const greeting2 = mod2.greet('Again');
assert.strictEqual(
  typeof mod2.greet,
  'function',
  'mod2.greet should be a function'
);
assert.strictEqual(
  typeof greeting2,
  'string',
  'mod2.greet should return a string'
);
assert.strictEqual(typeof mod2.data, 'object', 'mod2.data should be an object');
console.log('mod2.greet("Again"):', greeting2);
console.log('mod2.data:', mod2.data);

// console.log('\n=== All CommonJS tests completed ===');
