// WebCrypto AES-GCM tests
console.log('=== Starting WebCrypto AES-GCM Tests ===');

// Test 1: crypto.subtle.generateKey for AES-GCM
console.log('Test 1: AES-GCM key generation');
try {
  const keyPromise = crypto.subtle.generateKey(
    {
      name: "AES-GCM",
      length: 256
    },
    true,
    ["encrypt", "decrypt"]
  );
  
  console.log('Key generation returned:', typeof keyPromise);
  console.log('Has then method:', typeof keyPromise.then);
  
  keyPromise.then(key => {
    console.log('✅ PASS: AES-GCM key generated successfully');
    console.log('Key type:', key.type);
    console.log('Key extractable:', key.extractable);
    console.log('Key algorithm name:', key.algorithm.name);
    console.log('Key length:', key.algorithm.length);
    
    // Test 2: Basic AES-GCM encryption/decryption test
    console.log('Test 2: Basic AES-GCM encryption/decryption');
    
    const plaintext = new TextEncoder().encode('Hello, WebCrypto AES-GCM!');
    console.log('Plaintext:', Array.from(plaintext));
    
    // Generate a random IV (12 bytes is recommended for GCM)
    const iv = crypto.getRandomValues(new Uint8Array(12));
    console.log('IV:', Array.from(iv));
    
    // Encrypt
    const encryptPromise = crypto.subtle.encrypt(
      {
        name: "AES-GCM",
        iv: iv
      },
      key,
      plaintext
    );
    
    encryptPromise.then(ciphertext => {
      console.log('✅ PASS: AES-GCM encryption completed');
      const ciphertextArray = new Uint8Array(ciphertext);
      console.log('Ciphertext length:', ciphertextArray.length);
      console.log('Ciphertext includes tag (first 16 bytes):', Array.from(ciphertextArray.slice(0, 16)));
      
      // Note: AES-GCM includes authentication tag at the end
      // Expected length = plaintext length + tag length (16 bytes)
      const expectedLength = plaintext.length + 16;
      if (ciphertextArray.length === expectedLength) {
        console.log('✅ PASS: Ciphertext length correct (includes 16-byte tag)');
      } else {
        console.log('❌ FAIL: Ciphertext length incorrect. Expected:', expectedLength, 'Got:', ciphertextArray.length);
      }
      
      // Decrypt
      const decryptPromise = crypto.subtle.decrypt(
        {
          name: "AES-GCM",
          iv: iv
        },
        key,
        ciphertext
      );
      
      decryptPromise.then(decryptedData => {
        console.log('✅ PASS: AES-GCM decryption completed');
        const decryptedArray = new Uint8Array(decryptedData);
        console.log('Decrypted data length:', decryptedArray.length);
        console.log('Decrypted data:', Array.from(decryptedArray));
        
        // Verify the decrypted data matches the original
        const decryptedText = new TextDecoder().decode(decryptedData);
        console.log('Decrypted text:', decryptedText);
        
        if (decryptedText === 'Hello, WebCrypto AES-GCM!') {
          console.log('✅ PASS: AES-GCM encrypt/decrypt round-trip successful!');
          
          // Test 3: AES-GCM with additional authenticated data
          testWithAAD(key);
          
        } else {
          console.log('❌ FAIL: Decrypted text does not match original');
          console.log('Expected: "Hello, WebCrypto AES-GCM!"');
          console.log('Actual:', decryptedText);
        }
      }).catch(error => {
        console.error('❌ FAIL: AES-GCM decryption failed:', error.name, error.message);
      });
      
    }).catch(error => {
      console.error('❌ FAIL: AES-GCM encryption failed:', error.name, error.message);
    });
    
  }).catch(error => {
    console.error('❌ FAIL: AES-GCM key generation failed:', error.name, error.message);
  });
  
} catch (error) {
  console.error('❌ FAIL: Exception in AES-GCM key generation:', error.name, error.message);
}

