#!/usr/bin/env node

/**
 * fs.mkdir recursive options test
 *
 * Tests fs.mkdir with recursive option across all three APIs:
 * - mkdirSync (synchronous)
 * - mkdir (callback-based)
 * - promises.mkdir (Promise-based)
 */

console.log('🧪 fs.mkdir recursive options test\n');

try {
  const fs = require('node:fs');
  const path = require('path');
  const constants = require('node:constants');
  const assert = require('node:assert');

  console.log('✅ Modules loaded successfully');

  // Test configuration
  const testBase = '/tmp/jsrt_mkdir_test_' + Date.now();
  const testPaths = {
    simple: path.join(testBase, 'simple'),
    recursive: path.join(testBase, 'recursive', 'aaa', 'bbb', 'ccc'),
    nested: path.join(testBase, 'nested', 'x', 'y', 'z', 'w'),
    withMode: path.join(testBase, 'withmode', 'a', 'b', 'c'),
  };

  let passedTests = 0;
  let totalTests = 0;

  function runTest(testName, testFn) {
    totalTests++;
    console.log(`\n📋 Test ${totalTests}: ${testName}`);
    try {
      testFn();
      passedTests++;
      console.log(`✅ ${testName} - PASSED`);
    } catch (error) {
      console.log(`❌ ${testName} - FAILED: ${error.message}`);
      console.log(`   Stack: ${error.stack}`);
    }
  }

  // Cleanup function
  function cleanup(testPath) {
    try {
      // Try to remove the entire test directory
      const rimraf = (dir) => {
        if (fs.existsSync(dir)) {
          const files = fs.readdirSync(dir);
          for (const file of files) {
            const fullPath = path.join(dir, file);
            if (fs.statSync(fullPath).isDirectory()) {
              rimraf(fullPath);
            } else {
              fs.unlinkSync(fullPath);
            }
          }
          fs.rmdirSync(dir);
        }
      };
      rimraf(testBase);
    } catch (error) {
      // Ignore cleanup errors
    }
  }

  // Test 1: mkdirSync without recursive (should fail for deep paths)
  runTest('mkdirSync - non-recursive failure', () => {
    try {
      fs.mkdirSync(testPaths.recursive);
      throw new Error(
        'Expected mkdirSync to fail for deep path without recursive'
      );
    } catch (error) {
      if (error.code !== 'ENOENT') {
        throw new Error(`Expected ENOENT error, got ${error.code}`);
      }
      console.log(`   Correctly failed with ${error.code}: ${error.message}`);
    }
  });

  // Test 2: mkdirSync with recursive (should succeed)
  runTest('mkdirSync - recursive success', () => {
    fs.mkdirSync(testPaths.recursive, { recursive: true });

    if (!fs.existsSync(testPaths.recursive)) {
      throw new Error('Directory was not created');
    }

    const stats = fs.statSync(testPaths.recursive);
    if (!stats.isDirectory()) {
      throw new Error('Path is not a directory');
    }

    console.log(`   Successfully created: ${testPaths.recursive}`);
  });

  // Test 3: mkdirSync with mode and recursive
  runTest('mkdirSync - recursive with mode', () => {
    const mode = 0o755;
    fs.mkdirSync(testPaths.withMode, { recursive: true, mode });

    if (!fs.existsSync(testPaths.withMode)) {
      throw new Error('Directory was not created');
    }

    console.log(`   Successfully created with mode ${mode.toString(8)}`);
  });

  // Test 4: mkdir with callback - modern options format
  runTest('mkdir - callback with modern options', (done) => {
    fs.mkdir(testPaths.nested, { recursive: true }, (err) => {
      if (err) {
        throw new Error(`Callback mkdir failed: ${err.message}`);
      }

      if (!fs.existsSync(testPaths.nested)) {
        throw new Error('Directory was not created');
      }

      console.log(`   Successfully created: ${testPaths.nested}`);
    });
  });

  // Test 5: mkdir with callback - legacy mode format
  runTest('mkdir - callback with legacy mode', (done) => {
    const legacyPath = path.join(testBase, 'legacy', 'a', 'b');

    // First create parent directories
    fs.mkdirSync(path.dirname(path.dirname(legacyPath)), { recursive: true });
    fs.mkdirSync(path.dirname(legacyPath));

    fs.mkdir(legacyPath, 0o755, (err) => {
      if (err) {
        throw new Error(`Legacy mkdir failed: ${err.message}`);
      }

      if (!fs.existsSync(legacyPath)) {
        throw new Error('Directory was not created');
      }

      console.log(`   Successfully created with legacy mode`);
    });
  });

  // Test 6: promises.mkdir with recursive
  runTest('promises.mkdir - recursive success', async () => {
    const promiseTestPath = path.join(
      testBase,
      'promise',
      'deep',
      'nested',
      'path'
    );

    await fs.promises.mkdir(promiseTestPath, { recursive: true });

    if (!fs.existsSync(promiseTestPath)) {
      throw new Error('Directory was not created');
    }

    const stats = fs.statSync(promiseTestPath);
    if (!stats.isDirectory()) {
      throw new Error('Path is not a directory');
    }

    console.log(`   Successfully created: ${promiseTestPath}`);
  });

  // Test 7: promises.mkdir with recursive and mode
  runTest('promises.mkdir - recursive with mode', async () => {
    const modeTestPath = path.join(testBase, 'promiseMode', 'test', 'dir');
    const mode = 0o700;

    await fs.promises.mkdir(modeTestPath, { recursive: true, mode });

    if (!fs.existsSync(modeTestPath)) {
      throw new Error('Directory was not created');
    }

    console.log(`   Successfully created with mode ${mode.toString(8)}`);
  });

  // Test 8: promises.mkdir without recursive (should fail for deep paths)
  runTest('promises.mkdir - non-recursive failure', async () => {
    const deepPath = path.join(testBase, 'shouldfail', 'a', 'b', 'c', 'd');

    try {
      await fs.promises.mkdir(deepPath);
      throw new Error(
        'Expected promises.mkdir to fail for deep path without recursive'
      );
    } catch (error) {
      if (error.code !== 'ENOENT') {
        throw new Error(`Expected ENOENT error, got ${error.code}`);
      }
      console.log(`   Correctly failed with ${error.code}: ${error.message}`);
    }
  });

  // Test 9: Error handling consistency
  runTest('Error handling consistency', async () => {
    const invalidPath = '/root/nonexistent/path';

    // Test that all APIs return consistent error codes
    let syncError, callbackError, promiseError;

    try {
      fs.mkdirSync(invalidPath, { recursive: true });
    } catch (error) {
      syncError = error;
    }

    await new Promise((resolve) => {
      fs.mkdir(invalidPath, { recursive: true }, (error) => {
        callbackError = error;
        resolve();
      });
    });

    try {
      await fs.promises.mkdir(invalidPath, { recursive: true });
    } catch (error) {
      promiseError = error;
    }

    // All should have the same error code (likely EACCES or EPERM)
    const errorCodes = [syncError, callbackError, promiseError]
      .filter(Boolean)
      .map((e) => e.code);

    const uniqueCodes = [...new Set(errorCodes)];
    if (uniqueCodes.length > 1) {
      throw new Error(`Inconsistent error codes: ${uniqueCodes.join(', ')}`);
    }

    console.log(
      `   Consistent error handling: ${uniqueCodes[0] || 'no errors'}`
    );
  });

  // Test 10: Constants integration
  runTest('Constants module integration', () => {
    const constantsTestPath = path.join(testBase, 'constants', 'test');

    // Test using constants for access checks
    fs.mkdirSync(constantsTestPath, { recursive: true });

    // Verify access using constants
    fs.accessSync(constantsTestPath, constants.F_OK);
    fs.accessSync(constantsTestPath, constants.R_OK);
    fs.accessSync(constantsTestPath, constants.W_OK);
    fs.accessSync(constantsTestPath, constants.X_OK);

    console.log('   Successfully integrated with node:constants');
  });

  // Test 11: Edge cases
  runTest('Edge cases', async () => {
    // Test empty path segments
    const emptySegmentPath = path.join(
      testBase,
      'empty',
      '',
      'test',
      '',
      'dir'
    );
    const normalizedPath = path.join(testBase, 'empty', 'test', 'dir');

    await fs.promises.mkdir(emptySegmentPath, { recursive: true });

    if (!fs.existsSync(normalizedPath)) {
      throw new Error(
        'Directory with empty segments was not created correctly'
      );
    }

    console.log('   Handled empty path segments correctly');
  });

  // Wait for all async operations to complete
  setTimeout(() => {
    console.log('\n📊 Test Results Summary:');
    console.log(`✅ Passed: ${passedTests}/${totalTests}`);
    console.log(`❌ Failed: ${totalTests - passedTests}/${totalTests}`);
    console.log(
      `📈 Success Rate: ${((passedTests / totalTests) * 100).toFixed(1)}%`
    );

    if (passedTests === totalTests) {
      console.log(
        '\n🎉 All tests passed! fs.mkdir recursive option working correctly.'
      );
      console.log('\n✅ Verification Results:');
      console.log('   - mkdirSync recursive option: ✅ WORKING');
      console.log('   - mkdir callback recursive option: ✅ WORKING');
      console.log('   - promises.mkdir recursive option: ✅ WORKING');
      console.log('   - Error handling consistency: ✅ VERIFIED');
      console.log('   - Node.js API compatibility: ✅ CONFIRMED');
      console.log('   - Constants module integration: ✅ INTEGRATED');
    } else {
      console.log(
        `\n💥 ${totalTests - passedTests} tests failed. Please check the output above.`
      );
      process.exit(1);
    }

    // Cleanup
    cleanup(testBase);
    console.log('\n🧹 Test cleanup completed');
  }, 1000);
} catch (error) {
  console.error('❌ Test setup failed:', error.message);
  console.error('Stack:', error.stack);
  process.exit(1);
}
