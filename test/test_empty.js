// Cross-platform compatible runtime tests
const assert = require('jsrt:assert');
const testValue = 123;

console.log('Runtime test - basic functionality check');
console.log('Test value:', testValue);

// Test arithmetic
assert.strictEqual(1 + 2, 3, 'Basic arithmetic should work');
console.log('Arithmetic test: PASS');

// Test basic object creation
const testObj = { key: 'value' };
assert.strictEqual(testObj.key, 'value', 'Object property access should work');
console.log('Object test: PASS');

// Test basic array functionality
const testArray = [1, 2, 3];
assert.strictEqual(testArray.length, 3, 'Array length should be correct');
console.log('Array test: PASS');

console.log('Runtime tests completed successfully');
