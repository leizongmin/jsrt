#!/usr/bin/env node

/**
 * Tests for module.findPackageJSON() implementation
 */

const fs = require('fs');
const path = require('path');
const os = require('os');

// Test helper functions
function createTempDir() {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'jsrt-test-'));
  return tempDir;
}

function createPackageJson(dir, content = {}) {
  const packagePath = path.join(dir, 'package.json');
  fs.writeFileSync(packagePath, JSON.stringify(content, null, 2));
  return packagePath;
}

function cleanupTempDir(dir) {
  fs.rmSync(dir, { recursive: true, force: true });
}

// Import the module we're testing
const { findPackageJSON, parsePackageJSON } = require('node:module');

console.log('Starting module.findPackageJSON() tests...\n');

// Test 1: Basic package.json lookup
function testBasicLookup() {
  console.log('Test 1: Basic package.json lookup');
  const tempDir = createTempDir();
  const packagePath = createPackageJson(tempDir, {
    name: 'test-package',
    version: '1.0.0',
  });

  try {
    const result = findPackageJSON('./some-file.js', tempDir);
    if (result === packagePath) {
      console.log('✅ PASS: Found package.json in current directory');
    } else {
      console.log(`❌ FAIL: Expected ${packagePath}, got ${result}`);
      return false;
    }
  } catch (error) {
    console.log(`❌ FAIL: Unexpected error: ${error.message}`);
    return false;
  } finally {
    cleanupTempDir(tempDir);
  }
  return true;
}

// Test 2: Upward directory traversal
function testUpwardTraversal() {
  console.log('\nTest 2: Upward directory traversal');
  const tempDir = createTempDir();
  const packagePath = createPackageJson(tempDir, {
    name: 'parent-package',
    version: '2.0.0',
  });
  const subDir = path.join(tempDir, 'src', 'utils');
  fs.mkdirSync(subDir, { recursive: true });

  try {
    const result = findPackageJSON('./helper.js', subDir);
    if (result === packagePath) {
      console.log('✅ PASS: Found package.json in parent directory');
    } else {
      console.log(`❌ FAIL: Expected ${packagePath}, got ${result}`);
      return false;
    }
  } catch (error) {
    console.log(`❌ FAIL: Unexpected error: ${error.message}`);
    return false;
  } finally {
    cleanupTempDir(tempDir);
  }
  return true;
}

// Test 3: No package.json found
function testNoPackageFound() {
  console.log('\nTest 3: No package.json found');
  const tempDir = createTempDir();

  try {
    const result = findPackageJSON('./some-file.js', tempDir);
    if (result === undefined) {
      console.log(
        '✅ PASS: Correctly returned undefined when no package.json found'
      );
    } else {
      console.log(`❌ FAIL: Expected undefined, got ${result}`);
      return false;
    }
  } catch (error) {
    console.log(`❌ FAIL: Unexpected error: ${error.message}`);
    return false;
  } finally {
    cleanupTempDir(tempDir);
  }
  return true;
}

// Test 4: Absolute path handling
function testAbsolutePath() {
  console.log('\nTest 4: Absolute path handling');
  const tempDir = createTempDir();
  const packagePath = createPackageJson(tempDir, {
    name: 'absolute-test',
    version: '3.0.0',
  });
  const testFile = path.join(tempDir, 'src', 'test.js');
  fs.mkdirSync(path.dirname(testFile), { recursive: true });
  fs.writeFileSync(testFile, 'console.log("test");');

  try {
    const result = findPackageJSON(testFile);
    if (result === packagePath) {
      console.log('✅ PASS: Found package.json using absolute path');
    } else {
      console.log(`❌ FAIL: Expected ${packagePath}, got ${result}`);
      return false;
    }
  } catch (error) {
    console.log(`❌ FAIL: Unexpected error: ${error.message}`);
    return false;
  } finally {
    cleanupTempDir(tempDir);
  }
  return true;
}

// Test 5: Cache performance (multiple calls)
function testCachePerformance() {
  console.log('\nTest 5: Cache performance test');
  const tempDir = createTempDir();
  const packagePath = createPackageJson(tempDir, {
    name: 'cache-test',
    version: '4.0.0',
  });

  try {
    const start = Date.now();

    // Make multiple calls - should use cache after first call
    for (let i = 0; i < 10; i++) {
      const result = findPackageJSON('./file.js', tempDir);
      if (result !== packagePath) {
        console.log(
          `❌ FAIL: Cache test failed on iteration ${i}, expected ${packagePath}, got ${result}`
        );
        return false;
      }
    }

    const end = Date.now();
    const duration = end - start;

    if (duration < 100) {
      // Should be very fast with caching
      console.log(
        `✅ PASS: Cache working correctly (${duration}ms for 10 calls)`
      );
    } else {
      console.log(
        `⚠️  WARN: Cache might not be working efficiently (${duration}ms for 10 calls)`
      );
    }
  } catch (error) {
    console.log(`❌ FAIL: Unexpected error: ${error.message}`);
    return false;
  } finally {
    cleanupTempDir(tempDir);
  }
  return true;
}

// Run all tests
function runAllTests() {
  const tests = [
    testBasicLookup,
    testUpwardTraversal,
    testNoPackageFound,
    testAbsolutePath,
    testCachePerformance,
  ];

  let passed = 0;
  let failed = 0;

  for (const test of tests) {
    try {
      if (test()) {
        passed++;
      } else {
        failed++;
      }
    } catch (error) {
      console.log(`❌ FAIL: Test threw exception: ${error.message}`);
      failed++;
    }
  }

  console.log(`\n=== Test Results ===`);
  console.log(`Passed: ${passed}`);
  console.log(`Failed: ${failed}`);
  console.log(`Total:  ${passed + failed}`);

  if (failed === 0) {
    console.log('🎉 All tests passed!');
    process.exit(0);
  } else {
    console.log('💥 Some tests failed!');
    process.exit(1);
  }
}

runAllTests();
