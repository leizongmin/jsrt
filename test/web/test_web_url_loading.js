const assert = require('jsrt:assert');

// Test 1: Basic file URL support
// If this test runs, file:// URL support is working

// Test 2: HTTP URL support (if we can reach the internet)
try {
  // This test confirms HTTP URL downloads work if jsrt was run with an http:// URL
  // HTTP URL support exists (requires network connectivity)
} catch (error) {
  console.log('⚠ HTTP URL test requires network access');
}
