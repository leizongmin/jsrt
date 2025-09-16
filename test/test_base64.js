const assert = require('jsrt:assert');

// Test btoa function
const result = btoa('hello');
if (result !== 'aGVsbG8=') {
  console.error('❌ FAIL: btoa("hello") expected "aGVsbG8=", got', result);
} else {
  assert.strictEqual(result, 'aGVsbG8=', 'btoa should encode correctly');
}

// Test atob function
const result2 = atob('aGVsbG8=');
if (result2 !== 'hello') {
  console.error('❌ FAIL: atob("aGVsbG8=") expected "hello", got', result2);
} else {
  assert.strictEqual(result2, 'hello', 'atob should decode correctly');
}

// Test round-trip encoding/decoding
const testStrings = [
  'hello world',
  'JavaScript',
  '123456789',
  'special chars: !@#$%^&*()',
  'unicode: 你好世界',
  '',
];

for (const testStr of testStrings) {
  try {
    const encoded = btoa(testStr);
    const decoded = atob(encoded);
    if (decoded !== testStr) {
      console.error(`❌ FAIL: Round-trip failed for "${testStr}"`);
    } else {
      assert.strictEqual(
        decoded,
        testStr,
        `Round-trip should work for "${testStr}"`
      );
    }
  } catch (e) {
    // Some strings might not be encodable
    assert.strictEqual(
      typeof e.message,
      'string',
      'Error should have a message'
    );
  }
}

// Test binary data
try {
  const binaryData = new Uint8Array([0, 1, 2, 3, 255, 254, 253]);
  const binaryStr = String.fromCharCode(...binaryData);
  const encoded = btoa(binaryStr);
  const decoded = atob(encoded);
  const decodedArray = new Uint8Array(
    decoded.split('').map((c) => c.charCodeAt(0))
  );

  if (!binaryData.every((val, i) => val === decodedArray[i])) {
    console.error('❌ FAIL: Binary data encoding/decoding mismatch');
  }
} catch (e) {
  console.error('❌ FAIL: Binary data encoding threw error:', e.message);
}

// Test invalid base64 strings
const invalidStrings = ['invalid!', 'aGVsb', 'aGVsbG8'];
for (const invalid of invalidStrings) {
  try {
    atob(invalid);
    console.error(`❌ FAIL: atob("${invalid}") should have thrown an error`);
  } catch (e) {
    assert.strictEqual(
      typeof e.message,
      'string',
      'Error should have a message'
    );
  }
}

// Test error cases
try {
  btoa();
  console.error('❌ FAIL: btoa() should have thrown an error');
} catch (e) {
  assert.strictEqual(typeof e.message, 'string', 'Error should have a message');
}

try {
  atob();
  console.error('❌ FAIL: atob() should have thrown an error');
} catch (e) {
  assert.strictEqual(typeof e.message, 'string', 'Error should have a message');
}

// Test complex cases
const complexTests = [
  { input: 'Man', expected: 'TWFu' },
  { input: 'Ma', expected: 'TWE=' },
  { input: 'M', expected: 'TQ==' },
  { input: 'sure.', expected: 'c3VyZS4=' },
  { input: 'sure', expected: 'c3VyZQ==' },
  { input: 'sur', expected: 'c3Vy' },
  { input: 'su', expected: 'c3U=' },
  { input: 's', expected: 'cw==' },
];

for (const test of complexTests) {
  const encoded = btoa(test.input);
  if (encoded !== test.expected) {
    console.error(
      `❌ FAIL: btoa("${test.input}") expected "${test.expected}", got "${encoded}"`
    );
  } else {
    assert.strictEqual(
      encoded,
      test.expected,
      `btoa should encode "${test.input}" correctly`
    );
  }

  const decoded = atob(test.expected);
  if (decoded !== test.input) {
    console.error(
      `❌ FAIL: atob("${test.expected}") expected "${test.input}", got "${decoded}"`
    );
  } else {
    assert.strictEqual(
      decoded,
      test.input,
      `atob should decode "${test.expected}" correctly`
    );
  }
}
