// Test: Base Stream Class with EventEmitter Integration
// Tests for Phase 1.1: Base Stream Class

import { Readable, Writable } from 'node:stream';
import assert from 'node:assert';

console.log('Test: Base Stream Class - EventEmitter Integration');

// Test 1: Stream extends EventEmitter - on() method available
{
  const stream = new Readable();
  assert.ok(typeof stream.on === 'function', 'stream.on should be a function');
  assert.ok(
    typeof stream.once === 'function',
    'stream.once should be a function'
  );
  assert.ok(
    typeof stream.emit === 'function',
    'stream.emit should be a function'
  );
  assert.ok(
    typeof stream.off === 'function',
    'stream.off should be a function'
  );
  console.log(
    '  ✓ Readable stream has EventEmitter methods (on, once, emit, off)'
  );
}

// Test 2: Writable stream extends EventEmitter
{
  const stream = new Writable();
  assert.ok(typeof stream.on === 'function', 'stream.on should be a function');
  assert.ok(
    typeof stream.once === 'function',
    'stream.once should be a function'
  );
  assert.ok(
    typeof stream.emit === 'function',
    'stream.emit should be a function'
  );
  console.log('  ✓ Writable stream has EventEmitter methods');
}

// Test 3: stream.destroy() method exists
{
  const stream = new Readable();
  assert.ok(
    typeof stream.destroy === 'function',
    'stream.destroy should be a function'
  );
  console.log('  ✓ stream.destroy() method exists');
}

// Test 4: stream.destroyed property getter
{
  const stream = new Readable();
  assert.strictEqual(
    stream.destroyed,
    false,
    'stream.destroyed should initially be false'
  );
  console.log('  ✓ stream.destroyed property reflects initial state');
}

// Test 5: stream.destroy() emits close event
{
  const stream = new Readable();
  let closeEmitted = false;

  stream.on('close', () => {
    closeEmitted = true;
  });

  stream.destroy();

  // Give time for event to fire
  setTimeout(() => {
    assert.strictEqual(closeEmitted, true, 'close event should be emitted');
    assert.strictEqual(
      stream.destroyed,
      true,
      'stream.destroyed should be true after destroy'
    );
    console.log(
      '  ✓ stream.destroy() emits close event and updates destroyed property'
    );
  }, 10);
}

// Test 6: stream.destroy(error) emits error event
{
  const stream = new Readable();
  let errorEmitted = false;
  let emittedError = null;

  stream.on('error', (err) => {
    errorEmitted = true;
    emittedError = err;
  });

  const testError = new Error('test error');
  stream.destroy(testError);

  setTimeout(() => {
    assert.strictEqual(errorEmitted, true, 'error event should be emitted');
    assert.strictEqual(emittedError, testError, 'emitted error should match');
    console.log('  ✓ stream.destroy(error) emits error event');
  }, 10);
}

// Test 7: stream.errored property
{
  const stream = new Readable();
  assert.strictEqual(
    stream.errored,
    null,
    'stream.errored should initially be null'
  );

  const testError = new Error('test error');
  stream.destroy(testError);

  setTimeout(() => {
    assert.strictEqual(
      stream.errored,
      testError,
      'stream.errored should return the error'
    );
    console.log('  ✓ stream.errored property returns error object');
  }, 10);
}

// Test 8: EventEmitter events work on streams
{
  const stream = new Readable();
  let eventFired = false;

  stream.on('customEvent', (data) => {
    eventFired = true;
    assert.strictEqual(data, 'test data', 'event data should match');
  });

  stream.emit('customEvent', 'test data');

  setTimeout(() => {
    assert.strictEqual(eventFired, true, 'custom event should fire');
    console.log('  ✓ Custom events work via EventEmitter integration');
  }, 10);
}

// Wait for async tests to complete
setTimeout(() => {
  console.log('\n✅ All base stream tests passed');
}, 50);
