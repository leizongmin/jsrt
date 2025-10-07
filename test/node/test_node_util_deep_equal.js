// Test file for util.isDeepStrictEqual()
const util = require('node:util');
const assert = require('jsrt:assert');

console.log('Testing util.isDeepStrictEqual()\n');

// Test 1: Primitive values
console.log('=== Test 1: Primitive values ===');
assert.strictEqual(util.isDeepStrictEqual(1, 1), true, '1 === 1');
assert.strictEqual(util.isDeepStrictEqual('test', 'test'), true, 'strings equal');
assert.strictEqual(util.isDeepStrictEqual(true, true), true, 'booleans equal');
assert.strictEqual(util.isDeepStrictEqual(null, null), true, 'null === null');
assert.strictEqual(util.isDeepStrictEqual(1, 2), false, '1 !== 2');
assert.strictEqual(util.isDeepStrictEqual(1, '1'), false, 'strict mode');
console.log('✓ Primitive values work');

// Test 2: Simple objects
console.log('\n=== Test 2: Simple objects ===');
assert.strictEqual(util.isDeepStrictEqual({a: 1}, {a: 1}), true, 'Simple objects equal');
assert.strictEqual(util.isDeepStrictEqual({a: 1, b: 2}, {a: 1, b: 2}), true, 'Objects with multiple keys');
assert.strictEqual(util.isDeepStrictEqual({a: 1}, {a: 2}), false, 'Different values');
assert.strictEqual(util.isDeepStrictEqual({a: 1}, {b: 1}), false, 'Different keys');
assert.strictEqual(util.isDeepStrictEqual({a: 1}, {a: 1, b: 2}), false, 'Different number of keys');
console.log('✓ Simple objects work');

// Test 3: Nested objects
console.log('\n=== Test 3: Nested objects ===');
assert.strictEqual(
  util.isDeepStrictEqual({a: {b: 1}}, {a: {b: 1}}),
  true,
  'Nested objects equal'
);
assert.strictEqual(
  util.isDeepStrictEqual({a: {b: {c: 1}}}, {a: {b: {c: 1}}}),
  true,
  'Deeply nested objects equal'
);
assert.strictEqual(
  util.isDeepStrictEqual({a: {b: 1}}, {a: {b: 2}}),
  false,
  'Nested objects not equal'
);
console.log('✓ Nested objects work');

// Test 4: Arrays
console.log('\n=== Test 4: Arrays ===');
assert.strictEqual(util.isDeepStrictEqual([1, 2, 3], [1, 2, 3]), true, 'Arrays equal');
assert.strictEqual(util.isDeepStrictEqual([1, 2], [1, 2, 3]), false, 'Different lengths');
assert.strictEqual(util.isDeepStrictEqual([1, 2, 3], [1, 3, 2]), false, 'Different order');
assert.strictEqual(
  util.isDeepStrictEqual([[1, 2], [3, 4]], [[1, 2], [3, 4]]),
  true,
  'Nested arrays equal'
);
console.log('✓ Arrays work');

// Test 5: Mixed structures
console.log('\n=== Test 5: Mixed structures ===');
assert.strictEqual(
  util.isDeepStrictEqual({a: [1, 2], b: {c: 3}}, {a: [1, 2], b: {c: 3}}),
  true,
  'Mixed structures equal'
);
assert.strictEqual(
  util.isDeepStrictEqual({a: [1, 2], b: {c: 3}}, {a: [1, 2], b: {c: 4}}),
  false,
  'Mixed structures not equal'
);
console.log('✓ Mixed structures work');

// Test 6: Edge cases
console.log('\n=== Test 6: Edge cases ===');
assert.strictEqual(util.isDeepStrictEqual({}, {}), true, 'Empty objects equal');
assert.strictEqual(util.isDeepStrictEqual([], []), true, 'Empty arrays equal');
assert.strictEqual(util.isDeepStrictEqual(null, undefined), false, 'null !== undefined');
console.log('✓ Edge cases work');

console.log('\n' + '='.repeat(60));
console.log('✅ All isDeepStrictEqual tests passed!');
console.log('='.repeat(60));
