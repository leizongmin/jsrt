// Test: Readable Stream Piping
// Tests for Phase 2.3: Piping & Advanced Features

import { Readable, Writable } from 'node:stream';
import assert from 'node:assert';

console.log('Test: Readable Stream Piping');

// Test 1: pipe() method exists
{
  const readable = new Readable();
  assert.strictEqual(
    typeof readable.pipe,
    'function',
    'pipe() should be a function'
  );
  console.log('  ✓ pipe() method exists');
}

// Test 2: unpipe() method exists
{
  const readable = new Readable();
  assert.strictEqual(
    typeof readable.unpipe,
    'function',
    'unpipe() should be a function'
  );
  console.log('  ✓ unpipe() method exists');
}

// Test 3: pipe() connects readable to writable
{
  const readable = new Readable();
  const writable = new Writable();
  const writtenData = [];

  writable.write = function (chunk) {
    writtenData.push(chunk);
    return true;
  };

  readable.pipe(writable);
  readable.push('test');

  setTimeout(() => {
    assert.strictEqual(
      writtenData.length,
      1,
      'data should be written to destination'
    );
    assert.strictEqual(
      writtenData[0],
      'test',
      'correct data should be written'
    );
    console.log('  ✓ pipe() connects readable to writable');
  }, 20);
}

// Test 4: pipe() emits 'pipe' event
{
  const readable = new Readable();
  const writable = new Writable();
  let pipeFired = false;
  let pipeSource = null;

  readable.on('pipe', (src) => {
    pipeFired = true;
    pipeSource = src;
  });

  readable.pipe(writable);

  setTimeout(() => {
    assert.strictEqual(pipeFired, true, 'pipe event should fire');
    assert.strictEqual(pipeSource, readable, 'pipe event should pass source');
    console.log('  ✓ pipe() emits "pipe" event');
  }, 10);
}

// Test 5: pipe() switches to flowing mode
{
  const readable = new Readable();
  const writable = new Writable();

  assert.strictEqual(readable.isPaused(), true, 'should start paused');
  readable.pipe(writable);

  setTimeout(() => {
    assert.strictEqual(
      readable.isPaused(),
      false,
      'should be flowing after pipe'
    );
    console.log('  ✓ pipe() switches to flowing mode');
  }, 10);
}

// Test 6: unpipe() removes destination
{
  const readable = new Readable();
  const writable = new Writable();
  let unpipeFired = false;

  readable.on('unpipe', () => {
    unpipeFired = true;
  });

  readable.pipe(writable);
  readable.unpipe(writable);

  setTimeout(() => {
    assert.strictEqual(unpipeFired, true, 'unpipe event should fire');
    console.log('  ✓ unpipe() removes destination');
  }, 10);
}

// Test 7: unpipe() without args removes all destinations
{
  const readable = new Readable();
  const writable1 = new Writable();
  const writable2 = new Writable();
  let unpipeCount = 0;

  readable.on('unpipe', () => {
    unpipeCount++;
  });

  readable.pipe(writable1);
  readable.pipe(writable2);
  readable.unpipe(); // Remove all

  setTimeout(() => {
    assert.strictEqual(unpipeCount, 2, 'should unpipe all destinations');
    console.log('  ✓ unpipe() without args removes all destinations');
  }, 10);
}

// Test 8: pipe() returns destination
{
  const readable = new Readable();
  const writable = new Writable();

  const result = readable.pipe(writable);
  assert.strictEqual(result, writable, 'pipe() should return destination');
  console.log('  ✓ pipe() returns destination for chaining');
}

// Test 9: Multiple pipes
{
  const readable = new Readable();
  const writable1 = new Writable();
  const writable2 = new Writable();
  const written1 = [];
  const written2 = [];

  writable1.write = function (chunk) {
    written1.push(chunk);
    return true;
  };

  writable2.write = function (chunk) {
    written2.push(chunk);
    return true;
  };

  readable.pipe(writable1);
  readable.pipe(writable2);
  readable.push('data');

  setTimeout(() => {
    assert.strictEqual(
      written1.length,
      1,
      'first writable should receive data'
    );
    assert.strictEqual(
      written2.length,
      1,
      'second writable should receive data'
    );
    console.log('  ✓ Multiple pipes work correctly');
  }, 20);
}

// Test 10: pipe() with {end: false} option
{
  const readable = new Readable();
  const writable = new Writable();

  const result = readable.pipe(writable, { end: false });
  assert.strictEqual(result, writable, 'pipe() should work with options');
  console.log('  ✓ pipe() accepts options');
}

setTimeout(() => {
  console.log('\n✅ All piping tests passed');
}, 100);
