// Debug URL parsing
console.log('=== URL Debug Test ===');
console.log('Input string: \\x\\hello');
console.log('Expected after normalization: //x/hello');

try {
  const url = new URL('\\x\\hello', 'http://example.org/foo/bar');
  console.log('Result href:', url.href);
  console.log('Expected href: http://x/hello');
  console.log('Result origin:', url.origin);
  console.log('Expected origin: http://x');
  console.log('Match:', url.href === 'http://x/hello');
} catch (e) {
  console.log('Error:', e.message);
}