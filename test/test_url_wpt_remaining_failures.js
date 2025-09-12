// WPT URL remaining failure cases - extracted from SHOW_ALL_FAILURES=1 make wpt N=url output
// Updated 2025-01-15 based on comprehensive analysis of 355 actual WPT test failures
// These are the ACTUAL failing test cases from the current WPT run that need to be fixed
const assert = require('jsrt:assert');

console.log(
  '=== WPT URL Remaining Failures Tests (Updated 2025-01-15 - 355 ACTUAL FAILURES) ==='
);

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
    // 继续运行但记录失败
  }
}

// ===== CATEGORY 1: URL PARSING FAILURES (HIGHEST PRIORITY) =====

test('URL Parsing - Basic protocol and path issues', () => {
  console.log('  Testing basic URL parsing failures...');

  const testCases = [
    // Single slash protocol normalization
    {
      input: 'http:foo.com',
      base: 'http://example.org/foo/bar',
      expected: 'http://foo.com/',
    },
    {
      input: 'http:/example.com/',
      base: 'http://example.org/foo/bar',
      expected: 'http://example.com/',
    },
    {
      input: 'http:example.com/',
      base: 'http://example.org/foo/bar',
      expected: 'http://example.com/',
    },

    // Port parsing with leading zeros
    {
      input: 'http://f:00000000000000/c',
      base: 'http://example.org/foo/bar',
      expected: 'http://f/c',
    },

    // Backslash normalization
    {
      input: 'http:\\foo.com\\',
      base: 'http://example.org/foo/bar',
      expected: 'http://foo.com/',
    },
    {
      input: 'http:\\a\\b:c\\d@foo.com\\',
      base: 'http://example.org/foo/bar',
      expected: 'http://foo.com/',
    },

    // Path normalization
    {
      input: 'http://example.com/foo/bar/../ton/../../a',
      base: null,
      expected: 'http://example.com/a',
    },
    {
      input: 'http://example.com/foo%00%51',
      base: null,
      expected: 'http://example.com/foo%00Q',
    },
    {
      input: 'http://example.com/(%28:%3A%29)',
      base: null,
      expected: 'http://example.com/(%28:%3A%29)',
    },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input} ${base ? `(base: ${base})` : ''}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(`URL parsing failed for "${input}": ${e.message}`);
    }
  });
});

test('URL Parsing - File URL special cases', () => {
  console.log('  Testing file URL parsing failures...');

  const testCases = [
    // Windows drive letters with pipe
    { input: 'file:///w|m', base: null, expected: 'file:///w:/m' },
    { input: 'file:C|/m/', base: null, expected: 'file:///C:/m/' },
    { input: 'file:/C|/', base: null, expected: 'file:///C:/' },
    { input: 'file://C|/', base: null, expected: 'file:///C:/' },

    // File URL with host normalization
    {
      input: 'file://localhost//a//../..//foo',
      base: null,
      expected: 'file:///foo',
    },
    { input: 'file://localhost////foo', base: null, expected: 'file:///foo' },
    { input: 'file:////foo', base: null, expected: 'file:///foo' },

    // Relative file URLs
    {
      input: '/c:/foo/bar',
      base: 'file:///c:/baz/qux',
      expected: 'file:///c:/foo/bar',
    },
    {
      input: '/c|/foo/bar',
      base: 'file:///c:/baz/qux',
      expected: 'file:///c:/foo/bar',
    },
    {
      input: '/c:/foo/bar',
      base: 'file://host/path',
      expected: 'file://host/c:/foo/bar',
    },

    // IPv6 in file URLs
    { input: 'file://[1::8]/C:/', base: null, expected: 'file://[1::8]/C:/' },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input} ${base ? `(base: ${base})` : ''}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(`File URL parsing failed for "${input}": ${e.message}`);
    }
  });
});

