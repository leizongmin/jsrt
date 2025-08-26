// Base64 Utilities tests for WinterCG compliance
console.log('=== Starting Base64 Utilities Tests ===');

// Test 1: Basic btoa functionality
console.log('Test 1: btoa basic functionality');
if (typeof btoa === 'function') {
  const result = btoa('hello');
  console.log('btoa("hello") =', result);
  if (result === 'aGVsbG8=') {
    console.log('✅ PASS: Basic btoa encoding works correctly');
  } else {
    console.log('❌ FAIL: Expected "aGVsbG8=", got', result);
  }
} else {
  console.log('❌ FAIL: btoa is not implemented');
}

// Test 2: Basic atob functionality  
console.log('Test 2: atob basic functionality');
if (typeof atob === 'function') {
  const result = atob('aGVsbG8=');
  console.log('atob("aGVsbG8=") =', result);
  if (result === 'hello') {
    console.log('✅ PASS: Basic atob decoding works correctly');
  } else {
    console.log('❌ FAIL: Expected "hello", got', result);
  }
} else {
  console.log('❌ FAIL: atob is not implemented');
}

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
  try {
    const encoded = btoa(testStr);
    const decoded = atob(encoded);
    if (decoded === testStr) {
      console.log(`✅ PASS: "${testStr}" round-trip successful`);
    } else {
      console.log(`❌ FAIL: "${testStr}" round-trip failed. Got: "${decoded}"`);
    }
  } catch (e) {
    console.log(`❌ FAIL: "${testStr}" round-trip threw error:`, e.message);
  }
}

// Test 4: Binary data encoding (bytes 0-255)
console.log('Test 4: Binary data encoding');
try {
  // Test with various byte values
  const binaryStr = String.fromCharCode(0, 1, 127, 255);
  const encoded = btoa(binaryStr);
  const decoded = atob(encoded);
  
  let match = decoded.length === binaryStr.length;
  for (let i = 0; i < binaryStr.length && match; i++) {
    if (decoded.charCodeAt(i) !== binaryStr.charCodeAt(i)) {
      match = false;
    }
  }
  
  if (match) {
    console.log('✅ PASS: Binary data encoding works correctly');
  } else {
    console.log('❌ FAIL: Binary data encoding failed');
  }
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
try {
  btoa();
  console.log('❌ FAIL: btoa() should have thrown an error');
} catch (e) {
  console.log('✅ PASS: btoa() correctly threw error:', e.message);
}

try {
  atob();
  console.log('❌ FAIL: atob() should have thrown an error');
} catch (e) {
  console.log('✅ PASS: atob() correctly threw error:', e.message);
}

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
  try {
    const encoded = btoa(vector.input);
    if (encoded === vector.output) {
      console.log(`✅ PASS: btoa("${vector.input}") = "${encoded}"`);
    } else {
      console.log(`❌ FAIL: btoa("${vector.input}") expected "${vector.output}", got "${encoded}"`);
    }
    
    const decoded = atob(vector.output);
    if (decoded === vector.input) {
      console.log(`✅ PASS: atob("${vector.output}") = "${decoded}"`);
    } else {
      console.log(`❌ FAIL: atob("${vector.output}") expected "${vector.input}", got "${decoded}"`);
    }
  } catch (e) {
    console.log(`❌ FAIL: Test vector failed with error:`, e.message);
  }
}

console.log('=== Base64 Utilities Tests Completed ===');