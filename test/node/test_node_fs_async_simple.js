/**
 * Simple test for refactored async fs APIs (appendFile, copyFile)
 */

import * as fs from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const testDir = tmpdir();
let passed = 0;
let failed = 0;

console.log('Testing appendFile async...');

// Test 1: appendFile
const testFile1 = join(testDir, 'test-append.txt');
try {
  fs.unlinkSync(testFile1);
} catch (e) {}

fs.appendFile(testFile1, 'Hello', (err) => {
  if (err) {
    console.log('FAIL: appendFile - ' + err.message);
    failed++;
  } else {
    const content = fs.readFileSync(testFile1, 'utf8');
    if (content === 'Hello') {
      console.log('PASS: appendFile creates file');
      passed++;
    } else {
      console.log('FAIL: appendFile content mismatch');
      failed++;
    }
    try {
      fs.unlinkSync(testFile1);
    } catch (e) {}
  }
});

// Test 2: copyFile
console.log('Testing copyFile async...');
const srcFile = join(testDir, 'test-copy-src.txt');
const destFile = join(testDir, 'test-copy-dest.txt');

fs.writeFileSync(srcFile, 'Copy test');
try {
  fs.unlinkSync(destFile);
} catch (e) {}

fs.copyFile(srcFile, destFile, (err) => {
  if (err) {
    console.log('FAIL: copyFile - ' + err.message);
    failed++;
  } else {
    const content = fs.readFileSync(destFile, 'utf8');
    if (content === 'Copy test') {
      console.log('PASS: copyFile copies content');
      passed++;
    } else {
      console.log('FAIL: copyFile content mismatch');
      failed++;
    }
    try {
      fs.unlinkSync(srcFile);
      fs.unlinkSync(destFile);
    } catch (e) {}
  }
});

// Print results after a short delay
setTimeout(() => {
  console.log(`\nResults: ${passed} passed, ${failed} failed`);
  process.exit(failed > 0 ? 1 : 0);
}, 200);
