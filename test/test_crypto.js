// WebCrypto API tests for WinterCG compliance
console.log('=== Starting WebCrypto API Tests ===');

// Test 1: crypto object existence
console.log('Test 1: crypto object existence');
if (typeof crypto === 'undefined') {
  console.log('❌ SKIP: crypto object not available (OpenSSL not found)');
  console.log('=== WebCrypto API Tests Completed (Skipped) ===');
} else {
  console.log('✅ PASS: crypto object is available');

  // Test 2: crypto.getRandomValues basic functionality
  console.log('Test 2: crypto.getRandomValues basic functionality');
  if (typeof crypto.getRandomValues === 'function') {
    console.log('✅ PASS: crypto.getRandomValues is a function');
    
    // Test with Uint8Array
    const uint8Array = new Uint8Array(16);
    const result = crypto.getRandomValues(uint8Array);
    
    if (result === uint8Array) {
      console.log('✅ PASS: crypto.getRandomValues returns the same array');
    } else {
      console.log('❌ FAIL: crypto.getRandomValues should return the same array');
    }
    
    // Check that at least some bytes were modified (very unlikely all remain zero)
    let allZeros = true;
    for (let i = 0; i < uint8Array.length; i++) {
      if (uint8Array[i] !== 0) {
        allZeros = false;
        break;
      }
    }
    if (!allZeros) {
      console.log('✅ PASS: Random values are not all zero');
    } else {
      console.log('❌ FAIL: All random values are zero (very unlikely)');
    }
  } else {
    console.log('❌ FAIL: crypto.getRandomValues should be a function');
  }

  // Test 3: crypto.randomUUID basic functionality
  console.log('Test 3: crypto.randomUUID basic functionality');
  if (typeof crypto.randomUUID === 'function') {
    console.log('✅ PASS: crypto.randomUUID is a function');
    
    const uuid1 = crypto.randomUUID();
    if (typeof uuid1 === 'string') {
      console.log('✅ PASS: crypto.randomUUID returns a string');
    } else {
      console.log('❌ FAIL: crypto.randomUUID should return a string');
    }
    
    if (uuid1.length === 36) {
      console.log('✅ PASS: UUID is 36 characters long');
    } else {
      console.log('❌ FAIL: UUID should be 36 characters long');
    }
    
    // Test UUID format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    const uuidPattern = /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
    if (uuidPattern.test(uuid1)) {
      console.log('✅ PASS: UUID matches RFC 4122 v4 format');
    } else {
      console.log('❌ FAIL: UUID does not match RFC 4122 v4 format');
    }
    
    console.log('Generated UUID:', uuid1);
  } else {
    console.log('❌ FAIL: crypto.randomUUID should be a function');
  }

  console.log('=== WebCrypto API Tests Completed ===');
}