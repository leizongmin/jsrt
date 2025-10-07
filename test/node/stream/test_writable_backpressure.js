// Test Writable stream backpressure
const { Writable } = require('node:stream');
const assert = require('node:assert');

let passCount = 0;
let failCount = 0;

function test(name, fn) {
  try {
    fn();
    passCount++;
  } catch (err) {
    console.log(`❌ FAIL: ${name}`);
    console.log(`  ${err.message}`);
    if (err.stack) {
      console.log(err.stack.split('\n').slice(1, 3).join('\n'));
    }
    failCount++;
  }
}

// Test 1: write() returns true when below highWaterMark
test('write() returns true when below highWaterMark', () => {
  const writable = new Writable({ highWaterMark: 10 });
  const result = writable.write('data');
  assert.strictEqual(result, true, 'write returns true below highWaterMark');
});

// Test 2: write() returns false when exceeding highWaterMark
test('write() returns false when exceeding highWaterMark', () => {
  const writable = new Writable({ highWaterMark: 2 });
  writable.write('data1');
  writable.write('data2');
  const result = writable.write('data3'); // Third write exceeds
  assert.strictEqual(
    result,
    false,
    'write returns false when exceeding highWaterMark'
  );
});

// Test 3: highWaterMark with object mode (count objects)
test('highWaterMark with object mode', () => {
  const writable = new Writable({ objectMode: true, highWaterMark: 2 });
  assert.strictEqual(writable.write({ a: 1 }), true, 'first object ok');
  assert.strictEqual(writable.write({ b: 2 }), true, 'second object ok');
  assert.strictEqual(
    writable.write({ c: 3 }),
    false,
    'third object triggers backpressure'
  );
});

// Test 4: Default highWaterMark
test('Default highWaterMark is 16384', () => {
  const writable = new Writable();
  assert.strictEqual(
    writable.writableHighWaterMark,
    16384,
    'default highWaterMark is 16384'
  );
});

// Test 5: Custom highWaterMark
test('Custom highWaterMark', () => {
  const writable = new Writable({ highWaterMark: 100 });
  assert.strictEqual(
    writable.writableHighWaterMark,
    100,
    'custom highWaterMark set'
  );
});

// Test 6: writableLength tracks buffer size
test('writableLength tracks buffer size', () => {
  const writable = new Writable({ highWaterMark: 10 });
  assert.strictEqual(writable.writableLength, 0, 'initially 0');
  writable.write('a');
  assert.strictEqual(writable.writableLength, 1, 'length is 1 after write');
  writable.write('b');
  assert.strictEqual(
    writable.writableLength,
    2,
    'length is 2 after second write'
  );
});

// Test 7: Backpressure with small highWaterMark
test('Backpressure with small highWaterMark', () => {
  const writable = new Writable({ highWaterMark: 1 });
  const result1 = writable.write('data');
  assert.strictEqual(result1, true, 'first write ok');
  const result2 = writable.write('more');
  assert.strictEqual(result2, false, 'second write triggers backpressure');
});

// Test 8: Object mode highWaterMark default
test('Object mode highWaterMark default is 16', () => {
  const writable = new Writable({ objectMode: true });
  assert.strictEqual(
    writable.writableHighWaterMark,
    16,
    'object mode default is 16'
  );
});

// Test 9: writableLength in object mode
test('writableLength in object mode', () => {
  const writable = new Writable({ objectMode: true });
  writable.write({ a: 1 });
  writable.write({ b: 2 });
  assert.strictEqual(writable.writableLength, 2, 'counts objects correctly');
});

// Test 10: Backpressure does not prevent writes
test('Backpressure does not prevent writes, just signals', () => {
  const writable = new Writable({ highWaterMark: 1 });
  writable.write('data1'); // ok
  writable.write('data2'); // backpressure but still writes
  writable.write('data3'); // backpressure but still writes
  assert.strictEqual(
    writable.writableLength,
    3,
    'all writes buffered despite backpressure'
  );
});

console.log(`\n✅ Passed: ${passCount}/${passCount + failCount}`);
console.log(`❌ Failed: ${failCount}/${passCount + failCount}`);

if (failCount > 0) {
  throw new Error(`${failCount} test(s) failed`);
}
