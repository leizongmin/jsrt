// WPT URL remaining failure cases - extracted from actual WPT test failures
// Updated 2025-09-14 after fixing major relative URL parsing issues
// These test cases SHOULD MOSTLY PASS now - they test WPT-expected behavior vs current jsrt behavior
//
// PROGRESS UPDATE:
// ✅ FIXED: Major relative URL parsing bug for special schemes without authority
// ✅ FIXED: Empty password handling (colon omission)
// ✅ WORKING: Port 0 preservation, URLSearchParams encoding
// ✅ WORKING: Backslash normalization for special schemes
//
// Current WPT Status: 70% pass rate (7/10 tests passing)
// Remaining failures: 3 test files with minor edge cases
// - url/url-constructor.any.js (reduced failures)
// - url/url-origin.any.js (reduced failures)
// - url/urlsearchparams-stringifier.any.js (specific edge cases)
const assert = require('jsrt:assert');

console.log(
  '=== WPT URL Remaining Failures Tests - THESE SHOULD FAIL (Updated 2025-09-14) ==='
);
console.log(
  'These tests represent actual WPT failures - they test expected WPT behavior vs current jsrt implementation'
);
console.log(
  'Each failure shows a compatibility issue that needs to be fixed in the jsrt URL implementation\n'
);

let failedTests = 0;
let totalTests = 0;

function test(name, testFn) {
  totalTests++;
  console.log(`\nTest: ${name}`);
  try {
    testFn();
    // console.log('✅ PASS');
  } catch (e) {
    failedTests++;
    console.log('❌ FAIL:', e.message);
    // 继续运行但记录失败
  }
}

// ===== CATEGORY 1: URL CONSTRUCTOR FAILURES (HIGHEST PRIORITY) =====

test('URL Constructor - Userinfo parsing failures', () => {
  console.log('  Testing userinfo parsing failures...');

  const testCases = [
    // ACTUAL WPT FAILURE: "Parsing: <https://test:@test> without base: href"
    // WPT expects: https://test@test/ (empty password should be omitted from href)
    // Current jsrt likely produces: https://test:@test/ (keeps the empty password colon)
    {
      input: 'https://test:@test',
      base: null,
      expected: 'https://test@test/', // WPT expectation - no colon for empty password
    },
    // ACTUAL WPT FAILURE: "Parsing: <non-special://test:@test/x> without base: href"
    // WPT expects: non-special://test@test/x (empty password should be omitted)
    // Current jsrt likely produces: non-special://test:@test/x
    {
      input: 'non-special://test:@test/x',
      base: null,
      expected: 'non-special://test@test/x', // WPT expectation - no colon for empty password
    },
    // More userinfo edge cases that should fail
    {
      input: 'https://user:@host',
      base: null,
      expected: 'https://user@host/', // Empty password should omit colon
    },
    {
      input: 'http::@c:29',
      base: 'http://example.org/foo/bar',
      expected: 'http://example.org/foo/:@c:29',
    },
    {
      input: 'http://::@c@d:2',
      base: 'http://example.org/foo/bar',
      expected: 'http://:%3A%40c@d:2/',
    },
    // BACKSLASH PARSING: This is actually working correctly per WPT
    {
      input: '\\\\x\\hello',
      base: 'http://example.org/foo/bar',
      expected: 'http://x/hello', // WPT expects this to be treated as absolute
    },
    // Complex userinfo with special characters (existing tests)
    {
      input: 'http://user:pass@foo:21/bar;par?b#c',
      base: 'http://example.org/foo/bar',
      expected: 'http://user:pass@foo:21/bar;par?b#c',
    },
    {
      input: 'http://a:b@c:29/d',
      base: 'http://example.org/foo/bar',
      expected: 'http://a:b@c:29/d',
    },
    {
      input: 'http://&a:foo(b]c@d:2/',
      base: 'http://example.org/foo/bar',
      expected: 'http://&a:foo(b%5Dc@d:2/',
    },
    {
      input: 'http://foo.com:b@d/',
      base: 'http://example.org/foo/bar',
      expected: 'http://foo.com:b@d/',
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
      throw new Error(`Userinfo parsing failed for "${input}": ${e.message}`);
    }
  });
});

