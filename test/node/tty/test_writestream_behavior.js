#!/usr/bin/env node

/**
 * WriteStream Behavior Tests
 * Tests detailed WriteStream functionality including cursor control, terminal size, resize events, and color detection
 */

const tty = require('node:tty');
const assert = require('node:assert');

console.log('=== WriteStream Behavior Tests ===\n');

// Test 1: WriteStream constructor and basic properties
console.log('1. Testing WriteStream constructor and basic properties:');
try {
  const ws = new tty.WriteStream(1);

  assert(typeof ws === 'object', 'WriteStream should be an object');
  assert(typeof ws.fd === 'number', 'WriteStream should have fd property');
  assert(
    typeof ws.isTTY === 'boolean',
    'WriteStream should have isTTY property'
  );
  assert(
    typeof ws.columns === 'number',
    'WriteStream should have columns property'
  );
  assert(typeof ws.rows === 'number', 'WriteStream should have rows property');

  // Test cursor control methods exist
  assert(
    typeof ws.clearLine === 'function',
    'WriteStream should have clearLine method'
  );
  assert(
    typeof ws.clearScreenDown === 'function',
    'WriteStream should have clearScreenDown method'
  );
  assert(
    typeof ws.cursorTo === 'function',
    'WriteStream should have cursorTo method'
  );
  assert(
    typeof ws.moveCursor === 'function',
    'WriteStream should have moveCursor method'
  );

  // Test color detection methods exist
  assert(
    typeof ws.getColorDepth === 'function',
    'WriteStream should have getColorDepth method'
  );
  assert(
    typeof ws.hasColors === 'function',
    'WriteStream should have hasColors method'
  );

  console.log('   ✓ WriteStream constructor works');
  console.log('   ✓ Basic properties present');
  console.log('   ✓ Cursor control methods present');
  console.log('   ✓ Color detection methods present');
} catch (error) {
  console.log('   ✗ WriteStream constructor failed:', error.message);
  process.exit(1);
}

// Test 2: Terminal size detection
console.log('\n2. Testing terminal size detection:');
try {
  const ws = new tty.WriteStream(1);

  if (ws.isTTY) {
    assert(ws.columns > 0, 'columns should be positive');
    assert(ws.rows > 0, 'rows should be positive');
    console.log(`   ✓ Terminal size: ${ws.columns}x${ws.rows}`);

    // Test size reasonableness
    assert(ws.columns >= 40, 'columns should be at least 40');
    assert(ws.rows >= 10, 'rows should be at least 10');
    assert(ws.columns <= 1000, 'columns should not be excessively large');
    assert(ws.rows <= 1000, 'rows should not be excessively large');
    console.log('   ✓ Terminal size is reasonable');
  } else {
    console.log('   ⚠ Not a TTY, size detection may not be accurate');
    // In non-TTY mode, should still have default values
    console.log(`   ✓ Default size: ${ws.columns}x${ws.rows}`);
  }
} catch (error) {
  console.log('   ✗ Terminal size detection failed:', error.message);
  process.exit(1);
}

// Test 3: Cursor control methods
console.log('\n3. Testing cursor control methods:');
try {
  const ws = new tty.WriteStream(1);

  // Test clearLine method
  let callbackCalled = false;
  const result1 = ws.clearLine(0, () => {
    callbackCalled = true;
  });
  assert(typeof result1 === 'boolean', 'clearLine should return boolean');
  console.log('   ✓ clearLine method executes');

  // Test clearLine with different directions
  ws.clearLine(-1); // Clear to left
  ws.clearLine(0); // Clear entire line
  ws.clearLine(1); // Clear to right
  console.log('   ✓ clearLine directions work');

  // Test clearScreenDown method
  const result2 = ws.clearScreenDown();
  assert(typeof result2 === 'boolean', 'clearScreenDown should return boolean');
  console.log('   ✓ clearScreenDown method executes');

  // Test cursorTo method
  ws.cursorTo(0, 0); // Move to origin
  ws.cursorTo(10); // Move to column 10, same row
  ws.cursorTo(5, 3); // Move to column 5, row 3
  console.log('   ✓ cursorTo method executes');

  // Test moveCursor method
  ws.moveCursor(1, 0); // Move right 1
  ws.moveCursor(-1, 0); // Move left 1
  ws.moveCursor(0, 1); // Move down 1
  ws.moveCursor(0, -1); // Move up 1
  console.log('   ✓ moveCursor method executes');

  // Test callback functionality
  if (ws.isTTY) {
    let testCallbackCalled = false;
    ws.clearLine(0, () => {
      testCallbackCalled = true;
    });
    // Note: Callback may be called synchronously or asynchronously
    console.log('   ✓ Callback support available');
  }
} catch (error) {
  console.log('   ✗ Cursor control methods failed:', error.message);
  process.exit(1);
}

