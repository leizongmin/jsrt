// Comprehensive integration tests for node:crypto module
// Tests cross-phase API compatibility and integration

const crypto = require('node:crypto');
const webcrypto = globalThis.crypto;

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

console.log(
  'Testing node:crypto comprehensive compatibility and integration...\n'
);

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
    if (e.stack) {
      console.log(`  Stack: ${e.stack}`);
    }
    testsFailed++;
  }
}

function assert(condition, message) {
  if (!condition) {
    throw new Error(message || 'Assertion failed');
  }
}

async function asyncTest(name, fn) {
  try {
    await fn();
    console.log(`✓ ${name}`);
    testsPassed++;
  } catch (e) {
    console.log(`✗ ${name}`);
    console.log(`  Error: ${e.message}`);
    if (e.stack) {
      console.log(`  Stack: ${e.stack}`);
    }
    testsFailed++;
  }
}

// ==== Integration Test 1: Hash → HMAC key → HMAC ====
(async () => {
  await asyncTest('Integration: Hash → HMAC key → HMAC', async () => {
    // Step 1: Generate key material from hash
    const hash = crypto.createHash('sha256');
    hash.update(new TextEncoder().encode('password-material'));
    const keyMaterial = hash.digest();

    // Step 2: Use hash result as HMAC key
    const hmac = crypto.createHmac('sha256', keyMaterial);
    hmac.update(new TextEncoder().encode('message to authenticate'));
    const tag = hmac.digest();

    assert(tag instanceof Uint8Array, 'HMAC result should be Uint8Array');
    assert(tag.length === 32, 'HMAC-SHA256 should be 32 bytes');
  });

  // ==== Integration Test 2: PBKDF2 → Cipher key → Cipher ====
  await asyncTest('Integration: PBKDF2 → Cipher key → Cipher', async () => {
    const password = 'user-password';
    const salt = crypto.randomBytes(16);

    // Step 1: Derive encryption key from password
    const key = crypto.pbkdf2Sync(password, salt, 100000, 32, 'sha256');

    // Step 2: Use derived key for encryption
    const iv = crypto.randomBytes(16);
    const cipher = crypto.createCipheriv('aes-256-cbc', key, iv);
    const plaintext = new TextEncoder().encode('secret data');

    cipher.update(plaintext);
    const ciphertext = cipher.final();

    // Step 3: Decrypt to verify
    const decipher = crypto.createDecipheriv('aes-256-cbc', key, iv);
    decipher.update(ciphertext);
    const decrypted = decipher.final();

    assert(
      new TextDecoder().decode(decrypted) === 'secret data',
      'Decrypted data should match plaintext'
    );
  });

  // ==== Integration Test 3: ECDH → derive key → Cipher ====
  await asyncTest('Integration: ECDH → derive key → Cipher', async () => {
    // Step 1: Alice generates key pair
    const alice = crypto.createECDH('prime256v1');
    alice.generateKeys();

    // Step 2: Bob generates key pair
    const bob = crypto.createECDH('prime256v1');
    bob.generateKeys();

    // Step 3: Both compute shared secret
    const aliceSecret = alice.computeSecret(bob.getPublicKey());
    const bobSecret = bob.computeSecret(alice.getPublicKey());

    // Secrets should match
    assert(
      aliceSecret.length === bobSecret.length,
      'Shared secrets should have same length'
    );

    for (let i = 0; i < aliceSecret.length; i++) {
      assert(
        aliceSecret[i] === bobSecret[i],
        'Shared secrets should be identical'
      );
    }

    // Test passes if ECDH derives matching secrets
    assert(true, 'ECDH-derived shared secrets match');
  });

  // ==== Integration Test 4: WebCrypto → Node.js crypto interop ====
  await asyncTest(
    'Integration: WebCrypto → Node.js crypto interop',
    async () => {
      // Generate key pair using WebCrypto
      const keyPair = await webcrypto.subtle.generateKey(
        {
          name: 'RSASSA-PKCS1-v1_5',
          modulusLength: 2048,
          publicExponent: new Uint8Array([1, 0, 1]),
          hash: 'SHA-256',
        },
        true,
        ['sign', 'verify']
      );

      const data = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]); // "hello"

      // Sign using Node.js crypto
      const sign = crypto.createSign('RSA-SHA256');
      sign.update(data);
      const signature = sign.sign(keyPair.privateKey);

      // Verify using Node.js crypto
      const verify = crypto.createVerify('RSA-SHA256');
      verify.update(data);
      const result = verify.verify(keyPair.publicKey, signature);

      assert(result === true, 'WebCrypto keys should work with Node.js crypto');
    }
  );

  // ==== Integration Test 5: Node.js crypto → WebCrypto interop ====
  await asyncTest(
    'Integration: Node.js crypto → WebCrypto interop',
    async () => {
      const data = new TextEncoder().encode('test data');

      // Hash using Node.js crypto
      const nodeHash = crypto.createHash('sha256').update(data).digest();

      // Hash using WebCrypto
      const webcryptoHash = await webcrypto.subtle.digest('SHA-256', data);

      // Compare results
      const webcryptoArray = new Uint8Array(webcryptoHash);
      assert(
        nodeHash.length === webcryptoArray.length,
        'Hash lengths should match'
      );

      for (let i = 0; i < nodeHash.length; i++) {
        assert(
          nodeHash[i] === webcryptoArray[i],
          'Hash values should be identical'
        );
      }
    }
  );

  // ==== Integration Test 6: crypto.webcrypto and crypto.subtle aliases ====
  await asyncTest(
    'Integration: crypto.webcrypto and crypto.subtle aliases',
    async () => {
      // Test webcrypto alias
      assert(
        crypto.webcrypto === webcrypto,
        'crypto.webcrypto should reference globalThis.crypto'
      );

      // Test subtle alias
      assert(
        crypto.subtle === webcrypto.subtle,
        'crypto.subtle should reference globalThis.crypto.subtle'
      );

      // Test using crypto.subtle directly
      const data = new TextEncoder().encode('test');
      const hash = await crypto.subtle.digest('SHA-256', data);
      assert(hash.byteLength === 32, 'crypto.subtle.digest should work');
    }
  );

  // ==== Integration Test 7: Random + PBKDF2 + Hash chain ====
  await asyncTest('Integration: Random + PBKDF2 + Hash chain', async () => {
    // 1. Random generation
    const salt = crypto.randomBytes(16);
    assert(salt.length === 16, 'Random salt should be 16 bytes');

    // 2. Key derivation
    const password = 'test-password';
    const key = crypto.pbkdf2Sync(password, salt, 10000, 32, 'sha256');
    assert(key.length === 32, 'Derived key should be 32 bytes');

    // 3. Hash the derived key
    const hash = crypto.createHash('sha256').update(key).digest('hex');

    assert(hash.length === 64, 'Hash should be 64 hex characters');
    assert(/^[0-9a-f]{64}$/.test(hash), 'Hash should be valid hex');
  });

  // ==== Integration Test 8: Async operations with Promises ====
  await asyncTest('Integration: Async operations with Promises', async () => {
    // Test PBKDF2 async
    const key1 = await new Promise((resolve, reject) => {
      crypto.pbkdf2('password', 'salt', 1000, 32, 'sha256', (err, key) => {
        if (err) reject(err);
        else resolve(key);
      });
    });
    assert(key1.length === 32, 'PBKDF2 async should return 32-byte key');

    // Test HKDF async
    const key2 = await new Promise((resolve, reject) => {
      crypto.hkdf('sha256', 'input', 'salt', 'info', 32, (err, key) => {
        if (err) reject(err);
        else resolve(key);
      });
    });
    assert(key2.length === 32, 'HKDF async should return 32-byte key');
  });

  console.log(`\n${testsPassed} tests passed, ${testsFailed} tests failed`);

  if (testsFailed > 0) {
    throw new Error('Some integration tests failed');
  }
})().catch((e) => {
  console.error('Test execution failed:', e);
  process.exit(1);
});