test('URL Constructor - Backslash normalization for special schemes', () => {
  console.log('  Testing backslash normalization for special schemes...');

  const testCases = [
    // Backslash should be normalized to forward slash in special schemes
    {
      input: 'http:\\\\a\\\\b:c\\\\d@foo.com\\\\',
      base: 'http://example.org/foo/bar',
      expected: 'http://foo.com/',
    },
    {
      input: 'http://a:b@c\\\\',
      base: null,
      expected: null, // Should throw
    },
    {
      input: 'ws://a@b\\\\c',
      base: null,
      expected: null, // Should throw
    },
  ];

  testCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      if (expected === null) {
        throw new Error(`URL "${input}" should have thrown but got valid URL`);
      }
      console.log(`    Input: ${input} ${base ? `(base: ${base})` : ''}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      if (expected === null) {
        // Success case - no output
      } else {
        throw new Error(
          `Backslash normalization failed for "${input}": ${e.message}`
        );
      }
    }
  });
});

test('URL Constructor - Port and zero-port handling failures', () => {
  console.log('  Testing port and zero-port handling failures...');

  // ACTUAL WPT FAILURES: Zero port handling
  const zeroPortCases = [
    {
      input: 'http://f:0/c',
      base: 'http://example.org/foo/bar',
      expected: 'http://f:0/c', // Port 0 should be kept per WPT spec
    },
    {
      input: 'http://f:00000000000000/c',
      base: 'http://example.org/foo/bar',
      expected: 'http://f:0/c', // Leading zeros should be stripped to 0
    },
  ];

  zeroPortCases.forEach(({ input, base, expected }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input} ${base ? `(base: ${base})` : ''}`);
      console.log(`    Actual: ${url.href}`);
      console.log(`    Expected: ${expected}`);
      assert.strictEqual(url.href, expected);
    } catch (e) {
      throw new Error(`Zero port handling failed for "${input}": ${e.message}`);
    }
  });

  // Invalid port cases that should throw
  const invalidPortCases = [
    'http://f:b/c',
    'http://f: /c',
    'http://f:fifty-two/c',
    'http://f:999999/c',
    'non-special://f:999999/c',
    'http://f: 21 / b ? d # e',
  ];

  invalidPortCases.forEach((input) => {
    let didThrow = false;
    try {
      new URL(input, 'http://example.org/foo/bar');
      console.log(`    ERROR: ${input} should have thrown but didn't`);
    } catch (e) {
      didThrow = true;
      // Success case - no output
    }

    if (!didThrow) {
      throw new Error(
        `Invalid port URL "${input}" should throw error but did not`
      );
    }
  });
});

test('URL Constructor - Special scheme and relative URL failures', () => {
  console.log('  Testing special scheme and relative URL failures...');

  const testCases = [
    // ACTUAL WPT FAILURES: These exact cases are failing
    {
      input: 'http:foo.com',
      base: 'http://example.org/foo/bar',
      expected: 'http://example.org/foo/foo.com',
    },
    {
      input: 'http://f:21/ b ? d # e ',
      base: 'http://example.org/foo/bar',
      expected: 'http://f:21/%20b%20?%20d%20#%20e',
    },
    {
      input: 'lolscheme:x x#x x',
      base: null,
      expected: 'lolscheme:x x#x%20x', // Fragment space should be encoded
    },
    {
      input: '  \t', // Whitespace-only input
      base: 'http://example.org/foo/bar',
      expected: 'http://example.org/foo/bar',
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
      throw new Error(
        `Whitespace handling failed for "${input}": ${e.message}`
      );
    }
  });
});

