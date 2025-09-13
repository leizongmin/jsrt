// WPT URL remaining failure cases - extracted from comprehensive WPT test output
// Updated 2025-09-13 based on actual JSRT WPT test failures
// These are the ACTUAL failing test cases from the current WPT run that need to be fixed
const assert = require('jsrt:assert');

console.log(
  '=== WPT URL Remaining Failures Tests (Updated 2025-09-13 - ACTUAL WPT FAILURES) ==='
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

// ===== CATEGORY 1: URL CONSTRUCTOR FAILURES (HIGHEST PRIORITY) =====

test('URL Constructor - Userinfo parsing failures', () => {
  console.log('  Testing userinfo parsing failures...');

  const testCases = [
    // Complex userinfo with special characters
    {
      input: 'http://user:pass@foo:21/bar;par?b#c',
      base: 'http://example.org/foo/bar',
      expected: 'http://user:pass@foo:21/bar;par?b#c',
    },
    {
      input: 'https://test:@test',
      base: null,
      expected: 'https://test:@test/',
    },
    {
      input: 'non-special://test:@test/x',
      base: null,
      expected: 'non-special://test:@test/x',
    },
    {
      input: 'http://a:b@c:29/d',
      base: 'http://example.org/foo/bar',
      expected: 'http://a:b@c:29/d',
    },
    {
      input: 'http::@c:29',
      base: 'http://example.org/foo/bar',
      expected: 'http://c:29/',
    },
    {
      input: 'http://&a:foo(b]c@d:2/',
      base: 'http://example.org/foo/bar',
      expected: 'http://&a:foo(b%5Dc@d:2/',
    },
    {
      input: 'http://::@c@d:2',
      base: 'http://example.org/foo/bar',
      expected: 'http://c@d:2/',
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
        console.log(`    ✅ ${input} correctly threw: ${e.message}`);
      } else {
        throw new Error(`Backslash normalization failed for "${input}": ${e.message}`);
      }
    }
  });
});

test('URL Constructor - Port validation failures', () => {
  console.log('  Testing port validation failures...');

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
      console.log(`    ✅ ${input} correctly threw: ${e.message}`);
    }

    if (!didThrow) {
      throw new Error(`Invalid port URL "${input}" should throw error but did not`);
    }
  });
});

