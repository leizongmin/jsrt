// Test process timing APIs with enhanced precision

console.log('Testing process timing enhancements...\n');

// Test 1: process.uptime()
console.log('Test 1: process.uptime()');
if (typeof process.uptime === 'function') {
  console.log('✓ process.uptime() exists');

  const uptime1 = process.uptime();
  if (typeof uptime1 === 'number' && uptime1 >= 0) {
    console.log(
      '✓ uptime() returns non-negative number:',
      uptime1.toFixed(6),
      'seconds'
    );
  } else {
    throw new Error('uptime() should return non-negative number');
  }

  // Test precision by checking multiple calls
  const uptime2 = process.uptime();
  if (uptime2 >= uptime1) {
    console.log('✓ uptime() increases monotonically');
  } else {
    throw new Error('uptime() should be monotonic');
  }
} else {
  throw new Error('process.uptime not found');
}

// Test 2: process.hrtime()
console.log('\nTest 2: process.hrtime()');
if (typeof process.hrtime === 'function') {
  console.log('✓ process.hrtime() exists');

  const time1 = process.hrtime();
  if (Array.isArray(time1) && time1.length === 2) {
    console.log('✓ hrtime() returns [seconds, nanoseconds] array');
    console.log('  time:', time1[0], 's +', time1[1], 'ns');

    if (typeof time1[0] === 'number' && typeof time1[1] === 'number') {
      console.log('✓ hrtime() components are numbers');
    } else {
      throw new Error('hrtime() should return array of numbers');
    }

    if (time1[1] >= 0 && time1[1] < 1000000000) {
      console.log('✓ nanoseconds in valid range [0, 999999999]');
    } else {
      throw new Error('nanoseconds out of range: ' + time1[1]);
    }
  } else {
    throw new Error('hrtime() should return array of length 2');
  }

  // Test with previousTime parameter
  const time2 = process.hrtime(time1);
  if (Array.isArray(time2) && time2.length === 2) {
    console.log('✓ hrtime(previousTime) returns delta');
    console.log('  delta:', time2[0], 's +', time2[1], 'ns');

    if (time2[1] >= 0 && time2[1] < 1000000000) {
      console.log('✓ delta nanoseconds in valid range');
    } else {
      throw new Error('delta nanoseconds out of range: ' + time2[1]);
    }
  } else {
    throw new Error('hrtime(previousTime) failed');
  }
} else {
  throw new Error('process.hrtime not found');
}

// Test 3: process.hrtime.bigint()
console.log('\nTest 3: process.hrtime.bigint()');
if (typeof process.hrtime.bigint === 'function') {
  console.log('✓ process.hrtime.bigint() exists');

  const bigtime1 = process.hrtime.bigint();
  if (typeof bigtime1 === 'bigint') {
    console.log('✓ hrtime.bigint() returns bigint');
    console.log('  time:', bigtime1.toString(), 'ns');

    if (bigtime1 > 0n) {
      console.log('✓ bigint time is positive');
    } else {
      throw new Error('bigint time should be positive');
    }
  } else {
    throw new Error(
      'hrtime.bigint() should return bigint, got: ' + typeof bigtime1
    );
  }

  // Test monotonicity
  const bigtime2 = process.hrtime.bigint();
  if (bigtime2 >= bigtime1) {
    console.log('✓ hrtime.bigint() is monotonic');
  } else {
    throw new Error('hrtime.bigint() went backwards');
  }

  // Test precision - difference should be measurable
  const diff = bigtime2 - bigtime1;
  console.log('  delta between calls:', diff.toString(), 'ns');
} else {
  throw new Error('process.hrtime.bigint not found');
}

// Test 4: hrtime() error handling
console.log('\nTest 4: hrtime() error handling');
try {
  // Invalid argument type
  process.hrtime('not an array');
  throw new Error('hrtime() should throw on invalid argument type');
} catch (e) {
  if (e.message.includes('must be an instance of Array')) {
    console.log('✓ hrtime() throws TypeError for non-array argument');
  } else {
    throw new Error('Wrong error message: ' + e.message);
  }
}

try {
  // Invalid nanosecond range
  process.hrtime([0, 1000000000]);
  throw new Error('hrtime() should throw on invalid nanosecond range');
} catch (e) {
  if (e.message.includes('Nanoseconds must be in range')) {
    console.log('✓ hrtime() throws RangeError for invalid nanoseconds');
  } else {
    throw new Error('Wrong error message: ' + e.message);
  }
}

try {
  // Negative seconds
  process.hrtime([-1, 0]);
  throw new Error('hrtime() should throw on negative seconds');
} catch (e) {
  if (e.message.includes('must be non-negative')) {
    console.log('✓ hrtime() throws RangeError for negative seconds');
  } else {
    throw new Error('Wrong error message: ' + e.message);
  }
}

// Test 5: Precision verification
console.log('\nTest 5: Precision verification');

// Test hrtime precision
const iterations = 100;
let minDelta = Infinity;
let maxDelta = 0;

for (let i = 0; i < iterations; i++) {
  const t1 = process.hrtime.bigint();
  const t2 = process.hrtime.bigint();
  const delta = Number(t2 - t1);
  if (delta > 0) {
    minDelta = Math.min(minDelta, delta);
    maxDelta = Math.max(maxDelta, delta);
  }
}

console.log(
  '  hrtime.bigint() precision test (' + iterations + ' iterations):'
);
console.log('    min delta:', minDelta, 'ns');
console.log('    max delta:', maxDelta, 'ns');

if (minDelta < 100000) {
  // Less than 100 microseconds
  console.log('✓ High-resolution timer has sub-100µs precision');
} else {
  console.log(
    '⚠️  Timer precision is coarser than expected (min: ' + minDelta + 'ns)'
  );
}

// Test uptime precision
const uptime_start = process.uptime();
// Busy wait for a tiny amount
let counter = 0;
for (let i = 0; i < 100000; i++) {
  counter++;
}
const uptime_end = process.uptime();
const uptime_diff = uptime_end - uptime_start;

console.log('  uptime() precision test:');
console.log('    delta:', uptime_diff.toFixed(9), 'seconds');

if (uptime_diff > 0) {
  console.log('✓ uptime() has sufficient precision to measure small intervals');
} else {
  console.log(
    '⚠️  uptime() may not have sufficient precision for very small intervals'
  );
}

// Test 6: Consistency between hrtime() and hrtime.bigint()
console.log('\nTest 6: Consistency check');
const array_time = process.hrtime();
const bigint_time = process.hrtime.bigint();

// Convert array to bigint for comparison
const array_as_bigint =
  BigInt(array_time[0]) * 1000000000n + BigInt(array_time[1]);

// They should be very close (within a few microseconds due to measurement overhead)
const consistency_diff =
  bigint_time > array_as_bigint
    ? bigint_time - array_as_bigint
    : array_as_bigint - bigint_time;

console.log('  hrtime() as bigint:', array_as_bigint.toString(), 'ns');
console.log('  hrtime.bigint():', bigint_time.toString(), 'ns');
console.log('  difference:', consistency_diff.toString(), 'ns');

if (consistency_diff < 1000000n) {
  // Less than 1ms difference
  console.log('✓ hrtime() and hrtime.bigint() are consistent');
} else {
  console.log('⚠️  Large discrepancy between hrtime() and hrtime.bigint()');
}

console.log('\n✅ All process timing tests passed!');
