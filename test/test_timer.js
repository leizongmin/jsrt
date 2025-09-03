// Cross-platform compatible timer tests
const assert = require('std:assert');
console.log('Starting timer tests...');

// Test 1: Basic setTimeout functionality
console.log('Test 1: setTimeout basic test');
setTimeout(() => {
  console.log('setTimeout callback executed');
  assert(true, 'setTimeout callback should execute');
}, 0);

// Test 2: setTimeout with arguments
console.log('Test 2: setTimeout with arguments');
setTimeout(
  (msg, num) => {
    console.log('setTimeout with args:', msg, num);
    assert.strictEqual(msg, 'hello', 'First argument should be "hello"');
    assert.strictEqual(num, 123, 'Second argument should be 123');
  },
  0,
  'hello',
  123
);

// Test 3: Timer return value validation
console.log('Test 3: Timer return value');
const timer1 = setTimeout(() => {
  console.log('timer1 executed');
}, 0);
console.log('Timer type:', typeof timer1);
console.log('Timer has id:', 'id' in timer1);
assert.strictEqual(typeof timer1, 'object', 'Timer should return an object');
assert.strictEqual('id' in timer1, true, 'Timer should have an id property');

// Test 4: Clear timeout functionality
console.log('Test 4: clearTimeout test');
const timer2 = setTimeout(() => {
  console.log('This should NOT appear - timer was cleared');
}, 100);
clearTimeout(timer2);

// Test 5: setInterval basic test (limited iterations)
console.log('Test 5: setInterval basic test');
let count = 0;
const interval1 = setInterval(() => {
  count++;
  console.log('setInterval iteration:', count);
  assert(count > 0, 'Count should be greater than 0');
  assert(count <= 2, 'Count should not exceed 2');
  if (count >= 2) {
    clearInterval(interval1);
    console.log('setInterval cleared');
    assert.strictEqual(count, 2, 'Final count should be exactly 2');
  }
}, 50);

// Test 6: Clear functions with invalid arguments (should not crash)
console.log('Test 6: Invalid clear calls');
try {
  clearTimeout();
  clearTimeout(null);
  clearInterval();
  clearInterval(null);
  assert(true, 'Clear functions should not crash with invalid arguments');
} catch (e) {
  assert(false, 'Clear functions should not throw errors: ' + e.message);
}

// Final completion message
setTimeout(() => {
  console.log('Timer tests completed successfully');
  assert(true, 'Timer tests completed without errors');
}, 150);
