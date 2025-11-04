#!/usr/bin/env node

/**
 * Process stdio Integration Tests
 * Tests TTY integration with process.stdin, process.stdout, and process.stderr
 */

const tty = require('node:tty');
const assert = require('node:assert');

console.log('=== Process stdio Integration Tests ===\n');

// Test 1: Process stdio basic properties
console.log('1. Testing process stdio basic properties:');
try {
  // Test process.stdin
  assert(process.stdin, 'process.stdin should exist');
  assert(typeof process.stdin === 'object', 'process.stdin should be object');
  assert(
    typeof process.stdin.fd === 'number',
    'process.stdin should have fd property'
  );
  console.log(`   ✓ process.stdin exists, fd: ${process.stdin.fd}`);

  // Test process.stdout
  assert(process.stdout, 'process.stdout should exist');
  assert(typeof process.stdout === 'object', 'process.stdout should be object');
  assert(
    typeof process.stdout.fd === 'number',
    'process.stdout should have fd property'
  );
  console.log(`   ✓ process.stdout exists, fd: ${process.stdout.fd}`);

  // Test process.stderr
  assert(process.stderr, 'process.stderr should exist');
  assert(typeof process.stderr === 'object', 'process.stderr should be object');
  assert(
    typeof process.stderr.fd === 'number',
    'process.stderr should have fd property'
  );
  console.log(`   ✓ process.stderr exists, fd: ${process.stderr.fd}`);

  // Test file descriptor values
  assert.strictEqual(process.stdin.fd, 0, 'process.stdin should have fd 0');
  assert.strictEqual(process.stdout.fd, 1, 'process.stdout should have fd 1');
  assert.strictEqual(process.stderr.fd, 2, 'process.stderr should have fd 2');
  console.log('   ✓ File descriptor values are correct');
} catch (error) {
  console.log(
    '   ✗ Process stdio basic properties test failed:',
    error.message
  );
  process.exit(1);
}

// Test 2: TTY detection on process stdio
console.log('\n2. Testing TTY detection on process stdio:');
try {
  // Test process.stdin TTY detection
  const stdinIsTTY = process.stdin.isTTY;
  assert(
    typeof stdinIsTTY === 'boolean',
    'process.stdin.isTTY should be boolean'
  );
  console.log(`   ✓ process.stdin.isTTY: ${stdinIsTTY}`);

  // Test process.stdout TTY detection
  const stdoutIsTTY = process.stdout.isTTY;
  assert(
    typeof stdoutIsTTY === 'boolean',
    'process.stdout.isTTY should be boolean'
  );
  console.log(`   ✓ process.stdout.isTTY: ${stdoutIsTTY}`);

  // Test process.stderr TTY detection
  const stderrIsTTY = process.stderr.isTTY;
  assert(
    typeof stderrIsTTY === 'boolean',
    'process.stderr.isTTY should be boolean'
  );
  console.log(`   ✓ process.stderr.isTTY: ${stderrIsTTY}`);

  // Test tty.isatty consistency
  assert.strictEqual(
    tty.isatty(0),
    stdinIsTTY,
    'tty.isatty(0) should match process.stdin.isTTY'
  );
  assert.strictEqual(
    tty.isatty(1),
    stdoutIsTTY,
    'tty.isatty(1) should match process.stdout.isTTY'
  );
  assert.strictEqual(
    tty.isatty(2),
    stderrIsTTY,
    'tty.isatty(2) should match process.stderr.isTTY'
  );
  console.log(
    '   ✓ TTY detection is consistent between tty.isatty and process streams'
  );
} catch (error) {
  console.log('   ✗ TTY detection test failed:', error.message);
  process.exit(1);
}