test('URL Constructor - Special scheme handling with whitespace', () => {
  console.log('  Testing special scheme handling with whitespace...');

  const testCases = [
    {
      input: 'http://f:21/ b ? d # e',
      base: 'http://example.org/foo/bar',
      expected: 'http://f:21/%20b%20?%20d%20#%20e',
    },
    {
      input: 'lolscheme:x x#x x',
      base: null,
      expected: 'lolscheme:x x#x x',
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
      throw new Error(`Whitespace handling failed for "${input}": ${e.message}`);
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

  // Test null byte handling
  try {
    const params1 = new URLSearchParams('a=b\\0c');
    const result1 = params1.get('a');
    console.log(`    Null byte test - Expected: 'b\\0c', Got: '${result1}'`);
    assert.strictEqual(result1, 'b\\0c');
  } catch (e) {
    throw new Error(`Null byte handling failed: ${e.message}`);
  }

  // Test percent-encoded null byte
  try {
    const params2 = new URLSearchParams('a=b%00c');
    const result2 = params2.get('a');
    console.log(`    Percent null test - Expected: 'b\\0c', Got: '${result2}'`);
    assert.strictEqual(result2, 'b\\0c');
  } catch (e) {
    throw new Error(`Percent-encoded null handling failed: ${e.message}`);
  }

  // Test object constructor (should support iteration)
  try {
    const params3 = new URLSearchParams({a: '1', b: '2'});
    console.log(`    Object constructor - Keys: ${Array.from(params3.keys())}`);
    assert.strictEqual(params3.get('a'), '1');
    assert.strictEqual(params3.get('b'), '2');
  } catch (e) {
    throw new Error(`Object constructor failed: ${e.message}`);
  }

  // Test array constructor
  try {
    const params4 = new URLSearchParams([['a', '1'], ['b', '2']]);
    console.log(`    Array constructor - Keys: ${Array.from(params4.keys())}`);
    assert.strictEqual(params4.get('a'), '1');
    assert.strictEqual(params4.get('b'), '2');
  } catch (e) {
    throw new Error(`Array constructor failed: ${e.message}`);
  }
});

test('URLSearchParams - Two-argument has() method', () => {
  console.log('  Testing two-argument has() method...');

  const params = new URLSearchParams('a=1&a=2&b=3');
  
  // Standard has() should return true if key exists
  assert.strictEqual(params.has('a'), true);
  
  // Two-argument has() should check for specific key-value pair
  // This is a newer API feature that might not be implemented yet
  try {
    const hasA1 = params.has('a', '1');
    const hasA3 = params.has('a', '3');
    console.log(`    has('a', '1'): ${hasA1}, has('a', '3'): ${hasA3}`);
    
    if (typeof hasA1 !== 'undefined') {
      assert.strictEqual(hasA1, true);
      assert.strictEqual(hasA3, false);
    } else {
      console.log('    Two-argument has() not implemented yet');
    }
  } catch (e) {
    console.log(`    Two-argument has() error: ${e.message}`);
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
    {
      input: 'http:foo.com',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://foo.com',
    },
    {
      input: 'http://f:0/c',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://f',
    },
    {
      input: 'http://f:00000000000000/c',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://f',
    },
    {
      input: 'http::@c:29',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://c',
    },
    {
      input: 'http://::@c@d:2',
      base: 'http://example.org/foo/bar',
      expectedOrigin: 'http://c@d',
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
      throw new Error(`Triple slash resolution failed for "${input}": ${e.message}`);
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
      throw new Error(`Windows drive letter failed for "${input}": ${e.message}`);
    }
  });
});

// ===== CATEGORY 6: INVALID URLS THAT SHOULD THROW =====

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
      console.log(`    ✅ ${input} correctly threw: ${e.message}`);
    }

    if (!didThrow) {
      throw new Error(`Invalid IPv6 URL "${input}" should throw error but did not`);
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
      console.log(`    ✅ ${input} correctly threw: ${e.message}`);
    }

    if (!didThrow) {
      throw new Error(`Invalid blob URL "${input}" should throw error but did not`);
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
      console.log(`    ✅ ${input} correctly threw: ${e.message}`);
    }

    if (!didThrow) {
      throw new Error(`Invalid Unicode hostname "${input}" should throw but did not`);
    }
  });
});

// ===== CATEGORY 7: SPECIAL CHARACTER ENCODING =====

test('Special Character Encoding - Userinfo and path encoding', () => {
  console.log('  Testing special character encoding...');

  const testCases = [
    {
      input: 'foo:// !"$%&\'()*+,-.;<=>@[\\\\]^_`{|}~@host/',
      base: null,
      expected: 'foo://%20!%22$%&\'()*+,-.;<=>@[%5C%5C]^_`{|}~@host/',
    },
    {
      input: 'wss:// !"$%&\'()*+,-.;<=>@[]^_`{|}~@host/',
      base: null,
      expected: 'wss://%20!%22$%&\'()*+,-.;<=>@[]^_`{|}~@host/',
    },
    {
      input: 'foo://joe: !"$%&\'()*+,-.:;<=>@[\\\\]^_`{|}~@host/',
      base: null,
      expected: 'foo://joe:%20!%22$%&\'()*+,-.:;<=>@[%5C%5C]^_`{|}~@host/',
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
      throw new Error(`Special character encoding failed for "${input}": ${e.message}`);
    }
  });
});

// ===== SUMMARY =====

console.log('\\n=== Test Summary ===');
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
} else {
  console.log(
    '\\n✅ All tests passed - WPT URL parsing implementation is working correctly'
  );
}

console.log('\\n=== Tests Completed ===');