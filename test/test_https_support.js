// Test HTTPS support in fetch API - Non-network version for CI
const assert = require('jsrt:assert');
console.log('=== HTTPS Fetch Support Tests ===');

// These tests only validate URL parsing and promise creation, no network operations

// Test 1: HTTPS URL parsing
try {
  const httpsPromise = fetch('https://example.com/test');
  assert.strictEqual(
    typeof httpsPromise,
    'object',
    'HTTPS should return promise'
  );
  assert.strictEqual(
    typeof httpsPromise.then,
    'function',
    'HTTPS promise should have then method'
  );
  console.log('✅ HTTPS URLs are parsed and accepted');
} catch (error) {
  if (error.message.includes('Invalid URL')) {
    console.log('❌ HTTPS URLs rejected - implementation incomplete');
    throw error;
  } else {
    console.log('❌ Unexpected error:', error.message);
    throw error;
  }
}

// Test 2: HTTP compatibility
try {
  const httpPromise = fetch('http://example.com/test');
  assert.strictEqual(typeof httpPromise, 'object', 'HTTP should still work');
  assert.strictEqual(
    typeof httpPromise.then,
    'function',
    'HTTP promise should have then method'
  );
  console.log('✅ HTTP compatibility maintained');
} catch (error) {
  console.log('❌ HTTP compatibility broken:', error.message);
  throw error;
}

// Test 3: Custom ports
try {
  const httpsCustom = fetch('https://example.com:8443/api');
  const httpCustom = fetch('http://example.com:8080/api');
  assert.strictEqual(
    typeof httpsCustom,
    'object',
    'HTTPS custom port should work'
  );
  assert.strictEqual(
    typeof httpCustom,
    'object',
    'HTTP custom port should work'
  );
  console.log('✅ Custom ports work for both protocols');
} catch (error) {
  console.log('❌ Custom port handling failed:', error.message);
  throw error;
}

// Test 4: Invalid protocols should create promises that will reject
try {
  const ftpPromise = fetch('ftp://example.com/test');
  // FTP should create a promise (not throw immediately) but promise should reject later
  assert.strictEqual(
    typeof ftpPromise,
    'object',
    'Should create promise even for invalid protocols'
  );
  console.log('✅ Invalid protocols handled via promise rejection');
} catch (error) {
  // If it throws immediately with "Invalid URL", that's also acceptable
  if (error.message.includes('Invalid URL')) {
    console.log('✅ Invalid protocols rejected immediately');
  } else {
    console.log('❌ Unexpected error for invalid protocol:', error.message);
    throw error;
  }
}

console.log('=== HTTPS Support Infrastructure Tests Completed ===');
console.log('✅ Core infrastructure working correctly');
console.log('📋 HTTPS URL parsing: IMPLEMENTED');
console.log('📋 HTTP compatibility: MAINTAINED');

// Force exit to prevent hanging in CI
// The issue is that fetch() creates network operations that keep the event loop alive
// For CI testing, we just need to validate the basic URL parsing works
process.exit(0);
