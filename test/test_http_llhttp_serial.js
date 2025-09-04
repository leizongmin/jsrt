const assert = require('jsrt:assert');

console.log('=== HTTP llhttp Parser Tests (Serial) ===');

let testsPassed = 0;
let testsTotal = 7;

function runNextTest(testIndex) {
  if (testIndex >= testsTotal) {
    console.log(
      `=== All Tests Completed (${testsPassed}/${testsTotal} passed) ===`
    );
    return;
  }

  // Delay between tests
  setTimeout(
    () => {
      switch (testIndex) {
        case 0:
          // Test 1: Basic HTTP response parsing
          console.log('Test 1: Basic fetch with llhttp');
          fetch('http://httpbin.org/get')
            .then((response) => {
              assert.ok(response, 'Response should exist');
              assert.strictEqual(
                typeof response.status,
                'number',
                'Status should be a number'
              );
              assert.ok(
                response.status >= 200 && response.status < 300,
                'Status should be 2xx'
              );
              assert.ok(response.headers, 'Headers should exist');
              return response.text();
            })
            .then((text) => {
              assert.ok(text, 'Response text should exist');
              assert.ok(
                typeof text === 'string',
                'Response text should be a string'
              );
              console.log('✓ Basic fetch test passed');
              testsPassed++;
            })
            .catch((error) => {
              console.error('✗ Basic fetch test failed:', error.message);
            })
            .finally(() => runNextTest(testIndex + 1));
          break;

        case 1:
          // Test 2: JSON response parsing
          console.log('Test 2: JSON response parsing');
          fetch('http://httpbin.org/json')
            .then((response) => {
              assert.strictEqual(response.status, 200, 'Status should be 200');
              return response.json();
            })
            .then((json) => {
              assert.ok(json, 'JSON should be parsed');
              assert.strictEqual(
                typeof json,
                'object',
                'JSON should be an object'
              );
              console.log('✓ JSON parsing test passed');
              testsPassed++;
            })
            .catch((error) => {
              console.error('✗ JSON test failed:', error.message);
            })
            .finally(() => runNextTest(testIndex + 1));
          break;

        case 2:
          // Test 3: POST request with body
          console.log('Test 3: POST request with body');
          fetch('http://httpbin.org/post', {
            method: 'POST',
            body: JSON.stringify({ test: 'data', llhttp: true }),
            headers: {
              'Content-Type': 'application/json',
            },
          })
            .then((response) => {
              assert.strictEqual(
                response.status,
                200,
                'POST status should be 200'
              );
              return response.json();
            })
            .then((json) => {
              assert.ok(json.json, 'Posted JSON should be echoed');
              assert.strictEqual(
                json.json.test,
                'data',
                'Posted data should match'
              );
              assert.strictEqual(
                json.json.llhttp,
                true,
                'Posted boolean should match'
              );
              console.log('✓ POST request test passed');
              testsPassed++;
            })
            .catch((error) => {
              console.error('✗ POST test failed:', error.message);
            })
            .finally(() => runNextTest(testIndex + 1));
          break;

        case 3:
          // Test 4: Custom headers
          console.log('Test 4: Custom headers');
          fetch('http://httpbin.org/headers', {
            headers: {
              'X-Test-Header': 'llhttp-test',
              'User-Agent': 'jsrt-llhttp/1.0',
            },
          })
            .then((response) => {
              return response.json();
            })
            .then((json) => {
              assert.ok(json.headers, 'Headers should be present');
              assert.ok(
                json.headers['X-Test-Header'],
                'Custom header should be present'
              );
              assert.strictEqual(
                json.headers['X-Test-Header'],
                'llhttp-test',
                'Custom header value should match'
              );
              console.log('✓ Custom headers test passed');
              testsPassed++;
            })
            .catch((error) => {
              console.error('✗ Custom headers test failed:', error.message);
            })
            .finally(() => runNextTest(testIndex + 1));
          break;

        case 4:
          // Test 5: Error handling - invalid URL
          console.log('Test 5: Error handling');
          fetch('invalid-url')
            .then((response) => {
              console.error('✗ Should have failed with invalid URL');
            })
            .catch((error) => {
              assert.ok(error, 'Error should be thrown for invalid URL');
              assert.ok(error.message, 'Error should have message');
              console.log('✓ Error handling test passed');
              testsPassed++;
            })
            .finally(() => runNextTest(testIndex + 1));
          break;

        case 5:
          // Test 6: HTTP status codes
          console.log('Test 6: HTTP status codes');
          fetch('http://httpbin.org/status/404')
            .then((response) => {
              assert.strictEqual(response.status, 404, 'Status should be 404');
              assert.strictEqual(
                response.ok,
                false,
                'Response should not be ok for 404'
              );
              console.log('✓ Status code test passed');
              testsPassed++;
            })
            .catch((error) => {
              console.error('✗ Status code test failed:', error.message);
            })
            .finally(() => runNextTest(testIndex + 1));
          break;

        case 6:
          // Test 7: Response headers inspection
          console.log('Test 7: Response headers');
          fetch(
            'http://httpbin.org/response-headers?Content-Type=application/json&X-Custom=test'
          )
            .then((response) => {
              assert.ok(response.headers, 'Response headers should exist');
              if (typeof response.headers === 'object') {
                console.log('✓ Response headers test passed (object format)');
              } else {
                console.log('✓ Response headers test passed (other format)');
              }
              testsPassed++;
            })
            .catch((error) => {
              console.error('✗ Response headers test failed:', error.message);
            })
            .finally(() => runNextTest(testIndex + 1));
          break;
      }
    },
    testIndex === 0 ? 0 : 800
  ); // 800ms delay between network tests
}

// Start the serial test execution
runNextTest(0);
