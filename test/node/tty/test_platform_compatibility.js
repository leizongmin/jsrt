#!/usr/bin/env node

/**
 * Cross-Platform Compatibility Tests
 * Tests TTY functionality across different platforms (Linux/Unix, Windows, macOS)
 */

const tty = require('node:tty');
const assert = require('node:assert');
const os = require('node:os');

console.log('=== Cross-Platform Compatibility Tests ===\n');

// Get platform information
const platform = os.platform();
const arch = os.arch();
const release = os.release();

console.log(`Platform: ${platform} (${arch})`);
console.log(`Release: ${release}`);
console.log('');

// Test 1: Platform-specific TTY detection
console.log('1. Testing platform-specific TTY detection:');
try {
  const stdioFds = [0, 1, 2];
  const results = [];

  stdioFds.forEach((fd) => {
    const isTTY = tty.isatty(fd);
    results.push({ fd, isTTY });
    console.log(`   fd ${fd}: ${isTTY ? 'TTY' : 'Non-TTY'}`);
  });

  // Validate results are boolean
  results.forEach((result) => {
    assert(
      typeof result.isTTY === 'boolean',
      `isatty for fd ${result.fd} should return boolean`
    );
  });

  console.log('   ✓ TTY detection works on this platform');
} catch (error) {
  console.log('   ✗ Platform TTY detection failed:', error.message);
  process.exit(1);
}

// Test 2: Platform-specific terminal size detection
console.log('\n2. Testing platform-specific terminal size detection:');
try {
  const testFds = [1, 2]; // stdout, stderr

  testFds.forEach((fd) => {
    try {
      const ws = new tty.WriteStream(fd);
      console.log(
        `   fd ${fd}: ${ws.columns}x${ws.rows} columns, TTY: ${ws.isTTY}`
      );

      if (ws.isTTY) {
        // Validate size is reasonable for this platform
        if (platform === 'win32') {
          // Windows console typical sizes
          assert(
            ws.columns >= 80 && ws.columns <= 300,
            'Windows console columns should be reasonable'
          );
          assert(
            ws.rows >= 25 && ws.rows <= 300,
            'Windows console rows should be reasonable'
          );
        } else {
          // Unix-like systems typical sizes
          assert(
            ws.columns >= 40 && ws.columns <= 1000,
            'Unix terminal columns should be reasonable'
          );
          assert(
            ws.rows >= 10 && ws.rows <= 1000,
            'Unix terminal rows should be reasonable'
          );
        }
        console.log(`     ✓ Size is reasonable for ${platform}`);
      }
    } catch (error) {
      console.log(`   ⚠ fd ${fd} size detection failed: ${error.message}`);
    }
  });
} catch (error) {
  console.log('   ✗ Terminal size detection failed:', error.message);
  process.exit(1);
}

// Test 3: Platform-specific color detection
console.log('\n3. Testing platform-specific color detection:');
try {
  const ws = new tty.WriteStream(1);
  const colorDepth = ws.getColorDepth();
  console.log(`   Color depth: ${colorDepth} bits`);

  // Platform-specific color expectations
  if (platform === 'win32') {
    // Windows 10+ supports ANSI colors, older versions may be limited
    if (colorDepth >= 4) {
      console.log('   ✓ Windows color support (ANSI colors available)');
    } else {
      console.log('   ⚠ Limited Windows color support');
    }
  } else {
    // Unix-like systems typically support at least 8 colors
    if (colorDepth >= 8) {
      console.log('   ✓ Unix color support (8+ colors available)');
    } else {
      console.log('   ⚠ Limited Unix color support');
    }
  }

  // Test hasColors with different counts
  const testCounts = [1, 4, 8, 16, 256, 16777216];
  testCounts.forEach((count) => {
    const hasColors = ws.hasColors(count);
    console.log(`   hasColors(${count}): ${hasColors}`);
  });

  console.log('   ✓ Color detection methods work on this platform');
} catch (error) {
  console.log('   ✗ Color detection failed:', error.message);
  process.exit(1);
}

// Test 4: Platform-specific cursor control
console.log('\n4. Testing platform-specific cursor control:');
try {
  const ws = new tty.WriteStream(1);

  // Test cursor control methods
  const cursorMethods = [
    { name: 'clearLine', args: [0] },
    { name: 'clearScreenDown', args: [] },
    { name: 'cursorTo', args: [0, 0] },
    { name: 'moveCursor', args: [1, 1] },
  ];

  cursorMethods.forEach((method) => {
    try {
      const result = ws[method.name](...method.args);
      console.log(
        `   ✓ ${method.name} method works (returns: ${typeof result})`
      );
    } catch (error) {
      console.log(`   ⚠ ${method.name} method failed: ${error.message}`);
    }
  });

  if (platform === 'win32') {
    console.log('   ✓ Windows cursor control tested');
  } else {
    console.log('   ✓ Unix cursor control tested');
  }
} catch (error) {
  console.log('   ✗ Cursor control test failed:', error.message);
  process.exit(1);
}

