// Test basic zlib module loading
const zlib = require('node:zlib');

console.log('zlib module loaded:', typeof zlib);
console.log('gzipSync:', typeof zlib.gzipSync);
console.log('gunzipSync:', typeof zlib.gunzipSync);
console.log('deflateSync:', typeof zlib.deflateSync);
console.log('inflateSync:', typeof zlib.inflateSync);

// Test that placeholder functions throw
try {
  zlib.gzipSync(Buffer.from('test'));
  console.error('ERROR: gzipSync should have thrown');
} catch (e) {
  console.log('✓ gzipSync throws as expected:', e.message);
}

console.log('\n✓ All basic module tests passed');
