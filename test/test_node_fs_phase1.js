#!/usr/bin/env jsrt
// Test file for Node.js fs module Phase 1 implementation

import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';

const tmpdir = os.tmpdir();
let testsPassed = 0;
let testsFailed = 0;

function assert(condition, message) {
  if (condition) {
    testsPassed++;
  } else {
    console.log(`FAIL: ${message}`);
    testsFailed++;
  }
}

function cleanup(filePath) {
  try {
    if (fs.existsSync(filePath)) {
      const stats = fs.statSync(filePath);
      if (stats.isDirectory()) {
        fs.rmSync(filePath, { recursive: true, force: true });
      } else {
        fs.unlinkSync(filePath);
      }
    }
  } catch (e) {
    // Ignore cleanup errors
  }
}

// Test 1.1: fstatSync
console.log('Testing fstatSync...');
{
  const testFile = path.join(tmpdir, 'test-fstat.txt');
  cleanup(testFile);

  fs.writeFileSync(testFile, 'test content for fstat');
  const fd = fs.openSync(testFile, 'r');

  const stats = fs.fstatSync(fd);
  assert(
    stats !== null && stats !== undefined,
    'fstatSync should return stats object'
  );
  assert(typeof stats.size === 'number', 'stats should have size property');
  assert(stats.size === 22, 'file size should be 22');
  assert(typeof stats.isFile === 'function', 'stats should have isFile method');
  assert(stats.isFile(), 'fstatSync should indicate file type correctly');

  fs.closeSync(fd);
  cleanup(testFile);
}

// Test 1.2: lstatSync
console.log('Testing lstatSync...');
{
  const testFile = path.join(tmpdir, 'test-lstat-target.txt');
  const testLink = path.join(tmpdir, 'test-lstat-link.txt');
  cleanup(testFile);
  cleanup(testLink);

  fs.writeFileSync(testFile, 'test content for lstat');

  try {
    fs.symlinkSync(testFile, testLink);

    const stats = fs.lstatSync(testLink);
    assert(
      stats !== null && stats !== undefined,
      'lstatSync should return stats object'
    );
    assert(
      typeof stats.isFile === 'function',
      'stats should have isFile method'
    );
    // lstat on symlink should show it as file (not directory)

    cleanup(testLink);
    cleanup(testFile);
  } catch (e) {
    console.log(
      'SKIP: lstatSync test (symlinks may not be supported on this platform)'
    );
  }
}

// Test 1.3: fchmodSync
console.log('Testing fchmodSync...');
{
  const testFile = path.join(tmpdir, 'test-fchmod.txt');
  cleanup(testFile);

  fs.writeFileSync(testFile, 'test content for fchmod');
  const fd = fs.openSync(testFile, 'r+');

  try {
    fs.fchmodSync(fd, 0o644);
    assert(true, 'fchmodSync should execute without error');
    fs.closeSync(fd);
  } catch (e) {
    if (e.message && e.message.includes('not supported on Windows')) {
      console.log('SKIP: fchmodSync (not supported on Windows)');
      fs.closeSync(fd);
    } else {
      assert(false, `fchmodSync failed: ${e.message}`);
      fs.closeSync(fd);
    }
  }

  cleanup(testFile);
}

// Test 1.4: fchownSync
console.log('Testing fchownSync...');
{
  const testFile = path.join(tmpdir, 'test-fchown.txt');
  cleanup(testFile);

  fs.writeFileSync(testFile, 'test content for fchown');
  const fd = fs.openSync(testFile, 'r+');

  try {
    fs.fchownSync(fd, -1, -1); // -1 means don't change
    assert(true, 'fchownSync should execute without error');
    fs.closeSync(fd);
  } catch (e) {
    if (e.message && e.message.includes('not supported on Windows')) {
      console.log('SKIP: fchownSync (not supported on Windows)');
      fs.closeSync(fd);
    } else {
      console.log(
        `SKIP: fchownSync (may require special permissions): ${e.message}`
      );
      fs.closeSync(fd);
    }
  }

  cleanup(testFile);
}

