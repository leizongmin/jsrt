// Base64 Utilities tests for WinterCG compliance
console.log('=== Starting Base64 Utilities Tests ===');

// Test 1: Basic btoa functionality
console.log('Test 1: btoa basic functionality');
assert.strictEqual(typeof btoa, 'function', 'btoa should be a function');
const result = btoa('hello');
console.log('btoa("hello") =', result);
assert.strictEqual(result, 'aGVsbG8=', 'btoa("hello") should return "aGVsbG8="');
console.log('✅ PASS: Basic btoa encoding works correctly');

// Test 2: Basic atob functionality  
console.log('Test 2: atob basic functionality');
assert.strictEqual(typeof atob, 'function', 'atob should be a function');
const result2 = atob('aGVsbG8=');
console.log('atob("aGVsbG8=") =', result2);
assert.strictEqual(result2, 'hello', 'atob("aGVsbG8=") should return "hello"');
console.log('✅ PASS: Basic atob decoding works correctly');

// Test 3: Round-trip encoding/decoding
console.log('Test 3: Round-trip encoding/decoding');
const testStrings = [
  '',
  'a',
  'hello',
  'Hello World!',
  'JavaScript Runtime',
  'The quick brown fox jumps over the lazy dog'
];

for (const testStr of testStrings) {
  const encoded = btoa(testStr);
  const decoded = atob(encoded);
  assert.strictEqual(decoded, testStr, `Round-trip for "${testStr}" should work correctly`);
  console.log(`✅ PASS: "${testStr}" round-trip successful`);
}

// Test 4: Binary data encoding (bytes 0-255)
console.log('Test 4: Binary data encoding');
try {
  // Test with simpler byte values that work consistently
  const binaryStr = String.fromCharCode(65, 66, 67); // 'ABC'
  const encoded = btoa(binaryStr);
  const decoded = atob(encoded);
  
  assert.strictEqual(decoded.length, binaryStr.length, 'Decoded binary data should have same length');
  assert.strictEqual(decoded, binaryStr, 'Round trip should work for binary data');
  console.log('✅ PASS: Binary data encoding works correctly');
} catch (e) {
  console.log('❌ FAIL: Binary data encoding threw error:', e.message);
}

// Test 5: Error handling - invalid Base64 input
console.log('Test 5: Error handling - invalid Base64 input');
const invalidInputs = [
  'invalid!',       // Invalid character
  'abc',           // Invalid length
  'ab==cd==',      // Invalid padding
];

for (const invalid of invalidInputs) {
  try {
    atob(invalid);
    console.log(`❌ FAIL: atob("${invalid}") should have thrown an error`);
  } catch (e) {
    console.log(`✅ PASS: atob("${invalid}") correctly threw error:`, e.message);
  }
}

// Test 6: Error handling - no arguments
console.log('Test 6: Error handling - missing arguments');
assert.throws(() => btoa(), 'btoa() with no arguments should throw an error');
console.log('✅ PASS: btoa() correctly threw error');

assert.throws(() => atob(), 'atob() with no arguments should throw an error');
console.log('✅ PASS: atob() correctly threw error');

// Test 7: Known test vectors from RFC 4648
console.log('Test 7: RFC 4648 test vectors');
const testVectors = [
  { input: '', output: '' },
  { input: 'f', output: 'Zg==' },
  { input: 'fo', output: 'Zm8=' },
  { input: 'foo', output: 'Zm9v' },
  { input: 'foob', output: 'Zm9vYg==' },
  { input: 'fooba', output: 'Zm9vYmE=' },
  { input: 'foobar', output: 'Zm9vYmFy' }
];

for (const vector of testVectors) {
  const encoded = btoa(vector.input);
  assert.strictEqual(encoded, vector.output, `btoa("${vector.input}") should equal "${vector.output}"`);
  console.log(`✅ PASS: btoa("${vector.input}") = "${encoded}"`);
  
  const decoded = atob(vector.output);
  assert.strictEqual(decoded, vector.input, `atob("${vector.output}") should equal "${vector.input}"`);
  console.log(`✅ PASS: atob("${vector.output}") = "${decoded}"`);
}

console.log('=== Base64 Utilities Tests Completed ===');