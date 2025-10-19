// Test node:crypto Cipher and Decipher implementation

const crypto = require('node:crypto');

// Check if OpenSSL symmetric functions are available (required on Windows)
try {
  const testKey = new Uint8Array(32);
  const testIv = new Uint8Array(16);
  crypto.createCipheriv('aes-256-cbc', testKey, testIv);
} catch (e) {
  if (e.message && e.message.includes('OpenSSL symmetric functions not available')) {
    console.log('Error: OpenSSL symmetric functions not available');
    process.exit(0);
  }
  // Re-throw other errors
  throw e;
}

console.log('Testing node:crypto Cipher and Decipher implementation...\n');

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

function hexToUint8Array(hex) {
  const bytes = new Uint8Array(hex.length / 2);
  for (let i = 0; i < hex.length; i += 2) {
    bytes[i / 2] = parseInt(hex.substr(i, 2), 16);
  }
  return bytes;
}

function uint8ArrayToHex(arr) {
  return Array.from(arr)
    .map((b) => b.toString(16).padStart(2, '0'))
    .join('');
}

// Test 1: createCipheriv basic functionality
test('createCipheriv returns a Cipher object', () => {
  const key = new Uint8Array(32); // 256-bit key
  const iv = new Uint8Array(16); // 128-bit IV
  const cipher = crypto.createCipheriv('aes-256-cbc', key, iv);
  assert(cipher !== null, 'Cipher object should not be null');
  assert(
    typeof cipher.update === 'function',
    'Cipher should have update method'
  );
  assert(typeof cipher.final === 'function', 'Cipher should have final method');
});

// Test 2: AES-256-CBC encryption/decryption
test('AES-256-CBC encryption and decryption', () => {
  const key = hexToUint8Array(
    '603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4'
  );
  const iv = hexToUint8Array('000102030405060708090a0b0c0d0e0f');
  const plaintext = new Uint8Array([
    0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64, 0x21,
  ]); // "Hello World!"

  // Encrypt
  const cipher = crypto.createCipheriv('aes-256-cbc', key, iv);
  const encrypted1 = cipher.update(plaintext);
  const encrypted2 = cipher.final();

  // Combine encrypted chunks
  const ciphertext = new Uint8Array(encrypted1.length + encrypted2.length);
  ciphertext.set(encrypted1);
  ciphertext.set(encrypted2, encrypted1.length);

  // Decrypt
  const decipher = crypto.createDecipheriv('aes-256-cbc', key, iv);
  const decrypted1 = decipher.update(ciphertext);
  const decrypted2 = decipher.final();

  // Combine decrypted chunks
  const decrypted = new Uint8Array(decrypted1.length + decrypted2.length);
  decrypted.set(decrypted1);
  decrypted.set(decrypted2, decrypted1.length);

  assert(
    decrypted.length === plaintext.length,
    'Decrypted length should match'
  );
  for (let i = 0; i < plaintext.length; i++) {
    assert(
      decrypted[i] === plaintext[i],
      `Byte ${i} should match: ${decrypted[i]} vs ${plaintext[i]}`
    );
  }
});

// Test 3: AES-128-CBC encryption/decryption
test('AES-128-CBC encryption and decryption', () => {
  const key = hexToUint8Array('2b7e151628aed2a6abf7158809cf4f3c');
  const iv = hexToUint8Array('000102030405060708090a0b0c0d0e0f');
  const plaintext = new Uint8Array([
    0x54, 0x65, 0x73, 0x74, 0x20, 0x44, 0x61, 0x74, 0x61,
  ]); // "Test Data"

  const cipher = crypto.createCipheriv('aes-128-cbc', key, iv);
  const encrypted = new Uint8Array([
    ...cipher.update(plaintext),
    ...cipher.final(),
  ]);

  const decipher = crypto.createDecipheriv('aes-128-cbc', key, iv);
  const decrypted = new Uint8Array([
    ...decipher.update(encrypted),
    ...decipher.final(),
  ]);

  assert(decrypted.length === plaintext.length, 'Lengths should match');
  for (let i = 0; i < plaintext.length; i++) {
    assert(decrypted[i] === plaintext[i], `Byte ${i} should match`);
  }
});

// Test 4: AES-256-CTR encryption/decryption
test('AES-256-CTR encryption and decryption', () => {
  const key = hexToUint8Array(
    '603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4'
  );
  const iv = hexToUint8Array('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff');
  const plaintext = new Uint8Array([
    0x43, 0x54, 0x52, 0x20, 0x6d, 0x6f, 0x64, 0x65,
  ]); // "CTR mode"

  const cipher = crypto.createCipheriv('aes-256-ctr', key, iv);
  const encrypted = new Uint8Array([
    ...cipher.update(plaintext),
    ...cipher.final(),
  ]);

  const decipher = crypto.createDecipheriv('aes-256-ctr', key, iv);
  const decrypted = new Uint8Array([
    ...decipher.update(encrypted),
    ...decipher.final(),
  ]);

  assert(decrypted.length === plaintext.length, 'Lengths should match');
  for (let i = 0; i < plaintext.length; i++) {
    assert(decrypted[i] === plaintext[i], `Byte ${i} should match`);
  }
});

