// WebCrypto HMAC tests
console.log('=== Starting WebCrypto HMAC Tests ===');

// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== WebCrypto HMAC Tests Completed (Skipped) ===');
} else {

// Test 1: crypto.subtle.generateKey for HMAC
console.log('Test 1: HMAC key generation');
try {
  const keyPromise = crypto.subtle.generateKey(
    {
      name: "HMAC",
      hash: "SHA-256"
    },
    true,
    ["sign", "verify"]
  );
  
  console.log('Key generation returned:', typeof keyPromise);
  console.log('Has then method:', typeof keyPromise.then);
  
  keyPromise.then(key => {
    console.log('✅ PASS: HMAC key generated successfully');
    console.log('Key type:', key.type);
    console.log('Key extractable:', key.extractable);
    console.log('Key algorithm name:', key.algorithm.name);
    console.log('Key algorithm hash:', key.algorithm.hash);
    
    // Test 2: Basic HMAC sign/verify test
    console.log('Test 2: Basic HMAC sign/verify');
    
    const data = new TextEncoder().encode('Hello, WebCrypto HMAC!');
    console.log('Data:', Array.from(data));
    
    // Sign the data
    const signPromise = crypto.subtle.sign(
      "HMAC",
      key,
      data
    );
    
    signPromise.then(signature => {
      console.log('✅ PASS: HMAC signing completed');
      const signatureArray = new Uint8Array(signature);
      console.log('Signature length:', signatureArray.length);
      console.log('Signature (first 8 bytes):', Array.from(signatureArray.slice(0, 8)));
      
      // HMAC-SHA-256 should produce 32-byte signature
      if (signatureArray.length === 32) {
        console.log('✅ PASS: Signature length correct (32 bytes for HMAC-SHA-256)');
      } else {
        console.log('❌ FAIL: Signature length incorrect. Expected: 32, Got:', signatureArray.length);
      }
      
      // Verify the signature
      const verifyPromise = crypto.subtle.verify(
        "HMAC",
        key,
        signature,
        data
      );
      
      verifyPromise.then(isValid => {
        console.log('✅ PASS: HMAC verification completed');
        console.log('Verification result:', isValid);
        
        if (isValid === true) {
          console.log('✅ PASS: HMAC sign/verify round-trip successful!');
          
          // Test 3: Verify with wrong data (should fail)
          testWrongData(key, signature);
          
        } else {
          console.log('❌ FAIL: Verification failed for valid signature');
        }
      }).catch(error => {
        console.error('❌ FAIL: HMAC verification failed:', error.name, error.message);
      });
      
    }).catch(error => {
      console.error('❌ FAIL: HMAC signing failed:', error.name, error.message);
    });
    
  }).catch(error => {
    console.error('❌ FAIL: HMAC key generation failed:', error.name, error.message);
  });
  
} catch (error) {
  console.error('❌ FAIL: Exception in HMAC key generation:', error.name, error.message);
}

function testWrongData(key, originalSignature) {
  console.log('Test 3: HMAC authentication test (wrong data should fail)');
  
  const wrongData = new TextEncoder().encode('Wrong data');
  
  // Try to verify original signature with wrong data - this should fail
  crypto.subtle.verify(
    "HMAC",
    key,
    originalSignature,
    wrongData
  ).then(isValid => {
    if (isValid === false) {
      console.log('✅ PASS: Verification with wrong data failed as expected (authentication works!)');
      
      // Test 4: Different hash algorithms
      testDifferentHashAlgorithms();
    } else {
      console.log('❌ FAIL: Verification with wrong data should have failed but succeeded');
    }
  }).catch(error => {
    console.error('❌ FAIL: Verification with wrong data threw error:', error.name, error.message);
  });
}

function testDifferentHashAlgorithms() {
  console.log('Test 4: Different HMAC hash algorithms');
  
  const hashAlgorithms = ['SHA-1', 'SHA-256', 'SHA-384', 'SHA-512'];
  const expectedSizes = {
    'SHA-1': 20,    // 160 bits
    'SHA-256': 32,  // 256 bits 
    'SHA-384': 48,  // 384 bits
    'SHA-512': 64   // 512 bits
  };
  
  let testIndex = 0;
  
  function testNextHashAlgorithm() {
    if (testIndex >= hashAlgorithms.length) {
      console.log('=== WebCrypto HMAC Tests Completed ===');
      return;
    }
    
    const hashAlg = hashAlgorithms[testIndex++];
    const expectedSize = expectedSizes[hashAlg];
    console.log(`Test 4.${testIndex}: HMAC-${hashAlg} sign/verify`);
    
    crypto.subtle.generateKey(
      {
        name: "HMAC",
        hash: hashAlg
      },
      true,
      ["sign", "verify"]
    ).then(key => {
      const testData = new TextEncoder().encode(`Test HMAC-${hashAlg}`);
      
      return crypto.subtle.sign("HMAC", key, testData)
        .then(signature => {
          const signatureArray = new Uint8Array(signature);
          
          if (signatureArray.length === expectedSize) {
            console.log(`✅ PASS: HMAC-${hashAlg} signature size correct (${expectedSize} bytes)`);
            
            // Verify the signature
            return crypto.subtle.verify("HMAC", key, signature, testData);
          } else {
            console.log(`❌ FAIL: HMAC-${hashAlg} signature size incorrect. Expected: ${expectedSize}, Got: ${signatureArray.length}`);
            return Promise.resolve(false);
          }
        })
        .then(isValid => {
          if (isValid) {
            console.log(`✅ PASS: HMAC-${hashAlg} works correctly`);
          } else {
            console.log(`❌ FAIL: HMAC-${hashAlg} verification failed`);
          }
          testNextHashAlgorithm();
        });
    }).catch(error => {
      console.error(`❌ FAIL: HMAC-${hashAlg} test failed:`, error.name, error.message);
      testNextHashAlgorithm();
    });
  }
  
  testNextHashAlgorithm();

} // End of crypto availability check
}