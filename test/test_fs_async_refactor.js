/**
 * Test suite for refactored async fs APIs (appendFile, copyFile)
 * These APIs now use true libuv async I/O instead of blocking fopen
 */

import * as fs from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const testDir = tmpdir();
let testCount = 0;
let passCount = 0;

function test(name, fn) {
  testCount++;
  try {
    fn();
    passCount++;
  } catch (err) {
    console.log(`FAIL: ${name}`);
    console.log(`  ${err.message}`);
    if (err.stack) {
      console.log(err.stack);
    }
  }
}

function assert(condition, message) {
  if (!condition) {
    throw new Error(message || 'Assertion failed');
  }
}

// Test appendFile async
test('appendFile creates file if not exists', (done) => {
  const testFile = join(testDir, 'test-append-new.txt');

  // Clean up if exists
  try {
    fs.unlinkSync(testFile);
  } catch (e) {}

  fs.appendFile(testFile, 'Hello', (err) => {
    assert(!err, `appendFile should not error: ${err}`);
    const content = fs.readFileSync(testFile, 'utf8');
    assert(content === 'Hello', `Content should be "Hello", got "${content}"`);
    fs.unlinkSync(testFile);
  });
});

test('appendFile appends to existing file', (done) => {
  const testFile = join(testDir, 'test-append-existing.txt');

  // Create initial file
  fs.writeFileSync(testFile, 'Hello');

  fs.appendFile(testFile, ' World', (err) => {
    assert(!err, `appendFile should not error: ${err}`);
    const content = fs.readFileSync(testFile, 'utf8');
    assert(
      content === 'Hello World',
      `Content should be "Hello World", got "${content}"`
    );
    fs.unlinkSync(testFile);
  });
});

test('appendFile handles errors (nonexistent directory)', (done) => {
  const testFile = join(testDir, 'nonexistent-dir', 'test.txt');

  fs.appendFile(testFile, 'data', (err) => {
    assert(err, 'appendFile should error for nonexistent directory');
    assert(
      err.code === 'ENOENT',
      `Error code should be ENOENT, got ${err.code}`
    );
  });
});

// Test copyFile async
test('copyFile copies file successfully', (done) => {
  const srcFile = join(testDir, 'test-copy-src.txt');
  const destFile = join(testDir, 'test-copy-dest.txt');

  // Create source file
  fs.writeFileSync(srcFile, 'Test content for copy');

  // Clean up dest if exists
  try {
    fs.unlinkSync(destFile);
  } catch (e) {}

  fs.copyFile(srcFile, destFile, (err) => {
    assert(!err, `copyFile should not error: ${err}`);
    const content = fs.readFileSync(destFile, 'utf8');
    assert(
      content === 'Test content for copy',
      `Copied content mismatch: "${content}"`
    );

    // Cleanup
    fs.unlinkSync(srcFile);
    fs.unlinkSync(destFile);
  });
});

test('copyFile overwrites existing destination', (done) => {
  const srcFile = join(testDir, 'test-copy-src2.txt');
  const destFile = join(testDir, 'test-copy-dest2.txt');

  // Create source and dest files
  fs.writeFileSync(srcFile, 'New content');
  fs.writeFileSync(destFile, 'Old content');

  fs.copyFile(srcFile, destFile, (err) => {
    assert(!err, `copyFile should not error: ${err}`);
    const content = fs.readFileSync(destFile, 'utf8');
    assert(
      content === 'New content',
      `Dest should have new content: "${content}"`
    );

    // Cleanup
    fs.unlinkSync(srcFile);
    fs.unlinkSync(destFile);
  });
});

test('copyFile handles large files (8KB+ for chunked copy)', (done) => {
  const srcFile = join(testDir, 'test-copy-large.txt');
  const destFile = join(testDir, 'test-copy-large-dest.txt');

  // Create 16KB file (2x chunk size)
  const largeContent = 'x'.repeat(16 * 1024);
  fs.writeFileSync(srcFile, largeContent);

  fs.copyFile(srcFile, destFile, (err) => {
    assert(!err, `copyFile should not error on large file: ${err}`);
    const content = fs.readFileSync(destFile, 'utf8');
    assert(
      content.length === largeContent.length,
      `Size mismatch: ${content.length} vs ${largeContent.length}`
    );
    assert(content === largeContent, 'Content should match exactly');

    // Cleanup
    fs.unlinkSync(srcFile);
    fs.unlinkSync(destFile);
  });
});

test('copyFile handles errors (source not found)', (done) => {
  const srcFile = join(testDir, 'nonexistent-src.txt');
  const destFile = join(testDir, 'test-dest.txt');

  fs.copyFile(srcFile, destFile, (err) => {
    assert(err, 'copyFile should error when source not found');
    assert(
      err.code === 'ENOENT',
      `Error code should be ENOENT, got ${err.code}`
    );
  });
});

test('copyFile handles errors (invalid destination directory)', (done) => {
  const srcFile = join(testDir, 'test-src.txt');
  const destFile = join(testDir, 'nonexistent-dir', 'test-dest.txt');

  fs.writeFileSync(srcFile, 'test');

  fs.copyFile(srcFile, destFile, (err) => {
    assert(err, 'copyFile should error when dest directory not found');
    assert(
      err.code === 'ENOENT',
      `Error code should be ENOENT, got ${err.code}`
    );
    fs.unlinkSync(srcFile);
  });
});

// Wait for all async callbacks to complete
setTimeout(() => {
  console.log(`\nAsync fs refactor tests: ${passCount}/${testCount} passed`);
  if (passCount < testCount) {
    process.exit(1);
  }
}, 100);
