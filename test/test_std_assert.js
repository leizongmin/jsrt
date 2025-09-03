const assert = require('jsrt:assert');

console.log('=== Testing jsrt:assert module (CommonJS) ===');
console.log('Assert module type:', typeof assert);

// Test 1: Basic assert function
console.log('\nTest 1: Basic assert function');
try {
  assert(true, 'This should pass');
  console.log('✅ assert(true) passed');
} catch (e) {
  console.log('❌ assert(true) failed:', e.message);
}

try {
  assert(false, 'This should fail');
  console.log('❌ assert(false) should have failed');
} catch (e) {
  console.log('✅ assert(false) correctly failed:', e.message);
}

// Test 2: assert.ok (alias for assert)
console.log('\nTest 2: assert.ok method');
try {
  assert.ok(true, 'This should pass');
  console.log('✅ assert.ok(true) passed');
} catch (e) {
  console.log('❌ assert.ok(true) failed:', e.message);
}

try {
  assert.ok(false, 'This should fail');
  console.log('❌ assert.ok(false) should have failed');
} catch (e) {
  console.log('✅ assert.ok(false) correctly failed:', e.message);
}

// Test 3: assert.equal (loose equality)
console.log('\nTest 3: assert.equal method');
try {
  assert.equal(1, 1);
  assert.equal('1', 1); // Should pass with loose equality
  assert.equal(true, 1);
  console.log('✅ assert.equal tests passed');
} catch (e) {
  console.log('❌ assert.equal failed:', e.message);
}

try {
  assert.equal(1, 2, '1 should not equal 2');
  console.log('❌ assert.equal(1, 2) should have failed');
} catch (e) {
  console.log('✅ assert.equal(1, 2) correctly failed:', e.message);
}

// Test 4: assert.notEqual
console.log('\nTest 4: assert.notEqual method');
try {
  assert.notEqual(1, 2);
  assert.notEqual('hello', 'world');
  console.log('✅ assert.notEqual tests passed');
} catch (e) {
  console.log('❌ assert.notEqual failed:', e.message);
}

try {
  assert.notEqual(1, 1, '1 should equal 1');
  console.log('❌ assert.notEqual(1, 1) should have failed');
} catch (e) {
  console.log('✅ assert.notEqual(1, 1) correctly failed:', e.message);
}

// Test 5: assert.strictEqual
console.log('\nTest 5: assert.strictEqual method');
try {
  assert.strictEqual(1, 1);
  assert.strictEqual('hello', 'hello');
  assert.strictEqual(true, true);
  console.log('✅ assert.strictEqual tests passed');
} catch (e) {
  console.log('❌ assert.strictEqual failed:', e.message);
}

try {
  assert.strictEqual('1', 1, "String '1' should not strictly equal number 1");
  console.log("❌ assert.strictEqual('1', 1) should have failed");
} catch (e) {
  console.log("✅ assert.strictEqual('1', 1) correctly failed:", e.message);
}

// Test 6: assert.notStrictEqual
console.log('\nTest 6: assert.notStrictEqual method');
try {
  assert.notStrictEqual('1', 1);
  assert.notStrictEqual(true, 1);
  assert.notStrictEqual(null, undefined);
  console.log('✅ assert.notStrictEqual tests passed');
} catch (e) {
  console.log('❌ assert.notStrictEqual failed:', e.message);
}

try {
  assert.notStrictEqual(1, 1, '1 should strictly equal 1');
  console.log('❌ assert.notStrictEqual(1, 1) should have failed');
} catch (e) {
  console.log('✅ assert.notStrictEqual(1, 1) correctly failed:', e.message);
}

// Test 7: assert.deepEqual
console.log('\nTest 7: assert.deepEqual method');
try {
  assert.deepEqual({ a: 1, b: 2 }, { a: 1, b: 2 });
  assert.deepEqual([1, 2, 3], [1, 2, 3]);
  assert.deepEqual({ nested: { value: 42 } }, { nested: { value: 42 } });
  console.log('✅ assert.deepEqual tests passed');
} catch (e) {
  console.log('❌ assert.deepEqual failed:', e.message);
}

try {
  assert.deepEqual({ a: 1 }, { a: 2 }, 'Objects should not be deeply equal');
  console.log('❌ assert.deepEqual({a: 1}, {a: 2}) should have failed');
} catch (e) {
  console.log(
    '✅ assert.deepEqual({a: 1}, {a: 2}) correctly failed:',
    e.message
  );
}

// Test 8: assert.notDeepEqual
console.log('\nTest 8: assert.notDeepEqual method');
try {
  assert.notDeepEqual({ a: 1 }, { a: 2 });
  assert.notDeepEqual([1, 2], [1, 3]);
  console.log('✅ assert.notDeepEqual tests passed');
} catch (e) {
  console.log('❌ assert.notDeepEqual failed:', e.message);
}

try {
  assert.notDeepEqual({ a: 1 }, { a: 1 }, 'Objects should be deeply equal');
  console.log('❌ assert.notDeepEqual({a: 1}, {a: 1}) should have failed');
} catch (e) {
  console.log(
    '✅ assert.notDeepEqual({a: 1}, {a: 1}) correctly failed:',
    e.message
  );
}

// Test 9: assert.throws
console.log('\nTest 9: assert.throws method');
try {
  assert.throws(() => {
    throw new Error('Test error');
  }, 'Function should throw');
  console.log('✅ assert.throws test passed');
} catch (e) {
  console.log('❌ assert.throws failed:', e.message);
}

try {
  assert.throws(() => {
    return 'no error';
  }, "Function should throw but doesn't");
  console.log('❌ assert.throws should have failed');
} catch (e) {
  console.log('✅ assert.throws correctly failed:', e.message);
}

// Test 10: assert.doesNotThrow
console.log('\nTest 10: assert.doesNotThrow method');
try {
  assert.doesNotThrow(() => {
    return 'success';
  }, 'Function should not throw');
  console.log('✅ assert.doesNotThrow test passed');
} catch (e) {
  console.log('❌ assert.doesNotThrow failed:', e.message);
}

try {
  assert.doesNotThrow(() => {
    throw new Error('Unexpected error');
  }, 'Function should not throw but does');
  console.log('❌ assert.doesNotThrow should have failed');
} catch (e) {
  console.log('✅ assert.doesNotThrow correctly failed:', e.message);
}

// Test 11: Error message customization
console.log('\nTest 11: Custom error messages');
try {
  assert.strictEqual(1, 2, 'Custom error: 1 should equal 2');
  console.log('❌ Custom message test should have failed');
} catch (e) {
  console.log('✅ Custom error message:', e.message);
}

console.log('\n=== All jsrt:assert (CommonJS) tests completed ===');
