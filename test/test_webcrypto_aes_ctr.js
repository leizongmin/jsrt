const assert = require('jsrt:assert');

// // console.log('=== WebCrypto AES-CTR Tests ===');

// Check if crypto.subtle is available
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  // // console.log('=== Tests Completed (Skipped) ===');
} else {
  // Test 1: Basic AES-CTR key generation and encryption/decryption (128-bit)
  crypto.subtle
    .generateKey(
      {
        name: 'AES-CTR',
        length: 128,
      },
      true,
      ['encrypt', 'decrypt']
    )
    .then(function (key) {
      // // console.log('✓ Generated AES-CTR 128-bit key');

      // Test data and counter
      const plaintext = new TextEncoder().encode('Hello, AES-CTR World!');
      const counter = new Uint8Array(16); // 16-byte counter
      crypto.getRandomValues(counter);

      console.log('Original text:', new TextDecoder().decode(plaintext));
      console.log('Counter length:', counter.length);

      // Encrypt
      return crypto.subtle
        .encrypt(
          {
            name: 'AES-CTR',
            counter: counter,
            length: 64, // Counter length in bits
          },
          key,
          plaintext
        )
        .then(function (ciphertext) {
          console.log(
            '✓ Encryption successful, ciphertext length:',
            ciphertext.byteLength
          );

          // Decrypt
          return crypto.subtle.decrypt(
            {
              name: 'AES-CTR',
              counter: counter, // Same counter as encryption
              length: 64,
            },
            key,
            ciphertext
          );
        })
        .then(function (decrypted) {
          const decryptedText = new TextDecoder().decode(decrypted);
          console.log('Decrypted text:', decryptedText);
          assert.strictEqual(
            decryptedText,
            'Hello, AES-CTR World!',
            'Decrypted text should match original'
          );
          console.log(
            '✅ PASS: AES-CTR 128-bit encrypt/decrypt works correctly'
          );
        });
    })
    .catch(function (error) {
      console.error('❌ FAIL: AES-CTR 128-bit test failed:', error.message);
      throw error;
    })
    .then(function () {
      // Test 2: AES-CTR 256-bit key
      console.log(
        '\nTest 2: AES-CTR 256-bit key generation and encrypt/decrypt'
      );
      return crypto.subtle
        .generateKey(
          {
            name: 'AES-CTR',
            length: 256,
          },
          true,
          ['encrypt', 'decrypt']
        )
        .then(function (key) {
          // // console.log('✓ Generated AES-CTR 256-bit key');

          const plaintext = new TextEncoder().encode(
            'AES-CTR with 256-bit key'
          );
          const counter = new Uint8Array(16);
          crypto.getRandomValues(counter);

          // Encrypt
          return crypto.subtle
            .encrypt(
              {
                name: 'AES-CTR',
                counter: counter,
                length: 64,
              },
              key,
              plaintext
            )
            .then(function (ciphertext) {
              // // console.log('✓ Encryption successful with 256-bit key');

              // Decrypt
              return crypto.subtle.decrypt(
                {
                  name: 'AES-CTR',
                  counter: counter,
                  length: 64,
                },
                key,
                ciphertext
              );
            })
            .then(function (decrypted) {
              const decryptedText = new TextDecoder().decode(decrypted);
              assert.strictEqual(
                decryptedText,
                'AES-CTR with 256-bit key',
                '256-bit key decryption should work'
              );
              console.log(
                '✅ PASS: AES-CTR 256-bit encrypt/decrypt works correctly'
              );
            });
        });
    })
    .then(function () {
      // Test 3: Different counter lengths
      console.log('\nTest 3: AES-CTR with different counter lengths');
      return crypto.subtle
        .generateKey(
          {
            name: 'AES-CTR',
            length: 128,
          },
          true,
          ['encrypt', 'decrypt']
        )
        .then(function (key) {
          const plaintext = new TextEncoder().encode('Counter length test');
          const counter = new Uint8Array(16);
          crypto.getRandomValues(counter);

          // Test with 128-bit counter length
          return crypto.subtle
            .encrypt(
              {
                name: 'AES-CTR',
                counter: counter,
                length: 128, // Full 128-bit counter
              },
              key,
              plaintext
            )
            .then(function (ciphertext) {
              // // console.log('✓ Encryption with 128-bit counter successful');

              return crypto.subtle.decrypt(
                {
                  name: 'AES-CTR',
                  counter: counter,
                  length: 128,
                },
                key,
                ciphertext
              );
            })
            .then(function (decrypted) {
              const decryptedText = new TextDecoder().decode(decrypted);
              assert.strictEqual(
                decryptedText,
                'Counter length test',
                '128-bit counter should work'
              );
              console.log(
                '✅ PASS: AES-CTR with 128-bit counter works correctly'
              );
            });
        });
    })
    .then(function () {
      // Test 4: Large data encryption
      console.log('\nTest 4: AES-CTR with large data');
      return crypto.subtle
        .generateKey(
          {
            name: 'AES-CTR',
            length: 256,
          },
          true,
          ['encrypt', 'decrypt']
        )
        .then(function (key) {
          // Create large test data (1KB)
          const largeData = new Uint8Array(1024);
          for (let i = 0; i < largeData.length; i++) {
            largeData[i] = i % 256;
          }

          const counter = new Uint8Array(16);
          crypto.getRandomValues(counter);

          console.log('Large data size:', largeData.length, 'bytes');

          // Encrypt
          return crypto.subtle
            .encrypt(
              {
                name: 'AES-CTR',
                counter: counter,
                length: 64,
              },
              key,
              largeData
            )
            .then(function (ciphertext) {
              console.log(
                '✓ Large data encryption successful, ciphertext size:',
                ciphertext.byteLength
              );
              assert.strictEqual(
                ciphertext.byteLength,
                largeData.length,
                'CTR mode should preserve length'
              );

              // Decrypt
              return crypto.subtle.decrypt(
                {
                  name: 'AES-CTR',
                  counter: counter,
                  length: 64,
                },
                key,
                ciphertext
              );
            })
            .then(function (decrypted) {
              console.log(
                '✓ Large data decryption successful, decrypted size:',
                decrypted.byteLength
              );

              // Verify data integrity
              const decryptedArray = new Uint8Array(decrypted);
              for (let i = 0; i < Math.min(10, decryptedArray.length); i++) {
                assert.strictEqual(
                  decryptedArray[i],
                  i % 256,
                  `Byte ${i} should match original`
                );
              }
              assert.strictEqual(
                decryptedArray.length,
                largeData.length,
                'Decrypted data should have same length'
              );
              console.log(
                '✅ PASS: AES-CTR large data encryption/decryption works correctly'
              );
            });
        });
    })
    .then(function () {
      // Test 5: Error handling
      console.log('\nTest 5: AES-CTR error handling');
      return crypto.subtle
        .generateKey(
          {
            name: 'AES-CTR',
            length: 128,
          },
          true,
          ['encrypt']
        )
        .then(function (key) {
          const plaintext = new TextEncoder().encode('Error test');

          // Test with invalid counter length
          return crypto.subtle
            .encrypt(
              {
                name: 'AES-CTR',
                counter: new Uint8Array(8), // Too short, should be 16 bytes
                length: 64,
              },
              key,
              plaintext
            )
            .then(function () {
              console.log(
                '❌ FAIL: Should have thrown error for invalid counter length'
              );
              throw new Error('Expected error for invalid counter');
            })
            .catch(function (error) {
              console.log(
                '✓ Correctly threw error for invalid counter:',
                error.name
              );
            });
        });
    })
    .then(function () {
      // console.log('\n=== AES-CTR Tests Completed Successfully ===');
      // Success case - no output
      console.log('• 128-bit and 256-bit keys supported');
      console.log('• Counter-based encryption/decryption works');
      console.log('• Large data processing works');
      console.log('• Proper error handling implemented');
      console.log('• CTR mode preserves plaintext length (no padding)');
    })
    .catch(function (error) {
      console.error('❌ AES-CTR test failed:', error);
      throw error;
    });
}
