// Cross-platform compatible runtime tests
const assert = require('jsrt:assert');
const testValue = 123;

// Test arithmetic
assert.strictEqual(1 + 2, 3, 'Basic arithmetic should work');

// Test basic object creation
const testObj = { key: 'value' };
assert.strictEqual(testObj.key, 'value', 'Object property access should work');

// Test basic array functionality
const testArray = [1, 2, 3];
assert.strictEqual(testArray.length, 3, 'Array length should be correct');
