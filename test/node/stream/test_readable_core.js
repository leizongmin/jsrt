// Test: Readable Stream Core API
// Tests for Phase 2.1: Readable Core API

import { Readable } from 'node:stream';
import assert from 'node:assert';

console.log('Test: Readable Stream Core API');

// Test 1: read() returns buffered data
{
  const readable = new Readable();
  readable.push('hello');
  const data = readable.read();
  assert.strictEqual(data, 'hello', 'read() should return buffered data');
  console.log('  ✓ read() returns buffered data');
}

// Test 2: read() returns null when empty
{
  const readable = new Readable();
  const data = readable.read();
  assert.strictEqual(data, null, 'read() should return null when empty');
  console.log('  ✓ read() returns null when empty');
}

// Test 3: push(null) ends the stream
{
  const readable = new Readable();
  readable.push('data');
  const result = readable.push(null);
  assert.strictEqual(result, false, 'push(null) should return false');
  console.log('  ✓ push(null) ends the stream');
}

// Test 4: push() returns backpressure signal
{
  const readable = new Readable({ highWaterMark: 2 });
  const result1 = readable.push('a');
  assert.strictEqual(
    result1,
    true,
    'push() should return true when below highWaterMark'
  );

  const result2 = readable.push('b');
  assert.strictEqual(
    result2,
    false,
    'push() should return false when at/above highWaterMark'
  );
  console.log('  ✓ push() returns backpressure signal');
}

// Test 5: pause() switches to paused mode
{
  const readable = new Readable();
  readable.resume(); // Start flowing
  readable.pause();
  assert.strictEqual(
    readable.isPaused(),
    true,
    'isPaused() should return true after pause()'
  );
  console.log('  ✓ pause() switches to paused mode');
}

// Test 6: resume() switches to flowing mode
{
  const readable = new Readable();
  assert.strictEqual(readable.isPaused(), true, 'stream starts in paused mode');
  readable.resume();
  assert.strictEqual(
    readable.isPaused(),
    false,
    'isPaused() should return false after resume()'
  );
  console.log('  ✓ resume() switches to flowing mode');
}

// Test 7: isPaused() returns correct state
{
  const readable = new Readable();
  assert.strictEqual(
    typeof readable.isPaused(),
    'boolean',
    'isPaused() should return boolean'
  );
  console.log('  ✓ isPaused() returns correct state');
}

// Test 8: setEncoding() method exists and returns this
{
  const readable = new Readable();
  const result = readable.setEncoding('utf8');
  assert.strictEqual(
    result,
    readable,
    'setEncoding() should return this for chaining'
  );
  console.log('  ✓ setEncoding() method works');
}

// Test 9: readable property getter
{
  const readable = new Readable();
  assert.strictEqual(
    readable.readable,
    true,
    'readable property should be true initially'
  );
  console.log('  ✓ readable property getter works');
}

// Test 10: readable property is false after destroy
{
  const readable = new Readable();
  readable.destroy();
  setTimeout(() => {
    assert.strictEqual(
      readable.readable,
      false,
      'readable should be false after destroy'
    );
    console.log('  ✓ readable property is false after destroy');
  }, 10);
}

// Test 11: Multiple push() calls buffer data
{
  const readable = new Readable();
  readable.push('a');
  readable.push('b');
  readable.push('c');

  assert.strictEqual(readable.read(), 'a', 'first read should return "a"');
  assert.strictEqual(readable.read(), 'b', 'second read should return "b"');
  assert.strictEqual(readable.read(), 'c', 'third read should return "c"');
  console.log('  ✓ Multiple push() calls buffer data correctly');
}

// Test 12: read() returns null after stream ends
{
  const readable = new Readable();
  readable.push('data');
  readable.push(null); // End stream

  assert.strictEqual(readable.read(), 'data', 'should read buffered data');
  assert.strictEqual(readable.read(), null, 'should return null after end');
  console.log('  ✓ read() returns null after stream ends');
}

setTimeout(() => {
  console.log('\n✅ All readable core API tests passed');
}, 50);
