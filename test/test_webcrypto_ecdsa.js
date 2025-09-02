// WebCrypto ECDSA signature/verification test
const assert = require("std:assert");

// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== ECDSA Tests Completed (Skipped) ===');
} else {

console.log('=== WebCrypto ECDSA Signature/Verification Tests ===');

// Test 1: ECDSA key generation with P-256 curve
function testECDSAKeyGenerationP256() {
  console.log('\n1. Testing ECDSA key generation with P-256 curve...');
  
  return crypto.subtle.generateKey(
    {
      name: "ECDSA",
      namedCurve: "P-256"
    },
    true,
    ["sign", "verify"]
  ).then(function(keyPair) {
    console.log('✅ ECDSA P-256 key pair generated successfully');
    
    // Verify key properties
    assert.strictEqual(keyPair.publicKey.type, 'public', 'Public key type should be "public"');
    assert.strictEqual(keyPair.privateKey.type, 'private', 'Private key type should be "private"');
    assert.strictEqual(keyPair.publicKey.algorithm.name, 'ECDSA', 'Algorithm name should be ECDSA');
    assert.strictEqual(keyPair.publicKey.algorithm.namedCurve, 'P-256', 'Named curve should be P-256');
    
    return keyPair;
  }).catch(function(error) {
    console.error('❌ ECDSA key generation failed:', error.message);
    throw error;
  });
}

// Test 2: ECDSA signature and verification
function testECDSASignVerify(keyPair) {
  console.log('\n2. Testing ECDSA signature and verification...');
  
  const data = new TextEncoder().encode("Test message for ECDSA signature");
  
  return crypto.subtle.sign(
    {
      name: "ECDSA",
      hash: "SHA-256"
    },
    keyPair.privateKey,
    data
  ).then(function(signature) {
    console.log('✅ ECDSA signature created, length:', signature.byteLength);
    
    // Verify the signature
    return crypto.subtle.verify(
      {
        name: "ECDSA",
        hash: "SHA-256"
      },
      keyPair.publicKey,
      signature,
      data
    ).then(function(isValid) {
      assert.strictEqual(isValid, true, 'Signature should be valid');
      console.log('✅ ECDSA signature verified successfully');
      return { keyPair, signature, data };
    });
  }).catch(function(error) {
    console.error('❌ ECDSA sign/verify failed:', error.message);
    throw error;
  });
}

// Test 3: ECDSA verification with tampered data
function testECDSAInvalidSignature(testData) {
  console.log('\n3. Testing ECDSA verification with tampered data...');
  
  const tamperedData = new TextEncoder().encode("Tampered message");
  
  return crypto.subtle.verify(
    {
      name: "ECDSA",
      hash: "SHA-256"
    },
    testData.keyPair.publicKey,
    testData.signature,
    tamperedData
  ).then(function(isValid) {
    assert.strictEqual(isValid, false, 'Signature should be invalid for tampered data');
    console.log('✅ ECDSA correctly rejected tampered data');
  }).catch(function(error) {
    console.error('❌ ECDSA invalid signature test failed:', error.message);
    throw error;
  });
}

// Test 4: ECDSA with different curves
function testECDSADifferentCurves() {
  console.log('\n4. Testing ECDSA with different curves...');
  
  const curves = ["P-256", "P-384", "P-521"];
  const testPromises = [];
  
  curves.forEach(function(curve) {
    const promise = crypto.subtle.generateKey(
      {
        name: "ECDSA",
        namedCurve: curve
      },
      true,
      ["sign", "verify"]
    ).then(function(keyPair) {
      console.log('✅ Generated ECDSA key pair with curve:', curve);
      
      // Test signing with the generated key
      const testData = new TextEncoder().encode("Test data for " + curve);
      return crypto.subtle.sign(
        {
          name: "ECDSA",
          hash: "SHA-256"
        },
        keyPair.privateKey,
        testData
      ).then(function(signature) {
        return crypto.subtle.verify(
          {
            name: "ECDSA",
            hash: "SHA-256"
          },
          keyPair.publicKey,
          signature,
          testData
        );
      }).then(function(isValid) {
        assert.strictEqual(isValid, true, 'Signature should be valid for ' + curve);
        console.log('✅ ECDSA sign/verify successful with curve:', curve);
      });
    }).catch(function(error) {
      console.error('❌ ECDSA test failed for curve', curve + ':', error.message);
      throw error;
    });
    
    testPromises.push(promise);
  });
  
  return Promise.all(testPromises);
}

// Test 5: ECDSA with different hash algorithms
function testECDSADifferentHashes() {
  console.log('\n5. Testing ECDSA with different hash algorithms...');
  
  return crypto.subtle.generateKey(
    {
      name: "ECDSA",
      namedCurve: "P-256"
    },
    true,
    ["sign", "verify"]
  ).then(function(keyPair) {
    const hashes = ["SHA-256", "SHA-384", "SHA-512"];
    const testPromises = [];
    
    hashes.forEach(function(hash) {
      const promise = (function() {
        const testData = new TextEncoder().encode("Test data for " + hash);
        return crypto.subtle.sign(
          {
            name: "ECDSA",
            hash: hash
          },
          keyPair.privateKey,
          testData
        ).then(function(signature) {
          return crypto.subtle.verify(
            {
              name: "ECDSA",
              hash: hash
            },
            keyPair.publicKey,
            signature,
            testData
          );
        }).then(function(isValid) {
          assert.strictEqual(isValid, true, 'Signature should be valid with ' + hash);
          console.log('✅ ECDSA sign/verify successful with hash:', hash);
        });
      })();
      
      testPromises.push(promise);
    });
    
    return Promise.all(testPromises);
  }).catch(function(error) {
    console.error('❌ ECDSA hash test failed:', error.message);
    throw error;
  });
}

// Run all tests
testECDSAKeyGenerationP256()
  .then(testECDSASignVerify)
  .then(testECDSAInvalidSignature)
  .then(testECDSADifferentCurves)
  .then(testECDSADifferentHashes)
  .then(function() {
    console.log('\n=== All ECDSA Tests Passed! ===');
  })
  .catch(function(error) {
    console.error('\n=== ECDSA Tests Failed ===');
    console.error('Error:', error.message);
    if (error.stack) {
      console.error('Stack:', error.stack);
    }
  });

} // End of crypto availability check