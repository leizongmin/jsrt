// Test RSA-PKCS1-v1_5 encryption/decryption functionality
const assert = require('std:assert');

// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== RSA-PKCS1-v1_5 Tests Completed (Skipped) ===');
} else {
  function testRSAPKCS1v15KeyGeneration() {
    console.log('Testing RSA-PKCS1-v1_5 key generation...');

    return crypto.subtle
      .generateKey(
        {
          name: 'RSA-PKCS1-v1_5',
          modulusLength: 2048,
          publicExponent: new Uint8Array([1, 0, 1]), // 65537
          hash: 'SHA-256',
        },
        true, // extractable
        ['encrypt', 'decrypt']
      )
      .then(function (keyPair) {
        console.log('✓ RSA-PKCS1-v1_5 key generation successful');

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
          'RSA-PKCS1-v1_5',
          'Algorithm name should be RSA-PKCS1-v1_5'
        );
        assert.strictEqual(
          keyPair.publicKey.algorithm.modulusLength,
          2048,
          'Modulus length should be 2048'
        );

        console.log('Public key type:', keyPair.publicKey.type);
        console.log('Private key type:', keyPair.privateKey.type);
        console.log('Algorithm name:', keyPair.publicKey.algorithm.name);
        console.log(
          'Modulus length:',
          keyPair.publicKey.algorithm.modulusLength
        );

        return keyPair;
      })
      .catch(function (error) {
        console.error('✗ RSA-PKCS1-v1_5 key generation failed:', error);
        throw error;
      });
  }

  function testRSAPKCS1v15Encryption(keyPair) {
    console.log('\nTesting RSA-PKCS1-v1_5 encryption/decryption...');

    const plaintext = new TextEncoder().encode('Hello, RSA-PKCS1-v1_5!');

    // Encrypt with public key
    return crypto.subtle
      .encrypt(
        {
          name: 'RSA-PKCS1-v1_5',
        },
        keyPair.publicKey,
        plaintext
      )
      .then(function (ciphertext) {
        console.log(
          '✓ RSA-PKCS1-v1_5 encryption successful, ciphertext length:',
          ciphertext.byteLength
        );

        // Decrypt with private key
        return crypto.subtle.decrypt(
          {
            name: 'RSA-PKCS1-v1_5',
          },
          keyPair.privateKey,
          ciphertext
        );
      })
      .then(function (decryptedData) {
        const decryptedText = new TextDecoder().decode(decryptedData);
        console.log(
          '✓ RSA-PKCS1-v1_5 decryption successful, decrypted text:',
          decryptedText
        );

        // Use assert for verification
        assert.strictEqual(
          decryptedText,
          'Hello, RSA-PKCS1-v1_5!',
          'Decrypted text should match original plaintext'
        );
        console.log('✓ RSA-PKCS1-v1_5 encryption/decryption test PASSED');
      })
      .catch(function (error) {
        console.error(
          '✗ RSA-PKCS1-v1_5 encryption/decryption test FAILED:',
          error
        );
        throw error;
      });
  }

  // Run all tests
  function runTests() {
    console.log('=== RSA-PKCS1-v1_5 WebCrypto Tests ===\n');

    testRSAPKCS1v15KeyGeneration()
      .then(function (keyPair) {
        return testRSAPKCS1v15Encryption(keyPair);
      })
      .then(function () {
        console.log('\n=== All RSA-PKCS1-v1_5 tests PASSED! ===');
      })
      .catch(function (error) {
        console.error('\n=== RSA-PKCS1-v1_5 tests FAILED ===');
        console.error('Error details:', error.message);
      });
  }

  runTests();
} // End of crypto availability check