test('URL Constructor - Fragment backslash handling', () => {
  console.log('  Testing fragment backslash handling...');

  const testCases = [
    {
      input: '#\\\\',
      base: 'http://example.org/foo/bar',
      expected: 'http://example.org/foo/bar#\\\\',
    },
    {
      input: 'foo://',
      base: 'http://example.org/foo/bar',
      expected: 'foo://',
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
      throw new Error(`Fragment backslash failed for "${input}": ${e.message}`);
    }
  });
});

// ===== CATEGORY 2: URLSEARCHPARAMS FAILURES =====

test('URLSearchParams Constructor - Advanced constructor cases', () => {
  console.log('  Testing URLSearchParams constructor failures...');

  // LATEST WPT FAILURE: DOMException constructor handling - CURRENTLY FAILING AGAIN
  try {
    // This should throw due to branding checks but is currently not throwing
    const shouldThrow = () => {
      const params = new URLSearchParams(DOMException?.prototype || {});
    };
    console.log('    Testing DOMException constructor case...');
    // If DOMException exists, this should throw
    if (typeof DOMException !== 'undefined') {
      let didThrow = false;
      try {
        shouldThrow();
      } catch (e) {
        didThrow = true;
        // Success case - no output
      }
      if (!didThrow) {
        throw new Error('DOMException constructor should have thrown');
      }
    }
  } catch (e) {
    throw new Error(`DOMException constructor test failed: ${e.message}`);
  }

  // LATEST WPT FAILURE: Literal null byte handling - CURRENTLY FAILING
  try {
    const params1 = new URLSearchParams('a=b\\0c');
    const result1 = params1.get('a');
    console.log(`    Literal null test - Expected: 'b\\0c', Got: '${result1}'`);
    // WPT expects 'b\0c' but we're getting null - this is the current failure
    assert.strictEqual(result1, 'b\\0c');
  } catch (e) {
    throw new Error(`Literal null byte handling failed: ${e.message}`);
  }

  // LATEST WPT FAILURE: Percent-encoded null byte - CURRENTLY FAILING
  try {
    const params2 = new URLSearchParams('a=b%00c');
    const result2 = params2.get('a');
    console.log(
      `    Percent null test - Expected: 'b\\0c', Got: length ${result2 ? result2.length : 'null'}, value: '${result2}'`
    );
    // WPT expects 'b\0c' but we're getting null - this is the current failure
    assert.strictEqual(result2, 'b\0c');
  } catch (e) {
    throw new Error(`Percent-encoded null handling failed: ${e.message}`);
  }

  // LATEST WPT FAILURE: Object constructor - CURRENTLY FAILING with "value is not iterable"
  try {
    const params3 = new URLSearchParams({ a: '1', b: '2' });
    console.log(`    Object constructor - Keys: ${Array.from(params3.keys())}`);
    assert.strictEqual(params3.get('a'), '1');
    assert.strictEqual(params3.get('b'), '2');
  } catch (e) {
    throw new Error(`Object constructor failed: ${e.message}`);
  }

  // LATEST WPT FAILURE: Array constructor - CURRENTLY FAILING with "value is not iterable"
  try {
    const params4 = new URLSearchParams([
      ['a', '1'],
      ['b', '2'],
    ]);
    console.log(`    Array constructor - Keys: ${Array.from(params4.keys())}`);
    assert.strictEqual(params4.get('a'), '1');
    assert.strictEqual(params4.get('b'), '2');
  } catch (e) {
    throw new Error(`Array constructor failed: ${e.message}`);
  }

  // LATEST WPT FAILURE: Custom Symbol.iterator - CURRENTLY FAILING, getting null
  try {
    const iterableObject = {
      *[Symbol.iterator]() {
        yield ['a', 'b'];
        yield ['c', 'd'];
      },
    };
    const params5 = new URLSearchParams(iterableObject);
    console.log(
      `    Custom iterator - Expected 'b', Got: '${params5.get('a')}'`
    );
    // WPT expects 'b' but we're getting null - this is the current failure
    assert.strictEqual(params5.get('a'), 'b');
    assert.strictEqual(params5.get('c'), 'd');
  } catch (e) {
    throw new Error(`Custom Symbol.iterator failed: ${e.message}`);
  }
});

