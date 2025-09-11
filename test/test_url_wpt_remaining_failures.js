// WPT URL remaining failure cases - extracted from make wpt N=url output
// Updated based on actual WPT test failures analysis
// These are the specific remaining failing test cases that need to be fixed
const assert = require('jsrt:assert');

console.log('=== WPT URL Remaining Failures Tests ===');

let failedTests = 0;
let totalTests = 0;

function test(name, testFn) {
  totalTests++;
  console.log(`\nTest: ${name}`);
  try {
    testFn();
    console.log('✅ PASS');
  } catch (e) {
    failedTests++;
    console.log('❌ FAIL:', e.message);
    // 重要：继续运行但记录失败
  }
}

// Test 1: Port parsing with excessive leading zeros (WPT failure)
test('Port parsing - excessive leading zeros', () => {
  console.log('  Testing port with excessive leading zeros...');

  try {
    const url = new URL(
      'http://f:00000000000000000000080/c',
      'http://example.org/foo/bar'
    );
    console.log('  Actual href:', url.href);
    console.log('  Actual port:', url.port);
    console.log('  Expected href: http://f/c');
    console.log('  Expected port: "" (empty)');

    // WPT expects excessive leading zeros to make port invalid
    assert.strictEqual(url.href, 'http://f/c', 'href should omit invalid port');
    assert.strictEqual(url.port, '', 'port should be empty for invalid port');
    assert.strictEqual(url.hostname, 'f', 'hostname should be correct');
  } catch (e) {
    console.log('  Current error:', e.message);
    throw new Error(`Port parsing with leading zeros failed: ${e.message}`);
  }
});

// Test 2: IPv6 hostname format (should include brackets - WPT failure)
test('IPv6 hostname format - brackets required', () => {
  console.log('  Testing IPv6 hostname format...');

  try {
    const url = new URL('http://[2001::1]', 'http://example.org/foo/bar');
    console.log('  Actual hostname:', url.hostname);
    console.log('  Actual host:', url.host);
    console.log('  Expected hostname: [2001::1] (with brackets)');
    console.log('  Expected host: [2001::1]');

    // WPT expects IPv6 hostnames to include brackets
    assert.strictEqual(
      url.hostname,
      '[2001::1]',
      'IPv6 hostname should include brackets'
    );
    assert.strictEqual(
      url.host,
      '[2001::1]',
      'IPv6 host should include brackets'
    );
  } catch (e) {
    console.log('  Current error:', e.message);
    throw new Error(`IPv6 hostname format failed: ${e.message}`);
  }
});

// Test 3: IPv6 with IPv4 normalization to hex (WPT failure)
test('IPv6 IPv4-mapped normalization to hex', () => {
  console.log('  Testing IPv6 with IPv4 normalization...');

  try {
    const url = new URL('http://[::127.0.0.1]', 'http://example.org/foo/bar');
    console.log('  Actual href:', url.href);
    console.log('  Actual hostname:', url.hostname);
    console.log('  Expected href: http://[::7f00:1]/');
    console.log('  Expected hostname: [::7f00:1]');

    // WPT expects IPv4 in IPv6 to be converted to hex
    // 127.0.0.1 = 7f00:0001 = 7f00:1 (no leading zeros)
    assert.strictEqual(
      url.href,
      'http://[::7f00:1]/',
      'IPv4 should be converted to hex'
    );
    assert.strictEqual(
      url.hostname,
      '[::7f00:1]',
      'IPv4 in IPv6 should be hex'
    );
  } catch (e) {
    console.log('  Current error:', e.message);
    throw new Error(`IPv6 IPv4 normalization failed: ${e.message}`);
  }
});

// Test 4: Invalid IPv6 should throw error (WPT failure)
test('Invalid IPv6 with trailing dot should throw', () => {
  console.log('  Testing invalid IPv6 address...');

  let didThrow = false;
  try {
    const url = new URL('http://[::127.0.0.1.]', 'http://example.org/foo/bar');
    console.log('  ERROR: Should have thrown but got href:', url.href);
  } catch (e) {
    didThrow = true;
    console.log('  ✅ Correctly threw error:', e.message);
  }

  // WPT expects this to throw due to invalid IPv6 format
  if (!didThrow) {
    throw new Error('Invalid IPv6 address should throw error but did not');
  }
});

