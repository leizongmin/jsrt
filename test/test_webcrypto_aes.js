// WebCrypto AES-CBC tests
console.log('=== Starting WebCrypto AES-CBC Tests ===');

// Check if crypto is available (skip if OpenSSL not found)
if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available (OpenSSL not found)');
  console.log('=== WebCrypto AES-CBC Tests Completed (Skipped) ===');
} else {

// Test 1: crypto.subtle.generateKey for AES-CBC
console.log('Test 1: AES key generation');
try {
  const keyPromise = crypto.subtle.generateKey(
    {
      name: "AES-CBC",
      length: 256
    },
    true,
    ["encrypt", "decrypt"]
  );
  
  console.log('Key generation returned:', typeof keyPromise);
  console.log('Has then method:', typeof keyPromise.then);
  
  keyPromise.then(key => {
    console.log('✅ PASS: AES key generated successfully');
    console.log('Key type:', key.type);
    console.log('Key extractable:', key.extractable);
    console.log('Key algorithm name:', key.algorithm.name);
    console.log('Key length:', key.algorithm.length);
    
    // Test 2: Basic AES-CBC encryption/decryption test
    console.log('Test 2: Basic AES-CBC encryption/decryption');
    
    const plaintext = new TextEncoder().encode('Hello, WebCrypto AES-CBC!');
    console.log('Plaintext:', Array.from(plaintext));
    
    // Generate a random IV
    const iv = crypto.getRandomValues(new Uint8Array(16));
    console.log('IV:', Array.from(iv));
    
    // Encrypt
    const encryptPromise = crypto.subtle.encrypt(
      {
        name: "AES-CBC",
        iv: iv
      },
      key,
      plaintext
    );
    
    encryptPromise.then(ciphertext => {
      console.log('✅ PASS: Encryption completed');
      const ciphertextArray = new Uint8Array(ciphertext);
      console.log('Ciphertext length:', ciphertextArray.length);
      console.log('Ciphertext (first 16 bytes):', Array.from(ciphertextArray.slice(0, 16)));
      
      // Decrypt
      const decryptPromise = crypto.subtle.decrypt(
        {
          name: "AES-CBC",
          iv: iv
        },
        key,
        ciphertext
      );
      
      decryptPromise.then(decryptedData => {
        console.log('✅ PASS: Decryption completed');
        const decryptedArray = new Uint8Array(decryptedData);
        console.log('Decrypted data length:', decryptedArray.length);
        console.log('Decrypted data:', Array.from(decryptedArray));
        
        // Verify the decrypted data matches the original
        const decryptedText = new TextDecoder().decode(decryptedData);
        console.log('Decrypted text:', decryptedText);
        
        if (decryptedText === 'Hello, WebCrypto AES-CBC!') {
          console.log('✅ PASS: Encrypt/decrypt round-trip successful!');
          
          // Test 3: Different key sizes
          testDifferentKeySizes();
          
        } else {
          console.log('❌ FAIL: Decrypted text does not match original');
          console.log('Expected: "Hello, WebCrypto AES-CBC!"');
          console.log('Actual:', decryptedText);
        }
      }).catch(error => {
        console.error('❌ FAIL: Decryption failed:', error.name, error.message);
      });
      
    }).catch(error => {
      console.error('❌ FAIL: Encryption failed:', error.name, error.message);
    });
    
  }).catch(error => {
    console.error('❌ FAIL: Key generation failed:', error.name, error.message);
  });
  
} catch (error) {
  console.error('❌ FAIL: Exception in key generation:', error.name, error.message);
}

function testDifferentKeySizes() {
  console.log('Test 3: Different AES key sizes');
  
  const keySizes = [128, 256]; // Test 128 and 256-bit keys
  let testIndex = 0;
  
  function testNextKeySize() {
    if (testIndex >= keySizes.length) {
      console.log('=== WebCrypto AES-CBC Tests Completed ===');
      return;
    }
    
    const keySize = keySizes[testIndex++];
    console.log(`Test 3.${testIndex}: AES-${keySize} encryption/decryption`);
    
    crypto.subtle.generateKey(
      {
        name: "AES-CBC",
        length: keySize
      },
      true,
      ["encrypt", "decrypt"]
    ).then(key => {
      const testData = new TextEncoder().encode(`Test AES-${keySize}`);
      const iv = crypto.getRandomValues(new Uint8Array(16));
      
      return crypto.subtle.encrypt({name: "AES-CBC", iv: iv}, key, testData)
        .then(ciphertext => crypto.subtle.decrypt({name: "AES-CBC", iv: iv}, key, ciphertext))
        .then(decryptedData => {
          const decryptedText = new TextDecoder().decode(decryptedData);
          if (decryptedText === `Test AES-${keySize}`) {
            console.log(`✅ PASS: AES-${keySize} works correctly`);
          } else {
            console.log(`❌ FAIL: AES-${keySize} round-trip failed`);
          }
          testNextKeySize();
        });
    }).catch(error => {
      console.error(`❌ FAIL: AES-${keySize} test failed:`, error.name, error.message);
      testNextKeySize();
    });
  }
  
  testNextKeySize();

} // End of crypto availability check
}