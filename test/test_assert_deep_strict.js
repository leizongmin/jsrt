// Test deepStrictEqual and notDeepStrictEqual
const assert = require('node:assert');

// Test NaN equality (Object.is semantics)
assert.deepStrictEqual(NaN, NaN, 'NaN should equal NaN');

// Test -0 vs +0 (Object.is semantics)
try {
  assert.deepStrictEqual(-0, +0);
  throw new Error('Should have thrown');
} catch (e) {
  if (e.name !== 'AssertionError') throw e;
}

// Test simple objects
assert.deepStrictEqual({ a: 1, b: 2 }, { a: 1, b: 2 });

// Test different key order
assert.deepStrictEqual({ a: 1, b: 2 }, { b: 2, a: 1 });

// Test nested objects
assert.deepStrictEqual({ a: { b: { c: 1 } } }, { a: { b: { c: 1 } } });

// Test arrays
assert.deepStrictEqual([1, 2, 3], [1, 2, 3]);
assert.deepStrictEqual([1, [2, [3]]], [1, [2, [3]]]);

// Test Date objects
const d1 = new Date('2025-01-01');
const d2 = new Date('2025-01-01');
assert.deepStrictEqual(d1, d2);

// Test RegExp objects
assert.deepStrictEqual(/abc/gi, /abc/gi);

// Test RegExp with different flags
try {
  assert.deepStrictEqual(/abc/g, /abc/i);
  throw new Error('Should have thrown');
} catch (e) {
  if (e.name !== 'AssertionError') throw e;
}

// Test circular references
const obj1 = { a: 1 };
obj1.self = obj1;
const obj2 = { a: 1 };
obj2.self = obj2;
assert.deepStrictEqual(obj1, obj2);

// Test objects with NaN property
assert.deepStrictEqual({ a: NaN }, { a: NaN });

// Test notDeepStrictEqual
assert.notDeepStrictEqual({ a: 1 }, { a: 2 });

try {
  assert.notDeepStrictEqual({ a: 1 }, { a: 1 });
  throw new Error('Should have thrown');
} catch (e) {
  if (e.name !== 'AssertionError') throw e;
}

// Test undefined vs missing property
try {
  assert.deepStrictEqual({ a: undefined }, {});
  throw new Error('Should have thrown');
} catch (e) {
  if (e.name !== 'AssertionError') throw e;
}

// Test null values
assert.deepStrictEqual({ a: null }, { a: null });

// Test different types
try {
  assert.deepStrictEqual({ a: 1 }, { a: '1' });
  throw new Error('Should have thrown');
} catch (e) {
  if (e.name !== 'AssertionError') throw e;
}

// Test empty objects and arrays
assert.deepStrictEqual({}, {});
assert.deepStrictEqual([], []);

console.log('✓ All deepStrictEqual tests passed');
