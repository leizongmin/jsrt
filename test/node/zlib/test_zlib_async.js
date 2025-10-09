// Test zlib async compression and decompression
const zlib = require('node:zlib');

console.log('Testing zlib async methods...');

const testString = 'Hello, World! This is a test of async zlib compression.';
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const testData = encoder.encode(testString);

let testsPassed = 0;
let testsFailed = 0;

// Test gzip async
console.log('\n1. Testing async gzip/gunzip...');
zlib.gzip(testData, (err, compressed) => {
  if (err) {
    console.error('❌ gzip failed:', err.message);
    testsFailed++;
    checkComplete();
    return;
  }

  console.log('  Compressed length:', compressed.length);

  // Test gunzip
  zlib.gunzip(compressed, (err, decompressed) => {
    if (err) {
      console.error('❌ gunzip failed:', err.message);
      testsFailed++;
      checkComplete();
      return;
    }

    const result = decoder.decode(decompressed);
    if (result === testString) {
      console.log('  ✓ Async gzip/gunzip round-trip passed!');
      testsPassed++;
    } else {
      console.error('  ❌ Round-trip failed! Got:', result);
      testsFailed++;
    }
    checkComplete();
  });
});

// Test deflate async
console.log('\n2. Testing async deflate/inflate...');
zlib.deflate(testData, (err, deflated) => {
  if (err) {
    console.error('❌ deflate failed:', err.message);
    testsFailed++;
    checkComplete();
    return;
  }

  console.log('  Deflated length:', deflated.length);

  // Test inflate
  zlib.inflate(deflated, (err, inflated) => {
    if (err) {
      console.error('❌ inflate failed:', err.message);
      testsFailed++;
      checkComplete();
      return;
    }

    const result = decoder.decode(inflated);
    if (result === testString) {
      console.log('  ✓ Async deflate/inflate round-trip passed!');
      testsPassed++;
    } else {
      console.error('  ❌ Round-trip failed! Got:', result);
      testsFailed++;
    }
    checkComplete();
  });
});

// Test with options
console.log('\n3. Testing async with compression level options...');
zlib.gzip(testData, { level: 9 }, (err, compressed) => {
  if (err) {
    console.error('❌ gzip with options failed:', err.message);
    testsFailed++;
    checkComplete();
    return;
  }

  console.log('  Compressed with level 9, length:', compressed.length);

  zlib.gunzip(compressed, (err, decompressed) => {
    if (err) {
      console.error('❌ gunzip failed:', err.message);
      testsFailed++;
      checkComplete();
      return;
    }

    const result = decoder.decode(decompressed);
    if (result === testString) {
      console.log('  ✓ Async with options passed!');
      testsPassed++;
    } else {
      console.error('  ❌ Options test failed!');
      testsFailed++;
    }
    checkComplete();
  });
});

// Test deflateRaw/inflateRaw async
console.log('\n4. Testing async deflateRaw/inflateRaw...');
zlib.deflateRaw(testData, (err, deflated) => {
  if (err) {
    console.error('❌ deflateRaw failed:', err.message);
    testsFailed++;
    checkComplete();
    return;
  }

  zlib.inflateRaw(deflated, (err, inflated) => {
    if (err) {
      console.error('❌ inflateRaw failed:', err.message);
      testsFailed++;
      checkComplete();
      return;
    }

    const result = decoder.decode(inflated);
    if (result === testString) {
      console.log('  ✓ Async raw deflate/inflate passed!');
      testsPassed++;
    } else {
      console.error('  ❌ Raw test failed!');
      testsFailed++;
    }
    checkComplete();
  });
});

// Test unzip async with gzip data
console.log('\n5. Testing async unzip with gzip data...');
zlib.gzip(testData, (err, gzipped) => {
  if (err) {
    console.error('❌ gzip for unzip test failed:', err.message);
    testsFailed++;
    checkComplete();
    return;
  }

  zlib.unzip(gzipped, (err, unzipped) => {
    if (err) {
      console.error('❌ unzip failed:', err.message);
      testsFailed++;
      checkComplete();
      return;
    }

    const result = decoder.decode(unzipped);
    if (result === testString) {
      console.log('  ✓ Async unzip with gzip data passed!');
      testsPassed++;
    } else {
      console.error('  ❌ Unzip test failed!');
      testsFailed++;
    }
    checkComplete();
  });
});

let completedTests = 0;
const totalTests = 5;

function checkComplete() {
  completedTests++;
  if (completedTests === totalTests) {
    console.log('\n========================================');
    console.log('Tests passed:', testsPassed);
    console.log('Tests failed:', testsFailed);

    if (testsFailed === 0) {
      console.log('\n✓ All async zlib tests passed!');
      process.exit(0);
    } else {
      console.log('\n❌ Some async tests failed!');
      process.exit(1);
    }
  }
}