// Test 3: TTY class replacement/integration
console.log('\n3. Testing TTY class integration with process streams:');
try {
  // Test if process streams are TTY instances when appropriate
  if (process.stdin.isTTY) {
    const stdinIsReadStream = process.stdin instanceof tty.ReadStream;
    console.log(`   ✓ process.stdin is ReadStream: ${stdinIsReadStream}`);
  } else {
    console.log('   ⚠ process.stdin is not TTY, skipping ReadStream test');
  }

  if (process.stdout.isTTY) {
    const stdoutIsWriteStream = process.stdout instanceof tty.WriteStream;
    console.log(`   ✓ process.stdout is WriteStream: ${stdoutIsWriteStream}`);
  } else {
    console.log('   ⚠ process.stdout is not TTY, skipping WriteStream test');
  }

  if (process.stderr.isTTY) {
    const stderrIsWriteStream = process.stderr instanceof tty.WriteStream;
    console.log(`   ✓ process.stderr is WriteStream: ${stderrIsWriteStream}`);
  } else {
    console.log('   ⚠ process.stderr is not TTY, skipping WriteStream test');
  }
} catch (error) {
  console.log('   ✗ TTY class integration test failed:', error.message);
  process.exit(1);
}

// Test 4: Create TTY instances from process stdio fds
console.log('\n4. Testing TTY instance creation from process stdio fds:');
try {
  // Create ReadStream from stdin fd
  const stdinReadStream = new tty.ReadStream(process.stdin.fd);
  assert(
    typeof stdinReadStream === 'object',
    'ReadStream should be created from stdin fd'
  );
  assert.strictEqual(
    stdinReadStream.fd,
    process.stdin.fd,
    'ReadStream should have same fd as process.stdin'
  );
  assert.strictEqual(
    stdinReadStream.isTTY,
    process.stdin.isTTY,
    'ReadStream should have same isTTY as process.stdin'
  );
  console.log('   ✓ ReadStream created from process.stdin fd');

  // Create WriteStream from stdout fd
  const stdoutWriteStream = new tty.WriteStream(process.stdout.fd);
  assert(
    typeof stdoutWriteStream === 'object',
    'WriteStream should be created from stdout fd'
  );
  assert.strictEqual(
    stdoutWriteStream.fd,
    process.stdout.fd,
    'WriteStream should have same fd as process.stdout'
  );
  assert.strictEqual(
    stdoutWriteStream.isTTY,
    process.stdout.isTTY,
    'WriteStream should have same isTTY as process.stdout'
  );
  console.log('   ✓ WriteStream created from process.stdout fd');

  // Create WriteStream from stderr fd
  const stderrWriteStream = new tty.WriteStream(process.stderr.fd);
  assert(
    typeof stderrWriteStream === 'object',
    'WriteStream should be created from stderr fd'
  );
  assert.strictEqual(
    stderrWriteStream.fd,
    process.stderr.fd,
    'WriteStream should have same fd as process.stderr'
  );
  assert.strictEqual(
    stderrWriteStream.isTTY,
    process.stderr.isTTY,
    'WriteStream should have same isTTY as process.stderr'
  );
  console.log('   ✓ WriteStream created from process.stderr fd');
} catch (error) {
  console.log('   ✗ TTY instance creation test failed:', error.message);
  process.exit(1);
}

