const module = require('node:module');

console.log('Testing with /tmp directory...');

try {
  const result = module.findPackageJSON('/tmp');
  console.log('Result:', result);
  console.log('Type:', typeof result);
  if (result === undefined) {
    console.log(
      '✅ Correctly returned undefined for directory without package.json'
    );
  } else {
    console.log('❌ Expected undefined, got:', result);
  }
} catch (error) {
  console.log('Error:', error.message);
}
