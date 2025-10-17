const assert = require('jsrt:assert');

// Test WebAssembly.Table constructor and methods (Phase 4.1)

console.log('\n=== Test 1: Table constructor with funcref element type ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 10,
  });
  assert.ok(
    table instanceof WebAssembly.Table,
    'Should be instance of WebAssembly.Table'
  );
  assert.strictEqual(table.length, 10, 'Initial length should be 10');
  console.log('✅ Test 1 passed: funcref table created successfully');
} catch (error) {
  console.log('❌ Test 1 failed:', error.message);
  console.log(error.stack);
}

console.log('\n=== Test 2: Table constructor with externref element type ===');
try {
  const table = new WebAssembly.Table({
    element: 'externref',
    initial: 5,
    maximum: 20,
  });
  assert.ok(
    table instanceof WebAssembly.Table,
    'Should be instance of WebAssembly.Table'
  );
  assert.strictEqual(table.length, 5, 'Initial length should be 5');
  console.log(
    '✅ Test 2 passed: externref table with maximum created successfully'
  );
} catch (error) {
  console.log('❌ Test 2 failed:', error.message);
  console.log(error.stack);
}

console.log(
  '\n=== Test 3: Table constructor with value parameter (default null) ==='
);
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 3,
    value: null,
  });
  assert.strictEqual(table.length, 3, 'Initial length should be 3');
  console.log('✅ Test 3 passed: table with value parameter created');
} catch (error) {
  console.log('❌ Test 3 failed:', error.message);
  console.log(error.stack);
}

console.log('\n=== Test 4: Table constructor error - missing descriptor ===');
try {
  new WebAssembly.Table();
  console.log(
    '❌ Test 4 failed: should throw TypeError for missing descriptor'
  );
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  assert.ok(
    error.message.includes('requires 1 argument'),
    'Error message should mention argument requirement'
  );
  console.log('✅ Test 4 passed: TypeError thrown for missing descriptor');
}

console.log(
  '\n=== Test 5: Table constructor error - non-object descriptor ==='
);
try {
  new WebAssembly.Table('not an object');
  console.log(
    '❌ Test 5 failed: should throw TypeError for non-object descriptor'
  );
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  assert.ok(
    error.message.includes('must be an object'),
    'Error message should mention object requirement'
  );
  console.log('✅ Test 5 passed: TypeError thrown for non-object descriptor');
}

console.log(
  '\n=== Test 6: Table constructor error - missing element property ==='
);
try {
  new WebAssembly.Table({
    initial: 10,
  });
  console.log('❌ Test 6 failed: should throw TypeError for missing element');
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  console.log('✅ Test 6 passed: TypeError thrown for missing element');
}

console.log('\n=== Test 7: Table constructor error - invalid element type ===');
try {
  new WebAssembly.Table({
    element: 'invalid',
    initial: 10,
  });
  console.log(
    '❌ Test 7 failed: should throw TypeError for invalid element type'
  );
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  assert.ok(
    error.message.includes('funcref') || error.message.includes('externref'),
    'Error should mention valid types'
  );
  console.log('✅ Test 7 passed: TypeError thrown for invalid element type');
}

console.log(
  '\n=== Test 8: Table constructor error - missing initial property ==='
);
try {
  new WebAssembly.Table({
    element: 'funcref',
  });
  console.log('❌ Test 8 failed: should throw TypeError for missing initial');
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  assert.ok(
    error.message.includes('initial'),
    'Error message should mention initial property'
  );
  console.log('✅ Test 8 passed: TypeError thrown for missing initial');
}

console.log('\n=== Test 9: Table constructor error - negative initial ===');
try {
  new WebAssembly.Table({
    element: 'funcref',
    initial: -1,
  });
  console.log('❌ Test 9 failed: should throw RangeError for negative initial');
} catch (error) {
  assert.ok(error instanceof RangeError, 'Should throw RangeError');
  console.log('✅ Test 9 passed: RangeError thrown for negative initial');
}

console.log('\n=== Test 10: Table constructor error - maximum < initial ===');
try {
  new WebAssembly.Table({
    element: 'funcref',
    initial: 10,
    maximum: 5,
  });
  console.log(
    '❌ Test 10 failed: should throw RangeError when maximum < initial'
  );
} catch (error) {
  assert.ok(error instanceof RangeError, 'Should throw RangeError');
  console.log('✅ Test 10 passed: RangeError thrown when maximum < initial');
}

console.log('\n=== Test 11: Table.prototype.length getter ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 15,
  });
  assert.strictEqual(table.length, 15, 'Length should be 15');

  // Length should be read-only
  table.length = 20;
  assert.strictEqual(table.length, 15, 'Length should remain 15 (read-only)');

  console.log('✅ Test 11 passed: length getter works correctly');
} catch (error) {
  console.log('❌ Test 11 failed:', error.message);
  console.log(error.stack);
}

console.log('\n=== Test 12: Table.prototype.grow() basic usage ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 5,
    maximum: 20,
  });
  assert.strictEqual(table.length, 5, 'Initial length should be 5');

  const oldLength = table.grow(3);
  assert.strictEqual(oldLength, 5, 'grow() should return old length (5)');
  assert.strictEqual(table.length, 8, 'New length should be 8');

  console.log('✅ Test 12 passed: grow() increases table size correctly');
} catch (error) {
  console.log('❌ Test 12 failed:', error.message);
  console.log(error.stack);
}

