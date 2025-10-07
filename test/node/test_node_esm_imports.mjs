// Test ES module import syntax for node:stream and node:fs
import { Readable, Writable, PassThrough, Transform } from 'node:stream';
import { readFileSync, writeFileSync, existsSync } from 'node:fs';

// Test stream imports
const readable = new Readable();
const writable = new Writable();
const passthrough = new PassThrough();
const transform = new Transform();

// Test fs imports
import os from 'node:os';
const tmpDir = os.tmpdir();
const testFile = tmpDir + '/esm_test.txt';
const testData = 'ES module test data';

writeFileSync(testFile, testData);
const readData = readFileSync(testFile, { encoding: 'utf8' });
const exists = existsSync(testFile);

if (readData === testData && exists) {
} else {
  throw new Error('FS ES imports failed');
}

// Cleanup
import fs from 'node:fs';
fs.unlinkSync(testFile);