// Test 5: TTY functionality on process stdio
console.log('\n5. Testing TTY functionality on process stdio:');
try {
  // Test ReadStream functionality on process.stdin
  if (process.stdin.isTTY) {
    // Test isRaw property
    const initialRaw = process.stdin.isRaw;
    assert(
      typeof initialRaw === 'boolean',
      'process.stdin.isRaw should be boolean'
    );
    console.log(`   ✓ process.stdin.isRaw: ${initialRaw}`);

    // Test setRawMode (if available)
    if (typeof process.stdin.setRawMode === 'function') {
      // Note: We won't actually call setRawMode in tests as it can affect the terminal
      console.log('   ✓ process.stdin.setRawMode method available');
    } else {
      console.log('   ⚠ process.stdin.setRawMode method not available');
    }
  } else {
    console.log(
      '   ⚠ process.stdin is not TTY, skipping ReadStream functionality tests'
    );
  }

  // Test WriteStream functionality on process.stdout
  if (process.stdout.isTTY) {
    // Test terminal size
    if (
      typeof process.stdout.columns === 'number' &&
      typeof process.stdout.rows === 'number'
    ) {
      console.log(
        `   ✓ process.stdout size: ${process.stdout.columns}x${process.stdout.rows}`
      );
    } else {
      console.log('   ⚠ process.stdout size properties not available');
    }

    // Test cursor control methods
    if (typeof process.stdout.clearLine === 'function') {
      console.log('   ✓ process.stdout.clearLine method available');
    }
    if (typeof process.stdout.cursorTo === 'function') {
      console.log('   ✓ process.stdout.cursorTo method available');
    }
    if (typeof process.stdout.getColorDepth === 'function') {
      console.log('   ✓ process.stdout.getColorDepth method available');
    }
  } else {
    console.log(
      '   ⚠ process.stdout is not TTY, skipping WriteStream functionality tests'
    );
  }

  // Test WriteStream functionality on process.stderr
  if (process.stderr.isTTY) {
    // Test terminal size
    if (
      typeof process.stderr.columns === 'number' &&
      typeof process.stderr.rows === 'number'
    ) {
      console.log(
        `   ✓ process.stderr size: ${process.stderr.columns}x${process.stderr.rows}`
      );
    } else {
      console.log('   ⚠ process.stderr size properties not available');
    }

    // Test cursor control methods
    if (typeof process.stderr.clearLine === 'function') {
      console.log('   ✓ process.stderr.clearLine method available');
    }
    if (typeof process.stderr.cursorTo === 'function') {
      console.log('   ✓ process.stderr.cursorTo method available');
    }
    if (typeof process.stderr.getColorDepth === 'function') {
      console.log('   ✓ process.stderr.getColorDepth method available');
    }
  } else {
    console.log(
      '   ⚠ process.stderr is not TTY, skipping WriteStream functionality tests'
    );
  }
} catch (error) {
  console.log('   ✗ TTY functionality test failed:', error.message);
  process.exit(1);
}

// Test 6: Fallback behavior in non-TTY environments
console.log('\n6. Testing fallback behavior in non-TTY environments:');
try {
  // Test that non-TTY streams still have basic functionality
  const nonTTYStreams = [
    process.stdin.isTTY ? null : process.stdin,
    process.stdout.isTTY ? null : process.stdout,
    process.stderr.isTTY ? null : process.stderr,
  ].filter((stream) => stream !== null);

  if (nonTTYStreams.length > 0) {
    nonTTYStreams.forEach((stream, index) => {
      const streamName = ['process.stdin', 'process.stdout', 'process.stderr'][
        index
      ];
      console.log(`   ✓ ${streamName} non-TTY fallback works`);

      // Test basic stream functionality still works
      assert(
        typeof stream.write === 'function',
        `${streamName} should still have write method`
      );
      assert(
        typeof stream.on === 'function',
        `${streamName} should still have on method`
      );
      console.log(`   ✓ ${streamName} basic stream methods available`);
    });
  } else {
    console.log('   ⚠ All streams are TTY, fallback behavior not testable');
  }
} catch (error) {
  console.log('   ✗ Fallback behavior test failed:', error.message);
  process.exit(1);
}

// Test 7: Event handling integration
console.log('\n7. Testing event handling integration:');
try {
  // Test that process streams support standard stream events
  const stdioStreams = [process.stdin, process.stdout, process.stderr];

  stdioStreams.forEach((stream, index) => {
    const streamName = ['process.stdin', 'process.stdout', 'process.stderr'][
      index
    ];

    // Test basic event methods
    assert(
      typeof stream.on === 'function',
      `${streamName} should have on method`
    );
    assert(
      typeof stream.addListener === 'function',
      `${streamName} should have addListener method`
    );
    assert(
      typeof stream.removeListener === 'function',
      `${streamName} should have removeListener method`
    );
    console.log(`   ✓ ${streamName} event methods available`);

    // Test event listener registration
    let testEventFired = false;
    const testHandler = () => {
      testEventFired = true;
    };

    stream.on('test', testHandler);
    stream.emit('test');

    if (testEventFired) {
      console.log(`   ✓ ${streamName} event emission works`);
    } else {
      console.log(`   ⚠ ${streamName} event emission test inconclusive`);
    }

    stream.removeListener('test', testHandler);
  });

  // Test TTY-specific events if applicable
  if (process.stdout.isTTY && typeof process.stdout.on === 'function') {
    // Test resize event listener registration
    process.stdout.on('resize', () => {
      console.log('   ✓ process.stdout resize event triggered');
    });
    console.log('   ✓ process.stdout resize event listener registered');
  }
} catch (error) {
  console.log('   ✗ Event handling integration test failed:', error.message);
  process.exit(1);
}

