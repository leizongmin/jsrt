// Test fs.promises API (Phase 3)
import { promises as fsPromises } from 'node:fs';
import * as fs from 'node:fs';

console.log('Testing fs.promises API (Phase 3)...\n');

let testsRun = 0;
let testsPassed = 0;

function testAssert(condition, message) {
  testsRun++;
  if (condition) {
    testsPassed++;
    console.log(`  ✓ ${message}`);
  } else {
    console.error(`  ✗ FAIL: ${message}`);
    process.exit(1);
  }
}

(async () => {
  try {
    console.log('=== Testing fsPromises namespace ===');
    testAssert(fsPromises !== undefined, 'fs.promises namespace exists');
    testAssert(
      fs.promises !== undefined,
      'fs.promises accessible via fs module'
    );
    testAssert(typeof fsPromises.open === 'function', 'fsPromises.open exists');

    console.log('\n=== Testing FileHandle.open/close ===');
    const testFile = 'target/tmp/test_promises.txt';
    fs.writeFileSync(testFile, 'Hello, Promises!', 'utf8');

    // Test basic open/close
    let fh = await fsPromises.open(testFile, 'r');
    testAssert(
      fh !== null && fh !== undefined,
      'fsPromises.open returns FileHandle'
    );
    testAssert(typeof fh.close === 'function', 'FileHandle has close method');

    await fh.close();
    testAssert(true, 'FileHandle.close() completes');

    // Test double close
    await fh.close();
    testAssert(true, 'Double close succeeds');

    // Test error handling
    try {
      await fsPromises.open('nonexistent_xyz.txt', 'r');
      testAssert(false, 'Opening non-existent file should reject');
    } catch (err) {
      testAssert(
        err.code === 'ENOENT',
        'Opening non-existent file rejects with ENOENT'
      );
    }

    console.log('\n=== Testing FileHandle.read() ===');
    fs.writeFileSync(testFile, 'Test content for reading', 'utf8');
    fh = await fsPromises.open(testFile, 'r');

    const buffer = new ArrayBuffer(100);
    const result = await fh.read(buffer, 0, 20, 0);
    testAssert(result.bytesRead > 0, 'FileHandle.read() reads data');
    testAssert(result.buffer !== undefined, 'FileHandle.read() returns buffer');
    await fh.close();

    console.log('\n=== Testing FileHandle.write() ===');
    fh = await fsPromises.open(testFile, 'w');
    const writeBuffer = new TextEncoder().encode(
      'Written by FileHandle'
    ).buffer;
    const writeResult = await fh.write(
      writeBuffer,
      0,
      writeBuffer.byteLength,
      0
    );
    testAssert(writeResult.bytesWritten > 0, 'FileHandle.write() writes data');
    testAssert(
      writeResult.bytesWritten === writeBuffer.byteLength,
      'FileHandle.write() writes all data'
    );
    await fh.close();

    // Verify written content
    const content = fs.readFileSync(testFile, 'utf8');
    testAssert(content === 'Written by FileHandle', 'Written content matches');

    console.log('\n=== Testing FileHandle.stat() ===');
    fh = await fsPromises.open(testFile, 'r');
    const stats = await fh.stat();
    testAssert(stats !== null, 'FileHandle.stat() returns stats');
    testAssert(typeof stats.size === 'number', 'Stats has size property');
    testAssert(typeof stats.isFile === 'function', 'Stats has isFile method');
    testAssert(stats.isFile(), 'Stats.isFile() returns true for file');
    await fh.close();

    console.log('\n=== Testing FileHandle.chmod() ===');
    fh = await fsPromises.open(testFile, 'r+');
    await fh.chmod(0o644);
    testAssert(true, 'FileHandle.chmod() succeeds');
    await fh.close();

    console.log('\n=== Testing FileHandle.truncate() ===');
    fh = await fsPromises.open(testFile, 'r+');
    await fh.truncate(5);
    const truncStats = await fh.stat();
    testAssert(
      truncStats.size === 5,
      'FileHandle.truncate() changes file size'
    );
    await fh.close();

    console.log('\n=== Testing FileHandle.sync() ===');
    fh = await fsPromises.open(testFile, 'r+');
    await fh.sync();
    testAssert(true, 'FileHandle.sync() succeeds');
    await fh.close();

    console.log('\n=== Testing FileHandle.datasync() ===');
    fh = await fsPromises.open(testFile, 'r+');
    await fh.datasync();
    testAssert(true, 'FileHandle.datasync() succeeds');
    await fh.close();

    console.log('\n=== Testing fsPromises wrapper methods ===');

    // Test fsPromises.stat()
    testAssert(typeof fsPromises.stat === 'function', 'fsPromises.stat exists');
    const promiseStats = await fsPromises.stat(testFile);
    testAssert(promiseStats !== null, 'fsPromises.stat() returns stats');
    testAssert(promiseStats.isFile(), 'fsPromises.stat() returns valid stats');

    // Test fsPromises.lstat()
    testAssert(
      typeof fsPromises.lstat === 'function',
      'fsPromises.lstat exists'
    );
    const lstatStats = await fsPromises.lstat(testFile);
    testAssert(lstatStats !== null, 'fsPromises.lstat() returns stats');

    // Test fsPromises.mkdir/rmdir()
    testAssert(
      typeof fsPromises.mkdir === 'function',
      'fsPromises.mkdir exists'
    );
    testAssert(
      typeof fsPromises.rmdir === 'function',
      'fsPromises.rmdir exists'
    );
    const testDir = 'target/tmp/test_promises_dir';
    await fsPromises.mkdir(testDir);
    testAssert(fs.existsSync(testDir), 'fsPromises.mkdir() creates directory');
    await fsPromises.rmdir(testDir);
    testAssert(!fs.existsSync(testDir), 'fsPromises.rmdir() removes directory');

    // Test fsPromises.unlink()
    testAssert(
      typeof fsPromises.unlink === 'function',
      'fsPromises.unlink exists'
    );
    const unlinkTest = 'target/tmp/test_promises_unlink.txt';
    fs.writeFileSync(unlinkTest, 'test');
    await fsPromises.unlink(unlinkTest);
    testAssert(!fs.existsSync(unlinkTest), 'fsPromises.unlink() removes file');

    // Test fsPromises.rename()
    testAssert(
      typeof fsPromises.rename === 'function',
      'fsPromises.rename exists'
    );
    const renameFrom = 'target/tmp/test_promises_rename_from.txt';
    const renameTo = 'target/tmp/test_promises_rename_to.txt';
    fs.writeFileSync(renameFrom, 'rename test');
    await fsPromises.rename(renameFrom, renameTo);
    testAssert(
      !fs.existsSync(renameFrom),
      'fsPromises.rename() removes old file'
    );
    testAssert(fs.existsSync(renameTo), 'fsPromises.rename() creates new file');
    fs.unlinkSync(renameTo);

    // Test fsPromises.readlink()
    testAssert(
      typeof fsPromises.readlink === 'function',
      'fsPromises.readlink exists'
    );
    const linkTarget = 'target/tmp/test_promises_link_target.txt';
    const linkPath = 'target/tmp/test_promises_link.txt';
    fs.writeFileSync(linkTarget, 'link test');
    try {
      fs.symlinkSync(linkTarget, linkPath);
      const linkedPath = await fsPromises.readlink(linkPath);
      testAssert(
        linkedPath === linkTarget,
        'fsPromises.readlink() reads symlink'
      );
      fs.unlinkSync(linkPath);
    } catch (err) {
      // Symlinks may not be available on all platforms
      console.log('  ! Skipping readlink test (symlinks not available)');
    }
    fs.unlinkSync(linkTarget);

    // Cleanup
    fs.unlinkSync(testFile);

    console.log(`\n=== Summary ===`);
    console.log(`Tests run: ${testsRun}`);
    console.log(`Tests passed: ${testsPassed}`);
    console.log(`Pass rate: 100%`);
    console.log('\nAll fs.promises Phase 3 tests passed! ✓');
  } catch (err) {
    console.error('\n✗ FAIL: Unexpected error:', err.message);
    console.error('Stack:', err.stack);
    process.exit(1);
  }
})();
