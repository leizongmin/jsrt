// Test fs.promises API (Phase 3)
import { promises as fsPromises } from 'node:fs';
import * as fs from 'node:fs';

console.log('Testing fs.promises API...');

// Test 1: Verify promises namespace exists
if (!fsPromises) {
  console.error('FAIL: fs.promises namespace not found');
  process.exit(1);
}

if (!fs.promises) {
  console.error('FAIL: fs.promises not accessible via fs module');
  process.exit(1);
}

console.log('PASS: fs.promises namespace exists');

// Test 2: Verify fsPromises.open exists
if (typeof fsPromises.open !== 'function') {
  console.error('FAIL: fsPromises.open is not a function');
  process.exit(1);
}

console.log('PASS: fsPromises.open is a function');

// Test 3: Test fsPromises.open() with read mode
const testFile = 'target/tmp/test_promises.txt';

// Create test file first
fs.writeFileSync(testFile, 'Hello, Promises!');

(async () => {
  try {
    // Test opening a file
    const fh = await fsPromises.open(testFile, 'r');

    if (!fh) {
      console.error('FAIL: fsPromises.open did not return FileHandle');
      process.exit(1);
    }

    console.log('PASS: fsPromises.open returned FileHandle');

    // Test that FileHandle has close method
    if (typeof fh.close !== 'function') {
      console.error('FAIL: FileHandle.close is not a function');
      process.exit(1);
    }

    console.log('PASS: FileHandle has close method');

    // Test closing the file
    await fh.close();
    console.log('PASS: FileHandle.close() completed');

    // Test double close (should succeed)
    await fh.close();
    console.log('PASS: Double close handled correctly');

    // Test opening non-existent file (should reject)
    try {
      await fsPromises.open('nonexistent_file_xyz.txt', 'r');
      console.error('FAIL: Opening non-existent file should reject');
      process.exit(1);
    } catch (err) {
      if (err.code !== 'ENOENT') {
        console.error('FAIL: Expected ENOENT error, got:', err.code);
        process.exit(1);
      }
      console.log('PASS: Opening non-existent file rejected with ENOENT');
    }

    // Test opening with write mode
    const fh2 = await fsPromises.open(
      'target/tmp/test_promises_write.txt',
      'w'
    );
    await fh2.close();
    console.log('PASS: Opening with write mode works');

    // Test opening with append mode
    const fh3 = await fsPromises.open(
      'target/tmp/test_promises_append.txt',
      'a'
    );
    await fh3.close();
    console.log('PASS: Opening with append mode works');

    // Cleanup
    fs.unlinkSync(testFile);
    fs.unlinkSync('target/tmp/test_promises_write.txt');
    fs.unlinkSync('target/tmp/test_promises_append.txt');

    console.log('\nAll fs.promises tests passed!');
  } catch (err) {
    console.error('FAIL: Unexpected error:', err.message);
    console.error('Stack:', err.stack);
    process.exit(1);
  }
})();
