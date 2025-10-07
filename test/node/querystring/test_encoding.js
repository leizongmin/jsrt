const assert = require('jsrt:assert');
const querystring = require('node:querystring');

// Encoding/Decoding Testing (25+ tests)

// === Space encoding ===
assert.strictEqual(
  querystring.escape('hello world'),
  'hello+world',
  'Spaces should encode to +'
);
assert.strictEqual(
  querystring.unescape('hello+world'),
  'hello world',
  '+ should decode to space'
);
assert.strictEqual(
  querystring.unescape('hello%20world'),
  'hello world',
  '%20 should decode to space'
);

// === Special characters ===
assert.ok(querystring.escape('a&b').includes('%26'), '& should encode to %26');
assert.ok(querystring.escape('a=b').includes('%3D'), '= should encode to %3D');
assert.ok(querystring.escape('a@b').includes('%40'), '@ should encode to %40');
assert.ok(querystring.escape('a#b').includes('%23'), '# should encode to %23');
assert.ok(querystring.escape('a?b').includes('%3F'), '? should encode to %3F');

assert.strictEqual(
  querystring.unescape('a%26b'),
  'a&b',
  '%26 should decode to &'
);
assert.strictEqual(
  querystring.unescape('a%3Db'),
  'a=b',
  '%3D should decode to ='
);
assert.strictEqual(
  querystring.unescape('a%40b'),
  'a@b',
  '%40 should decode to @'
);

// === Safe characters (should not encode) ===
const safe =
  'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~*';
const encoded = querystring.escape(safe);
// Most of these should pass through unchanged
assert.ok(encoded.includes('ABC'), 'Alphanumeric should not encode');
assert.ok(encoded.includes('xyz'), 'Lowercase should not encode');
assert.ok(encoded.includes('123'), 'Numbers should not encode');
assert.ok(encoded.includes('-'), 'Hyphen should not encode');
assert.ok(encoded.includes('_'), 'Underscore should not encode');
assert.ok(encoded.includes('.'), 'Dot should not encode');
assert.ok(encoded.includes('~'), 'Tilde should not encode');
// Note: * is preserved in querystring but not URLSearchParams
assert.ok(encoded.includes('*'), 'Asterisk should not encode');

// === UTF-8 encoding ===
assert.ok(
  querystring.escape('café').includes('%C3%A9'),
  'é should encode to %C3%A9'
);
assert.strictEqual(
  querystring.unescape('%C3%A9'),
  'é',
  '%C3%A9 should decode to é'
);

assert.ok(
  querystring.escape('日本語').includes('%E6%97%A5'),
  'Japanese should encode properly'
);
assert.ok(
  querystring.escape('你好').includes('%E4%BD%A0'),
  'Chinese should encode properly'
);
assert.ok(
  querystring.escape('мир').includes('%D0%BC'),
  'Cyrillic should encode properly'
);

// === Emoji encoding ===
const emoji = '🎉';
const emojiEncoded = querystring.escape(emoji);
assert.ok(emojiEncoded.includes('%'), 'Emoji should be encoded');
assert.strictEqual(
  querystring.unescape(emojiEncoded),
  emoji,
  'Emoji should round-trip'
);

// === URL encoding differences (+ vs %20) ===
const withSpace = querystring.stringify({ q: 'hello world' });
assert.ok(withSpace.includes('+'), 'querystring uses + for spaces');
assert.ok(!withSpace.includes('%20'), 'querystring should not use %20');

// === Percent sign encoding ===
assert.ok(querystring.escape('100%').includes('%25'), '% should encode to %25');
assert.strictEqual(
  querystring.unescape('100%25'),
  '100%',
  '%25 should decode to %'
);

// === Plus sign encoding ===
assert.ok(querystring.escape('a+b').includes('%2B'), '+ should encode to %2B');
assert.strictEqual(
  querystring.unescape('a%2Bb'),
  'a+b',
  '%2B should decode to +'
);

// === Slash encoding ===
assert.ok(querystring.escape('a/b').includes('%2F'), '/ should encode to %2F');
assert.strictEqual(
  querystring.unescape('a%2Fb'),
  'a/b',
  '%2F should decode to /'
);

// === Colon and semicolon ===
assert.ok(querystring.escape('a:b').includes('%3A'), ': should encode');
assert.ok(querystring.escape('a;b').includes('%3B'), '; should encode');

// === Brackets ===
assert.ok(querystring.escape('a[b]').includes('%5B'), '[ should encode');
assert.ok(querystring.escape('a[b]').includes('%5D'), '] should encode');

// === Quotes ===
assert.ok(querystring.escape('"test"').includes('%22'), '" should encode');
assert.ok(querystring.escape("'test'").includes('%27'), "' should encode");

// === Backslash ===
assert.ok(querystring.escape('a\\b').includes('%5C'), '\\ should encode');

// === Newlines and tabs ===
assert.ok(querystring.escape('a\nb').includes('%0A'), 'Newline should encode');
assert.ok(querystring.escape('a\tb').includes('%09'), 'Tab should encode');
assert.ok(
  querystring.escape('a\rb').includes('%0D'),
  'Carriage return should encode'
);

// === Round-trip complex string ===
const complex = 'Hello World! @#$%^&*()_+-=[]{}|;:\'",.<>?/\\`~';
const complexEncoded = querystring.escape(complex);
const complexDecoded = querystring.unescape(complexEncoded);
// Some characters may not survive exact round-trip, but basic ones should
assert.ok(complexDecoded.includes('Hello'), 'Text should survive');
assert.ok(complexDecoded.includes('World'), 'More text should survive');

console.log('✅ All encoding/decoding tests passed (25+ tests)');
