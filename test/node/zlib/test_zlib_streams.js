// Test zlib streaming compression
const zlib = require('node:zlib');
const { Readable, Writable, pipeline } = require('node:stream');

console.log('Testing zlib stream classes...\n');

let passed = 0;
let failed = 0;
let testsContinued = false;

const testData = 'The quick brown fox jumps over the lazy dog. '.repeat(100);

// Test 1: createGzip factory function exists
console.log('1. Testing createGzip factory...');
try {
  if (typeof zlib.createGzip === 'function') {
    const gzip = zlib.createGzip();
    if (
      gzip &&
      typeof gzip.write === 'function' &&
      typeof gzip.pipe === 'function'
    ) {
      console.log('  ✓ createGzip returns a stream object');
      passed++;
    } else {
      console.error('  ❌ createGzip did not return a valid stream');
      failed++;
    }
  } else {
    console.error('  ❌ createGzip is not a function');
    failed++;
  }
} catch (e) {
  console.error('  ❌ Error:', e.message);
  failed++;
}

// Test 2: Stream compression round-trip
console.log('\n2. Testing stream compression round-trip...');
try {
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();
  const input = encoder.encode(testData);

  // Create readable stream from data
  let inputChunks = [];
  const chunkSize = 100;
  for (let i = 0; i < input.length; i += chunkSize) {
    inputChunks.push(input.slice(i, i + chunkSize));
  }

  const readable = new Readable({
    read() {
      if (inputChunks.length > 0) {
        this.push(inputChunks.shift());
      } else {
        this.push(null);
      }
    },
  });

  let compressed = [];
  const gzip = zlib.createGzip();

  gzip.on('data', (chunk) => {
    compressed.push(chunk);
  });

  gzip.on('end', () => {
    // Now decompress
    const compressedData = Buffer.concat(compressed);
    console.log('  Compressed size:', compressedData.length);

    let decompressed = [];
    const gunzip = zlib.createGunzip();

    gunzip.on('data', (chunk) => {
      decompressed.push(chunk);
    });

    gunzip.on('end', () => {
      const result = decoder.decode(Buffer.concat(decompressed));
      if (result === testData) {
        console.log('  ✓ Stream round-trip passed!');
        passed++;
      } else {
        console.error('  ❌ Data mismatch after round-trip');
        failed++;
      }
      continueTests();
    });

    gunzip.on('error', (err) => {
      console.error('  ❌ Gunzip error:', err.message);
      failed++;
      continueTests();
    });

    gunzip.write(compressedData);
    gunzip.end();
  });

  gzip.on('error', (err) => {
    console.error('  ❌ Gzip error:', err.message);
    failed++;
    continueTests();
  });

  readable.pipe(gzip);
} catch (e) {
  console.error('  ❌ Error:', e.message);
  failed++;
  continueTests();
}

function continueTests() {
  if (testsContinued) return;
  testsContinued = true;

  // Test 3: Check all factory functions exist
  console.log('\n3. Testing all stream factory functions...');
  const factories = [
    'createGzip',
    'createGunzip',
    'createDeflate',
    'createInflate',
    'createDeflateRaw',
    'createInflateRaw',
    'createUnzip',
  ];

  let allExist = true;
  for (const name of factories) {
    if (typeof zlib[name] !== 'function') {
      console.error(`  ❌ ${name} is not a function`);
      allExist = false;
    }
  }

  if (allExist) {
    console.log('  ✓ All factory functions exist');
    passed++;
  } else {
    failed++;
  }

  // Test 4: Check all stream classes exist
  console.log('\n4. Testing all stream classes...');
  const classes = [
    'Gzip',
    'Gunzip',
    'Deflate',
    'Inflate',
    'DeflateRaw',
    'InflateRaw',
    'Unzip',
  ];

  let allClassesExist = true;
  for (const name of classes) {
    if (typeof zlib[name] !== 'function') {
      console.error(`  ❌ ${name} class is not defined`);
      allClassesExist = false;
    }
  }

  if (allClassesExist) {
    console.log('  ✓ All stream classes exist');
    passed++;
  } else {
    failed++;
  }

  // Summary
  console.log('\n========================================');
  console.log('Total tests passed:', passed);
  console.log('Total tests failed:', failed);

  if (failed === 0) {
    console.log('\n✓ All stream tests passed!');
    process.exit(0);
  } else {
    console.log('\n❌ Some tests failed!');
    process.exit(1);
  }
}

// Fallback timeout
setTimeout(() => {
  if (!testsContinued) {
    console.error('\n⏱ Test timeout!');
    continueTests();
  }
}, 5000);
