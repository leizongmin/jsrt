// Test deflateRaw, inflateRaw, and unzip
const zlib = require('node:zlib');

console.log('Testing deflateRawSync/inflateRawSync...');

const testString = 'Raw deflate test data';
const encoder = new TextEncoder();
const testData = encoder.encode(testString);

// Test deflateRawSync/inflateRawSync
const deflatedRaw = zlib.deflateRawSync(testData);
console.log('Raw deflated length:', deflatedRaw.length);

const inflatedRaw = zlib.inflateRawSync(deflatedRaw);
const decoder = new TextDecoder();
const inflatedString = decoder.decode(inflatedRaw);
console.log('Inflated data:', inflatedString);

if (inflatedString === testString) {
  console.log('✓ deflateRawSync/inflateRawSync round-trip passed!');
} else {
  console.error('❌ Raw deflate test failed!');
  process.exit(1);
}

// Test unzipSync with gzip data
console.log('\nTesting unzipSync with gzip data...');
const gzipped = zlib.gzipSync(testData);
const unzippedFromGzip = zlib.unzipSync(gzipped);
const unzippedString1 = decoder.decode(unzippedFromGzip);

if (unzippedString1 === testString) {
  console.log('✓ unzipSync works with gzip data!');
} else {
  console.error('❌ unzipSync failed with gzip data!');
  process.exit(1);
}

// Test unzipSync with deflate data
console.log('\nTesting unzipSync with deflate data...');
const deflated = zlib.deflateSync(testData);
const unzippedFromDeflate = zlib.unzipSync(deflated);
const unzippedString2 = decoder.decode(unzippedFromDeflate);

if (unzippedString2 === testString) {
  console.log('✓ unzipSync works with deflate data!');
} else {
  console.error('❌ unzipSync failed with deflate data!');
  process.exit(1);
}

console.log('\n✓ All raw deflate and unzip tests passed!');
