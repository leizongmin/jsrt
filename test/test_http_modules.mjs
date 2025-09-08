import assert from 'jsrt:assert';

console.log('=== HTTP Module Loading Tests with Real Imports ===');

function runTests() {
  // Test 1: Check that HTTP module loading is enabled by default
  console.log('\n--- Test 1: HTTP Module Loading Enabled by Default ---');
  console.log('Testing that HTTP module loading is now enabled by default...');

  try {
    // Test with HTTPS (most reliable)
    const testResult = require('https://cdn.jsdelivr.net/npm/lodash@4.17.21/lodash.min.js');
    console.log(
      'Module loaded:',
      testResult && testResult.VERSION
        ? `Lodash v${testResult.VERSION}`
        : 'module loaded'
    );
    console.log('✅ HTTP module loading is fully functional with network access');
  } catch (error) {
    const errorMsg = error.message || '';
    if (
      errorMsg.includes('HTTP module loading is disabled') ||
      errorMsg.includes('protocol not allowed') ||
      errorMsg.includes('Domain not in allowlist')
    ) {
      console.log('❌ HTTP module loading functionality is disabled');
      throw error;
    } else {
      console.log('❌ CRITICAL: HTTP module loading enabled but not working');
      console.log('   Error:', errorMsg);
      throw error;
    }
  }

  console.log('✅ HTTP module loading functionality is enabled and network is available');

  // Test 2: Test actual module loading from supported CDNs
  console.log('\n--- Test 2: Real Module Loading from CDNs ---');

  // Test 2a: jsDelivr (CommonJS format)
  console.log('\n--- Test 2a: Loading lodash from cdn.jsdelivr.net ---');
  const lodashJsd = require('https://cdn.jsdelivr.net/npm/lodash@4.17.21/lodash.min.js');
  console.log('✅ Successfully loaded lodash from jsDelivr');
  console.log('Lodash version:', lodashJsd.VERSION);
  console.log('Testing lodashJsd.uniq([1,2,2,3]):', JSON.stringify(lodashJsd.uniq([1, 2, 2, 3])));
  assert.ok(lodashJsd, 'Lodash from jsDelivr should be loaded');
  assert.ok(typeof lodashJsd.uniq === 'function', 'Lodash uniq function should exist');

  // Test 2b: unpkg (CommonJS format)
  console.log('\n--- Test 2b: Loading lodash from unpkg.com ---');
  const lodashUnpkg = require('https://unpkg.com/lodash@4.17.21/lodash.min.js');
  console.log('✅ Successfully loaded lodash from unpkg');
  console.log('Lodash version:', lodashUnpkg.VERSION);
  console.log('Testing lodashUnpkg.range(1, 5):', JSON.stringify(lodashUnpkg.range(1, 5)));
  assert.ok(lodashUnpkg, 'Lodash from unpkg should be loaded');
  assert.ok(typeof lodashUnpkg.range === 'function', 'Lodash range function should exist');

  // Test 3: Mixed module systems
  console.log('\n--- Test 3: Mixed Module System Integration ---');
  const jsrtAssert = require('jsrt:assert');
  console.log('✅ Local jsrt:assert module loaded successfully');
  assert.ok(jsrtAssert, 'jsrt:assert should be loaded');
  console.log('✅ Successfully demonstrated mixed local + HTTP module loading');

  console.log('\n=== All HTTP Module Loading Tests Completed Successfully ===');
  console.log('🎯 HTTP module loading is fully functional');
}

// Run the tests
try {
  runTests();
} catch (error) {
  console.error('Test failed:', error.message);
  process.exit(1);
}
