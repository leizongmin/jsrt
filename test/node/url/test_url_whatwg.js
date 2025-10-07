// Test WHATWG URL API from node:url module
const url = require('node:url');

let passed = 0;
let failed = 0;

function assert(condition, message) {
  if (condition) {
    passed++;
  } else {
    console.log('FAIL:', message);
    failed++;
  }
}

function assertEquals(actual, expected, message) {
  if (actual === expected) {
    passed++;
  } else {
    console.log(`FAIL: ${message}`);
    console.log(`  Expected: ${expected}`);
    console.log(`  Actual: ${actual}`);
    failed++;
  }
}

// Test 1: URL class export
assert(typeof url.URL === 'function', 'URL should be exported');

// Test 2: URLSearchParams class export
assert(
  typeof url.URLSearchParams === 'function',
  'URLSearchParams should be exported'
);

// Test 3: URL constructor with absolute URL
try {
  const u1 = new url.URL('https://example.com/path?a=1#hash');
  assertEquals(u1.href, 'https://example.com/path?a=1#hash', 'URL href');
  assertEquals(u1.protocol, 'https:', 'URL protocol');
  assertEquals(u1.hostname, 'example.com', 'URL hostname');
  assertEquals(u1.pathname, '/path', 'URL pathname');
  assertEquals(u1.search, '?a=1', 'URL search');
  assertEquals(u1.hash, '#hash', 'URL hash');
  assertEquals(u1.origin, 'https://example.com', 'URL origin');
} catch (e) {
  console.log('FAIL: URL constructor failed:', e.message);
  failed += 7;
}

// Test 4: URL with base
try {
  const u2 = new url.URL('/path', 'https://example.com');
  assertEquals(u2.href, 'https://example.com/path', 'URL with base href');
  assertEquals(u2.hostname, 'example.com', 'URL with base hostname');
} catch (e) {
  console.log('FAIL: URL with base failed:', e.message);
  failed += 2;
}

// Test 5: URL with username and password
try {
  const u3 = new url.URL('https://user:pass@example.com/path');
  assertEquals(u3.username, 'user', 'URL username');
  assertEquals(u3.password, 'pass', 'URL password');
} catch (e) {
  console.log('FAIL: URL with credentials failed:', e.message);
  failed += 2;
}

// Test 6: URL with port
try {
  const u4 = new url.URL('https://example.com:8080/path');
  assertEquals(u4.port, '8080', 'URL port');
  assertEquals(u4.host, 'example.com:8080', 'URL host');
} catch (e) {
  console.log('FAIL: URL with port failed:', e.message);
  failed += 2;
}

// Test 7: URLSearchParams basic operations
try {
  const params = new url.URLSearchParams('a=1&b=2&c=3');
  assertEquals(params.get('a'), '1', 'URLSearchParams get');
  assert(params.has('b'), 'URLSearchParams has');
  params.set('d', '4');
  assert(params.has('d'), 'URLSearchParams set');
  params.delete('c');
  assert(!params.has('c'), 'URLSearchParams delete');
} catch (e) {
  console.log('FAIL: URLSearchParams operations failed:', e.message);
  failed += 5;
}

// Test 8: URLSearchParams from object
try {
  const params2 = new url.URLSearchParams({ foo: 'bar', baz: 'qux' });
  assertEquals(
    params2.get('foo'),
    'bar',
    'URLSearchParams from object get foo'
  );
  assertEquals(
    params2.get('baz'),
    'qux',
    'URLSearchParams from object get baz'
  );
} catch (e) {
  console.log('FAIL: URLSearchParams from object failed:', e.message);
  failed += 2;
}

// Test 9: URL searchParams property
try {
  const u5 = new url.URL('https://example.com?a=1&b=2');
  const sp = u5.searchParams;
  assertEquals(sp.get('a'), '1', 'URL searchParams get a');
  assertEquals(sp.get('b'), '2', 'URL searchParams get b');
} catch (e) {
  console.log('FAIL: URL searchParams property failed:', e.message);
  failed += 2;
}

// Test 10: URL toString and toJSON
try {
  const u6 = new url.URL('https://example.com/path');
  assertEquals(u6.toString(), 'https://example.com/path', 'URL toString');
  assertEquals(u6.toJSON(), 'https://example.com/path', 'URL toJSON');
} catch (e) {
  console.log('FAIL: URL toString/toJSON failed:', e.message);
  failed += 2;
}

// Summary
console.log(`\nWHATWG API Tests: ${passed} passed, ${failed} failed`);
if (failed === 0) {
  console.log('All WHATWG API tests passed!');
}
