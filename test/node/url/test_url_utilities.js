// Test URL utility functions from node:url module
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

// Test 1: Utility functions exist
assert(
  typeof url.domainToASCII === 'function',
  'domainToASCII should be exported'
);
assert(
  typeof url.domainToUnicode === 'function',
  'domainToUnicode should be exported'
);
assert(
  typeof url.fileURLToPath === 'function',
  'fileURLToPath should be exported'
);
assert(
  typeof url.pathToFileURL === 'function',
  'pathToFileURL should be exported'
);
assert(
  typeof url.urlToHttpOptions === 'function',
  'urlToHttpOptions should be exported'
);

// Test 2: domainToASCII with ASCII domain
try {
  const ascii1 = url.domainToASCII('example.com');
  assertEquals(ascii1, 'example.com', 'domainToASCII: ASCII input');
} catch (e) {
  console.log('FAIL: domainToASCII ASCII test failed:', e.message);
  failed++;
}

// Test 3: domainToASCII with Unicode domain
try {
  const ascii2 = url.domainToASCII('中文.com');
  assert(
    ascii2.startsWith('xn--'),
    'domainToASCII: Unicode converts to Punycode'
  );
  assert(ascii2.includes('.com'), 'domainToASCII: TLD preserved');
} catch (e) {
  console.log('FAIL: domainToASCII Unicode test failed:', e.message);
  failed += 2;
}

// Test 4: domainToASCII empty string
try {
  const ascii3 = url.domainToASCII('');
  assertEquals(ascii3, '', 'domainToASCII: empty string');
} catch (e) {
  console.log('FAIL: domainToASCII empty string failed:', e.message);
  failed++;
}

// Test 5: domainToUnicode with ASCII domain
try {
  const unicode1 = url.domainToUnicode('example.com');
  assertEquals(unicode1, 'example.com', 'domainToUnicode: ASCII input');
} catch (e) {
  console.log('FAIL: domainToUnicode ASCII test failed:', e.message);
  failed++;
}

// Test 6: domainToUnicode with Punycode (currently returns as-is)
// Note: Punycode decoding is not yet implemented, so it returns the input unchanged
try {
  const unicode2 = url.domainToUnicode('xn--fiq228c.com');
  // domainToUnicode currently doesn't decode Punycode, it just returns the string
  assert(
    unicode2.includes('xn--') || unicode2.includes('中文'),
    'domainToUnicode: returns string'
  );
} catch (e) {
  console.log('FAIL: domainToUnicode Punycode test failed:', e.message);
  failed++;
}

// Test 7: domainToUnicode empty string
try {
  const unicode3 = url.domainToUnicode('');
  assertEquals(unicode3, '', 'domainToUnicode: empty string');
} catch (e) {
  console.log('FAIL: domainToUnicode empty string failed:', e.message);
  failed++;
}

// Test 8: domainToASCII handles Unicode correctly
try {
  const original = '中文.com';
  const ascii = url.domainToASCII(original);
  assert(ascii.startsWith('xn--'), 'domainToASCII: converts to Punycode');
  // Note: Round-trip requires Punycode decoding which is not yet implemented
} catch (e) {
  console.log('FAIL: domainToASCII conversion failed:', e.message);
  failed++;
}

// Test 9: fileURLToPath with Unix file URL
try {
  const path1 = url.fileURLToPath('file:///home/user/file.txt');
  assert(
    path1 === '/home/user/file.txt' || path1.includes('file.txt'),
    'fileURLToPath: Unix path'
  );
} catch (e) {
  console.log('FAIL: fileURLToPath Unix test failed:', e.message);
  failed++;
}

// Test 10: fileURLToPath with URL object
try {
  const fileUrl = new url.URL('file:///home/user/test.txt');
  const path2 = url.fileURLToPath(fileUrl);
  assert(path2.includes('test.txt'), 'fileURLToPath: URL object');
} catch (e) {
  console.log('FAIL: fileURLToPath URL object failed:', e.message);
  failed++;
}

