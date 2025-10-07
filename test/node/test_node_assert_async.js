// Test async assertions: rejects and doesNotReject
const assert = require('node:assert');

async function runTests() {
  // Test rejects with async function
  await assert.rejects(async () => {
    throw new Error('async error');
  });

  // Test rejects with Promise
  await assert.rejects(Promise.reject(new Error('promise error')));

  // Test rejects should fail with resolving promise
  try {
    await assert.rejects(async () => {
      return 'success';
    });
    throw new Error('Should have thrown');
  } catch (e) {
    if (e.name !== 'AssertionError') throw e;
  }

  // Test doesNotReject with resolving promise
  await assert.doesNotReject(async () => {
    return 'success';
  });

  // Test doesNotReject with Promise
  await assert.doesNotReject(Promise.resolve('value'));

  // Test doesNotReject should fail with rejecting promise
  try {
    await assert.doesNotReject(async () => {
      throw new Error('error');
    });
    throw new Error('Should have thrown');
  } catch (e) {
    if (e.name !== 'AssertionError') throw e;
  }

  // Test rejects returns the error
  const err = await assert.rejects(async () => {
    throw new Error('returned error');
  });
  assert.strictEqual(err.message, 'returned error');

  // Test doesNotReject returns the value
  const val = await assert.doesNotReject(async () => {
    return 'test value';
  });
  assert.strictEqual(val, 'test value');

  console.log('✓ All async assertion tests passed');
}

runTests().catch((err) => {
  console.error('Test failed:', err);
  process.exit(1);
});