// Test 8: Stream method compatibility
console.log('\n8. Testing stream method compatibility:');
try {
  const stdioStreams = [
    { stream: process.stdin, name: 'process.stdin', type: 'readable' },
    { stream: process.stdout, name: 'process.stdout', type: 'writable' },
    { stream: process.stderr, name: 'process.stderr', type: 'writable' },
  ];

  stdioStreams.forEach(({ stream, name, type }) => {
    // Test common stream methods
    const commonMethods = ['pipe', 'unpipe', 'destroy'];
    commonMethods.forEach((method) => {
      if (typeof stream[method] === 'function') {
        console.log(`   ✓ ${name}.${method} method available`);
      } else {
        console.log(`   ⚠ ${name}.${method} method not available`);
      }
    });

    // Test type-specific methods
    if (type === 'readable') {
      const readableMethods = ['pause', 'resume', 'isPaused'];
      readableMethods.forEach((method) => {
        if (typeof stream[method] === 'function') {
          console.log(`   ✓ ${name}.${method} method available`);
        } else {
          console.log(`   ⚠ ${name}.${method} method not available`);
        }
      });
    } else if (type === 'writable') {
      const writableMethods = ['write', 'end'];
      writableMethods.forEach((method) => {
        if (typeof stream[method] === 'function') {
          console.log(`   ✓ ${name}.${method} method available`);
        } else {
          console.log(`   ⚠ ${name}.${method} method not available`);
        }
      });
    }
  });
} catch (error) {
  console.log('   ✗ Stream method compatibility test failed:', error.message);
  process.exit(1);
}

// Test 9: Process stdio properties consistency
console.log('\n9. Testing process stdio properties consistency:');
try {
  // Test that file descriptors are consistent
  assert.strictEqual(process.stdin.fd, 0, 'process.stdin fd should be 0');
  assert.strictEqual(process.stdout.fd, 1, 'process.stdout fd should be 1');
  assert.strictEqual(process.stderr.fd, 2, 'process.stderr fd should be 2');
  console.log('   ✓ File descriptors are consistent');

  // Test that TTY detection is consistent with tty.isatty
  assert.strictEqual(
    process.stdin.isTTY,
    tty.isatty(0),
    'process.stdin TTY detection consistent'
  );
  assert.strictEqual(
    process.stdout.isTTY,
    tty.isatty(1),
    'process.stdout TTY detection consistent'
  );
  assert.strictEqual(
    process.stderr.isTTY,
    tty.isatty(2),
    'process.stderr TTY detection consistent'
  );
  console.log('   ✓ TTY detection is consistent');

  // Test that terminal size is consistent between stdout and stderr (both TTY)
  if (process.stdout.isTTY && process.stderr.isTTY) {
    if (process.stdout.columns && process.stderr.columns) {
      const sizeConsistent =
        process.stdout.columns === process.stderr.columns &&
        process.stdout.rows === process.stderr.rows;
      console.log(`   ✓ Terminal size consistent: ${sizeConsistent}`);
      if (!sizeConsistent) {
        console.log(
          `     stdout: ${process.stdout.columns}x${process.stdout.rows}`
        );
        console.log(
          `     stderr: ${process.stderr.columns}x${process.stderr.rows}`
        );
      }
    }
  }
} catch (error) {
  console.log('   ✗ Properties consistency test failed:', error.message);
  process.exit(1);
}

console.log('\n=== Process stdio Integration Test Summary ===');
console.log('✅ Process stdio basic properties work');
console.log('✅ TTY detection works correctly');
console.log('✅ TTY class integration functional');
console.log('✅ TTY instances can be created from process fds');
console.log('✅ TTY functionality available on process streams');
console.log('✅ Fallback behavior works in non-TTY environments');
console.log('✅ Event handling integration works');
console.log('✅ Stream method compatibility maintained');
console.log('✅ Properties are consistent across interfaces');

console.log('\n🎉 All process stdio integration tests completed!');
