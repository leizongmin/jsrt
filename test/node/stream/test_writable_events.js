// Test Writable stream events
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

// Test 1: 'finish' event on end()
test("'finish' event on end()", () => {
  const writable = new Writable();
  let finishEmitted = false;
  writable.on('finish', () => {
    finishEmitted = true;
  });
  writable.end();
  assert.strictEqual(finishEmitted, true, 'finish event emitted');
});

// Test 2: 'finish' event with data
test("'finish' event with data in end()", () => {
  const writable = new Writable();
  let finishEmitted = false;
  writable.on('finish', () => {
    finishEmitted = true;
  });
  writable.end('final chunk');
  assert.strictEqual(finishEmitted, true, 'finish event emitted');
});

// Test 3: 'error' event on write after end
test("'error' event on write after end", () => {
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

// Test 4: 'close' event on destroy()
test("'close' event on destroy()", () => {
  const writable = new Writable({ emitClose: true });
  let closeEmitted = false;
  writable.on('close', () => {
    closeEmitted = true;
  });
  writable.destroy();
  assert.strictEqual(closeEmitted, true, 'close event emitted');
});

// Test 5: 'error' event on destroy(error)
test("'error' event on destroy(error)", () => {
  const writable = new Writable();
  let errorEmitted = false;
  writable.on('error', (err) => {
    errorEmitted = true;
    assert.strictEqual(err.message, 'test error', 'correct error');
  });
  writable.destroy(new Error('test error'));
  assert.strictEqual(errorEmitted, true, 'error event emitted');
});

// Test 6: Multiple finish listeners
test('Multiple finish listeners', () => {
  const writable = new Writable();
  let listener1Called = false;
  let listener2Called = false;
  writable.on('finish', () => {
    listener1Called = true;
  });
  writable.on('finish', () => {
    listener2Called = true;
  });
  writable.end();
  assert.strictEqual(listener1Called, true, 'listener 1 called');
  assert.strictEqual(listener2Called, true, 'listener 2 called');
});

// Test 7: once() for finish event
test('once() for finish event', () => {
  const writable = new Writable();
  let callCount = 0;
  writable.once('finish', () => {
    callCount++;
  });
  writable.end();
  // Try to emit again (won't happen naturally, but tests once() behavior)
  assert.strictEqual(callCount, 1, 'callback called once');
});

// Test 8: removeListener works
test('removeListener works', () => {
  const writable = new Writable();
  let finishEmitted = false;
  const listener = () => {
    finishEmitted = true;
  };
  writable.on('finish', listener);
  writable.removeListener('finish', listener);
  writable.end();
  assert.strictEqual(finishEmitted, false, 'listener was removed');
});

// Test 9: emitClose option controls close event
test('emitClose option controls close event', () => {
  const writable1 = new Writable({ emitClose: false });
  let close1Emitted = false;
  writable1.on('close', () => {
    close1Emitted = true;
  });
  writable1.destroy();
  assert.strictEqual(
    close1Emitted,
    false,
    'close not emitted when emitClose is false'
  );

  const writable2 = new Writable({ emitClose: true });
  let close2Emitted = false;
  writable2.on('close', () => {
    close2Emitted = true;
  });
  writable2.destroy();
  assert.strictEqual(
    close2Emitted,
    true,
    'close emitted when emitClose is true'
  );
});

// Test 10: EventEmitter listenerCount
test('EventEmitter listenerCount', () => {
  const writable = new Writable();
  assert.strictEqual(
    writable.listenerCount('finish'),
    0,
    'initially 0 listeners'
  );
  writable.on('finish', () => {});
  assert.strictEqual(writable.listenerCount('finish'), 1, '1 listener added');
  writable.on('finish', () => {});
  assert.strictEqual(writable.listenerCount('finish'), 2, '2 listeners total');
});

console.log(`\n✅ Passed: ${passCount}/${passCount + failCount}`);
console.log(`❌ Failed: ${failCount}/${passCount + failCount}`);

if (failCount > 0) {
  throw new Error(`${failCount} test(s) failed`);
}
