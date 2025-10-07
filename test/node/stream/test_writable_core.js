// Test Writable stream core functionality
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

// Test 1: Constructor creates writable stream
test('Constructor creates writable stream', () => {
  const writable = new Writable();
  assert.strictEqual(typeof writable.write, 'function', 'write method exists');
  assert.strictEqual(typeof writable.end, 'function', 'end method exists');
  assert.strictEqual(writable.writable, true, 'writable property is true');
});

// Test 2: Constructor accepts options
test('Constructor accepts options', () => {
  const writable = new Writable({ highWaterMark: 32 });
  assert.strictEqual(
    writable.writableHighWaterMark,
    32,
    'highWaterMark set correctly'
  );
});

// Test 3: write() returns boolean
test('write() returns boolean', () => {
  const writable = new Writable({ highWaterMark: 1 });
  const result = writable.write('data');
  assert.strictEqual(typeof result, 'boolean', 'write returns boolean');
});

// Test 4: write() with callback
test('write() with callback', () => {
  const writable = new Writable();
  let callbackCalled = false;
  writable.write('data', () => {
    callbackCalled = true;
  });
  assert.strictEqual(callbackCalled, true, 'callback was called');
});

// Test 5: end() marks stream as ended
test('end() marks stream as ended', () => {
  const writable = new Writable();
  writable.end();
  assert.strictEqual(writable.writable, false, 'writable is false after end');
  assert.strictEqual(writable.writableEnded, true, 'writableEnded is true');
});

// Test 6: end() emits finish event
test('end() emits finish event', () => {
  const writable = new Writable();
  let finishEmitted = false;
  writable.on('finish', () => {
    finishEmitted = true;
  });
  writable.end();
  assert.strictEqual(finishEmitted, true, 'finish event emitted');
});

// Test 7: end() with chunk writes it first
test('end() with chunk writes it first', () => {
  const writable = new Writable();
  writable.end('final data');
  assert.strictEqual(writable.writableLength, 1, 'chunk was written');
  assert.strictEqual(writable.writableEnded, true, 'stream ended');
});

// Test 8: end() with callback
test('end() with callback', () => {
  const writable = new Writable();
  let callbackCalled = false;
  writable.end(() => {
    callbackCalled = true;
  });
  assert.strictEqual(callbackCalled, true, 'callback was called');
});

// Test 9: write() after end() throws error
test('write() after end() emits error', () => {
  const writable = new Writable();
  let errorEmitted = false;
  writable.on('error', (err) => {
    errorEmitted = true;
    assert.ok(err.message.includes('write after end'), 'correct error message');
  });
  writable.end();
  writable.write('data');
  assert.strictEqual(errorEmitted, true, 'error event emitted');
});

// Test 10: writableLength property
test('writableLength property', () => {
  const writable = new Writable();
  assert.strictEqual(writable.writableLength, 0, 'initially 0');
  writable.write('data1');
  assert.strictEqual(writable.writableLength, 1, 'increases after write');
  writable.write('data2');
  assert.strictEqual(writable.writableLength, 2, 'increases again');
});

// Test 11: writableFinished property
test('writableFinished property', () => {
  const writable = new Writable();
  assert.strictEqual(writable.writableFinished, false, 'initially false');
  writable.end();
  assert.strictEqual(writable.writableFinished, true, 'true after end');
});

// Test 12: writableObjectMode property
test('writableObjectMode property', () => {
  const writable1 = new Writable({ objectMode: false });
  assert.strictEqual(writable1.writableObjectMode, false, 'object mode false');

  const writable2 = new Writable({ objectMode: true });
  assert.strictEqual(writable2.writableObjectMode, true, 'object mode true');
});

// Test 13: Destroyed property via EventEmitter
test('Destroyed property', () => {
  const writable = new Writable();
  assert.strictEqual(writable.destroyed, false, 'initially false');
  writable.destroy();
  assert.strictEqual(writable.destroyed, true, 'true after destroy');
});

// Test 14: EventEmitter methods available
test('EventEmitter methods available', () => {
  const writable = new Writable();
  assert.strictEqual(typeof writable.on, 'function', 'on method exists');
  assert.strictEqual(typeof writable.once, 'function', 'once method exists');
  assert.strictEqual(typeof writable.emit, 'function', 'emit method exists');
  assert.strictEqual(typeof writable.off, 'function', 'off method exists');
});

// Test 15: Multiple writes accumulate
test('Multiple writes accumulate', () => {
  const writable = new Writable();
  writable.write('a');
  writable.write('b');
  writable.write('c');
  assert.strictEqual(writable.writableLength, 3, '3 chunks buffered');
});

console.log(`\n✅ Passed: ${passCount}/${passCount + failCount}`);
console.log(`❌ Failed: ${failCount}/${passCount + failCount}`);

if (failCount > 0) {
  throw new Error(`${failCount} test(s) failed`);
}
