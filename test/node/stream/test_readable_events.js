// Test: Readable Stream Events
// Tests for Phase 2.2: Readable Events

import { Readable } from 'node:stream';
import assert from 'node:assert';

console.log('Test: Readable Stream Events');

// Test 1: 'readable' event fires in paused mode
{
  const readable = new Readable();
  let readableFired = false;

  readable.on('readable', () => {
    readableFired = true;
  });

  readable.push('data');

  setTimeout(() => {
    assert.strictEqual(readableFired, true, 'readable event should fire');
    console.log('  ✓ "readable" event fires in paused mode');
  }, 10);
}

// Test 2: 'data' event fires in flowing mode
{
  const readable = new Readable();
  let dataFired = false;
  let receivedData = null;

  readable.on('data', (chunk) => {
    dataFired = true;
    receivedData = chunk;
  });

  readable.push('test data');

  setTimeout(() => {
    assert.strictEqual(dataFired, true, 'data event should fire');
    assert.strictEqual(
      receivedData,
      'test data',
      'should receive correct data'
    );
    console.log('  ✓ "data" event fires in flowing mode');
  }, 10);
}

// Test 3: 'end' event fires when stream ends
{
  const readable = new Readable();
  let endFired = false;

  readable.on('end', () => {
    endFired = true;
  });

  readable.push('data');
  readable.push(null); // End stream

  // Trigger end by reading
  readable.read();

  setTimeout(() => {
    assert.strictEqual(endFired, true, 'end event should fire');
    console.log('  ✓ "end" event fires when stream ends');
  }, 20);
}

// Test 4: 'pause' event fires when paused
{
  const readable = new Readable();
  let pauseFired = false;

  readable.on('pause', () => {
    pauseFired = true;
  });

  readable.resume(); // Start flowing
  readable.pause();

  setTimeout(() => {
    assert.strictEqual(pauseFired, true, 'pause event should fire');
    console.log('  ✓ "pause" event fires when paused');
  }, 10);
}

// Test 5: 'resume' event fires when resumed
{
  const readable = new Readable();
  let resumeFired = false;

  readable.on('resume', () => {
    resumeFired = true;
  });

  readable.resume();

  setTimeout(() => {
    assert.strictEqual(resumeFired, true, 'resume event should fire');
    console.log('  ✓ "resume" event fires when resumed');
  }, 10);
}

// Test 6: No 'data' event in paused mode
{
  const readable = new Readable();
  let dataCount = 0;

  readable.on('data', () => {
    dataCount++;
  });

  // Pause immediately (starts paused by default, but be explicit)
  readable.pause();
  readable.push('data1');
  readable.push('data2');

  setTimeout(() => {
    assert.strictEqual(
      dataCount,
      0,
      'data events should not fire in paused mode'
    );
    console.log('  ✓ No "data" event in paused mode');
  }, 10);
}

// Test 7: resume() emits buffered data
{
  const readable = new Readable();
  const receivedData = [];

  readable.on('data', (chunk) => {
    receivedData.push(chunk);
  });

  // Push data while paused
  readable.pause();
  readable.push('a');
  readable.push('b');
  readable.push('c');

  // Resume should emit all buffered data
  readable.resume();

  setTimeout(() => {
    assert.strictEqual(
      receivedData.length,
      3,
      'should receive all buffered data'
    );
    assert.deepStrictEqual(
      receivedData,
      ['a', 'b', 'c'],
      'data should be in correct order'
    );
    console.log('  ✓ resume() emits buffered data');
  }, 10);
}

// Test 8: 'readable' event fires only once until read
{
  const readable = new Readable();
  let readableCount = 0;

  readable.on('readable', () => {
    readableCount++;
  });

  readable.push('data1');
  readable.push('data2');
  readable.push('data3');

  setTimeout(() => {
    assert.strictEqual(readableCount, 1, 'readable should fire only once');

    // Read data, then push more
    readable.read();
    readable.push('data4');

    setTimeout(() => {
      assert.strictEqual(
        readableCount,
        2,
        'readable should fire again after read'
      );
      console.log('  ✓ "readable" event fires correctly with read()');
    }, 10);
  }, 10);
}

// Test 9: Event order: data events, then end
{
  const readable = new Readable();
  const events = [];

  readable.on('data', (chunk) => {
    events.push('data:' + chunk);
  });

  readable.on('end', () => {
    events.push('end');
  });

  readable.push('a');
  readable.push('b');
  readable.push(null);

  setTimeout(() => {
    assert.deepStrictEqual(
      events,
      ['data:a', 'data:b', 'end'],
      'events should be in correct order'
    );
    console.log('  ✓ Events fire in correct order (data, then end)');
  }, 20);
}

// Test 10: Multiple 'data' events
{
  const readable = new Readable();
  let dataCount = 0;

  readable.on('data', () => {
    dataCount++;
  });

  readable.push('1');
  readable.push('2');
  readable.push('3');

  setTimeout(() => {
    assert.strictEqual(dataCount, 3, 'should fire data event for each chunk');
    console.log('  ✓ Multiple "data" events fire correctly');
  }, 10);
}

setTimeout(() => {
  console.log('\n✅ All readable event tests passed');
}, 100);
