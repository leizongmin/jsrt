// Test Writable stream cork/uncork functionality
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

// Test 1: cork() method exists
test('cork() method exists', () => {
  const writable = new Writable();
  assert.strictEqual(typeof writable.cork, 'function', 'cork method exists');
});

// Test 2: uncork() method exists
test('uncork() method exists', () => {
  const writable = new Writable();
  assert.strictEqual(
    typeof writable.uncork,
    'function',
    'uncork method exists'
  );
});

// Test 3: writableCorked property initially 0
test('writableCorked property initially 0', () => {
  const writable = new Writable();
  assert.strictEqual(writable.writableCorked, 0, 'initially 0');
});

// Test 4: cork() increments writableCorked
test('cork() increments writableCorked', () => {
  const writable = new Writable();
  writable.cork();
  assert.strictEqual(writable.writableCorked, 1, 'writableCorked is 1');
  writable.cork();
  assert.strictEqual(
    writable.writableCorked,
    2,
    'writableCorked is 2 after second cork'
  );
});

// Test 5: uncork() decrements writableCorked
test('uncork() decrements writableCorked', () => {
  const writable = new Writable();
  writable.cork();
  writable.cork();
  assert.strictEqual(writable.writableCorked, 2, 'corked twice');
  writable.uncork();
  assert.strictEqual(writable.writableCorked, 1, 'uncorked once');
  writable.uncork();
  assert.strictEqual(writable.writableCorked, 0, 'fully uncorked');
});

// Test 6: writes while corked are buffered
test('writes while corked are buffered', () => {
  const writable = new Writable();
  writable.cork();
  writable.write('data1');
  writable.write('data2');
  assert.strictEqual(writable.writableLength, 2, 'data buffered while corked');
});

// Test 7: write() returns true while corked
test('write() returns true while corked', () => {
  const writable = new Writable({ highWaterMark: 1 });
  writable.cork();
  const result = writable.write('data');
  assert.strictEqual(result, true, 'write returns true while corked');
});

// Test 8: callbacks queued while corked
test('callbacks queued while corked', () => {
  const writable = new Writable();
  writable.cork();
  let callback1Called = false;
  let callback2Called = false;
  writable.write('data1', () => {
    callback1Called = true;
  });
  writable.write('data2', () => {
    callback2Called = true;
  });
  assert.strictEqual(callback1Called, false, 'callback not called yet');
  assert.strictEqual(callback2Called, false, 'callback not called yet');
  writable.uncork();
  assert.strictEqual(callback1Called, true, 'callback called after uncork');
  assert.strictEqual(callback2Called, true, 'callback called after uncork');
});

// Test 9: Nested cork/uncork
test('Nested cork/uncork', () => {
  const writable = new Writable();
  writable.cork();
  writable.cork();
  writable.cork();
  assert.strictEqual(writable.writableCorked, 3, 'corked 3 times');
  writable.uncork();
  assert.strictEqual(writable.writableCorked, 2, 'still corked');
  writable.uncork();
  writable.uncork();
  assert.strictEqual(writable.writableCorked, 0, 'fully uncorked');
});

// Test 10: uncork() when not corked is safe
test('uncork() when not corked is safe', () => {
  const writable = new Writable();
  writable.uncork(); // Should not throw
  assert.strictEqual(writable.writableCorked, 0, 'remains 0');
});

// Test 11: Cork buffers multiple writes
test('Cork buffers multiple writes', () => {
  const writable = new Writable();
  writable.cork();
  for (let i = 0; i < 10; i++) {
    writable.write(`data${i}`);
  }
  assert.strictEqual(writable.writableLength, 10, '10 writes buffered');
  writable.uncork();
  assert.strictEqual(writable.writableLength, 10, 'still 10 after uncork');
});

// Test 12: Multiple cork/uncork cycles
test('Multiple cork/uncork cycles', () => {
  const writable = new Writable();
  writable.cork();
  writable.write('a');
  writable.uncork();
  assert.strictEqual(writable.writableLength, 1, 'first cycle');

  writable.cork();
  writable.write('b');
  writable.write('c');
  writable.uncork();
  assert.strictEqual(writable.writableLength, 3, 'second cycle');
});

console.log(`\n✅ Passed: ${passCount}/${passCount + failCount}`);
console.log(`❌ Failed: ${failCount}/${passCount + failCount}`);

if (failCount > 0) {
  throw new Error(`${failCount} test(s) failed`);
}
