#!/usr/bin/env node

/**
 * Constants Module Test Runner
 *
 * This script runs all the Node.js constants module tests
 * from the test/node/constants directory.
 */

console.log('🧪 Running Node.js Constants Module Tests...\n');

const testFiles = ['constants.js', 'compatibility.js'];

let passedTests = 0;
let totalTests = 0;

async function runTestFile(testFile) {
  console.log(`\n📁 Running ${testFile}...`);

  try {
    // Use child_process to run the test file
    const { execSync } = require('child_process');
    const output = execSync(`${process.argv[0]} ${__dirname}/${testFile}`, {
      encoding: 'utf8',
      stdio: 'pipe',
    });

    console.log('✅ PASSED');
    console.log(output);
    passedTests++;
    return true;
  } catch (error) {
    console.log('❌ FAILED');
    console.log(error.stdout || error.message);
    return false;
  }
}

async function runAllTests() {
  console.log('🚀 Starting Constants Module Test Suite\n');

  for (const testFile of testFiles) {
    totalTests++;
    await runTestFile(testFile);
  }

  console.log('\n📊 Test Results Summary:');
  console.log(`✅ Passed: ${passedTests}/${totalTests}`);
  console.log(`❌ Failed: ${totalTests - passedTests}/${totalTests}`);
  console.log(
    `📈 Success Rate: ${((passedTests / totalTests) * 100).toFixed(1)}%`
  );

  if (passedTests === totalTests) {
    console.log(
      '\n🎉 All tests passed! Constants module is working perfectly.'
    );
    console.log('✅ Ready for npm package compatibility!');
    process.exit(0);
  } else {
    console.log('\n💥 Some tests failed. Please check the output above.');
    process.exit(1);
  }
}

// Check if constants module is available first
try {
  const constants = require('node:constants');
  console.log('✅ Constants module loaded successfully\n');
  console.log(
    `📦 Module contains ${Object.keys(constants).length} top-level properties`
  );
  if (constants.errno)
    console.log(`   - errno: ${Object.keys(constants.errno).length} constants`);
  if (constants.signals)
    console.log(
      `   - signals: ${Object.keys(constants.signals).length} constants`
    );
  if (constants.F_OK !== undefined)
    console.log(`   - file access: F_OK, R_OK, W_OK, X_OK available`);
  if (constants.priority)
    console.log(
      `   - priority: ${Object.keys(constants.priority).length} constants`
    );
  if (constants.crypto)
    console.log(
      `   - crypto: ${Object.keys(constants.crypto).length} constants`
    );
  console.log('');
} catch (error) {
  console.error('❌ Failed to load constants module:', error.message);
  process.exit(1);
}

runAllTests().catch(console.error);
