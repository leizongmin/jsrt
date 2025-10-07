// Test Fetch API implementation - Web Standards Compliant
const assert = require('jsrt:assert');

// Test 1: Headers class - Complete API
const headers = new Headers();
assert.strictEqual(typeof headers, 'object', 'Headers should be an object');

// Test headers methods
headers.set('Content-Type', 'application/json');
headers.set('User-Agent', 'jsrt/1.0');

const hasContentType = headers.has('Content-Type');
const contentType = headers.get('Content-Type');
const hasAuth = headers.has('Authorization');
const authHeader = headers.get('Authorization');

assert.strictEqual(hasContentType, true, 'Should have Content-Type header');
assert.strictEqual(
  contentType,
  'application/json',
  'Content-Type should be application/json'
);
assert.strictEqual(hasAuth, false, 'Should not have Authorization header');
assert.strictEqual(authHeader, null, 'Non-existent header should return null');

// Test case insensitive headers
assert.strictEqual(
  headers.get('content-type'),
  'application/json',
  'Headers should be case-insensitive'
);
assert.strictEqual(
  headers.has('user-agent'),
  true,
  'Headers should be case-insensitive'
);

// Test header deletion
headers.delete('User-Agent');
assert.strictEqual(
  headers.has('User-Agent'),
  false,
  'Deleted header should not exist'
);

// Test header overwriting
headers.set('Content-Type', 'application/xml');
assert.strictEqual(
  headers.get('Content-Type'),
  'application/xml',
  'Header should be overwritten'
);

// Test 2: Request class - Web Standard API
const request1 = new Request('http://httpbin.org/get');
assert.strictEqual(typeof request1, 'object', 'Request should be an object');
assert.strictEqual(request1.method, 'GET', 'Default method should be GET');
assert.strictEqual(request1.url, 'http://httpbin.org/get', 'URL should match');

// Test request with options
const request2 = new Request('http://httpbin.org/post', {
  method: 'POST',
});
assert.strictEqual(
  typeof request2,
  'object',
  'POST Request should be an object'
);
assert.strictEqual(request2.method, 'POST', 'Method should be POST');

// Test 3: Response class - Web Standard API
const response = new Response();
assert.strictEqual(typeof response, 'object', 'Response should be an object');
assert.strictEqual(response.status, 200, 'Default status should be 200');
assert.strictEqual(response.ok, true, 'Default ok should be true');

// Test 3.1: Response methods return Promises
const textPromise = response.text();
const jsonPromise = response.json();
const arrayBufferPromise = response.arrayBuffer();
const blobPromise = response.blob();

const isTextPromise = textPromise && typeof textPromise.then === 'function';
const isJsonPromise = jsonPromise && typeof jsonPromise.then === 'function';
const isArrayBufferPromise =
  arrayBufferPromise && typeof arrayBufferPromise.then === 'function';
const isBlobPromise = blobPromise && typeof blobPromise.then === 'function';

assert.strictEqual(isTextPromise, true, 'text() should return a Promise');
assert.strictEqual(isJsonPromise, true, 'json() should return a Promise');
assert.strictEqual(
  isArrayBufferPromise,
  true,
  'arrayBuffer() should return a Promise'
);
assert.strictEqual(isBlobPromise, true, 'blob() should return a Promise');

// Test promise resolution (async)
async function testResponsePromises() {
  try {
    const text = await textPromise;
  } catch (e) {
    // Expected for empty response
  }

  try {
    const buffer = await arrayBufferPromise;
  } catch (e) {
    // Expected error
  }

  try {
    const blob = await blobPromise;
  } catch (e) {
    // Expected error
  }
}

// Execute async tests
testResponsePromises();

// Test 4: Fetch function return types
try {
  // Test basic GET request
  const getPromise = fetch('http://example.com/get');
  assert.strictEqual(
    typeof getPromise,
    'object',
    'fetch should return an object'
  );
  assert.strictEqual(
    typeof getPromise.then,
    'function',
    'fetch should return a Promise'
  );

  // Test POST with body
  const postHeaders = new Headers();
  postHeaders.set('Content-Type', 'application/json');

  const postPromise = fetch('http://example.com/post', {
    method: 'POST',
    headers: postHeaders,
    body: JSON.stringify({ test: 'data', timestamp: Date.now() }),
  });
  assert.strictEqual(
    typeof postPromise,
    'object',
    'POST fetch should return an object'
  );

  // Test with plain object headers
  const plainHeadersPromise = fetch('http://example.com/api', {
    method: 'PUT',
    headers: {
      Authorization: 'Bearer token123',
      'Content-Type': 'application/json',
    },
    body: '{"updated": true}',
  });
  assert.strictEqual(
    typeof plainHeadersPromise,
    'object',
    'PUT fetch should return an object'
  );
} catch (error) {
  console.error('Fetch error:', error.message);
}
