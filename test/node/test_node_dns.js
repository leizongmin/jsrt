const assert = require('jsrt:assert');

// Test DNS module loading
const dns = require('node:dns');
assert.ok(dns, 'node:dns module should load');

// Test DNS functions exist
assert.strictEqual(
  typeof dns.lookup,
  'function',
  'dns.lookup should be a function'
);
assert.strictEqual(
  typeof dns.resolve,
  'function',
  'dns.resolve should be a function'
);
assert.strictEqual(
  typeof dns.resolve4,
  'function',
  'dns.resolve4 should be a function'
);
assert.strictEqual(
  typeof dns.resolve6,
  'function',
  'dns.resolve6 should be a function'
);
assert.strictEqual(
  typeof dns.reverse,
  'function',
  'dns.reverse should be a function'
);

// Test DNS constants
assert.ok(dns.RRTYPE, 'DNS RRTYPE constants should be available');
assert.strictEqual(dns.RRTYPE.A, 1, 'RRTYPE.A should be 1');
assert.strictEqual(dns.RRTYPE.AAAA, 28, 'RRTYPE.AAAA should be 28');
assert.strictEqual(dns.RRTYPE.CNAME, 5, 'RRTYPE.CNAME should be 5');

// Test basic DNS lookup functionality (mock test - no actual network)
try {
  // Test localhost lookup (should work without network)
  const lookupResult = dns.lookup('localhost');
  assert.strictEqual(
    typeof lookupResult,
    'object',
    'dns.lookup should return promise'
  );
  assert.strictEqual(
    typeof lookupResult.then,
    'function',
    'Result should be a promise'
  );

  // Test with options
  const lookupWithOptions = dns.lookup('localhost', { family: 4 });
  assert.strictEqual(
    typeof lookupWithOptions,
    'object',
    'dns.lookup with options should return promise'
  );

  // Test resolve4
  const resolve4Result = dns.resolve4('localhost');
  assert.strictEqual(
    typeof resolve4Result,
    'object',
    'dns.resolve4 should return promise'
  );

  // Test resolve6
  const resolve6Result = dns.resolve6('localhost');
  assert.strictEqual(
    typeof resolve6Result,
    'object',
    'dns.resolve6 should return promise'
  );

  // Test reverse (should return promise that rejects with not implemented)
  const reverseResult = dns.reverse('127.0.0.1');
  assert.strictEqual(
    typeof reverseResult,
    'object',
    'dns.reverse should return promise'
  );

  // Test that promises have proper methods
  assert.strictEqual(
    typeof lookupResult.catch,
    'function',
    'Promise should have catch method'
  );
  assert.strictEqual(
    typeof lookupResult.finally,
    'function',
    'Promise should have finally method'
  );
} catch (error) {
  console.log('❌ DNS API test failed:', error.message);
  throw error;
}

// Test error handling for missing arguments
try {
  dns.lookup();
  console.log('❌ dns.lookup() without arguments should throw');
  assert.fail('dns.lookup() without arguments should throw');
} catch (error) {
  assert.ok(
    error.message.includes('hostname'),
    'Error should mention hostname requirement'
  );
}

try {
  dns.resolve();
  console.log('❌ dns.resolve() without arguments should throw');
  assert.fail('dns.resolve() without arguments should throw');
} catch (error) {
  assert.ok(
    error.message.includes('hostname'),
    'Error should mention hostname requirement'
  );
}
