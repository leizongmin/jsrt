// Test AES key generation to verify crypto system is working

console.log('Testing AES key generation...');

crypto.subtle.generateKey(
  {
    name: "AES-GCM",
    length: 256,
  },
  true,
  ["encrypt", "decrypt"]
).then(key => {
  console.log('✓ AES key generation successful!');
  console.log('Key type:', key.type);
  console.log('Key extractable:', key.extractable);
}).catch(error => {
  console.error('✗ AES key generation failed:', error);
});