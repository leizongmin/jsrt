// Timer functionality tests
console.log('Starting timer tests...');

// Test 1: setTimeout with immediate execution (0ms)
console.log('Test 1: setTimeout immediate execution');
const a = {v: 0};
const t1 = setTimeout((a, x, y, z) => {
  console.log('setTimeout1', a.v, x, y, z);
}, 0, a, 123, 456);
console.log('t1', t1, t1.id);
a.v++;

// Test 2: setTimeout with no additional arguments
console.log('Test 2: setTimeout no extra args');
const t2 = setTimeout((...args) => {
  console.log('setTimeout2', ...args);
}, 0);
console.log('t2', t2, t2.id);

// Test 3: setTimeout with longer delay - but still short for testing
console.log('Test 3: setTimeout with delay');
const t3 = setTimeout((...args) => {
  console.log('setTimeout3', ...args);
}, 50);
console.log('t3', t3, t3.id);

// Test 4: setTimeout that gets cancelled
console.log('Test 4: setTimeout cancelled');
const t4 = setTimeout((...args) => {
  console.log('setTimeout4 - SHOULD NOT APPEAR', ...args);
}, 100);
console.log('t4', t4, t4.id);

// Test clearTimeout with no arguments (should not crash)
clearTimeout();
// Cancel t4
clearTimeout(t4);

// Test 5: setInterval that runs a few times then gets cancelled
console.log('Test 5: setInterval limited runs');
let counter = 0;
const t5 = setInterval((...args) => {
  counter++;
  console.log('setInterval5', counter, ...args);
  if (counter >= 3) { // Reduced from 5 to 3 to make test faster
    clearInterval(t5);
    console.log('setInterval5 cleared');
  }
}, 30); // Reduced from 100ms to 30ms
console.log('t5', t5, t5.id);

// Test 6: clearInterval with invalid argument (should not crash)
console.log('Test 6: clearInterval with invalid args');
clearInterval();
clearInterval(null);
clearInterval(undefined);

// Test 7: setTimeout with function that throws error
console.log('Test 7: setTimeout with error');
const t6 = setTimeout(() => {
  throw new Error('Test error in setTimeout');
}, 0);

// Test 8: setTimeout return value properties
console.log('Test 8: Timer object properties');
const t7 = setTimeout(() => {
  console.log('Timer t7 executed');
}, 0);
console.log('Timer t7 type:', typeof t7);
console.log('Timer t7 has id:', t7.hasOwnProperty('id'));

// Test 9: Multiple quick timeouts to test timer ID incrementing
console.log('Test 9: Multiple timer IDs');
for (let i = 0; i < 3; i++) {
  const timer = setTimeout(() => {
    console.log('Multi-timer', i, 'executed');
  }, i * 10);
  console.log('Multi-timer', i, 'id:', timer.id);
}

// Give some time for all timers to execute
setTimeout(() => {
  console.log('Timer tests completed successfully');
}, 200);
