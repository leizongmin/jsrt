const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const assert = require('node:assert');

let asyncTestsCompleted = 0;
let totalAsyncTests = 4;

console.log('Testing mkdir with recursive option...');

// Get temporary directory and create test directory path
const tmpDir = os.tmpdir();
const testDir = path.join(tmpDir, 'test_mkdir_temp');

// Clean up function
function cleanup() {
  try {
    if (fs.existsSync(testDir)) {
      // Remove nested directories
      const removeRecursive = (dir) => {
        try {
          const files = fs.readdirSync(dir);
          for (const file of files) {
            const filePath = path.join(dir, file);
            const stat = fs.statSync(filePath);
            if (stat.isDirectory()) {
              removeRecursive(filePath);
            } else {
              fs.unlinkSync(filePath);
            }
          }
          fs.rmdirSync(dir);
        } catch (e) {
          // Ignore cleanup errors
        }
      };
      removeRecursive(testDir);
    }
  } catch (e) {
    // Ignore cleanup errors
  }
}

// Clean up before starting
cleanup();

// Test 1: Basic mkdir without recursive (should work as before)
console.log('Testing basic mkdir...');
fs.mkdirSync(testDir);
assert(fs.existsSync(testDir), 'Basic mkdir should create directory');
// console.log('✓ Basic mkdir test passed');

// Test 2: mkdir with recursive option (single level)
console.log('Testing mkdir with recursive option (single level)...');
fs.rmdirSync(testDir);
fs.mkdirSync(testDir, { recursive: true });
assert(fs.existsSync(testDir), 'Recursive mkdir should create directory');
// console.log('✓ Recursive mkdir (single level) test passed');

// Test 3: mkdir with recursive option (multiple levels)
console.log('Testing mkdir with recursive option (multiple levels)...');
fs.rmdirSync(testDir);
const nestedPath = path.join(testDir, 'nested', 'deep', 'path');
fs.mkdirSync(nestedPath, { recursive: true });
assert(fs.existsSync(testDir), 'Recursive mkdir should create root directory');
assert(
  fs.existsSync(path.join(testDir, 'nested')),
  'Recursive mkdir should create nested directory'
);
assert(
  fs.existsSync(path.join(testDir, 'nested', 'deep')),
  'Recursive mkdir should create deep directory'
);
assert(
  fs.existsSync(nestedPath),
  'Recursive mkdir should create final directory'
);
// console.log('✓ Recursive mkdir (multiple levels) test passed');

// Test 4: mkdir with recursive option on existing directory (should not fail)
console.log('Testing mkdir with recursive option on existing directory...');
fs.mkdirSync(nestedPath, { recursive: true });
assert(
  fs.existsSync(nestedPath),
  'Recursive mkdir on existing path should not fail'
);
// console.log('✓ Recursive mkdir on existing directory test passed');

// Test 5: mkdir with custom mode and recursive
console.log('Testing mkdir with custom mode and recursive...');
const modeTestPath = path.join(testDir, 'mode_test');
fs.mkdirSync(modeTestPath, { recursive: true, mode: 0o755 });
assert(
  fs.existsSync(modeTestPath),
  'Recursive mkdir with mode should create directory'
);
// console.log('✓ Recursive mkdir with mode test passed');

// Test 6: mkdir without recursive on nested path (should fail)
console.log('Testing mkdir without recursive on nested path (should fail)...');
try {
  const failPath = path.join(testDir, 'should', 'fail');
  fs.mkdirSync(failPath);
  assert(false, 'Non-recursive mkdir on nested path should fail');
} catch (error) {
  assert(
    error.code === 'ENOENT',
    'Should get ENOENT error for non-recursive nested path'
  );
  // console.log('✓ Non-recursive mkdir failure test passed');
}

// Clean up
cleanup();

console.log('All synchronous mkdir recursive tests passed!');

// Now test async versions
console.log('\nTesting async mkdir with recursive option...');

const asyncTestBase = path.join(tmpDir, 'test_async_mkdir_temp');

