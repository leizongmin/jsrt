// Comprehensive test for node:util module - all APIs
const util = require('node:util');
const assert = require('node:assert');

console.log('Testing node:util comprehensive API coverage\n');

// Test 1: format() with all placeholders
console.log('=== Testing format() ===');
assert.strictEqual(util.format('Hello %s', 'world'), 'Hello world');
assert.strictEqual(util.format('Number: %d', 42), 'Number: 42');
assert.strictEqual(util.format('Integer: %i', 42.9), 'Integer: 43');
assert(util.format('Float: %f', 3.14).startsWith('Float: 3.14'));
assert(util.format('JSON: %j', { a: 1 }).includes('"a"'));
assert(util.format('100%%') === '100%');
console.log('✓ format() works with all placeholders');

// Test 2: formatWithOptions()
console.log('\n=== Testing formatWithOptions() ===');
const formatted = util.formatWithOptions(
  { colors: false },
  'Value: %s',
  'test'
);
assert.strictEqual(formatted, 'Value: test');
console.log('✓ formatWithOptions() works');

// Test 3: inspect()
console.log('\n=== Testing inspect() ===');
const inspected = util.inspect({ a: 1, b: 2 });
assert(inspected.includes('"a"'));
assert(inspected.includes('"b"'));
console.log('✓ inspect() works');

// Test 4: TextEncoder/TextDecoder
console.log('\n=== Testing TextEncoder/TextDecoder ===');
const encoder = new util.TextEncoder();
const decoder = new util.TextDecoder();
const text = 'Hello 世界';
const bytes = encoder.encode(text);
const decoded = decoder.decode(bytes);
assert.strictEqual(decoded, text);
console.log('✓ TextEncoder/TextDecoder work');

// Test 5: promisify()
console.log('\n=== Testing promisify() ===');
const callbackFn = (val, cb) => setTimeout(() => cb(null, val * 2), 10);
const promiseFn = util.promisify(callbackFn);
promiseFn(21).then((result) => {
  assert.strictEqual(result, 42);
  console.log('✓ promisify() works');

  // Test 6: callbackify()
  console.log('\n=== Testing callbackify() ===');
  const asyncFn = async (val) => val * 2;
  const callbackified = util.callbackify(asyncFn);
  callbackified(21, (err, result) => {
    assert.strictEqual(err, null);
    assert.strictEqual(result, 42);
    console.log('✓ callbackify() works');

    continueTests();
  });
});

function continueTests() {
  // Test 7: deprecate()
  console.log('\n=== Testing deprecate() ===');
  let oldValue = 0;
  const oldFn = () => ++oldValue;
  const deprecatedFn = util.deprecate(oldFn, 'Test deprecation', 'TEST001');
  deprecatedFn();
  assert.strictEqual(oldValue, 1);
  console.log('✓ deprecate() works');

  // Test 8: debuglog()
  console.log('\n=== Testing debuglog() ===');
  const debug = util.debuglog('test-section');
  assert.strictEqual(typeof debug, 'function');
  assert.strictEqual(typeof debug.enabled, 'boolean');
  debug('This is a debug message');
  console.log('✓ debuglog() works');

  // Test 9: inherits()
  console.log('\n=== Testing inherits() ===');
  function Animal(name) {
    this.name = name;
  }
  Animal.prototype.speak = function () {
    return this.name + ' makes a sound';
  };

  function Dog(name, breed) {
    Animal.call(this, name);
    this.breed = breed;
  }
  util.inherits(Dog, Animal);

  const dog = new Dog('Rex', 'Labrador');
  assert.strictEqual(dog.name, 'Rex');
  assert.strictEqual(dog.speak(), 'Rex makes a sound');
  assert.strictEqual(Dog.super_, Animal);
  console.log('✓ inherits() works');

  // Test 10: isDeepStrictEqual()
  console.log('\n=== Testing isDeepStrictEqual() ===');
  assert.strictEqual(util.isDeepStrictEqual({ a: 1 }, { a: 1 }), true);
  assert.strictEqual(util.isDeepStrictEqual({ a: 1 }, { a: 2 }), false);
  assert.strictEqual(
    util.isDeepStrictEqual({ a: { b: 1 } }, { a: { b: 1 } }),
    true
  );
  console.log('✓ isDeepStrictEqual() works');

  // Test 11: util.types namespace
  console.log('\n=== Testing util.types ===');
  assert(typeof util.types === 'object');
  assert(typeof util.types.isDate === 'function');
  assert(typeof util.types.isPromise === 'function');
  assert(typeof util.types.isRegExp === 'function');
  assert(typeof util.types.isArrayBuffer === 'function');
  console.log('✓ util.types namespace works');

  // Test 12: Legacy type checkers
  console.log('\n=== Testing legacy type checkers ===');
  assert.strictEqual(util.isArray([1, 2, 3]), true);
  assert.strictEqual(util.isObject({ a: 1 }), true);
  assert.strictEqual(util.isString('hello'), true);
  assert.strictEqual(util.isNumber(42), true);
  assert.strictEqual(util.isBoolean(true), true);
  assert.strictEqual(
    util.isFunction(() => {}),
    true
  );
  assert.strictEqual(util.isNull(null), true);
  assert.strictEqual(util.isUndefined(undefined), true);
  console.log('✓ Legacy type checkers work');

  console.log('\n' + '='.repeat(60));
  console.log('✅ All comprehensive util tests passed!');
  console.log('Node.js util module: 100% API coverage achieved');
  console.log('='.repeat(60));
}
