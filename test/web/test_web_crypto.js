// Comprehensive WebCrypto API tests for WinterCG compliance
const assert = require('jsrt:assert');

// Test counter for statistics
let testCount = 0;
let passCount = 0;
let skipCount = 0;

function runTest(testName, testFn) {
  testCount++;
  try {
    if (testFn()) {
      passCount++;
    }
  } catch (error) {
    console.log(`❌ FAIL: ${testName} - ${error.message}`);
    throw error;
  }
}

function skipTest(testName, reason) {
  testCount++;
  skipCount++;
  console.log(`❌ SKIP: ${testName} - ${reason}`);
}

// Check if crypto is available
if (typeof crypto === 'undefined') {
  skipTest(
    'All crypto tests',
    'crypto object not available (OpenSSL not found)'
  );
} else {
  // Basic crypto object tests
  runTest('crypto object existence', () => {
    assert.notStrictEqual(
      typeof crypto,
      'undefined',
      'crypto object should be available'
    );
    return true;
  });

  // crypto.getRandomValues tests
  runTest('crypto.getRandomValues is a function', () => {
    assert.strictEqual(
      typeof crypto.getRandomValues,
      'function',
      'crypto.getRandomValues should be a function'
    );
    return true;
  });

  runTest('crypto.getRandomValues with Uint8Array', () => {
    const uint8Array = new Uint8Array(16);
    const result = crypto.getRandomValues(uint8Array);
    assert.strictEqual(
      result,
      uint8Array,
      'crypto.getRandomValues should return the same array'
    );

    // Check that at least some bytes were modified
    let allZeros = true;
    for (let i = 0; i < uint8Array.length; i++) {
      if (uint8Array[i] !== 0) {
        allZeros = false;
        break;
      }
    }
    assert.strictEqual(allZeros, false, 'Random values should not be all zero');
    return true;
  });

  // crypto.randomUUID tests
  runTest('crypto.randomUUID is a function', () => {
    assert.strictEqual(
      typeof crypto.randomUUID,
      'function',
      'crypto.randomUUID should be a function'
    );
    return true;
  });

  runTest('crypto.randomUUID format validation', () => {
    const uuid1 = crypto.randomUUID();
    assert.strictEqual(
      typeof uuid1,
      'string',
      'crypto.randomUUID should return a string'
    );
    assert.strictEqual(uuid1.length, 36, 'UUID should be 36 characters long');

    const uuidPattern =
      /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
    assert.strictEqual(
      uuidPattern.test(uuid1),
      true,
      'UUID should match RFC 4122 v4 format'
    );
    return true;
  });

  // crypto.subtle tests
  if (!crypto.subtle) {
    skipTest('All crypto.subtle tests', 'crypto.subtle not available');
  } else {
    runTest('crypto.subtle object existence', () => {
      assert.notStrictEqual(
        typeof crypto.subtle,
        'undefined',
        'crypto.subtle should be available'
      );
      return true;
    });

    // Digest tests
    runTest('crypto.subtle.digest SHA-256 (async)', async () => {
      const data = new TextEncoder().encode('hello world');
      const hashBuffer = await crypto.subtle.digest('SHA-256', data);
      assert.strictEqual(
        hashBuffer.byteLength,
        32,
        'SHA-256 hash should be 32 bytes'
      );
      return true;
    });

    runTest('crypto.subtle.digest SHA-384 (async)', async () => {
      const data = new TextEncoder().encode('test data');
      const hashBuffer = await crypto.subtle.digest('SHA-384', data);
      assert.strictEqual(
        hashBuffer.byteLength,
        48,
        'SHA-384 hash should be 48 bytes'
      );
      return true;
    });

    runTest('crypto.subtle.digest SHA-512 (async)', async () => {
      const data = new TextEncoder().encode('test data');
      const hashBuffer = await crypto.subtle.digest('SHA-512', data);
      assert.strictEqual(
        hashBuffer.byteLength,
        64,
        'SHA-512 hash should be 64 bytes'
      );
      return true;
    });

    // AES tests
    runTest('AES-GCM key generation (async)', async () => {
      const key = await crypto.subtle.generateKey(
        { name: 'AES-GCM', length: 256 },
        true,
        ['encrypt', 'decrypt']
      );
      assert.strictEqual(key.type, 'secret', 'AES key should be secret type');
      assert.strictEqual(
        key.algorithm.name,
        'AES-GCM',
        'Algorithm should be AES-GCM'
      );
      assert.strictEqual(key.algorithm.length, 256, 'Key length should be 256');
      return true;
    });

    runTest('AES-CBC key generation (async)', async () => {
      const key = await crypto.subtle.generateKey(
        { name: 'AES-CBC', length: 256 },
        true,
        ['encrypt', 'decrypt']
      );
      assert.strictEqual(key.type, 'secret', 'AES key should be secret type');
      assert.strictEqual(
        key.algorithm.name,
        'AES-CBC',
        'Algorithm should be AES-CBC'
      );
      return true;
    });

    // HMAC tests
    runTest('HMAC key generation and verification (async)', async () => {
      const key = await crypto.subtle.generateKey(
        { name: 'HMAC', hash: 'SHA-256' },
        true,
        ['sign', 'verify']
      );
      assert.strictEqual(key.type, 'secret', 'HMAC key should be secret type');
      assert.strictEqual(
        key.algorithm.name,
        'HMAC',
        'Algorithm should be HMAC'
      );

      const data = new TextEncoder().encode('test message');
      const signature = await crypto.subtle.sign('HMAC', key, data);
      assert.strictEqual(
        signature.byteLength,
        32,
        'HMAC-SHA-256 signature should be 32 bytes'
      );

      const isValid = await crypto.subtle.verify('HMAC', key, signature, data);
      assert.strictEqual(
        isValid,
        true,
        'HMAC signature should verify correctly'
      );
      return true;
    });

    // RSA tests
    runTest('RSA-OAEP key generation (async)', async () => {
      const keyPair = await crypto.subtle.generateKey(
        {
          name: 'RSA-OAEP',
          modulusLength: 2048,
          publicExponent: new Uint8Array([1, 0, 1]),
          hash: 'SHA-256',
        },
        true,
        ['encrypt', 'decrypt']
      );
      assert.strictEqual(
        keyPair.publicKey.type,
        'public',
        'Public key should be public type'
      );
      assert.strictEqual(
        keyPair.privateKey.type,
        'private',
        'Private key should be private type'
      );
      assert.strictEqual(
        keyPair.publicKey.algorithm.name,
        'RSA-OAEP',
        'Algorithm should be RSA-OAEP'
      );
      return true;
    });

    runTest(
      'RSASSA-PKCS1-v1_5 key generation and signing (async)',
      async () => {
        const keyPair = await crypto.subtle.generateKey(
          {
            name: 'RSASSA-PKCS1-v1_5',
            modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
            hash: 'SHA-256',
          },
          true,
          ['sign', 'verify']
        );

        const data = new TextEncoder().encode('test signature data');
        const signature = await crypto.subtle.sign(
          { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
          keyPair.privateKey,
          data
        );
        assert.ok(signature.byteLength > 0, 'Signature should have content');

        const isValid = await crypto.subtle.verify(
          { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
          keyPair.publicKey,
          signature,
          data
        );
        assert.strictEqual(
          isValid,
          true,
          'RSA signature should verify correctly'
        );
        return true;
      }
    );

    // EC tests
    runTest('ECDSA key generation and signing (async)', async () => {
      const keyPair = await crypto.subtle.generateKey(
        { name: 'ECDSA', namedCurve: 'P-256' },
        true,
        ['sign', 'verify']
      );
      assert.strictEqual(
        keyPair.publicKey.type,
        'public',
        'EC public key should be public type'
      );
      assert.strictEqual(
        keyPair.privateKey.type,
        'private',
        'EC private key should be private type'
      );

      const data = new TextEncoder().encode('test ecdsa data');
      const signature = await crypto.subtle.sign(
        { name: 'ECDSA', hash: 'SHA-256' },
        keyPair.privateKey,
        data
      );
      assert.ok(
        signature.byteLength > 0,
        'ECDSA signature should have content'
      );

      const isValid = await crypto.subtle.verify(
        { name: 'ECDSA', hash: 'SHA-256' },
        keyPair.publicKey,
        signature,
        data
      );
      assert.strictEqual(
        isValid,
        true,
        'ECDSA signature should verify correctly'
      );
      return true;
    });

    runTest('ECDH key generation and key derivation (async)', async () => {
      const aliceKeyPair = await crypto.subtle.generateKey(
        { name: 'ECDH', namedCurve: 'P-256' },
        true,
        ['deriveBits']
      );

      const bobKeyPair = await crypto.subtle.generateKey(
        { name: 'ECDH', namedCurve: 'P-256' },
        true,
        ['deriveBits']
      );

      // Alice derives shared secret using Bob's public key
      const aliceSharedSecret = await crypto.subtle.deriveBits(
        { name: 'ECDH', public: bobKeyPair.publicKey },
        aliceKeyPair.privateKey,
        256
      );
      assert.strictEqual(
        aliceSharedSecret.byteLength,
        32,
        'Shared secret should be 32 bytes'
      );
      return true;
    });

    // Key derivation tests
    runTest('PBKDF2 key derivation (async)', async () => {
      const password = await crypto.subtle.importKey(
        'raw',
        new TextEncoder().encode('password123'),
        { name: 'PBKDF2' },
        false,
        ['deriveKey']
      );

      const derivedKey = await crypto.subtle.deriveKey(
        {
          name: 'PBKDF2',
          salt: new TextEncoder().encode('salt'),
          iterations: 1000,
          hash: 'SHA-256',
        },
        password,
        { name: 'AES-GCM', length: 256 },
        false,
        ['encrypt', 'decrypt']
      );

      assert.strictEqual(
        derivedKey.type,
        'secret',
        'Derived key should be secret type'
      );
      assert.strictEqual(
        derivedKey.algorithm.name,
        'AES-GCM',
        'Derived key should be AES-GCM'
      );
      return true;
    });

    // Execute async tests sequentially
    Promise.resolve()
      .then(() =>
        runTest('crypto.subtle.digest SHA-256 (async)', async () => {
          const data = new TextEncoder().encode('hello world');
          const hashBuffer = await crypto.subtle.digest('SHA-256', data);
          assert.strictEqual(
            hashBuffer.byteLength,
            32,
            'SHA-256 hash should be 32 bytes'
          );
          return true;
        })
      )
      .then(() =>
        runTest('crypto.subtle.digest SHA-384 (async)', async () => {
          const data = new TextEncoder().encode('test data');
          const hashBuffer = await crypto.subtle.digest('SHA-384', data);
          assert.strictEqual(
            hashBuffer.byteLength,
            48,
            'SHA-384 hash should be 48 bytes'
          );
          return true;
        })
      )
      .then(() =>
        runTest('crypto.subtle.digest SHA-512 (async)', async () => {
          const data = new TextEncoder().encode('test data');
          const hashBuffer = await crypto.subtle.digest('SHA-512', data);
          assert.strictEqual(
            hashBuffer.byteLength,
            64,
            'SHA-512 hash should be 64 bytes'
          );
          return true;
        })
      )
      .then(() =>
        runTest('AES-GCM key generation (async)', async () => {
          const key = await crypto.subtle.generateKey(
            { name: 'AES-GCM', length: 256 },
            true,
            ['encrypt', 'decrypt']
          );
          assert.strictEqual(
            key.type,
            'secret',
            'AES key should be secret type'
          );
          assert.strictEqual(
            key.algorithm.name,
            'AES-GCM',
            'Algorithm should be AES-GCM'
          );
          assert.strictEqual(
            key.algorithm.length,
            256,
            'Key length should be 256'
          );
          return true;
        })
      )
      .then(() =>
        runTest('AES-CBC key generation (async)', async () => {
          const key = await crypto.subtle.generateKey(
            { name: 'AES-CBC', length: 256 },
            true,
            ['encrypt', 'decrypt']
          );
          assert.strictEqual(
            key.type,
            'secret',
            'AES key should be secret type'
          );
          assert.strictEqual(
            key.algorithm.name,
            'AES-CBC',
            'Algorithm should be AES-CBC'
          );
          return true;
        })
      )
      .then(() =>
        runTest('HMAC key generation and verification (async)', async () => {
          const key = await crypto.subtle.generateKey(
            { name: 'HMAC', hash: 'SHA-256' },
            true,
            ['sign', 'verify']
          );
          assert.strictEqual(
            key.type,
            'secret',
            'HMAC key should be secret type'
          );
          assert.strictEqual(
            key.algorithm.name,
            'HMAC',
            'Algorithm should be HMAC'
          );

          const data = new TextEncoder().encode('test message');
          const signature = await crypto.subtle.sign('HMAC', key, data);
          assert.strictEqual(
            signature.byteLength,
            32,
            'HMAC-SHA-256 signature should be 32 bytes'
          );

          const isValid = await crypto.subtle.verify(
            'HMAC',
            key,
            signature,
            data
          );
          assert.strictEqual(
            isValid,
            true,
            'HMAC signature should verify correctly'
          );
          return true;
        })
      )
      .then(() =>
        runTest('RSA-OAEP key generation (async)', async () => {
          const keyPair = await crypto.subtle.generateKey(
            {
              name: 'RSA-OAEP',
              modulusLength: 2048,
              publicExponent: new Uint8Array([1, 0, 1]),
              hash: 'SHA-256',
            },
            true,
            ['encrypt', 'decrypt']
          );
          assert.strictEqual(
            keyPair.publicKey.type,
            'public',
            'Public key should be public type'
          );
          assert.strictEqual(
            keyPair.privateKey.type,
            'private',
            'Private key should be private type'
          );
          assert.strictEqual(
            keyPair.publicKey.algorithm.name,
            'RSA-OAEP',
            'Algorithm should be RSA-OAEP'
          );
          return true;
        })
      )
      .then(() =>
        runTest(
          'RSASSA-PKCS1-v1_5 key generation and signing (async)',
          async () => {
            const keyPair = await crypto.subtle.generateKey(
              {
                name: 'RSASSA-PKCS1-v1_5',
                modulusLength: 2048,
                publicExponent: new Uint8Array([1, 0, 1]),
                hash: 'SHA-256',
              },
              true,
              ['sign', 'verify']
            );

            const data = new TextEncoder().encode('test signature data');
            const signature = await crypto.subtle.sign(
              { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
              keyPair.privateKey,
              data
            );
            assert.ok(
              signature.byteLength > 0,
              'Signature should have content'
            );

            const isValid = await crypto.subtle.verify(
              { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
              keyPair.publicKey,
              signature,
              data
            );
            assert.strictEqual(
              isValid,
              true,
              'RSA signature should verify correctly'
            );
            return true;
          }
        )
      )
      .then(() =>
        runTest('ECDSA key generation and signing (async)', async () => {
          const keyPair = await crypto.subtle.generateKey(
            { name: 'ECDSA', namedCurve: 'P-256' },
            true,
            ['sign', 'verify']
          );
          assert.strictEqual(
            keyPair.publicKey.type,
            'public',
            'EC public key should be public type'
          );
          assert.strictEqual(
            keyPair.privateKey.type,
            'private',
            'EC private key should be private type'
          );

          const data = new TextEncoder().encode('test ecdsa data');
          const signature = await crypto.subtle.sign(
            { name: 'ECDSA', hash: 'SHA-256' },
            keyPair.privateKey,
            data
          );
          assert.ok(
            signature.byteLength > 0,
            'ECDSA signature should have content'
          );

          const isValid = await crypto.subtle.verify(
            { name: 'ECDSA', hash: 'SHA-256' },
            keyPair.publicKey,
            signature,
            data
          );
          assert.strictEqual(
            isValid,
            true,
            'ECDSA signature should verify correctly'
          );
          return true;
        })
      )
      .then(() =>
        runTest('ECDH key generation and key derivation (async)', async () => {
          const aliceKeyPair = await crypto.subtle.generateKey(
            { name: 'ECDH', namedCurve: 'P-256' },
            true,
            ['deriveBits']
          );

          const bobKeyPair = await crypto.subtle.generateKey(
            { name: 'ECDH', namedCurve: 'P-256' },
            true,
            ['deriveBits']
          );

          // Alice derives shared secret using Bob's public key
          const aliceSharedSecret = await crypto.subtle.deriveBits(
            { name: 'ECDH', public: bobKeyPair.publicKey },
            aliceKeyPair.privateKey,
            256
          );
          assert.strictEqual(
            aliceSharedSecret.byteLength,
            32,
            'Shared secret should be 32 bytes'
          );
          return true;
        })
      )
      .then(() =>
        runTest('PBKDF2 key derivation (async)', async () => {
          const password = await crypto.subtle.importKey(
            'raw',
            new TextEncoder().encode('password123'),
            { name: 'PBKDF2' },
            false,
            ['deriveKey']
          );

          const derivedKey = await crypto.subtle.deriveKey(
            {
              name: 'PBKDF2',
              salt: new TextEncoder().encode('salt'),
              iterations: 1000,
              hash: 'SHA-256',
            },
            password,
            { name: 'AES-GCM', length: 256 },
            false,
            ['encrypt', 'decrypt']
          );

          assert.strictEqual(
            derivedKey.type,
            'secret',
            'Derived key should be secret type'
          );
          assert.strictEqual(
            derivedKey.algorithm.name,
            'AES-GCM',
            'Derived key should be AES-GCM'
          );
          return true;
        })
      )
      .then(() => {
        // Tests completed successfully
      })
      .catch((error) => {
        console.log(`❌ Fatal test error: ${error.message}`);
        // Test Summary
        console.log(`Total Tests: ${testCount}`);
        console.log(`Passed: ${passCount}`);
        console.log(`Skipped: ${skipCount}`);
        console.log(`Failed: ${testCount - passCount - skipCount}`);
      });
  }
}
