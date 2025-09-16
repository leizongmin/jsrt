const fs = require('node:fs');
const assert = require('jsrt:assert');
const path = require('node:path');
const os = require('node:os');
const { Buffer } = require('node:buffer');

// // console.log('=== Testing Advanced File Operations ===');

// Test setup
const testDir = path.join(os.tmpdir(), 'jsrt-advanced-tests');
const testFile = path.join(testDir, 'truncate-test.txt');
const testContent =
  'Hello World! This is a test file with some content for truncation testing.';

// Clean up any existing test directory
try {
  fs.rmdirSync(testDir);
} catch (e) {
  // Ignore if directory doesn't exist
}

// Setup test environment
fs.mkdirSync(testDir, { recursive: true });

console.log('Test setup completed');

// Test 1: truncateSync
console.log('\n1. Testing truncateSync...');
try {
  // Create a test file with content
  fs.writeFileSync(testFile, testContent);

  // Verify original file size
  const originalStats = fs.statSync(testFile);
  assert.strictEqual(
    originalStats.size,
    testContent.length,
    'Original file should have expected size'
  );

  // Truncate to 10 bytes
  fs.truncateSync(testFile, 10);

  // Verify truncated size
  const truncatedStats = fs.statSync(testFile);
  assert.strictEqual(
    truncatedStats.size,
    10,
    'File should be truncated to 10 bytes'
  );

  // Verify content is truncated correctly
  const truncatedContent = fs.readFileSync(testFile, 'utf8');
  assert.strictEqual(
    truncatedContent,
    testContent.substring(0, 10),
    'Content should be truncated correctly'
  );

  // Test truncating to 0 (default)
  fs.truncateSync(testFile);
  const emptyStats = fs.statSync(testFile);
  assert.strictEqual(
    emptyStats.size,
    0,
    'File should be truncated to 0 bytes when no length specified'
  );

  // console.log('✓ truncateSync test passed');
} catch (error) {
  console.error('✗ truncateSync test failed:', error.message);
  throw error;
}

// Test 2: ftruncateSync
console.log('\n2. Testing ftruncateSync...');
try {
  // Write content to test file
  fs.writeFileSync(testFile, testContent);

  // Open file descriptor
  const fd = fs.openSync(testFile, 'r+');

  try {
    // Truncate using file descriptor
    fs.ftruncateSync(fd, 15);

    // Verify truncated size
    const truncatedStats = fs.statSync(testFile);
    assert.strictEqual(
      truncatedStats.size,
      15,
      'File should be truncated to 15 bytes using fd'
    );

    // Verify content
    const truncatedContent = fs.readFileSync(testFile, 'utf8');
    assert.strictEqual(
      truncatedContent,
      testContent.substring(0, 15),
      'Content should be truncated correctly using fd'
    );

    // Test default truncation to 0
    fs.ftruncateSync(fd);
    const emptyStats = fs.statSync(testFile);
    assert.strictEqual(
      emptyStats.size,
      0,
      'File should be truncated to 0 bytes using fd when no length specified'
    );
  } finally {
    fs.closeSync(fd);
  }

  // console.log('✓ ftruncateSync test passed');
} catch (error) {
  console.error('✗ ftruncateSync test failed:', error.message);
  throw error;
}

// Test 3: mkdtempSync
console.log('\n3. Testing mkdtempSync...');
try {
  const tempPrefix = path.join(testDir, 'temp-');

  // Create temporary directory
  const tempDir = fs.mkdtempSync(tempPrefix);

  // Verify temporary directory exists
  assert.strictEqual(
    fs.existsSync(tempDir),
    true,
    'Temporary directory should exist'
  );

  // Verify it's a directory
  const stats = fs.statSync(tempDir);
  assert.strictEqual(
    stats.isDirectory(),
    true,
    'Created path should be a directory'
  );

  // Verify the directory name starts with the prefix
  assert.strictEqual(
    tempDir.startsWith(tempPrefix),
    true,
    'Temporary directory should start with prefix'
  );

  // Verify the directory name is longer than the prefix (has random suffix)
  assert.strictEqual(
    tempDir.length > tempPrefix.length,
    true,
    'Temporary directory should have random suffix'
  );

  // Test that multiple calls create different directories
  const tempDir2 = fs.mkdtempSync(tempPrefix);
  assert.notStrictEqual(
    tempDir,
    tempDir2,
    'Multiple mkdtemp calls should create different directories'
  );

  // Clean up temporary directories
  fs.rmdirSync(tempDir);
  fs.rmdirSync(tempDir2);

  // console.log('✓ mkdtempSync test passed');
} catch (error) {
  console.error('✗ mkdtempSync test failed:', error.message);
  throw error;
}

