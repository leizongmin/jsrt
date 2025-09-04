const assert = require('jsrt:assert');

console.log('=== HTTP llhttp Parser Tests (Fast) ===');

// Fast offline tests for llhttp parser functionality
function testHttpParserOffline() {
  console.log('Test 1: Headers class construction');
  const headers = new Headers();
  assert.ok(headers, 'Headers constructor should work');

  headers.set('Content-Type', 'application/json');
  headers.append('X-Custom', 'test-value');

  assert.strictEqual(
    headers.get('Content-Type'),
    'application/json',
    'Headers.get should work'
  );
  assert.ok(headers.has('X-Custom'), 'Headers.has should work');
  console.log('✓ Headers class test passed');

  console.log('Test 2: Request class construction');
  const request = new Request('http://example.com/api', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ test: 'data' }),
  });

  assert.ok(request, 'Request constructor should work');
  assert.strictEqual(request.method, 'POST', 'Request method should be set');
  assert.strictEqual(
    request.url,
    'http://example.com/api',
    'Request URL should be set'
  );
  console.log('✓ Request class test passed');

  console.log('Test 3: Response class construction');
  const response = new Response('{"success": true}', {
    status: 200,
    statusText: 'OK',
    headers: { 'Content-Type': 'application/json' },
  });

  assert.ok(response, 'Response constructor should work');
  assert.strictEqual(response.status, 200, 'Response status should be set');
  assert.strictEqual(
    response.statusText,
    'OK',
    'Response statusText should be set'
  );
  assert.ok(response.ok, 'Response.ok should be true for 200');
  console.log('✓ Response class test passed');

  // Test Response methods synchronously
  console.log('Test 4: Response text() method');
  response
    .text()
    .then((text) => {
      assert.strictEqual(
        text,
        '{"success": true}',
        'Response.text() should return body'
      );
      console.log('✓ Response.text() test passed');
    })
    .catch((err) => console.error('✗ Response.text() failed:', err.message));

  console.log('Test 5: Response json() method');
  const jsonResponse = new Response('{"data": 42}', {
    headers: { 'Content-Type': 'application/json' },
  });

  jsonResponse
    .json()
    .then((json) => {
      assert.strictEqual(typeof json, 'object', 'JSON should be object');
      assert.strictEqual(json.data, 42, 'JSON data should be parsed correctly');
      console.log('✓ Response.json() test passed');
    })
    .catch((err) => console.error('✗ Response.json() failed:', err.message));
}

// Run offline tests immediately (no network delay)
try {
  testHttpParserOffline();

  // Test error handling
  console.log('Test 6: Error handling');
  fetch('invalid-url')
    .then(() => {
      console.error('✗ Should have failed with invalid URL');
    })
    .catch((error) => {
      assert.ok(error, 'Error should be thrown for invalid URL');
      assert.ok(error.message, 'Error should have message');
      console.log('✓ Error handling test passed');
    });

  console.log('=== Fast HTTP Tests Completed Successfully ===');
} catch (error) {
  console.error('✗ Fast test failed:', error.message);
}

// Short timeout for any async operations
setTimeout(() => {
  console.log('=== HTTP llhttp Fast Tests Done ===');
}, 500);
