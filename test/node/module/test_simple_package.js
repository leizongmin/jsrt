const module = require('node:module');

console.log('Testing node:module exports...');
console.log('Available properties:', Object.getOwnPropertyNames(module));

if (typeof module.findPackageJSON === 'function') {
  console.log('✅ findPackageJSON is available');

  // Simple test
  const result = module.findPackageJSON('./test_simple_package.js');
  console.log('Result:', result);
} else {
  console.log('❌ findPackageJSON is not available');
}

if (typeof module.parsePackageJSON === 'function') {
  console.log('✅ parsePackageJSON is available');
} else {
  console.log('❌ parsePackageJSON is not available');
}
