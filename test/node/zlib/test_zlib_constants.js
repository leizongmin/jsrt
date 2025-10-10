// Test zlib constants and utilities
const zlib = require('node:zlib');

console.log('Testing zlib constants and utilities...\n');

let passed = 0;
let failed = 0;

// Test compression level constants
console.log('1. Testing compression level constants...');
if (typeof zlib.Z_NO_COMPRESSION === 'number' && zlib.Z_NO_COMPRESSION === 0) {
  console.log('  ✓ Z_NO_COMPRESSION =', zlib.Z_NO_COMPRESSION);
  passed++;
} else {
  console.error('  ❌ Z_NO_COMPRESSION failed');
  failed++;
}

if (typeof zlib.Z_BEST_SPEED === 'number' && zlib.Z_BEST_SPEED === 1) {
  console.log('  ✓ Z_BEST_SPEED =', zlib.Z_BEST_SPEED);
  passed++;
} else {
  console.error('  ❌ Z_BEST_SPEED failed');
  failed++;
}

if (
  typeof zlib.Z_BEST_COMPRESSION === 'number' &&
  zlib.Z_BEST_COMPRESSION === 9
) {
  console.log('  ✓ Z_BEST_COMPRESSION =', zlib.Z_BEST_COMPRESSION);
  passed++;
} else {
  console.error('  ❌ Z_BEST_COMPRESSION failed');
  failed++;
}

if (
  typeof zlib.Z_DEFAULT_COMPRESSION === 'number' &&
  zlib.Z_DEFAULT_COMPRESSION === -1
) {
  console.log('  ✓ Z_DEFAULT_COMPRESSION =', zlib.Z_DEFAULT_COMPRESSION);
  passed++;
} else {
  console.error('  ❌ Z_DEFAULT_COMPRESSION failed');
  failed++;
}

// Test strategy constants
console.log('\n2. Testing strategy constants...');
if (typeof zlib.Z_DEFAULT_STRATEGY === 'number') {
  console.log('  ✓ Z_DEFAULT_STRATEGY =', zlib.Z_DEFAULT_STRATEGY);
  passed++;
} else {
  console.error('  ❌ Z_DEFAULT_STRATEGY failed');
  failed++;
}

if (typeof zlib.Z_FILTERED === 'number') {
  console.log('  ✓ Z_FILTERED =', zlib.Z_FILTERED);
  passed++;
} else {
  console.error('  ❌ Z_FILTERED failed');
  failed++;
}

// Test flush constants
console.log('\n3. Testing flush constants...');
if (typeof zlib.Z_NO_FLUSH === 'number' && zlib.Z_NO_FLUSH === 0) {
  console.log('  ✓ Z_NO_FLUSH =', zlib.Z_NO_FLUSH);
  passed++;
} else {
  console.error('  ❌ Z_NO_FLUSH failed');
  failed++;
}

if (typeof zlib.Z_SYNC_FLUSH === 'number') {
  console.log('  ✓ Z_SYNC_FLUSH =', zlib.Z_SYNC_FLUSH);
  passed++;
} else {
  console.error('  ❌ Z_SYNC_FLUSH failed');
  failed++;
}

if (typeof zlib.Z_FINISH === 'number') {
  console.log('  ✓ Z_FINISH =', zlib.Z_FINISH);
  passed++;
} else {
  console.error('  ❌ Z_FINISH failed');
  failed++;
}

// Test return code constants
console.log('\n4. Testing return code constants...');
if (typeof zlib.Z_OK === 'number' && zlib.Z_OK === 0) {
  console.log('  ✓ Z_OK =', zlib.Z_OK);
  passed++;
} else {
  console.error('  ❌ Z_OK failed');
  failed++;
}

if (typeof zlib.Z_STREAM_END === 'number') {
  console.log('  ✓ Z_STREAM_END =', zlib.Z_STREAM_END);
  passed++;
} else {
  console.error('  ❌ Z_STREAM_END failed');
  failed++;
}

// Test constants object
console.log('\n5. Testing constants object...');
if (typeof zlib.constants === 'object' && zlib.constants !== null) {
  console.log('  ✓ zlib.constants exists');
  if (zlib.constants.Z_OK === zlib.Z_OK) {
    console.log('  ✓ constants.Z_OK matches direct export');
    passed += 2;
  } else {
    console.error('  ❌ constants.Z_OK mismatch');
    failed++;
  }
} else {
  console.error('  ❌ zlib.constants not found');
  failed++;
}

// Test versions
console.log('\n6. Testing version info...');
if (
  typeof zlib.versions === 'object' &&
  typeof zlib.versions.zlib === 'string'
) {
  console.log('  ✓ zlib.versions.zlib =', zlib.versions.zlib);
  passed++;
} else {
  console.error('  ❌ zlib.versions.zlib failed');
  failed++;
}

// Test crc32 utility
console.log('\n7. Testing crc32 utility...');
const testData = new Uint8Array([0x48, 0x65, 0x6c, 0x6c, 0x6f]); // "Hello"
const crc = zlib.crc32(testData);
console.log('  CRC32 of "Hello":', crc);
if (typeof crc === 'number' && crc > 0) {
  console.log('  ✓ crc32 returned a number');
  passed++;

  // Test with initial value
  const crc2 = zlib.crc32(testData, 0);
  if (crc2 === crc) {
    console.log('  ✓ crc32 with initial value 0 matches');
    passed++;
  } else {
    console.error('  ❌ crc32 with initial value mismatch');
    failed++;
  }
} else {
  console.error('  ❌ crc32 failed');
  failed++;
}

// Test adler32 utility
console.log('\n8. Testing adler32 utility...');
const adler = zlib.adler32(testData);
console.log('  Adler-32 of "Hello":', adler);
if (typeof adler === 'number' && adler > 0) {
  console.log('  ✓ adler32 returned a number');
  passed++;

  // Test with initial value
  const adler2 = zlib.adler32(testData, 1);
  if (adler2 === adler) {
    console.log('  ✓ adler32 with initial value 1 matches');
    passed++;
  } else {
    console.error('  ❌ adler32 with initial value mismatch');
    failed++;
  }
} else {
  console.error('  ❌ adler32 failed');
  failed++;
}

// Summary
console.log('\n========================================');
console.log('Total tests passed:', passed);
console.log('Total tests failed:', failed);

if (failed === 0) {
  console.log('\n✓ All constants and utilities tests passed!');
  process.exit(0);
} else {
  console.log('\n❌ Some tests failed!');
  process.exit(1);
}
