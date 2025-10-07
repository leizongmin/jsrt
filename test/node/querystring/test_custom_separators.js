const assert = require('jsrt:assert');
const querystring = require('node:querystring');

// Custom Separators Testing (15+ tests)

// === Semicolon and colon ===
const parsed1 = querystring.parse('a:1;b:2;c:3', ';', ':');
assert.deepEqual(
  parsed1,
  { a: '1', b: '2', c: '3' },
  'Semicolon and colon separators'
);

const stringified1 = querystring.stringify(
  { a: '1', b: '2', c: '3' },
  ';',
  ':'
);
assert.strictEqual(
  stringified1,
  'a:1;b:2;c:3',
  'Stringify with custom separators'
);

// === Comma separator ===
const parsed2 = querystring.parse('a=1,b=2,c=3', ',');
assert.deepEqual(parsed2, { a: '1', b: '2', c: '3' }, 'Comma separator');

// === Pipe separator ===
const parsed3 = querystring.parse('a=1|b=2|c=3', '|');
assert.deepEqual(parsed3, { a: '1', b: '2', c: '3' }, 'Pipe separator');

// === Multi-character separators ===
const parsed4 = querystring.parse('a=1::b=2::c=3', '::');
assert.deepEqual(parsed4, { a: '1', b: '2', c: '3' }, 'Multi-char separator');

const stringified4 = querystring.stringify({ a: '1', b: '2' }, '::', '=');
assert.strictEqual(
  stringified4,
  'a=1::b=2',
  'Stringify with multi-char separator'
);

// === Space as separator ===
const parsed5 = querystring.parse('a=1 b=2 c=3', ' ');
assert.deepEqual(parsed5, { a: '1', b: '2', c: '3' }, 'Space separator');

// === Tab as separator ===
const parsed6 = querystring.parse('a=1\tb=2\tc=3', '\t');
assert.deepEqual(parsed6, { a: '1', b: '2', c: '3' }, 'Tab separator');

// === Custom equals separator ===
const parsed7 = querystring.parse('a:1&b:2&c:3', '&', ':');
assert.deepEqual(
  parsed7,
  { a: '1', b: '2', c: '3' },
  'Custom equals separator'
);

// === Mixed separators in data ===
const data = { url: 'http://test.com', path: '/a/b/c' };
const encoded = querystring.stringify(data, ';', '=');
assert.ok(!encoded.includes('&'), 'Should not use default separator');
assert.ok(encoded.includes(';'), 'Should use custom separator');

// === Array values with custom separators ===
const arrayData = querystring.stringify({ tags: ['a', 'b', 'c'] }, ';', ':');
assert.strictEqual(
  arrayData,
  'tags:a;tags:b;tags:c',
  'Array with custom separators'
);

// === Round-trip with custom separators ===
const original = { name: 'test', value: '123', flag: 'true' };
const encoded2 = querystring.stringify(original, '|', ':');
const decoded2 = querystring.parse(encoded2, '|', ':');
assert.deepEqual(decoded2, original, 'Custom separators round-trip');

// === Complex custom separators ===
const parsed8 = querystring.parse('a=>1<>b=>2<>c=>3', '<>', '=>');
assert.deepEqual(
  parsed8,
  { a: '1', b: '2', c: '3' },
  'Complex multi-char separators'
);

// === Null separators (use defaults) ===
const parsed9 = querystring.parse('a=1&b=2', null, null);
assert.deepEqual(
  parsed9,
  { a: '1', b: '2' },
  'Null separators should use defaults'
);

const stringified9 = querystring.stringify({ a: '1', b: '2' }, null, null);
assert.strictEqual(stringified9, 'a=1&b=2', 'Null separators in stringify');

// === Undefined separators (use defaults) ===
const parsed10 = querystring.parse('a=1&b=2', undefined, undefined);
assert.deepEqual(
  parsed10,
  { a: '1', b: '2' },
  'Undefined separators should use defaults'
);

console.log('✅ All custom separator tests passed (15+ tests)');
