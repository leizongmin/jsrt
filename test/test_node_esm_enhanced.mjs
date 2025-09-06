// Test ES module imports for new Node.js compatibility modules
import querystring from 'node:querystring';
import process from 'node:process';

const assert = {
  ok: (condition, message) => {
    if (!condition) throw new Error(message);
  },
  strictEqual: (actual, expected, message) => {
    if (actual !== expected)
      throw new Error(`${message}: expected ${expected}, got ${actual}`);
  },
};

console.log(
  '=== ES Module Import Tests for Enhanced Node.js Compatibility ==='
);

// Test querystring ES module import
console.log('\n--- Testing node:querystring ES module import ---');
assert.ok(querystring, 'querystring should import');
assert.strictEqual(
  typeof querystring.parse,
  'function',
  'querystring.parse should be available'
);
assert.strictEqual(
  typeof querystring.stringify,
  'function',
  'querystring.stringify should be available'
);

const testData = querystring.parse('a=1&b=2&c=hello%20world');
assert.strictEqual(testData.a, '1', 'Parse should work correctly');
assert.strictEqual(testData.b, '2', 'Parse should work correctly');
assert.strictEqual(testData.c, 'hello world', 'URL decoding should work');

console.log('✓ ES module import for node:querystring works correctly');
console.log('✓ Parsed data:', testData);

// Test process ES module import
console.log('\n--- Testing node:process ES module import ---');
assert.ok(process, 'process should import');
assert.strictEqual(
  typeof process.hrtime,
  'function',
  'process.hrtime should be available'
);
assert.strictEqual(
  typeof process.uptime,
  'function',
  'process.uptime should be available'
);
assert.strictEqual(
  typeof process.pid,
  'number',
  'process.pid should be a number'
);

const hrStart = process.hrtime();
assert.ok(Array.isArray(hrStart), 'hrtime should return array');
assert.strictEqual(hrStart.length, 2, 'hrtime should return array of length 2');

console.log('✓ ES module import for node:process works correctly');
console.log('✓ Process PID:', process.pid);
console.log('✓ High-resolution time:', hrStart);

console.log('\n✅ All ES module import tests passed!');
console.log(
  '🎉 Enhanced Node.js compatibility with ES modules working perfectly!'
);
