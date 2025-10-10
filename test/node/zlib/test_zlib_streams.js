// Test zlib streaming compression
// Stream classes are now implemented in C and exported from node:zlib
const zlib = require('node:zlib');
const streams = zlib; // Stream factories are now part of zlib module
const { Readable, Writable, pipeline } = require('node:stream');

console.log('Testing zlib stream classes...\n');

let passed = 0;
let failed = 0;
let testsContinued = false;

const testData = 'The quick brown fox jumps over the lazy dog. '.repeat(100);

// Test 1: createGzip factory function exists
console.log('1. Testing createGzip factory...');
try {
  if (typeof streams.createGzip === 'function') {
    const gzip = streams.createGzip();
    if (
      gzip &&
      typeof gzip.write === 'function' &&
      typeof gzip._transform === 'function'
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
  const gzip = streams.createGzip();

  gzip.on('data', (chunk) => {
    compressed.push(chunk);
  });

  gzip.on('end', () => {
    // Now decompress
    const compressedData = Buffer.concat(compressed);
    console.log('  Compressed size:', compressedData.length);

    let decompressed = [];
    const gunzip = streams.createGunzip();

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

  // Manually pipe by using events (in case pipe() is not available)
  readable.on('data', (chunk) => gzip.write(chunk));
  readable.on('end', () => gzip.end());
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
    if (typeof streams[name] !== 'function') {
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

  // Test 4: Check stream instances can be created with options
  console.log('\n4. Testing stream creation with options...');
  try {
    const gzip = streams.createGzip({ level: 9 });
    const gunzip = streams.createGunzip();
    const deflate = streams.createDeflate({ level: 6 });
    const inflate = streams.createInflate();

    if (gzip && gunzip && deflate && inflate) {
      console.log('  ✓ All streams can be created with options');
      passed++;
    } else {
      console.error('  ❌ Failed to create streams with options');
      failed++;
    }
  } catch (e) {
    console.error('  ❌ Error creating streams:', e.message);
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
