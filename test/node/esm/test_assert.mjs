// Test node:assert module (ES Module)
import assert from 'node:assert';
import { ok, equal, strictEqual, deepEqual, throws } from 'node:assert';

console.log('Testing node:assert module (ES Module)...');

// Test 1: Default export
assert(true);
console.log('✓ Default export assert(true) passed');

// Test 2: Named export - ok
ok(1);
console.log('✓ Named export ok(1) passed');

// Test 3: Named export - equal
equal(1, 1);
console.log('✓ Named export equal(1, 1) passed');

// Test 4: Named export - strictEqual
strictEqual('hello', 'hello');
console.log('✓ Named export strictEqual passed');

// Test 5: Named export - deepEqual
deepEqual({ a: 1 }, { a: 1 });
console.log('✓ Named export deepEqual passed');

// Test 6: Named export - throws
throws(() => {
  throw new Error('test');
});
console.log('✓ Named export throws passed');

// Test 7: Verify default export has methods
assert.ok(1);
console.log('✓ Default export has ok method');

assert.equal(1, 1);
console.log('✓ Default export has equal method');

console.log('All node:assert ES Module tests completed!');
