// Test zlib compression and decompression
const zlib = require('node:zlib');

console.log('Testing zlib gzip/gunzip...');

// Test data - create as Uint8Array directly
const testString = 'Hello, World! This is a test of zlib compression.';
const encoder = new TextEncoder();
const testData = encoder.encode(testString);

// Test gzipSync
console.log('Original data:', testString);
console.log('Original length:', testData.length);

const compressed = zlib.gzipSync(testData);
console.log('Compressed length:', compressed.length);
console.log('Compressed data (first 20 bytes):', compressed.slice(0, 20));

// Test gunzipSync
const decompressed = zlib.gunzipSync(compressed);
console.log('Decompressed length:', decompressed.length);
const decoder = new TextDecoder();
const decompressedString = decoder.decode(decompressed);
console.log('Decompressed data:', decompressedString);

// Verify round-trip
if (testString === decompressedString) {
  console.log('✓ Round-trip test passed!');
} else {
  console.error('❌ Round-trip test failed!');
  console.error('Expected:', testString);
  console.error('Got:', decompressedString);
  process.exit(1);
}

// Test deflateSync/inflateSync
console.log('\nTesting zlib deflate/inflate...');
const deflated = zlib.deflateSync(testData);
console.log('Deflated length:', deflated.length);

const inflated = zlib.inflateSync(deflated);
const inflatedString = decoder.decode(inflated);
console.log('Inflated data:', inflatedString);

if (testString === inflatedString) {
  console.log('✓ Deflate/inflate round-trip passed!');
} else {
  console.error('❌ Deflate/inflate round-trip failed!');
  process.exit(1);
}

// Test with options
console.log('\nTesting with compression level options...');
const compressed1 = zlib.gzipSync(testData, { level: 1 }); // Fast
const compressed9 = zlib.gzipSync(testData, { level: 9 }); // Best compression

console.log('Level 1 size:', compressed1.length);
console.log('Level 9 size:', compressed9.length);
console.log('Level 9 is smaller:', compressed9.length < compressed1.length);

// Verify both decompress correctly
const decompressed1String = decoder.decode(zlib.gunzipSync(compressed1));
const decompressed9String = decoder.decode(zlib.gunzipSync(compressed9));

if (decompressed1String === testString && decompressed9String === testString) {
  console.log('✓ Compression level options work!');
} else {
  console.error('❌ Compression level options failed!');
  process.exit(1);
}

console.log('\n✓ All zlib tests passed!');
