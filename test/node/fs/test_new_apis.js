const assert = require('jsrt:assert');
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');

// Get temporary directory and create test directory path
const tmpDir = os.tmpdir();
const testDir = path.join(tmpDir, 'test_fs_temp');
const testFile = path.join(testDir, 'test.txt');
const testFile2 = path.join(testDir, 'test2.txt');
const testFile3 = path.join(testDir, 'test3.txt');
const testSubDir = path.join(testDir, 'subdir');

// Clean up and create test directory
try {
  if (fs.existsSync(testDir)) {
    // Remove any existing files first
    try {
      const files = fs.readdirSync(testDir);
      for (const file of files) {
        const filePath = path.join(testDir, file);
        const stat = fs.statSync(filePath);
        if (stat.isDirectory()) {
          fs.rmdirSync(filePath);
        } else {
          fs.unlinkSync(filePath);
        }
      }
      fs.rmdirSync(testDir);
    } catch (e) {
      // Ignore cleanup errors
    }
  }
} catch (e) {
  // Ignore cleanup errors
}

// Cleanup function
function cleanup() {
  try {
    if (fs.existsSync(testFile)) fs.unlinkSync(testFile);
    if (fs.existsSync(testFile2)) fs.unlinkSync(testFile2);
    if (fs.existsSync(testFile3)) fs.unlinkSync(testFile3);
    if (fs.existsSync(testSubDir)) fs.rmdirSync(testSubDir);
    if (fs.existsSync(testDir)) fs.rmdirSync(testDir);
  } catch (e) {
    // Ignore cleanup errors
  }
}

// Create test directory
fs.mkdirSync(testDir);

// Test 1: appendFileSync and appendFile
// Create initial file
fs.writeFileSync(testFile, 'Hello');

// Test appendFileSync
fs.appendFileSync(testFile, ' World');
const content1 = fs.readFileSync(testFile, 'utf8');
assert.strictEqual(
  content1,
  'Hello World',
  'appendFileSync should append text'
);

// Test 2: copyFileSync
fs.copyFileSync(testFile, testFile2);
const originalContent = fs.readFileSync(testFile, 'utf8');
const copiedContent = fs.readFileSync(testFile2, 'utf8');
assert.strictEqual(
  originalContent,
  copiedContent,
  'copyFileSync should copy file content'
);

// Test 3: renameSync
const renamedFile = path.join(testDir, 'renamed.txt');
fs.renameSync(testFile2, renamedFile);
assert.ok(
  !fs.existsSync(testFile2),
  'Original file should not exist after rename'
);
assert.ok(fs.existsSync(renamedFile), 'Renamed file should exist');

// Test 4: accessSync
try {
  fs.accessSync(testFile);
  // accessSync: file exists check passed
} catch (e) {
  assert.fail('accessSync should not throw for existing file');
}

try {
  fs.accessSync('nonexistent.txt');
  assert.fail('accessSync should throw for non-existent file');
} catch (e) {
  assert.ok(
    e.code === 'ENOENT',
    'accessSync should throw ENOENT for non-existent file'
  );
}

// Test 5: rmdirSync
fs.mkdirSync(testSubDir);
assert.ok(fs.existsSync(testSubDir), 'Test subdirectory should exist');
fs.rmdirSync(testSubDir);
assert.ok(
  !fs.existsSync(testSubDir),
  'Subdirectory should be removed by rmdirSync'
);

// Test 6: Error handling
try {
  fs.copyFileSync('nonexistent.txt', 'dest.txt');
  assert.fail('copyFileSync should throw for non-existent source');
} catch (e) {
  assert.strictEqual(
    e.code,
    'ENOENT',
    'copyFileSync should throw ENOENT for non-existent source'
  );
}

try {
  fs.renameSync('nonexistent.txt', 'dest.txt');
  assert.fail('renameSync should throw for non-existent source');
} catch (e) {
  assert.strictEqual(
    e.code,
    'ENOENT',
    'renameSync should throw ENOENT for non-existent source'
  );
}

try {
  fs.rmdirSync('nonexistent_dir');
  assert.fail('rmdirSync should throw for non-existent directory');
} catch (e) {
  assert.strictEqual(
    e.code,
    'ENOENT',
    'rmdirSync should throw ENOENT for non-existent directory'
  );
}

// Test async operations using callback chaining
// Test appendFile (async)
fs.appendFile(testFile, '!', (err) => {
  assert.strictEqual(err, null, 'appendFile should not error');
  const content2 = fs.readFileSync(testFile, 'utf8');
  assert.strictEqual(content2, 'Hello World!', 'appendFile should append text');

  // Test copyFile (async)
  fs.copyFile(testFile, testFile3, (err) => {
    assert.strictEqual(err, null, 'copyFile should not error');
    const copiedContent2 = fs.readFileSync(testFile3, 'utf8');
    assert.strictEqual(
      originalContent + '!',
      copiedContent2,
      'copyFile should copy file content'
    );

    // Test rename (async)
    const renamedFile2 = path.join(testDir, 'renamed2.txt');
    fs.rename(renamedFile, renamedFile2, (err) => {
      assert.strictEqual(err, null, 'rename should not error');
      assert.ok(
        !fs.existsSync(renamedFile),
        'Original file should not exist after async rename'
      );
      assert.ok(
        fs.existsSync(renamedFile2),
        'Renamed file should exist after async rename'
      );

      // Test access (async) - file exists
      fs.access(testFile, (err) => {
        assert.strictEqual(
          err,
          null,
          'access should not error for existing file'
        );

        // Test access (async) - file doesn't exist
        fs.access('nonexistent.txt', (err) => {
          assert.ok(err !== null, 'access should error for non-existent file');
          assert.strictEqual(
            err.code,
            'ENOENT',
            'access should return ENOENT for non-existent file'
          );

          // Test rmdir (async)
          const testSubDir2 = path.join(testDir, 'subdir2');
          fs.mkdirSync(testSubDir2);
          fs.rmdir(testSubDir2, (err) => {
            assert.strictEqual(err, null, 'rmdir should not error');
            assert.ok(
              !fs.existsSync(testSubDir2),
              'Subdirectory should be removed by rmdir'
            );

            // Cleanup
            cleanup();
          });
        });
      });
    });
  });
});
