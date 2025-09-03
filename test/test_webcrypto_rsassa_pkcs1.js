// Test RSASSA-PKCS1-v1_5 signature/verification functionality
const assert = require('jsrt:assert');

// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== RSASSA-PKCS1-v1_5 Tests Completed (Skipped) ===');
} else {
  function testRSASSAPKCS1v15KeyGeneration() {
    console.log('Testing RSASSA-PKCS1-v1_5 key generation...');

    return crypto.subtle
      .generateKey(
        {
          name: 'RSASSA-PKCS1-v1_5',
          modulusLength: 2048,
          publicExponent: new Uint8Array([1, 0, 1]), // 65537
          hash: 'SHA-256',
        },
        true, // extractable
        ['sign', 'verify']
      )
      .then(function (keyPair) {
        console.log('✓ RSASSA-PKCS1-v1_5 key generation successful');

        // Use assert for actual verification
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
          'RSASSA-PKCS1-v1_5',
          'Algorithm name should be RSASSA-PKCS1-v1_5'
        );
        assert.strictEqual(
          keyPair.publicKey.algorithm.modulusLength,
          2048,
          'Modulus length should be 2048'
        );
        assert.strictEqual(
          keyPair.publicKey.algorithm.hash.name,
          'SHA-256',
          'Hash algorithm should be SHA-256'
        );

        console.log('Public key type:', keyPair.publicKey.type);
        console.log('Private key type:', keyPair.privateKey.type);
        console.log('Algorithm name:', keyPair.publicKey.algorithm.name);
        console.log(
          'Modulus length:',
          keyPair.publicKey.algorithm.modulusLength
        );
        console.log('Hash algorithm:', keyPair.publicKey.algorithm.hash.name);

        return keyPair;
      })
      .catch(function (error) {
        console.error('✗ RSASSA-PKCS1-v1_5 key generation failed:', error);
        throw error;
      });
  }

  function testRSASSAPKCS1v15Signature(keyPair) {
    console.log('\nTesting RSASSA-PKCS1-v1_5 signature/verification...');

    const data = new TextEncoder().encode('Hello, RSASSA-PKCS1-v1_5!');

    // Sign with private key
    return crypto.subtle
      .sign(
        {
          name: 'RSASSA-PKCS1-v1_5',
        },
        keyPair.privateKey,
        data
      )
      .then(function (signature) {
        console.log(
          '✓ RSASSA-PKCS1-v1_5 signature successful, signature length:',
          signature.byteLength
        );

        // Verify with public key
        return crypto.subtle.verify(
          {
            name: 'RSASSA-PKCS1-v1_5',
          },
          keyPair.publicKey,
          signature,
          data
        );
      })
      .then(function (isValid) {
        console.log('✓ RSASSA-PKCS1-v1_5 verification result:', isValid);

        // Use assert for verification
        assert.strictEqual(isValid, true, 'Signature should be valid');
        console.log('✓ RSASSA-PKCS1-v1_5 signature/verification test PASSED');

        return keyPair;
      })
      .catch(function (error) {
        console.error(
          '✗ RSASSA-PKCS1-v1_5 signature/verification test FAILED:',
          error
        );
        throw error;
      });
  }

  function testRSASSAPKCS1v15InvalidSignature(keyPair) {
    console.log('\nTesting RSASSA-PKCS1-v1_5 with invalid signature...');

    const data = new TextEncoder().encode('Original data');
    const tamperedData = new TextEncoder().encode('Tampered data');

    // Sign original data with private key
    return crypto.subtle
      .sign(
        {
          name: 'RSASSA-PKCS1-v1_5',
        },
        keyPair.privateKey,
        data
      )
      .then(function (signature) {
        console.log('✓ Original signature created');

        // Try to verify with tampered data (should fail)
        return crypto.subtle.verify(
          {
            name: 'RSASSA-PKCS1-v1_5',
          },
          keyPair.publicKey,
          signature,
          tamperedData
        );
      })
      .then(function (isValid) {
        console.log(
          '✓ RSASSA-PKCS1-v1_5 verification result for tampered data:',
          isValid
        );

        // Use assert for verification
        assert.strictEqual(
          isValid,
          false,
          'Signature should be invalid for tampered data'
        );
        console.log('✓ RSASSA-PKCS1-v1_5 invalid signature test PASSED');
      })
      .catch(function (error) {
        console.error(
          '✗ RSASSA-PKCS1-v1_5 invalid signature test FAILED:',
          error
        );
        throw error;
      });
  }

  function testRSASSAPKCS1v15WithDifferentHashes(keyPair) {
    console.log(
      '\nTesting RSASSA-PKCS1-v1_5 with different hash algorithms...'
    );

    const data = new TextEncoder().encode('Test data for different hashes');
    const hashes = ['SHA-256', 'SHA-384', 'SHA-512']; // SHA-1 is not supported for security reasons

    let promise = Promise.resolve();

    hashes.forEach(function (hash) {
      promise = promise.then(function () {
        console.log('Testing with hash:', hash);

        // Generate key pair for this hash algorithm
        return crypto.subtle
          .generateKey(
            {
              name: 'RSASSA-PKCS1-v1_5',
              modulusLength: 2048,
              publicExponent: new Uint8Array([1, 0, 1]),
              hash: hash,
            },
            true,
            ['sign', 'verify']
          )
          .then(function (hashKeyPair) {
            // Sign with this hash
            return crypto.subtle
              .sign(
                {
                  name: 'RSASSA-PKCS1-v1_5',
                },
                hashKeyPair.privateKey,
                data
              )
              .then(function (signature) {
                // Verify with the same hash
                return crypto.subtle.verify(
                  {
                    name: 'RSASSA-PKCS1-v1_5',
                  },
                  hashKeyPair.publicKey,
                  signature,
                  data
                );
              })
              .then(function (isValid) {
                console.log('✓', hash, 'signature verification:', isValid);
                assert.strictEqual(
                  isValid,
                  true,
                  'Signature should be valid for ' + hash
                );
              });
          });
      });
    });

    return promise.then(function () {
      console.log('✓ All hash algorithms test PASSED');
    });
  }

  // Run all tests
  function runTests() {
    console.log('=== RSASSA-PKCS1-v1_5 WebCrypto Tests ===\n');

    testRSASSAPKCS1v15KeyGeneration()
      .then(function (keyPair) {
        return testRSASSAPKCS1v15Signature(keyPair).then(function () {
          return keyPair;
        });
      })
      .then(function (keyPair) {
        return testRSASSAPKCS1v15InvalidSignature(keyPair).then(function () {
          return keyPair;
        });
      })
      .then(function (keyPair) {
        return testRSASSAPKCS1v15WithDifferentHashes(keyPair);
      })
      .then(function () {
        console.log('\n=== All RSASSA-PKCS1-v1_5 tests PASSED! ===');
      })
      .catch(function (error) {
        console.error('\n=== RSASSA-PKCS1-v1_5 tests FAILED ===');
        console.error('Error details:', error.message);
      });
  }

  runTests();
} // End of crypto availability check
