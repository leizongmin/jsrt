const assert = require('jsrt:assert');
const querystring = require('node:querystring');

// Edge Cases Testing (30+ tests)

// === Empty inputs ===
assert.deepEqual(
  querystring.parse(''),
  {},
  'Empty string should return empty object'
);
assert.deepEqual(
  querystring.parse(null),
  {},
  'null should return empty object'
);
assert.deepEqual(
  querystring.parse(undefined),
  {},
  'undefined should return empty object'
);
assert.strictEqual(
  querystring.stringify({}),
  '',
  'Empty object should return empty string'
);
assert.strictEqual(
  querystring.stringify(null),
  '',
  'null should return empty string'
);
assert.strictEqual(
  querystring.stringify(undefined),
  '',
  'undefined should return empty string'
);

// === Special characters ===
const specialChars = querystring.parse('a=%26&b=%3D&c=%2B&d=%20');
assert.strictEqual(specialChars.a, '&', 'Should decode %26 to &');
assert.strictEqual(specialChars.b, '=', 'Should decode %3D to =');
assert.strictEqual(specialChars.c, '+', 'Should decode %2B to +');
assert.strictEqual(specialChars.d, ' ', 'Should decode %20 to space');

const encoded = querystring.stringify({ a: '&', b: '=', c: ' ' });
assert.ok(encoded.includes('%26'), 'Should encode & to %26');
assert.ok(encoded.includes('%3D'), 'Should encode = to %3D');
assert.ok(
  encoded.includes('+') || encoded.includes('%20'),
  'Should encode space'
);

// === Unicode handling ===
const unicode1 = querystring.parse(
  'emoji=%F0%9F%8E%89&chinese=%E4%BD%A0%E5%A5%BD'
);
assert.strictEqual(unicode1.emoji, '🎉', 'Should decode emoji correctly');
assert.strictEqual(
  unicode1.chinese,
  '你好',
  'Should decode Chinese characters'
);

const unicode2 = querystring.stringify({ emoji: '🎉', text: '日本語' });
assert.ok(unicode2.includes('%'), 'Should encode unicode characters');

const roundtrip = querystring.parse(
  querystring.stringify({ emoji: '😀', text: 'Ñoño' })
);
assert.strictEqual(roundtrip.emoji, '😀', 'Emoji should survive round-trip');
assert.strictEqual(
  roundtrip.text,
  'Ñoño',
  'Accented text should survive round-trip'
);

// === Null and undefined values ===
const nullValues = querystring.stringify({ a: null, b: undefined, c: '' });
assert.ok(nullValues.includes('a='), 'null should become empty string');
assert.ok(nullValues.includes('b='), 'undefined should become empty string');
assert.ok(nullValues.includes('c='), 'empty string should be present');

// === Type coercion ===
const numbers = querystring.stringify({ num: 42, float: 3.14, neg: -5 });
assert.ok(numbers.includes('num=42'), 'Should convert number to string');
assert.ok(numbers.includes('float=3.14'), 'Should convert float to string');
assert.ok(numbers.includes('neg=-5'), 'Should handle negative numbers');

const bools = querystring.stringify({ t: true, f: false });
assert.ok(bools.includes('t=true'), 'Should convert true to string');
assert.ok(bools.includes('f=false'), 'Should convert false to string');

// === Trailing/leading separators ===
assert.deepEqual(
  querystring.parse('&a=1&b=2'),
  { a: '1', b: '2' },
  'Leading & should be ignored'
);
assert.deepEqual(
  querystring.parse('a=1&b=2&'),
  { a: '1', b: '2' },
  'Trailing & should be ignored'
);
assert.deepEqual(
  querystring.parse('&a=1&'),
  { a: '1' },
  'Both leading and trailing & should be ignored'
);

// === Multiple separators ===
assert.deepEqual(
  querystring.parse('a=1&&b=2'),
  { a: '1', b: '2' },
  'Double && should be handled'
);
assert.deepEqual(
  querystring.parse('a=1&&&b=2'),
  { a: '1', b: '2' },
  'Triple &&& should be handled'
);

// === Keys with no equals sign ===
assert.deepEqual(
  querystring.parse('a&b&c'),
  { a: '', b: '', c: '' },
  'Keys without = should have empty values'
);
assert.deepEqual(
  querystring.parse('key'),
  { key: '' },
  'Single key without = should have empty value'
);

// === Plus sign handling ===
const plusTest = querystring.parse('text=hello+world+test');
assert.strictEqual(
  plusTest.text,
  'hello world test',
  'Plus signs should decode to spaces'
);

const plusEncode = querystring.stringify({ text: 'hello world' });
assert.ok(plusEncode.includes('hello+world'), 'Spaces should encode to plus');

// === Long strings ===
const longKey = 'k'.repeat(1000);
const longValue = 'v'.repeat(1000);
const longTest = querystring.stringify({ [longKey]: longValue });
assert.ok(longTest.length > 2000, 'Should handle long strings');
const longParsed = querystring.parse(longTest);
assert.strictEqual(
  longParsed[longKey],
  longValue,
  'Long strings should round-trip'
);

// === Many parameters ===
const manyParams = {};
for (let i = 0; i < 100; i++) {
  manyParams[`key${i}`] = `value${i}`;
}
const manyEncoded = querystring.stringify(manyParams);
const manyDecoded = querystring.parse(manyEncoded);
assert.strictEqual(
  Object.keys(manyDecoded).length,
  100,
  'Should handle 100 parameters'
);

console.log('✅ All edge case tests passed (30+ tests)');