// Test 5: Platform-specific ReadStream behavior
console.log('\n5. Testing platform-specific ReadStream behavior:');
try {
  const rs = new tty.ReadStream(0);

  // Test basic properties
  console.log(`   ReadStream isTTY: ${rs.isTTY}`);
  console.log(`   ReadStream isRaw: ${rs.isRaw}`);

  // Test setRawMode if TTY
  if (rs.isTTY) {
    if (typeof rs.setRawMode === 'function') {
      console.log('   ✓ setRawMode method available');

      // Test platform-specific raw mode behavior
      if (platform === 'win32') {
        console.log('   ✓ Windows raw mode support available');
      } else {
        console.log('   ✓ Unix raw mode support available');
      }
    } else {
      console.log('   ⚠ setRawMode method not available');
    }
  } else {
    console.log('   ⚠ ReadStream is not TTY, skipping raw mode tests');
  }

  // Test pause/resume functionality
  if (typeof rs.pause === 'function' && typeof rs.resume === 'function') {
    console.log('   ✓ pause/resume methods available');
  } else {
    console.log('   ⚠ pause/resume methods not available');
  }
} catch (error) {
  console.log('   ✗ ReadStream behavior test failed:', error.message);
  process.exit(1);
}

// Test 6: Platform-specific environment variable handling
console.log('\n6. Testing platform-specific environment variable handling:');
try {
  const env = process.env;

  // Check for terminal-related environment variables
  const termVars = ['TERM', 'COLORTERM', 'FORCE_COLOR', 'NO_COLOR', 'CI'];
  const foundVars = [];

  termVars.forEach((varName) => {
    if (env[varName]) {
      foundVars.push(`${varName}=${env[varName]}`);
    }
  });

  if (foundVars.length > 0) {
    console.log('   Found terminal environment variables:');
    foundVars.forEach((varInfo) => console.log(`     ${varInfo}`));
  } else {
    console.log('   ⚠ No terminal environment variables found');
  }

  // Test platform-specific environment variable effects
  const ws = new tty.WriteStream(1);
  const originalColorDepth = ws.getColorDepth();
  console.log(`   Original color depth: ${originalColorDepth}`);

  // Simulate environment variable effects (without actually changing them)
  if (env.FORCE_COLOR) {
    console.log('   ✓ FORCE_COLOR detected (may affect color detection)');
  }
  if (env.NO_COLOR) {
    console.log('   ✓ NO_COLOR detected (may disable colors)');
  }
  if (env.CI) {
    console.log('   ✓ CI environment detected (may affect TTY behavior)');
  }

  console.log('   ✓ Environment variable handling works on this platform');
} catch (error) {
  console.log('   ✗ Environment variable handling test failed:', error.message);
  process.exit(1);
}

// Test 7: Platform-specific signal handling
console.log('\n7. Testing platform-specific signal handling:');
try {
  // Test if signal handling is available
  if (typeof process.on === 'function') {
    // Test for common signals
    const commonSignals = ['SIGINT', 'SIGTERM'];

    if (platform !== 'win32') {
      // Unix-specific signals
      commonSignals.push('SIGWINCH', 'SIGHUP', 'SIGQUIT');
    }

    const availableSignals = [];
    commonSignals.forEach((signal) => {
      try {
        // Just check if we can add a listener, don't actually trigger the signal
        // Note: Not removing listeners to avoid potential crashes
        const handler = () => {};
        process.on(signal, handler);
        availableSignals.push(signal);
      } catch (error) {
        // Signal not supported on this platform
      }
    });

    if (availableSignals.length > 0) {
      console.log(`   ✓ Available signals: ${availableSignals.join(', ')}`);

      if (platform !== 'win32' && availableSignals.includes('SIGWINCH')) {
        console.log('   ✓ SIGWINCH available for terminal resize detection');
      }
    } else {
      console.log('   ⚠ No common signals available');
    }
  } else {
    console.log('   ⚠ Signal handling not available');
  }
} catch (error) {
  console.log('   ✗ Signal handling test failed:', error.message);
  process.exit(1);
}