// Test 5: AES-256-GCM encryption/decryption with AAD
test('AES-256-GCM encryption and decryption with AAD', () => {
  const key = hexToUint8Array(
    '603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4'
  );
  const iv = hexToUint8Array('cafebabefacedbaddecaf888'); // 12 bytes for GCM
  const plaintext = new Uint8Array([
    0x47, 0x43, 0x4d, 0x20, 0x74, 0x65, 0x73, 0x74,
  ]); // "GCM test"
  const aad = new Uint8Array([0x01, 0x02, 0x03, 0x04]); // Additional Authenticated Data

  // Encrypt
  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  cipher.setAAD(aad);
  const encrypted1 = cipher.update(plaintext);
  const encrypted2 = cipher.final();
  const authTag = cipher.getAuthTag();

  const ciphertext = new Uint8Array(encrypted1.length + encrypted2.length);
  ciphertext.set(encrypted1);
  ciphertext.set(encrypted2, encrypted1.length);

  // Decrypt
  const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);
  decipher.setAAD(aad);
  decipher.setAuthTag(authTag);
  const decrypted1 = decipher.update(ciphertext);
  const decrypted2 = decipher.final();

  const decrypted = new Uint8Array(decrypted1.length + decrypted2.length);
  decrypted.set(decrypted1);
  decrypted.set(decrypted2, decrypted1.length);

  assert(decrypted.length === plaintext.length, 'Lengths should match');
  for (let i = 0; i < plaintext.length; i++) {
    assert(decrypted[i] === plaintext[i], `Byte ${i} should match`);
  }
});

// Test 6: GCM authentication failure
test('GCM decryption fails with wrong auth tag', () => {
  const key = hexToUint8Array(
    '603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4'
  );
  const iv = hexToUint8Array('cafebabefacedbaddecaf888');
  const plaintext = new Uint8Array([0x74, 0x65, 0x73, 0x74]); // "test"

  const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
  const encrypted = new Uint8Array([
    ...cipher.update(plaintext),
    ...cipher.final(),
  ]);
  cipher.getAuthTag(); // Get correct tag but don't use it

  // Use wrong tag
  const wrongTag = new Uint8Array(16);

  const decipher = crypto.createDecipheriv('aes-256-gcm', key, iv);
  decipher.setAuthTag(wrongTag);
  decipher.update(encrypted);

  let errorThrown = false;
  try {
    decipher.final();
  } catch (e) {
    errorThrown = true;
  }
  assert(errorThrown, 'Should throw error with wrong auth tag');
});

// Test 7: Multiple update calls
test('Cipher with multiple update calls', () => {
  const key = hexToUint8Array('2b7e151628aed2a6abf7158809cf4f3c');
  const iv = hexToUint8Array('000102030405060708090a0b0c0d0e0f');

  const cipher = crypto.createCipheriv('aes-128-cbc', key, iv);
  const chunk1 = cipher.update(new Uint8Array([0x48, 0x65, 0x6c, 0x6c, 0x6f])); // "Hello"
  const chunk2 = cipher.update(
    new Uint8Array([0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64])
  ); // " World"
  const final = cipher.final();

  const ciphertext = new Uint8Array(
    chunk1.length + chunk2.length + final.length
  );
  ciphertext.set(chunk1);
  ciphertext.set(chunk2, chunk1.length);
  ciphertext.set(final, chunk1.length + chunk2.length);

  const decipher = crypto.createDecipheriv('aes-128-cbc', key, iv);
  const decrypted = new Uint8Array([
    ...decipher.update(ciphertext),
    ...decipher.final(),
  ]);

  const expected = new Uint8Array([
    0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64,
  ]);
  assert(decrypted.length === expected.length, 'Lengths should match');
  for (let i = 0; i < expected.length; i++) {
    assert(decrypted[i] === expected[i], `Byte ${i} should match`);
  }
});

