// Comprehensive URL parser debug tests
// This file contains various test cases for debugging URL parsing issues
const assert = require('jsrt:assert');

console.log('=== Comprehensive URL Parser Debug Tests ===');

const baseUrl = 'http://example.org/foo/bar';

// Test 1: Non-special scheme with whitespace
console.log('\n--- Test 1: Non-special Scheme Whitespace Handling ---');

console.log('1.1: Tab and space in non-special scheme');
try {
  const url1 = new URL('a:\t foo.com', baseUrl);
  console.log('  ✅ SUCCESS: "a:\\t foo.com" →', url1.href);
  console.log(
    '  Protocol:',
    url1.protocol,
    'Pathname:',
    JSON.stringify(url1.pathname)
  );
} catch (e) {
  console.log('  ❌ FAILED:', e.message);
}

console.log('1.2: Space in non-special scheme path');
try {
  const url2 = new URL('lolscheme:x x#x x');
  console.log('  ✅ SUCCESS: "lolscheme:x x#x x" →', url2.href);
  console.log(
    '  Protocol:',
    url2.protocol,
    'Pathname:',
    JSON.stringify(url2.pathname)
  );
} catch (e) {
  console.log('  ❌ FAILED:', e.message);
}

// Test 2: Fragment and Query Edge Cases
console.log('\n--- Test 2: Fragment and Query Edge Cases ---');

console.log('2.1: Colon-hash combination');
try {
  const url3 = new URL(':#', baseUrl);
  console.log('  ✅ SUCCESS: ":#" →', url3.href);
  console.log(
    '  Pathname:',
    JSON.stringify(url3.pathname),
    'Hash:',
    JSON.stringify(url3.hash)
  );
} catch (e) {
  console.log('  ❌ FAILED:', e.message);
}

console.log('2.2: Empty fragment property');
try {
  const url4 = new URL('#', baseUrl);
  console.log('  ✅ SUCCESS: "#" →', url4.href);
  console.log(
    '  Hash property should be empty string:',
    JSON.stringify(url4.hash)
  );
} catch (e) {
  console.log('  ❌ FAILED:', e.message);
}

console.log('2.3: Empty query property');
try {
  const url5 = new URL('?', baseUrl);
  console.log('  ✅ SUCCESS: "?" →', url5.href);
  console.log(
    '  Search property should be empty string:',
    JSON.stringify(url5.search)
  );
} catch (e) {
  console.log('  ❌ FAILED:', e.message);
}

// Test 3: Relative Path Resolution
console.log('\n--- Test 3: Relative Path Resolution ---');

const testPaths = [':', ':abc', 'xyz', ':#'];
testPaths.forEach((path) => {
  try {
    const url = new URL(path, baseUrl);
    console.log(`  ✅ "${path}" → ${url.href} (pathname: ${url.pathname})`);
  } catch (e) {
    console.log(`  ❌ "${path}" → FAILED: ${e.message}`);
  }
});

// Test 4: Userinfo Selective Encoding
console.log('\n--- Test 4: Userinfo Selective Encoding ---');

console.log('4.1: Special characters in userinfo');
try {
  const url6 = new URL('http://&a:foo(b]c@d:2/');
  console.log('  ✅ SUCCESS: "http://&a:foo(b]c@d:2/" →', url6.href);
  console.log('  Username:', JSON.stringify(url6.username));
  console.log('  Password:', JSON.stringify(url6.password));
  console.log('  Expected: & and ( should not be encoded, ] should be encoded');
} catch (e) {
  console.log('  ❌ FAILED:', e.message);
}

// Test 5: Comparison with Special Schemes
console.log('\n--- Test 5: Special vs Non-Special Schemes ---');

console.log('5.1: Special scheme (HTTP) with spaces');
try {
  const url7 = new URL('http://example.com/path with spaces');
  console.log('  ✅ HTTP: "http://example.com/path with spaces" →', url7.href);
  console.log(
    '  Pathname:',
    JSON.stringify(url7.pathname),
    '(spaces should be %20)'
  );
} catch (e) {
  console.log('  ❌ FAILED:', e.message);
}

console.log('5.2: Non-special scheme with spaces');
try {
  const url8 = new URL('custom:path with spaces');
  console.log('  ✅ Custom: "custom:path with spaces" →', url8.href);
  console.log(
    '  Pathname:',
    JSON.stringify(url8.pathname),
    '(spaces may be preserved)'
  );
} catch (e) {
  console.log('  ❌ FAILED:', e.message);
}

// Test 6: Edge Case Validation
console.log('\n--- Test 6: Edge Case Validation ---');

const edgeCases = [
  'a:foo', // Simple non-special scheme
  'a:foo\tbar', // Tab in non-special scheme
  'a:foo bar', // Space in non-special scheme
  'a:foo%20bar', // Percent-encoded space
];

edgeCases.forEach((testCase) => {
  try {
    const url = new URL(testCase);
    console.log(`  ✅ "${testCase}" → ${url.href}`);
  } catch (e) {
    console.log(`  ❌ "${testCase}" → FAILED: ${e.message}`);
  }
});

console.log('\n=== Debug Tests Complete ===');
console.log('\nThis test file validates:');
console.log('- Non-special scheme parsing with whitespace');
console.log('- Fragment and query edge cases');
console.log('- Relative path resolution');
console.log('- Userinfo selective encoding');
console.log('- Special vs non-special scheme differences');
