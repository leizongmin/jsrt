// Comprehensive Encoding API tests for WinterCG compliance
const assert = require("std:assert");
console.log('=== Starting Encoding API Tests ===');

// Test 1: TextEncoder basic functionality
console.log('Test 1: TextEncoder - basic functionality');
const encoder = new TextEncoder();
console.log('TextEncoder created successfully');
assert.strictEqual(typeof encoder, 'object', 'TextEncoder should be an object');
console.log('Encoding:', encoder.encoding);
assert.strictEqual(encoder.encoding, 'utf-8', 'TextEncoder encoding should be utf-8');
console.log('✓ TextEncoder encoding property is correct');

// Test 2: TextEncoder encode() method
console.log('Test 2: TextEncoder encode() method');
try {
  const encoder = new TextEncoder();
  const result = encoder.encode('Hello, World!');
  console.log('Encoded result type:', typeof result);
  console.log('Encoded result constructor:', result.constructor.name);
  
  assert(result instanceof Uint8Array, 'encode() should return Uint8Array');
  console.log('✓ encode() returns Uint8Array');
  console.log('Encoded bytes length:', result.length);
  console.log('Encoded bytes:', Array.from(result));
  
  // Verify UTF-8 encoding
  const expected = [72, 101, 108, 108, 111, 44, 32, 87, 111, 114, 108, 100, 33];
  assert.strictEqual(result.length, expected.length, 'Encoded length should match expected');
  for (let i = 0; i < result.length; i++) {
    assert.strictEqual(result[i], expected[i], `Byte at position ${i} should match expected`);
  }
  console.log('✓ UTF-8 encoding is correct');
} catch (e) {
  console.log('✗ TextEncoder encode() failed:', e.message);
}

// Test 3: TextEncoder encode() with empty string
console.log('Test 3: TextEncoder encode() with empty string');
try {
  const encoder = new TextEncoder();
  const result = encoder.encode('');
  assert.strictEqual(result.length, 0, 'Empty string should encode to empty array');
  console.log('✓ Empty string encodes to empty array');
} catch (e) {
  console.log('✗ TextEncoder encode() empty string failed:', e.message);
}

// Test 4: TextEncoder encode() with Unicode characters
console.log('Test 4: TextEncoder encode() with Unicode characters');
try {
  const encoder = new TextEncoder();
  const result = encoder.encode('🚀');
  console.log('Unicode emoji encoded bytes:', Array.from(result));
  
  // Rocket emoji in UTF-8: [240, 159, 154, 128]
  const expected = [240, 159, 154, 128];
  let matches = result.length === expected.length;
  if (matches) {
    for (let i = 0; i < result.length; i++) {
      if (result[i] !== expected[i]) {
        matches = false;
        break;
      }
    }
  }
  
  assert.strictEqual(result.length, expected.length, 'Unicode encoded length should match expected');
  for (let i = 0; i < result.length; i++) {
    assert.strictEqual(result[i], expected[i], `Unicode byte at position ${i} should match expected`);
  }
  console.log('✓ Unicode character encoding is correct');
} catch (e) {
  console.log('✗ TextEncoder Unicode encoding failed:', e.message);
}

// Test 5: TextEncoder encodeInto() method
console.log('Test 5: TextEncoder encodeInto() method');
try {
  const encoder = new TextEncoder();
  const destination = new Uint8Array(20);
  const result = encoder.encodeInto('Hello', destination);
  
  console.log('encodeInto result:', result);
  if (typeof result === 'object' && result.read !== undefined && result.written !== undefined) {
    console.log('✓ encodeInto returns result object with read/written properties');
    console.log('Read:', result.read, 'Written:', result.written);
    
    if (result.read === 5 && result.written === 5) {
      console.log('✓ encodeInto read/written values are correct');
    } else {
      console.log('✗ encodeInto read/written values are incorrect');
    }
    
    // Check destination array
    const expected = [72, 101, 108, 108, 111]; // 'Hello' in UTF-8
    let matches = true;
    for (let i = 0; i < expected.length; i++) {
      if (destination[i] !== expected[i]) {
        matches = false;
        break;
      }
    }
    
    if (matches) {
      console.log('✓ encodeInto writes correct bytes to destination');
    } else {
      console.log('✗ encodeInto writes incorrect bytes');
      console.log('Expected:', expected);
      console.log('Actual:', Array.from(destination.slice(0, 5)));
    }
  } else {
    console.log('✗ encodeInto does not return proper result object');
  }
} catch (e) {
  console.log('✗ TextEncoder encodeInto() failed:', e.message);
}

