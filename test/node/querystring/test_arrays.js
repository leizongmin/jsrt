const assert = require('jsrt:assert');
const querystring = require('node:querystring');

// Array Handling Testing (15+ tests)

// === Simple array values ===
const array1 = querystring.stringify({ tags: ['a', 'b', 'c'] });
assert.strictEqual(
  array1,
  'tags=a&tags=b&tags=c',
  'Array values should repeat key'
);

// === Parse duplicate keys into array ===
const parsed1 = querystring.parse('tags=a&tags=b&tags=c');
assert.ok(Array.isArray(parsed1.tags), 'Duplicate keys should create array');
assert.deepEqual(
  parsed1.tags,
  ['a', 'b', 'c'],
  'Array should contain all values'
);

// === Mixed single and array values ===
const mixed = querystring.stringify({ single: 'one', multi: ['a', 'b'] });
assert.ok(mixed.includes('single=one'), 'Single value should be present');
assert.ok(mixed.includes('multi=a'), 'First array value');
assert.ok(mixed.includes('multi=b'), 'Second array value');

// === Empty array ===
const empty = querystring.stringify({ tags: [] });
assert.strictEqual(empty, '', 'Empty array should produce empty string');

// === Array with empty strings ===
const emptyStrings = querystring.stringify({ tags: ['', 'a', ''] });
assert.strictEqual(
  emptyStrings,
  'tags=&tags=a&tags=',
  'Empty strings in array'
);

// === Array with null/undefined ===
const nullUndef = querystring.stringify({ tags: [null, 'a', undefined] });
assert.ok(nullUndef.includes('tags=&'), 'null in array becomes empty');
assert.ok(nullUndef.includes('tags=a'), 'Normal value preserved');

// === Large arrays ===
const largeArray = [];
for (let i = 0; i < 100; i++) {
  largeArray.push(`val${i}`);
}
const large = querystring.stringify({ items: largeArray });
const largeParsed = querystring.parse(large);
assert.ok(Array.isArray(largeParsed.items), 'Large array should parse back');
assert.strictEqual(
  largeParsed.items.length,
  100,
  'All 100 items should be present'
);
assert.strictEqual(largeParsed.items[0], 'val0', 'First item correct');
assert.strictEqual(largeParsed.items[99], 'val99', 'Last item correct');

// === Array with special characters ===
const special = querystring.stringify({ tags: ['a&b', 'c=d', 'e+f'] });
const specialParsed = querystring.parse(special);
assert.deepEqual(
  specialParsed.tags,
  ['a&b', 'c=d', 'e+f'],
  'Special chars in array'
);

// === Round-trip with arrays ===
const original = {
  name: 'test',
  tags: ['javascript', 'node.js', 'testing'],
  values: ['1', '2', '3'],
};
const encoded = querystring.stringify(original);
const decoded = querystring.parse(encoded);
assert.deepEqual(decoded.tags, original.tags, 'Tags array should round-trip');
assert.deepEqual(
  decoded.values,
  original.values,
  'Values array should round-trip'
);

// === Array with numbers ===
const numbers = querystring.stringify({ nums: [1, 2, 3, 4, 5] });
const numbersParsed = querystring.parse(numbers);
assert.deepEqual(
  numbersParsed.nums,
  ['1', '2', '3', '4', '5'],
  'Numbers become strings'
);

// === Nested arrays (should flatten) ===
const nested = querystring.stringify({
  data: [
    ['a', 'b'],
    ['c', 'd'],
  ],
});
// Note: Nested arrays are converted to [object Object] or stringified
assert.ok(nested.includes('data='), 'Nested arrays handled');

// === Array ordering preserved ===
const ordered = querystring.parse('items=first&items=second&items=third');
assert.deepEqual(
  ordered.items,
  ['first', 'second', 'third'],
  'Array order preserved'
);

// === Three or more duplicates ===
const many = querystring.parse('x=1&x=2&x=3&x=4&x=5');
assert.strictEqual(many.x.length, 5, 'Should handle many duplicates');
assert.deepEqual(many.x, ['1', '2', '3', '4', '5'], 'All values preserved');

// === Array with unicode ===
const unicode = querystring.stringify({ emojis: ['😀', '😁', '😂'] });
const unicodeParsed = querystring.parse(unicode);
assert.deepEqual(unicodeParsed.emojis, ['😀', '😁', '😂'], 'Unicode in arrays');

console.log('✅ All array handling tests passed (15+ tests)');
