#!/usr/bin/env node

/**
 * Tests for module.parsePackageJSON() implementation
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
const { parsePackageJSON } = require('node:module');

console.log('Starting module.parsePackageJSON() tests...\n');

// Test 1: Basic package.json parsing
function testBasicParsing() {
  console.log('Test 1: Basic package.json parsing');
  const tempDir = createTempDir();
  const packageContent = {
    name: 'test-package',
    version: '1.0.0',
    description: 'A test package',
    main: 'index.js',
  };
  const packagePath = createPackageJson(tempDir, packageContent);

  try {
    const result = parsePackageJSON(packagePath);

    if (typeof result === 'object' && result !== null) {
      if (
        result.name === packageContent.name &&
        result.version === packageContent.version &&
        result.description === packageContent.description &&
        result.main === packageContent.main
      ) {
        console.log('✅ PASS: Parsed package.json correctly');
        return true;
      } else {
        console.log(
          `❌ FAIL: Content mismatch. Expected name=${packageContent.name}, version=${packageContent.version}, got name=${result.name}, version=${result.version}`
        );
        return false;
      }
    } else {
      console.log(`❌ FAIL: Expected object, got ${typeof result}`);
      return false;
    }
  } catch (error) {
    console.log(`❌ FAIL: Unexpected error: ${error.message}`);
    return false;
  } finally {
    cleanupTempDir(tempDir);
  }
}

// Test 2: Complex package.json with nested objects
function testComplexParsing() {
  console.log('\nTest 2: Complex package.json parsing');
  const tempDir = createTempDir();
  const packageContent = {
    name: 'complex-package',
    version: '2.0.0',
    scripts: {
      start: 'node index.js',
      test: 'jest',
      build: 'webpack',
    },
    dependencies: {
      express: '^4.18.0',
      lodash: '^4.17.21',
    },
    devDependencies: {
      jest: '^28.0.0',
    },
    repository: {
      type: 'git',
      url: 'https://github.com/user/repo.git',
    },
  };
  const packagePath = createPackageJson(tempDir, packageContent);

  try {
    const result = parsePackageJSON(packagePath);

    if (typeof result === 'object' && result !== null) {
      if (
        result.name === packageContent.name &&
        typeof result.scripts === 'object' &&
        result.scripts.start === packageContent.scripts.start &&
        typeof result.dependencies === 'object' &&
        result.dependencies.express === packageContent.dependencies.express &&
        typeof result.repository === 'object' &&
        result.repository.type === packageContent.repository.type
      ) {
        console.log('✅ PASS: Parsed complex package.json correctly');
        return true;
      } else {
        console.log(`❌ FAIL: Complex content mismatch`);
        console.log(
          'Expected scripts:',
          JSON.stringify(packageContent.scripts)
        );
        console.log('Got scripts:', JSON.stringify(result.scripts));
        return false;
      }
    } else {
      console.log(`❌ FAIL: Expected object, got ${typeof result}`);
      return false;
    }
  } catch (error) {
    console.log(`❌ FAIL: Unexpected error: ${error.message}`);
    return false;
  } finally {
    cleanupTempDir(tempDir);
  }
}

// Test 3: Invalid JSON handling
function testInvalidJson() {
  console.log('\nTest 3: Invalid JSON handling');
  const tempDir = createTempDir();
  const packagePath = path.join(tempDir, 'package.json');

  // Write invalid JSON
  fs.writeFileSync(packagePath, '{ "name": "invalid", "version": "1.0.0", }');

  try {
    const result = parsePackageJSON(packagePath);
    console.log(
      `❌ FAIL: Expected error for invalid JSON, but got result: ${typeof result}`
    );
    return false;
  } catch (error) {
    if (
      error.message.includes('parsePackageJSON') ||
      error.name === 'SyntaxError'
    ) {
      console.log('✅ PASS: Correctly threw error for invalid JSON');
      return true;
    } else {
      console.log(`❌ FAIL: Expected JSON parse error, got: ${error.message}`);
      return false;
    }
  } finally {
    cleanupTempDir(tempDir);
  }
}

// Test 4: Non-existent file handling
function testNonExistentFile() {
  console.log('\nTest 4: Non-existent file handling');

  try {
    const result = parsePackageJSON('/non/existent/path/package.json');
    console.log(
      `❌ FAIL: Expected error for non-existent file, but got result: ${typeof result}`
    );
    return false;
  } catch (error) {
    if (
      error.message.includes('parsePackageJSON') &&
      error.message.includes('failed to read file')
    ) {
      console.log('✅ PASS: Correctly threw error for non-existent file');
      return true;
    } else {
      console.log(`❌ FAIL: Expected file read error, got: ${error.message}`);
      return false;
    }
  }
}

// Test 5: Empty package.json
function testEmptyPackage() {
  console.log('\nTest 5: Empty package.json');
  const tempDir = createTempDir();
  const packagePath = createPackageJson(tempDir, {});

  try {
    const result = parsePackageJSON(packagePath);

    if (typeof result === 'object' && result !== null) {
      if (Object.keys(result).length === 0) {
        console.log('✅ PASS: Parsed empty package.json correctly');
        return true;
      } else {
        console.log(
          `❌ FAIL: Expected empty object, got keys: ${Object.keys(result)}`
        );
        return false;
      }
    } else {
      console.log(`❌ FAIL: Expected object, got ${typeof result}`);
      return false;
    }
  } catch (error) {
    console.log(`❌ FAIL: Unexpected error: ${error.message}`);
    return false;
  } finally {
    cleanupTempDir(tempDir);
  }
}

// Run all tests
function runAllTests() {
  const tests = [
    testBasicParsing,
    testComplexParsing,
    testInvalidJson,
    testNonExistentFile,
    testEmptyPackage,
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
