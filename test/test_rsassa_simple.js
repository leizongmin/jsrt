// Simple RSASSA-PKCS1-v1_5 test
const assert = require('jsrt:assert');

console.log('=== Simple RSASSA-PKCS1-v1_5 Test ===');

// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== Simple RSASSA-PKCS1-v1_5 Test Completed (Skipped) ===');
} else {
  // Test just key generation first
  console.log('Testing key generation...');
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
      console.log('✓ Key generation successful');
      console.log('Public key algorithm:', keyPair.publicKey.algorithm.name);
      console.log('Private key algorithm:', keyPair.privateKey.algorithm.name);

      // Test signature with minimal data
      const testData = new Uint8Array([0x01, 0x02, 0x03, 0x04]);
      console.log('Testing signature with simple data...');

      return crypto.subtle
        .sign(
          {
            name: 'RSASSA-PKCS1-v1_5',
          },
          keyPair.privateKey,
          testData
        )
        .then(function (signature) {
          console.log('✓ Signature successful, length:', signature.byteLength);
          console.log('=== Simple test PASSED ===');
        });
    })
    .catch(function (error) {
      console.error('✗ Test failed:', error.name, error.message);
      console.log('=== Simple test FAILED ===');
    });
}
