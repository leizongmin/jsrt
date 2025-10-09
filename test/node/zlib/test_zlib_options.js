// Test comprehensive zlib options
const zlib = require('node:zlib');

console.log('Testing zlib advanced options...\n');

let passed = 0;
let failed = 0;

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const testString = 'The quick brown fox jumps over the lazy dog. '.repeat(10);
const testData = encoder.encode(testString);

// Test 1: Compression level option (6.1.1)
console.log('1. Testing compression level option...');
try {
  const level0 = zlib.gzipSync(testData, { level: 0 }); // No compression
  const level1 = zlib.gzipSync(testData, { level: 1 }); // Best speed
  const level9 = zlib.gzipSync(testData, { level: 9 }); // Best compression
  const levelDefault = zlib.gzipSync(testData, { level: -1 }); // Default

  console.log('  Level 0 (no compression) size:', level0.length);
  console.log('  Level 1 (best speed) size:', level1.length);
  console.log('  Level 9 (best compression) size:', level9.length);
  console.log('  Level -1 (default) size:', levelDefault.length);

  // Verify all decompress correctly
  const d0 = decoder.decode(zlib.gunzipSync(level0));
  const d1 = decoder.decode(zlib.gunzipSync(level1));
  const d9 = decoder.decode(zlib.gunzipSync(level9));
  const dd = decoder.decode(zlib.gunzipSync(levelDefault));

  if (
    d0 === testString &&
    d1 === testString &&
    d9 === testString &&
    dd === testString
  ) {
    console.log('  ✓ All compression levels work correctly');
    passed++;
  } else {
    console.error('  ❌ Compression level test failed');
    failed++;
  }
} catch (e) {
  console.error('  ❌ Error:', e.message);
  failed++;
}

// Test 2: WindowBits option (6.1.2)
console.log('\n2. Testing windowBits option...');
try {
  // Test different windowBits values
  const wb8 = zlib.deflateSync(testData, { windowBits: 8 });
  const wb15 = zlib.deflateSync(testData, { windowBits: 15 });

  console.log('  windowBits=8 size:', wb8.length);
  console.log('  windowBits=15 size:', wb15.length);

  // Decompress without specifying windowBits (let zlib auto-detect)
  const dwb8 = decoder.decode(zlib.inflateSync(wb8));
  const dwb15 = decoder.decode(zlib.inflateSync(wb15));

  if (dwb8 === testString && dwb15 === testString) {
    console.log('  ✓ WindowBits option works correctly');
    passed++;
  } else {
    console.error('  ❌ WindowBits test failed');
    failed++;
  }
} catch (e) {
  console.error('  ❌ Error:', e.message);
  failed++;
}

// Test 3: MemLevel option (6.1.3)
console.log('\n3. Testing memLevel option...');
try {
  const ml1 = zlib.deflateSync(testData, { memLevel: 1 });
  const ml9 = zlib.deflateSync(testData, { memLevel: 9 });

  console.log('  memLevel=1 size:', ml1.length);
  console.log('  memLevel=9 size:', ml9.length);

  // Decompress and verify
  const dml1 = decoder.decode(zlib.inflateSync(ml1));
  const dml9 = decoder.decode(zlib.inflateSync(ml9));

  if (dml1 === testString && dml9 === testString) {
    console.log('  ✓ MemLevel option works correctly');
    passed++;
  } else {
    console.error('  ❌ MemLevel test failed');
    failed++;
  }
} catch (e) {
  console.error('  ❌ Error:', e.message);
  failed++;
}

// Test 4: Strategy option (6.1.4)
console.log('\n4. Testing strategy option...');
try {
  const stratDefault = zlib.deflateSync(testData, {
    strategy: zlib.Z_DEFAULT_STRATEGY,
  });
  const stratFiltered = zlib.deflateSync(testData, {
    strategy: zlib.Z_FILTERED,
  });
  const stratHuffman = zlib.deflateSync(testData, {
    strategy: zlib.Z_HUFFMAN_ONLY,
  });

  console.log('  Z_DEFAULT_STRATEGY size:', stratDefault.length);
  console.log('  Z_FILTERED size:', stratFiltered.length);
  console.log('  Z_HUFFMAN_ONLY size:', stratHuffman.length);

  // Decompress and verify
  const ds1 = decoder.decode(zlib.inflateSync(stratDefault));
  const ds2 = decoder.decode(zlib.inflateSync(stratFiltered));
  const ds3 = decoder.decode(zlib.inflateSync(stratHuffman));

  if (ds1 === testString && ds2 === testString && ds3 === testString) {
    console.log('  ✓ Strategy option works correctly');
    passed++;
  } else {
    console.error('  ❌ Strategy test failed');
    failed++;
  }
} catch (e) {
  console.error('  ❌ Error:', e.message);
  failed++;
}

// Test 5: ChunkSize option
console.log('\n5. Testing chunkSize option...');
try {
  const chunk8k = zlib.gzipSync(testData, { chunkSize: 8192 });
  const chunk32k = zlib.gzipSync(testData, { chunkSize: 32768 });

  console.log('  chunkSize=8192 size:', chunk8k.length);
  console.log('  chunkSize=32768 size:', chunk32k.length);

  // Decompress and verify
  const dc1 = decoder.decode(zlib.gunzipSync(chunk8k));
  const dc2 = decoder.decode(zlib.gunzipSync(chunk32k));

  if (dc1 === testString && dc2 === testString) {
    console.log('  ✓ ChunkSize option works correctly');
    passed++;
  } else {
    console.error('  ❌ ChunkSize test failed');
    failed++;
  }
} catch (e) {
  console.error('  ❌ Error:', e.message);
  failed++;
}

// Test 6: Combined options
console.log('\n6. Testing combined options...');
try {
  const combined = zlib.gzipSync(testData, {
    level: 6,
    windowBits: 15,
    memLevel: 8,
    strategy: zlib.Z_DEFAULT_STRATEGY,
    chunkSize: 16384,
  });

  console.log('  Combined options size:', combined.length);

  const dcombined = decoder.decode(zlib.gunzipSync(combined));

  if (dcombined === testString) {
    console.log('  ✓ Combined options work correctly');
    passed++;
  } else {
    console.error('  ❌ Combined options test failed');
    failed++;
  }
} catch (e) {
  console.error('  ❌ Error:', e.message);
  failed++;
}

// Summary
console.log('\n========================================');
console.log('Total tests passed:', passed);
console.log('Total tests failed:', failed);

if (failed === 0) {
  console.log('\n✓ All advanced options tests passed!');
  process.exit(0);
} else {
  console.log('\n❌ Some tests failed!');
  process.exit(1);
}