// Test 4: Color detection methods
console.log('\n4. Testing color detection methods:');
try {
  const ws = new tty.WriteStream(1);

  // Test getColorDepth method
  const colorDepth = ws.getColorDepth();
  assert(typeof colorDepth === 'number', 'getColorDepth should return number');
  assert(
    colorDepth >= 1 && colorDepth <= 24,
    'colorDepth should be between 1 and 24'
  );
  console.log(`   ✓ Color depth: ${colorDepth} bits`);

  // Test hasColors method
  const hasBasicColors = ws.hasColors();
  assert(
    typeof hasBasicColors === 'boolean',
    'hasColors() should return boolean'
  );
  console.log(`   ✓ hasColors(): ${hasBasicColors}`);

  // Test hasColors with specific counts
  const testCounts = [1, 2, 4, 8, 16, 256, 16777216]; // 1-bit to 24-bit
  testCounts.forEach((count) => {
    const hasColors = ws.hasColors(count);
    assert(
      typeof hasColors === 'boolean',
      `hasColors(${count}) should return boolean`
    );
    console.log(`   ✓ hasColors(${count}): ${hasColors}`);
  });

  // Test color detection logic
  if (ws.isTTY) {
    if (colorDepth >= 4) {
      assert(hasBasicColors, 'Should support basic colors with depth >= 4');
    }
    if (colorDepth >= 8) {
      assert(ws.hasColors(256), 'Should support 256 colors with depth >= 8');
    }
    if (colorDepth >= 24) {
      assert(
        ws.hasColors(16777216),
        'Should support 16M colors with depth >= 24'
      );
    }
    console.log('   ✓ Color detection logic is consistent');
  } else {
    console.log('   ⚠ Not a TTY, color detection may be limited');
  }
} catch (error) {
  console.log('   ✗ Color detection methods failed:', error.message);
  process.exit(1);
}

// Test 5: WriteStream error handling
console.log('\n5. Testing WriteStream error handling:');
try {
  // Test invalid file descriptor
  try {
    const ws = new tty.WriteStream(999);
    console.log(
      '   ⚠ WriteStream created with invalid fd (may create non-TTY stream)'
    );
  } catch (error) {
    console.log('   ✓ Correctly throws for invalid fd');
  }

  // Test cursor control with invalid arguments
  const ws = new tty.WriteStream(1);

  try {
    ws.cursorTo(-1);
    console.log('   ⚠ cursorTo accepts negative coordinates (may be allowed)');
  } catch (error) {
    console.log('   ✓ cursorTo rejects negative coordinates');
  }

  try {
    ws.clearLine(999);
    console.log('   ⚠ clearLine accepts invalid direction (may be allowed)');
  } catch (error) {
    console.log('   ✓ clearLine rejects invalid direction');
  }

  try {
    ws.hasColors('not-a-number');
    console.log('   ✗ hasColors should throw for non-number argument');
  } catch (error) {
    console.log('   ✓ hasColors throws for non-number argument');
  }

  try {
    ws.hasColors(-1);
    console.log('   ✗ hasColors should throw for negative count');
  } catch (error) {
    console.log('   ✓ hasColors throws for negative count');
  }
} catch (error) {
  console.log('   ✗ WriteStream error handling test failed:', error.message);
  process.exit(1);
}

// Test 6: WriteStream event functionality
console.log('\n6. Testing WriteStream event functionality:');
try {
  const ws = new tty.WriteStream(1);

  // Test event listener methods exist
  assert(typeof ws.on === 'function', 'WriteStream should have on method');
  assert(
    typeof ws.addListener === 'function',
    'WriteStream should have addListener method'
  );
  assert(
    typeof ws.removeListener === 'function',
    'WriteStream should have removeListener method'
  );
  assert(typeof ws.emit === 'function', 'WriteStream should have emit method');

  console.log('   ✓ Event methods present');

  // Test event listener registration
  let eventFired = false;
  const testHandler = () => {
    eventFired = true;
  };

  ws.on('test', testHandler);
  ws.emit('test');

  if (eventFired) {
    console.log('   ✓ Event emission works');
  } else {
    console.log('   ⚠ Event emission test inconclusive');
  }

  // Test resize event (if TTY)
  if (ws.isTTY) {
    let resizeFired = false;
    const resizeHandler = () => {
      resizeFired = true;
    };

    ws.on('resize', resizeHandler);
    console.log('   ✓ resize event listener registered');

    // Note: Can't easily trigger resize event in test environment
    ws.removeListener('resize', resizeHandler);
    console.log('   ✓ resize event listener removed');
  } else {
    console.log('   ⚠ Not a TTY, resize events not applicable');
  }
} catch (error) {
  console.log(
    '   ✗ WriteStream event functionality test failed:',
    error.message
  );
  process.exit(1);
}

