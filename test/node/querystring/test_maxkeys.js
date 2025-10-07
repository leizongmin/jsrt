const assert = require('jsrt:assert');
const querystring = require('node:querystring');

// maxKeys Testing (10+ tests)

// === Default maxKeys (1000) ===
// Generate query string with 1001 parameters
let qs1001 = '';
for (let i = 0; i < 1001; i++) {
  if (i > 0) qs1001 += '&';
  qs1001 += `k${i}=v${i}`;
}

const result1001 = querystring.parse(qs1001);
assert.strictEqual(
  Object.keys(result1001).length,
  1000,
  'Default maxKeys should be 1000'
);
assert.ok(result1001.k0, 'First key should be present');
assert.ok(result1001.k999, 'Last key (999) should be present');
assert.strictEqual(
  result1001.k1000,
  undefined,
  'Key 1000 should not be present'
);

// === Custom maxKeys ===
const result10 = querystring.parse(
  'a=1&b=2&c=3&d=4&e=5&f=6&g=7&h=8&i=9&j=10&k=11',
  null,
  null,
  { maxKeys: 10 }
);
assert.strictEqual(
  Object.keys(result10).length,
  10,
  'maxKeys=10 should limit to 10 keys'
);
assert.ok(result10.a, 'First key should be present');
assert.ok(result10.j, 'Tenth key should be present');
assert.strictEqual(result10.k, undefined, 'Eleventh key should not be present');

// === maxKeys=0 (unlimited) ===
const result0 = querystring.parse(qs1001, null, null, { maxKeys: 0 });
assert.strictEqual(
  Object.keys(result0).length,
  1001,
  'maxKeys=0 should be unlimited'
);

// === maxKeys=1 ===
const result1 = querystring.parse('a=1&b=2&c=3', null, null, { maxKeys: 1 });
assert.strictEqual(
  Object.keys(result1).length,
  1,
  'maxKeys=1 should limit to 1 key'
);
assert.ok(result1.a, 'First key should be present');

// === maxKeys with duplicate keys ===
const resultDup = querystring.parse('a=1&a=2&a=3&b=4&c=5', null, null, {
  maxKeys: 2,
});
assert.strictEqual(
  Object.keys(resultDup).length,
  2,
  'maxKeys with duplicates should count unique keys'
);
assert.ok(Array.isArray(resultDup.a), 'Duplicate key should create array');
assert.deepEqual(
  resultDup.a,
  ['1', '2', '3'],
  'All duplicate values should be captured'
);
assert.ok(resultDup.b, 'Second unique key should be present');
assert.strictEqual(
  resultDup.c,
  undefined,
  'Third unique key should not be present'
);

// === Negative maxKeys (should be treated as 0/unlimited) ===
const resultNeg = querystring.parse('a=1&b=2&c=3', null, null, { maxKeys: -1 });
assert.ok(resultNeg.a, 'Negative maxKeys should work');

// === Very large maxKeys ===
const resultLarge = querystring.parse('a=1&b=2&c=3', null, null, {
  maxKeys: 1000000,
});
assert.strictEqual(
  Object.keys(resultLarge).length,
  3,
  'Large maxKeys should work normally'
);

// === maxKeys with empty strings ===
const resultEmpty = querystring.parse('&&&a=1&b=2&c=3', null, null, {
  maxKeys: 2,
});
assert.ok(resultEmpty.a, 'maxKeys should ignore empty parameters');
assert.ok(resultEmpty.b, 'maxKeys should count only real parameters');

// === maxKeys boundary conditions ===
const result999 = querystring.parse(qs1001, null, null, { maxKeys: 999 });
assert.strictEqual(
  Object.keys(result999).length,
  999,
  'maxKeys=999 should work correctly'
);
assert.ok(result999.k998, 'Key 998 should be present');
assert.strictEqual(result999.k999, undefined, 'Key 999 should not be present');

console.log('✅ All maxKeys tests passed (10+ tests)');
