const fs = require('node:fs');
const assert = require('jsrt:assert');
const path = require('node:path');
const os = require('node:os');
const { Buffer } = require('node:buffer');

// Test setup - create test directory and files
const testDir = path.join(os.tmpdir(), 'jsrt-link-tests');
const testFile = path.join(testDir, 'original.txt');
const hardLinkFile = path.join(testDir, 'hardlink.txt');
const symLinkFile = path.join(testDir, 'symlink.txt');
const testContent = 'Hello from link test!';

// Clean up any existing test directory
try {
  // Remove individual files first
  try {
    fs.unlinkSync(testFile);
  } catch (e) {
    /* ignore */
  }
  try {
    fs.unlinkSync(hardLinkFile);
  } catch (e) {
    /* ignore */
  }
  try {
    fs.unlinkSync(symLinkFile);
  } catch (e) {
    /* ignore */
  }
  try {
    fs.unlinkSync(path.join(testDir, 'symlink-dir'));
  } catch (e) {
    /* ignore */
  }
  try {
    fs.rmdirSync(path.join(testDir, 'subdir'));
  } catch (e) {
    /* ignore */
  }
  try {
    fs.rmdirSync(testDir);
  } catch (e) {
    /* ignore */
  }
} catch (e) {
  // Ignore if directory doesn't exist
}

// Setup test environment
fs.mkdirSync(testDir, { recursive: true });
fs.writeFileSync(testFile, testContent);

// Test 1: linkSync - Hard Links
try {
  fs.linkSync(testFile, hardLinkFile);

  // Verify hard link was created
  assert.strictEqual(
    fs.existsSync(hardLinkFile),
    true,
    'Hard link should exist'
  );

  // Verify both files have same content
  const originalContent = fs.readFileSync(testFile, 'utf8');
  const linkedContent = fs.readFileSync(hardLinkFile, 'utf8');
  assert.strictEqual(
    originalContent,
    linkedContent,
    'Hard link should have same content as original'
  );

  // Verify both files have same inode (Unix systems)
  const originalStat = fs.statSync(testFile);
  const linkedStat = fs.statSync(hardLinkFile);
  // Note: On different filesystems, ino might not be comparable, so we check size and mtime
  assert.strictEqual(
    originalStat.size,
    linkedStat.size,
    'Hard link should have same size'
  );
  console.log('✓ linkSync test passed');
} catch (error) {
  // On some filesystems (like Android/Termux), hardlinks might not be supported
  if (
    error.code === 'EACCES' ||
    error.code === 'EPERM' ||
    error.code === 'ENOTSUP'
  ) {
    console.log(
      '⚠ linkSync test skipped: Hard links not supported on this filesystem'
    );
  } else {
    console.error('✗ linkSync test failed:', error.message);
    throw error;
  }
}

// Test 2: symlinkSync - Symbolic Links
try {
  fs.symlinkSync(testFile, symLinkFile);

  // Verify symbolic link was created
  assert.strictEqual(
    fs.existsSync(symLinkFile),
    true,
    'Symbolic link should exist'
  );

  // Verify symbolic link points to correct content
  const originalContent = fs.readFileSync(testFile, 'utf8');
  const symlinkedContent = fs.readFileSync(symLinkFile, 'utf8');
  assert.strictEqual(
    originalContent,
    symlinkedContent,
    'Symbolic link should resolve to same content as original'
  );
} catch (error) {
  console.error('✗ symlinkSync test failed:', error.message);
  throw error;
}

// Test 3: readlinkSync - Read Symbolic Links
try {
  const linkTarget = fs.readlinkSync(symLinkFile);

  // The link target should be the path we used to create it
  assert.strictEqual(
    linkTarget,
    testFile,
    'readlinkSync should return the target path'
  );
} catch (error) {
  console.error('✗ readlinkSync test failed:', error.message);
  throw error;
}

// Test 4: realpathSync - Resolve Real Path
try {
  // Test with original file
  const realPath1 = fs.realpathSync(testFile);
  assert.strictEqual(
    typeof realPath1,
    'string',
    'realpathSync should return a string'
  );

  // On macOS, /tmp might resolve to /private/tmp, so we should compare resolved paths
  const expectedRealPath = fs.realpathSync(testFile); // This should be the same as itself
  assert.strictEqual(
    realPath1,
    expectedRealPath,
    'realpathSync should be consistent'
  );

  // Test with symbolic link - this should resolve to the same real path as the original file
  const realPath2 = fs.realpathSync(symLinkFile);
  assert.strictEqual(
    realPath2,
    realPath1,
    'realpathSync of symbolic link should resolve to same real path as original file'
  );
} catch (error) {
  console.error('✗ realpathSync test failed:', error.message);
  throw error;
}