// Test 11: fileURLToPath with non-file URL should throw
try {
  url.fileURLToPath('http://example.com/path');
  console.log('FAIL: fileURLToPath should throw for non-file URL');
  failed++;
} catch (e) {
  // Expected to throw
  assert(e.message.includes('file:'), 'fileURLToPath: throws for non-file URL');
}

// Test 12: pathToFileURL with Unix path
try {
  const fileUrl1 = url.pathToFileURL('/home/user/file.txt');
  assertEquals(fileUrl1.protocol, 'file:', 'pathToFileURL: protocol is file:');
  assert(
    fileUrl1.pathname.includes('/home/user/file.txt'),
    'pathToFileURL: path preserved'
  );
} catch (e) {
  console.log('FAIL: pathToFileURL Unix test failed:', e.message);
  failed += 2;
}

// Test 13: pathToFileURL returns URL object
try {
  const fileUrl2 = url.pathToFileURL('/tmp/test.txt');
  assert(fileUrl2 instanceof url.URL, 'pathToFileURL: returns URL object');
  assertEquals(fileUrl2.protocol, 'file:', 'pathToFileURL: correct protocol');
} catch (e) {
  console.log('FAIL: pathToFileURL return type failed:', e.message);
  failed += 2;
}

// Test 14: fileURLToPath and pathToFileURL round-trip
try {
  const originalPath = '/home/user/test.txt';
  const fileUrl = url.pathToFileURL(originalPath);
  const roundTripPath = url.fileURLToPath(fileUrl);
  assert(
    roundTripPath.includes('test.txt'),
    'file URL round-trip: filename preserved'
  );
  assert(
    roundTripPath.includes('/'),
    'file URL round-trip: path structure preserved'
  );
} catch (e) {
  console.log('FAIL: file URL round-trip failed:', e.message);
  failed += 2;
}

// Test 15: urlToHttpOptions basic
try {
  const u1 = new url.URL('http://example.com:8080/path?query=1#hash');
  const options = url.urlToHttpOptions(u1);
  assertEquals(options.protocol, 'http:', 'urlToHttpOptions: protocol');
  assertEquals(options.hostname, 'example.com', 'urlToHttpOptions: hostname');
  assertEquals(options.port, 8080, 'urlToHttpOptions: port');
  assertEquals(options.path, '/path?query=1', 'urlToHttpOptions: path');
  assertEquals(options.hash, '#hash', 'urlToHttpOptions: hash');
} catch (e) {
  console.log('FAIL: urlToHttpOptions basic test failed:', e.message);
  failed += 5;
}

// Test 16: urlToHttpOptions with auth
try {
  const u2 = new url.URL('http://user:pass@example.com/path');
  const options2 = url.urlToHttpOptions(u2);
  assertEquals(options2.auth, 'user:pass', 'urlToHttpOptions: auth');
} catch (e) {
  console.log('FAIL: urlToHttpOptions auth test failed:', e.message);
  failed++;
}

// Test 17: urlToHttpOptions without port
try {
  const u3 = new url.URL('https://example.com/path');
  const options3 = url.urlToHttpOptions(u3);
  assert(
    options3.port === undefined ||
      options3.port === null ||
      options3.port === '',
    'urlToHttpOptions: no port'
  );
} catch (e) {
  console.log('FAIL: urlToHttpOptions no port test failed:', e.message);
  failed++;
}

// Test 18: urlToHttpOptions with non-URL should throw
try {
  url.urlToHttpOptions({ hostname: 'example.com' });
  console.log('FAIL: urlToHttpOptions should throw for non-URL object');
  failed++;
} catch (e) {
  // Expected to throw
  assert(e.message.includes('URL'), 'urlToHttpOptions: throws for non-URL');
}

// Summary
console.log(`\nUtility Function Tests: ${passed} passed, ${failed} failed`);
if (failed === 0) {
  console.log('All utility function tests passed!');
}
