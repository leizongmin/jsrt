const module = require('node:module');

console.log('Starting test...');

try {
  // Try to call the function with minimal parameters
  const result = module.findPackageJSON('.');
  console.log('Result:', result);
} catch (error) {
  console.log('Error:', error.message);
}
