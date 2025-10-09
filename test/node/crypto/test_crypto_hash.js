// Test node:crypto Hash and HMAC implementation

const crypto = require('node:crypto');

console.log('Testing node:crypto Hash and HMAC implementation...\n');

let testsPassed = 0;
let testsFailed = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`✓ ${name}`);
    testsPassed++;
  } catch (e) {
    console.log(`✗ ${name}`);
    console.log(`  Error: ${e.message}`);
    testsFailed++;
  }
}

function assert(condition, message) {
  if (!condition) {
    throw new Error(message || 'Assertion failed');
  }
}

// Test 1: createHash basic functionality
test('createHash returns a Hash object', () => {
  const hash = crypto.createHash('sha256');
  assert(hash !== null, 'Hash object should not be null');
  assert(typeof hash.update === 'function', 'Hash should have update method');
  assert(typeof hash.digest === 'function', 'Hash should have digest method');
});

// Test 2: SHA-256 hash with hex encoding
test('SHA-256 hash with hex encoding', () => {
  const hash = crypto.createHash('sha256');
  hash.update(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f])); // "hello"
  const result = hash.digest('hex');
  // Expected SHA-256 of "hello"
  const expected =
    '2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824';
  assert(result === expected, `Expected ${expected}, got ${result}`);
});

// Test 3: SHA-256 hash with buffer output
test('SHA-256 hash with buffer output', () => {
  const hash = crypto.createHash('sha256');
  hash.update(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f])); // "hello"
  const result = hash.digest();
  assert(result instanceof Uint8Array, 'Result should be Uint8Array');
  assert(result.length === 32, 'SHA-256 should produce 32 bytes');
});

// Test 4: Multiple updates
test('Hash with multiple updates', () => {
  const hash = crypto.createHash('sha256');
  hash.update(new Uint8Array([0x68, 0x65])); // "he"
  hash.update(new Uint8Array([0x6c, 0x6c])); // "ll"
  hash.update(new Uint8Array([0x6f])); // "o"
  const result = hash.digest('hex');
  const expected =
    '2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824';
  assert(result === expected, `Multiple updates should produce same result`);
});

// Test 5: SHA-512 hash
test('SHA-512 hash', () => {
  const hash = crypto.createHash('sha512');
  hash.update(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f])); // "hello"
  const result = hash.digest();
  assert(result.length === 64, 'SHA-512 should produce 64 bytes');
});

// Test 6: SHA-384 hash
test('SHA-384 hash', () => {
  const hash = crypto.createHash('sha384');
  hash.update(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f])); // "hello"
  const result = hash.digest();
  assert(result.length === 48, 'SHA-384 should produce 48 bytes');
});

// Test 7: HMAC basic functionality
test('createHmac returns an Hmac object', () => {
  const key = new Uint8Array([0x6b, 0x65, 0x79]); // "key"
  const hmac = crypto.createHmac('sha256', key);
  assert(hmac !== null, 'HMAC object should not be null');
  assert(typeof hmac.update === 'function', 'HMAC should have update method');
  assert(typeof hmac.digest === 'function', 'HMAC should have digest method');
});

// Test 8: HMAC-SHA256 with hex encoding
test('HMAC-SHA256 with hex encoding', () => {
  const key = new Uint8Array([0x6b, 0x65, 0x79]); // "key"
  const hmac = crypto.createHmac('sha256', key);
  hmac.update(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f])); // "hello"
  const result = hmac.digest('hex');
  // HMAC-SHA256("key", "hello")
  const expected =
    '9307b3b915efb5171ff14d8cb55fbcc798c6c0ef1456d66ded1a6aa723a58b7b';
  assert(result === expected, `Expected ${expected}, got ${result}`);
});

// Test 9: HMAC with buffer output
test('HMAC-SHA256 with buffer output', () => {
  const key = new Uint8Array([0x6b, 0x65, 0x79]); // "key"
  const hmac = crypto.createHmac('sha256', key);
  hmac.update(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f])); // "hello"
  const result = hmac.digest();
  assert(result instanceof Uint8Array, 'Result should be Uint8Array');
  assert(result.length === 32, 'HMAC-SHA256 should produce 32 bytes');
});

// Test 10: HMAC with multiple updates
test('HMAC with multiple updates', () => {
  const key = new Uint8Array([0x6b, 0x65, 0x79]); // "key"
  const hmac = crypto.createHmac('sha256', key);
  hmac.update(new Uint8Array([0x68, 0x65])); // "he"
  hmac.update(new Uint8Array([0x6c, 0x6c])); // "ll"
  hmac.update(new Uint8Array([0x6f])); // "o"
  const result = hmac.digest('hex');
  const expected =
    '9307b3b915efb5171ff14d8cb55fbcc798c6c0ef1456d66ded1a6aa723a58b7b';
  assert(result === expected, `Multiple updates should produce same result`);
});

// Test 11: Error - digest called twice
test('Error when digest called twice', () => {
  const hash = crypto.createHash('sha256');
  hash.update(new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]));
  hash.digest('hex');

  let errorThrown = false;
  try {
    hash.digest('hex');
  } catch (e) {
    errorThrown = true;
  }
  assert(errorThrown, 'Should throw error when digest called twice');
});

// Test 12: randomBytes exists
test('randomBytes function exists', () => {
  assert(
    typeof crypto.randomBytes === 'function',
    'randomBytes should be a function'
  );
  const bytes = crypto.randomBytes(16);
  assert(bytes instanceof Uint8Array, 'randomBytes should return Uint8Array');
  assert(bytes.length === 16, 'randomBytes should return requested length');
});

// Test 13: randomUUID exists
test('randomUUID function exists', () => {
  assert(
    typeof crypto.randomUUID === 'function',
    'randomUUID should be a function'
  );
  const uuid = crypto.randomUUID();
  assert(typeof uuid === 'string', 'randomUUID should return string');
  assert(uuid.length === 36, 'UUID should be 36 characters');
  assert(
    /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/.test(
      uuid
    ),
    'Should be valid UUID v4'
  );
});

// Test 14: constants exists
test('constants object exists', () => {
  assert(typeof crypto.constants === 'object', 'constants should be an object');
  assert(crypto.constants !== null, 'constants should not be null');
});

console.log(`\n${testsPassed} tests passed, ${testsFailed} tests failed`);

if (testsFailed > 0) {
  throw new Error('Some tests failed');
}
