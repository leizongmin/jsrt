console.log('=== RSA-PSS Debug Test ===');

if (typeof crypto === 'undefined' || !crypto.subtle) {
  console.log('❌ SKIP: WebCrypto not available');
} else {
  console.log('✓ WebCrypto available');
  
  crypto.subtle.generateKey(
    {
      name: 'RSA-PSS',
      modulusLength: 2048,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256',
    },
    true,
    ['sign', 'verify']
  ).then(keyPair => {
    console.log('✓ RSA-PSS key generation successful');
    console.log('Public key type:', keyPair.publicKey.type);
    console.log('Private key type:', keyPair.privateKey.type);
    
    const data = new TextEncoder().encode('Hello RSA-PSS!');
    console.log('Data to sign:', new TextDecoder().decode(data));
    
    return crypto.subtle.sign(
      {
        name: 'RSA-PSS'
      },
      keyPair.privateKey,
      data
    ).then(signature => {
      console.log('✓ Signing successful, signature length:', signature.byteLength);
      
      return crypto.subtle.verify(
        {
          name: 'RSA-PSS'
        },
        keyPair.publicKey,
        signature,
        data
      );
    }).then(isValid => {
      console.log('✓ Verification result:', isValid);
      console.log('✅ RSA-PSS test PASSED');
    });
  }).catch(error => {
    console.error('❌ Test failed:', error.name, '-', error.message);
    console.error('Stack:', error.stack);
  });
}