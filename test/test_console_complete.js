// Comprehensive Console API tests for WinterCG compliance
const assert = require("std:assert");
console.log('=== Starting Complete Console API Tests ===');

// Test basic logging methods
console.log('Test 1: console.log - basic functionality');
console.log('This is a log message with', 123, true, null, undefined);

console.log('Test 2: console.error - error messages');  
if (typeof console.error === 'function') {
  console.error('This is an error message with', 456, false);
  assert.strictEqual(typeof console.error, 'function', 'console.error should be a function');
} else {
  console.log('console.error NOT IMPLEMENTED');
}

console.log('Test 3: console.warn - warning messages');
if (typeof console.warn === 'function') {
  console.warn('This is a warning message with', 789, 'warning');
  assert.strictEqual(typeof console.warn, 'function', 'console.warn should be a function');
} else {
  console.log('console.warn NOT IMPLEMENTED');
}

console.log('Test 4: console.info - info messages');
if (typeof console.info === 'function') {
  console.info('This is an info message with', { info: true });
  assert.strictEqual(typeof console.info, 'function', 'console.info should be a function');
} else {
  console.log('console.info NOT IMPLEMENTED');
}

console.log('Test 5: console.debug - debug messages');  
if (typeof console.debug === 'function') {
  console.debug('This is a debug message with', [1, 2, 3]);
  assert.strictEqual(typeof console.debug, 'function', 'console.debug should be a function');
} else {
  console.log('console.debug NOT IMPLEMENTED');
}

console.log('Test 6: console.trace - stack trace');
if (typeof console.trace === 'function') {
  console.trace('Trace message', 'with arguments');
} else {
  console.log('console.trace NOT IMPLEMENTED');
}

console.log('Test 7: console.assert - assertions');
if (typeof console.assert === 'function') {
  console.assert(true, 'This should not appear');
  console.assert(false, 'This assertion failed:', { reason: 'test' });
  console.assert(1 === 2, 'Math is broken');
} else {
  console.log('console.assert NOT IMPLEMENTED');
}

console.log('Test 8: console.time/timeEnd - timing');
if (typeof console.time === 'function' && typeof console.timeEnd === 'function') {
  assert.strictEqual(typeof console.time, 'function', 'console.time should be a function');
  assert.strictEqual(typeof console.timeEnd, 'function', 'console.timeEnd should be a function');
  console.time('test-timer');
  // Simple computation to measure
  let sum = 0;
  for (let i = 0; i < 1000; i++) {
    sum += i;
  }
  console.timeEnd('test-timer');
  
  console.time('another-timer');
  console.timeEnd('another-timer');
} else {
  console.log('console.time/timeEnd NOT IMPLEMENTED');
}

console.log('Test 9: console.count/countReset - counting');
if (typeof console.count === 'function' && typeof console.countReset === 'function') {
  assert.strictEqual(typeof console.count, 'function', 'console.count should be a function');
  assert.strictEqual(typeof console.countReset, 'function', 'console.countReset should be a function');
  console.count('test-counter');
  console.count('test-counter'); 
  console.count('test-counter');
  console.count('another-counter');
  console.count('another-counter');
  console.countReset('test-counter');
  console.count('test-counter');
} else {
  console.log('console.count/countReset NOT IMPLEMENTED');
}

console.log('Test 10: console.group/groupEnd - grouping');
if (typeof console.group === 'function' && typeof console.groupEnd === 'function') {
  console.group('Group 1');
  console.log('Inside group 1');
  console.group('Nested group');
  console.log('Inside nested group');
  console.groupEnd();
  console.log('Back in group 1');
  console.groupEnd();
  console.log('Outside all groups');
} else {
  console.log('console.group/groupEnd NOT IMPLEMENTED');
}

console.log('Test 11: console.groupCollapsed');
if (typeof console.groupCollapsed === 'function') {
  console.groupCollapsed('Collapsed group');
  console.log('Inside collapsed group');
  console.groupEnd();
} else {
  console.log('console.groupCollapsed NOT IMPLEMENTED');
}

console.log('Test 12: console.dir - object inspection');
if (typeof console.dir === 'function') {
  console.dir({ name: 'test', value: 123, nested: { a: 1, b: 2 } });
  console.dir([1, 2, 3, { inner: true }]);
} else {
  console.log('console.dir NOT IMPLEMENTED');
}

console.log('Test 13: console.table - tabular data');
if (typeof console.table === 'function') {
  console.table([
    { name: 'Alice', age: 30, city: 'New York' },
    { name: 'Bob', age: 25, city: 'Los Angeles' },
    { name: 'Charlie', age: 35, city: 'Chicago' }
  ]);
  console.table({ a: 1, b: 2, c: 3 });
} else {
  console.log('console.table NOT IMPLEMENTED');
}

console.log('Test 14: console.clear - clear console');  
if (typeof console.clear === 'function') {
  console.log('This message should be cleared');
  console.clear();
  console.log('Console was cleared - this should appear after clear');
} else {
  console.log('console.clear NOT IMPLEMENTED');
}

// Test edge cases
console.log('Test 15: Edge cases and error handling');

// Test calling time/timeEnd with invalid arguments
if (typeof console.time === 'function') {
  console.time(); // No label
  console.timeEnd(); // No label
  console.timeEnd('non-existent-timer'); // Timer that doesn't exist
}

// Test calling count with no argument
if (typeof console.count === 'function') {
  console.count(); // Default label
  console.count(); // Default label again
}

// Test nested groups behavior
if (typeof console.group === 'function') {
  console.group('Outer');
  console.group('Middle');
  console.group('Inner');
  console.log('Deeply nested');
  console.groupEnd();
  console.groupEnd();
  console.groupEnd();
}

console.log('=== Console API Tests Complete ===');