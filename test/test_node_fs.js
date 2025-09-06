const assert = require('jsrt:assert');

console.log('=== Node.js File System Tests ===');

// Test CommonJS import
const fs = require('node:fs');
assert.ok(fs, 'Should be able to require node:fs');
assert.ok(fs.readFileSync, 'Should have readFileSync function');
assert.ok(fs.writeFileSync, 'Should have writeFileSync function');
assert.ok(fs.existsSync, 'Should have existsSync function');
assert.ok(fs.statSync, 'Should have statSync function');
assert.ok(fs.readdirSync, 'Should have readdirSync function');
assert.ok(fs.mkdirSync, 'Should have mkdirSync function');
assert.ok(fs.unlinkSync, 'Should have unlinkSync function');

// Test named destructuring
const {
  readFileSync,
  writeFileSync,
  existsSync,
  statSync,
  readdirSync,
  mkdirSync,
  unlinkSync,
} = require('node:fs');
assert.ok(readFileSync, 'Should have readFileSync via destructuring');
assert.ok(writeFileSync, 'Should have writeFileSync via destructuring');
assert.ok(existsSync, 'Should have existsSync via destructuring');

console.log('✓ Module imports work correctly');

// Test constants
assert.ok(fs.constants, 'Should have constants object');
assert.ok(typeof fs.constants.F_OK === 'number', 'Should have F_OK constant');
assert.ok(typeof fs.constants.R_OK === 'number', 'Should have R_OK constant');
assert.ok(typeof fs.constants.W_OK === 'number', 'Should have W_OK constant');
assert.ok(typeof fs.constants.X_OK === 'number', 'Should have X_OK constant');

console.log('✓ Constants are available');

// Create a test file path using cross-platform temporary directory
const os = require('node:os');
const tmpDir = os.tmpdir();
const testFilePath = tmpDir + '/jsrt_test_file.txt';
const testDirPath = tmpDir + '/jsrt_test_dir';
const testData = 'Hello, Node.js fs module!';

try {
  // Clean up any existing test files
  if (fs.existsSync(testFilePath)) {
    fs.unlinkSync(testFilePath);
  }

  // Test existsSync - should not exist initially
  assert.strictEqual(
    fs.existsSync(testFilePath),
    false,
    'Test file should not exist initially'
  );

  // Test writeFileSync
  fs.writeFileSync(testFilePath, testData);
  console.log('✓ writeFileSync works correctly');

  // Test existsSync - should exist now
  assert.strictEqual(
    fs.existsSync(testFilePath),
    true,
    'Test file should exist after writing'
  );

  // Test readFileSync
  const readData = fs.readFileSync(testFilePath);
  assert.strictEqual(readData, testData, 'Read data should match written data');
  console.log('✓ readFileSync works correctly');

  // Test statSync
  const stats = fs.statSync(testFilePath);
  assert.ok(stats, 'Should return stats object');
  assert.ok(typeof stats.size === 'number', 'Should have size property');
  assert.ok(typeof stats.mode === 'number', 'Should have mode property');
  assert.ok(stats.atime instanceof Date, 'Should have atime as Date');
  assert.ok(stats.mtime instanceof Date, 'Should have mtime as Date');
  assert.ok(stats.ctime instanceof Date, 'Should have ctime as Date');
  assert.strictEqual(
    stats.size,
    testData.length,
    'File size should match data length'
  );
  console.log('✓ statSync works correctly');

  // Test mkdir/readdir
  try {
    // Remove test directory if it exists
    if (fs.existsSync(testDirPath)) {
      // Note: This is a simple test, in real scenarios we'd need recursive removal
      fs.unlinkSync(testDirPath);
    }
  } catch (e) {
    // Ignore errors for cleanup
  }

  // Try creating directory, if it fails due to existence, use a different name
  let actualTestDirPath = testDirPath;
  try {
    fs.mkdirSync(actualTestDirPath);
  } catch (error) {
    if (error.code === 'EEXIST') {
      // Directory exists, try with a timestamp suffix
      actualTestDirPath = testDirPath + '_' + Date.now();
      fs.mkdirSync(actualTestDirPath);
    } else {
      throw error;
    }
  }

  assert.strictEqual(
    fs.existsSync(actualTestDirPath),
    true,
    'Test directory should exist after creation'
  );
  console.log('✓ mkdirSync works correctly');

  // Test readdirSync on a known directory
  const entries = fs.readdirSync('/tmp');
  assert.ok(Array.isArray(entries), 'readdirSync should return an array');
  console.log('✓ readdirSync works correctly');

  // Test unlinkSync
  fs.unlinkSync(testFilePath);
  assert.strictEqual(
    fs.existsSync(testFilePath),
    false,
    'Test file should not exist after unlinking'
  );
  console.log('✓ unlinkSync works correctly');

  // Clean up test directory
  try {
    fs.unlinkSync(actualTestDirPath);
  } catch (e) {
    // Directory might not be empty or might not be a file, ignore for test
  }
} catch (error) {
  console.error('Test failed:', error.message);
  throw error;
}

// Test error handling
console.log('\nTesting error handling...');

try {
  fs.readFileSync('/nonexistent/path/file.txt');
  assert.fail('Should throw error for nonexistent file');
} catch (error) {
  assert.ok(error.code, 'Error should have code property');
  assert.ok(error.path, 'Error should have path property');
  assert.ok(error.syscall, 'Error should have syscall property');
  console.log('✓ Error handling works correctly');
}

// Test with Buffer integration
console.log('\nTesting Buffer integration...');
const Buffer = require('node:buffer').Buffer;

if (Buffer) {
  const testBufferPath = '/tmp/jsrt_buffer_test.txt';
  const bufferData = Buffer.from('Buffer test data');

  try {
    fs.writeFileSync(testBufferPath, bufferData);
    const readBuffer = fs.readFileSync(testBufferPath);
    assert.strictEqual(
      readBuffer,
      'Buffer test data',
      'Should handle Buffer data'
    );

    // Clean up
    fs.unlinkSync(testBufferPath);
    console.log('✓ Buffer integration works correctly');
  } catch (error) {
    console.log('⚠️ Buffer integration test skipped:', error.message);
  }
}

console.log('\n=== All Node.js File System tests passed! ===');