// Clean up async test directory
function asyncCleanup() {
  try {
    if (fs.existsSync(asyncTestBase)) {
      const removeRecursive = (dir) => {
        try {
          const files = fs.readdirSync(dir);
          for (const file of files) {
            const filePath = path.join(dir, file);
            const stat = fs.statSync(filePath);
            if (stat.isDirectory()) {
              removeRecursive(filePath);
            } else {
              fs.unlinkSync(filePath);
            }
          }
          fs.rmdirSync(dir);
        } catch (e) {
          // Ignore cleanup errors
        }
      };
      removeRecursive(asyncTestBase);
    }
  } catch (e) {
    // Ignore cleanup errors
  }
}

asyncCleanup();

function checkAsyncTestComplete() {
  asyncTestsCompleted++;
  if (asyncTestsCompleted === totalAsyncTests) {
    console.log('All async mkdir recursive tests passed!');
    asyncCleanup();
    console.log(
      '\n🎉 All mkdir recursive tests (sync + async + promises) passed!'
    );
  }
}

// Async Test 1: mkdir callback with recursive option
console.log('Testing mkdir callback with recursive option...');
const callbackTestPath = path.join(asyncTestBase, 'callback', 'deep', 'path');
fs.mkdir(callbackTestPath, { recursive: true }, (err) => {
  if (err) {
    console.error('❌ mkdir callback test failed:', err.message);
    process.exit(1);
  }

  try {
    assert(
      fs.existsSync(callbackTestPath),
      'Callback mkdir should create directory'
    );
    console.log('✓ mkdir callback with recursive test passed');
  } catch (error) {
    console.error('❌ mkdir callback test assertion failed:', error.message);
    process.exit(1);
  }

  checkAsyncTestComplete();
});

// Async Test 2: mkdir callback with legacy mode format
console.log('Testing mkdir callback with legacy mode format...');
const legacyTestPath = path.join(asyncTestBase, 'legacy', 'deep', 'path');
fs.mkdir(legacyTestPath, 0o755, (err) => {
  // This should fail because parent directories don't exist
  if (err) {
    try {
      assert(
        err.code === 'ENOENT',
        'Should get ENOENT error for non-recursive legacy call'
      );
      console.log(
        '✓ mkdir callback legacy mode test passed (correctly failed)'
      );
    } catch (error) {
      console.error(
        '❌ mkdir callback legacy mode test assertion failed:',
        error.message
      );
      process.exit(1);
    }
  } else {
    console.error('❌ mkdir callback legacy mode should have failed');
    process.exit(1);
  }

  checkAsyncTestComplete();
});

// Async Test 3: promises.mkdir with recursive option
console.log('Testing promises.mkdir with recursive option...');
(async () => {
  try {
    const promiseTestPath = path.join(asyncTestBase, 'promise', 'deep', 'path');
    await fs.promises.mkdir(promiseTestPath, { recursive: true });

    assert(
      fs.existsSync(promiseTestPath),
      'Promise mkdir should create directory'
    );
    console.log('✓ promises.mkdir with recursive test passed');

    checkAsyncTestComplete();
  } catch (error) {
    console.error('❌ promises.mkdir test failed:', error.message);
    process.exit(1);
  }
})();

// Async Test 4: promises.mkdir without recursive on nested path (should fail)
console.log('Testing promises.mkdir without recursive on nested path...');
(async () => {
  try {
    const failPath = path.join(asyncTestBase, 'should', 'fail');
    await fs.promises.mkdir(failPath);

    console.error('❌ promises.mkdir should have failed for nested path');
    process.exit(1);
  } catch (error) {
    try {
      assert(
        error.code === 'ENOENT',
        'Should get ENOENT error for non-recursive nested path'
      );
      console.log(
        '✓ promises.mkdir without recursive test passed (correctly failed)'
      );

      checkAsyncTestComplete();
    } catch (assertionError) {
      console.error(
        '❌ promises.mkdir assertion failed:',
        assertionError.message
      );
      process.exit(1);
    }
  }
})();
