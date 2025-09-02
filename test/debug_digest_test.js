console.log('=== SubtleCrypto Digest Debug Test ===');

try {
  const data = new TextEncoder().encode('hello');
  console.log('Input data:', Array.from(data));
  
  const promise = crypto.subtle.digest('SHA-256', data);
  console.log('Promise type:', typeof promise);
  console.log('Promise:', promise);
  
  // Check if it has then method
  console.log('Has then method:', typeof promise.then);
  
  // Try to resolve it
  promise.then(hash => {
    console.log('✅ Promise resolved!');
    const hashArray = new Uint8Array(hash);
    console.log('Hash array:', Array.from(hashArray));
    console.log('Hash length:', hashArray.length);
    
    // Expected SHA-256 of "hello": 2cf24dba4f21d4288dfcb1eb908e6ba18a40d8e14a03c5cdf2df5659cfd4d2cf
    const expected_start = [0x2c, 0xf2, 0x4d, 0xba];
    const actual_start = Array.from(hashArray.slice(0, 4));
    
    console.log('Expected first 4 bytes:', expected_start);
    console.log('Actual first 4 bytes:', actual_start);
    
    let matches = true;
    for (let i = 0; i < 4; i++) {
      if (actual_start[i] !== expected_start[i]) {
        matches = false;
        break;
      }
    }
    
    if (matches) {
      console.log('✅ PASS: Hash matches expected value!');
    } else {
      console.log('❌ FAIL: Hash does not match expected value');
    }
    
  }).catch(error => {
    console.error('❌ Promise rejected:');
    console.error('Error name:', error.name);
    console.error('Error message:', error.message);
    console.error('Error:', error);
  });
  
} catch (error) {
  console.error('❌ Exception thrown:');
  console.error('Error name:', error.name);  
  console.error('Error message:', error.message);
  console.error('Error:', error);
}