test('URL Parsing - IPv6 address failures', () => {
  console.log('  Testing IPv6 address parsing failures...');

  const invalidIPv6Cases = [
    'http://[0:1:2:3:4:5:6:7:8]', // Too many groups
    'https://[0::0::0]', // Multiple double colons
    'https://[0:0:]', // Incomplete
  ];

  invalidIPv6Cases.forEach((input) => {
    let didThrow = false;
    try {
      new URL(input);
      console.log(`    ERROR: ${input} should have thrown but didn't`);
    } catch (e) {
      didThrow = true;
      console.log(`    ✅ ${input} correctly threw: ${e.message}`);
    }

    if (!didThrow) {
      throw new Error(
        `Invalid IPv6 URL "${input}" should throw error but did not`
      );
    }
  });
});

test('URL Parsing - Special scheme handling', () => {
  console.log('  Testing special scheme parsing failures...');

  const testCases = [
    // Non-special schemes with special characters
    { input: 'sc://ñ', base: null, expected: 'sc://ñ' },
    { input: 'sc://ñ?x', base: null, expected: 'sc://ñ?x' },
    { input: 'sc://ñ#x', base: null, expected: 'sc://ñ#x' },
    { input: '#x', base: 'sc://ñ', expected: 'sc://ñ#x' },
    { input: '?x', base: 'sc://ñ', expected: 'sc://ñ?x' },

    // Non-special schemes with path normalization
    { input: 'non-spec:/.//path', base: null, expected: 'non-spec:/.//path' },
    { input: 'non-spec:/..//path', base: null, expected: 'non-spec:/..//path' },
    {
      input: 'non-spec:/a/..//path',
      base: null,
      expected: 'non-spec:/a/..//path',
    },

    // Relative URLs against non-special base
    { input: '/.//path', base: 'non-spec:/p', expected: 'non-spec:/.//path' },
    { input: '/..//path', base: 'non-spec:/p', expected: 'non-spec:/..//path' },
    { input: '..//path', base: 'non-spec:/p', expected: 'non-spec:..//path' },
    {
      input: 'a/..//path',
      base: 'non-spec:/p',
      expected: 'non-spec:a/..//path',
    },

    // Empty relative against non-special base
    { input: '', base: 'non-spec:/..//p', expected: 'non-spec:/..//p' },
    { input: 'path', base: 'non-spec:/..//p', expected: 'non-spec:/..//path' },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input} ${base ? `(base: ${base})` : ''}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(
        `Special scheme parsing failed for "${input}": ${e.message}`
      );
    }
  });
});

test('URL Parsing - Blob URL handling', () => {
  console.log('  Testing blob URL parsing failures...');

  const testCases = [
    // Valid blob URLs
    {
      input: 'blob:https://example.com:443/',
      base: null,
      expected: 'blob:https://example.com:443/',
    },
    {
      input: 'blob:http://example.org:88/',
      base: null,
      expected: 'blob:http://example.org:88/',
    },
    {
      input: 'blob:d3958f5c-0777-0845-9dcf-2cb28783acaf',
      base: null,
      expected: 'blob:d3958f5c-0777-0845-9dcf-2cb28783acaf',
    },
  ];

  const invalidBlobCases = [
    'blob:blob:',
    'blob:blob:https://example.org/',
    'blob:about:blank',
    'blob:file://host/path',
    'blob:ftp://host/path',
    'blob:ws://example.org/',
    'blob:wss://example.org/',
    'blob:http%3a//example.org/',
  ];

  // Test valid blob URLs
  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(`Blob URL parsing failed for "${input}": ${e.message}`);
    }
  });

  // Test invalid blob URLs
  invalidBlobCases.forEach((input) => {
    let didThrow = false;
    try {
      new URL(input);
      console.log(`    ERROR: ${input} should have thrown but didn't`);
    } catch (e) {
      didThrow = true;
      console.log(`    ✅ ${input} correctly threw: ${e.message}`);
    }

    if (!didThrow) {
      throw new Error(
        `Invalid blob URL "${input}" should throw error but did not`
      );
    }
  });
});

// ===== CATEGORY 2: ORIGIN PARSING FAILURES =====

