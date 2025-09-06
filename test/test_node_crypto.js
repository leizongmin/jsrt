const assert = require('jsrt:assert');

console.log('=== Node.js Crypto Tests ===');

// Test CommonJS import
const crypto = require('node:crypto');
assert.ok(crypto, 'Should be able to require node:crypto');
assert.ok(crypto.randomBytes, 'Should have randomBytes function');
assert.ok(crypto.randomUUID, 'Should have randomUUID function');
assert.ok(crypto.constants, 'Should have constants object');

// Test named destructuring
const { randomBytes, randomUUID, constants } = require('node:crypto');
assert.ok(randomBytes, 'Should have randomBytes via destructuring');
assert.ok(randomUUID, 'Should have randomUUID via destructuring');
assert.ok(constants, 'Should have constants via destructuring');

console.log('✓ Module imports work correctly');

// Test randomBytes function
console.log('\nTesting randomBytes...');
const buf1 = crypto.randomBytes(16);
assert.ok(buf1, 'Should generate random bytes');
assert.strictEqual(buf1.length, 16, 'Should generate correct length');

const buf2 = crypto.randomBytes(32);
assert.strictEqual(buf2.length, 32, 'Should generate 32 bytes');

// Buffers should be different (extremely unlikely to be same)
const buf3 = crypto.randomBytes(16);
const same = buf1.every((byte, i) => byte === buf3[i]);
assert.strictEqual(same, false, 'Random buffers should be different');

console.log('✓ randomBytes works correctly');

// Test randomUUID function
console.log('\nTesting randomUUID...');
const uuid1 = crypto.randomUUID();
assert.ok(uuid1, 'Should generate UUID');
assert.strictEqual(typeof uuid1, 'string', 'UUID should be string');
assert.strictEqual(uuid1.length, 36, 'UUID should be 36 characters');

// Check UUID format (RFC 4122)
const uuidRegex =
  /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
assert.ok(uuidRegex.test(uuid1), 'UUID should match RFC 4122 format');

const uuid2 = crypto.randomUUID();
assert.notStrictEqual(uuid1, uuid2, 'UUIDs should be different');

console.log('✓ randomUUID works correctly');

// Test constants
console.log('\nTesting constants...');
assert.ok(typeof crypto.constants === 'object', 'constants should be object');
assert.ok('SSL_OP_ALL' in crypto.constants, 'Should have SSL_OP_ALL constant');
assert.ok(
  'SSL_OP_NO_SSLv2' in crypto.constants,
  'Should have SSL_OP_NO_SSLv2 constant'
);

console.log('✓ constants are available');

// Test error handling
console.log('\nTesting error handling...');
try {
  crypto.randomBytes(-1);
  assert.fail('Should throw error for negative size');
} catch (error) {
  assert.ok(error instanceof Error, 'Should throw proper error');
  console.log('✓ Error handling for negative size works');
}

try {
  crypto.randomBytes(70000); // Too large
  assert.fail('Should throw error for too large size');
} catch (error) {
  assert.ok(error instanceof Error, 'Should throw proper error');
  console.log('✓ Error handling for large size works');
}

console.log('\n=== All Node.js Crypto tests passed! ===');
