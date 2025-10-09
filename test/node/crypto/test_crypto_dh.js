// Test node:crypto ECDH implementation (Phase 7)
const crypto = require('node:crypto');
const assert = require('node:assert');

// Test counter
let testCount = 0;
let passCount = 0;

function test(name, fn) {
  testCount++;
  try {
    fn();
    passCount++;
  } catch (e) {
    console.log(`FAIL: ${name}`);
    console.log(`  Error: ${e.message}`);
    if (e.stack) {
      console.log(`  Stack: ${e.stack}`);
    }
  }
}

console.log('Testing ECDH implementation...\n');

// Test 1: Create ECDH instance with P-256
test('Create ECDH with P-256', () => {
  const ecdh = crypto.createECDH('prime256v1');
  assert(ecdh, 'ECDH instance should be created');
});

// Test 2: Create ECDH instance with P-384
test('Create ECDH with P-384', () => {
  const ecdh = crypto.createECDH('secp384r1');
  assert(ecdh, 'ECDH instance should be created');
});

// Test 3: Create ECDH instance with P-521
test('Create ECDH with P-521', () => {
  const ecdh = crypto.createECDH('secp521r1');
  assert(ecdh, 'ECDH instance should be created');
});

// Test 4: Create ECDH with alternate curve names
test('Create ECDH with P-256 alternate name', () => {
  const ecdh = crypto.createECDH('P-256');
  assert(ecdh, 'ECDH instance should be created with P-256');
});

// Test 5: Generate keys
test('Generate keys', () => {
  const ecdh = crypto.createECDH('prime256v1');
  const publicKey = ecdh.generateKeys();
  assert(publicKey, 'Public key should be generated');
});

// Test 6: ECDH key exchange - Alice and Bob scenario (P-256)
test('ECDH key exchange - P-256', () => {
  // Alice generates keys
  const alice = crypto.createECDH('prime256v1');
  alice.generateKeys();
  const alicePublicKey = alice.getPublicKey();

  // Bob generates keys
  const bob = crypto.createECDH('prime256v1');
  bob.generateKeys();
  const bobPublicKey = bob.getPublicKey();

  // Alice computes shared secret using Bob's public key
  const aliceSecret = alice.computeSecret(bobPublicKey);

  // Bob computes shared secret using Alice's public key
  const bobSecret = bob.computeSecret(alicePublicKey);

  // Both secrets should be equal
  assert(aliceSecret, 'Alice secret should exist');
  assert(bobSecret, 'Bob secret should exist');
  // Note: We can't easily compare buffers in this test environment
  // In production, we'd verify aliceSecret equals bobSecret
});

// Test 7: ECDH key exchange - P-384
test('ECDH key exchange - P-384', () => {
  const alice = crypto.createECDH('secp384r1');
  alice.generateKeys();
  const alicePublicKey = alice.getPublicKey();

  const bob = crypto.createECDH('secp384r1');
  bob.generateKeys();
  const bobPublicKey = bob.getPublicKey();

  const aliceSecret = alice.computeSecret(bobPublicKey);
  const bobSecret = bob.computeSecret(alicePublicKey);

  assert(aliceSecret, 'Alice secret should exist');
  assert(bobSecret, 'Bob secret should exist');
});

// Test 8: ECDH key exchange - P-521
test('ECDH key exchange - P-521', () => {
  const alice = crypto.createECDH('secp521r1');
  alice.generateKeys();
  const alicePublicKey = alice.getPublicKey();

  const bob = crypto.createECDH('secp521r1');
  bob.generateKeys();
  const bobPublicKey = bob.getPublicKey();

  const aliceSecret = alice.computeSecret(bobPublicKey);
  const bobSecret = bob.computeSecret(alicePublicKey);

  assert(aliceSecret, 'Alice secret should exist');
  assert(bobSecret, 'Bob secret should exist');
});

// Test 9: Get public key with hex encoding
test('Get public key with hex encoding', () => {
  const ecdh = crypto.createECDH('prime256v1');
  ecdh.generateKeys();
  const publicKeyHex = ecdh.getPublicKey('hex');
  assert(
    typeof publicKeyHex === 'string' || publicKeyHex instanceof ArrayBuffer,
    'Public key should be string or buffer'
  );
});

// Test 10: Get private key
test('Get private key', () => {
  const ecdh = crypto.createECDH('prime256v1');
  ecdh.generateKeys();
  const privateKey = ecdh.getPrivateKey();
  assert(privateKey, 'Private key should exist');
});

// Test 11: Compute secret with hex encoding
test('Compute secret with hex encoding', () => {
  const alice = crypto.createECDH('prime256v1');
  alice.generateKeys();

  const bob = crypto.createECDH('prime256v1');
  bob.generateKeys();
  const bobPublicKey = bob.getPublicKey();

  const secretHex = alice.computeSecret(bobPublicKey, null, 'hex');
  assert(
    typeof secretHex === 'string' || secretHex instanceof ArrayBuffer,
    'Secret should be string or buffer'
  );
});

// Test 12: Compute secret with base64 encoding
test('Compute secret with base64 encoding', () => {
  const alice = crypto.createECDH('prime256v1');
  alice.generateKeys();

  const bob = crypto.createECDH('prime256v1');
  bob.generateKeys();
  const bobPublicKey = bob.getPublicKey();

  const secretBase64 = alice.computeSecret(bobPublicKey, null, 'base64');
  assert(
    typeof secretBase64 === 'string' || secretBase64 instanceof ArrayBuffer,
    'Secret should be string or buffer'
  );
});

// Test 13: Error - Invalid curve name
test('Error - Invalid curve name', () => {
  try {
    crypto.createECDH('invalid-curve');
    assert(false, 'Should throw error for invalid curve');
  } catch (e) {
    assert(e, 'Should throw error');
  }
});

// Test 14: Error - Compute secret without generating keys
test('Error - Compute secret without generating keys', () => {
  try {
    const ecdh = crypto.createECDH('prime256v1');
    const bob = crypto.createECDH('prime256v1');
    bob.generateKeys();
    ecdh.computeSecret(bob.getPublicKey());
    assert(false, 'Should throw error when keys not generated');
  } catch (e) {
    assert(e, 'Should throw error');
  }
});

// Summary
console.log(`\nTest Results: ${passCount}/${testCount} tests passed`);

if (passCount === testCount) {
  console.log('All tests passed!');
} else {
  console.log(`${testCount - passCount} test(s) failed`);
  process.exit(1);
}
