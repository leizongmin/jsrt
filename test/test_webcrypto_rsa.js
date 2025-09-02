// Test RSA-OAEP encryption/decryption functionality

function testRSAOAEPKeyGeneration() {
  console.log('Testing RSA-OAEP key generation...');
  
  return crypto.subtle.generateKey(
    {
      name: "RSA-OAEP",
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1]), // 65537
      hash: "SHA-256",
    },
    true, // extractable
    ["encrypt", "decrypt"]
  ).then(function(keyPair) {
    console.log('✓ Key generation successful');
    console.log('Public key type:', keyPair.publicKey.type);
    console.log('Private key type:', keyPair.privateKey.type);
    console.log('Algorithm name:', keyPair.publicKey.algorithm.name);
    console.log('Modulus length:', keyPair.publicKey.algorithm.modulusLength);
    console.log('Hash algorithm:', keyPair.publicKey.algorithm.hash.name);
    
    return keyPair;
  }).catch(function(error) {
    console.error('✗ Key generation failed:', error);
    throw error;
  });
}

function testRSAOAEPEncryption(keyPair) {
  console.log('\nTesting RSA-OAEP encryption/decryption...');
  
  const plaintext = new TextEncoder().encode("Hello, RSA-OAEP!");
  
  // Encrypt with public key
  return crypto.subtle.encrypt(
    {
      name: "RSA-OAEP",
      hash: "SHA-256",
    },
    keyPair.publicKey,
    plaintext
  ).then(function(ciphertext) {
    console.log('✓ Encryption successful, ciphertext length:', ciphertext.byteLength);
    
    // Decrypt with private key
    return crypto.subtle.decrypt(
      {
        name: "RSA-OAEP",
        hash: "SHA-256",
      },
      keyPair.privateKey,
      ciphertext
    );
  }).then(function(decryptedData) {
    const decryptedText = new TextDecoder().decode(decryptedData);
    console.log('✓ Decryption successful, decrypted text:', decryptedText);
    
    if (decryptedText === "Hello, RSA-OAEP!") {
      console.log('✓ RSA-OAEP encryption/decryption test PASSED');
    } else {
      throw new Error(`Decrypted text mismatch: expected "Hello, RSA-OAEP!", got "${decryptedText}"`);
    }
  }).catch(function(error) {
    console.error('✗ RSA-OAEP encryption/decryption test FAILED:', error);
    throw error;
  });
}

function testRSAOAEPWithLabel(keyPair) {
  console.log('\nTesting RSA-OAEP with label...');
  
  const plaintext = new TextEncoder().encode("Test with label");
  const label = new TextEncoder().encode("test-label");
  
  // Encrypt with label
  return crypto.subtle.encrypt(
    {
      name: "RSA-OAEP",
      hash: "SHA-256",
      label: label,
    },
    keyPair.publicKey,
    plaintext
  ).then(function(ciphertext) {
    console.log('✓ Encryption with label successful');
    
    // Decrypt with same label
    return crypto.subtle.decrypt(
      {
        name: "RSA-OAEP",
        hash: "SHA-256",
        label: label,
      },
      keyPair.privateKey,
      ciphertext
    );
  }).then(function(decryptedData) {
    const decryptedText = new TextDecoder().decode(decryptedData);
    console.log('✓ Decryption with label successful:', decryptedText);
    
    if (decryptedText === "Test with label") {
      console.log('✓ RSA-OAEP with label test PASSED');
    } else {
      throw new Error(`Decrypted text mismatch with label`);
    }
  }).catch(function(error) {
    console.error('✗ RSA-OAEP with label test FAILED:', error);
    throw error;
  });
}

// Run all tests
function runTests() {
  console.log('=== RSA-OAEP WebCrypto Tests ===\n');
  
  testRSAOAEPKeyGeneration().then(function(keyPair) {
    return testRSAOAEPEncryption(keyPair).then(function() {
      return keyPair;
    });
  }).then(function(keyPair) {
    return testRSAOAEPWithLabel(keyPair);
  }).then(function() {
    console.log('\n=== All RSA-OAEP tests PASSED! ===');
  }).catch(function(error) {
    console.error('\n=== RSA-OAEP tests FAILED ===');
    process.exit(1);
  });
}

runTests();