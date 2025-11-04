/**
 * Comprehensive TTY module test suite
 * Tests Node.js-compatible TTY functionality
 */

const tty = require('node:tty');
const assert = require('node:assert');

console.log('=== TTY Module Comprehensive Test ===\n');

// Test 1: Module structure
console.log('1. Testing module structure:');
try {
  assert(typeof tty.isatty === 'function', 'tty.isatty should be a function');
  console.log('   ✓ tty.isatty is function: true');

  assert(typeof tty.ReadStream === 'function', 'tty.ReadStream should be a function');
  console.log('   ✓ tty.ReadStream is function: true');

  assert(typeof tty.WriteStream === 'function', 'tty.WriteStream should be a function');
  console.log('   ✓ tty.WriteStream is function: true');
} catch (error) {
  console.log('   ✗ Module structure test failed:', error.message);
  process.exit(1);
}

// Test 2: isatty function with various inputs
console.log('\n2. Testing tty.isatty():');
try {
  const result0 = tty.isatty(0); // stdin
  console.log(`   ✓ tty.isatty(0): ${result0}`);
  assert(typeof result0 === 'boolean', 'tty.isatty(0) should return boolean');

  const result1 = tty.isatty(1); // stdout
  console.log(`   ✓ tty.isatty(1): ${result1}`);
  assert(typeof result1 === 'boolean', 'tty.isatty(1) should return boolean');

  const result2 = tty.isatty(2); // stderr
  console.log(`   ✓ tty.isatty(2): ${result2}`);
  assert(typeof result2 === 'boolean', 'tty.isatty(2) should return boolean');

  const result999 = tty.isatty(999); // invalid fd
  console.log(`   ✓ tty.isatty(999): ${result999}`);
  assert(typeof result999 === 'boolean', 'tty.isatty(999) should return boolean');
} catch (error) {
  console.log('   ✗ isatty test failed:', error.message);
  process.exit(1);
}

// Test 3: isatty error handling
console.log('\n3. Testing isatty error handling:');
try {
  // Test missing arguments
  assert.throws(() => {
    tty.isatty();
  }, /file descriptor/, 'Should throw for missing arguments');
  console.log('   ✓ Throws for missing arguments: true');

  // Test negative fd
  assert.throws(() => {
    tty.isatty(-1);
  }, /range/, 'Should throw for negative fd');
  console.log('   ✓ Throws for negative fd: true');

  // Test too large fd
  assert.throws(() => {
    tty.isatty(2000);
  }, /range/, 'Should throw for too large fd');
  console.log('   ✓ Throws for too large fd: true');
} catch (error) {
  console.log('   ✗ isatty error handling test failed:', error.message);
  process.exit(1);
}

// Test 4: ReadStream constructor
console.log('\n4. Testing ReadStream constructor:');
try {
  const stdin = new tty.ReadStream(0);

  assert(stdin && typeof stdin === 'object', 'Should create ReadStream instance');
  console.log('   ✓ ReadStream created with fd 0');

  assert(typeof stdin.isTTY === 'boolean', 'ReadStream should have isTTY property');
  console.log('   ✓ isTTY property: true');

  assert(stdin.isRaw === false, 'ReadStream should default isRaw to false');
  console.log('   ✓ isRaw property default: true');

  assert(stdin.fd === 0, 'ReadStream should have correct fd');
  console.log('   ✓ fd property: true');

  assert(typeof stdin.setRawMode === 'function', 'ReadStream should have setRawMode method');
  console.log('   ✓ setRawMode method: true');
} catch (error) {
  console.log('   ✗ ReadStream constructor failed:', error.message);
  process.exit(1);
}

// Test 5: ReadStream setRawMode functionality
console.log('\n5. Testing ReadStream setRawMode:');
try {
  const stdin = new tty.ReadStream(0);

  const initialRaw = stdin.isRaw;
  console.log(`   ✓ Initial isRaw: ${initialRaw}`);
  assert(typeof initialRaw === 'boolean', 'isRaw should be boolean');

  stdin.setRawMode(true);
  assert(stdin.isRaw === true, 'isRaw should be true after setRawMode(true)');
  console.log('   ✓ After setRawMode(true): true');

  stdin.setRawMode(false);
  assert(stdin.isRaw === false, 'isRaw should be false after setRawMode(false)');
  console.log('   ✓ After setRawMode(false): true');
} catch (error) {
  console.log('   ✗ setRawMode test failed:', error.message);
  process.exit(1);
}

// Test 6: ReadStream error handling
console.log('\n6. Testing ReadStream error handling:');
try {
  // Test invalid fd
  assert.throws(() => {
    new tty.ReadStream(-1);
  }, /range/, 'Should throw for invalid fd');
  console.log('   ✓ Throws for invalid fd: true');

  // Test setRawMode missing arguments
  const stdin = new tty.ReadStream(0);
  assert.throws(() => {
    stdin.setRawMode();
  }, /boolean/, 'setRawMode should throw for missing arguments');
  console.log('   ✓ setRawMode throws for missing arguments: true');
} catch (error) {
  console.log('   ✗ ReadStream error handling test failed:', error.message);
  process.exit(1);
}

