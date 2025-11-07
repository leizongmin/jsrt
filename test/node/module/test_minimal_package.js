const assert = require('jsrt:assert');
const module = require('node:module');

console.log('Starting test...');

try {
  // Try to call the function with minimal parameters
  const result = module.findPackageJSON('.');
  // Test should return a valid result (object, string path, or undefined)
  assert(typeof result === 'object' || typeof result === 'string' || result === undefined,
         `Expected object, string, or undefined, got ${typeof result}: ${result}`);

  if (typeof result === 'string') {
    console.log('✅ Test passed - Returned package.json path:', result);
  } else if (typeof result === 'object') {
    console.log('✅ Test passed - Returned parsed package object');
  } else {
    console.log('✅ Test passed - No package.json found (undefined)');
  }
} catch (error) {
  // Re-throw the error so test fails properly
  assert.fail(`Unexpected error: ${error.message}`);
}
