// Test IPv4 address parsing with trailing dots
// This test covers the fix for WPT case: http://%30%78%63%30%2e%30%32%35%30.01%2e

console.log('=== Testing IPv4 Trailing Dot Parsing ===');

function test(description, fn) {
  try {
    fn();
    console.log(`✓ ${description}`);
  } catch (error) {
    console.log(`✗ ${description}: ${error.message}`);
  }
}

// Test 1: WPT specific case
test('WPT hex IPv4 with trailing dot', () => {
  const input = 'http://%30%78%63%30%2e%30%32%35%30.01%2e';
  const base = 'http://other.com/';
  const url = new URL(input, base);

  if (url.href !== 'http://192.168.0.1/') {
    throw new Error(`Expected http://192.168.0.1/, got ${url.href}`);
  }
  if (url.hostname !== '192.168.0.1') {
    throw new Error(`Expected hostname 192.168.0.1, got ${url.hostname}`);
  }
});

// Test 2: Standard decimal IPv4 with trailing dot
test('Decimal IPv4 with trailing dot', () => {
  const url = new URL('http://192.168.1.1./');
  if (url.hostname !== '192.168.1.1') {
    throw new Error(`Expected hostname 192.168.1.1, got ${url.hostname}`);
  }
});

// Test 3: Mixed notation with trailing dot
test('Mixed hex/decimal IPv4 with trailing dot', () => {
  const url = new URL('http://0xc0.168.1.1./path');
  if (url.hostname !== '192.168.1.1') {
    throw new Error(`Expected hostname 192.168.1.1, got ${url.hostname}`);
  }
  if (url.pathname !== '/path') {
    throw new Error(`Expected pathname /path, got ${url.pathname}`);
  }
});

// Test 4: Edge case - only trailing dot should fail
test('Single dot should be invalid', () => {
  try {
    new URL('http://./');
    throw new Error('Should have thrown an error');
  } catch (e) {
    if (e.message === 'Should have thrown an error') {
      throw e;
    }
    // Expected to fail - this is correct behavior
  }
});

// Test 5: Ensure octal notation with trailing dot works
test('Octal IPv4 with trailing dot', () => {
  const url = new URL('http://0300.0250.01.01./');
  if (url.hostname !== '192.168.1.1') {
    throw new Error(`Expected hostname 192.168.1.1, got ${url.hostname}`);
  }
});

// Test 6: IPv4 without trailing dot should still work
test('IPv4 without trailing dot still works', () => {
  const url = new URL('http://0xc0.0250.01');
  if (url.hostname !== '192.168.0.1') {
    throw new Error(`Expected hostname 192.168.0.1, got ${url.hostname}`);
  }
});

console.log('\n=== All IPv4 trailing dot tests completed ===');
