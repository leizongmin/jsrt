// Test Legacy URL API from node:url module
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

// Test 1: url.parse function exists
assert(typeof url.parse === 'function', 'url.parse should be exported');
assert(typeof url.format === 'function', 'url.format should be exported');
assert(typeof url.resolve === 'function', 'url.resolve should be exported');

// Test 2: url.parse basic functionality
try {
  const parsed = url.parse('http://user:pass@host.com:8080/path?query=1#hash');
  assertEquals(parsed.protocol, 'http:', 'parse: protocol');
  assertEquals(parsed.auth, 'user:pass', 'parse: auth');
  assertEquals(parsed.hostname, 'host.com', 'parse: hostname');
  assertEquals(parsed.port, '8080', 'parse: port');
  assertEquals(parsed.pathname, '/path', 'parse: pathname');
  assertEquals(parsed.search, '?query=1', 'parse: search');
  assertEquals(parsed.query, 'query=1', 'parse: query');
  assertEquals(parsed.hash, '#hash', 'parse: hash');
  assert(parsed.slashes === true, 'parse: slashes');
  assertEquals(parsed.path, '/path?query=1', 'parse: path');
} catch (e) {
  console.log('FAIL: url.parse basic test failed:', e.message);
  failed += 10;
}

// Test 3: url.parse with parseQueryString = true
try {
  const parsed2 = url.parse('http://example.com?a=1&b=2', true);
  assert(typeof parsed2.query === 'object', 'parse: query is object');
  assertEquals(parsed2.query.a, '1', 'parse: query.a');
  assertEquals(parsed2.query.b, '2', 'parse: query.b');
} catch (e) {
  console.log('FAIL: url.parse with parseQueryString failed:', e.message);
  failed += 3;
}

// Test 4: url.parse without auth
try {
  const parsed3 = url.parse('http://example.com/path');
  assert(
    parsed3.auth === null || parsed3.auth === undefined,
    'parse: auth null'
  );
  assertEquals(parsed3.hostname, 'example.com', 'parse: hostname without auth');
} catch (e) {
  console.log('FAIL: url.parse without auth failed:', e.message);
  failed += 2;
}

// Test 5: url.parse with only hostname
try {
  const parsed4 = url.parse('http://example.com');
  assertEquals(parsed4.hostname, 'example.com', 'parse: hostname only');
  assertEquals(parsed4.pathname, '/', 'parse: default pathname');
} catch (e) {
  console.log('FAIL: url.parse hostname only failed:', e.message);
  failed += 2;
}

// Test 6: url.format with legacy object
try {
  const formatted = url.format({
    protocol: 'http:',
    hostname: 'example.com',
    pathname: '/path',
    search: '?query=1',
  });
  assertEquals(formatted, 'http://example.com/path?query=1', 'format: basic');
} catch (e) {
  console.log('FAIL: url.format basic failed:', e.message);
  failed++;
}

// Test 7: url.format with auth
try {
  const formatted2 = url.format({
    protocol: 'http:',
    auth: 'user:pass',
    hostname: 'example.com',
    pathname: '/path',
  });
  assertEquals(
    formatted2,
    'http://user:pass@example.com/path',
    'format: with auth'
  );
} catch (e) {
  console.log('FAIL: url.format with auth failed:', e.message);
  failed++;
}

// Test 8: url.format with port
try {
  const formatted3 = url.format({
    protocol: 'http:',
    hostname: 'example.com',
    port: '8080',
    pathname: '/path',
  });
  assertEquals(formatted3, 'http://example.com:8080/path', 'format: with port');
} catch (e) {
  console.log('FAIL: url.format with port failed:', e.message);
  failed++;
}

// Test 9: url.format with URL object (WHATWG)
try {
  const u = new url.URL('https://example.com/path?query=1');
  const formatted4 = url.format(u);
  assertEquals(
    formatted4,
    'https://example.com/path?query=1',
    'format: URL object'
  );
} catch (e) {
  console.log('FAIL: url.format with URL object failed:', e.message);
  failed++;
}

// Test 10: url.format with href (highest priority)
try {
  const formatted5 = url.format({
    href: 'http://override.com/path',
    hostname: 'ignored.com',
  });
  assertEquals(formatted5, 'http://override.com/path', 'format: href priority');
} catch (e) {
  console.log('FAIL: url.format href priority failed:', e.message);
  failed++;
}

// Test 11: url.resolve basic
try {
  const resolved = url.resolve('http://example.com/foo', 'bar');
  assertEquals(resolved, 'http://example.com/bar', 'resolve: basic');
} catch (e) {
  console.log('FAIL: url.resolve basic failed:', e.message);
  failed++;
}

// Test 12: url.resolve with trailing slash
try {
  const resolved2 = url.resolve('http://example.com/foo/', 'bar');
  assertEquals(
    resolved2,
    'http://example.com/foo/bar',
    'resolve: trailing slash'
  );
} catch (e) {
  console.log('FAIL: url.resolve with trailing slash failed:', e.message);
  failed++;
}

// Test 13: url.resolve with absolute path
try {
  const resolved3 = url.resolve('http://example.com/foo', '/bar');
  assertEquals(resolved3, 'http://example.com/bar', 'resolve: absolute path');
} catch (e) {
  console.log('FAIL: url.resolve absolute path failed:', e.message);
  failed++;
}

// Test 14: url.resolve with parent directory
try {
  const resolved4 = url.resolve('http://example.com/foo/bar', '../baz');
  assertEquals(
    resolved4,
    'http://example.com/baz',
    'resolve: parent directory'
  );
} catch (e) {
  console.log('FAIL: url.resolve parent directory failed:', e.message);
  failed++;
}

// Test 15: url.resolve with absolute URL
try {
  const resolved5 = url.resolve(
    'http://example.com/foo',
    'http://other.com/bar'
  );
  assertEquals(resolved5, 'http://other.com/bar', 'resolve: absolute URL');
} catch (e) {
  console.log('FAIL: url.resolve absolute URL failed:', e.message);
  failed++;
}

// Test 16: url.parse and url.format round-trip
try {
  const original = 'http://user:pass@example.com:8080/path?query=1#hash';
  const parsed = url.parse(original);
  const formatted = url.format(parsed);
  // Note: Format may produce slightly different output, so just check key components
  assert(formatted.includes('example.com'), 'round-trip: hostname preserved');
  assert(formatted.includes('/path'), 'round-trip: path preserved');
  assert(formatted.includes('query=1'), 'round-trip: query preserved');
} catch (e) {
  console.log('FAIL: parse/format round-trip failed:', e.message);
  failed += 3;
}

// Summary
console.log(`\nLegacy API Tests: ${passed} passed, ${failed} failed`);
if (failed === 0) {
  console.log('All Legacy API tests passed!');
}
