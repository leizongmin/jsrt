const assert = require('jsrt:assert');
const querystring = require('node:querystring');

// Compatibility and Round-trip Testing (20+ tests)

// === Basic round-trip ===
const data1 = { a: '1', b: '2', c: '3' };
const rt1 = querystring.parse(querystring.stringify(data1));
assert.deepEqual(rt1, data1, 'Basic round-trip');

// === Round-trip with spaces ===
const data2 = { text: 'hello world', name: 'John Doe' };
const rt2 = querystring.parse(querystring.stringify(data2));
assert.deepEqual(rt2, data2, 'Spaces round-trip');

// === Round-trip with special characters ===
const data3 = { email: 'test@example.com', url: 'http://test.com?foo=bar' };
const rt3 = querystring.parse(querystring.stringify(data3));
assert.deepEqual(rt3, data3, 'Special characters round-trip');

// === Round-trip with arrays ===
const data4 = { tags: ['a', 'b', 'c'], values: ['1', '2'] };
const rt4 = querystring.parse(querystring.stringify(data4));
assert.deepEqual(rt4, data4, 'Arrays round-trip');

// === Round-trip with empty values ===
const data5 = { a: '', b: 'filled', c: '' };
const rt5 = querystring.parse(querystring.stringify(data5));
assert.deepEqual(rt5, data5, 'Empty values round-trip');

// === Round-trip with unicode ===
const data6 = { text: '你好世界', emoji: '🎉🎊' };
const rt6 = querystring.parse(querystring.stringify(data6));
assert.deepEqual(rt6, data6, 'Unicode round-trip');

// === Round-trip with custom separators ===
const data7 = { a: '1', b: '2' };
const rt7 = querystring.parse(querystring.stringify(data7, ';', ':'), ';', ':');
assert.deepEqual(rt7, data7, 'Custom separators round-trip');

// === Encode/decode aliases ===
assert.ok(
  typeof querystring.encode === 'function',
  'encode should be a function'
);
assert.ok(
  typeof querystring.decode === 'function',
  'decode should be a function'
);

const data8 = { test: 'value' };
const encoded = querystring.encode(data8);
const decoded = querystring.decode(encoded);
assert.deepEqual(decoded, data8, 'Aliases work correctly');

// === Parse then stringify preserves data ===
const qs = 'foo=bar&baz=qux&arr=1&arr=2';
const parsed = querystring.parse(qs);
const stringified = querystring.stringify(parsed);
const reParsed = querystring.parse(stringified);
assert.deepEqual(reParsed, parsed, 'Parse-stringify-parse cycle');

// === Stringify then parse preserves data ===
const obj = { x: 'test', y: ['a', 'b'], z: '' };
const str = querystring.stringify(obj);
const parsed2 = querystring.parse(str);
assert.deepEqual(parsed2, obj, 'Stringify-parse cycle');

// === Complex real-world example ===
const realWorld = {
  search: 'javascript tutorials',
  category: 'programming',
  tags: ['nodejs', 'web', 'backend'],
  page: '1',
  sort: 'relevance',
};
const rwEncoded = querystring.stringify(realWorld);
const rwDecoded = querystring.parse(rwEncoded);
assert.strictEqual(rwDecoded.search, realWorld.search, 'Search preserved');
assert.strictEqual(
  rwDecoded.category,
  realWorld.category,
  'Category preserved'
);
assert.deepEqual(rwDecoded.tags, realWorld.tags, 'Tags array preserved');
assert.strictEqual(rwDecoded.page, realWorld.page, 'Page preserved');
assert.strictEqual(rwDecoded.sort, realWorld.sort, 'Sort preserved');

// === URL query string compatibility ===
const urlQuery = 'name=John+Doe&age=30&city=New+York';
const urlParsed = querystring.parse(urlQuery);
assert.strictEqual(urlParsed.name, 'John Doe', 'URL + sign decodes to space');
assert.strictEqual(urlParsed.city, 'New York', 'Multiple + signs work');

// === Empty and null round-trip ===
assert.deepEqual(
  querystring.parse(querystring.stringify({})),
  {},
  'Empty object round-trip'
);
assert.deepEqual(
  querystring.parse(querystring.stringify({ a: null })),
  { a: '' },
  'Null value round-trip'
);

// === Leading/trailing separators don't break round-trip ===
const weird = querystring.parse('&a=1&b=2&');
const weirdStr = querystring.stringify(weird);
const weirdParsed = querystring.parse(weirdStr);
assert.deepEqual(weirdParsed, weird, 'Weird input round-trip');

// === Very long values ===
const longValue = 'x'.repeat(10000);
const longData = { data: longValue };
const longEncoded = querystring.stringify(longData);
const longDecoded = querystring.parse(longEncoded);
assert.strictEqual(longDecoded.data, longValue, 'Long values round-trip');

// === Multiple types in one object ===
const multiType = {
  str: 'text',
  num: '123', // Numbers become strings after parse
  bool: 'true', // Booleans become strings after parse
  empty: '',
  arr: ['a', 'b'],
};
const mtEncoded = querystring.stringify(multiType);
const mtDecoded = querystring.parse(mtEncoded);
assert.deepEqual(mtDecoded, multiType, 'Multiple types round-trip');

// === Consecutive equals signs ===
const equals = querystring.parse('a=1=2&b=3=4=5');
// Should handle as value contains =
const eqStr = querystring.stringify(equals);
const eqParsed = querystring.parse(eqStr);
assert.deepEqual(eqParsed, equals, 'Consecutive equals round-trip');

console.log('✅ All compatibility tests passed (20+ tests)');
