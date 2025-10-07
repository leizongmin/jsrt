const assert = require('jsrt:assert');

// console.log('=== Node.js Query String Module Tests ===');

// Test CommonJS require
const querystring = require('node:querystring');
assert.ok(querystring, 'node:querystring should load');

// console.log('✓ Node.js querystring module loaded successfully');

// Test module exports
// console.log('\n--- Testing Module Exports ---');
assert.strictEqual(
  typeof querystring.parse,
  'function',
  'querystring.parse should be a function'
);
assert.strictEqual(
  typeof querystring.stringify,
  'function',
  'querystring.stringify should be a function'
);
assert.strictEqual(
  typeof querystring.escape,
  'function',
  'querystring.escape should be a function'
);
assert.strictEqual(
  typeof querystring.unescape,
  'function',
  'querystring.unescape should be a function'
);
assert.strictEqual(
  typeof querystring.decode,
  'function',
  'querystring.decode should be a function'
);
assert.strictEqual(
  typeof querystring.encode,
  'function',
  'querystring.encode should be a function'
);
// console.log('✓ All querystring methods available');

// Test parse function
// console.log('\n--- Testing querystring.parse() ---');

// Basic parsing
const parsed1 = querystring.parse('a=1&b=2&c=3');
assert.deepEqual(
  parsed1,
  { a: '1', b: '2', c: '3' },
  'Basic parse should work'
);
// console.log('✓ Basic parsing:', parsed1);

// Empty values
const parsed2 = querystring.parse('a=&b=2&c=');
assert.deepEqual(parsed2, { a: '', b: '2', c: '' }, 'Empty values should work');
// console.log('✓ Empty values:', parsed2);

// Keys without values
const parsed3 = querystring.parse('a&b=2&c');
assert.deepEqual(
  parsed3,
  { a: '', b: '2', c: '' },
  'Keys without values should work'
);
// console.log('✓ Keys without values:', parsed3);

// Multiple values for same key
const parsed4 = querystring.parse('a=1&a=2&a=3');
assert.ok(Array.isArray(parsed4.a), 'Multiple values should create array');
assert.deepEqual(
  parsed4.a,
  ['1', '2', '3'],
  'Multiple values should be in array'
);
// console.log('✓ Multiple values:', parsed4);

// URL encoding
const parsed5 = querystring.parse('name=John%20Doe&email=john%40example.com');
assert.strictEqual(
  parsed5.name,
  'John Doe',
  'URL encoded spaces should decode'
);
assert.strictEqual(
  parsed5.email,
  'john@example.com',
  'URL encoded @ should decode'
);
// console.log('✓ URL decoding:', parsed5);

// Custom separators
const parsed6 = querystring.parse('a:1;b:2;c:3', ';', ':');
assert.deepEqual(
  parsed6,
  { a: '1', b: '2', c: '3' },
  'Custom separators should work'
);
// console.log('✓ Custom separators:', parsed6);

// Test stringify function
// console.log('\n--- Testing querystring.stringify() ---');

// Basic stringify
const stringified1 = querystring.stringify({ a: '1', b: '2', c: '3' });
assert.strictEqual(stringified1, 'a=1&b=2&c=3', 'Basic stringify should work');
// console.log('✓ Basic stringify:', stringified1);

// Array values
const stringified2 = querystring.stringify({ a: ['1', '2', '3'] });
assert.strictEqual(
  stringified2,
  'a=1&a=2&a=3',
  'Array values should be repeated'
);
// console.log('✓ Array values:', stringified2);

// URL encoding in stringify
const stringified3 = querystring.stringify({
  name: 'John Doe',
  email: 'john@example.com',
});
assert.ok(stringified3.includes('John+Doe'), 'Spaces should be encoded as +');
assert.ok(
  stringified3.includes('john%40example.com'),
  '@ should be URL encoded'
);
// console.log('✓ URL encoding:', stringified3);

// Custom separators in stringify
const stringified4 = querystring.stringify({ a: '1', b: '2' }, ';', ':');
assert.strictEqual(
  stringified4,
  'a:1;b:2',
  'Custom separators in stringify should work'
);
// console.log('✓ Custom separators:', stringified4);

// Test escape function
// console.log('\n--- Testing querystring.escape() ---');
const escaped = querystring.escape('hello world@test');
assert.ok(escaped.includes('hello+world'), 'escape should encode spaces as +');
assert.ok(escaped.includes('%40test'), 'escape should encode @ as %40');
// console.log('✓ escape():', escaped);

// Test unescape function
// console.log('\n--- Testing querystring.unescape() ---');
const unescaped = querystring.unescape('hello+world%40test');
assert.strictEqual(
  unescaped,
  'hello world@test',
  'unescape should decode properly'
);
// console.log('✓ unescape():', unescaped);

// Test round-trip
// console.log('\n--- Testing Round-trip Encoding ---');
const original = {
  name: 'John Doe',
  email: 'john+doe@example.com',
  tags: ['javascript', 'node.js', 'web'],
  description: 'A test & demo with special chars!',
};

const encoded = querystring.stringify(original);
const decoded = querystring.parse(encoded);

// Check basic properties
assert.strictEqual(
  decoded.name,
  original.name,
  'Name should survive round-trip'
);
assert.strictEqual(
  decoded.email,
  original.email,
  'Email should survive round-trip'
);
assert.strictEqual(
  decoded.description,
  original.description,
  'Description should survive round-trip'
);

// Check array
assert.ok(
  Array.isArray(decoded.tags),
  'Tags should be an array after round-trip'
);
assert.deepEqual(
  decoded.tags,
  original.tags,
  'Tags array should survive round-trip'
);

// console.log('✓ Round-trip encoding successful');
console.log('  Original:', original);
console.log('  Encoded: ', encoded);
console.log('  Decoded: ', decoded);

// Test error handling
// console.log('\n--- Testing Error Handling ---');

try {
  querystring.parse();
  assert.fail('parse() without arguments should throw');
} catch (error) {
  assert.ok(error.message.includes('string'), 'Should require string argument');
  // console.log('✓ parse() error handling works');
}

try {
  querystring.stringify();
  assert.fail('stringify() without arguments should throw');
} catch (error) {
  assert.ok(error.message.includes('object'), 'Should require object argument');
  // console.log('✓ stringify() error handling works');
}

// console.log('\n✅ All Node.js Query String module tests passed!');
console.log('📊 Node.js querystring compatibility implemented successfully!');
