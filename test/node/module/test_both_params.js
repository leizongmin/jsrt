const module = require('node:module');

console.log('Testing with both parameters...');

try {
  const result = module.findPackageJSON('./test_both_params.js', '/tmp');
  console.log('Result:', result);
} catch (error) {
  console.log('Error:', error.message);
}