function testWithAAD(key) {
  console.log('Test 3: AES-GCM with Additional Authenticated Data (AAD)');
  
  const plaintext = new TextEncoder().encode('Secret message');
  const aad = new TextEncoder().encode('Public header data');
  const iv = crypto.getRandomValues(new Uint8Array(12));
  
  console.log('Plaintext:', Array.from(plaintext));
  console.log('AAD:', Array.from(aad));
  console.log('IV:', Array.from(iv));
  
  // Encrypt with AAD  
  let encryptedCiphertext;
  
  crypto.subtle.encrypt(
    {
      name: "AES-GCM",
      iv: iv,
      additionalData: aad
    },
    key,
    plaintext
  ).then(ciphertext => {
    console.log('✅ PASS: AES-GCM encryption with AAD completed');
    encryptedCiphertext = ciphertext;
    const ciphertextArray = new Uint8Array(ciphertext);
    console.log('Ciphertext with AAD length:', ciphertextArray.length);
    
    // Decrypt with same AAD
    return crypto.subtle.decrypt(
      {
        name: "AES-GCM",
        iv: iv,
        additionalData: aad
      },
      key,
      ciphertext
    );
  }).then(decryptedData => {
    console.log('✅ PASS: AES-GCM decryption with AAD completed');
    const decryptedText = new TextDecoder().decode(decryptedData);
    
    if (decryptedText === 'Secret message') {
      console.log('✅ PASS: AES-GCM with AAD round-trip successful!');
      
      // Test 4: Try decrypting with wrong AAD (should fail)
      testWrongAAD(key, encryptedCiphertext, iv, aad);
      
    } else {
      console.log('❌ FAIL: AES-GCM with AAD decryption incorrect');
      console.log('Expected: "Secret message"');
      console.log('Actual:', decryptedText);
    }
  }).catch(error => {
    console.error('❌ FAIL: AES-GCM with AAD failed:', error.name, error.message);
  });
}

function testWrongAAD(key, ciphertext, iv, originalAAD) {
  console.log('Test 4: AES-GCM authentication test (wrong AAD should fail)');
  
  const wrongAAD = new TextEncoder().encode('Wrong header data');
  
  // Try to decrypt with wrong AAD - this should fail authentication
  crypto.subtle.decrypt(
    {
      name: "AES-GCM",
      iv: iv,
      additionalData: wrongAAD
    },
    key,
    ciphertext
  ).then(decryptedData => {
    console.log('❌ FAIL: Decryption with wrong AAD should have failed but succeeded');
  }).catch(error => {
    console.log('✅ PASS: Decryption with wrong AAD failed as expected (authentication works!)');
    console.log('Error:', error.name, error.message);
    
    // Test 5: Different key sizes
    testDifferentKeySizes();
  });
}

function testDifferentKeySizes() {
  console.log('Test 5: Different AES-GCM key sizes');
  
  const keySizes = [128, 256]; // Test 128 and 256-bit keys
  let testIndex = 0;
  
  function testNextKeySize() {
    if (testIndex >= keySizes.length) {
      console.log('=== WebCrypto AES-GCM Tests Completed ===');
      return;
    }
    
    const keySize = keySizes[testIndex++];
    console.log(`Test 5.${testIndex}: AES-GCM-${keySize} encryption/decryption`);
    
    crypto.subtle.generateKey(
      {
        name: "AES-GCM",
        length: keySize
      },
      true,
      ["encrypt", "decrypt"]
    ).then(key => {
      const testData = new TextEncoder().encode(`Test AES-GCM-${keySize}`);
      const iv = crypto.getRandomValues(new Uint8Array(12));
      
      return crypto.subtle.encrypt({name: "AES-GCM", iv: iv}, key, testData)
        .then(ciphertext => crypto.subtle.decrypt({name: "AES-GCM", iv: iv}, key, ciphertext))
        .then(decryptedData => {
          const decryptedText = new TextDecoder().decode(decryptedData);
          if (decryptedText === `Test AES-GCM-${keySize}`) {
            console.log(`✅ PASS: AES-GCM-${keySize} works correctly`);
          } else {
            console.log(`❌ FAIL: AES-GCM-${keySize} round-trip failed`);
          }
          testNextKeySize();
        });
    }).catch(error => {
      console.error(`❌ FAIL: AES-GCM-${keySize} test failed:`, error.name, error.message);
      testNextKeySize();
    });
  }
  
  testNextKeySize();
}