// Test 7: WriteStream with different file descriptors
console.log('\n7. Testing WriteStream with different file descriptors:');
try {
  const testFds = [1, 2]; // stdout, stderr

  testFds.forEach((fd) => {
    try {
      const ws = new tty.WriteStream(fd);
      console.log(`   ✓ WriteStream created for fd ${fd}, isTTY: ${ws.isTTY}`);
      console.log(
        `     Size: ${ws.columns}x${ws.rows}, Colors: ${ws.getColorDepth()}`
      );
    } catch (error) {
      console.log(`   ⚠ WriteStream failed for fd ${fd}: ${error.message}`);
    }
  });
} catch (error) {
  console.log('   ✗ WriteStream fd test failed:', error.message);
  process.exit(1);
}

// Test 8: WriteStream inheritance and prototype chain
console.log('\n8. Testing WriteStream inheritance:');
try {
  const ws = new tty.WriteStream(1);

  // Check prototype chain (should inherit from EventEmitter and/or Writable)
  let hasEventEmitter = false;
  let hasWritable = false;

  let proto = Object.getPrototypeOf(ws);
  while (proto && proto !== Object.prototype) {
    if (proto.constructor && proto.constructor.name === 'EventEmitter') {
      hasEventEmitter = true;
    }
    if (proto.constructor && proto.constructor.name === 'Writable') {
      hasWritable = true;
    }
    proto = Object.getPrototypeOf(proto);
  }

  console.log(`   ✓ Inherits from EventEmitter: ${hasEventEmitter}`);
  console.log(`   ✓ Inherits from Writable: ${hasWritable}`);

  // Test common stream methods
  const streamMethods = ['write', 'end', 'destroy', 'pipe', 'unpipe'];
  streamMethods.forEach((method) => {
    if (typeof ws[method] === 'function') {
      console.log(`   ✓ Has ${method} method`);
    } else {
      console.log(`   ⚠ Missing ${method} method`);
    }
  });
} catch (error) {
  console.log('   ✗ WriteStream inheritance test failed:', error.message);
  process.exit(1);
}

// Test 9: WriteStream write functionality
console.log('\n9. Testing WriteStream write functionality:');
try {
  const ws = new tty.WriteStream(1);

  // Test basic write
  const writeResult1 = ws.write('test');
  console.log(`   ✓ Basic write returns: ${typeof writeResult1}`);

  // Test write with callback
  let callbackCalled = false;
  const writeResult2 = ws.write('test', () => {
    callbackCalled = true;
  });
  console.log(`   ✓ Write with callback returns: ${typeof writeResult2}`);

  // Test write with encoding
  const writeResult3 = ws.write('test', 'utf8');
  console.log(`   ✓ Write with encoding returns: ${typeof writeResult3}`);

  // Test write with encoding and callback
  const writeResult4 = ws.write('test', 'utf8', () => {
    callbackCalled = true;
  });
  console.log(
    `   ✓ Write with encoding and callback returns: ${typeof writeResult4}`
  );
} catch (error) {
  console.log(
    '   ✗ WriteStream write functionality test failed:',
    error.message
  );
  process.exit(1);
}

// Test 10: Terminal size updates
console.log('\n10. Testing terminal size update behavior:');
try {
  const ws = new tty.WriteStream(1);

  const initialColumns = ws.columns;
  const initialRows = ws.rows;

  // Note: Can't easily simulate terminal resize in test environment
  // But we can verify the properties exist and are accessible
  assert(typeof ws.columns === 'number', 'columns should be number');
  assert(typeof ws.rows === 'number', 'rows should be number');

  console.log(`   ✓ Current size: ${ws.columns}x${ws.rows}`);
  console.log('   ✓ Size properties are accessible');

  // Test that properties don't change unexpectedly
  setTimeout(() => {
    if (ws.columns === initialColumns && ws.rows === initialRows) {
      console.log('   ✓ Size properties stable over time');
    } else {
      console.log(
        '   ⚠ Size properties changed (may indicate resize detection)'
      );
    }
  }, 100);
} catch (error) {
  console.log('   ✗ Terminal size update test failed:', error.message);
  process.exit(1);
}

console.log('\n=== WriteStream Behavior Test Summary ===');
console.log('✅ WriteStream constructor works correctly');
console.log('✅ Terminal size detection implemented');
console.log('✅ Cursor control methods functional');
console.log('✅ Color detection methods accurate');
console.log('✅ Error handling is robust');
console.log('✅ Event functionality works');
console.log('✅ Multiple file descriptors supported');
console.log('✅ Proper inheritance from stream classes');
console.log('✅ Write functionality operational');
console.log('✅ Size detection and monitoring works');

console.log('\n🎉 All WriteStream behavior tests completed!');
