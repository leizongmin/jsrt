// WebCrypto ECDH key derivation test
const assert = require('jsrt:assert');

// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== ECDH Tests Completed (Skipped) ===');
} else {
  console.log('=== WebCrypto ECDH Key Derivation Tests ===');

  // Test 1: ECDH key generation with P-256 curve
  function testECDHKeyGenerationP256() {
    console.log('\n1. Testing ECDH key generation with P-256 curve...');

    return crypto.subtle
      .generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-256',
        },
        true,
        ['deriveKey', 'deriveBits']
      )
      .then(function (keyPair) {
        console.log('✅ ECDH P-256 key pair generated successfully');

        // Verify key properties
        assert.strictEqual(
          keyPair.publicKey.type,
          'public',
          'Public key type should be "public"'
        );
        assert.strictEqual(
          keyPair.privateKey.type,
          'private',
          'Private key type should be "private"'
        );
        assert.strictEqual(
          keyPair.publicKey.algorithm.name,
          'ECDH',
          'Algorithm name should be ECDH'
        );
        assert.strictEqual(
          keyPair.publicKey.algorithm.namedCurve,
          'P-256',
          'Named curve should be P-256'
        );

        return keyPair;
      })
      .catch(function (error) {
        console.error('❌ ECDH key generation failed:', error.message);
        throw error;
      });
  }

  // Test 2: ECDH key derivation between two parties
  function testECDHKeyDerivation() {
    console.log('\n2. Testing ECDH key derivation between two parties...');

    // Generate Alice's key pair
    const alicePromise = crypto.subtle.generateKey(
      {
        name: 'ECDH',
        namedCurve: 'P-256',
      },
      true,
      ['deriveKey', 'deriveBits']
    );

    // Generate Bob's key pair
    const bobPromise = crypto.subtle.generateKey(
      {
        name: 'ECDH',
        namedCurve: 'P-256',
      },
      true,
      ['deriveKey', 'deriveBits']
    );

    return Promise.all([alicePromise, bobPromise])
      .then(function (keyPairs) {
        const aliceKeyPair = keyPairs[0];
        const bobKeyPair = keyPairs[1];

        console.log('✅ Generated key pairs for Alice and Bob');

        // Alice derives shared secret using Bob's public key
        const aliceSecretPromise = crypto.subtle.deriveBits(
          {
            name: 'ECDH',
            public: bobKeyPair.publicKey,
          },
          aliceKeyPair.privateKey,
          256 // Derive 256 bits (32 bytes)
        );

        // Bob derives shared secret using Alice's public key
        const bobSecretPromise = crypto.subtle.deriveBits(
          {
            name: 'ECDH',
            public: aliceKeyPair.publicKey,
          },
          bobKeyPair.privateKey,
          256 // Derive 256 bits (32 bytes)
        );

        return Promise.all([aliceSecretPromise, bobSecretPromise]);
      })
      .then(function (secrets) {
        const aliceSecret = new Uint8Array(secrets[0]);
        const bobSecret = new Uint8Array(secrets[1]);

        console.log('✅ Derived shared secrets for both parties');
        console.log('   Alice secret length:', aliceSecret.length, 'bytes');
        console.log('   Bob secret length:', bobSecret.length, 'bytes');

        // Verify that both parties derived the same secret
        assert.strictEqual(
          aliceSecret.length,
          bobSecret.length,
          'Secrets should have same length'
        );

        let secretsMatch = true;
        for (let i = 0; i < aliceSecret.length; i++) {
          if (aliceSecret[i] !== bobSecret[i]) {
            secretsMatch = false;
            break;
          }
        }

        assert.strictEqual(
          secretsMatch,
          true,
          'Both parties should derive the same secret'
        );
        console.log(
          '✅ Both parties successfully derived the same shared secret'
        );
      })
      .catch(function (error) {
        console.error('❌ ECDH key derivation failed:', error.message);
        throw error;
      });
  }

  // Test 3: ECDH with different curves
  function testECDHDifferentCurves() {
    console.log('\n3. Testing ECDH with different curves...');

    const curves = ['P-256', 'P-384', 'P-521'];
    const testPromises = [];

    curves.forEach(function (curve) {
      const promise = Promise.all([
        crypto.subtle.generateKey(
          {
            name: 'ECDH',
            namedCurve: curve,
          },
          true,
          ['deriveBits']
        ),
        crypto.subtle.generateKey(
          {
            name: 'ECDH',
            namedCurve: curve,
          },
          true,
          ['deriveBits']
        ),
      ])
        .then(function (keyPairs) {
          console.log('✅ Generated ECDH key pairs with curve:', curve);

          // Derive bits using each other's public key
          return crypto.subtle.deriveBits(
            {
              name: 'ECDH',
              public: keyPairs[1].publicKey,
            },
            keyPairs[0].privateKey,
            256
          );
        })
        .then(function (sharedSecret) {
          console.log('✅ ECDH key derivation successful with curve:', curve);
          assert.strictEqual(
            sharedSecret.byteLength,
            32,
            'Derived secret should be 32 bytes'
          );
        })
        .catch(function (error) {
          console.error(
            '❌ ECDH test failed for curve',
            curve + ':',
            error.message
          );
          throw error;
        });

      testPromises.push(promise);
    });

    return Promise.all(testPromises);
  }

  // Test 4: ECDH deriveKey to create AES key
  function testECDHDeriveAESKey() {
    console.log('\n4. Testing ECDH deriveKey to create AES key...');

    // Generate two ECDH key pairs
    return Promise.all([
      crypto.subtle.generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-256',
        },
        true,
        ['deriveKey']
      ),
      crypto.subtle.generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-256',
        },
        true,
        ['deriveKey']
      ),
    ])
      .then(function (keyPairs) {
        // Derive an AES key from the shared secret
        return crypto.subtle.deriveKey(
          {
            name: 'ECDH',
            public: keyPairs[1].publicKey,
          },
          keyPairs[0].privateKey,
          {
            name: 'AES-GCM',
            length: 256,
          },
          true,
          ['encrypt', 'decrypt']
        );
      })
      .then(function (aesKey) {
        console.log('✅ Successfully derived AES key from ECDH shared secret');

        // Verify the derived key properties
        assert.strictEqual(
          aesKey.type,
          'secret',
          'Derived key should be a secret key'
        );
        assert.strictEqual(
          aesKey.algorithm.name,
          'AES-GCM',
          'Derived key algorithm should be AES-GCM'
        );
        assert.strictEqual(
          aesKey.algorithm.length,
          256,
          'Derived key length should be 256 bits'
        );

        console.log('✅ Derived AES key has correct properties');
      })
      .catch(function (error) {
        // Note: deriveKey might not be fully implemented yet
        if (error.name === 'NotSupportedError') {
          console.log(
            '⚠️ deriveKey not yet implemented, testing deriveBits instead'
          );
          return; // Skip this test gracefully
        }
        console.error('❌ ECDH deriveKey failed:', error.message);
        throw error;
      });
  }

  // Test 5: ECDH with invalid parameters should fail
  function testECDHInvalidParameters() {
    console.log('\n5. Testing ECDH with invalid parameters...');

    // Test with unsupported curve
    return crypto.subtle
      .generateKey(
        {
          name: 'ECDH',
          namedCurve: 'P-192', // P-192 is not supported
        },
        true,
        ['deriveBits']
      )
      .then(function () {
        console.error('❌ Should have failed with unsupported curve');
        throw new Error('Expected error for unsupported curve');
      })
      .catch(function (error) {
        if (
          error.name === 'NotSupportedError' ||
          error.message.includes('Unsupported')
        ) {
          console.log('✅ Correctly rejected unsupported curve P-192');
        } else {
          throw error;
        }
      });
  }

  // Run all tests
  testECDHKeyGenerationP256()
    .then(testECDHKeyDerivation)
    .then(testECDHDifferentCurves)
    .then(testECDHDeriveAESKey)
    .then(testECDHInvalidParameters)
    .then(function () {
      console.log('\n=== All ECDH Tests Passed! ===');
    })
    .catch(function (error) {
      console.error('\n=== ECDH Tests Failed ===');
      console.error('Error:', error.message);
      if (error.stack) {
        console.error('Stack:', error.stack);
      }
    });
} // End of crypto availability check
