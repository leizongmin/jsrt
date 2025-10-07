const assert = require('jsrt:assert');

try {
  // Test CommonJS require
  const { Buffer } = require('node:buffer');

  // Test Buffer.alloc
  const buf1 = Buffer.alloc(10);
  assert.strictEqual(
    buf1.length,
    10,
    'Buffer.alloc should create buffer with correct length'
  );

  // Test Buffer.alloc with fill
  const buf2 = Buffer.alloc(5, 42);
  assert.strictEqual(
    buf2.length,
    5,
    'Buffer.alloc with fill should have correct length'
  );
  assert.strictEqual(
    buf2[0],
    42,
    'Buffer.alloc should fill with provided value'
  );
  assert.strictEqual(buf2[4], 42, 'All bytes should be filled');

  // Test Buffer.alloc with string fill
  const buf3 = Buffer.alloc(6, 'abc');
  assert.strictEqual(
    buf3.length,
    6,
    'Buffer.alloc with string fill should have correct length'
  );
  assert.strictEqual(buf3[0], 97, 'First byte should be "a" (97)'); // 'a'
  assert.strictEqual(buf3[1], 98, 'Second byte should be "b" (98)'); // 'b'
  assert.strictEqual(buf3[2], 99, 'Third byte should be "c" (99)'); // 'c'
  assert.strictEqual(buf3[3], 97, 'Pattern should repeat'); // 'a' again

  // Test Buffer.allocUnsafe
  const buf4 = Buffer.allocUnsafe(8);
  assert.strictEqual(
    buf4.length,
    8,
    'Buffer.allocUnsafe should create buffer with correct length'
  );

  // Test Buffer.from with string
  const buf5 = Buffer.from('hello');
  assert.strictEqual(
    buf5.length,
    5,
    'Buffer.from string should have correct length'
  );
  assert.strictEqual(buf5[0], 104, 'First byte should be "h" (104)'); // 'h'
  assert.strictEqual(buf5[1], 101, 'Second byte should be "e" (101)'); // 'e'

  // Test Buffer.from with array
  const buf6 = Buffer.from([1, 2, 3, 4, 5]);
  assert.strictEqual(
    buf6.length,
    5,
    'Buffer.from array should have correct length'
  );
  assert.strictEqual(buf6[0], 1, 'First byte should be 1');
  assert.strictEqual(buf6[4], 5, 'Last byte should be 5');

  // Test Buffer.isBuffer
  assert.strictEqual(
    Buffer.isBuffer(buf1),
    true,
    'Buffer.isBuffer should return true for buffers'
  );
  assert.strictEqual(
    Buffer.isBuffer([1, 2, 3]),
    false,
    'Buffer.isBuffer should return false for arrays'
  );
  assert.strictEqual(
    Buffer.isBuffer('string'),
    false,
    'Buffer.isBuffer should return false for strings'
  );

  // Test Buffer.concat
  const buf7 = Buffer.from('hello');
  const buf8 = Buffer.from(' world');
  const buf9 = Buffer.concat([buf7, buf8]);
  assert.strictEqual(
    buf9.length,
    11,
    'Buffer.concat should have correct total length'
  );
  // Convert to string for easier verification
  let result_str = '';
  for (let i = 0; i < buf9.length; i++) {
    result_str += String.fromCharCode(buf9[i]);
  }
  assert.strictEqual(
    result_str,
    'hello world',
    'Buffer.concat should join buffers correctly'
  );

  // Test Buffer.concat with specified length
  const buf10 = Buffer.concat([buf7, buf8], 7); // Only first 7 bytes
  assert.strictEqual(
    buf10.length,
    7,
    'Buffer.concat with length should respect length parameter'
  );
  let truncated_str = '';
  for (let i = 0; i < buf10.length; i++) {
    truncated_str += String.fromCharCode(buf10[i]);
  }
  assert.strictEqual(
    truncated_str,
    'hello w',
    'Buffer.concat should truncate correctly'
  );
} catch (error) {
  console.error('❌ Buffer test failed:', error.message);
  console.error(error.stack);
  throw error;
}
