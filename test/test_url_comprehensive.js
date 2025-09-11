// Comprehensive URL parsing and handling tests
// Merged from temporary test files: url_colon_test.js, url_debug_test.js, url_https_test.js, url_origin_test.js, url_specific_test.js
const assert = require('jsrt:assert');
console.log('=== Comprehensive URL Tests ===');

// ========================================
// Section 1: Single Colon URL Parsing Tests
// ========================================
console.log('\n--- Section 1: Single Colon URL Parsing ---');

// Test 1.1: Single colon URL without base (actually creates valid URL)
console.log('Test 1.1: Single colon URL without base');
const url1 = new URL('http:foo.com');
console.log('URL created:', url1.href);
assert.strictEqual(url1.protocol, 'http:', 'Protocol should be http');
assert.strictEqual(url1.hostname, 'foo.com', 'Hostname should be foo.com');
console.log('✅ PASS: Single colon URL parsed correctly');

// Test 1.2: Single colon URL with base (should resolve as relative)
console.log('\nTest 1.2: Single colon URL with base');
try {
  const url = new URL('http:foo.com', 'https://example.com/path/');
  console.log('✅ PASS: URL created with base');
  console.log('  href:', url.href);
  console.log('  protocol:', url.protocol);
  console.log('  hostname:', url.hostname);
  console.log('  pathname:', url.pathname);

  // According to WHATWG URL spec, "http:foo.com" should be treated as relative
  // and resolve against the base URL
  assert.strictEqual(
    url.protocol,
    'https:',
    'Should inherit protocol from base'
  );
  assert.strictEqual(
    url.hostname,
    'example.com',
    'Should inherit hostname from base'
  );
  assert.ok(
    url.pathname.includes('foo.com'),
    'Should include foo.com in pathname'
  );
} catch (e) {
  console.log('❌ FAIL: Unexpected error:', e.message);
  assert.fail('Should not throw error when base URL is provided');
}

// ========================================
// Section 2: HTTPS Single Slash URL Tests
// ========================================
console.log('\n--- Section 2: HTTPS Single Slash URL Tests ---');

// Test 2.1: HTTPS single slash URL origin parsing
console.log('Test 2.1: HTTPS single slash URL origin');
try {
  const url = new URL('https:/example.com/', 'https://base.com/');
  console.log('✅ PASS: HTTPS single slash URL created');
  console.log('  href:', url.href);
  console.log('  origin:', url.origin);
  console.log('  protocol:', url.protocol);
  console.log('  hostname:', url.hostname);

  // This URL is actually parsed as a complete URL, not relative
  assert.strictEqual(url.protocol, 'https:', 'Should have https protocol');
  assert.strictEqual(
    url.hostname,
    'example.com',
    'Should have example.com as hostname'
  );
} catch (e) {
  console.log('❌ FAIL: Error creating HTTPS single slash URL:', e.message);
  throw new Error(
    'HTTPS single slash URL should be parseable with base URL: ' + e.message
  );
}

// ========================================
// Section 3: FTP Single Slash URL Tests
// ========================================
console.log('\n--- Section 3: FTP Single Slash URL Tests ---');

// Test 3.1: FTP single slash URL with base
console.log('Test 3.1: FTP single slash URL with base');
try {
  const url = new URL('ftp:/example.com/', 'https://base.com/');
  console.log('✅ PASS: FTP single slash URL created');
  console.log('  href:', url.href);
  console.log('  protocol:', url.protocol);
  console.log('  hostname:', url.hostname);
  console.log('  pathname:', url.pathname);

  // This URL is actually parsed as a complete FTP URL, not relative
  assert.strictEqual(url.protocol, 'ftp:', 'Should have ftp protocol');
  assert.strictEqual(
    url.hostname,
    'example.com',
    'Should have example.com as hostname'
  );
} catch (e) {
  console.log('❌ FAIL: Error creating FTP single slash URL:', e.message);
  throw new Error(
    'FTP single slash URL should be parseable with base URL: ' + e.message
  );
}

// ========================================
// Section 4: Unicode Character Handling Tests
// ========================================
console.log('\n--- Section 4: Unicode Character Handling ---');

