// WebCrypto API tests for WinterCG compliance
const assert = require('std:assert');
console.log('=== Starting WebCrypto API Tests ===');

// Test 1: crypto object existence
console.log('Test 1: crypto object existence');
if (typeof crypto === 'undefined') {
  console.log('❌ SKIP: crypto object not available (OpenSSL not found)');
  console.log('=== WebCrypto API Tests Completed (Skipped) ===');
} else {
  console.log('✅ PASS: crypto object is available');
  assert.notStrictEqual(
    typeof crypto,
    'undefined',
    'crypto object should be available'
  );

  // Test 2: crypto.getRandomValues basic functionality
  console.log('Test 2: crypto.getRandomValues basic functionality');
  assert.strictEqual(
    typeof crypto.getRandomValues,
    'function',
    'crypto.getRandomValues should be a function'
  );
  console.log('✅ PASS: crypto.getRandomValues is a function');

  // Test with Uint8Array
  const uint8Array = new Uint8Array(16);
  const result = crypto.getRandomValues(uint8Array);

  assert.strictEqual(
    result,
    uint8Array,
    'crypto.getRandomValues should return the same array'
  );
  console.log('✅ PASS: crypto.getRandomValues returns the same array');
  // Check that at least some bytes were modified (very unlikely all remain zero)
  let allZeros = true;
  for (let i = 0; i < uint8Array.length; i++) {
    if (uint8Array[i] !== 0) {
      allZeros = false;
      break;
    }
  }
  assert.strictEqual(allZeros, false, 'Random values should not be all zero');
  console.log('✅ PASS: Random values are not all zero');

  // Test 3: crypto.randomUUID basic functionality
  console.log('Test 3: crypto.randomUUID basic functionality');
  assert.strictEqual(
    typeof crypto.randomUUID,
    'function',
    'crypto.randomUUID should be a function'
  );
  console.log('✅ PASS: crypto.randomUUID is a function');

  const uuid1 = crypto.randomUUID();
  assert.strictEqual(
    typeof uuid1,
    'string',
    'crypto.randomUUID should return a string'
  );
  console.log('✅ PASS: crypto.randomUUID returns a string');

  assert.strictEqual(uuid1.length, 36, 'UUID should be 36 characters long');
  console.log('✅ PASS: UUID is 36 characters long');

  // Test UUID format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
  const uuidPattern =
    /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
  assert.strictEqual(
    uuidPattern.test(uuid1),
    true,
    'UUID should match RFC 4122 v4 format'
  );
  console.log('✅ PASS: UUID matches RFC 4122 v4 format');

  console.log('Generated UUID:', uuid1);

  console.log('=== WebCrypto API Tests Completed ===');
}
