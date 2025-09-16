const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const assert = require('jsrt:assert');

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
assert.ok(fs.existsSync(testDir), 'Basic mkdir should create directory');
// console.log('✓ Basic mkdir test passed');

// Test 2: mkdir with recursive option (single level)
console.log('Testing mkdir with recursive option (single level)...');
fs.rmdirSync(testDir);
fs.mkdirSync(testDir, { recursive: true });
assert.ok(fs.existsSync(testDir), 'Recursive mkdir should create directory');
// console.log('✓ Recursive mkdir (single level) test passed');

// Test 3: mkdir with recursive option (multiple levels)
console.log('Testing mkdir with recursive option (multiple levels)...');
fs.rmdirSync(testDir);
const nestedPath = path.join(testDir, 'nested', 'deep', 'path');
fs.mkdirSync(nestedPath, { recursive: true });
assert.ok(
  fs.existsSync(testDir),
  'Recursive mkdir should create root directory'
);
assert.ok(
  fs.existsSync(path.join(testDir, 'nested')),
  'Recursive mkdir should create nested directory'
);
assert.ok(
  fs.existsSync(path.join(testDir, 'nested', 'deep')),
  'Recursive mkdir should create deep directory'
);
assert.ok(
  fs.existsSync(nestedPath),
  'Recursive mkdir should create final directory'
);
// console.log('✓ Recursive mkdir (multiple levels) test passed');

// Test 4: mkdir with recursive option on existing directory (should not fail)
console.log('Testing mkdir with recursive option on existing directory...');
fs.mkdirSync(nestedPath, { recursive: true });
assert.ok(
  fs.existsSync(nestedPath),
  'Recursive mkdir on existing path should not fail'
);
// console.log('✓ Recursive mkdir on existing directory test passed');

// Test 5: mkdir with custom mode and recursive
console.log('Testing mkdir with custom mode and recursive...');
const modeTestPath = path.join(testDir, 'mode_test');
fs.mkdirSync(modeTestPath, { recursive: true, mode: 0o755 });
assert.ok(
  fs.existsSync(modeTestPath),
  'Recursive mkdir with mode should create directory'
);
// console.log('✓ Recursive mkdir with mode test passed');

// Test 6: mkdir without recursive on nested path (should fail)
console.log('Testing mkdir without recursive on nested path (should fail)...');
try {
  const failPath = path.join(testDir, 'should', 'fail');
  fs.mkdirSync(failPath);
  assert.ok(false, 'Non-recursive mkdir on nested path should fail');
} catch (error) {
  assert.ok(
    error.code === 'ENOENT',
    'Should get ENOENT error for non-recursive nested path'
  );
  // console.log('✓ Non-recursive mkdir failure test passed');
}

// Clean up
cleanup();

console.log('All mkdir recursive tests passed!');
