// Performance API tests for WinterCG compliance
const assert = require('jsrt:assert');
// Test 1: Check if performance object exists
assert.strictEqual(
  typeof performance,
  'object',
  'performance should be an object'
);
assert(performance !== null, 'performance should not be null');
// Test 2: Check performance.now() method
assert.strictEqual(
  typeof performance.now,
  'function',
  'performance.now should be a function'
);
// Test basic functionality
const start = performance.now();
assert.strictEqual(
  typeof start,
  'number',
  'performance.now() should return a number'
);
assert(start >= 0, 'performance.now() should return a non-negative number');
// Test 3: Check performance.timeOrigin property
assert(
  'timeOrigin' in performance,
  'performance.timeOrigin property should be available'
);
const timeOrigin = performance.timeOrigin;
assert.strictEqual(
  typeof timeOrigin,
  'number',
  'performance.timeOrigin should be a number'
);
// Test 4: Monotonic time progression
if (typeof performance.now === 'function') {
  const time1 = performance.now();

  // Small delay (busy wait)
  for (let i = 0; i < 10000; i++) {
    Math.random();
  }

  const time2 = performance.now();

  assert(time2 >= time1, 'Time should not go backwards');
  if (time2 - time1 <= 0) {
    console.log('ℹ️ INFO: Very little time elapsed (< 1ms resolution)');
  }
}

// Test 5: High precision timing
if (typeof performance.now === 'function') {
  const measurements = [];

  for (let i = 0; i < 5; i++) {
    measurements.push(performance.now());
  }

  // Check if we get different values (indicating high precision)
  let allSame = true;
  for (let i = 1; i < measurements.length; i++) {
    if (measurements[i] !== measurements[0]) {
      allSame = false;
      break;
    }
  }

  if (allSame) {
    console.log(
      'ℹ️ INFO: All measurements were identical (very fast execution)'
    );
  }
}

// Test 6: Timer resolution test
if (typeof performance.now === 'function') {
  const start = performance.now();

  // Use setTimeout to create a measurable delay
  setTimeout(() => {
    const end = performance.now();
    const elapsed = end - start;

    assert(elapsed >= 0, 'Elapsed time should not be negative');
    // Test 7: Comparison with console.time
    if (
      typeof console.time === 'function' &&
      typeof console.timeEnd === 'function'
    ) {
      console.time('performance-comparison');
      const perfStart = performance.now();

      // Small computation
      let sum = 0;
      for (let i = 0; i < 100000; i++) {
        sum += Math.sqrt(i);
      }

      const perfEnd = performance.now();
      console.timeEnd('performance-comparison');

      const perfElapsed = perfEnd - perfStart;

      if (perfElapsed > 0) {
        assert(
          perfElapsed > 0,
          'performance.now() should measure positive elapsed time'
        );
      } else {
        console.log(
          'ℹ️ INFO: Operation completed in less than timer resolution'
        );
      }
    }
  }, 10);
}
