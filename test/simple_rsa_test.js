// Simple RSA encryption test

function testRSAEncryption() {
  console.log('Generating RSA key pair...');
  
  crypto.subtle.generateKey(
    {
      name: "RSA-OAEP",
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: "SHA-256",
    },
    true,
    ["encrypt", "decrypt"]
  ).then(function(keyPair) {
    console.log('Key pair generated successfully');
    
    console.log('Testing encryption...');
    const plaintext = new TextEncoder().encode("Hello RSA!");
    
    return crypto.subtle.encrypt(
      {
        name: "RSA-OAEP",
        hash: "SHA-256",
      },
      keyPair.publicKey,
      plaintext
    ).then(function(ciphertext) {
      console.log('Encryption successful, ciphertext length:', ciphertext.byteLength);
      
      console.log('Testing decryption...');
      return crypto.subtle.decrypt(
        {
          name: "RSA-OAEP", 
          hash: "SHA-256",
        },
        keyPair.privateKey,
        ciphertext
      );
    }).then(function(decrypted) {
      const decryptedText = new TextDecoder().decode(decrypted);
      console.log('Decryption successful:', decryptedText);
      
      if (decryptedText === "Hello RSA!") {
        console.log('✅ RSA test PASSED!');
      } else {
        console.log('❌ RSA test FAILED - text mismatch');
      }
    });
  }).catch(function(error) {
    console.error('❌ RSA test FAILED:', error);
  });
}

testRSAEncryption();