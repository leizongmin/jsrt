const assert = require('jsrt:assert');
const module = require('node:module');

console.log('Testing with /tmp directory...');

try {
  const result = module.findPackageJSON('/tmp');
  console.log('Result:', result);
  console.log('Type:', typeof result);

  // For /tmp directory, we expect undefined since there shouldn't be a package.json
  assert.strictEqual(result, undefined,
                     `Expected undefined for directory without package.json, got: ${result}`);
  console.log('✅ Correctly returned undefined for directory without package.json');
} catch (error) {
  // Re-throw the error so test fails properly
  assert.fail(`Unexpected error: ${error.message}`);
}
