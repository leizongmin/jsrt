// Cross-platform compatible timer tests
console.log('Starting timer tests...');

// Test 1: Basic setTimeout functionality
console.log('Test 1: setTimeout basic test');
setTimeout(() => {
  console.log('setTimeout callback executed');
}, 0);

// Test 2: setTimeout with arguments
console.log('Test 2: setTimeout with arguments');
setTimeout((msg, num) => {
  console.log('setTimeout with args:', msg, num);
}, 0, 'hello', 123);

// Test 3: Timer return value validation
console.log('Test 3: Timer return value');
const timer1 = setTimeout(() => {
  console.log('timer1 executed');
}, 0);
console.log('Timer type:', typeof timer1);
console.log('Timer has id:', 'id' in timer1);

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
  if (count >= 2) {
    clearInterval(interval1);
    console.log('setInterval cleared');
  }
}, 50);

// Test 6: Clear functions with invalid arguments (should not crash)
console.log('Test 6: Invalid clear calls');
clearTimeout();
clearTimeout(null);
clearInterval();
clearInterval(null);

// Final completion message
setTimeout(() => {
  console.log('Timer tests completed successfully');
}, 150);
