const assert = require('jsrt:assert');

// // console.log('=== Node.js util Module Tests ===');

// Test CommonJS import
const util = require('node:util');
// // console.log('✓ CommonJS require("node:util") successful');

// Test util.format()
console.log('\n🔧 Testing util.format():');
let formatted = util.format('Hello %s', 'world');
console.log('  util.format("Hello %s", "world"):', formatted);
assert.strictEqual(typeof formatted, 'string', 'format should return string');

formatted = util.format('Number: %d', 42);
console.log('  util.format("Number: %d", 42):', formatted);

formatted = util.format('Multiple', 'args', 123);
console.log('  util.format("Multiple", "args", 123):', formatted);

// Test util.inspect()
console.log('\n🔍 Testing util.inspect():');
let obj = { name: 'test', value: 42, nested: { a: 1, b: 2 } };
let inspected = util.inspect(obj);
console.log('  util.inspect(object):', inspected);
assert.strictEqual(typeof inspected, 'string', 'inspect should return string');

// Test type checking functions
console.log('\n🧪 Testing type checking functions:');

// util.isArray()
assert.strictEqual(
  util.isArray([1, 2, 3]),
  true,
  'isArray should return true for arrays'
);
assert.strictEqual(
  util.isArray('not array'),
  false,
  'isArray should return false for non-arrays'
);
// console.log('  ✓ util.isArray() works correctly');

// util.isObject()
assert.strictEqual(
  util.isObject({}),
  true,
  'isObject should return true for objects'
);
assert.strictEqual(
  util.isObject([]),
  false,
  'isObject should return false for arrays'
);
assert.strictEqual(
  util.isObject('string'),
  false,
  'isObject should return false for strings'
);
// console.log('  ✓ util.isObject() works correctly');

// util.isString()
assert.strictEqual(
  util.isString('hello'),
  true,
  'isString should return true for strings'
);
assert.strictEqual(
  util.isString(123),
  false,
  'isString should return false for numbers'
);
// console.log('  ✓ util.isString() works correctly');

// util.isNumber()
assert.strictEqual(
  util.isNumber(123),
  true,
  'isNumber should return true for numbers'
);
assert.strictEqual(
  util.isNumber('123'),
  false,
  'isNumber should return false for string numbers'
);
// console.log('  ✓ util.isNumber() works correctly');

// util.isBoolean()
assert.strictEqual(
  util.isBoolean(true),
  true,
  'isBoolean should return true for booleans'
);
assert.strictEqual(
  util.isBoolean(false),
  true,
  'isBoolean should return true for false'
);
assert.strictEqual(
  util.isBoolean(1),
  false,
  'isBoolean should return false for numbers'
);
// console.log('  ✓ util.isBoolean() works correctly');

// util.isFunction()
function testFn() {}
assert.strictEqual(
  util.isFunction(testFn),
  true,
  'isFunction should return true for functions'
);
assert.strictEqual(
  util.isFunction('string'),
  false,
  'isFunction should return false for non-functions'
);
// console.log('  ✓ util.isFunction() works correctly');

// util.isNull()
assert.strictEqual(
  util.isNull(null),
  true,
  'isNull should return true for null'
);
assert.strictEqual(
  util.isNull(undefined),
  false,
  'isNull should return false for undefined'
);
assert.strictEqual(
  util.isNull(0),
  false,
  'isNull should return false for falsy values'
);
// console.log('  ✓ util.isNull() works correctly');

// util.isUndefined()
assert.strictEqual(
  util.isUndefined(undefined),
  true,
  'isUndefined should return true for undefined'
);
assert.strictEqual(
  util.isUndefined(),
  true,
  'isUndefined should return true for no arguments'
);
assert.strictEqual(
  util.isUndefined(null),
  false,
  'isUndefined should return false for null'
);
// console.log('  ✓ util.isUndefined() works correctly');

// Test util.promisify()
console.log('\n⚡ Testing util.promisify():');
function callbackFn(arg, callback) {
  callback(null, 'result: ' + arg);
}
let promisified = util.promisify(callbackFn);
assert.strictEqual(
  typeof promisified,
  'function',
  'promisify should return a function'
);
// console.log('  ✓ util.promisify() returns a function');

// Success case - no output
console.log('📦 node:util module is working correctly');
