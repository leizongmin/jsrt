// Test URL and URLSearchParams implementation
const assert = require('jsrt:assert');

// Test 1: Basic URL construction
const url1 = new URL('https://example.com/path?param=value#hash');
assert.strictEqual(typeof url1, 'object', 'URL should be an object');
assert.strictEqual(
  url1.href,
  'https://example.com/path?param=value#hash',
  'href should match input'
);
assert.strictEqual(url1.protocol, 'https:', 'protocol should be https:');
assert.strictEqual(url1.host, 'example.com', 'host should be example.com');
assert.strictEqual(
  url1.hostname,
  'example.com',
  'hostname should be example.com'
);
assert.strictEqual(url1.pathname, '/path', 'pathname should be /path');
assert.strictEqual(
  url1.search,
  '?param=value',
  'search should be ?param=value'
);
assert.strictEqual(url1.hash, '#hash', 'hash should be #hash');
assert.strictEqual(
  url1.origin,
  'https://example.com',
  'origin should be https://example.com'
);

// Test 2: URL with port
const url2 = new URL('http://localhost:3000/api');

// Test 3: URL toString and toJSON
url1.toString();
url1.toJSON();

// Test 4: Basic URLSearchParams
const params1 = new URLSearchParams('name=John&age=30&city=New York');

// Test 5: URLSearchParams methods
const params2 = new URLSearchParams();

params2.set('foo', 'bar');
params2.set('baz', 'qux');

params2.append('foo', 'bar2');

params2.delete('baz');

// Test 6: URLSearchParams from query string
const params3 = new URLSearchParams('?a=1&b=2&a=3');

// Test 7: Empty and edge cases
const emptyParams = new URLSearchParams('');

const emptyParams2 = new URLSearchParams();

// Test 8: URL with empty components
try {
  const simpleUrl = new URL('http://example.com');
} catch (e) {
  console.log('Error creating simple URL:', e.message);
}

// Test 9: Complex query parameters
const complexParams = new URLSearchParams();
complexParams.set('filter[status]', 'active');
complexParams.set('sort', 'name');
complexParams.append('tags', 'javascript');
complexParams.append('tags', 'web');
