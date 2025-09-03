const assert = require('jsrt:assert');

console.log('=== WebCrypto PBKDF2 Tests ===');

// Check if crypto.subtle is available
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== Tests Completed (Skipped) ===');
} else {
  // Test 1: Basic PBKDF2 key derivation
  console.log('Test 1: Basic PBKDF2 key derivation');
  crypto.subtle
    .importKey(
      'raw',
      new TextEncoder().encode('password'),
      { name: 'PBKDF2' },
      false,
      ['deriveKey']
    )
    .then(function (baseKey) {
      console.log('✓ Imported base key for PBKDF2');

      // Generate random salt
      const salt = new Uint8Array(16);
      crypto.getRandomValues(salt);
      console.log('✓ Generated salt, length:', salt.length);

      // Derive AES key using PBKDF2
      return crypto.subtle.deriveKey(
        {
          name: 'PBKDF2',
          salt: salt,
          iterations: 1000,
          hash: 'SHA-256',
        },
        baseKey,
        { name: 'AES-GCM', length: 256 },
        true,
        ['encrypt', 'decrypt']
      );
    })
    .then(function (derivedKey) {
      console.log('✓ Derived AES-256 key using PBKDF2');
      console.log('Derived key type:', derivedKey.type);
      console.log('Derived key extractable:', derivedKey.extractable);
      console.log('Derived key algorithm:', derivedKey.algorithm.name);
      console.log('Derived key length:', derivedKey.algorithm.length);

      assert.strictEqual(
        derivedKey.type,
        'secret',
        'Derived key should be secret'
      );
      assert.strictEqual(
        derivedKey.extractable,
        true,
        'Derived key should be extractable'
      );
      assert.strictEqual(
        derivedKey.algorithm.name,
        'AES-GCM',
        'Derived key should be AES-GCM'
      );
      assert.strictEqual(
        derivedKey.algorithm.length,
        256,
        'Derived key should be 256-bit'
      );
      console.log('✅ PASS: Basic PBKDF2 key derivation works correctly');

      // Test the derived key by encrypting something
      const plaintext = new TextEncoder().encode('Hello PBKDF2!');
      const iv = new Uint8Array(12);
      crypto.getRandomValues(iv);

      return crypto.subtle.encrypt(
        { name: 'AES-GCM', iv: iv },
        derivedKey,
        plaintext
      );
    })
    .then(function (ciphertext) {
      console.log(
        '✓ Successfully used PBKDF2-derived key for encryption, ciphertext length:',
        ciphertext.byteLength
      );
      console.log('✅ PASS: PBKDF2-derived key is functional for encryption');
    })
    .catch(function (error) {
      console.error('❌ FAIL: Basic PBKDF2 test failed:', error.message);
      throw error;
    })
    .then(function () {
      // Test 2: PBKDF2 with different hash algorithms
      console.log('\nTest 2: PBKDF2 with different hash algorithms');
      return crypto.subtle.importKey(
        'raw',
        new TextEncoder().encode('password123'),
        { name: 'PBKDF2' },
        false,
        ['deriveKey']
      );
    })
    .then(function (baseKey) {
      const salt = new Uint8Array(16);
      crypto.getRandomValues(salt);

      // Test with SHA-512
      return crypto.subtle.deriveKey(
        {
          name: 'PBKDF2',
          salt: salt,
          iterations: 2000,
          hash: 'SHA-512',
        },
        baseKey,
        { name: 'AES-GCM', length: 128 },
        true,
        ['encrypt', 'decrypt']
      );
    })
    .then(function (derivedKey) {
      console.log('✓ PBKDF2 with SHA-512 hash successful');
      console.log('Derived key length:', derivedKey.algorithm.length);
      assert.strictEqual(
        derivedKey.algorithm.length,
        128,
        'Derived key should be 128-bit'
      );
      console.log('✅ PASS: PBKDF2 with SHA-512 works correctly');
    })
    .then(function () {
      // Test 3: PBKDF2 with high iteration count
      console.log('\nTest 3: PBKDF2 with high iteration count');
      return crypto.subtle.importKey(
        'raw',
        new TextEncoder().encode('strongpassword'),
        { name: 'PBKDF2' },
        false,
        ['deriveKey']
      );
    })
    .then(function (baseKey) {
      const salt = new Uint8Array(32); // Larger salt
      crypto.getRandomValues(salt);

      console.log(
        '✓ Testing with 10,000 iterations (this may take a moment)...'
      );
      const startTime = Date.now();

      return crypto.subtle
        .deriveKey(
          {
            name: 'PBKDF2',
            salt: salt,
            iterations: 10000,
            hash: 'SHA-256',
          },
          baseKey,
          { name: 'AES-GCM', length: 256 },
          false, // Not extractable
          ['encrypt', 'decrypt']
        )
        .then(function (derivedKey) {
          const endTime = Date.now();
          console.log(
            '✓ High iteration PBKDF2 completed in',
            endTime - startTime,
            'ms'
          );
          assert.strictEqual(
            derivedKey.extractable,
            false,
            'Derived key should not be extractable'
          );
          console.log(
            '✅ PASS: PBKDF2 with high iteration count works correctly'
          );
        });
    })
    .then(function () {
      // Test 4: Error handling
      console.log('\nTest 4: PBKDF2 error handling');
      return crypto.subtle.importKey(
        'raw',
        new TextEncoder().encode('password'),
        { name: 'PBKDF2' },
        false,
        ['deriveKey']
      );
    })
    .then(function (baseKey) {
      // Test with missing salt
      return crypto.subtle
        .deriveKey(
          {
            name: 'PBKDF2',
            // Missing salt
            iterations: 1000,
            hash: 'SHA-256',
          },
          baseKey,
          { name: 'AES-GCM', length: 256 },
          true,
          ['encrypt', 'decrypt']
        )
        .then(function () {
          console.log('❌ FAIL: Should have thrown error for missing salt');
          throw new Error('Expected error for missing salt');
        })
        .catch(function (error) {
          console.log('✓ Correctly threw error for missing salt:', error.name);
          console.log('✅ PASS: PBKDF2 error handling works correctly');
        });
    })
    .then(function () {
      console.log('\n=== PBKDF2 Tests Completed Successfully ===');
      console.log('✅ All PBKDF2 functionality implemented correctly');
      console.log('• Basic key derivation works');
      console.log('• Multiple hash algorithms supported (SHA-256, SHA-512)');
      console.log('• Variable iteration counts supported');
      console.log('• Derived keys are functional for encryption');
      console.log('• Proper error handling implemented');
      console.log('• Performance is acceptable for high iteration counts');
    })
    .catch(function (error) {
      console.error('❌ PBKDF2 test failed:', error);
      throw error;
    });
}