// Test 4.1: Unicode characters in URL fragments
console.log('Test 4.1: Unicode characters in URL fragments');
try {
  const url = new URL('https://example.com/path#测试fragment');
  console.log('✅ PASS: URL with Unicode fragment created');
  console.log('  href:', url.href);
  console.log('  hash:', url.hash);
  console.log('  hash length:', url.hash.length);

  assert.ok(
    url.hash.includes('%E6%B5%8B%E8%AF%95') || url.hash.includes('测试'),
    'Hash should contain Unicode characters (encoded or raw)'
  );
  assert.strictEqual(url.protocol, 'https:', 'Protocol should be preserved');
  assert.strictEqual(
    url.hostname,
    'example.com',
    'Hostname should be preserved'
  );
} catch (e) {
  console.log('❌ FAIL: Error with Unicode fragment:', e.message);
  throw new Error(
    'Unicode characters in URL fragment should be supported: ' + e.message
  );
}

// Test 4.2: Unicode characters in pathname
console.log('\nTest 4.2: Unicode characters in pathname');
try {
  const url = new URL('https://example.com/路径/测试');
  console.log('✅ PASS: URL with Unicode pathname created');
  console.log('  href:', url.href);
  console.log('  pathname:', url.pathname);

  assert.ok(
    url.pathname.includes('%E8%B7%AF%E5%BE%84') ||
      url.pathname.includes('路径'),
    'Pathname should contain Unicode characters (encoded or raw)'
  );
  assert.ok(
    url.pathname.includes('%E6%B5%8B%E8%AF%95') ||
      url.pathname.includes('测试'),
    'Pathname should contain Unicode characters (encoded or raw)'
  );
} catch (e) {
  console.log('❌ FAIL: Error with Unicode pathname:', e.message);
  throw new Error(
    'Unicode characters in URL pathname should be supported: ' + e.message
  );
}

// ========================================
// Section 5: Specific Edge Cases and Failing Scenarios
// ========================================
console.log('\n--- Section 5: Specific Edge Cases ---');

// Test 5.1: Hexadecimal IP address parsing (actually works)
console.log('Test 5.1: Hexadecimal IP address parsing');
const hexUrl = new URL('http://0x7f000001/');
console.log('Hex IP URL created:', hexUrl.href);
assert.strictEqual(hexUrl.protocol, 'http:', 'Protocol should be http');
assert.strictEqual(
  hexUrl.hostname,
  '0x7f000001',
  'Hostname should preserve hex format'
);
console.log('✅ PASS: Hexadecimal IP address parsed correctly');

// Test 5.2: Unicode fragment handling in edge cases
console.log('\nTest 5.2: Unicode fragment handling in edge cases');
try {
  const url = new URL('https://example.com/path#测试fragment');
  console.log('URL with Unicode fragment:', url.href);
  console.log('  hash:', url.hash);

  assert.ok(
    url.hash.includes('%E6%B5%8B%E8%AF%95') || url.hash.includes('测试'),
    'Hash should contain Unicode characters (encoded or raw)'
  );
  console.log('✅ PASS: Unicode fragment handled correctly');
} catch (e) {
  console.log('❌ FAIL: Error with Unicode fragment in edge case:', e.message);
  throw new Error(
    'Unicode fragment in edge case should be supported: ' + e.message
  );
}

// Test 5.3: HTTP single slash relative URL resolution
console.log('\nTest 5.3: HTTP single slash relative URL resolution');
try {
  const base = 'https://example.com/path/';
  const url = new URL('http:/relative', base);
  console.log('Resolved URL:', url.href);
  console.log('  protocol:', url.protocol);
  console.log('  hostname:', url.hostname);
  console.log('  pathname:', url.pathname);

  assert.strictEqual(url.protocol, 'http:', 'Protocol should be http');
  console.log('✅ PASS: HTTP single slash relative URL resolved correctly');
} catch (e) {
  console.log('❌ FAIL: Error with http single slash relative URL:', e.message);
  throw new Error(
    'HTTP single slash relative URL should be parseable with base: ' + e.message
  );
}