test('Origin Parsing - Basic failures', () => {
  console.log('  Testing origin parsing failures...');

  const testCases = [
    // Protocol normalization in origin
    {
      input: 'http:foo.com',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://foo.com',
    },
    {
      input: 'http://f:00000000000000/c',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://f',
    },

    // Backslash normalization in origin
    {
      input: '\\x\\hello',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://example.org',
    },
    {
      input: 'http::@c:29',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://c',
    },
    {
      input: 'http:\\foo.com\\',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://foo.com',
    },
    {
      input: 'http:\\a\\b:c\\d@foo.com\\',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://foo.com',
    },

    // Invalid origins that should be null
    { input: 'http://a:b@c\\', base: null, expectedOrigin: null },
    { input: 'ws://a@b\\c', base: null, expectedOrigin: null },
  ];

  testCases.forEach(({ input, base, expectedOrigin }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input} ${base ? `(base: ${base})` : ''}`);
      console.log(`    Actual origin: ${url.origin}`);
      console.log(`    Expected origin: ${expectedOrigin}`);

      if (expectedOrigin === null) {
        assert.strictEqual(url.origin, 'null');
      } else {
        assert.strictEqual(url.origin, expectedOrigin);
      }
    } catch (e) {
      if (expectedOrigin === null) {
        console.log(`    ✅ ${input} correctly threw: ${e.message}`);
      } else {
        throw new Error(`Origin parsing failed for "${input}": ${e.message}`);
      }
    }
  });
});

test('Origin Parsing - Unicode and IDN failures', () => {
  console.log('  Testing Unicode hostname origin failures...');

  const testCases = [
    // Unicode characters that should be handled or cause failure
    {
      input: 'http://GOO​⁠﻿goo.com',
      base: 'http://other.com/',
      shouldThrow: true,
    },
    {
      input: 'http://www.foo。bar.com',
      base: 'http://other.com/',
      shouldThrow: true,
    },
    { input: 'http://Ｇｏ.com', base: 'http://other.com/', shouldThrow: true },
    { input: 'http://你好你好', base: 'http://other.com/', shouldThrow: true },
    { input: 'https://faß.ExAmPlE/', base: null, shouldThrow: true },
    { input: 'sc://faß.ExAmPlE/', base: null, shouldThrow: true },
    {
      input: 'http://０Ｘｃ０．０２５０．０１',
      base: 'http://other.com/',
      shouldThrow: true,
    },
  ];

  testCases.forEach(({ input, base, shouldThrow }) => {
    let didThrow = false;
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input} - Origin: ${url.origin}`);
      if (shouldThrow) {
        console.log(`    ERROR: ${input} should have thrown but got valid URL`);
      }
    } catch (e) {
      didThrow = true;
      console.log(`    ✅ ${input} correctly threw: ${e.message}`);
    }

    if (shouldThrow && !didThrow) {
      throw new Error(`Unicode hostname "${input}" should throw but did not`);
    }
  });
});

// ===== CATEGORY 3: INVALID URLS THAT SHOULD THROW =====

