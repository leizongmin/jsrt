const assert = require('jsrt:assert');
const module = require('node:module');

console.log('Testing findPackageJSON with simple case...');
console.log('Current directory:', process.cwd());

try {
  // Try with current directory - should either find a package.json or return undefined
  const result = module.findPackageJSON('./test_debug_package.js');
  // Test should return a valid result (object, string path, or undefined) without throwing
  assert(typeof result === 'object' || typeof result === 'string' || result === undefined,
         `Expected object, string, or undefined, got ${typeof result}: ${result}`);
  console.log('✅ Test passed - Result type:', typeof result);
} catch (error) {
  // Re-throw the error so test fails properly
  assert.fail(`Unexpected error: ${error.message}`);
}