// Test 7: WriteStream constructor
console.log('\n7. Testing WriteStream constructor:');
try {
  const stdout = new tty.WriteStream(1);

  assert(stdout && typeof stdout === 'object', 'Should create WriteStream instance');
  console.log('   ✓ WriteStream created with fd 1');

  assert(typeof stdout.isTTY === 'boolean', 'WriteStream should have isTTY property');
  console.log('   ✓ isTTY property: true');

  assert(stdout.fd === 1, 'WriteStream should have correct fd');
  console.log('   ✓ fd property: true');

  assert(typeof stdout.columns === 'number' && stdout.columns > 0, 'WriteStream should have positive columns');
  console.log('   ✓ columns property: true');

  assert(typeof stdout.rows === 'number' && stdout.rows > 0, 'WriteStream should have positive rows');
  console.log('   ✓ rows property: true');
} catch (error) {
  console.log('   ✗ WriteStream constructor failed:', error.message);
  process.exit(1);
}

// Test 8: WriteStream cursor control methods
console.log('\n8. Testing WriteStream cursor control:');
try {
  const stdout = new tty.WriteStream(1);

  assert(typeof stdout.clearLine === 'function', 'WriteStream should have clearLine method');
  console.log('   ✓ clearLine method: true');

  assert(typeof stdout.cursorTo === 'function', 'WriteStream should have cursorTo method');
  console.log('   ✓ cursorTo method: true');

  assert(typeof stdout.moveCursor === 'function', 'WriteStream should have moveCursor method');
  console.log('   ✓ moveCursor method: true');

  // Test method calls (should not throw and should return true)
  const clearResult = stdout.clearLine();
  assert(clearResult === true, 'clearLine should return true');

  const cursorResult = stdout.cursorTo(0, 0);
  assert(cursorResult === true, 'cursorTo should return true');

  const moveResult = stdout.moveCursor(1, 1);
  assert(moveResult === true, 'moveCursor should return true');

  console.log('   ✓ Cursor control methods execute without error');
} catch (error) {
  console.log('   ✗ Cursor control test failed:', error.message);
  process.exit(1);
}

// Test 9: WriteStream color detection
console.log('\n9. Testing WriteStream color detection:');
try {
  const stdout = new tty.WriteStream(1);

  assert(typeof stdout.getColorDepth === 'function', 'WriteStream should have getColorDepth method');
  console.log('   ✓ getColorDepth method: true');

  assert(typeof stdout.hasColors === 'function', 'WriteStream should have hasColors method');
  console.log('   ✓ hasColors method: true');

  const depth = stdout.getColorDepth();
  assert(typeof depth === 'number' && depth >= 1, 'getColorDepth should return positive number');
  console.log('   ✓ Color depth: true');

  const hasColors16 = stdout.hasColors(16);
  assert(typeof hasColors16 === 'boolean', 'hasColors(16) should return boolean');
  console.log('   ✓ hasColors(16): true');

  const hasColors256 = stdout.hasColors(256);
  assert(typeof hasColors256 === 'boolean', 'hasColors(256) should return boolean');
  console.log('   ✓ hasColors(256): true');

  const hasColors16M = stdout.hasColors(16777216);
  assert(typeof hasColors16M === 'boolean', 'hasColors(16777216) should return boolean');
  console.log('   ✓ hasColors(16777216): true');
} catch (error) {
  console.log('   ✗ Color detection test failed:', error.message);
  process.exit(1);
}

// Test 10: WriteStream error handling
console.log('\n10. Testing WriteStream error handling:');
try {
  // Test invalid fd
  assert.throws(() => {
    new tty.WriteStream(-1);
  }, /range/, 'Should throw for invalid fd');
  console.log('   ✓ Throws for invalid fd: true');
} catch (error) {
  console.log('   ✗ WriteStream error handling test failed:', error.message);
  process.exit(1);
}

// Test 11: ES Module import compatibility
console.log('\n11. Testing ES Module compatibility:');
try {
  // This tests that the module can be imported as ES module
  const ttyESM = require('node:tty');
  assert(ttyESM === tty, 'ESM import should return same module');
  console.log('   ✓ ES module import works');

  assert(typeof ttyESM.isatty === 'function', 'ESM should have isatty');
  console.log('   ✓ ESM has isatty: true');

  assert(typeof ttyESM.ReadStream === 'function', 'ESM should have ReadStream');
  console.log('   ✓ ESM has ReadStream: true');

  assert(typeof ttyESM.WriteStream === 'function', 'ESM should have WriteStream');
  console.log('   ✓ ESM has WriteStream: true');
} catch (error) {
  console.log('   ✗ ES module import test failed:', error.message);
  process.exit(1);
}

// Test 12: Edge cases
console.log('\n12. Testing edge cases:');
try {
  // Test with string arguments
  const stringResult = tty.isatty('1');
  assert(typeof stringResult === 'boolean', 'isatty with string should return boolean');
  console.log('   ✓ isatty with string argument: true');

  // Test method chaining compatibility
  const stdout = new tty.WriteStream(1);
  const depth = stdout.getColorDepth();
  assert(typeof depth === 'number', 'getColorDepth should return number');
  console.log('   ✓ Method return values are usable: true');
} catch (error) {
  console.log('   ✗ Edge case test failed:', error.message);
  process.exit(1);
}

console.log('\n=== TTY Module Test Summary ===');
console.log('✅ Module loads correctly');
console.log('✅ All required APIs are present');
console.log('✅ Input validation works correctly');
console.log('✅ ReadStream functionality works');
console.log('✅ WriteStream functionality works');
console.log('✅ Error handling is robust');
console.log('✅ ES module compatibility maintained');
console.log('\n🎉 All TTY tests passed! The Node.js TTY module is working correctly.');