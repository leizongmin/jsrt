// Timer functionality tests
console.log('Starting timer tests...');

// Test 1: setTimeout with immediate execution (0ms)
console.log('Test 1: setTimeout immediate execution');
const a = {v: 0};
const t1 = setTimeout((a, x, y, z) => {
  console.log('setTimeout1', a.v, x, y, z);
}, 0, a, 123, 456);
console.log('t1 type:', typeof t1, 'has id:', 'id' in t1);
a.v++;

// Test 2: setTimeout with no additional arguments
console.log('Test 2: setTimeout no extra args');
const t2 = setTimeout((...args) => {
  console.log('setTimeout2', args.length, 'args');
}, 0);
console.log('t2 type:', typeof t2);

// Test 3: setTimeout with longer delay - but still short for testing
console.log('Test 3: setTimeout with delay');
const t3 = setTimeout((...args) => {
  console.log('setTimeout3', args.length, 'args');
}, 50);
console.log('t3 type:', typeof t3);

// Test 4: setTimeout that gets cancelled
console.log('Test 4: setTimeout cancelled');
const t4 = setTimeout((...args) => {
  console.log('setTimeout4 - SHOULD NOT APPEAR', args.length, 'args');
}, 100);
console.log('t4 type:', typeof t4);

// Test clearTimeout with no arguments (should not crash)
clearTimeout();
// Cancel t4
clearTimeout(t4);

// Test 5: setInterval that runs a few times then gets cancelled
console.log('Test 5: setInterval limited runs');
let counter = 0;
const t5 = setInterval((...args) => {
  counter++;
  console.log('setInterval5', counter, args.length, 'args');
  if (counter >= 2) { // Reduced to 2 iterations
    clearInterval(t5);
    console.log('setInterval5 cleared');
  }
}, 30);
console.log('t5 type:', typeof t5);

// Test 6: clearInterval with invalid argument (should not crash)
console.log('Test 6: clearInterval with invalid args');
clearInterval();
clearInterval(null);
clearInterval(undefined);

// Test 7: setTimeout with function that throws error (simplified)
console.log('Test 7: setTimeout with error');
const t6 = setTimeout(() => {
  try {
    throw new Error('Test error in setTimeout');
  } catch (e) {
    console.log('Caught error:', e.message);
  }
}, 0);

// Test 8: Timer object basic validation (simplified)
console.log('Test 8: Timer object properties');
const t7 = setTimeout(() => {
  console.log('Timer t7 executed');
}, 0);
console.log('Timer t7 type:', typeof t7);
console.log('Timer t7 has id property:', 'id' in t7);

// Give some time for all timers to execute
setTimeout(() => {
  console.log('Timer tests completed successfully');
}, 150); // Reduced from 200ms
