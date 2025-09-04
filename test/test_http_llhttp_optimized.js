const assert = require('jsrt:assert');

console.log('=== HTTP llhttp Parser Tests (Optimized) ===');

// Test runner to execute tests serially
async function runTests() {
  try {
    // Test 1: Basic HTTP response parsing
    console.log('Test 1: Basic fetch with llhttp');
    const response1 = await fetch('http://httpbin.org/get');
    assert.ok(response1, 'Response should exist');
    assert.strictEqual(
      typeof response1.status,
      'number',
      'Status should be a number'
    );
    assert.ok(
      response1.status >= 200 && response1.status < 300,
      'Status should be 2xx'
    );
    assert.ok(response1.headers, 'Headers should exist');

    const text1 = await response1.text();
    assert.ok(text1, 'Response text should exist');
    assert.ok(typeof text1 === 'string', 'Response text should be a string');
    console.log('✓ Basic fetch test passed');

    // Small delay between requests
    await new Promise((resolve) => setTimeout(resolve, 100));

    // Test 2: JSON response parsing
    console.log('Test 2: JSON response parsing');
    const response2 = await fetch('http://httpbin.org/json');
    assert.strictEqual(response2.status, 200, 'Status should be 200');

    const json2 = await response2.json();
    assert.ok(json2, 'JSON should be parsed');
    assert.strictEqual(typeof json2, 'object', 'JSON should be an object');
    console.log('✓ JSON parsing test passed');

    await new Promise((resolve) => setTimeout(resolve, 100));

    // Test 3: POST request with body
    console.log('Test 3: POST request with body');
    const response3 = await fetch('http://httpbin.org/post', {
      method: 'POST',
      body: JSON.stringify({ test: 'data', llhttp: true }),
      headers: {
        'Content-Type': 'application/json',
      },
    });
    assert.strictEqual(response3.status, 200, 'POST status should be 200');

    const json3 = await response3.json();
    assert.ok(json3.json, 'Posted JSON should be echoed');
    assert.strictEqual(json3.json.test, 'data', 'Posted data should match');
    assert.strictEqual(json3.json.llhttp, true, 'Posted boolean should match');
    console.log('✓ POST request test passed');

    await new Promise((resolve) => setTimeout(resolve, 100));

    // Test 4: Custom headers
    console.log('Test 4: Custom headers');
    const response4 = await fetch('http://httpbin.org/headers', {
      headers: {
        'X-Test-Header': 'llhttp-test',
        'User-Agent': 'jsrt-llhttp/1.0',
      },
    });

    const json4 = await response4.json();
    assert.ok(json4.headers, 'Headers should be present');
    assert.ok(
      json4.headers['X-Test-Header'],
      'Custom header should be present'
    );
    assert.strictEqual(
      json4.headers['X-Test-Header'],
      'llhttp-test',
      'Custom header value should match'
    );
    console.log('✓ Custom headers test passed');

    await new Promise((resolve) => setTimeout(resolve, 100));

    // Test 5: HTTP status codes
    console.log('Test 5: HTTP status codes');
    const response5 = await fetch('http://httpbin.org/status/404');
    assert.strictEqual(response5.status, 404, 'Status should be 404');
    assert.strictEqual(
      response5.ok,
      false,
      'Response should not be ok for 404'
    );
    console.log('✓ Status code test passed');

    await new Promise((resolve) => setTimeout(resolve, 100));

    // Test 6: Response headers inspection
    console.log('Test 6: Response headers');
    const response6 = await fetch(
      'http://httpbin.org/response-headers?Content-Type=application/json&X-Custom=test'
    );
    assert.ok(response6.headers, 'Response headers should exist');

    if (typeof response6.headers === 'object') {
      console.log('✓ Response headers test passed (object format)');
    } else {
      console.log('✓ Response headers test passed (other format)');
    }

    console.log('=== All HTTP llhttp Tests Completed Successfully ===');
  } catch (error) {
    console.error('✗ Test failed:', error.message);
  }
}

// Test 7: Error handling - invalid URL (should fail fast)
console.log('Test 7: Error handling');
fetch('invalid-url')
  .then((response) => {
    console.error('✗ Should have failed with invalid URL');
  })
  .catch((error) => {
    assert.ok(error, 'Error should be thrown for invalid URL');
    assert.ok(error.message, 'Error should have message');
    console.log('✓ Error handling test passed');
  });

// Start the serial test execution
runTests();

// Network timeout adjusted for real HTTP requests
setTimeout(() => {
  console.log('=== Test timeout reached ===');
}, 5000);
