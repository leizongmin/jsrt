// Test file for util.callbackify()
const util = require('node:util');
const assert = require('jsrt:assert');

console.log('Testing util.callbackify()\n');

let testsPassed = 0;
let testsFailed = 0;

// Test 1: Basic async to callback conversion
console.log('=== Test 1: Basic conversion ===');
async function asyncDouble(x) {
  return x * 2;
}
const cbDouble = util.callbackify(asyncDouble);
cbDouble(21, (err, result) => {
  if (err) {
    console.error('✗ Test 1 failed:', err);
    testsFailed++;
  } else if (result === 42) {
    console.log('✓ Basic conversion works');
    testsPassed++;
  } else {
    console.error('✗ Test 1 failed: Expected 42, got', result);
    testsFailed++;
  }
  continueTests();
});

function continueTests() {
  // Test 2: Async error handling
  console.log('\n=== Test 2: Error handling ===');
  async function asyncError() {
    throw new Error('Test error');
  }
  const cbError = util.callbackify(asyncError);
  cbError((err, result) => {
    if (err && err.message === 'Test error') {
      console.log('✓ Error handling works');
      testsPassed++;
    } else {
      console.error('✗ Test 2 failed');
      testsFailed++;
    }

    // Test 3: Multiple async arguments
    console.log('\n=== Test 3: Multiple arguments ===');
    async function asyncSum(a, b, c) {
      return a + b + c;
    }
    const cbSum = util.callbackify(asyncSum);
    cbSum(1, 2, 3, (err, result) => {
      if (!err && result === 6) {
        console.log('✓ Multiple arguments work');
        testsPassed++;
      } else {
        console.error('✗ Test 3 failed');
        testsFailed++;
      }

      // Test 4: Verify function type
      console.log('\n=== Test 4: Function type ===');
      async function testFn() {}
      const cbFn = util.callbackify(testFn);
      if (typeof cbFn === 'function') {
        console.log('✓ Returns function');
        testsPassed++;
      } else {
        console.error('✗ Test 4 failed');
        testsFailed++;
      }

      printSummary();
    });
  });
}

function printSummary() {
  console.log('\n' + '='.repeat(60));
  console.log(`Tests passed: ${testsPassed}`);
  console.log(`Tests failed: ${testsFailed}`);
  if (testsFailed === 0) {
    console.log('✅ All callbackify tests passed!');
  } else {
    console.log('❌ Some tests failed');
    process.exit(1);
  }
  console.log('='.repeat(60));
}
