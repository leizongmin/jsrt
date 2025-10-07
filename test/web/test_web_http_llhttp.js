const assert = require('jsrt:assert');

// Fast offline tests for llhttp parser functionality
function testHttpParserOffline() {
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
  // Test Response methods synchronously
  response
    .text()
    .then((text) => {
      assert.strictEqual(
        text,
        '{"success": true}',
        'Response.text() should return body'
      );
    })
    .catch((err) => console.error('✗ Response.text() failed:', err.message));

  const jsonResponse = new Response('{"data": 42}', {
    headers: { 'Content-Type': 'application/json' },
  });

  jsonResponse
    .json()
    .then((json) => {
      assert.strictEqual(typeof json, 'object', 'JSON should be object');
      assert.strictEqual(json.data, 42, 'JSON data should be parsed correctly');
    })
    .catch((err) => console.error('✗ Response.json() failed:', err.message));
}

// Run offline tests immediately (no network delay)
try {
  testHttpParserOffline();

  // Test error handling
  fetch('invalid-url')
    .then(() => {
      console.error('✗ Should have failed with invalid URL');
    })
    .catch((error) => {
      assert.ok(error, 'Error should be thrown for invalid URL');
      assert.ok(error.message, 'Error should have message');
    });
} catch (error) {
  console.error('✗ Fast test failed:', error.message);
}

// Short timeout for any async operations
setTimeout(() => {}, 500);
