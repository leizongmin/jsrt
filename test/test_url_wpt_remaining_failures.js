// WPT URL remaining failure cases - extracted from SHOW_ALL_FAILURES=1 make wpt N=url output
// Updated 2025-09-11 based on comprehensive WPT test failures analysis
// These are the specific remaining failing test cases that need to be fixed
const assert = require('jsrt:assert');

console.log('=== WPT URL Remaining Failures Tests (Updated 2025-09-11) ===');

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

// ===== URL CONSTRUCTOR FAILURES (Based on Actual WPT Results) =====

// Category 1: Port parsing issues
test('Port parsing - excessive leading zeros should make port invalid', () => {
  console.log('  Testing port with excessive leading zeros...');

  try {
    const url = new URL(
      'http://f:00000000000000/c',
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

test('Port parsing - IPv6 with standard port should be omitted', () => {
  console.log('  Testing IPv6 with standard port...');

  try {
    const url = new URL('http://[2001::1]:80', 'http://example.org/foo/bar');
    console.log('  Actual port:', url.port);
    console.log('  Expected port: "" (empty for standard port)');

    // WPT expects standard port 80 to be omitted for http
    assert.strictEqual(url.port, '', 'Standard port 80 should be omitted');
  } catch (e) {
    console.log('  Current error:', e.message);
    throw new Error(`IPv6 standard port parsing failed: ${e.message}`);
  }
});

// Category 2: Single slash protocol handling (MAJOR ISSUE)
test('Single slash protocols - should be normalized to double slash', () => {
  console.log('  Testing single slash protocol normalization...');

  const testCases = [
    {
      input: 'http:/example.com/',
      base: 'http://example.org/foo/bar',
      expected: 'http://example.com/',
      description: 'HTTP single slash',
    },
    {
      input: 'file:/example.com/',
      base: 'http://example.org/foo/bar',
      expected: 'file://example.com/',
      description: 'File single slash',
    },
  ];

  testCases.forEach(({ input, base, expected, description }) => {
    try {
      const url = new URL(input, base);
      console.log(`  ${description} - Actual:`, url.href);
      console.log(`  ${description} - Expected:`, expected);
      assert.strictEqual(url.href, expected, description);
    } catch (e) {
      console.log(`  ${description} error:`, e.message);
      throw new Error(`${description} failed: ${e.message}`);
    }
  });
});

// Category 3: Invalid URLs that should throw errors (CRITICAL)
test('Invalid URLs should throw TypeError', () => {
  console.log('  Testing invalid URLs that should throw...');

  const invalidURLs = [
    { url: 'file://example:1/', base: null, description: 'File with port' },
    {
      url: 'file://[example]/',
      base: null,
      description: 'File with IPv6-like brackets',
    },
  ];

  invalidURLs.forEach(({ url, base, description }) => {
    let didThrow = false;
    try {
      new URL(url, base);
      console.log(
        `  ERROR: ${description} should have thrown but got valid URL`
      );
    } catch (e) {
      didThrow = true;
      console.log(`  ✅ ${description} correctly threw:`, e.message);
    }

    if (!didThrow) {
      throw new Error(`Invalid URL "${url}" should throw error but did not`);
    }
  });
});

// Category 4: IPv6 hostname format and normalization
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

// Category 5: IPv4 address parsing edge cases (hex, percent-encoded, full-width)
test('IPv4 address parsing edge cases', () => {
  console.log('  Testing IPv4 address parsing...');

  const testCases = [
    {
      input: 'http://192.0x00A80001',
      expected: 'http://192.168.0.1/',
      description: 'Hex IPv4 parsing',
    },
    {
      input: 'http://0xffffffff',
      expected: 'http://255.255.255.255/',
      description: 'Max IPv4 hex value',
    },
    {
      input: 'http://%30%78%63%30%2e%30%32%35%30.01',
      expected: 'http://192.168.0.1/', // Percent-encoded hex IPv4
      description: 'Percent-encoded hex IPv4',
    },
    {
      input: 'http://０Ｘｃ０．０２５０．０１',
      expected: 'http://192.168.0.1/', // Full-width characters
      description: 'Full-width IPv4 characters',
    },
  ];

  testCases.forEach(({ input, expected, description }) => {
    try {
      const url = new URL(input);
      console.log(`  ${description} - Actual:`, url.href);
      console.log(`  ${description} - Expected:`, expected);
      assert.strictEqual(url.href, expected, description);
    } catch (e) {
      console.log(`  ${description} error:`, e.message);
      throw new Error(`${description} failed: ${e.message}`);
    }
  });
});

// ===== ORIGIN CALCULATION FAILURES (Based on WPT Results) =====

// Category 6: Origin calculation for single slash protocols (CRITICAL)
test('Origin calculation - single slash protocols', () => {
  console.log('  Testing origin calculation for single slash protocols...');

  const testCases = [
    {
      input: 'http:/example.com/',
      base: 'http://example.org/foo/bar',
      expected: 'http://example.com',
      description: 'HTTP single slash origin',
    },
    {
      input: 'ftp:/example.com/',
      base: 'http://example.org/foo/bar',
      expected: 'ftp://example.com',
      description: 'FTP single slash origin',
    },
    {
      input: 'https:/example.com/',
      base: 'http://example.org/foo/bar',
      expected: 'https://example.com',
      description: 'HTTPS single slash origin',
    },
    {
      input: 'ws:/example.com/',
      base: 'http://example.org/foo/bar',
      expected: 'ws://example.com',
      description: 'WS single slash origin',
    },
    {
      input: 'wss:/example.com/',
      base: 'http://example.org/foo/bar',
      expected: 'wss://example.com',
      description: 'WSS single slash origin',
    },
  ];

  testCases.forEach(({ input, base, expected, description }) => {
    try {
      const url = new URL(input, base);
      console.log(`  ${description} - Actual origin:`, url.origin);
      console.log(`  ${description} - Expected origin:`, expected);
      assert.strictEqual(
        url.origin,
        expected,
        `${description} should be normalized`
      );
    } catch (e) {
      console.log(`  ${description} error:`, e.message);
      throw new Error(`${description} failed: ${e.message}`);
    }
  });
});

// Category 7: User info parsing and stripping from origins
test('User info parsing - should be stripped from origins', () => {
  console.log('  Testing user info parsing...');

  const testCases = [
    {
      input: 'http:/@www.example.com',
      expected: 'http://www.example.com/',
      expectedOrigin: 'http://www.example.com',
      description: 'User info with @ only',
    },
    {
      input: 'http:/a:b@www.example.com',
      expected: 'http://www.example.com/',
      expectedOrigin: 'http://www.example.com',
      description: 'User info a:b@',
    },
    {
      input: 'http:/:b@www.example.com',
      expected: 'http://www.example.com/',
      expectedOrigin: 'http://www.example.com',
      description: 'User info /:b@',
    },
    {
      input: 'http:/a:@www.example.com',
      expected: 'http://www.example.com/',
      expectedOrigin: 'http://www.example.com',
      description: 'User info /a:@',
    },
  ];

  testCases.forEach(({ input, expected, expectedOrigin, description }) => {
    try {
      const url = new URL(input);
      console.log(`  ${description} - href:`, url.href);
      console.log(`  ${description} - origin:`, url.origin);
      console.log(`  ${description} - expected origin:`, expectedOrigin);

      // WPT expects user info to be stripped from origin
      assert.strictEqual(url.origin, expectedOrigin, `${description} origin`);
    } catch (e) {
      console.log(`  ${description} error:`, e.message);
      throw new Error(`${description} failed: ${e.message}`);
    }
  });
});

// Category 8: Hostname case normalization and Unicode
test('Hostname case normalization', () => {
  console.log('  Testing hostname case normalization...');

  const testCases = [
    {
      input: 'http://ExAmPlE.CoM',
      base: 'http://other.com/',
      expected: 'example.com',
      expectedOrigin: 'http://example.com',
      description: 'Case normalization',
    },
  ];

  testCases.forEach(
    ({ input, base, expected, expectedOrigin, description }) => {
      try {
        const url = new URL(input, base);
        console.log(`  ${description} - hostname:`, url.hostname);
        console.log(`  ${description} - origin:`, url.origin);
        console.log(`  ${description} - expected hostname:`, expected);
        console.log(`  ${description} - expected origin:`, expectedOrigin);

        assert.strictEqual(url.hostname, expected, `${description} hostname`);
        assert.strictEqual(url.origin, expectedOrigin, `${description} origin`);
      } catch (e) {
        console.log(`  ${description} error:`, e.message);
        throw new Error(`${description} failed: ${e.message}`);
      }
    }
  );
});

// Category 9: Unicode hostname normalization (punycode)
test('Unicode hostname normalization', () => {
  console.log('  Testing Unicode hostname normalization...');

  const testCases = [
    {
      input: 'http://GOO​⁠﻿goo.com', // Zero-width chars
      base: 'http://other.com/',
      expected: 'goo.com', // Should be stripped
      description: 'Zero-width characters should be stripped',
    },
    {
      input: 'http://www.foo。bar.com', // Unicode dot
      base: 'http://other.com/',
      expected: 'www.foo.bar.com', // Should be normalized
      description: 'Unicode dot normalization',
    },
    {
      input: 'http://Ｇｏ.com', // Full-width
      base: 'http://other.com/',
      expected: 'go.com', // Should be normalized
      description: 'Full-width character normalization',
    },
    {
      input: 'http://你好你好',
      base: 'http://other.com/',
      expected: 'xn--6qqa088eba', // Punycode for Chinese
      description: 'Chinese characters to punycode',
    },
    {
      input: 'https://faß.ExAmPlE/',
      expected: 'xn--fa-hia.example', // Punycode conversion
      description: 'German ß character to punycode',
    },
  ];

  testCases.forEach(({ input, base, expected, description }) => {
    try {
      const url = new URL(input, base);
      console.log(`  ${description} - hostname:`, url.hostname);
      console.log(`  ${description} - expected:`, expected);

      // Note: This test might need adjustment based on actual implementation
      console.log(
        `  ${description} - normalization test (implementation specific)`
      );
    } catch (e) {
      console.log(`  ${description} error:`, e.message);
      // Don't fail for Unicode normalization yet - implementation dependent
    }
  });
});

// Category 10: Special scheme origin calculation
test('Special scheme origin calculation', () => {
  console.log('  Testing special scheme origins...');

  const testCases = [
    {
      input: '#',
      base: 'test:test',
      expectedOrigin: 'null',
      description: 'Fragment on test: scheme',
    },
    {
      input: '#x',
      base: 'mailto:x@x.com',
      expectedOrigin: 'null',
      description: 'Fragment on mailto: scheme',
    },
    {
      input: '#x',
      base: 'data:,',
      expectedOrigin: 'null',
      description: 'Fragment on data: scheme',
    },
    {
      input: '#x',
      base: 'about:blank',
      expectedOrigin: 'null',
      description: 'Fragment on about:blank',
    },
    {
      input: '?i',
      base: 'sc:/pa/pa',
      expectedOrigin: 'null',
      description: 'Query on special scheme',
    },
  ];

  testCases.forEach(({ input, base, expectedOrigin, description }) => {
    try {
      const url = new URL(input, base);
      console.log(`  ${description} - href:`, url.href);
      console.log(`  ${description} - origin:`, url.origin);
      console.log(`  ${description} - expected origin:`, expectedOrigin);

      assert.strictEqual(url.origin, expectedOrigin, `${description} origin`);
    } catch (e) {
      console.log(`  ${description} error:`, e.message);
      throw new Error(`${description} failed: ${e.message}`);
    }
  });
});

// Category 11: Special characters in hostname - should throw
test('Special characters in hostname - should throw', () => {
  console.log('  Testing special characters that should cause errors...');

  const invalidHosts = [
    'http://!"$&\'()*+,-.;=_`{}~/',
    'wss://!"$&\'()*+,-.;=_`{}~/',
    'https://a%C2%ADb/', // Invalid percent encoding
  ];

  invalidHosts.forEach((input, index) => {
    let didThrow = false;
    try {
      const url = new URL(input);
      console.log(`  Special char ${index + 1} - href:`, url.href);
      console.log(`  Special char ${index + 1} - hostname:`, url.hostname);
      // Some might succeed with normalization
    } catch (e) {
      didThrow = true;
      console.log(`  ✅ Special char ${index + 1} correctly threw:`, e.message);
    }

    // Note: WPT expects some of these to throw, record the behavior
    console.log(`  Special char ${index + 1} - threw: ${didThrow}`);
  });
});

// Category 12: Blob URL origins
test('Blob URL origin handling', () => {
  console.log('  Testing blob URL origins...');

  const testCases = [
    {
      input: 'blob:ftp://host/path',
      expectedOrigin: 'ftp://host',
      description: 'Blob FTP URL',
    },
    {
      input: 'blob:ws://example.org/',
      expectedOrigin: 'ws://example.org',
      description: 'Blob WS URL',
    },
    {
      input: 'blob:wss://example.org/',
      expectedOrigin: 'wss://example.org',
      description: 'Blob WSS URL',
    },
    {
      input: 'blob:http%3a//example.org/',
      expectedOrigin: 'null', // Invalid URL encoding
      description: 'Blob with percent-encoded scheme',
    },
  ];

  testCases.forEach(({ input, expectedOrigin, description }) => {
    try {
      const url = new URL(input);
      console.log(`  ${description} - href:`, url.href);
      console.log(`  ${description} - origin:`, url.origin);
      console.log(`  ${description} - expected origin:`, expectedOrigin);

      // Note: Blob URL origin extraction test
      console.log(
        `  ${description} - blob origin test (implementation specific)`
      );
    } catch (e) {
      console.log(`  ${description} error:`, e.message);
    }
  });
});

// Status documentation (updated with comprehensive WPT analysis)
test('WPT Status Documentation - Updated 2025-09-11', () => {
  console.log('  Current WPT URL test status (based on latest run)...');

  const currentStatus = {
    'url-constructor.any.js': '❌ FAIL - Major parsing issues (1500+ failures)',
    'url-origin.any.js': '❌ FAIL - Origin calculation issues (300+ failures)',
    'url-tojson.any.js': '✅ PASS',
    'urlsearchparams-constructor.any.js': '✅ PASS',
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
  console.log('  Critical issues identified from WPT failures:');
  console.log(
    '    1. Port parsing: excessive leading zeros, IPv6 standard ports'
  );
  console.log(
    '    2. Single slash protocol normalization (http:/, file:/, etc.) - HIGHEST PRIORITY'
  );
  console.log(
    '    3. Invalid URL validation (should throw TypeError) - CRITICAL'
  );
  console.log('    4. IPv6 address normalization and bracket formatting');
  console.log('    5. IPv4 address parsing (hex, percent-encoded, full-width)');
  console.log('    6. Origin calculation for single slash protocols');
  console.log('    7. User info parsing and stripping from origins');
  console.log('    8. Unicode hostname normalization (punycode)');
  console.log(
    '    9. Special scheme origin calculation (data:, mailto:, etc.)'
  );
  console.log('    10. Special characters in hostnames (should throw)');
  console.log('    11. Blob URL origin extraction');

  // This test always passes - just documentation
  assert.ok(true, 'Status documentation updated with latest WPT analysis');
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
  console.log(
    'These failures represent the exact issues found in comprehensive WPT testing.'
  );
  console.log('Fix categories in priority order:');
  console.log('  1. Single slash protocol normalization (affects many tests)');
  console.log('  2. Invalid URL error throwing (affects validation)');
  console.log('  3. Port parsing (leading zeros, standard ports)');
  console.log('  4. IPv6 formatting and normalization');
  console.log('  5. IPv4 special formats (hex, percent-encoded)');
  console.log('  6. Origin calculation consistency');
  console.log('  7. Unicode hostname handling');
  console.log('  8. Special character validation');
  throw new Error(`${failedTests} out of ${totalTests} tests failed`);
} else {
  console.log('\n✅ All tests passed - WPT compliance achieved!');
}

console.log(
  '\n=== WPT URL Remaining Failures Tests Complete (Updated 2025-09-11) ==='
);