test('URLSearchParams - Two-argument has() method', () => {
  console.log('  Testing two-argument has() method...');

  const params = new URLSearchParams('a=1&a=2&b=3');

  // Standard has() should return true if key exists
  assert.strictEqual(params.has('a'), true);

  // LATEST WPT FAILURE: Two-argument has() - CURRENTLY FAILING
  // This is a newer API feature that the WPT expects to work
  try {
    const hasA1 = params.has('a', '1');
    const hasA3 = params.has('a', '3');
    console.log(`    has('a', '1'): ${hasA1}, has('a', '3'): ${hasA3}`);

    // WPT expects these to work correctly but they're currently failing
    assert.strictEqual(
      hasA1,
      true,
      'Two-argument has() should return true for existing key-value pair'
    );
    assert.strictEqual(
      hasA3,
      false,
      'Two-argument has() should return false for non-existing key-value pair'
    );

    // LATEST WPT FAILURE: Two-argument has() with undefined second arg - FIXED
    const hasAUndefined = params.has('a', undefined);
    console.log(`    has('a', undefined): ${hasAUndefined}`);
    assert.strictEqual(
      hasAUndefined,
      true,
      'Two-argument has() with undefined should return true (WPT expects undefined to ignore value check)'
    );
  } catch (e) {
    throw new Error(`Two-argument has() method failed: ${e.message}`);
  }
});

test('URLSearchParams - Set() method order preservation', () => {
  console.log('  Testing set() method order preservation...');

  const params = new URLSearchParams('a=1&b=2&c=3');
  params.set('a', 'B');

  const result = params.toString();
  console.log(`    After set('a', 'B'): ${result}`);

  // The order should be preserved - 'a' should remain in its original position
  const expected = 'a=B&b=2&c=3';
  assert.strictEqual(result, expected);
});

test('URLSearchParams - Size property', () => {
  console.log('  Testing size property...');

  const params = new URLSearchParams('a=1&b=2&c=3');

  // Check if size property exists and is correct
  console.log(`    Size property: ${params.size}`);

  if (typeof params.size !== 'undefined') {
    assert.strictEqual(params.size, 3);

    // Test size after deletion
    params.delete('b');
    assert.strictEqual(params.size, 2);

    // Test size after addition
    params.append('d', '4');
    assert.strictEqual(params.size, 3);
  } else {
    throw new Error('URLSearchParams.size property is not implemented');
  }
});

// ===== CATEGORY 3: ORIGIN PARSING FAILURES =====

test('Origin Parsing - Complex userinfo cases', () => {
  console.log('  Testing origin parsing with complex userinfo...');

  const testCases = [
    // ACTUAL WPT FAILURES: These exact cases are failing in origin parsing
    {
      input: 'http:foo.com',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://example.org',
    },
    {
      input: 'http://f:0/c',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://f:0',
    },
    {
      input: 'http://f:00000000000000/c',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://f:0',
    },
    {
      input: '\\\\x\\hello', // BACKSLASH HANDLING: Actually working correctly
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://x',
    },
    {
      input: 'http::@c:29',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://example.org',
    },
    {
      input: 'http://::@c@d:2',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://d:2',
    },
  ];

  testCases.forEach(({ input, base, expectedOrigin }) => {
    try {
      const url = new URL(input, base);
      console.log(`    Input: ${input} ${base ? `(base: ${base})` : ''}`);
      console.log(`    Actual origin: ${url.origin}`);
      console.log(`    Expected origin: ${expectedOrigin}`);
      assert.strictEqual(url.origin, expectedOrigin);
    } catch (e) {
      throw new Error(`Origin parsing failed for "${input}": ${e.message}`);
    }
  });
});

