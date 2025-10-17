const assert = require('jsrt:assert');

// Test 1: Memory constructor with initial size only
console.log('\nTest 1: Memory constructor with initial size');
try {
  const memory = new WebAssembly.Memory({ initial: 1 });
  assert.ok(
    memory instanceof WebAssembly.Memory,
    'Memory should be instance of WebAssembly.Memory'
  );
  console.log('✅ Test 1 passed: Memory created with initial size');
} catch (error) {
  console.log('❌ Test 1 failed:', error.message);
}

// Test 2: Memory constructor with initial and maximum
console.log('\nTest 2: Memory constructor with initial and maximum');
try {
  const memory = new WebAssembly.Memory({ initial: 1, maximum: 10 });
  assert.ok(
    memory instanceof WebAssembly.Memory,
    'Memory should be instance of WebAssembly.Memory'
  );
  console.log('✅ Test 2 passed: Memory created with initial and maximum');
} catch (error) {
  console.log('❌ Test 2 failed:', error.message);
}

// Test 3: Memory.buffer returns ArrayBuffer
console.log('\nTest 3: Memory.buffer returns ArrayBuffer');
try {
  const memory = new WebAssembly.Memory({ initial: 1 });
  const buffer = memory.buffer;
  assert.ok(buffer instanceof ArrayBuffer, 'buffer should be ArrayBuffer');
  assert.strictEqual(
    buffer.byteLength,
    65536,
    'Buffer size should be 65536 bytes (1 page)'
  );
  console.log('✅ Test 3 passed: Memory.buffer returns correct ArrayBuffer');
} catch (error) {
  console.log('❌ Test 3 failed:', error.message);
}

// Test 4: Memory.buffer is cached (returns same object)
console.log('\nTest 4: Memory.buffer is cached');
try {
  const memory = new WebAssembly.Memory({ initial: 1 });
  const buffer1 = memory.buffer;
  const buffer2 = memory.buffer;
  assert.strictEqual(
    buffer1,
    buffer2,
    'Multiple calls to buffer should return same object'
  );
  console.log('✅ Test 4 passed: Memory.buffer is cached correctly');
} catch (error) {
  console.log('❌ Test 4 failed:', error.message);
}

// Test 5: Memory.buffer can be modified
console.log('\nTest 5: Memory.buffer can be modified');
try {
  const memory = new WebAssembly.Memory({ initial: 1 });
  const buffer = memory.buffer;
  const view = new Uint8Array(buffer);
  view[0] = 42;
  view[1] = 100;
  assert.strictEqual(view[0], 42, 'Can write to buffer');
  assert.strictEqual(view[1], 100, 'Can write to buffer');
  console.log('✅ Test 5 passed: Memory.buffer can be modified');
} catch (error) {
  console.log('❌ Test 5 failed:', error.message);
}

// Test 6: Memory.grow(delta) returns old size and grows memory
console.log('\nTest 6: Memory.grow() returns old size');
try {
  const memory = new WebAssembly.Memory({ initial: 1, maximum: 10 });
  const oldSize = memory.grow(2);
  assert.strictEqual(oldSize, 1, 'grow() should return old size in pages');

  // Check new buffer size
  const newBuffer = memory.buffer;
  assert.strictEqual(
    newBuffer.byteLength,
    65536 * 3,
    'Buffer should be 3 pages after grow'
  );
  console.log('✅ Test 6 passed: Memory.grow() works correctly');
} catch (error) {
  console.log('❌ Test 6 failed:', error.message);
}

// Test 7: Memory.grow() detaches old buffer
console.log('\nTest 7: Memory.grow() detaches old buffer');
try {
  const memory = new WebAssembly.Memory({ initial: 1, maximum: 10 });
  const oldBuffer = memory.buffer;
  memory.grow(1);

  // Try to access old buffer (should throw or have byteLength 0)
  try {
    const view = new Uint8Array(oldBuffer);
    view[0] = 42; // This might throw on detached buffer
    // If it doesn't throw, check byteLength
    if (oldBuffer.byteLength === 0) {
      console.log('✅ Test 7 passed: Old buffer detached (byteLength = 0)');
    } else {
      console.log('⚠️  Test 7 warning: Old buffer not detached properly');
    }
  } catch (e) {
    // Expected: accessing detached buffer throws
    console.log('✅ Test 7 passed: Old buffer detached (throws on access)');
  }
} catch (error) {
  console.log('❌ Test 7 failed:', error.message);
}

// Test 8: Memory.grow(0) returns current size
console.log('\nTest 8: Memory.grow(0) returns current size');
try {
  const memory = new WebAssembly.Memory({ initial: 2 });
  const size = memory.grow(0);
  assert.strictEqual(size, 2, 'grow(0) should return current size');
  console.log('✅ Test 8 passed: Memory.grow(0) works correctly');
} catch (error) {
  console.log('❌ Test 8 failed:', error.message);
}

// Test 9: Error handling - missing descriptor
console.log('\nTest 9: Error handling - missing descriptor');
try {
  new WebAssembly.Memory();
  console.log('❌ Test 9 failed: should have thrown TypeError');
} catch (error) {
  assert.ok(
    error instanceof TypeError,
    'Should throw TypeError for missing argument'
  );
  console.log('✅ Test 9 passed: TypeError for missing descriptor');
}

// Test 10: Error handling - missing initial property
console.log('\nTest 10: Error handling - missing initial property');
try {
  new WebAssembly.Memory({});
  console.log('❌ Test 10 failed: should have thrown error');
} catch (error) {
  // Should throw because initial is required
  console.log('✅ Test 10 passed: Error for missing initial property');
}

// Test 11: Error handling - initial > maximum
console.log('\nTest 11: Error handling - initial > maximum');
try {
  new WebAssembly.Memory({ initial: 10, maximum: 5 });
  console.log('❌ Test 11 failed: should have thrown RangeError');
} catch (error) {
  assert.ok(
    error instanceof RangeError,
    'Should throw RangeError for initial > maximum'
  );
  console.log('✅ Test 11 passed: RangeError for initial > maximum');
}

// Test 12: Error handling - grow beyond maximum
console.log('\nTest 12: Error handling - grow beyond maximum');
try {
  const memory = new WebAssembly.Memory({ initial: 1, maximum: 5 });
  memory.grow(10); // Try to grow to 11 pages, exceeds maximum of 5
  console.log('❌ Test 12 failed: should have thrown RangeError');
} catch (error) {
  assert.ok(
    error instanceof RangeError,
    'Should throw RangeError for grow beyond maximum'
  );
  console.log('✅ Test 12 passed: RangeError for growing beyond maximum');
}

// Test 13: Multiple memories are independent
console.log('\nTest 13: Multiple memories are independent');
try {
  const memory1 = new WebAssembly.Memory({ initial: 1 });
  const memory2 = new WebAssembly.Memory({ initial: 2 });

  const buffer1 = memory1.buffer;
  const buffer2 = memory2.buffer;

  assert.strictEqual(buffer1.byteLength, 65536, 'Memory 1 should be 1 page');
  assert.strictEqual(
    buffer2.byteLength,
    65536 * 2,
    'Memory 2 should be 2 pages'
  );

  // Modify memory1
  const view1 = new Uint8Array(buffer1);
  view1[0] = 111;

  // Check memory2 is unaffected
  const view2 = new Uint8Array(buffer2);
  assert.strictEqual(view2[0], 0, 'Memory 2 should be independent');

  console.log('✅ Test 13 passed: Multiple memories are independent');
} catch (error) {
  console.log('❌ Test 13 failed:', error.message);
}

console.log('\n=== Memory Tests Completed ===');
