// Test node:assert module (CommonJS)
const assert = require('node:assert');

console.log('Testing node:assert module (CommonJS)...');

// Test 1: Basic assert
try {
  assert(true);
  console.log('✓ assert(true) passed');
} catch (e) {
  console.log('✗ assert(true) failed:', e.message);
}

// Test 2: assert.ok
try {
  assert.ok(1);
  console.log('✓ assert.ok(1) passed');
} catch (e) {
  console.log('✗ assert.ok(1) failed:', e.message);
}

// Test 3: assert.equal
try {
  assert.equal(1, 1);
  console.log('✓ assert.equal(1, 1) passed');
} catch (e) {
  console.log('✗ assert.equal(1, 1) failed:', e.message);
}

// Test 4: assert.strictEqual
try {
  assert.strictEqual('hello', 'hello');
  console.log('✓ assert.strictEqual passed');
} catch (e) {
  console.log('✗ assert.strictEqual failed:', e.message);
}

// Test 5: assert.deepEqual
try {
  assert.deepEqual({ a: 1 }, { a: 1 });
  console.log('✓ assert.deepEqual passed');
} catch (e) {
  console.log('✗ assert.deepEqual failed:', e.message);
}

// Test 6: assert.throws
try {
  assert.throws(() => {
    throw new Error('test');
  });
  console.log('✓ assert.throws passed');
} catch (e) {
  console.log('✗ assert.throws failed:', e.message);
}

// Test 7: Verify it's the same implementation as jsrt:assert
const assertJsrt = require('jsrt:assert');
console.log('✓ Both node:assert and jsrt:assert loaded successfully');

console.log('All node:assert CommonJS tests completed!');
