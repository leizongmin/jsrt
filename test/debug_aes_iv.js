console.log('=== AES IV Debug Test ===');

try {
  const iv = crypto.getRandomValues(new Uint8Array(16));
  console.log('IV type:', typeof iv);
  console.log('IV constructor:', iv.constructor.name);
  console.log('IV buffer:', typeof iv.buffer);
  console.log('IV byteOffset:', iv.byteOffset);
  console.log('IV byteLength:', iv.byteLength);
  console.log('IV data:', Array.from(iv));

  // Test the algorithm object
  const algorithm = {
    name: 'AES-CBC',
    iv: iv,
  };

  console.log('Algorithm IV:', typeof algorithm.iv);
  console.log('Algorithm IV constructor:', algorithm.iv.constructor.name);
} catch (error) {
  console.error('Error:', error);
}