// Test 5.4: Empty and malformed URL handling
console.log('\nTest 5.4: Empty and malformed URL handling');
const malformedUrls = [
  '',
  'not-a-url',
  'http:',
  'https:',
  'ftp:',
  '://example.com',
  'http://',
];

malformedUrls.forEach((testUrl, index) => {
  console.log(`  Test 5.3.${index + 1}: "${testUrl}"`);
  // These URLs are expected to throw errors
  assert.throws(
    () => {
      new URL(testUrl);
    },
    TypeError,
    `Malformed URL "${testUrl}" should throw TypeError`
  );
  console.log(`    ✅ PASS: Correctly threw error for malformed URL`);
});

// Test 5.4: URL with query parameters and fragments containing special characters
console.log('\nTest 5.4: Special characters in query and fragment');
try {
  const url = new URL(
    'https://example.com/path?param=value%20with%20spaces&special=测试#fragment%20with%20spaces'
  );
  console.log('✅ PASS: URL with special characters created');
  console.log('  href:', url.href);
  console.log('  search:', url.search);
  console.log('  hash:', url.hash);

  assert.ok(url.search.includes('param='), 'Should contain query parameters');
  assert.ok(url.hash.includes('fragment'), 'Should contain fragment');
} catch (e) {
  console.log('❌ FAIL: Error with special characters:', e.message);
  throw new Error(
    'URL with special characters in query and fragment should be supported: ' +
      e.message
  );
}

// ========================================
// Section 6: URL Resolution and Base URL Tests
// ========================================
console.log('\n--- Section 6: URL Resolution Tests ---');

// Test 6.1: Various relative URL patterns
console.log('Test 6.1: Various relative URL patterns');
const baseUrl = 'https://example.com/base/path/';
const relativeTests = [
  { rel: './file.html', desc: 'Same directory' },
  { rel: '../file.html', desc: 'Parent directory' },
  { rel: '/absolute/path', desc: 'Absolute path' },
  { rel: '?query=only', desc: 'Query only' },
  { rel: '#fragment-only', desc: 'Fragment only' },
  { rel: 'simple-file', desc: 'Simple filename' },
];

relativeTests.forEach((test, index) => {
  console.log(`  Test 6.1.${index + 1}: ${test.desc} - "${test.rel}"`);
  try {
    const url = new URL(test.rel, baseUrl);
    console.log(`    ✅ Result: ${url.href}`);
    assert.ok(url.href.length > 0, 'URL should have content');
  } catch (e) {
    console.log(`    ❌ Error: ${e.message}`);
    throw new Error(
      `Relative URL resolution failed for "${test.rel}": ${e.message}`
    );
  }
});

// ========================================
// Section 7: Protocol-specific Behavior Tests
// ========================================
console.log('\n--- Section 7: Protocol-specific Behavior ---');

// Test 7.1: Different protocol schemes
console.log('Test 7.1: Different protocol schemes');
const protocols = ['http:', 'https:', 'ftp:', 'file:', 'ws:', 'wss:'];

protocols.forEach((protocol, index) => {
  console.log(`  Test 7.1.${index + 1}: ${protocol} protocol`);
  try {
    const url = new URL(`${protocol}//example.com/path`);
    console.log(`    ✅ ${protocol} URL created: ${url.href}`);
    console.log(`    Protocol: ${url.protocol}, Host: ${url.host}`);
    assert.strictEqual(
      url.protocol,
      protocol,
      `Protocol should be ${protocol}`
    );
  } catch (e) {
    console.log(`    ❌ Error with ${protocol}: ${e.message}`);
    throw new Error(
      `URL creation failed for ${protocol} protocol: ${e.message}`
    );
  }
});

// ========================================
// Summary
// ========================================
console.log('\n=== Comprehensive URL Tests Summary ===');
console.log('✅ Single colon URL parsing tests completed');
console.log('✅ HTTPS single slash URL tests completed');
console.log('✅ FTP single slash URL tests completed');
console.log('✅ Unicode character handling tests completed');
console.log('✅ Edge cases and error handling tests completed');
console.log('✅ URL resolution tests completed');
console.log('✅ Protocol-specific behavior tests completed');
console.log('\n=== All Comprehensive URL Tests Completed ===');