test('Invalid URLs - Should throw TypeError', () => {
  console.log('  Testing URLs that should throw TypeError...');

  const invalidURLs = [
    // Invalid IPv4 addresses
    'http://0x7f.0.0.0x7g',
    'http://0X7F.0.0.0X7G',
    'http://1.2.3.4.5',
    'http://1.2.3.4.5.',
    'http://0..0x300/',
    'http://0..0x300./',
    'http://256.256.256.256.256',
    'http://256.256.256.256.256.',
    'http://1.2.3.08.',
    'http://09.2.3.4.',
    'http://01.2.3.4.5',
    'http://01.2.3.4.5.',
    'http://0x1.2.3.4.5',
    'http://0x1.2.3.4.5.',
    'http://foo.1.2.3.4',
    'http://foo.1.2.3.4.',
    'http://foo.2.3.4',
    'http://foo.2.3.4.',
    'http://foo.09',
    'http://foo.09.',

    // Invalid hosts
    'http://./',
    'http://../',
    'h://.',
    'file://a­b/p',
    'file://a%C2%ADb/p',
    'file://loC𝐀𝐋𝐇𝐨𝐬𝐭/usr/bin',
    'https://a%C2%ADb/',

    // Invalid schemes with authority
    'data://example.com:8080/pathname?search#hash',
    'data:///test',
    'data://test/a/../b',
    'data://:443',
    'data://test:test',
    'data://[:1]',
    'javascript://:443',
    'javascript://[:1]',
    'mailto://:443',
    'mailto://[:1]',
    'intent://:443',
    'intent://[:1]',
    'urn://:443',
    'urn://[:1]',
    'turn://:443',
    'turn://[:1]',
    'stun://:443',
    'stun://[:1]',
  ];

  invalidURLs.forEach((input) => {
    let didThrow = false;
    try {
      new URL(input);
      console.log(`    ERROR: ${input} should have thrown but got valid URL`);
    } catch (e) {
      didThrow = true;
      console.log(`    ✅ ${input} correctly threw: ${e.message}`);
    }

    if (!didThrow) {
      throw new Error(`Invalid URL "${input}" should throw error but did not`);
    }
  });
});

// ===== CATEGORY 4: SPECIAL CHARACTER AND ENCODING ISSUES =====

test('Special Characters - Userinfo and path encoding', () => {
  console.log('  Testing special character encoding failures...');

  const testCases = [
    // Special characters in different URL parts
    {
      input:
        'non-special:cannot-be-a-base-url-!"$%&\'()*+,-.;<=>@[\\]^_`{|}~@/',
      base: null,
      expected:
        'non-special:cannot-be-a-base-url-!"$%&\'()*+,-.;<=>@[\\]^_`{|}~@/',
    },
    {
      input: 'foo:// !"$%&\'()*+,-.;<=>@[\\]^_`{|}~@host/',
      base: null,
      expected: 'foo://%20!"$%&\'()*+,-.;<=>@[\\]^_`{|}~@host/',
    },
    {
      input: 'wss:// !"$%&\'()*+,-.;<=>@[]^_`{|}~@host/',
      base: null,
      expected: 'wss://%20!"$%&\'()*+,-.;<=>@[]^_`{|}~@host/',
    },
    {
      input: 'foo://joe: !"$%&\'()*+,-.:;<=>@[\\]^_`{|}~@host/',
      base: null,
      expected: 'foo://joe:%20!"$%&\'()*+,-.:;<=>@[\\]^_`{|}~@host/',
    },
    {
      input: 'wss://joe: !"$%&\'()*+,-.:;<=>@[]^_`{|}~@host/',
      base: null,
      expected: 'wss://joe:%20!"$%&\'()*+,-.:;<=>@[]^_`{|}~@host/',
    },

    // Special characters in host
    {
      input: 'foo://!"$%&\'()*+,-.;=_`{}~/',
      base: null,
      expected: 'foo://!"$%&\'()*+,-.;=_`{}~/',
    },
    {
      input: 'wss://!"$&\'()*+,-.;=_`{}~/',
      base: null,
      expected: 'wss://!"$&\'()*+,-.;=_`{}~/',
    },

    // Special characters in path
    {
      input: 'foo://host/ !"$%&\'()*+,-./:;<=>@[\\]^_`{|}~',
      base: null,
      expected: 'foo://host/%20!"$%&\'()*+,-./:;<=>@[\\]^_`{|}~',
    },
    {
      input: 'wss://host/ !"$%&\'()*+,-./:;<=>@[\\]^_`{|}~',
      base: null,
      expected: 'wss://host/%20!"$%&\'()*+,-./:;<=>@[\\]^_`{|}~',
    },

    // Special characters in query
    {
      input: 'foo://host/dir/? !"$%&\'()*+,-./:;<=>?@[\\]^_`{|}~',
      base: null,
      expected: 'foo://host/dir/?%20!"$%&\'()*+,-./:;<=>?@[\\]^_`{|}~',
    },
    {
      input: 'wss://host/dir/? !"$%&\'()*+,-./:;<=>?@[\\]^_`{|}~',
      base: null,
      expected: 'wss://host/dir/?%20!"$%&\'()*+,-./:;<=>?@[\\]^_`{|}~',
    },

    // Special characters in fragment
    {
      input: 'foo://host/dir/# !"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~',
      base: null,
      expected: 'foo://host/dir/#%20!"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~',
    },
    {
      input: 'wss://host/dir/# !"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~',
      base: null,
      expected: 'wss://host/dir/#%20!"#$%&\'()*+,-./:;<=>?@[\\]^_`{|}~',
    },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(
        `Special character encoding failed for "${input}": ${e.message}`
      );
    }
  });
});

