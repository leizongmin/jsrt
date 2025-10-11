// Performance benchmark for zlib compression
const zlib = require('node:zlib');

console.log('zlib Performance Benchmark\n');
console.log('='.repeat(50));

// Test data of various sizes
const sizes = [
  { name: '1KB', size: 1024 },
  { name: '10KB', size: 10 * 1024 },
  { name: '100KB', size: 100 * 1024 },
];

const iterations = 10;

function generateTestData(size) {
  const chars =
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 \n\t';
  const encoder = new TextEncoder();
  let text = '';
  for (let i = 0; i < size; i++) {
    text += chars[Math.floor(Math.random() * chars.length)];
  }
  return encoder.encode(text);
}

function benchmark(name, fn) {
  const start = Date.now();
  for (let i = 0; i < iterations; i++) {
    fn();
  }
  const end = Date.now();
  const elapsed = end - start;
  const perOp = elapsed / iterations;
  return { elapsed, perOp };
}

// Benchmark results
const results = [];

for (const testCase of sizes) {
  console.log(
    `\nBenchmarking with ${testCase.name} data (${iterations} iterations)...`
  );

  const testData = generateTestData(testCase.size);
  console.log(`  Generated ${testData.length} bytes of test data`);

  // Benchmark gzipSync
  let compressed;
  const gzipResult = benchmark('gzipSync', () => {
    compressed = zlib.gzipSync(testData);
  });
  console.log(
    `  gzipSync: ${gzipResult.perOp.toFixed(2)}ms/op (total: ${gzipResult.elapsed}ms)`
  );
  console.log(
    `  Compression ratio: ${((compressed.length / testData.length) * 100).toFixed(1)}%`
  );

  // Benchmark gunzipSync
  const gunzipResult = benchmark('gunzipSync', () => {
    zlib.gunzipSync(compressed);
  });
  console.log(
    `  gunzipSync: ${gunzipResult.perOp.toFixed(2)}ms/op (total: ${gunzipResult.elapsed}ms)`
  );

  // Benchmark deflateSync
  let deflated;
  const deflateResult = benchmark('deflateSync', () => {
    deflated = zlib.deflateSync(testData);
  });
  console.log(
    `  deflateSync: ${deflateResult.perOp.toFixed(2)}ms/op (total: ${deflateResult.elapsed}ms)`
  );

  // Benchmark inflateSync
  const inflateResult = benchmark('inflateSync', () => {
    zlib.inflateSync(deflated);
  });
  console.log(
    `  inflateSync: ${inflateResult.perOp.toFixed(2)}ms/op (total: ${inflateResult.elapsed}ms)`
  );

  // Test with different compression levels
  console.log(`  Compression levels (level 1 vs 9):`);
  let level1, level9;
  const level1Result = benchmark('level 1', () => {
    level1 = zlib.gzipSync(testData, { level: 1 });
  });
  const level9Result = benchmark('level 9', () => {
    level9 = zlib.gzipSync(testData, { level: 9 });
  });
  console.log(
    `    Level 1: ${level1Result.perOp.toFixed(2)}ms/op, size: ${level1.length} bytes`
  );
  console.log(
    `    Level 9: ${level9Result.perOp.toFixed(2)}ms/op, size: ${level9.length} bytes`
  );
  console.log(
    `    Speed difference: ${((level9Result.perOp / level1Result.perOp - 1) * 100).toFixed(1)}% slower`
  );
  console.log(
    `    Size difference: ${((1 - level9.length / level1.length) * 100).toFixed(1)}% smaller`
  );

  results.push({
    size: testCase.name,
    bytes: testCase.size,
    gzip: gzipResult.perOp,
    gunzip: gunzipResult.perOp,
    deflate: deflateResult.perOp,
    inflate: inflateResult.perOp,
    compressionRatio: compressed.length / testData.length,
  });
}

// Summary
console.log('\n' + '='.repeat(50));
console.log('Summary:\n');
console.log(
  'Size     | gzipSync | gunzipSync | deflateSync | inflateSync | Ratio'
);
console.log('-'.repeat(75));
for (const result of results) {
  console.log(
    `${result.size.padEnd(8)} | ` +
      `${result.gzip.toFixed(2).padStart(8)}ms | ` +
      `${result.gunzip.toFixed(2).padStart(10)}ms | ` +
      `${result.deflate.toFixed(2).padStart(11)}ms | ` +
      `${result.inflate.toFixed(2).padStart(11)}ms | ` +
      `${(result.compressionRatio * 100).toFixed(1)}%`
  );
}

console.log('\nBenchmark complete!');

// Calculate throughput for largest size
if (results.length > 0) {
  const largest = results[results.length - 1];
  const gzipThroughput = largest.bytes / 1024 / 1024 / (largest.gzip / 1000);
  const gunzipThroughput =
    largest.bytes / 1024 / 1024 / (largest.gunzip / 1000);

  console.log(`\nThroughput for ${largest.size}:`);
  console.log(`  Compression: ${gzipThroughput.toFixed(2)} MB/s`);
  console.log(`  Decompression: ${gunzipThroughput.toFixed(2)} MB/s`);
}

console.log('\n✓ Benchmark test completed successfully!');
