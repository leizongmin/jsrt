const assert = require('jsrt:assert');

// console.log('=== WebCrypto PBKDF2 Tests ===');

// Check if crypto.subtle is available
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  // console.log('=== Tests Completed (Skipped) ===');
} else {
  // Test 1: Basic PBKDF2 key derivation
  crypto.subtle
    .importKey(
      'raw',
      new TextEncoder().encode('password'),
      { name: 'PBKDF2' },
      false,
      ['deriveKey']
    )
    .then(function (baseKey) {
      // Generate random salt
      const salt = new Uint8Array(16);
      crypto.getRandomValues(salt);

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
      // Encryption successful
    })
    .catch(function (error) {
      console.error('❌ FAIL: Basic PBKDF2 test failed:', error.message);
      throw error;
    })
    .then(function () {
      // Test 2: PBKDF2 with different hash algorithms
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
      assert.strictEqual(
        derivedKey.algorithm.length,
        128,
        'Derived key should be 128-bit'
      );
    })
    .then(function () {
      // Test 3: PBKDF2 with high iteration count
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
          assert.strictEqual(
            derivedKey.extractable,
            false,
            'Derived key should not be extractable'
          );
        });
    })
    .then(function () {
      // Test 4: Error handling
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
          // Expected error - this is correct behavior
        });
    })
    .then(function () {
      // Tests completed successfully
    })
    .catch(function (error) {
      console.error('❌ PBKDF2 test failed:', error);
      throw error;
    });
}
