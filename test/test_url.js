// Test URL and URLSearchParams implementation
const assert = require('std:assert');
console.log('=== URL/URLSearchParams API Tests ===');

// Test 1: Basic URL construction
console.log('Test 1: Basic URL construction');
const url1 = new URL('https://example.com/path?param=value#hash');
console.log('URL created:', typeof url1);
assert.strictEqual(typeof url1, 'object', 'URL should be an object');
console.log('href:', url1.href);
assert.strictEqual(
  url1.href,
  'https://example.com/path?param=value#hash',
  'href should match input'
);
console.log('protocol:', url1.protocol);
assert.strictEqual(url1.protocol, 'https:', 'protocol should be https:');
console.log('host:', url1.host);
assert.strictEqual(url1.host, 'example.com', 'host should be example.com');
console.log('hostname:', url1.hostname);
assert.strictEqual(
  url1.hostname,
  'example.com',
  'hostname should be example.com'
);
console.log('port:', url1.port);
console.log('pathname:', url1.pathname);
assert.strictEqual(url1.pathname, '/path', 'pathname should be /path');
console.log('search:', url1.search);
assert.strictEqual(
  url1.search,
  '?param=value',
  'search should be ?param=value'
);
console.log('hash:', url1.hash);
assert.strictEqual(url1.hash, '#hash', 'hash should be #hash');
console.log('origin:', url1.origin);
assert.strictEqual(
  url1.origin,
  'https://example.com',
  'origin should be https://example.com'
);

// Test 2: URL with port
console.log('\nTest 2: URL with port');
const url2 = new URL('http://localhost:3000/api');
console.log('host:', url2.host);
console.log('hostname:', url2.hostname);
console.log('port:', url2.port);
console.log('pathname:', url2.pathname);

// Test 3: URL toString and toJSON
console.log('\nTest 3: URL toString and toJSON');
console.log('toString():', url1.toString());
console.log('toJSON():', url1.toJSON());

// Test 4: Basic URLSearchParams
console.log('\nTest 4: Basic URLSearchParams construction');
const params1 = new URLSearchParams('name=John&age=30&city=New York');
console.log('URLSearchParams created:', typeof params1);
console.log('get(name):', params1.get('name'));
console.log('get(age):', params1.get('age'));
console.log('get(city):', params1.get('city'));
console.log('get(missing):', params1.get('missing'));

// Test 5: URLSearchParams methods
console.log('\nTest 5: URLSearchParams methods');
const params2 = new URLSearchParams();
console.log('Initial toString():', params2.toString());

params2.set('foo', 'bar');
params2.set('baz', 'qux');
console.log('After set operations:', params2.toString());

console.log('has(foo):', params2.has('foo'));
console.log('has(missing):', params2.has('missing'));

params2.append('foo', 'bar2');
console.log('After append foo:', params2.toString());

params2.delete('baz');
console.log('After delete baz:', params2.toString());

// Test 6: URLSearchParams from query string
console.log('\nTest 6: URLSearchParams from query string');
const params3 = new URLSearchParams('?a=1&b=2&a=3');
console.log('From query string:', params3.toString());
console.log('get(a):', params3.get('a')); // Should return first value
console.log('has(b):', params3.has('b'));

// Test 7: Empty and edge cases
console.log('\nTest 7: Empty and edge cases');
const emptyParams = new URLSearchParams('');
console.log('Empty params toString():', emptyParams.toString());

const emptyParams2 = new URLSearchParams();
console.log('Constructor no args toString():', emptyParams2.toString());

// Test 8: URL with empty components
console.log('\nTest 8: URL with minimal components');
try {
  const simpleUrl = new URL('http://example.com');
  console.log('Simple URL href:', simpleUrl.href);
  console.log('Simple URL pathname:', simpleUrl.pathname);
  console.log('Simple URL search:', simpleUrl.search);
  console.log('Simple URL hash:', simpleUrl.hash);
} catch (e) {
  console.log('Error creating simple URL:', e.message);
}

// Test 9: Complex query parameters
console.log('\nTest 9: Complex query parameters');
const complexParams = new URLSearchParams();
complexParams.set('filter[status]', 'active');
complexParams.set('sort', 'name');
complexParams.append('tags', 'javascript');
complexParams.append('tags', 'web');
console.log('Complex params:', complexParams.toString());

console.log('\n=== All URL/URLSearchParams tests completed ===');
