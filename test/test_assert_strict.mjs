// Test assert.strict namespace with ES modules
import assert from 'node:assert';
import { strict } from 'node:assert';

console.log('Testing assert.strict mode (ES Module)...');

// Test 1: named export strict works
try {
  strict.equal(1, 1);
  console.log('✓ Named export strict.equal works');
} catch (e) {
  console.log('✗ Named export strict.equal failed:', e.message);
}

// Test 2: strict.equal uses strict comparison
try {
  strict.equal(1, '1');
  console.log('✗ strict.equal should fail for 1 and "1"');
} catch (e) {
  if (e.operator === '===') {
    console.log('✓ Named export strict.equal uses === operator');
  } else {
    console.log('✗ Wrong operator:', e.operator);
  }
}

// Test 3: assert.strict also works
try {
  assert.strict.equal(1, 1);
  console.log('✓ assert.strict.equal works');
} catch (e) {
  console.log('✗ assert.strict.equal failed:', e.message);
}

// Test 4: Verify both are the same object
if (assert.strict === strict) {
  console.log('✓ assert.strict and named export strict are the same');
} else {
  console.log('✗ assert.strict and named export strict are different');
}

console.log('All assert.strict ES Module tests completed!');
