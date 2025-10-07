// Simple RSASSA-PKCS1-v1_5 test
const assert = require('jsrt:assert');

// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  // console.log('=== Simple RSASSA-PKCS1-v1_5 Test Completed (Skipped) ===');
} else {
  // Test just key generation first
  crypto.subtle
    .generateKey(
      {
        name: 'RSASSA-PKCS1-v1_5',
        modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-256',
      },
      true,
      ['sign', 'verify']
    )
    .then(function (keyPair) {
      // Test signature with minimal data
      const testData = new Uint8Array([0x01, 0x02, 0x03, 0x04]);

      return crypto.subtle
        .sign(
          {
            name: 'RSASSA-PKCS1-v1_5',
          },
          keyPair.privateKey,
          testData
        )
        .then(function (signature) {
          // Signature successful
        });
    })
    .catch(function (error) {
      console.error('✗ Test failed:', error.name, error.message);
      // console.log('=== Simple test FAILED ===');
    });
}
