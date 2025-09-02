console.log('Starting basic RSA test...');

crypto.subtle.generateKey(
  {
    name: "RSA-OAEP",
    modulusLength: 2048,
    publicExponent: new Uint8Array([1, 0, 1]),
    hash: "SHA-256",
  },
  true,
  ["encrypt", "decrypt"]
).then(keyPair => {
  console.log('✅ Key generation successful');
  console.log('Public key type:', keyPair.publicKey.type);
  console.log('Private key type:', keyPair.privateKey.type);
}).catch(error => {
  console.error('❌ Key generation failed:', error);
});