// Test 6: TextDecoder basic functionality
console.log('Test 6: TextDecoder - basic functionality');
try {
  const decoder = new TextDecoder();
  console.log('TextDecoder created successfully');
  console.log('Encoding:', decoder.encoding);
  console.log('Fatal:', decoder.fatal);
  console.log('IgnoreBOM:', decoder.ignoreBOM);
  
  assert.strictEqual(decoder.encoding, 'utf-8', 'TextDecoder encoding should be utf-8');
  assert.strictEqual(decoder.fatal, false, 'TextDecoder fatal should be false by default');
  assert.strictEqual(decoder.ignoreBOM, false, 'TextDecoder ignoreBOM should be false by default');
  console.log('✓ TextDecoder properties are correct');
} catch (e) {
  console.log('✗ TextDecoder constructor failed:', e.message);
}

// Test 7: TextDecoder constructor with options
console.log('Test 7: TextDecoder constructor with options');
try {
  const decoder = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true });
  console.log('Fatal:', decoder.fatal);
  console.log('IgnoreBOM:', decoder.ignoreBOM);
  
  if (decoder.fatal === true && decoder.ignoreBOM === true) {
    console.log('✓ TextDecoder options are correctly set');
  } else {
    console.log('✗ TextDecoder options are not correctly set');
  }
} catch (e) {
  console.log('✗ TextDecoder constructor with options failed:', e.message);
}

// Test 8: TextDecoder decode() method
console.log('Test 8: TextDecoder decode() method');
try {
  const decoder = new TextDecoder();
  const bytes = new Uint8Array([72, 101, 108, 108, 111, 44, 32, 87, 111, 114, 108, 100, 33]);
  const result = decoder.decode(bytes);
  
  console.log('Decoded result:', result);
  assert.strictEqual(result, 'Hello, World!', 'decode() should correctly decode UTF-8 bytes');
  console.log('✓ decode() correctly decodes UTF-8 bytes');
} catch (e) {
  console.log('✗ TextDecoder decode() failed:', e.message);
}

// Test 9: TextDecoder decode() with empty array
console.log('Test 9: TextDecoder decode() with empty array');
try {
  const decoder = new TextDecoder();
  const result = decoder.decode(new Uint8Array(0));
  
  assert.strictEqual(result, '', 'Empty array should decode to empty string');
  console.log('✓ Empty array decodes to empty string');
} catch (e) {
  console.log('✗ TextDecoder decode() empty array failed:', e.message);
}

// Test 10: TextDecoder decode() with Unicode characters
console.log('Test 10: TextDecoder decode() with Unicode characters');
try {
  const decoder = new TextDecoder();
  const bytes = new Uint8Array([240, 159, 154, 128]); // 🚀 in UTF-8
  const result = decoder.decode(bytes);
  
  console.log('Decoded Unicode result:', result);
  assert.strictEqual(result, '🚀', 'Unicode character should decode correctly');
  console.log('✓ Unicode character decoding is correct');
} catch (e) {
  console.log('✗ TextDecoder Unicode decoding failed:', e.message);
}

// Test 11: TextDecoder decode() with no arguments
console.log('Test 11: TextDecoder decode() with no arguments');
try {
  const decoder = new TextDecoder();
  const result = decoder.decode();
  
  if (result === '') {
    console.log('✓ decode() with no arguments returns empty string');
  } else {
    console.log('✗ decode() with no arguments failed, result:', JSON.stringify(result));
  }
} catch (e) {
  console.log('✗ TextDecoder decode() with no arguments failed:', e.message);
}

// Test 12: Round-trip encoding/decoding
console.log('Test 12: Round-trip encoding/decoding');
try {
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();
  
  const original = 'Hello, 世界! 🌍';
  const encoded = encoder.encode(original);
  const decoded = decoder.decode(encoded);
  
  console.log('Original:', original);
  console.log('Decoded:', decoded);
  
  assert.strictEqual(original, decoded, 'Round-trip encoding/decoding should preserve data');
  console.log('✓ Round-trip encoding/decoding preserves data');
} catch (e) {
  console.log('✗ Round-trip test failed:', e.message);
}

// Test 13: Error handling - invalid arguments
console.log('Test 13: Error handling');
try {
  const encoder = new TextEncoder();
  
  // Test encodeInto with invalid arguments
  try {
    encoder.encodeInto('test');
    console.log('✗ encodeInto should throw with insufficient arguments');
  } catch (e) {
    console.log('✓ encodeInto correctly throws with insufficient arguments');
  }
  
  const decoder = new TextDecoder();
  
  // Test decode with invalid input
  try {
    decoder.decode('not a typed array');
    console.log('✗ decode should throw with invalid input type');
  } catch (e) {
    console.log('✓ decode correctly throws with invalid input type');
  }
} catch (e) {
  console.log('✗ Error handling tests failed:', e.message);
}

console.log('=== Encoding API Tests Complete ===');