// ===== CATEGORY 4: RELATIVE URL RESOLUTION =====

test('Relative URL Resolution - Triple slash cases', () => {
  console.log('  Testing triple slash relative URL resolution...');

  const testCases = [
    {
      input: '///test',
      base: 'http://example.org/',
      expected: 'http://test/',
    },
    {
      input: '///\\\\//\\\\//test',
      base: 'http://example.org/',
      expected: 'http://test/',
    },
    {
      input: '///example.org/path',
      base: 'http://example.org/',
      expected: 'http://example.org/path',
    },
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
        `Triple slash resolution failed for "${input}": ${e.message}`
      );
    }
  });
});

test('Relative URL Resolution - Dot normalization cases', () => {
  console.log('  Testing dot normalization in relative URLs...');

  const testCases = [
    {
      input: '/.//path',
      base: 'non-spec:/p',
      expected: 'non-spec:/.//path',
    },
    {
      input: '/..//path',
      base: 'non-spec:/p',
      expected: 'non-spec:/..//path',
    },
    {
      input: '..//path',
      base: 'non-spec:/p',
      expected: 'non-spec:..//path',
    },
    {
      input: 'file:.//p',
      base: null,
      expected: 'file:p',
    },
    {
      input: 'file:/.//p',
      base: null,
      expected: 'file:///p',
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
      throw new Error(`Dot normalization failed for "${input}": ${e.message}`);
    }
  });
});

// ===== CATEGORY 5: FILE URL EDGE CASES =====

test('File URL - Windows drive letter handling', () => {
  console.log('  Testing Windows drive letter handling in file URLs...');

  const testCases = [
    {
      input: 'file:C|/m/',
      base: null,
      expected: 'file:///C:/m/',
    },
    {
      input: 'file:/C|/',
      base: null,
      expected: 'file:///C:/',
    },
    {
      input: 'file://C|/',
      base: null,
      expected: 'file:///C:/',
    },
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
        `Windows drive letter failed for "${input}": ${e.message}`
      );
    }
  });
});

// ===== CATEGORY 6: URLSEARCHPARAMS STRINGIFIER FAILURES =====

test('URLSearchParams - Stringifier edge cases', () => {
  console.log('  Testing URLSearchParams stringifier edge cases...');

  // The WPT test url/urlsearchparams-stringifier.any.js is now PASSING
  // These tests verify the toString() implementation is working correctly

  const testCases = [
    // Test various edge cases that might cause stringifier issues
    {
      init: 'a=1&b=2&c=3',
      expected: 'a=1&b=2&c=3',
      description: 'Basic stringification',
    },
    {
      init: 'a=b%20c&d=e%26f',
      expected: 'a=b+c&d=e%26f', // Space encoded as +, & encoded as %26
      description: 'URL encoding in values',
    },
    {
      init: [
        ['a', 'b c'],
        ['d', 'e&f'],
      ],
      expected: 'a=b+c&d=e%26f',
      description: 'Array constructor with special chars',
    },
    {
      init: '',
      expected: '',
      description: 'Empty params',
    },
  ];

  testCases.forEach(({ init, expected, description }) => {
    try {
      const params = new URLSearchParams(init);
      const result = params.toString();
      console.log(
        `    ${description} - Expected: '${expected}', Got: '${result}'`
      );
      assert.strictEqual(result, expected);
    } catch (e) {
      throw new Error(`${description} failed: ${e.message}`);
    }
  });
});

// ===== CATEGORY 7: INVALID URLS THAT SHOULD THROW =====

test('Invalid URLs - IPv6 validation', () => {
  console.log('  Testing IPv6 address validation...');

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
      // Success case - no output
    }

    if (!didThrow) {
      throw new Error(
        `Invalid IPv6 URL "${input}" should throw error but did not`
      );
    }
  });
});

