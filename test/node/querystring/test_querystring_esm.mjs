// ESM import testing
import assert from 'jsrt:assert';
import querystring from 'node:querystring';

// === ESM Module Loading (10+ tests) ===

// Test module loaded
assert.ok(querystring, 'querystring module should load via ESM');

// Test all exports available
assert.ok(typeof querystring.parse === 'function', 'parse available in ESM');
assert.ok(
  typeof querystring.stringify === 'function',
  'stringify available in ESM'
);
assert.ok(typeof querystring.escape === 'function', 'escape available in ESM');
assert.ok(
  typeof querystring.unescape === 'function',
  'unescape available in ESM'
);
assert.ok(typeof querystring.encode === 'function', 'encode available in ESM');
assert.ok(typeof querystring.decode === 'function', 'decode available in ESM');

// Test functionality works in ESM
const parsed = querystring.parse('a=1&b=2&c=3');
assert.deepEqual(parsed, { a: '1', b: '2', c: '3' }, 'parse works in ESM');

const stringified = querystring.stringify({ x: 'test', y: 'value' });
assert.ok(stringified.includes('x=test'), 'stringify works in ESM');

const escaped = querystring.escape('hello world');
assert.strictEqual(escaped, 'hello+world', 'escape works in ESM');

const unescaped = querystring.unescape('hello+world');
assert.strictEqual(unescaped, 'hello world', 'unescape works in ESM');

// Test aliases work in ESM
assert.strictEqual(
  querystring.encode,
  querystring.stringify,
  'encode alias works in ESM'
);
assert.strictEqual(
  querystring.decode,
  querystring.parse,
  'decode alias works in ESM'
);

// Test complex operations in ESM
const data = {
  name: 'Test User',
  email: 'test@example.com',
  tags: ['tag1', 'tag2', 'tag3'],
};
const encoded = querystring.encode(data);
const decoded = querystring.decode(encoded);
assert.deepEqual(decoded, data, 'Complex operations work in ESM');

console.log('✅ All ESM tests passed (10+ tests)');
