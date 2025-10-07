// Test file for util.types.* API
const util = require('node:util');
const assert = require('jsrt:assert');

console.log('Testing util.types namespace\n');

// Test 1: util.types.isDate()
console.log('=== Testing util.types.isDate() ===');
assert.strictEqual(util.types.isDate(new Date()), true, 'Date object should return true');
assert.strictEqual(util.types.isDate({}), false, 'Plain object should return false');
assert.strictEqual(util.types.isDate('2025-01-01'), false, 'String should return false');
assert.strictEqual(util.types.isDate(null), false, 'null should return false');
assert.strictEqual(util.types.isDate(undefined), false, 'undefined should return false');
console.log('✓ util.types.isDate() works correctly');

// Test 2: util.types.isPromise()
console.log('\n=== Testing util.types.isPromise() ===');
assert.strictEqual(util.types.isPromise(Promise.resolve()), true, 'Promise should return true');
assert.strictEqual(util.types.isPromise(Promise.reject().catch(() => {})), true, 'Rejected promise should return true');
assert.strictEqual(util.types.isPromise({}), false, 'Plain object should return false');
assert.strictEqual(util.types.isPromise({ then: () => {} }), false, 'Thenable should return false');
assert.strictEqual(util.types.isPromise(null), false, 'null should return false');
console.log('✓ util.types.isPromise() works correctly');

// Test 3: util.types.isRegExp()
console.log('\n=== Testing util.types.isRegExp() ===');
assert.strictEqual(util.types.isRegExp(/abc/), true, 'RegExp literal should return true');
assert.strictEqual(util.types.isRegExp(new RegExp('abc')), true, 'RegExp constructor should return true');
assert.strictEqual(util.types.isRegExp(/test/gi), true, 'RegExp with flags should return true');
assert.strictEqual(util.types.isRegExp('abc'), false, 'String should return false');
assert.strictEqual(util.types.isRegExp({}), false, 'Plain object should return false');
assert.strictEqual(util.types.isRegExp(null), false, 'null should return false');
console.log('✓ util.types.isRegExp() works correctly');

// Test 4: util.types.isArrayBuffer()
console.log('\n=== Testing util.types.isArrayBuffer() ===');
assert.strictEqual(util.types.isArrayBuffer(new ArrayBuffer(10)), true, 'ArrayBuffer should return true');
assert.strictEqual(util.types.isArrayBuffer(new Uint8Array(10)), false, 'TypedArray should return false');
assert.strictEqual(util.types.isArrayBuffer({}), false, 'Plain object should return false');
assert.strictEqual(util.types.isArrayBuffer([]), false, 'Array should return false');
assert.strictEqual(util.types.isArrayBuffer(null), false, 'null should return false');
console.log('✓ util.types.isArrayBuffer() works correctly');

// Test 5: Verify all checkers exist
console.log('\n=== Verifying all type checkers exist ===');
assert.strictEqual(typeof util.types, 'object', 'util.types should be an object');
assert.strictEqual(typeof util.types.isDate, 'function', 'isDate should be a function');
assert.strictEqual(typeof util.types.isPromise, 'function', 'isPromise should be a function');
assert.strictEqual(typeof util.types.isRegExp, 'function', 'isRegExp should be a function');
assert.strictEqual(typeof util.types.isArrayBuffer, 'function', 'isArrayBuffer should be a function');
console.log('✓ All type checkers exist');

// Test 6: Edge cases - no arguments
console.log('\n=== Testing edge cases ===');
assert.strictEqual(util.types.isDate(), false, 'No arguments should return false');
assert.strictEqual(util.types.isPromise(), false, 'No arguments should return false');
assert.strictEqual(util.types.isRegExp(), false, 'No arguments should return false');
assert.strictEqual(util.types.isArrayBuffer(), false, 'No arguments should return false');
console.log('✓ Edge cases handled correctly');

console.log('\n' + '='.repeat(60));
console.log('✅ All util.types tests passed!');
console.log('='.repeat(60));
