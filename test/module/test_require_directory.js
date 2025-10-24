// Test requiring a directory as a module

const assert = require('jsrt:assert');
const fs = require('node:fs');
const path = require('node:path');

console.log('Testing directory require...');

// Create a test package directory structure
const testDir = path.join(__dirname, '../../target/tmp/test_dir_module');
const binDir = path.join(testDir, 'bin');
const libDir = path.join(testDir, 'lib');

// Clean up if exists
try {
  fs.rmSync(testDir, { recursive: true, force: true });
} catch (e) {}

// Create directory structure
fs.mkdirSync(testDir, { recursive: true });
fs.mkdirSync(binDir, { recursive: true });
fs.mkdirSync(libDir, { recursive: true });

// Create package.json
fs.writeFileSync(
  path.join(testDir, 'package.json'),
  JSON.stringify({
    name: 'test-dir-module',
    main: './lib/index.js',
  })
);

// Create lib/index.js
fs.writeFileSync(
  path.join(libDir, 'index.js'),
  'module.exports = { hello: "world", value: 42 };'
);

// Create bin/test.js that requires parent directory
fs.writeFileSync(
  path.join(binDir, 'test.js'),
  "const parent = require('../'); console.log('Success:', parent.hello, parent.value);"
);

// Test 1: require directory by path
console.log('Test 1: require directory by path');
const mod1 = require(testDir);
assert.strictEqual(mod1.hello, 'world');
assert.strictEqual(mod1.value, 42);
console.log('  ✓ require directory works');

// Test 2: require parent directory from subdirectory
console.log('Test 2: require parent directory (..)');
const binTestPath = path.join(binDir, 'test.js');
require(binTestPath);
console.log('  ✓ require parent directory works');

// Test 3: Create another package without main field (fallback to index.js)
const testDir2 = path.join(__dirname, '../../target/tmp/test_dir_module2');
fs.mkdirSync(testDir2, { recursive: true });

fs.writeFileSync(
  path.join(testDir2, 'package.json'),
  JSON.stringify({ name: 'test-dir-module2' })
);

fs.writeFileSync(
  path.join(testDir2, 'index.js'),
  'module.exports = { fallback: true };'
);

console.log('Test 3: require directory with default index.js');
const mod2 = require(testDir2);
assert.strictEqual(mod2.fallback, true);
console.log('  ✓ fallback to index.js works');

// Test 4: Directory without package.json (should use index.js)
const testDir3 = path.join(__dirname, '../../target/tmp/test_dir_module3');
fs.mkdirSync(testDir3, { recursive: true });
fs.writeFileSync(
  path.join(testDir3, 'index.js'),
  'module.exports = { noPackageJson: true };'
);

console.log('Test 4: require directory without package.json');
const mod3 = require(testDir3);
assert.strictEqual(mod3.noPackageJson, true);
console.log('  ✓ directory without package.json works');

// Cleanup
fs.rmSync(testDir, { recursive: true, force: true });
fs.rmSync(testDir2, { recursive: true, force: true });
fs.rmSync(testDir3, { recursive: true, force: true });

console.log('\nAll directory require tests passed!');