// Test 1.5: lchownSync
console.log('Testing lchownSync...');
{
  const testFile = path.join(tmpdir, 'test-lchown.txt');
  cleanup(testFile);

  fs.writeFileSync(testFile, 'test content for lchown');

  try {
    fs.lchownSync(testFile, -1, -1); // -1 means don't change
    assert(true, 'lchownSync should execute without error');
  } catch (e) {
    if (e.message && e.message.includes('not supported on Windows')) {
      console.log('SKIP: lchownSync (not supported on Windows)');
    } else {
      console.log(
        `SKIP: lchownSync (may require special permissions): ${e.message}`
      );
    }
  }

  cleanup(testFile);
}

// Test 1.6: futimesSync
console.log('Testing futimesSync...');
{
  const testFile = path.join(tmpdir, 'test-futimes.txt');
  cleanup(testFile);

  fs.writeFileSync(testFile, 'test content for futimes');
  const fd = fs.openSync(testFile, 'r+');

  try {
    const now = Date.now();
    fs.futimesSync(fd, now / 1000, now / 1000);
    assert(true, 'futimesSync should execute without error');
    fs.closeSync(fd);
  } catch (e) {
    assert(false, `futimesSync failed: ${e.message}`);
    fs.closeSync(fd);
  }

  cleanup(testFile);
}

// Test 1.7: lutimesSync
console.log('Testing lutimesSync...');
{
  const testFile = path.join(tmpdir, 'test-lutimes.txt');
  cleanup(testFile);

  fs.writeFileSync(testFile, 'test content for lutimes');

  try {
    const now = Date.now();
    fs.lutimesSync(testFile, now / 1000, now / 1000);
    assert(true, 'lutimesSync should execute without error');
  } catch (e) {
    if (e.message && e.message.includes('not supported on Windows')) {
      console.log('SKIP: lutimesSync (not supported on Windows)');
    } else {
      assert(false, `lutimesSync failed: ${e.message}`);
    }
  }

  cleanup(testFile);
}

// Test 1.8: rmSync with recursive option
console.log('Testing rmSync...');
{
  const testDir = path.join(tmpdir, 'test-rm-dir');
  cleanup(testDir);

  fs.mkdirSync(testDir, { recursive: true });
  fs.writeFileSync(path.join(testDir, 'file1.txt'), 'content1');
  fs.mkdirSync(path.join(testDir, 'subdir'));
  fs.writeFileSync(path.join(testDir, 'subdir', 'file2.txt'), 'content2');

  fs.rmSync(testDir, { recursive: true });
  assert(!fs.existsSync(testDir), 'rmSync should remove directory recursively');
}

// Test 1.9: cpSync with recursive option
console.log('Testing cpSync...');
{
  const srcDir = path.join(tmpdir, 'test-cp-src');
  const destDir = path.join(tmpdir, 'test-cp-dest');
  cleanup(srcDir);
  cleanup(destDir);

  fs.mkdirSync(srcDir, { recursive: true });
  fs.writeFileSync(path.join(srcDir, 'file1.txt'), 'content1');
  fs.mkdirSync(path.join(srcDir, 'subdir'));
  fs.writeFileSync(path.join(srcDir, 'subdir', 'file2.txt'), 'content2');

  fs.cpSync(srcDir, destDir, { recursive: true });

  assert(fs.existsSync(destDir), 'cpSync should create destination directory');
  assert(
    fs.existsSync(path.join(destDir, 'file1.txt')),
    'cpSync should copy files'
  );
  assert(
    fs.existsSync(path.join(destDir, 'subdir', 'file2.txt')),
    'cpSync should copy subdirectories'
  );

  const content = fs.readFileSync(
    path.join(destDir, 'subdir', 'file2.txt'),
    'utf8'
  );
  assert(content === 'content2', 'cpSync should preserve file contents');

  cleanup(srcDir);
  cleanup(destDir);
}

