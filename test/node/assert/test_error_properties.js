// Test AssertionError properties
const assert = require('node:assert');

console.log('Testing AssertionError properties...');

// Test 1: strictEqual error properties
try {
  assert.strictEqual(1, 2);
  console.log('✗ strictEqual should have thrown');
} catch (e) {
  if (
    e.name === 'AssertionError' &&
    e.code === 'ERR_ASSERTION' &&
    e.actual === 1 &&
    e.expected === 2 &&
    e.operator === '===' &&
    e.generatedMessage === true
  ) {
    console.log('✓ strictEqual error has all properties');
  } else {
    console.log('✗ strictEqual error missing properties:', {
      name: e.name,
      code: e.code,
      actual: e.actual,
      expected: e.expected,
      operator: e.operator,
      generatedMessage: e.generatedMessage,
    });
  }
}

// Test 2: equal error properties
try {
  assert.equal(1, 2);
  console.log('✗ equal should have thrown');
} catch (e) {
  if (
    e.name === 'AssertionError' &&
    e.code === 'ERR_ASSERTION' &&
    e.actual === 1 &&
    e.expected === 2 &&
    e.operator === '==' &&
    e.generatedMessage === true
  ) {
    console.log('✓ equal error has all properties');
  } else {
    console.log('✗ equal error missing properties');
  }
}

// Test 3: Custom message (generatedMessage should be false)
try {
  assert.strictEqual(1, 2, 'custom message');
  console.log('✗ Should have thrown');
} catch (e) {
  if (e.message === 'custom message' && e.generatedMessage === false) {
    console.log('✓ Custom message has generatedMessage=false');
  } else {
    console.log('✗ Custom message error incorrect:', {
      message: e.message,
      generatedMessage: e.generatedMessage,
    });
  }
}

// Test 4: deepEqual error properties
try {
  assert.deepEqual({ a: 1 }, { a: 2 });
  console.log('✗ deepEqual should have thrown');
} catch (e) {
  if (
    e.name === 'AssertionError' &&
    e.code === 'ERR_ASSERTION' &&
    e.operator === 'deepEqual'
  ) {
    console.log('✓ deepEqual error has correct operator');
  } else {
    console.log('✗ deepEqual error missing properties');
  }
}

// Test 5: notStrictEqual error properties
try {
  assert.notStrictEqual(1, 1);
  console.log('✗ notStrictEqual should have thrown');
} catch (e) {
  if (e.operator === '!==') {
    console.log('✓ notStrictEqual error has correct operator');
  } else {
    console.log('✗ notStrictEqual error incorrect operator:', e.operator);
  }
}

console.log('All AssertionError property tests completed!');
