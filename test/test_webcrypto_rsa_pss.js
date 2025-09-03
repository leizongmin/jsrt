// WebCrypto RSA-PSS signature/verification test
const assert = require('std:assert');

// Main test function
(async function () {
  // Check if crypto is available (skip if OpenSSL not found)
  if (typeof crypto === 'undefined' || !crypto.subtle) {
    console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
    console.log('=== Tests Completed (Skipped) ===');
    return;
  }

  console.log('=== WebCrypto RSA-PSS Signature/Verification Tests ===');

  // For now, RSA-PSS is not fully supported due to OpenSSL 3.x compatibility
  // Just test that it's correctly marked as unsupported
  console.log('\n⚠️ RSA-PSS is not yet fully implemented');
  console.log(
    'RSA-PSS key generation works but signing/verification needs OpenSSL 3.x API updates'
  );

  // Test that RSA-PSS is correctly marked as not supported
  try {
    const keyPair = await crypto.subtle.generateKey(
      {
        name: 'RSA-PSS',
        modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-256',
      },
      true,
      ['sign', 'verify']
    );

    console.log('✅ RSA-PSS key generation works');

    // Try to sign - should fail with NotSupportedError
    try {
      const testMessage = new TextEncoder().encode('Test message');
      await crypto.subtle.sign(
        {
          name: 'RSA-PSS',
          saltLength: 32,
        },
        keyPair.privateKey,
        testMessage
      );

      console.log('❌ RSA-PSS sign should have failed but succeeded');
    } catch (error) {
      if (error.name === 'NotSupportedError') {
        console.log('✅ RSA-PSS sign correctly returns NotSupportedError');
      } else {
        console.log('⚠️ RSA-PSS sign failed with:', error.name, error.message);
      }
    }
  } catch (error) {
    console.error('Test error:', error);
  }

  console.log('\n=== RSA-PSS Tests Completed (Partial Implementation) ===');
})();
