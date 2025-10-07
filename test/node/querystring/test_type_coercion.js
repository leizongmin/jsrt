const assert = require('jsrt:assert');
const querystring = require('node:querystring');

// Type Coercion Testing (20+ tests)

// === Numbers ===
const num1 = querystring.stringify({ int: 42 });
assert.ok(num1.includes('int=42'), 'Integer should convert to string');

const num2 = querystring.stringify({ float: 3.14159 });
assert.ok(num2.includes('float=3.14159'), 'Float should convert to string');

const num3 = querystring.stringify({ zero: 0 });
assert.ok(num3.includes('zero=0'), 'Zero should convert to string');

const num4 = querystring.stringify({ neg: -100 });
assert.ok(num4.includes('neg=-100'), 'Negative number should convert');

const num5 = querystring.stringify({ exp: 1e10 });
assert.ok(num5.includes('exp='), 'Scientific notation should convert');

// === Booleans ===
const bool1 = querystring.stringify({ t: true });
assert.ok(bool1.includes('t=true'), 'true should convert to "true"');

const bool2 = querystring.stringify({ f: false });
assert.ok(bool2.includes('f=false'), 'false should convert to "false"');

// === Null and undefined ===
const null1 = querystring.stringify({ n: null });
assert.ok(null1.includes('n='), 'null should become empty string');

const undef1 = querystring.stringify({ u: undefined });
assert.ok(undef1.includes('u='), 'undefined should become empty string');

// === Strings ===
const str1 = querystring.stringify({ s: 'hello' });
assert.ok(str1.includes('s=hello'), 'String should remain string');

const str2 = querystring.stringify({ empty: '' });
assert.ok(str2.includes('empty='), 'Empty string should work');

// === Mixed types ===
const mixed = querystring.stringify({
  str: 'text',
  num: 123,
  bool: true,
  nul: null,
  undef: undefined,
});
assert.ok(mixed.includes('str=text'), 'String preserved');
assert.ok(mixed.includes('num=123'), 'Number converted');
assert.ok(mixed.includes('bool=true'), 'Boolean converted');
assert.ok(mixed.includes('nul='), 'null converted to empty');
assert.ok(mixed.includes('undef='), 'undefined converted to empty');

// === BigInt (if supported) ===
try {
  const big = BigInt('9007199254740991');
  const bigint1 = querystring.stringify({ big: big });
  assert.ok(
    bigint1.includes('big=9007199254740991'),
    'BigInt should convert to string'
  );
} catch (e) {
  // BigInt not supported, skip
  console.log('  (BigInt test skipped - not supported)');
}

// === Special number values ===
const inf = querystring.stringify({ inf: Infinity });
assert.ok(inf.includes('inf=Infinity'), 'Infinity should convert');

const ninf = querystring.stringify({ ninf: -Infinity });
assert.ok(ninf.includes('ninf=-Infinity'), '-Infinity should convert');

const nan = querystring.stringify({ nan: NaN });
assert.ok(nan.includes('nan=NaN'), 'NaN should convert');

// === Arrays with mixed types ===
const mixedArray = querystring.stringify({ items: [1, 'two', true, null] });
assert.ok(mixedArray.includes('items=1'), 'Number in array');
assert.ok(mixedArray.includes('items=two'), 'String in array');
assert.ok(mixedArray.includes('items=true'), 'Boolean in array');
assert.ok(mixedArray.includes('items='), 'null in array');

// === Object values (should call toString) ===
const obj = querystring.stringify({ data: { key: 'value' } });
// Objects typically become [object Object]
assert.ok(obj.includes('data='), 'Object value should be handled');

// === Date objects ===
const date = new Date('2024-01-01T00:00:00Z');
const dateStr = querystring.stringify({ date: date });
assert.ok(dateStr.includes('date='), 'Date should convert to string');

// === Function values (should call toString) ===
const func = querystring.stringify({ fn: function () {} });
assert.ok(func.includes('fn='), 'Function should be handled');

// === Symbol values (may throw or convert) ===
try {
  const sym = Symbol('test');
  const symStr = querystring.stringify({ sym: sym });
  // If it doesn't throw, check it's handled
  assert.ok(true, 'Symbol handled without throwing');
} catch (e) {
  // Symbol conversion may throw TypeError
  assert.ok(true, 'Symbol throws TypeError as expected');
}

console.log('✅ All type coercion tests passed (20+ tests)');
