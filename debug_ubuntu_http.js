console.log('=== Debug Ubuntu HTTP Module Loading ===');

try {
  console.log('Testing simple HTTP module loading...');
  
  // First try a very simple CDN resource
  console.log('1. Testing simple lodash from CDN...');
  const result = require('https://cdn.jsdelivr.net/npm/lodash@4.17.21/lodash.min.js');
  
  console.log('Result type:', typeof result);
  console.log('Result constructor:', result.constructor.name);
  console.log('Result keys (first 5):', Object.keys(result).slice(0, 5));
  
  if (result.VERSION) {
    console.log('✅ SUCCESS: Lodash version', result.VERSION);
  } else {
    console.log('⚠️ WARNING: No VERSION property found');
  }
  
} catch (error) {
  console.log('❌ ERROR details:');
  console.log('  Message:', error.message);
  console.log('  Stack:', error.stack);
  console.log('  Error type:', error.constructor.name);
}

console.log('\n2. Testing simple manual module...');
try {
  // Try a very simple manual module to isolate the issue
  const simple = require('https://unpkg.com/is-number@7.0.0/index.js');
  console.log('Simple module result:', typeof simple);
} catch (error) {
  console.log('Simple module error:', error.message);
}