// Test 4: mkdtempSync with options
console.log('\n4. Testing mkdtempSync with options...');
try {
  const tempPrefix = path.join(testDir, 'temp-opts-');

  // Test with utf8 encoding (default)
  const tempDir1 = fs.mkdtempSync(tempPrefix, 'utf8');
  assert.strictEqual(
    typeof tempDir1,
    'string',
    'mkdtempSync with utf8 should return string'
  );

  // Test with options object
  const tempDir2 = fs.mkdtempSync(tempPrefix, { encoding: 'utf8' });
  assert.strictEqual(
    typeof tempDir2,
    'string',
    'mkdtempSync with options object should return string'
  );

  // Test with buffer encoding
  const tempDirBuffer = fs.mkdtempSync(tempPrefix, { encoding: 'buffer' });
  assert.strictEqual(
    Buffer.isBuffer(tempDirBuffer),
    true,
    'mkdtempSync with buffer encoding should return Buffer'
  );

  // Clean up
  fs.rmdirSync(tempDir1);
  fs.rmdirSync(tempDir2);
  fs.rmdirSync(tempDirBuffer.toString());

  // console.log('✓ mkdtempSync options test passed');
} catch (error) {
  console.error('✗ mkdtempSync options test failed:', error.message);
  throw error;
}

// Test 5: fsyncSync
console.log('\n5. Testing fsyncSync...');
try {
  // Create and write to a test file
  fs.writeFileSync(testFile, testContent);

  // Open file descriptor for writing
  const fd = fs.openSync(testFile, 'r+');

  try {
    // Write some data
    const newData = '\nAdditional data for fsync test';
    fs.writeSync(fd, newData);

    // Sync the file (should not throw)
    fs.fsyncSync(fd);

    // console.log('✓ fsyncSync test passed');
  } finally {
    fs.closeSync(fd);
  }
} catch (error) {
  console.error('✗ fsyncSync test failed:', error.message);
  throw error;
}

// Test 6: fdatasyncSync
console.log('\n6. Testing fdatasyncSync...');
try {
  // Create and write to a test file
  fs.writeFileSync(testFile, testContent);

  // Open file descriptor for writing
  const fd = fs.openSync(testFile, 'r+');

  try {
    // Write some data
    const newData = '\nAdditional data for fdatasync test';
    fs.writeSync(fd, newData);

    // Data sync the file (should not throw)
    fs.fdatasyncSync(fd);

    // console.log('✓ fdatasyncSync test passed');
  } finally {
    fs.closeSync(fd);
  }
} catch (error) {
  console.error('✗ fdatasyncSync test failed:', error.message);
  throw error;
}

// Test 7: Error handling
console.log('\n7. Testing error handling...');
try {
  // Test truncateSync with non-existent file
  const nonExistentFile = path.join(testDir, 'nonexistent.txt');

  try {
    fs.truncateSync(nonExistentFile, 10);
    assert.fail('truncateSync should throw error for non-existent file');
  } catch (error) {
    assert.strictEqual(
      error.code,
      'ENOENT',
      'truncateSync should throw ENOENT error'
    );
    assert.strictEqual(
      error.syscall,
      'truncate',
      'Error should have correct syscall'
    );
    // console.log('✓ truncateSync error handling test passed');
  }

  // Test ftruncateSync with invalid fd
  try {
    fs.ftruncateSync(999999, 10); // Invalid file descriptor
    assert.fail('ftruncateSync should throw error for invalid fd');
  } catch (error) {
    // Different platforms may return different error codes for invalid fd
    assert.strictEqual(
      typeof error.code === 'string',
      true,
      'ftruncateSync should throw error with code'
    );
    // console.log('✓ ftruncateSync error handling test passed');
  }

  // Test mkdtempSync with invalid prefix (directory doesn't exist)
  const invalidPrefix = path.join(testDir, 'nonexistent-dir', 'temp-');
  try {
    fs.mkdtempSync(invalidPrefix);
    assert.fail('mkdtempSync should throw error for invalid prefix');
  } catch (error) {
    assert.strictEqual(
      error.code,
      'ENOENT',
      'mkdtempSync should throw ENOENT error'
    );
    // console.log('✓ mkdtempSync error handling test passed');
  }
} catch (error) {
  console.error('✗ Error handling test failed:', error.message);
  throw error;
}

// Test 8: Range validation
console.log('\n8. Testing range validation...');
try {
  fs.writeFileSync(testFile, testContent);

  // Test negative length for truncateSync
  try {
    fs.truncateSync(testFile, -1);
    assert.fail('truncateSync should throw error for negative length');
  } catch (error) {
    assert.strictEqual(
      error.name,
      'RangeError',
      'truncateSync should throw RangeError for negative length'
    );
    // console.log('✓ truncateSync range validation test passed');
  }

  // Test negative length for ftruncateSync
  const fd = fs.openSync(testFile, 'r+');
  try {
    try {
      fs.ftruncateSync(fd, -1);
      assert.fail('ftruncateSync should throw error for negative length');
    } catch (error) {
      assert.strictEqual(
        error.name,
        'RangeError',
        'ftruncateSync should throw RangeError for negative length'
      );
      // console.log('✓ ftruncateSync range validation test passed');
    }
  } finally {
    fs.closeSync(fd);
  }
} catch (error) {
  console.error('✗ Range validation test failed:', error.message);
  throw error;
}

// Cleanup
try {
  fs.rmdirSync(testDir);
  console.log('\nTest cleanup completed');
} catch (e) {
  console.warn('Warning: Failed to clean up test directory:', e.message);
}

// console.log('\n=== All Advanced Operations tests passed! ===');