// Test 8: Hex output encoding
test('Cipher with hex output encoding', () => {
  const key = hexToUint8Array('2b7e151628aed2a6abf7158809cf4f3c');
  const iv = hexToUint8Array('000102030405060708090a0b0c0d0e0f');
  const plaintext = new Uint8Array([0x48, 0x65, 0x6c, 0x6c, 0x6f]); // "Hello"

  const cipher = crypto.createCipheriv('aes-128-cbc', key, iv);
  const encrypted = cipher.update(plaintext, null, 'hex');
  const final = cipher.final('hex');

  assert(typeof encrypted === 'string', 'Update should return hex string');
  assert(typeof final === 'string', 'Final should return hex string');
  assert(/^[0-9a-f]*$/.test(encrypted + final), 'Should be valid hex');
});

// Test 9: Error - final called twice
test('Error when final called twice', () => {
  const key = new Uint8Array(16);
  const iv = new Uint8Array(16);
  const cipher = crypto.createCipheriv('aes-128-cbc', key, iv);

  cipher.update(new Uint8Array([0x01, 0x02, 0x03]));
  cipher.final();

  let errorThrown = false;
  try {
    cipher.final();
  } catch (e) {
    errorThrown = true;
  }
  assert(errorThrown, 'Should throw error when final called twice');
});

// Test 10: Error - update after final
test('Error when update called after final', () => {
  const key = new Uint8Array(16);
  const iv = new Uint8Array(16);
  const cipher = crypto.createCipheriv('aes-128-cbc', key, iv);

  cipher.update(new Uint8Array([0x01, 0x02, 0x03]));
  cipher.final();

  let errorThrown = false;
  try {
    cipher.update(new Uint8Array([0x04, 0x05]));
  } catch (e) {
    errorThrown = true;
  }
  assert(errorThrown, 'Should throw error when update called after final');
});

// Test 11: createDecipheriv basic functionality
test('createDecipheriv returns a Decipher object', () => {
  const key = new Uint8Array(32);
  const iv = new Uint8Array(16);
  const decipher = crypto.createDecipheriv('aes-256-cbc', key, iv);
  assert(decipher !== null, 'Decipher object should not be null');
  assert(
    typeof decipher.update === 'function',
    'Decipher should have update method'
  );
  assert(
    typeof decipher.final === 'function',
    'Decipher should have final method'
  );
});

// Test 12: AES-192-CBC encryption/decryption
test('AES-192-CBC encryption and decryption', () => {
  const key = hexToUint8Array(
    '8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b'
  );
  const iv = hexToUint8Array('000102030405060708090a0b0c0d0e0f');
  const plaintext = new Uint8Array([0x74, 0x65, 0x73, 0x74]); // "test"

  const cipher = crypto.createCipheriv('aes-192-cbc', key, iv);
  const encrypted = new Uint8Array([
    ...cipher.update(plaintext),
    ...cipher.final(),
  ]);

  const decipher = crypto.createDecipheriv('aes-192-cbc', key, iv);
  const decrypted = new Uint8Array([
    ...decipher.update(encrypted),
    ...decipher.final(),
  ]);

  assert(decrypted.length === plaintext.length, 'Lengths should match');
  for (let i = 0; i < plaintext.length; i++) {
    assert(decrypted[i] === plaintext[i], `Byte ${i} should match`);
  }
});

// Test 13: Large data encryption
test('Encrypt and decrypt large data', () => {
  const key = new Uint8Array(32);
  const iv = new Uint8Array(16);
  const plaintext = new Uint8Array(1000);
  for (let i = 0; i < plaintext.length; i++) {
    plaintext[i] = i % 256;
  }

  const cipher = crypto.createCipheriv('aes-256-cbc', key, iv);
  const encrypted = new Uint8Array([
    ...cipher.update(plaintext),
    ...cipher.final(),
  ]);

  const decipher = crypto.createDecipheriv('aes-256-cbc', key, iv);
  const decrypted = new Uint8Array([
    ...decipher.update(encrypted),
    ...decipher.final(),
  ]);

  assert(decrypted.length === plaintext.length, 'Lengths should match');
  for (let i = 0; i < plaintext.length; i++) {
    assert(decrypted[i] === plaintext[i], `Byte ${i} should match`);
  }
});

// Test 14: Empty plaintext encryption
test('Encrypt and decrypt empty data', () => {
  const key = new Uint8Array(16);
  const iv = new Uint8Array(16);
  const plaintext = new Uint8Array(0);

  const cipher = crypto.createCipheriv('aes-128-cbc', key, iv);
  const encrypted = new Uint8Array([
    ...cipher.update(plaintext),
    ...cipher.final(),
  ]);

  const decipher = crypto.createDecipheriv('aes-128-cbc', key, iv);
  const decrypted = new Uint8Array([
    ...decipher.update(encrypted),
    ...decipher.final(),
  ]);

  assert(decrypted.length === 0, 'Decrypted empty data should be empty');
});

console.log(`\n${testsPassed} tests passed, ${testsFailed} tests failed`);

if (testsFailed > 0) {
  throw new Error('Some tests failed');
}
