// Test URL module ES module imports
import {
  URL,
  URLSearchParams,
  parse,
  format,
  resolve,
  domainToASCII,
  domainToUnicode,
  fileURLToPath,
  pathToFileURL,
  urlToHttpOptions,
} from 'node:url';
import url from 'node:url';

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

// Test 1: Named exports exist
assert(typeof URL === 'function', 'URL named export');
assert(typeof URLSearchParams === 'function', 'URLSearchParams named export');
assert(typeof parse === 'function', 'parse named export');
assert(typeof format === 'function', 'format named export');
assert(typeof resolve === 'function', 'resolve named export');
assert(typeof domainToASCII === 'function', 'domainToASCII named export');
assert(typeof domainToUnicode === 'function', 'domainToUnicode named export');
assert(typeof fileURLToPath === 'function', 'fileURLToPath named export');
assert(typeof pathToFileURL === 'function', 'pathToFileURL named export');
assert(typeof urlToHttpOptions === 'function', 'urlToHttpOptions named export');

// Test 2: Default export exists
assert(typeof url === 'object', 'default export is object');
assert(typeof url.URL === 'function', 'default.URL');
assert(typeof url.URLSearchParams === 'function', 'default.URLSearchParams');
assert(typeof url.parse === 'function', 'default.parse');

// Test 3: URL functionality via named import
try {
  const u = new URL('https://example.com/path');
  assertEquals(u.hostname, 'example.com', 'URL named import works');
} catch (e) {
  console.log('FAIL: URL named import failed:', e.message);
  failed++;
}

// Test 4: parse functionality via named import
try {
  const parsed = parse('http://example.com/path');
  assertEquals(parsed.hostname, 'example.com', 'parse named import works');
} catch (e) {
  console.log('FAIL: parse named import failed:', e.message);
  failed++;
}

// Test 5: resolve functionality via named import
try {
  const resolved = resolve('http://example.com/foo', 'bar');
  assertEquals(
    resolved,
    'http://example.com/bar',
    'resolve named import works'
  );
} catch (e) {
  console.log('FAIL: resolve named import failed:', e.message);
  failed++;
}

// Test 6: domainToASCII via named import
try {
  const ascii = domainToASCII('example.com');
  assertEquals(ascii, 'example.com', 'domainToASCII named import works');
} catch (e) {
  console.log('FAIL: domainToASCII named import failed:', e.message);
  failed++;
}

// Test 7: pathToFileURL via named import
try {
  const fileUrl = pathToFileURL('/tmp/test.txt');
  assertEquals(fileUrl.protocol, 'file:', 'pathToFileURL named import works');
} catch (e) {
  console.log('FAIL: pathToFileURL named import failed:', e.message);
  failed++;
}

// Test 8: Default export works
try {
  const u2 = new url.URL('https://example.com/path');
  assertEquals(u2.hostname, 'example.com', 'URL via default export works');
} catch (e) {
  console.log('FAIL: URL via default export failed:', e.message);
  failed++;
}

// Summary
console.log(`\nESM Import Tests: ${passed} passed, ${failed} failed`);
if (failed === 0) {
  console.log('All ESM import tests passed!');
}