// Test 5: Extended IPv6 normalization (WPT failure)
test('Extended IPv6 format normalization', () => {
  console.log('  Testing extended IPv6 format normalization...');

  try {
    const url = new URL(
      'http://[0:0:0:0:0:0:13.1.68.3]',
      'http://example.org/foo/bar'
    );
    console.log('  Actual href:', url.href);
    console.log('  Actual hostname:', url.hostname);
    console.log('  Expected href: http://[::d01:4403]/');
    console.log('  Expected hostname: [::d01:4403]');

    // WPT expects this to be normalized:
    // 13.1.68.3 = 0d01:4403 in hex
    // 0:0:0:0:0:0:0d01:4403 = ::d01:4403 (compressed)
    assert.strictEqual(
      url.href,
      'http://[::d01:4403]/',
      'Extended IPv6 should be normalized'
    );
    assert.strictEqual(
      url.hostname,
      '[::d01:4403]',
      'Extended IPv6 hostname should be normalized'
    );
  } catch (e) {
    console.log('  Current error:', e.message);
    throw new Error(`Extended IPv6 normalization failed: ${e.message}`);
  }
});

// Test 6: Origin calculation for single slash protocols (WPT failure)
test('Origin calculation - FTP single slash', () => {
  console.log('  Testing origin calculation for single slash protocols...');

  try {
    const url = new URL('ftp:/example.com/', 'http://example.org/foo/bar');
    console.log('  Actual href:', url.href);
    console.log('  Actual origin:', url.origin);
    console.log('  Expected origin: ftp://example.com');

    // WPT expects single slash to be normalized in origin
    assert.strictEqual(
      url.origin,
      'ftp://example.com',
      'Origin should normalize single slash'
    );
  } catch (e) {
    console.log('  Current error:', e.message);
    throw new Error(`Origin calculation failed: ${e.message}`);
  }
});

// Test 7: Status documentation (updated with real issues)
test('WPT Status Documentation', () => {
  console.log('  Current WPT URL test status...');

  const currentStatus = {
    'url-constructor.any.js':
      '❌ FAIL - IPv6 normalization, port parsing, validation issues (1200+ failures)',
    'url-origin.any.js':
      '❌ FAIL - IPv6 origin calculation, protocol normalization (220+ failures)',
    'url-tojson.any.js': '✅ PASS',
    'urlsearchparams-constructor.any.js': '✅ PASS - DOMException issue fixed',
    'urlsearchparams-get.any.js': '✅ PASS',
    'urlsearchparams-getall.any.js': '✅ PASS',
    'urlsearchparams-has.any.js': '✅ PASS',
    'urlsearchparams-set.any.js': '✅ PASS',
    'urlsearchparams-size.any.js': '✅ PASS',
    'urlsearchparams-stringifier.any.js': '✅ PASS',
  };

  console.log('  WPT Test Results Summary:');
  Object.entries(currentStatus).forEach(([test, status]) => {
    console.log(`    ${test}: ${status}`);
  });

  console.log('  Current pass rate: 80% (8/10 tests passing)');
  console.log('  Main remaining issues:');
  console.log('    1. IPv6 address normalization (hex conversion)');
  console.log('    2. IPv6 hostname format (brackets)');
  console.log('    3. Port parsing (leading zeros)');
  console.log('    4. Invalid URL validation');
  console.log('    5. Origin calculation edge cases');

  // This test always passes - just documentation
  assert.ok(true, 'Status documentation updated');
});

console.log('\n=== Test Summary ===');
console.log(`Total tests: ${totalTests}`);
console.log(`Failed tests: ${failedTests}`);
console.log(
  `Pass rate: ${(((totalTests - failedTests) / totalTests) * 100).toFixed(1)}%`
);

if (failedTests > 0) {
  console.log(
    '\n❌ Some tests failed. This indicates WPT compliance issues that need to be fixed.'
  );
  throw new Error(`${failedTests} out of ${totalTests} tests failed`);
} else {
  console.log('\n✅ All tests passed.');
}

console.log('\n=== WPT URL Remaining Failures Tests Complete ===');
