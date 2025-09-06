const assert = require('jsrt:assert');
const fs = require('node:fs');
const Buffer = require('node:buffer').Buffer;
const os = require('node:os');

console.log('=== Enhanced Node.js File System Buffer Integration Tests ===');

// Create test file paths using cross-platform temporary directory
const tmpDir = os.tmpdir();
const testBinaryPath = tmpDir + '/jsrt_binary_test.bin';
const testTextPath = tmpDir + '/jsrt_text_test.txt';

// Test data
const binaryData = new Uint8Array([
  0x00, 0x01, 0x02, 0x03, 0xff, 0xfe, 0xfd, 0xfc,
]);
const textData = 'Hello, Enhanced Buffer Integration!';

try {
  // Clean up any existing test files
  if (fs.existsSync(testBinaryPath)) {
    fs.unlinkSync(testBinaryPath);
  }
  if (fs.existsSync(testTextPath)) {
    fs.unlinkSync(testTextPath);
  }

  console.log('\n--- Testing Enhanced Buffer Writing ---');

  // Test 1: Write Buffer to file
  const buffer = Buffer.from(binaryData);
  fs.writeFileSync(testBinaryPath, buffer);
  console.log('✓ Buffer.from(Uint8Array) writeFileSync works');

  // Test 2: Write Uint8Array directly
  fs.writeFileSync(testBinaryPath + '2', binaryData);
  console.log('✓ Uint8Array writeFileSync works');

  // Test 3: Write ArrayBuffer
  const arrayBuffer = new ArrayBuffer(8);
  const view = new Uint8Array(arrayBuffer);
  view.set([0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80]);
  const bufferFromArrayBuffer = Buffer.from(arrayBuffer);
  fs.writeFileSync(testBinaryPath + '3', bufferFromArrayBuffer);
  console.log('✓ Buffer.from(ArrayBuffer) writeFileSync works');

  console.log('\n--- Testing Enhanced Buffer Reading ---');

  // Test 4: Read binary file as Buffer (default behavior)
  const readBuffer = fs.readFileSync(testBinaryPath);
  assert.ok(Buffer.isBuffer(readBuffer), 'Should return Buffer by default');
  assert.strictEqual(
    readBuffer.length,
    binaryData.length,
    'Buffer length should match'
  );

  // Verify binary data integrity
  for (let i = 0; i < binaryData.length; i++) {
    assert.strictEqual(readBuffer[i], binaryData[i], `Byte ${i} should match`);
  }
  console.log('✓ Binary data integrity maintained');

  // Test 5: Read file with explicit encoding
  fs.writeFileSync(testTextPath, textData);
  const readText = fs.readFileSync(testTextPath, { encoding: 'utf8' });
  assert.strictEqual(
    typeof readText,
    'string',
    'Should return string with encoding'
  );
  assert.strictEqual(readText, textData, 'Text content should match');
  console.log('✓ Text reading with encoding works');

  // Test 6: Read file without encoding (should return Buffer)
  const readTextBuffer = fs.readFileSync(testTextPath);
  assert.ok(
    Buffer.isBuffer(readTextBuffer),
    'Should return Buffer without encoding'
  );
  assert.strictEqual(
    readTextBuffer.toString(),
    textData,
    'Buffer content should match'
  );
  console.log('✓ Text reading without encoding returns Buffer');

  console.log('\n--- Testing Binary File Round-trip ---');

  // Test 7: Binary round-trip test
  const originalBytes = new Uint8Array(256);
  for (let i = 0; i < 256; i++) {
    originalBytes[i] = i;
  }

  const originalBuffer = Buffer.from(originalBytes);
  fs.writeFileSync(testBinaryPath + '_roundtrip', originalBuffer);

  const readbackBuffer = fs.readFileSync(testBinaryPath + '_roundtrip');
  assert.ok(Buffer.isBuffer(readbackBuffer), 'Should return Buffer');
  assert.strictEqual(readbackBuffer.length, 256, 'Should have correct length');

  for (let i = 0; i < 256; i++) {
    assert.strictEqual(
      readbackBuffer[i],
      i,
      `Byte ${i} should match in round-trip`
    );
  }
  console.log('✓ 256-byte binary round-trip successful');

  console.log('\n--- Testing TypedArray Integration ---');

  // Test 8: Test various TypedArray types
  const int16Data = new Int16Array([0x1234, 0x5678, 0x9abc, 0xdef0]);
  const int16Buffer = Buffer.from(int16Data.buffer);
  fs.writeFileSync(testBinaryPath + '_int16', int16Buffer);

  const readInt16Buffer = fs.readFileSync(testBinaryPath + '_int16');
  assert.strictEqual(
    readInt16Buffer.length,
    int16Data.byteLength,
    'Int16 buffer length should match'
  );
  console.log('✓ Int16Array Buffer integration works');

  // Test 9: Test Float32Array
  const float32Data = new Float32Array([1.5, 2.5, 3.14159, -1.0]);
  const float32Buffer = Buffer.from(float32Data.buffer);
  fs.writeFileSync(testBinaryPath + '_float32', float32Buffer);

  const readFloat32Buffer = fs.readFileSync(testBinaryPath + '_float32');
  assert.strictEqual(
    readFloat32Buffer.length,
    float32Data.byteLength,
    'Float32 buffer length should match'
  );
  console.log('✓ Float32Array Buffer integration works');

  console.log('\n--- Testing Error Handling ---');

  // Test 10: Invalid data type
  try {
    fs.writeFileSync(testBinaryPath + '_invalid', { invalid: 'object' });
    assert.fail('Should throw error for invalid data type');
  } catch (error) {
    assert.ok(
      error.message.includes('must be string, Buffer, or TypedArray'),
      'Should throw appropriate error'
    );
    console.log('✓ Invalid data type error handling works');
  }

  // Cleanup test files
  try {
    fs.unlinkSync(testBinaryPath);
    fs.unlinkSync(testBinaryPath + '2');
    fs.unlinkSync(testBinaryPath + '3');
    fs.unlinkSync(testBinaryPath + '_roundtrip');
    fs.unlinkSync(testBinaryPath + '_int16');
    fs.unlinkSync(testBinaryPath + '_float32');
    fs.unlinkSync(testTextPath);
  } catch (e) {
    // Ignore cleanup errors
  }

  console.log('\n✅ All Enhanced Buffer Integration tests passed!');
  console.log('🎉 Node.js fs module now has enhanced Buffer support!');

  console.log('\nEnhanced Features:');
  console.log('  ✅ Binary file handling with proper Buffer objects');
  console.log('  ✅ TypedArray (Uint8Array, Int16Array, Float32Array) support');
  console.log('  ✅ ArrayBuffer integration via Buffer.from()');
  console.log('  ✅ Automatic Buffer return for binary data');
  console.log('  ✅ String return with explicit encoding option');
  console.log('  ✅ Memory-safe binary data handling');
  console.log('  ✅ Full round-trip binary data integrity');
} catch (error) {
  console.error('Enhanced Buffer integration test failed:', error.message);
  throw error;
}