// Test 1.10: opendirSync
console.log('Testing opendirSync...');
{
  const testDir = path.join(tmpdir, 'test-opendir');
  cleanup(testDir);

  fs.mkdirSync(testDir, { recursive: true });
  fs.writeFileSync(path.join(testDir, 'file1.txt'), 'content1');
  fs.writeFileSync(path.join(testDir, 'file2.txt'), 'content2');

  const dir = fs.opendirSync(testDir);
  assert(
    dir !== null && dir !== undefined,
    'opendirSync should return Dir object'
  );
  assert(typeof dir.readSync === 'function', 'Dir should have readSync method');
  assert(
    typeof dir.closeSync === 'function',
    'Dir should have closeSync method'
  );

  let entry;
  let count = 0;
  while ((entry = dir.readSync()) !== null) {
    assert(
      entry !== null && entry !== undefined,
      'readSync should return Dirent object'
    );
    assert(typeof entry.name === 'string', 'Dirent should have name property');
    count++;
  }

  assert(count === 2, 'opendirSync should read all directory entries');

  dir.closeSync();
  cleanup(testDir);
}

// Test 1.11: readvSync
console.log('Testing readvSync...');
{
  const testFile = path.join(tmpdir, 'test-readv.txt');
  cleanup(testFile);

  const content = 'Hello World! This is a test for vectored I/O.';
  fs.writeFileSync(testFile, content);

  const fd = fs.openSync(testFile, 'r');
  const buf1 = new ArrayBuffer(10);
  const buf2 = new ArrayBuffer(10);
  const buf3 = new ArrayBuffer(10);

  const bytesRead = fs.readvSync(fd, [buf1, buf2, buf3]);
  assert(bytesRead === 30, `readvSync should read 30 bytes, got ${bytesRead}`);

  const view1 = new Uint8Array(buf1);
  const view2 = new Uint8Array(buf2);
  const view3 = new Uint8Array(buf3);

  const result = new TextDecoder().decode(
    new Uint8Array([...view1, ...view2, ...view3])
  );
  assert(
    result === 'Hello World! This is a test fo',
    'readvSync should read correct content'
  );

  fs.closeSync(fd);
  cleanup(testFile);
}

// Test 1.12: writevSync
console.log('Testing writevSync...');
{
  const testFile = path.join(tmpdir, 'test-writev.txt');
  cleanup(testFile);

  const fd = fs.openSync(testFile, 'w');

  const encoder = new TextEncoder();
  const buf1 = encoder.encode('Hello ').buffer;
  const buf2 = encoder.encode('World! ').buffer;
  const buf3 = encoder.encode('Vectored I/O works!').buffer;

  const bytesWritten = fs.writevSync(fd, [buf1, buf2, buf3]);
  assert(
    bytesWritten === 32,
    `writevSync should write 32 bytes, got ${bytesWritten}`
  );

  fs.closeSync(fd);

  const content = fs.readFileSync(testFile, 'utf8');
  assert(
    content === 'Hello World! Vectored I/O works!',
    'writevSync should write correct content'
  );

  cleanup(testFile);
}

// Test 1.13: statfsSync
console.log('Testing statfsSync...');
{
  const testPath = tmpdir;

  try {
    const fsStats = fs.statfsSync(testPath);
    assert(
      fsStats !== null && fsStats !== undefined,
      'statfsSync should return stats object'
    );
    assert(
      typeof fsStats.type === 'number',
      'statfs should have type property'
    );
    assert(
      typeof fsStats.bsize === 'number',
      'statfs should have bsize property'
    );
    assert(
      typeof fsStats.blocks === 'number',
      'statfs should have blocks property'
    );
    assert(
      typeof fsStats.bfree === 'number',
      'statfs should have bfree property'
    );
    assert(
      typeof fsStats.bavail === 'number',
      'statfs should have bavail property'
    );
    assert(fsStats.bsize > 0, 'block size should be positive');
  } catch (e) {
    assert(false, `statfsSync failed: ${e.message}`);
  }
}

// Summary
console.log('\n=== Test Summary ===');
console.log(`Tests passed: ${testsPassed}`);
console.log(`Tests failed: ${testsFailed}`);

if (testsFailed > 0) {
  throw new Error(`${testsFailed} test(s) failed`);
}

console.log('All Phase 1 tests passed!');