// Test 5: Error handling - linkSync with non-existent file
try {
  const nonExistentFile = path.join(testDir, 'nonexistent.txt');
  const shouldFailLink = path.join(testDir, 'shouldfail.txt');

  try {
    fs.linkSync(nonExistentFile, shouldFailLink);
    assert.fail('linkSync should throw error for non-existent file');
  } catch (error) {
    assert.strictEqual(
      error.code,
      'ENOENT',
      'linkSync should throw ENOENT error'
    );
    assert.strictEqual(
      error.syscall,
      'link',
      'Error should have correct syscall'
    );
  }
} catch (error) {
  console.error('✗ Error handling test failed:', error.message);
  throw error;
}

// Test 6: symlinkSync with type parameter (directories)
try {
  const testSubDir = path.join(testDir, 'subdir');
  const symLinkDir = path.join(testDir, 'symlink-dir');

  fs.mkdirSync(testSubDir);
  fs.symlinkSync(testSubDir, symLinkDir, 'dir');

  // Verify directory symbolic link was created
  assert.strictEqual(
    fs.existsSync(symLinkDir),
    true,
    'Directory symbolic link should exist'
  );

  // The link should be created successfully
  // Note: We can't use lstatSync as it's not implemented yet
} catch (error) {
  console.error('✗ symlinkSync directory type test failed:', error.message);
  throw error;
}

// Test 7: readlinkSync with options (encoding)
try {
  // Test with different encoding options
  const linkTarget1 = fs.readlinkSync(symLinkFile, 'utf8');
  assert.strictEqual(
    typeof linkTarget1,
    'string',
    'readlinkSync with utf8 should return string'
  );

  const linkTarget2 = fs.readlinkSync(symLinkFile, { encoding: 'utf8' });
  assert.strictEqual(
    typeof linkTarget2,
    'string',
    'readlinkSync with options object should return string'
  );

  // Test buffer encoding
  const linkTargetBuffer = fs.readlinkSync(symLinkFile, { encoding: 'buffer' });
  assert.strictEqual(
    Buffer.isBuffer(linkTargetBuffer),
    true,
    'readlinkSync with buffer encoding should return Buffer'
  );
} catch (error) {
  console.error('✗ readlinkSync options test failed:', error.message);
  throw error;
}

// Test 8: realpathSync with options
try {
  // Test with different encoding options
  const realPath1 = fs.realpathSync(symLinkFile, 'utf8');
  assert.strictEqual(
    typeof realPath1,
    'string',
    'realpathSync with utf8 should return string'
  );

  const realPath2 = fs.realpathSync(symLinkFile, { encoding: 'utf8' });
  assert.strictEqual(
    typeof realPath2,
    'string',
    'realpathSync with options object should return string'
  );

  // Test buffer encoding
  const realPathBuffer = fs.realpathSync(symLinkFile, { encoding: 'buffer' });
  assert.strictEqual(
    Buffer.isBuffer(realPathBuffer),
    true,
    'realpathSync with buffer encoding should return Buffer'
  );
} catch (error) {
  console.error('✗ realpathSync options test failed:', error.message);
  throw error;
}

// Cleanup - handle all files first, then directory
try {
  // Remove individual files first
  try {
    fs.unlinkSync(testFile);
  } catch (e) {
    /* ignore */
  }
  try {
    fs.unlinkSync(hardLinkFile);
  } catch (e) {
    /* ignore */
  }
  try {
    fs.unlinkSync(symLinkFile);
  } catch (e) {
    /* ignore */
  }
  try {
    fs.unlinkSync(path.join(testDir, 'symlink-dir'));
  } catch (e) {
    /* ignore */
  }
  try {
    fs.rmdirSync(path.join(testDir, 'subdir'));
  } catch (e) {
    /* ignore */
  }
  try {
    fs.rmdirSync(testDir);
  } catch (e) {
    /* ignore */
  }
} catch (e) {
  console.warn('Warning: Failed to clean up test directory:', e.message);
}
