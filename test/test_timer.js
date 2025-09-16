const assert = require('jsrt:assert');

// Test setTimeout with arguments
let timeoutCalled = false;
let timeoutArgs = [];

setTimeout(
  (msg, num) => {
    timeoutCalled = true;
    timeoutArgs = [msg, num];

    if (msg !== 'hello') {
      console.error('❌ FAIL: setTimeout arg1 expected "hello", got', msg);
    }

    if (num !== 123) {
      console.error('❌ FAIL: setTimeout arg2 expected 123, got', num);
    }
  },
  10,
  'hello',
  123
);

// Test setTimeout return value and clearTimeout
const timer1 = setTimeout(() => {
  console.error('❌ FAIL: This should NOT appear - timer was cleared');
}, 50);

if (typeof timer1 !== 'object') {
  console.error('❌ FAIL: Timer should return an object, got', typeof timer1);
}

if (!timer1 || typeof timer1.id !== 'number') {
  console.error('❌ FAIL: Timer should have an id property');
}

clearTimeout(timer1);

// Test setInterval and clearInterval
let count = 0;
const timer2 = setInterval(() => {
  count++;
  if (count > 2) {
    console.error('❌ FAIL: setInterval count exceeded limit:', count);
    clearInterval(timer2);
  }
  if (count === 2) {
    clearInterval(timer2);
    setTimeout(() => {
      if (count !== 2) {
        console.error('❌ FAIL: Final count should be 2, got', count);
      }
    }, 100);
  }
}, 20);

// Test clearTimeout/clearInterval with invalid arguments
try {
  clearTimeout(null);
  clearTimeout(undefined);
  clearTimeout({});
  clearInterval(null);
  clearInterval(undefined);
  clearInterval({});
} catch (e) {
  console.error('❌ FAIL: Clear functions should not throw errors:', e.message);
}

// Wait for all timers to complete
setTimeout(() => {
  assert.strictEqual(timeoutCalled, true, 'setTimeout should have been called');
  assert.deepStrictEqual(
    timeoutArgs,
    ['hello', 123],
    'setTimeout should receive correct arguments'
  );
  assert.strictEqual(
    count,
    2,
    'setInterval should have been called exactly 2 times'
  );
}, 200);
