const module = require('node:module');

console.log('Testing findPackageJSON with simple case...');
console.log('Current directory:', process.cwd());

try {
  // Try with current directory - should either find a package.json or return undefined
  const result = module.findPackageJSON('./test_debug_package.js');
  console.log('✅ SUCCESS: Result:', result);
} catch (error) {
  console.log('❌ ERROR:', error.message);
  console.log('Stack:', error.stack);
}
