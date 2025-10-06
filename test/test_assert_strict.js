// Test assert.strict namespace
const assert = require('node:assert');
const strict = assert.strict;

console.log('Testing assert.strict mode...');

// Test 1: strict.equal maps to strictEqual
try {
  strict.equal(1, '1'); // Should fail because strict mode uses ===
  console.log('✗ strict.equal should use === comparison');
} catch (e) {
  if (e.operator === '===') {
    console.log('✓ strict.equal uses === operator');
  } else {
    console.log('✗ strict.equal has wrong operator:', e.operator);
  }
}

// Test 2: strict.equal succeeds with strict equality
try {
  strict.equal(1, 1);
  console.log('✓ strict.equal(1, 1) succeeds');
} catch (e) {
  console.log('✗ strict.equal should not throw for equal values');
}

// Test 3: strict.notEqual maps to notStrictEqual
try {
  strict.notEqual(1, 1);
  console.log('✗ strict.notEqual should throw');
} catch (e) {
  if (e.operator === '!==') {
    console.log('✓ strict.notEqual uses !== operator');
  } else {
    console.log('✗ strict.notEqual has wrong operator:', e.operator);
  }
}

// Test 4: strict.ok works
try {
  strict.ok(true);
  console.log('✓ strict.ok works');
} catch (e) {
  console.log('✗ strict.ok failed:', e.message);
}

// Test 5: strict.fail works
try {
  strict.fail('test failure');
  console.log('✗ strict.fail should throw');
} catch (e) {
  if (e.message === 'test failure') {
    console.log('✓ strict.fail works');
  } else {
    console.log('✗ strict.fail wrong message:', e.message);
  }
}

// Test 6: strict.match works
try {
  strict.match('hello world', /world/);
  console.log('✓ strict.match works');
} catch (e) {
  console.log('✗ strict.match failed:', e.message);
}

// Test 7: strict.throws works
try {
  strict.throws(() => {
    throw new Error('test');
  });
  console.log('✓ strict.throws works');
} catch (e) {
  console.log('✗ strict.throws failed:', e.message);
}

// Test 8: Verify strict namespace has all methods
const methods = [
  'ok',
  'equal',
  'notEqual',
  'strictEqual',
  'notStrictEqual',
  'deepEqual',
  'notDeepEqual',
  'throws',
  'doesNotThrow',
  'fail',
  'ifError',
  'match',
  'doesNotMatch',
];
let allPresent = true;
for (const method of methods) {
  if (typeof strict[method] !== 'function') {
    console.log('✗ strict.' + method + ' is missing');
    allPresent = false;
  }
}
if (allPresent) {
  console.log('✓ All methods present in strict namespace');
}

// Test 9: strict is also available via ES module named export
console.log('Testing ES module named export...');
