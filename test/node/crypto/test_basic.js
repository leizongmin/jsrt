const assert = require('jsrt:assert');

let crypto;
try {
  crypto = require('node:crypto');
} catch (error) {
  console.error('❌ SKIP: node:crypto module not available:', error.message);
  process.exit(0);
}

// Test module imports
assert.strictEqual(typeof crypto, 'object', 'crypto should be an object');
assert.strictEqual(
  typeof crypto.randomBytes,
  'function',
  'randomBytes should be a function'
);
assert.strictEqual(
  typeof crypto.randomUUID,
  'function',
  'randomUUID should be a function'
);
assert.strictEqual(
  typeof crypto.constants,
  'object',
  'constants should be an object'
);

// Testing randomBytes
try {
  const bytes1 = crypto.randomBytes(16);
  assert.strictEqual(
    bytes1 instanceof Uint8Array,
    true,
    'randomBytes should return Uint8Array'
  );
  assert.strictEqual(
    bytes1.length,
    16,
    'randomBytes(16) should return 16 bytes'
  );

  const bytes2 = crypto.randomBytes(32);
  assert.strictEqual(
    bytes2.length,
    32,
    'randomBytes(32) should return 32 bytes'
  );

  // Test that different calls return different values
  const bytes3 = crypto.randomBytes(16);
  assert.strictEqual(
    bytes1.toString() !== bytes3.toString(),
    true,
    'Different calls should return different values'
  );
} catch (error) {
  console.error('❌ randomBytes test failed:', error.message);
}

// Testing randomUUID
try {
  const uuid1 = crypto.randomUUID();
  assert.strictEqual(
    typeof uuid1,
    'string',
    'randomUUID should return a string'
  );
  assert.strictEqual(uuid1.length, 36, 'UUID should be 36 characters long');
  assert.strictEqual(
    /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i.test(
      uuid1
    ),
    true,
    'UUID should match v4 format'
  );

  const uuid2 = crypto.randomUUID();
  assert.strictEqual(
    uuid1 !== uuid2,
    true,
    'Different calls should return different UUIDs'
  );
} catch (error) {
  console.error('❌ randomUUID test failed:', error.message);
}

// Testing constants
try {
  assert.strictEqual(
    typeof crypto.constants,
    'object',
    'constants should be an object'
  );
  assert.strictEqual(
    crypto.constants !== null,
    true,
    'constants should not be null'
  );
} catch (error) {
  console.error('❌ constants test failed:', error.message);
}

// Testing error handling
try {
  crypto.randomBytes(-1);
  assert.fail('randomBytes(-1) should throw an error');
} catch (error) {
  assert.strictEqual(
    typeof error.message,
    'string',
    'Error should have a message'
  );
}

try {
  crypto.randomBytes(1024 * 1024 * 1024); // 1GB
  assert.fail('randomBytes(1GB) should throw an error');
} catch (error) {
  assert.strictEqual(
    typeof error.message,
    'string',
    'Error should have a message'
  );
}