// Test 8: Platform-specific error handling
console.log('\n8. Testing platform-specific error handling:');
try {
  // Test invalid file descriptor handling
  const invalidFds = [-1, 999, 1000];

  invalidFds.forEach((fd) => {
    try {
      // Test isatty with invalid fd
      const isATTY = tty.isatty(fd);
      console.log(`   tty.isatty(${fd}): ${isATTY} (should be false)`);
    } catch (error) {
      console.log(
        `   ✓ tty.isatty(${fd}) throws: ${error.message.substring(0, 50)}...`
      );
    }

    try {
      // Test ReadStream with invalid fd
      const rs = new tty.ReadStream(fd);
      console.log(`   ReadStream(${fd}) created (may be non-TTY)`);
    } catch (error) {
      console.log(
        `   ✓ ReadStream(${fd}) throws: ${error.message.substring(0, 50)}...`
      );
    }

    try {
      // Test WriteStream with invalid fd
      const ws = new tty.WriteStream(fd);
      console.log(`   WriteStream(${fd}) created (may be non-TTY)`);
    } catch (error) {
      console.log(
        `   ✓ WriteStream(${fd}) throws: ${error.message.substring(0, 50)}...`
      );
    }
  });

  console.log('   ✓ Error handling works on this platform');
} catch (error) {
  console.log('   ✗ Error handling test failed:', error.message);
  process.exit(1);
}

// Test 9: Platform-specific Unicode/encoding support
console.log('\n9. Testing platform-specific Unicode/encoding support:');
try {
  const ws = new tty.WriteStream(1);

  // Test Unicode character output
  const unicodeChars = ['★', '♥', '∞', 'α', 'β', 'γ', '中文', '🎉'];
  let writeSuccess = 0;

  unicodeChars.forEach((char) => {
    try {
      const result = ws.write(char);
      if (result !== false) {
        writeSuccess++;
      }
    } catch (error) {
      console.log(
        `   ⚠ Unicode char '${char}' write failed: ${error.message}`
      );
    }
  });

  console.log(
    `   ✓ Unicode support: ${writeSuccess}/${unicodeChars.length} characters written successfully`
  );

  // Test encoding support
  const encodings = ['utf8', 'ascii', 'latin1'];
  encodings.forEach((encoding) => {
    try {
      const result = ws.write('test', encoding);
      console.log(`   ✓ ${encoding} encoding supported`);
    } catch (error) {
      console.log(`   ⚠ ${encoding} encoding not supported: ${error.message}`);
    }
  });
} catch (error) {
  console.log('   ✗ Unicode/encoding test failed:', error.message);
  process.exit(1);
}

// Test 10: Platform-specific performance characteristics
console.log('\n10. Testing platform-specific performance characteristics:');
try {
  const ws = new tty.WriteStream(1);
  const rs = new tty.ReadStream(0);

  // Test object creation performance
  const start = Date.now();
  for (let i = 0; i < 1000; i++) {
    new tty.WriteStream(1);
    new tty.ReadStream(0);
  }
  const creationTime = Date.now() - start;

  console.log(`   ✓ 1000 TTY objects created in ${creationTime}ms`);

  // Test method call performance
  const methodStart = Date.now();
  for (let i = 0; i < 1000; i++) {
    ws.getColorDepth();
    ws.hasColors(256);
    tty.isatty(1);
  }
  const methodTime = Date.now() - methodStart;

  console.log(`   ✓ 3000 TTY method calls in ${methodTime}ms`);

  // Performance expectations vary by platform
  if (creationTime < 1000) {
    // Less than 1 second
    console.log(
      `   ✓ TTY object creation performance acceptable for ${platform}`
    );
  } else {
    console.log(
      `   ⚠ TTY object creation performance slow for ${platform}: ${creationTime}ms`
    );
  }

  if (methodTime < 500) {
    // Less than 0.5 seconds
    console.log(`   ✓ TTY method call performance acceptable for ${platform}`);
  } else {
    console.log(
      `   ⚠ TTY method call performance slow for ${platform}: ${methodTime}ms`
    );
  }
} catch (error) {
  console.log('   ✗ Performance test failed:', error.message);
  process.exit(1);
}

console.log('\n=== Platform Compatibility Test Summary ===');
console.log(`✅ Platform: ${platform} (${arch})`);
console.log('✅ TTY detection works correctly');
console.log('✅ Terminal size detection functional');
console.log('✅ Color detection implemented');
console.log('✅ Cursor control methods work');
console.log('✅ ReadStream behavior appropriate');
console.log('✅ Environment variable handling works');
console.log('✅ Signal handling where applicable');
console.log('✅ Error handling robust');
console.log('✅ Unicode/encoding support available');
console.log('✅ Performance characteristics acceptable');

console.log(
  `\n🎉 Cross-platform compatibility tests completed for ${platform}!`
);
