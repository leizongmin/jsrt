// Test npm module support
const assert = require('jsrt:assert');
console.log('=== NPM Module Loading Tests ===');

// Test 1: Basic npm package (CommonJS)
console.log('\nTest 1: Basic npm package with require');
const testPackage = require('test-package');
assert.strictEqual(typeof testPackage.greetFromPackage, 'function', 'greetFromPackage should be a function');
assert.strictEqual(typeof testPackage.packageData, 'object', 'packageData should be an object');
assert.strictEqual(testPackage.packageData.version, '1.0.0', 'version should be 1.0.0');
console.log('✓ Basic npm package loaded successfully');
console.log('  greet result:', testPackage.greetFromPackage('npm'));

// Test 2: Scoped npm package (CommonJS)
console.log('\nTest 2: Scoped npm package with require');
const scopedPackage = require('@scope/test-scoped-package');
assert.strictEqual(typeof scopedPackage.scopedGreet, 'function', 'scopedGreet should be a function');
console.log('✓ Scoped npm package loaded successfully');
console.log('  scoped greet result:', scopedPackage.scopedGreet('scoped-npm'));

// Test 3: Caching - require same module again
console.log('\nTest 3: Module caching');
const testPackage2 = require('test-package');
assert.strictEqual(testPackage, testPackage2, 'Cached modules should be the same instance');
console.log('✓ Module caching works correctly');

// Test 4: Non-existent package
console.log('\nTest 4: Non-existent package handling');
try {
  require('non-existent-package');
  assert.fail('Should have thrown an error for non-existent package');
} catch (error) {
  assert.ok(error.message.includes('Cannot find module'), 'Should throw appropriate error message');
  console.log('✓ Non-existent package handled correctly');
}

console.log('\n=== All npm module CommonJS tests passed ===');