console.log('\n=== Test 13: Table.prototype.grow() with null value ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 3,
    maximum: 10,
  });

  const oldLength = table.grow(2, null);
  assert.strictEqual(oldLength, 3, 'grow() should return old length (3)');
  assert.strictEqual(table.length, 5, 'New length should be 5');

  console.log('✅ Test 13 passed: grow() with null value works');
} catch (error) {
  console.log('❌ Test 13 failed:', error.message);
  console.log(error.stack);
}

console.log('\n=== Test 14: Table.prototype.grow() error - missing delta ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 5,
    maximum: 20,
  });
  table.grow();
  console.log('❌ Test 14 failed: should throw TypeError for missing delta');
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  console.log('✅ Test 14 passed: TypeError thrown for missing delta');
}

console.log(
  '\n=== Test 15: Table.prototype.grow() error - exceeds maximum ==='
);
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 5,
    maximum: 10,
  });
  table.grow(10); // Would result in size 15, exceeding max 10
  console.log(
    '❌ Test 15 failed: should throw RangeError when exceeding maximum'
  );
} catch (error) {
  assert.ok(error instanceof RangeError, 'Should throw RangeError');
  console.log('✅ Test 15 passed: RangeError thrown when exceeding maximum');
}

console.log(
  '\n=== Test 16: Table.prototype.get() - Phase 4.1 returns null ==='
);
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 10,
  });

  const value = table.get(0);
  assert.strictEqual(value, null, 'get() should return null in Phase 4.1');

  const value2 = table.get(5);
  assert.strictEqual(
    value2,
    null,
    'get() should return null for any valid index'
  );

  console.log('✅ Test 16 passed: get() returns null (Phase 4.1 placeholder)');
} catch (error) {
  console.log('❌ Test 16 failed:', error.message);
  console.log(error.stack);
}

console.log('\n=== Test 17: Table.prototype.get() error - missing index ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 10,
  });
  table.get();
  console.log('❌ Test 17 failed: should throw TypeError for missing index');
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  console.log('✅ Test 17 passed: TypeError thrown for missing index');
}

console.log(
  '\n=== Test 18: Table.prototype.get() error - index out of bounds ==='
);
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 5,
  });
  table.get(10); // Index 10 is out of bounds for size 5
  console.log(
    '❌ Test 18 failed: should throw RangeError for out of bounds index'
  );
} catch (error) {
  assert.ok(error instanceof RangeError, 'Should throw RangeError');
  console.log('✅ Test 18 passed: RangeError thrown for out of bounds index');
}

console.log('\n=== Test 19: Table.prototype.set() with null value ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 10,
  });

  // set() should accept null in Phase 4.1
  table.set(0, null);
  table.set(5, null);

  assert.strictEqual(table.get(0), null, 'Value at index 0 should be null');
  assert.strictEqual(table.get(5), null, 'Value at index 5 should be null');

  console.log('✅ Test 19 passed: set() with null value works');
} catch (error) {
  console.log('❌ Test 19 failed:', error.message);
  console.log(error.stack);
}

console.log(
  '\n=== Test 20: Table.prototype.set() error - missing arguments ==='
);
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 10,
  });
  table.set();
  console.log(
    '❌ Test 20 failed: should throw TypeError for missing arguments'
  );
} catch (error) {
  assert.ok(error instanceof TypeError, 'Should throw TypeError');
  console.log('✅ Test 20 passed: TypeError thrown for missing arguments');
}

console.log(
  '\n=== Test 21: Table.prototype.set() error - index out of bounds ==='
);
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 5,
  });
  table.set(10, null); // Index 10 is out of bounds for size 5
  console.log(
    '❌ Test 21 failed: should throw RangeError for out of bounds index'
  );
} catch (error) {
  assert.ok(error instanceof RangeError, 'Should throw RangeError');
  console.log('✅ Test 21 passed: RangeError thrown for out of bounds index');
}

console.log('\n=== Test 22: Table with zero initial size ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 0,
    maximum: 10,
  });
  assert.strictEqual(table.length, 0, 'Initial length should be 0');

  table.grow(5);
  assert.strictEqual(table.length, 5, 'Length should be 5 after grow');

  console.log('✅ Test 22 passed: Table with zero initial size works');
} catch (error) {
  console.log('❌ Test 22 failed:', error.message);
  console.log(error.stack);
}

console.log('\n=== Test 23: Table without maximum limit ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 5,
  });
  assert.strictEqual(table.length, 5, 'Initial length should be 5');

  // Should be able to grow without limit (up to WASM limits)
  const oldLength = table.grow(10);
  assert.strictEqual(oldLength, 5, 'grow() should return old length');
  assert.strictEqual(table.length, 15, 'New length should be 15');

  console.log('✅ Test 23 passed: Table without maximum can grow');
} catch (error) {
  console.log('❌ Test 23 failed:', error.message);
  console.log(error.stack);
}

console.log('\n=== Test 24: Table.prototype toString tag ===');
try {
  const table = new WebAssembly.Table({
    element: 'funcref',
    initial: 5,
  });

  const tag = Object.prototype.toString.call(table);
  assert.strictEqual(
    tag,
    '[object WebAssembly.Table]',
    'toString tag should be WebAssembly.Table'
  );

  console.log('✅ Test 24 passed: toString tag is correct');
} catch (error) {
  console.log('❌ Test 24 failed:', error.message);
  console.log(error.stack);
}

console.log('\n=== All Tests Completed ===');