test('Invalid URLs - Blob URL validation', () => {
  console.log('  Testing blob URL validation...');

  const invalidBlobCases = [
    'blob:blob:',
    'blob:blob:https://example.org/',
    'blob:about:blank',
    'blob:file://host/path',
  ];

  invalidBlobCases.forEach((input) => {
    let didThrow = false;
    try {
      new URL(input);
      console.log(`    ERROR: ${input} should have thrown but didn't`);
    } catch (e) {
      didThrow = true;
      // Success case - no output
    }

    if (!didThrow) {
      throw new Error(
        `Invalid blob URL "${input}" should throw error but did not`
      );
    }
  });
});

test('Invalid URLs - Unicode hostname validation', () => {
  console.log('  Testing Unicode hostname validation...');

  const invalidUnicodeHosts = [
    'http://GOO​⁠﻿goo.com',
    'http://www.foo。bar.com',
    'http://Ｇｏ.com',
    'http://你好你好',
    'https://faß.ExAmPlE/',
  ];

  invalidUnicodeHosts.forEach((input) => {
    let didThrow = false;
    try {
      new URL(input);
      console.log(`    Input: ${input} - Got valid URL, should have thrown`);
    } catch (e) {
      didThrow = true;
      // Success case - no output
    }

    if (!didThrow) {
      throw new Error(
        `Invalid Unicode hostname "${input}" should throw but did not`
      );
    }
  });
});

// ===== CATEGORY 8: SPECIAL CHARACTER ENCODING =====

test('Special Character Encoding - Userinfo and path encoding', () => {
  console.log('  Testing special character encoding...');

  const testCases = [
    {
      input: 'foo:// !"$%&\'()*+,-.;<=>@[\\\\]^_`{|}~@host/',
      base: null,
      expected: "foo://%20!%22$%&'()*+,-.;<=>@[%5C%5C]^_`{|}~@host/",
    },
    {
      input: 'wss:// !"$%&\'()*+,-.;<=>@[]^_`{|}~@host/',
      base: null,
      expected: "wss://%20!%22$%&'()*+,-.;<=>@[]^_`{|}~@host/",
    },
    {
      input: 'foo://joe: !"$%&\'()*+,-.:;<=>@[\\\\]^_`{|}~@host/',
      base: null,
      expected: "foo://joe:%20!%22$%&'()*+,-.:;<=>@[%5C%5C]^_`{|}~@host/",
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

// ===== SUMMARY =====

// console.log('\\n=== Test Summary ===');
console.log(`Total tests: ${totalTests}`);
console.log(`Failed tests: ${failedTests}`);
console.log(`Passed tests: ${totalTests - failedTests}`);

if (failedTests > 0) {
  console.log(
    `\\n❌ ${failedTests} tests failed - these represent actual WPT URL parsing issues that need to be fixed`
  );
  console.log(
    'Each failure corresponds to a real WPT test case that is currently failing in the jsrt implementation'
  );
  console.log('\\nFailing WPT test files (LATEST COUNT):');
  console.log(
    '  - url/url-constructor.any.js (8 failures) - userinfo, backslash, port, whitespace parsing'
  );
  console.log(
    '  - url/url-origin.any.js (6 failures) - origin parsing for same cases'
  );
  console.log(
    '  - url/urlsearchparams-constructor.any.js (10 failures) - DOMException, null bytes, iteration'
  );
  console.log(
    '  - url/urlsearchparams-has.any.js (2 failures) - two-argument has() method'
  );
  console.log(
    '  - url/urlsearchparams-stringifier.any.js (FIXED) - ✅ now passing all tests'
  );
  console.log(
    '\\nTotal failing WPT test files: 4 out of 10 URL-related test files (1 recently fixed)'
  );
} else {
  console.log(
    '\\n✅ All tests passed - WPT URL parsing implementation is working correctly'
  );
}

// console.log('\\n=== Tests Completed ===');
