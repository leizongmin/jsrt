// Test ES module import syntax for node:stream and node:fs
import { Readable, Writable, PassThrough, Transform } from 'node:stream';
import { readFileSync, writeFileSync, existsSync } from 'node:fs';

console.log('=== ES Module Import Tests ===');

// Test stream imports
console.log('Testing stream ES imports...');
const readable = new Readable();
const writable = new Writable();
const passthrough = new PassThrough();
const transform = new Transform();

console.log('✓ Stream ES imports work correctly');

// Test fs imports
console.log('Testing fs ES imports...');
import os from 'node:os';
const tmpDir = os.tmpdir();
const testFile = tmpDir + '/esm_test.txt';
const testData = 'ES module test data';

writeFileSync(testFile, testData);
const readData = readFileSync(testFile);
const exists = existsSync(testFile);

if (readData === testData && exists) {
  console.log('✓ FS ES imports work correctly');
} else {
  throw new Error('FS ES imports failed');
}

// Cleanup
import fs from 'node:fs';
fs.unlinkSync(testFile);

console.log('✓ All ES Module tests passed!');
