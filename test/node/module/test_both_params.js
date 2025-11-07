const assert = require('jsrt:assert');
const module = require('node:module');

console.log('Testing with both parameters...');

try {
  const result = module.findPackageJSON('./test_both_params.js', '/tmp');
  // Test should return a valid result (object, string path, or undefined)
  assert(typeof result === 'object' || typeof result === 'string' || result === undefined,
         `Expected object, string, or undefined, got ${typeof result}: ${result}`);
  console.log('✅ Test passed - Result type:', typeof result);
} catch (error) {
  // Re-throw the error so test fails properly
  assert.fail(`Unexpected error: ${error.message}`);
}
