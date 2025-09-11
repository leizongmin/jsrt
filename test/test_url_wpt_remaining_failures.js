// WPT URL remaining failure cases - extracted from make wpt N=url output
// These are the specific remaining failing test cases that need to be fixed
const assert = require('jsrt:assert');
console.log('=== WPT URL Remaining Failures Tests ===');

const baseUrl = 'http://example.org/foo/bar';

// Test 1: Scheme parsing with spaces/tabs (currently rejecting but should parse)
console.log('\n--- Test 1: Scheme parsing edge cases ---');

console.log('Test 1.1: Scheme with tab and space characters');
try {
  const url1 = new URL('a:\t foo.com', baseUrl);
  console.log('  Input: "a:\\t foo.com" with base');
  console.log('  Current result: Should be valid URL');
  console.log('  href:', url1.href);
  console.log('  Expected: Should parse as valid URL, not "Invalid URL"');
  // This currently fails as "Invalid URL" but should parse according to WPT
  assert.ok(url1.href, 'URL with scheme containing tab/space should be valid');
  console.log('✅ PASS: Scheme with tab/space parsed correctly');
} catch (e) {
  console.log('❌ FAIL: Scheme with tab/space:', e.message);
  console.log(
    '  Currently rejecting as Invalid URL but should parse according to WPT'
  );
  throw e;
}

console.log('\nTest 1.2: Non-special scheme with spaces - fragment encoding');
let url2;
try {
  url2 = new URL('lolscheme:x x#x x');
  console.log('  Input: "lolscheme:x x#x x" without base');
  console.log('  Current href:', url2.href);
  console.log('  Expected href: lolscheme:x x#x%20x');
  console.log('  Issue: Space in fragment should be percent-encoded as %20');

  // WPT expects spaces in fragment to be percent-encoded
  assert.strictEqual(
    url2.href,
    'lolscheme:x x#x%20x',
    'Fragment spaces should be percent-encoded'
  );
  console.log('✅ PASS: Fragment spaces encoded correctly');
} catch (e) {
  console.log('❌ FAIL: Fragment space encoding:', e.message);
  console.log('  Expected href: lolscheme:x x#x%20x');
  console.log('  Actual href:', url2?.href);
  throw e;
}

// Test 2: Fragment parsing edge cases
console.log('\n--- Test 2: Fragment parsing edge cases ---');

console.log('Test 2.1: Colon-hash combination');
let url3;
try {
  url3 = new URL(':#', baseUrl);
  console.log('  Input: ":#" with base "http://example.org/foo/bar"');
  console.log('  Current result:', url3.href);
  console.log('  Expected result: http://example.org/foo/:#');

  assert.strictEqual(
    url3.href,
    'http://example.org/foo/:#',
    ':#  should resolve to different href'
  );
  console.log('✅ PASS: :# resolved correctly');
} catch (e) {
  console.log('❌ FAIL: :# parsing:', e.message);
  console.log('  Expected href: http://example.org/foo/:#');
  console.log('  Actual href:', url3?.href);
  throw e;
}

console.log('\nTest 2.2: Empty hash should have empty hash property');
let url4;
try {
  url4 = new URL('#', baseUrl);
  console.log('  Input: "#" with base "http://example.org/foo/bar"');
  console.log('  Current hash:', JSON.stringify(url4.hash));
  console.log('  Expected hash: "" (empty string)');

  // According to WPT, # should have empty hash property, not "#"
  assert.strictEqual(
    url4.hash,
    '',
    'Empty fragment # should have empty hash property'
  );
  console.log('✅ PASS: Empty fragment hash correct');
} catch (e) {
  console.log('❌ FAIL: Empty fragment hash:', e.message);
  console.log('  Expected hash: ""');
  console.log('  Actual hash:', JSON.stringify(url4?.hash));
  throw e;
}

// Test 3: Query parsing edge cases
console.log('\n--- Test 3: Query parsing edge cases ---');

console.log('Test 3.1: Empty query should have empty search property');
let url5;
try {
  url5 = new URL('?', baseUrl);
  console.log('  Input: "?" with base "http://example.org/foo/bar"');
  console.log('  Current search:', JSON.stringify(url5.search));
  console.log('  Expected search: "" (empty string)');

  // According to WPT, ? should have empty search property, not "?"
  assert.strictEqual(
    url5.search,
    '',
    'Empty query ? should have empty search property'
  );
  console.log('✅ PASS: Empty query search correct');
} catch (e) {
  console.log('❌ FAIL: Empty query search:', e.message);
  console.log('  Expected search: ""');
  console.log('  Actual search:', JSON.stringify(url5?.search));
  throw e;
}

// Test 4: Userinfo encoding edge cases
console.log('\n--- Test 4: Userinfo encoding edge cases ---');

console.log(
  'Test 4.1: Special characters in userinfo should not be over-encoded'
);
let url6;
try {
  url6 = new URL('http://&a:foo(b]c@d:2/', baseUrl);
  console.log('  Input: "http://&a:foo(b]c@d:2/" with base');
  console.log('  Current href:', url6.href);
  console.log('  Expected href: http://&a:foo(b%5Dc@d:2/');

  // WPT expects some characters to not be percent-encoded
  // & should stay as &, ( should stay as (, only ] should be encoded as %5D
  assert.strictEqual(
    url6.href,
    'http://&a:foo(b%5Dc@d:2/',
    'Special chars in userinfo should have selective encoding'
  );
  console.log('✅ PASS: Userinfo selective encoding correct');
} catch (e) {
  console.log('❌ FAIL: Userinfo selective encoding:', e.message);
  console.log('  Expected href: http://&a:foo(b%5Dc@d:2/');
  console.log('  Actual href:', url6?.href);
  throw e;
}

// Test 5: Summary of remaining issues
console.log('\n--- Test 5: Summary of Remaining Issues ---');

console.log('REMAINING FAILURES TO FIX:');
console.log('1. Scheme parsing: Handle tabs and spaces in scheme context');
console.log(
  '2. Fragment encoding: Spaces in fragment should be percent-encoded (%20)'
);
console.log('3. Fragment edge case: ":#" should produce different href');
console.log('4. Empty fragment: "#" should have empty hash property, not "#"');
console.log('5. Empty query: "?" should have empty search property, not "?"');
console.log(
  '6. Userinfo encoding: Selective percent-encoding of special characters'
);
console.log('');
console.log('These are the final WPT compliance issues to address.');

console.log('\n=== WPT URL Remaining Failures Tests Complete ===');