// ===== CATEGORY 5: BACKSLASH NORMALIZATION =====

test('Backslash Normalization - Non-special schemes', () => {
  console.log('  Testing backslash normalization failures...');

  const testCases = [
    // Non-special schemes should preserve backslashes
    {
      input: 'non-special:\\opaque',
      base: null,
      expected: 'non-special:\\opaque',
    },
    {
      input: 'non-special:\\opaque/path',
      base: null,
      expected: 'non-special:\\opaque/path',
    },
    {
      input: 'non-special:\\opaque\\path',
      base: null,
      expected: 'non-special:\\opaque\\path',
    },
    {
      input: 'non-special:\\/opaque',
      base: null,
      expected: 'non-special:\\/opaque',
    },
    {
      input: 'non-special:/\\path',
      base: null,
      expected: 'non-special:/\\path',
    },
    {
      input: 'non-special://host\\a',
      base: null,
      expected: 'non-special://host\\a',
    },
    {
      input: 'non-special://host/a\\b',
      base: null,
      expected: 'non-special://host/a\\b',
    },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(
        `Backslash normalization failed for "${input}": ${e.message}`
      );
    }
  });
});

// ===== CATEGORY 6: RELATIVE URL RESOLUTION =====

test('Relative URL Resolution - Complex cases', () => {
  console.log('  Testing complex relative URL resolution failures...');

  const testCases = [
    // Triple slash resolution
    { input: '///test', base: 'http://example.org/', expected: 'http://test/' },
    {
      input: '///\\//\\//test',
      base: 'http://example.org/',
      expected: 'http://test/',
    },
    {
      input: '///example.org/path',
      base: 'http://example.org/',
      expected: 'http://example.org/path',
    },
    {
      input: '///example.org/../path',
      base: 'http://example.org/',
      expected: 'http://example.org/path',
    },
    {
      input: '///example.org/../../',
      base: 'http://example.org/',
      expected: 'http://example.org/',
    },
    {
      input: '///example.org/../path/../../',
      base: 'http://example.org/',
      expected: 'http://example.org/',
    },
    {
      input: '///example.org/../path/../../path',
      base: 'http://example.org/',
      expected: 'http://example.org/path',
    },
    {
      input: '/\\/\\//example.org/../path',
      base: 'http://example.org/',
      expected: 'http://example.org/path',
    },

    // File URL relative resolution
    { input: '/\\//\\/a/../', base: 'file:///', expected: 'file:///' },
    { input: '//a/../', base: 'file:///', expected: 'file:///' },

    // Quad slash resolution
    { input: '////one/two', base: 'file:///', expected: 'file:///one/two' },
    { input: '////one/two', base: 'file:///', expected: 'file:///one/two' },
    { input: 'file:///.///', base: 'file:////', expected: 'file:///' },

    // Dot normalization in file URLs
    { input: 'file:.//p', base: null, expected: 'file:p' },
    { input: 'file:/.//p', base: null, expected: 'file:///p' },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input} (base: ${base})`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(
        `Relative URL resolution failed for "${input}": ${e.message}`
      );
    }
  });
});

// ===== CATEGORY 7: INCOMPLETE URLS =====

test('Incomplete URLs - Missing components', () => {
  console.log('  Testing incomplete URL parsing...');

  const testCases = [
    // URLs with missing components that should still parse
    { input: 'https://x/', base: null, expected: 'https://x/' },
    { input: 'https://x/?', base: null, expected: 'https://x/?' },
    { input: 'https://x/?#', base: null, expected: 'https://x/?#' },
    { input: 'non-special:', base: null, expected: 'non-special:' },
    { input: 'non-special:x/', base: null, expected: 'non-special:x/' },
    { input: 'non-special:x/?', base: null, expected: 'non-special:x/?' },
    { input: 'non-special:x/?#', base: null, expected: 'non-special:x/?#' },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(
        `Incomplete URL parsing failed for "${input}": ${e.message}`
      );
    }
  });
});

// ===== CATEGORY 8: UNICODE AND ENCODING EDGE CASES =====

test('Unicode and Encoding - Edge cases', () => {
  console.log('  Testing Unicode and encoding edge cases...');

  const testCases = [
    // Query and fragment encoding
    {
      input: 'http://example.org/test?%23%23',
      base: null,
      expected: 'http://example.org/test?%23%23',
    },
    {
      input: 'http://example.org/test?a#%EF',
      base: null,
      expected: 'http://example.org/test?a#%EF',
    },

    // Complex Unicode characters
    {
      input: 'http://example.com/���𐟾���﷐﷏﷯ﷰ￾￿?���𐟾���﷐﷏﷯ﷰ￾￿',
      base: null,
      expected: 'http://example.com/���𐟾���﷐﷏﷯ﷰ￾￿?���𐟾���﷐﷏﷯ﷰ￾￿',
    },

    // Non-special scheme with percent encoding
    {
      input: 'non-special://%E2%80%A0/',
      base: null,
      expected: 'non-special://%E2%80%A0/',
    },
    {
      input: 'non-special://H%4fSt/path',
      base: null,
      expected: 'non-special://H%4fSt/path',
    },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(`Unicode/encoding failed for "${input}": ${e.message}`);
    }
  });
});

// ===== CATEGORY 9: EDGE CASES FROM ACTUAL FAILURES =====

test('Edge Cases - Miscellaneous failures', () => {
  console.log('  Testing miscellaneous edge case failures...');

  const testCases = [
    // Relative URLs against file base
    {
      input: '10.0.0.7:8080/foo.html',
      base: 'file:///some/dir/bar.html',
      expected: 'file:///some/dir/10.0.0.7:8080/foo.html',
    },
    {
      input: 'a!@$*=/foo.html',
      base: 'file:///some/dir/bar.html',
      expected: 'file:///some/dir/a!@$*=/foo.html',
    },

    // Test against non-special base
    {
      input: 'test-a-colon-slash-slash.html',
      base: 'a://',
      expected: 'a://test-a-colon-slash-slash.html',
    },

    // Incomplete URLs with newlines (should be handled)
    {
      input: 'http://example.org/test?a#b\n',
      base: null,
      expected: 'http://example.org/test?a#b',
    },
    {
      input: 'non-spec://example.org/test?a#b\n',
      base: null,
      expected: 'non-spec://example.org/test?a#b',
    },
    {
      input: 'non-spec:/test?a#b\n',
      base: null,
      expected: 'non-spec:/test?a#b',
    },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input} ${base ? `(base: ${base})` : ''}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(`Edge case failed for "${input}": ${e.message}`);
    }
  });
});

// ===== SUMMARY =====

console.log('\n=== Test Summary ===');
console.log(`Total tests: ${totalTests}`);
console.log(`Failed tests: ${failedTests}`);
console.log(`Passed tests: ${totalTests - failedTests}`);

if (failedTests > 0) {
  console.log(
    `\n❌ ${failedTests} tests failed - these represent actual WPT URL parsing issues that need to be fixed`
  );
  console.log(
    'Each failure corresponds to a real WPT test case that is currently failing in the jsrt implementation'
  );
} else {
  console.log(
    '\n✅ All tests passed - WPT URL parsing implementation is working correctly'
  );
}

console.log('\n=== Tests Completed ===');
